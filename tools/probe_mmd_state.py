# -*- coding: utf-8 -*-
# probe_mmd_state.py - locate the live MMDApp object inside a running copy of
# the reference MikuMikudance.exe by its global camera-key track signature,
# then read the camera/view fields the port needs to match.
#
# The track is a 10000 x 84-byte zeroed array whose head record carries
# distance -45.0f (+0x0C), eye[1] 10.0f (+0x14) and fov 30 (+0x44); the same
# triple repeats at +84.  Finding it gives a reliable anchor, and scanning
# for a pointer to it yields the app object address regardless of layout
# drift between builds.
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

PROCESS_QUERY_INFORMATION = 0x0400
PROCESS_VM_READ = 0x0010
MEM_COMMIT = 0x1000
PAGE_GUARD = 0x100
PAGE_NOACCESS = 0x01
READABLE = (0x02, 0x04, 0x20, 0x40, 0x80)  # RO, RW, EX_R, EX_RW, EX_WC

k32 = ctypes.WinDLL("kernel32", use_last_error=True)


class MBI(ctypes.Structure):
    _fields_ = [("BaseAddress", ctypes.c_ulonglong),
                ("AllocationBase", ctypes.c_ulonglong),
                ("AllocationProtect", wt.DWORD),
                ("__a1", wt.DWORD),
                ("RegionSize", ctypes.c_ulonglong),
                ("State", wt.DWORD),
                ("Protect", wt.DWORD),
                ("Type", wt.DWORD),
                ("__a2", wt.DWORD)]


def regions(handle):
    addr = 0
    mbi = MBI()
    while addr < 0x7FFFFFFFFFFF:
        got = k32.VirtualQueryEx(handle, ctypes.c_void_p(addr),
                                 ctypes.byref(mbi), ctypes.sizeof(mbi))
        if not got:
            break
        if (mbi.State == MEM_COMMIT and not (mbi.Protect & PAGE_GUARD)
                and not (mbi.Protect & PAGE_NOACCESS)
                and (mbi.Protect & 0xFF) in READABLE):
            yield mbi.BaseAddress, mbi.RegionSize
        addr = mbi.BaseAddress + mbi.RegionSize


def read(handle, addr, size):
    buf = (ctypes.c_char * size)()
    got = ctypes.c_size_t(0)
    if not k32.ReadProcessMemory(handle, ctypes.c_void_p(addr), buf, size,
                                 ctypes.byref(got)):
        return None
    return bytes(buf[:got.value])


def scan_camera_track(handle):
    """Find the camera key track: 84-byte stride head signature.

    CameraKey layout (ABI-checked in global_key_layout.hpp):
      +0x0C distance, +0x10 eye[3], +0x1C target[3], +0x28 interpolation,
      +0x40 perspective, +0x44 fov, +0x48 selected.
    """
    def key(fov):
        k = bytearray(84)
        struct.pack_into("<f", k, 0x0C, -45.0)
        struct.pack_into("<f", k, 0x14, 10.0)
        struct.pack_into("<i", k, 0x44, fov)
        return bytes(k)

    # key[0] carries the defaults; keys[1..] are zero except fov/interp.
    needle = key(30) + key(0)
    for base, size in regions(handle):
        chunk = read(handle, base, size)
        if not chunk:
            continue
        idx = chunk.find(needle)
        if idx >= 0:
            return base + idx
    # fallback: any record with distance -45 and fov 30 84 bytes later
    for base, size in regions(handle):
        chunk = read(handle, base, size)
        if not chunk:
            continue
        i = 0
        while True:
            i = chunk.find(struct.pack("<f", -45.0), i)
            if i < 0:
                break
            if i >= 0x0C and i + 0x44 - 0x0C + 4 <= len(chunk):
                if struct.unpack_from("<i", chunk, i + 0x38)[0] == 30:
                    return base + i - 0x0C
            i += 1
    return 0


def main():
    wait = float(sys.argv[1]) if len(sys.argv) > 1 else 8.0
    proc = subprocess.Popen([EXE], cwd=MMD_DIR)
    print("launched pid", proc.pid)
    time.sleep(wait)
    if proc.poll() is not None:
        print("exited early", proc.returncode)
        return 1
    h = k32.OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, False, proc.pid)
    if not h:
        print("OpenProcess failed", ctypes.get_last_error())
        proc.kill()
        return 1

    track = scan_camera_track(h)
    print("camera track @ 0x%X" % track)
    if track:
        rec = read(h, track, 84)
        print("key[0]:")
        for o in range(0, 84, 4):
            f = struct.unpack_from("<f", rec, o)[0]
            i = struct.unpack_from("<i", rec, o)[0]
            print("   +0x%02X int=%-12d float=%.6g" % (o, i, f))

    # find the app object: cameraFov == 30 at +0x9F0B4 with the camera track
    # pointer at +0x3D0.
    app = 0
    for base, size in regions(h):
        chunk = read(h, base, size)
        if not chunk:
            continue
        i = 0
        while True:
            i = chunk.find(struct.pack("<f", 30.0), i)
            if i < 0:
                break
            cand = base + i - 0x9F0B4
            if cand > 0:
                ptr = read(h, cand + 0x3D0, 8)
                if ptr and struct.unpack("<Q", ptr)[0] == track:
                    print("candidate app @ 0x%X" % cand)
                    app = cand
            i += 1
        if app:
            break
    if app == 0:
        print("app not found")
        proc.terminate()
        return 1

    print("\napp @ 0x%X" % app)
    fields = [
        (0x328, "uiOptionFlag0 camera panel", "byte"),
        (0x340, "viewOffsetX  (0x308 x86)", "float"),
        (0x344, "viewOffsetY  (0x30C x86)", "float"),
        (0x348, "cameraPitch  (0x310 x86)", "float"),
        (0x34C, "cameraYaw    (0x314 x86)", "float"),
        (0x350, "cameraRoll   (0x318 x86)", "float"),
        (0x354, "cameraPerspective", "byte"),
        (0x36C, "cameraPosX readout (0x334)", "float"),
        (0x370, "cameraPosY readout (0x338)", "float"),
        (0x374, "cameraPosZ readout (0x33C)", "float"),
        (0xA18F8, "cameraDistance (0xA08DC)", "float"),
        (0x9F0B4, "cameraFov (0x9E1E8?)", "float"),
        (0x9FCA0, "followCameraEnabled (0x9ED98)", "byte"),
        (0x9FCA4, "cameraReferenceMode (0x340x86?)", "int"),
        (0xA1404, "cameraParentModel (0xA0430)", "int"),
        (0xA1408, "cameraParentBone (0xA0434)", "int"),
    ]
    for off, name, kind in fields:
        raw = read(h, app + off, 4)
        if raw is None:
            print("  +0x%06X %-34s <unreadable>" % (off, name))
            continue
        if kind == "float":
            v = "%.6f" % struct.unpack("<f", raw)[0]
        elif kind == "byte":
            v = "%d" % raw[0]
        else:
            v = "%d" % struct.unpack("<i", raw)[0]
        print("  +0x%06X %-34s %s" % (off, name, v))

    proc.terminate()
    try:
        proc.wait(timeout=5)
    except Exception:
        proc.kill()
    return 0


