"""Launch a Windows benchmark suspended, apply CPU limits, then collect load."""
import ctypes as c
from ctypes import wintypes as w
import msvcrt
import os
import struct
import subprocess
import time


class StartupInfo(c.Structure):
    _fields_ = [('cb', w.DWORD), ('reserved', w.LPWSTR), ('desktop', w.LPWSTR),
                ('title', w.LPWSTR), ('x', w.DWORD), ('y', w.DWORD),
                ('width', w.DWORD), ('height', w.DWORD), ('chars_x', w.DWORD),
                ('chars_y', w.DWORD), ('fill', w.DWORD), ('flags', w.DWORD),
                ('show', w.WORD), ('reserved_size', w.WORD), ('reserved_ptr', c.c_void_p),
                ('stdin', w.HANDLE), ('stdout', w.HANDLE), ('stderr', w.HANDLE)]


class ProcessInfo(c.Structure):
    _fields_ = [('process', w.HANDLE), ('thread', w.HANDLE),
                ('pid', w.DWORD), ('tid', w.DWORD)]


class CpuRate(c.Structure):
    _fields_ = [('flags', w.DWORD), ('rate', w.DWORD)]


class BasicLimit(c.Structure):
    _fields_ = [('process_time', c.c_longlong), ('job_time', c.c_longlong),
                ('flags', w.DWORD), ('minimum_working_set', c.c_size_t),
                ('maximum_working_set', c.c_size_t), ('active_processes', w.DWORD),
                ('affinity', c.c_size_t), ('priority', w.DWORD), ('scheduling', w.DWORD)]


class ExtendedLimit(c.Structure):
    _fields_ = [('basic', BasicLimit), ('io_counters', c.c_ulonglong * 6),
                ('process_memory', c.c_size_t), ('job_memory', c.c_size_t),
                ('peak_process_memory', c.c_size_t), ('peak_job_memory', c.c_size_t)]


kernel = c.WinDLL('kernel32', use_last_error=True)


def api(name, result, *arguments):
    fn = getattr(kernel, name)
    fn.restype, fn.argtypes = result, arguments
    return fn


create_job = api('CreateJobObjectW', w.HANDLE, c.c_void_p, w.LPCWSTR)
set_job = api('SetInformationJobObject', w.BOOL, w.HANDLE, c.c_int, c.c_void_p, w.DWORD)
query_job = api('QueryInformationJobObject', w.BOOL, w.HANDLE, c.c_int, c.c_void_p, w.DWORD, c.c_void_p)
assign_job = api('AssignProcessToJobObject', w.BOOL, w.HANDLE, w.HANDLE)
create_process = api('CreateProcessW', w.BOOL, w.LPCWSTR, w.LPWSTR, c.c_void_p,
                     c.c_void_p, w.BOOL, w.DWORD, c.c_void_p, w.LPCWSTR,
                     c.POINTER(StartupInfo), c.POINTER(ProcessInfo))
set_affinity = api('SetProcessAffinityMask', w.BOOL, w.HANDLE, c.c_size_t)
get_affinity = api('GetProcessAffinityMask', w.BOOL, w.HANDLE, c.POINTER(c.c_size_t), c.POINTER(c.c_size_t))
resume = api('ResumeThread', w.DWORD, w.HANDLE)
wait = api('WaitForSingleObject', w.DWORD, w.HANDLE, w.DWORD)
exit_code = api('GetExitCodeProcess', w.BOOL, w.HANDLE, c.POINTER(w.DWORD))
terminate = api('TerminateProcess', w.BOOL, w.HANDLE, w.UINT)
terminate_job = api('TerminateJobObject', w.BOOL, w.HANDLE, w.UINT)
close = api('CloseHandle', w.BOOL, w.HANDLE)
system_times = api('GetSystemTimes', w.BOOL, c.POINTER(w.FILETIME), c.POINTER(w.FILETIME), c.POINTER(w.FILETIME))
process_times = api('GetProcessTimes', w.BOOL, w.HANDLE, c.POINTER(w.FILETIME),
                    c.POINTER(w.FILETIME), c.POINTER(w.FILETIME), c.POINTER(w.FILETIME))
cpu_sets = api('GetSystemCpuSetInformation', w.BOOL, c.c_void_p, w.ULONG,
               c.POINTER(w.ULONG), w.HANDLE, w.ULONG)


def checked(ok):
    if not ok:
        raise c.WinError(c.get_last_error())


def topology():
    size = w.ULONG()
    cpu_sets(None, 0, c.byref(size), None, 0)
    buffer = c.create_string_buffer(size.value)
    checked(cpu_sets(buffer, len(buffer), c.byref(size), None, 0))
    result, offset = [], 0
    while offset < size.value:
        length, kind = struct.unpack_from('<II', buffer, offset)
        if length < 8:
            raise RuntimeError('Invalid CPU topology record')
        if kind == 0:
            ident, group, logical, core, cache, node, efficiency, flags = struct.unpack_from('<IHBBBBBB', buffer, offset + 8)
            result.append(dict(id=ident, group=group, logical=logical, core=core,
                               efficiency=efficiency, flags=flags))
        offset += length
    if not result or any(item['group'] != 0 for item in result):
        raise RuntimeError('This benchmark limiter supports one processor group')
    return result


def timestamp(value):
    return (value.dwHighDateTime << 32) | value.dwLowDateTime


def usage(process):
    idle, kern, user = w.FILETIME(), w.FILETIME(), w.FILETIME()
    checked(system_times(c.byref(idle), c.byref(kern), c.byref(user)))
    start, end, pk, pu = w.FILETIME(), w.FILETIME(), w.FILETIME(), w.FILETIME()
    checked(process_times(process, c.byref(start), c.byref(end), c.byref(pk), c.byref(pu)))
    return time.perf_counter(), timestamp(idle), timestamp(kern) + timestamp(user), timestamp(pk) + timestamp(pu)


def run(command, cwd, env, stream, timeout, cpu_percent, logical_cpus, telemetry, start_barrier=None,
        stop_event=None):
    """Limits only the new child. All failures terminate that child and fail closed."""
    cpus = topology()
    uncapped = cpu_percent is None and logical_cpus is None
    if (cpu_percent is None) != (logical_cpus is None):
        raise ValueError('CPU rate and selected CPUs must both be set or both be None')
    if not uncapped and (len(set(logical_cpus)) != len(logical_cpus) or not logical_cpus):
        raise ValueError('Select distinct logical CPUs')
    if not uncapped and any(index not in {item['logical'] for item in cpus} for index in logical_cpus):
        raise ValueError('Selected CPU does not exist')
    rate = 0 if uncapped else round(cpu_percent * 100)
    if not uncapped and not 1 <= rate <= 10000:
        raise ValueError('CPU percentage must be between 0.01 and 100')
    mask = None if uncapped else sum(1 << index for index in logical_cpus)
    job, process = create_job(None, None), ProcessInfo()
    checked(job)
    samples = []
    telemetry.update(topology=cpus, requested_system_cpu_percent=cpu_percent,
                     logical_cpus=logical_cpus, affinity_mask=None if uncapped else hex(mask), samples=samples,
                     note=('No CPU rate control or affinity override; GPU remains shared.' if uncapped else
                           'Hard CPU cap, not CPU reservation; GPU remains shared and uncapped.'))
    try:
        cleanup = ExtendedLimit()
        cleanup.basic.flags = 0x2000  # JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
        checked(set_job(job, 9, c.byref(cleanup), c.sizeof(cleanup)))
        parent_rate = CpuRate()
        if query_job(None, 15, c.byref(parent_rate), c.sizeof(parent_rate), None):
            telemetry['parent_job_rate'] = dict(flags=parent_rate.flags, rate=parent_rate.rate)
            if parent_rate.flags & 1:
                raise RuntimeError('Parent job already controls CPU rate; cannot claim a system-relative cap')
        else:
            telemetry['parent_job_query_error'] = c.get_last_error()
        limits = CpuRate(0 if uncapped else 0x1 | 0x4, rate)
        if not uncapped:
            checked(set_job(job, 15, c.byref(limits), c.sizeof(limits)))
        info = StartupInfo()
        info.cb, info.flags, info.show = c.sizeof(info), 0x100 | 0x1, 0
        with open(os.devnull, 'rb') as null:
            info.stdin = msvcrt.get_osfhandle(null.fileno())
            info.stdout = info.stderr = msvcrt.get_osfhandle(stream.fileno())
            handles = (info.stdin, info.stdout)
            old_inherit = [os.get_handle_inheritable(handle) for handle in handles]
            try:
                for handle in handles:
                    os.set_handle_inheritable(handle, True)
                block = c.create_unicode_buffer('\0'.join(f'{key}={value}' for key, value in sorted(env.items())) + '\0\0')
                line = c.create_unicode_buffer(subprocess.list2cmdline(command))
                checked(create_process(None, line, None, None, True, 0x08000000 | 0x4 | 0x400,
                                       block, str(cwd), c.byref(info), c.byref(process)))
            finally:
                for handle, inherited in zip(handles, old_inherit):
                    os.set_handle_inheritable(handle, inherited)
        if not uncapped:
            checked(set_affinity(process.process, mask))
        checked(assign_job(job, process.process))
        actual_mask, system_mask, actual_rate = c.c_size_t(), c.c_size_t(), CpuRate()
        checked(get_affinity(process.process, c.byref(actual_mask), c.byref(system_mask)))
        checked(query_job(job, 15, c.byref(actual_rate), c.sizeof(actual_rate), None))
        if ((not uncapped and actual_mask.value != mask) or
            (actual_rate.flags, actual_rate.rate) != (limits.flags, rate)):
            raise RuntimeError('Resource limit readback mismatch')
        telemetry.update(pid=process.pid, verified_before_resume=True,
                         verified_affinity_mask=hex(actual_mask.value), verified_rate=actual_rate.rate,
                         verified_control_flags=actual_rate.flags)
        if start_barrier is not None:
            start_barrier.wait(timeout=60)
        previous = usage(process.process)
        started = previous[0]
        telemetry['resumed_at_unix_ns'] = time.time_ns()
        if resume(process.thread) == 0xffffffff:
            raise c.WinError(c.get_last_error())
        while True:
            status = wait(process.process, 1000)
            if status not in (0, 258):
                raise c.WinError(c.get_last_error())
            current = usage(process.process)
            elapsed, total = current[0] - previous[0], current[2] - previous[2]
            busy = max(0, total - (current[1] - previous[1]))
            child = max(0, current[3] - previous[3])
            samples.append(dict(end_seconds=current[0] - started, interval_seconds=elapsed,
                                system_cpu_percent=100 * busy / max(total, 1),
                                process_system_cpu_percent=100 * child / max(total, 1),
                                other_cpu_percent=100 * max(0, busy - child) / max(total, 1)))
            previous = current
            if status == 0:
                break
            if stop_event is not None and stop_event.is_set():
                telemetry['controller_requested_stop'] = True
                checked(terminate_job(job, 0))
                if wait(process.process, 5000) != 0:
                    raise RuntimeError('Owned test process did not stop')
                break
            if current[0] - started >= timeout:
                raise subprocess.TimeoutExpired(command, timeout)
        code = w.DWORD()
        checked(exit_code(process.process, c.byref(code)))
        return code.value
    finally:
        if process.process:
            if wait(process.process, 0) == 258:
                # Includes only this newly assigned process and its descendants.
                # Before assignment, terminate the still-suspended child itself.
                terminate_job(job, 1)
                terminate(process.process, 1)
                wait(process.process, 5000)
            close(process.process)
        if process.thread:
            close(process.thread)
        close(job)
