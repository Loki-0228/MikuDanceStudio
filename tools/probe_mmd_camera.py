# -*- coding: utf-8 -*-
# probe_mmd_camera.py - launch the reference MikuMikudance.exe and read the
# live default camera/view state out of its MMDApp object, so the port can be
# matched against measured behaviour instead of inference.
#
# App object: image base + 0x11000 (the .data pin the project's app_layout.hpp
# documents).  Fields are read by their x64 offsets.
import ctypes
import ctypes.wintypes as wt
import os
import struct
import subprocess
import sys
import time

sys.stdout.reconfigure(encoding="utf-8")

MMD_DIR = os.path.abspath(r"refs\MikuMikudance 926 CHS")
EXE = os.path.join(MMD_DIR, "MikuMikudance.exe")
APP_RVA = 0x11000

PROCESS_QUERY_INFORMATION = 0x0400
PROCESS_VM_READ = 0x0010
MEM_COMMIT = 0x1000
PAGE_GUARD = 0x100
PAGE_NOACCESS = 0x01


class MEMORY_BASIC_INFORMATION64(ctypes.Structure):
    _fields_ = [("BaseAddress", ctypes.c_ulonglong),
                ("AllocationBase", ctypes.c_ulonglong),
                ("AllocationProtect", wt.DWORD),
                ("__alignment1", wt.DWORD),
                ("RegionSize", ctypes.c_ulonglong),
                ("State", wt.DWORD),
                ("Protect", wt.DWORD),
                ("Type", wt.DWORD),
                ("__alignment2", wt.DWORD)]


k32 = ctypes.WinDLL("kernel32", use_last_error=True)


def find_main_module_base(pid):
    """Base address of the process' first (main) module via psapi."""
    psapi = ctypes.WinDLL("psapi", use_last_error=True)
    LIST_MODULES_ALL = 0x03
    hproc = k32.OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, False, pid)
    if not hproc:
        return 0
    needed = wt.DWORD(0)
    arr = (ctypes.c_void_p * 1024)()
    if not psapi.EnumProcessModulesEx(hproc, arr, ctypes.sizeof(arr),
                                      ctypes.byref(needed), LIST_MODULES_ALL):
        k32.CloseHandle(hproc)
        return 0
    base = arr[0] or 0
    k32.CloseHandle(hproc)
    return base


def read_mem(handle, addr, size):
    buf = (ctypes.c_char * size)()
    got = ctypes.c_size_t(0)
    if not k32.ReadProcessMemory(handle, ctypes.c_void_p(addr), buf, size,
                                 ctypes.byref(got)):
        return None
    return bytes(buf[:got.value])


def main():
    wait = float(sys.argv[1]) if len(sys.argv) > 1 else 6.0
    proc = subprocess.Popen([EXE], cwd=MMD_DIR)
    print("launched pid", proc.pid)
    time.sleep(wait)
    if proc.poll() is not None:
        print("process exited early with", proc.returncode)
        return 1
    handle = k32.OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, False,
                             proc.pid)
    if not handle:
        print("OpenProcess failed", ctypes.get_last_error())
        proc.kill()
        return 1
    base = find_main_module_base(proc.pid)
    print("main module base 0x%X" % base)
    app = base + APP_RVA
    print("app object 0x%X" % app)

    fields = {
        0x328: ("uiOptionFlag0 (camera panel)", "byte"),
        0x340: ("viewOffsetX", "float"),
        0x344: ("viewOffsetY", "float"),
        0x348: ("cameraPitch (rad)", "float"),
        0x34C: ("cameraYaw (rad)", "float"),
        0x350: ("cameraRoll (rad)", "float"),
        0x354: ("cameraPerspective", "byte"),
        0x368: ("followCameraEnabled", "byte"),
        0x36C: ("cameraPosX (readout)", "float"),
        0x370: ("cameraPosY (readout)", "float"),
        0x374: ("cameraPosZ (readout)", "float"),
        0x37C: ("byte37C", "int"),
        0x8C: ("middleButtonState", "int"),
        0x88: ("rightButtonState", "int"),
        0x98: ("numpad2State", "int"),
        0xA18F8: ("cameraDistance", "float"),
        0x9FC9C: ("dword9FC9C", "int"),
        0x9FCA0: ("byte9FCA0", "byte"),
        0x9FCA4: ("cameraReferenceMode", "int"),
        0xA1404: ("cameraParentModel", "int"),
        0xA1408: ("cameraParentBone", "int"),
        0xA1900: ("dwordA1900", "float"),
    }
    for off in sorted(fields):
        name, kind = fields[off]
        raw = read_mem(handle, app + off, 4)
        if raw is None:
            print("  app+0x%06X %-32s <unreadable>" % (off, name))
            continue
        if kind == "float":
            val = "%.6f" % struct.unpack("<f", raw)[0]
        elif kind == "byte":
            val = "%d" % raw[0]
        else:
            val = "%d" % struct.unpack("<i", raw)[0]
        print("  app+0x%06X %-32s %s" % (off, name, val))

    # camera key track: cameraKeyTrack pointer + first record
    keyptr_raw = read_mem(handle, app + 0x3D0, 8)
    if keyptr_raw:
        ptr = struct.unpack("<Q", keyptr_raw)[0]
        print("cameraKeyTrack ptr 0x%X" % ptr)
        # x64 CameraKey layout: 0x00 prev, 0x04 next, 0x08 frame? -> dump 84 bytes
        rec = read_mem(handle, ptr, 84)
        if rec:
            print("  key[0] hex:", rec.hex(" "))
            for o in range(0, 84, 4):
                f = struct.unpack_from("<f", rec, o)[0]
                i = struct.unpack_from("<i", rec, o)[0]
                print("    +0x%02X int=%-12d float=%.6g" % (o, i, f))

    ctypes.windll.user32.PostMessageW(
        ctypes.windll.user32.FindWindowW(None, None), 0, 0, 0)
    proc.terminate()
    try:
        proc.wait(timeout=5)
    except Exception:
        proc.kill()
    return 0


if __name__ == "__main__":
    sys.exit(main())
