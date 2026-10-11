"""Bounded original MMD 9.32 / mmd932-profile file interop probe.

The reference must match the repository's previously recorded unmodified
9.32 x64 SHA-256. A folder called "932 fix" is not equivalent. --identity-only
does not launch anything. Normal runs require a single-model PMM plus matching
standard model VMD/VPD seeds: frame zero, short exact CP932 identities, no
physics-dependent pose, and an absolute existing model path in the PMM.

Only copied runtimes and newly created child processes are used. Each child
starts suspended on a fresh private desktop, enters a kill-on-close job, then
resumes. The actual desktop of every addressed window is checked. There is no
Default-desktop fallback, SwitchDesktop, input injection, process attachment,
memory writing, DLL hooking, or interaction with existing user processes.

Normal File menu commands and real common dialogs exercise the production IO.
Success means two-direction fixture-specific VMD/VPD semantic equality, and
PMM model-count plus saved-pose import evidence. It is not a full PMM schema,
rendering, physics, PMX/PMD exporter, or arbitrary-model compatibility proof.
Private-desktop D3D failure is reported as blocked, never as interop success.
Requires Windows x64 and the adjacent limited_process.py for Win32 declarations.
"""

import argparse
import ctypes as c
from ctypes import wintypes as w
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import time
import uuid


ORIGINAL_SHA256 = "07516fd3bf1e6b1339836b6773a156f61bdd6f848eeb621fdda012375df313a1"
KNOWN_FIX_SHA256 = "3ebe6ead8076478c06539b14a0e32e70f30b0edc9c8f045819d04ad8d62bee49"
COMMAND = {"load_pmm": 205, "save_pmm": 208, "load_vmd": 209,
           "save_vmd": 210, "load_vpd": 202, "save_vpd": 203}


class Blocked(RuntimeError):
    pass


def digest(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def identity(path):
    # Some Windows sandboxes permit reading an EXE but deny the extra handle
    # access used by realpath/GetFinalPathNameByHandle. The byte hash is the
    # identity authority; resolving a reparse target is not required here.
    path = Path(path).absolute()
    data = path.read_bytes()
    pe = struct.unpack_from("<I", data, 0x3c)[0] if len(data) >= 64 else 0
    if not pe or data[:2] != b"MZ" or data[pe:pe + 4] != b"PE\0\0":
        raise Blocked(f"Not a PE executable: {path}")
    machine = struct.unpack_from("<H", data, pe + 4)[0]
    sha = hashlib.sha256(data).hexdigest()
    return dict(path=str(path), bytes=len(data), sha256=sha, machine=hex(machine),
                original_932_verified=sha == ORIGINAL_SHA256 and machine == 0x8664,
                known_932_fix=sha == KNOWN_FIX_SHA256)


class Reader:
    def __init__(self, data):
        self.data, self.offset = data, 0

    def take(self, count):
        if count < 0 or count > len(self.data) - self.offset:
            raise ValueError("Truncated standard file")
        result = self.data[self.offset:self.offset + count]
        self.offset += count
        return result

    def u32(self):
        return struct.unpack("<I", self.take(4))[0]


def cp932(raw):
    raw = raw.split(b"\0", 1)[0]
    text = raw.decode("cp932", errors="strict")
    if text.encode("cp932", errors="strict") != raw:
        raise ValueError("Non-roundtripping CP932 field")
    return text


def finite(values):
    if not all(math.isfinite(x) for x in values):
        raise ValueError("Nonfinite seed/output value")
    return list(values)


def vmd(data):
    reader = Reader(data)
    header = reader.take(30).split(b"\0", 1)[0]
    if header == b"Vocaloid Motion Data 0002":
        version, width = 2, 20
    elif header == b"Vocaloid Motion Data file":
        version, width = 1, 10
    else:
        raise ValueError("Unsupported standard VMD header")
    result = dict(model=cp932(reader.take(width)), bones=[], morphs=[], displays=[])
    count = reader.u32()
    if count > (len(data) - reader.offset) // 111:
        raise ValueError("Invalid VMD bone count")
    for _ in range(count):
        record = reader.take(111)
        result["bones"].append(dict(name=cp932(record[:15]), frame=struct.unpack_from("<I", record, 15)[0],
            pose=finite(struct.unpack_from("<7f", record, 19)), interpolation=list(record[47:63])))
    count = reader.u32()
    if count > (len(data) - reader.offset) // 23:
        raise ValueError("Invalid VMD morph count")
    for _ in range(count):
        record = reader.take(23)
        result["morphs"].append(dict(name=cp932(record[:15]), frame=struct.unpack_from("<I", record, 15)[0],
                                    value=finite(struct.unpack_from("<f", record, 19))[0]))
    # This probe intentionally accepts only model motions, so comparison
    # cannot silently omit camera, light or shadow data from a mixed file.
    for kind, size in (("camera", 61 if version == 2 else 36), ("light", 28), ("shadow", 9)):
        if reader.offset == len(data):
            break
        count = reader.u32()
        if count:
            raise ValueError(f"Probe requires an empty {kind} segment (record size {size})")
    else:
        if reader.offset != len(data):
            count = reader.u32()
            if count > (len(data) - reader.offset) // 9:
                raise ValueError("Invalid VMD display count")
            for _ in range(count):
                frame = reader.u32()
                visible = reader.take(1)[0]
                ik_count = reader.u32()
                if ik_count > (len(data) - reader.offset) // 21:
                    raise ValueError("Invalid VMD IK count")
                entries = [(cp932(reader.take(20)), reader.take(1)[0]) for _ in range(ik_count)]
                result["displays"].append(dict(frame=frame, visible=visible, ik=sorted(entries)))
    if reader.offset != len(data):
        raise ValueError("VMD extension/trailing bytes are outside this standard-only probe")
    for kind in ("bones", "morphs", "displays"):
        result[kind].sort(key=lambda item: (item.get("name", ""), item["frame"]))
    if not result["bones"] or min(item["frame"] for kind in ("bones", "morphs", "displays")
                                  for item in result[kind]) != 0:
        raise ValueError("Probe requires bone motion with a minimum selected frame of zero")
    return result


def vpd(data):
    text = data.decode("cp932", errors="strict").replace("\r\n", "\n")
    header = re.match(r"Vocaloid Pose Data file\n\n([^\n]*);[^\n]*\n(\d+);[^\n]*\n\n", text)
    if not header:
        raise ValueError("Invalid VPD header")
    body = re.compile(r"Bone\d+\{([^\n]*)\n\s*([^;\n]+);[^\n]*\n\s*([^;\n]+);[^\n]*\n\s*\}\s*")
    records, position = [], header.end()
    for _ in range(int(header[2])):
        match = body.match(text, position)
        if not match:
            raise ValueError("Incomplete VPD pose")
        xyz = finite(tuple(float(value.strip()) for value in match[2].split(",")))
        quat = finite(tuple(float(value.strip()) for value in match[3].split(",")))
        if len(xyz) != 3 or len(quat) != 4:
            raise ValueError("Invalid VPD pose component count")
        records.append(dict(name=match[1], pose=xyz + quat))
        position = match.end()
    if not records or text[position:].strip():
        raise ValueError("Empty VPD or nonstandard VPD tail")
    return dict(model=header[1], bones=sorted(records, key=lambda item: item["name"]))


def pmm_header(data):
    reader = Reader(data)
    if reader.take(30).split(b"\0", 1)[0] != b"Polygon Movie maker 0002":
        raise ValueError("Probe requires PMM version 2")
    reader.take(24)
    count = reader.take(1)[0]
    if count != 1:
        raise ValueError("Probe seed/output must contain exactly one model")
    slot = reader.take(1)[0]
    names = [cp932(reader.take(reader.take(1)[0])) for _ in range(2)]
    model_path = cp932(reader.take(256))
    # This is only a header/reference check. No whole-file PMM validity claim.
    return dict(model_count=count, slot=slot, model_names=names, model_path=model_path)


def equivalent(expected, actual, location="root"):
    if isinstance(expected, float):
        if not isinstance(actual, (float, int)) or not math.isclose(expected, actual, abs_tol=2e-5, rel_tol=2e-6):
            raise AssertionError(f"{location}: {actual!r} != {expected!r}")
    elif isinstance(expected, dict):
        if set(expected) != set(actual):
            raise AssertionError(f"{location}: field names differ")
        for key in expected:
            equivalent(expected[key], actual[key], f"{location}.{key}")
    elif isinstance(expected, (tuple, list)):
        if len(expected) != len(actual):
            raise AssertionError(f"{location}: record counts differ")
        for index, (first, second) in enumerate(zip(expected, actual)):
            equivalent(first, second, f"{location}[{index}]")
    elif expected != actual:
        raise AssertionError(f"{location}: {actual!r} != {expected!r}")


def stage(executable, destination):
    source = executable.parent
    if not (source / "Data").is_dir():
        raise Blocked(f"Runtime Data directory absent beside {executable}")
    destination.mkdir()
    shutil.copy2(executable, destination / "MikuMikuDance.exe")
    shutil.copytree(source / "Data", destination / "Data")
    # Explicit runtime dependencies only. Do not load MME/MMV or a proxy d3d9
    # merely because they happen to live in the source installation directory.
    copied = ["MikuMikuDance.exe", "Data/"]
    for path in source.iterdir():
        if path.is_file() and re.fullmatch(r"(?:d3dx9_\d+|d3dcompiler_\d+|vcruntime\w*|msvcp\w*)\.dll", path.name, re.I):
            shutil.copy2(path, destination / path.name)
            copied.append(path.name)
    (destination / "UserFile").mkdir()
    return dict(exe=str(destination / "MikuMikuDance.exe"), copied=copied)


class Session:
    def __init__(self, executable, record, timeout):
        import limited_process as lp
        self.lp, self.record, self.timeout = lp, record, timeout
        self.process, self.job, self.desktop = lp.ProcessInfo(), None, None
        self.main = None
        self.user = c.WinDLL("user32", use_last_error=True)
        self.callback = c.WINFUNCTYPE(w.BOOL, w.HWND, w.LPARAM)
        self.create_desktop = self.api("CreateDesktopW", w.HANDLE, w.LPCWSTR, c.c_void_p, c.c_void_p, w.DWORD, w.DWORD, c.c_void_p)
        self.close_desktop = self.api("CloseDesktop", w.BOOL, w.HANDLE)
        self.object_info = self.api("GetUserObjectInformationW", w.BOOL, w.HANDLE, c.c_int, c.c_void_p, w.DWORD, c.POINTER(w.DWORD))
        self.enum = self.api("EnumDesktopWindows", w.BOOL, w.HANDLE, self.callback, w.LPARAM)
        self.enum_children = self.api("EnumChildWindows", w.BOOL, w.HWND, self.callback, w.LPARAM)
        self.owner = self.api("GetWindowThreadProcessId", w.DWORD, w.HWND, c.POINTER(w.DWORD))
        self.item = self.api("GetDlgItem", w.HWND, w.HWND, c.c_int)
        self.get_owner = self.api("GetWindow", w.HWND, w.HWND, w.UINT)
        self.ancestor = self.api("GetAncestor", w.HWND, w.HWND, w.UINT)
        self.class_name = self.api("GetClassNameW", c.c_int, w.HWND, w.LPWSTR, c.c_int)
        self.is_window = self.api("IsWindow", w.BOOL, w.HWND)
        self.enabled = self.api("IsWindowEnabled", w.BOOL, w.HWND)
        self.post_api = self.api("PostMessageW", w.BOOL, w.HWND, w.UINT, w.WPARAM, w.LPARAM)
        self.send_api = self.api("SendMessageTimeoutW", c.c_size_t, w.HWND, w.UINT, w.WPARAM, w.LPARAM, w.UINT, w.UINT, c.POINTER(c.c_size_t))
        self.executable = executable
        self.desktop_name = "MMS_REPAIR_INTEROP_" + uuid.uuid4().hex
        self.record.update(desktop_requested=self.desktop_name, private_desktop_verified=False, events=[])

    def api(self, name, result, *args):
        method = getattr(self.user, name)
        method.restype, method.argtypes = result, args
        return method

    def desktop_name_of(self, desktop):
        buffer, needed = c.create_unicode_buffer(1024), w.DWORD()
        self.lp.checked(self.object_info(desktop, 2, buffer, c.sizeof(buffer), c.byref(needed)))
        return buffer.value

    def event(self, kind, **fields):
        self.record["events"].append(dict(kind=kind, time=time.time(), **fields))

    def __enter__(self):
        try:
            self.desktop = self.create_desktop(self.desktop_name, None, None, 0, 0x1ff, None)
            self.lp.checked(self.desktop)
            if self.desktop_name_of(self.desktop) != self.desktop_name:
                raise Blocked("Created desktop name mismatch")
            self.job = self.lp.create_job(None, None)
            self.lp.checked(self.job)
            limit = self.lp.ExtendedLimit()
            limit.basic.flags = 0x2000
            self.lp.checked(self.lp.set_job(self.job, 9, c.byref(limit), c.sizeof(limit)))
            startup = self.lp.StartupInfo()
            startup.cb, startup.flags, startup.show = c.sizeof(startup), 1, 0
            startup.desktop = self.desktop_name
            line = c.create_unicode_buffer(subprocess.list2cmdline([str(self.executable)]))
            self.lp.checked(self.lp.create_process(str(self.executable), line, None, None, False,
                0x4 | 0x08000000, None, str(self.executable.parent), c.byref(startup), c.byref(self.process)))
            self.record["pid"] = self.process.pid
            self.lp.checked(self.lp.assign_job(self.job, self.process.process))
            if self.lp.resume(self.process.thread) == 0xffffffff:
                raise c.WinError(c.get_last_error())
            # A thread may not acquire its USER desktop until initialization.
            def verified_main():
                windows = self.windows()
                for window in windows:
                    if self.cls(window) == "#32770":
                        raise Blocked("Startup dialog on private desktop: " + self.dialog_text(window))
                for window in windows:
                    if self.item(window, 436) and self.enabled(window):
                        return window
                return None
            self.main = self.until(verified_main, "main window / private desktop / D3D initialization")
            self.until(lambda: self.send(self.item(self.main, 436), 0x146) == 1,
                       "fresh empty scene before loading the fixture")
            self.record["private_desktop_verified"] = True
            self.record["desktop_actual"] = self.desktop_name
            self.event("private_desktop", name=self.desktop_name, hwnd=self.main)
            return self
        except BaseException:
            self.close()
            raise

    def alive(self):
        if self.lp.wait(self.process.process, 0) != 258:
            code = w.DWORD()
            self.lp.exit_code(self.process.process, c.byref(code))
            raise Blocked(f"Owned process exited early: {code.value}")

    def verify_window(self, window):
        pid = w.DWORD()
        self.owner(window, c.byref(pid))
        # A foreign thread's GetThreadDesktop handle is not necessarily usable
        # in this process. Prove actual window membership instead: enumerate
        # the named HDESK we own and require the target's top-level ancestor.
        if pid.value != self.process.pid or self.ancestor(window, 2) not in self.windows():
            raise Blocked("Window PID/desktop ownership mismatch")

    def windows(self):
        self.alive()
        found = []
        @self.callback
        def collect(window, _):
            pid = w.DWORD()
            self.owner(window, c.byref(pid))
            if pid.value == self.process.pid:
                found.append(window)
            return True
        c.set_last_error(0)
        ok = self.enum(self.desktop, collect, 0)
        if not ok and c.get_last_error():
            raise c.WinError(c.get_last_error())
        if found:
            actual = self.desktop_name_of(self.desktop)
            if actual != self.desktop_name:
                raise Blocked("Enumerated desktop identity changed")
            self.record.update(private_desktop_verified=True, desktop_actual=actual,
                desktop_verification="UOI_NAME of owned HDESK plus EnumDesktopWindows/PID/root-window membership")
        return found

    def cls(self, window):
        buffer = c.create_unicode_buffer(128)
        self.class_name(window, buffer, len(buffer))
        return buffer.value

    def send(self, window, message, first=0, second=0):
        self.verify_window(window)
        result = c.c_size_t()
        self.lp.checked(self.send_api(window, message, first, second, 2, 2000, c.byref(result)))
        return result.value

    def post(self, window, message, first=0, second=0):
        self.verify_window(window)
        self.lp.checked(self.post_api(window, message, first, second))

    def text(self, window):
        buffer = c.create_unicode_buffer(1024)
        self.send(window, 0xD, len(buffer), c.addressof(buffer))
        return buffer.value

    def dialog_text(self, window):
        children = []
        @self.callback
        def collect(child, _):
            children.append(child)
            return True
        self.enum_children(window, collect, 0)
        messages = [self.text(window)]
        messages.extend(self.text(child) for child in children if self.cls(child).lower() == "static")
        return " | ".join(message for message in messages if message)

    def until(self, check, description):
        end = time.monotonic() + self.timeout
        while time.monotonic() < end:
            self.alive()
            result = check()
            if result:
                return result
            time.sleep(0.05)
        self.event("timeout", operation=description, windows=[dict(hwnd=h, cls=self.cls(h), title=self.text(h)) for h in self.windows()])
        raise Blocked("Timed out: " + description)

    def file(self, operation, path):
        path = path.resolve()
        if len(str(path)) >= 240:
            raise ValueError("Keep fixture paths below the native 256-character field capacity")
        saving = operation.startswith("save_")
        if saving and path.exists():
            raise ValueError("Probe never approves overwriting an existing output")
        if not saving and not path.is_file():
            raise ValueError("Missing input fixture")
        before = set(self.windows())
        self.post(self.main, 0x111, COMMAND[operation])
        def find_picker():
            for window in self.windows():
                if window in before or self.cls(window) != "#32770":
                    continue
                control = self.item(window, 1152) or self.item(window, 1148)
                button = self.item(window, 1)
                owner = self.get_owner(window, 4)
                for _ in range(16):
                    if not owner or owner == self.main:
                        break
                    owner = self.get_owner(owner, 4)
                if control and button and owner == self.main and self.enabled(button):
                    return window, control, button
                raise Blocked("Unexpected dialog; not automatically approved: " + self.text(window))
            return None
        dialog, control, button = self.until(find_picker, operation + " file dialog")
        value = c.create_unicode_buffer(str(path))
        if not self.send(control, 0xC, 0, c.addressof(value)):
            raise Blocked("Filename control refused the fixture path")
        if self.text(control) != str(path):
            raise Blocked("Filename control readback mismatch")
        self.event("file_command", operation=operation, path=str(path), dialog=dialog)
        self.post(dialog, 0x111, 1, button)
        def completed():
            for window in self.windows():
                if window not in before and window != dialog and self.cls(window) == "#32770":
                    raise Blocked("Unexpected load/save confirmation; not approved: " + self.text(window))
            if self.is_window(dialog) or not self.enabled(self.main):
                return False
            return not saving or path.is_file() and path.stat().st_size > 0
        self.until(completed, operation + " completion")
        # Barrier after the modal loop returned, then require actual file
        # semantics in the caller. Dialog disappearance alone is insufficient.
        self.send(self.main, 0)
        if saving:
            self.event("saved", path=str(path), bytes=path.stat().st_size, sha256=digest(path))

    def select_model(self):
        combo = self.item(self.main, 436)
        count = self.until(lambda: self.send(combo, 0x146) == 2, "single model combo population")
        if not count or self.send(combo, 0x14e, 1) != 1:
            raise Blocked("Cannot select the single fixture model")
        self.post(self.main, 0x111, 436 | (1 << 16), combo)
        self.send(self.main, 0)
        if self.send(combo, 0x147) != 1:
            raise Blocked("Model selection readback mismatch")
        self.event("model_selection", combo_count=2, selected=1)

    def close(self):
        # Only handles obtained from our own CreateProcess/Job are terminated.
        # Never enumerate processes by name, post quit to a user instance, or
        # wait on an unsaved-file confirmation in order to clean up.
        errors = []
        try:
            if self.process.process and self.lp.wait(self.process.process, 0) == 258:
                if self.job:
                    self.lp.terminate_job(self.job, 0)
                if self.lp.wait(self.process.process, 5000) == 258:
                    self.lp.checked(self.lp.terminate(self.process.process, 1))
                    if self.lp.wait(self.process.process, 5000) != 0:
                        errors.append("owned child did not terminate")
        except BaseException as error:
            errors.append(repr(error))
        for handle in (self.process.thread, self.process.process, self.job):
            if handle and not self.lp.close(handle):
                errors.append("CloseHandle failed")
        self.process.thread = self.process.process = self.job = None
        if self.desktop and not self.close_desktop(self.desktop):
            errors.append("CloseDesktop failed")
        self.desktop = None
        self.record["cleanup_errors"] = errors
        if errors:
            self.record["cleanup_failed"] = True

    def __exit__(self, *_):
        self.close()
        if self.record.get("cleanup_failed"):
            raise Blocked("Owned process/desktop cleanup failed; see manifest")


def run(args, report):
    reference, candidate = identity(args.reference), identity(args.candidate)
    report.update(reference=reference, candidate=candidate)
    if not reference["original_932_verified"]:
        raise Blocked("Reference identity is not the recorded unmodified MMD 9.32 x64")
    if candidate["sha256"] != args.candidate_sha256.lower() or candidate["machine"] != "0x8664":
        raise Blocked("Candidate SHA-256/architecture differs from the requested test artifact")
    if args.identity_only:
        report["status"] = "identity_verified_only"
        return
    if os.name != "nt" or c.sizeof(c.c_void_p) != 8:
        raise Blocked("Execution requires 64-bit Windows Python")
    if not all((args.seed_pmm, args.seed_vmd, args.seed_vpd)):
        raise Blocked("Normal runs require --seed-pmm, --seed-vmd and --seed-vpd")
    seed_pmm = pmm_header(args.seed_pmm.read_bytes())
    seed_vmd, seed_vpd = vmd(args.seed_vmd.read_bytes()), vpd(args.seed_vpd.read_bytes())
    model = Path(seed_pmm["model_path"])
    if not model.is_absolute() or not model.is_file():
        raise Blocked("Seed PMM must contain an absolute existing model path")
    if seed_vmd["model"] != seed_pmm["model_names"][0]:
        raise Blocked("Seed PMM/VMD model identity mismatch")
    report["seeds"] = dict(pmm=seed_pmm, vmd=seed_vmd, vpd=seed_vpd,
                           hashes={ext: digest(getattr(args, "seed_" + ext)) for ext in ("pmm", "vmd", "vpd")})
    report["coverage"] = "single-model fixture; PMM model reference/pose, VMD selected standard model tracks, VPD selected pose; no full scene/rendering guarantee"
    report["sessions"] = []
    produced = {}
    for label in ("reference", "candidate"):
        runtime = stage(Path(report[label]["path"]), args.output / (label + "-runtime"))
        report[label]["staging"] = runtime
        record = dict(label=label + "-produce")
        report["sessions"].append(record)
        outputs = {ext: args.output / (label + "." + ext) for ext in ("pmm", "vmd", "vpd")}
        with Session(Path(runtime["exe"]), record, args.timeout) as session:
            session.file("load_pmm", args.seed_pmm)
            session.select_model()
            session.file("load_vmd", args.seed_vmd)
            session.file("save_vmd", outputs["vmd"])
            equivalent(seed_vmd, vmd(outputs["vmd"].read_bytes()))
            session.file("load_vpd", args.seed_vpd)
            session.file("save_vpd", outputs["vpd"])
            equivalent(seed_vpd, vpd(outputs["vpd"].read_bytes()))
            session.file("save_pmm", outputs["pmm"])
            equivalent(seed_pmm, pmm_header(outputs["pmm"].read_bytes()))
            record["semantic_checks_passed"] = True
        produced[label] = outputs
    for target, source in (("reference", "candidate"), ("candidate", "reference")):
        record = dict(label=source + "-to-" + target)
        report["sessions"].append(record)
        # Do not reuse the first session's writable Data/config directory.
        runtime = stage(Path(report[target]["path"]), args.output / (target + "-import-runtime"))
        record["staging"] = runtime
        with Session(Path(runtime["exe"]), record, args.timeout) as session:
            session.file("load_pmm", produced[source]["pmm"])
            session.select_model()
            # Before loading another pose, export the pose carried by PMM.
            pose_from_pmm = args.output / (record["label"] + "-pmm-pose.vpd")
            session.file("save_vpd", pose_from_pmm)
            equivalent(seed_vpd, vpd(pose_from_pmm.read_bytes()))
            for ext, expected in (("vmd", seed_vmd), ("vpd", seed_vpd)):
                result = args.output / (record["label"] + "." + ext)
                session.file("load_" + ext, produced[source][ext])
                session.file("save_" + ext, result)
                equivalent(expected, {"vmd": vmd, "vpd": vpd}[ext](result.read_bytes()))
            record["semantic_checks_passed"] = True
    report["status"] = "fixture_interop_passed"


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--candidate", type=Path, required=True)
    parser.add_argument("--candidate-sha256", required=True)
    parser.add_argument("--output", type=Path, required=True, help="new owned result directory; existing directories are refused")
    parser.add_argument("--identity-only", action="store_true")
    for ext in ("pmm", "vmd", "vpd"):
        parser.add_argument("--seed-" + ext, type=Path)
    parser.add_argument("--timeout", type=float, default=30, help="per-step timeout, 5..60 seconds")
    args = parser.parse_args()
    if not 5 <= args.timeout <= 60 or not re.fullmatch(r"[0-9a-fA-F]{64}", args.candidate_sha256):
        parser.error("Require timeout 5..60 and an exact 64-hex candidate SHA-256")
    args.output = args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=False)
    report = dict(status="incomplete", started=time.time(), reference_expected_sha256=ORIGINAL_SHA256,
                  desktop_mode="private-only", execution_requested=not args.identity_only)
    code = 1
    try:
        run(args, report)
        code = 0
    except Blocked as error:
        report.update(status="blocked", reason=str(error))
        code = 2
    except BaseException as error:
        report.update(status="failed", error=repr(error))
    finally:
        report["finished"] = time.time()
        (args.output / "manifest.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
        print(json.dumps(dict(status=report["status"], manifest=str(args.output / "manifest.json")), ensure_ascii=False))
    return code


if __name__ == "__main__":
    raise SystemExit(main())
