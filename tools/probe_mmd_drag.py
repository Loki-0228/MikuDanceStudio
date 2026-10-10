# -*- coding: utf-8 -*-
# probe_mmd_drag.py - drive a real RMB drag at the reference MMD's viewport and
# read the camera/view fields before and after, so the original's view-drag
# behaviour can be measured instead of inferred.
import ctypes
import ctypes.wintypes as wt
import os
import struct
import subprocess
import sys
import time

sys.stdout.reconfigure(encoding="utf-8")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from probe_mmd_state import (k32, read, regions, scan_camera_track,  # noqa: E402
                             PROCESS_QUERY_INFORMATION, PROCESS_VM_READ)
from shot_window import capture, find_window  # noqa: E402

u = ctypes.windll.user32
MOUSEEVENTF_RIGHTDOWN = 0x0008
MOUSEEVENTF_RIGHTUP = 0x0010
MOUSEEVENTF_MIDDLEDOWN = 0x0020
MOUSEEVENTF_MIDDLEUP = 0x0040

FIELDS = [
    (0x04, "mouseStoredX", "int"),
    (0x08, "mouseStoredY", "int"),
    (0x0C, "prevMouseX", "int"),
    (0x10, "prevMouseY", "int"),
    (0x24, "shiftState", "int"),
    (0x88, "rmbState", "int"),
    (0x8C, "mmbState", "int"),
    (0xC0, "ctrlState", "int"),
    (0x328, "uiCameraFlag", "byte"),
    (0x340, "viewOffsetX", "float"),
    (0x344, "viewOffsetY", "float"),
    (0x348, "cameraPitch", "float"),
    (0x34C, "cameraYaw", "float"),
    (0x350, "cameraRoll", "float"),
    (0x36C, "camPosX", "float"),
    (0x370, "camPosY", "float"),
    (0x374, "camPosZ", "float"),
    (0xA18F8, "cameraDistance", "float"),
    (0x9EDD1, "viewportInputActive", "byte"),
]


def find_app(h, track):
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
                    return cand
            i += 1
    return 0


def dump(h, app, tag):
    print("--- %s ---" % tag)
    for off, name, kind in FIELDS:
        raw = read(h, app + off, 4)
        if raw is None:
            print("  +0x%06X %-20s <unreadable>" % (off, name))
            continue
        if kind == "float":
            v = "%.4f" % struct.unpack("<f", raw)[0]
        elif kind == "byte":
            v = "%d" % raw[0]
        else:
            v = "%d" % struct.unpack("<i", raw)[0]
        print("  +0x%06X %-20s %s" % (off, name, v))


def main():
    exe = os.path.abspath(r"refs\MikuMikudance 926 CHS\MikuMikudance.exe")
    out = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else r".probe\refdrag"
    gesture = sys.argv[2] if len(sys.argv) > 2 else "rmb"
    dy = int(sys.argv[3]) if len(sys.argv) > 3 else -160
    proc = subprocess.Popen([exe], cwd=os.path.dirname(exe))
    print("pid", proc.pid)
    time.sleep(12)
    hits = find_window(proc.pid, "MikuMikuDance")
    hits.sort(key=lambda t: -t[2])
    hwnd = hits[0][0]
    u.ShowWindow(hwnd, 9)
    u.SetWindowPos(hwnd, 0, 0, 0, 0, 0, 0x0001 | 0x0004)
    u.SetForegroundWindow(hwnd)
    time.sleep(1.5)
    print("foreground is target:", u.GetForegroundWindow() == hwnd)

    h = k32.OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, False, proc.pid)
    track = scan_camera_track(h)
    app = find_app(h, track)
    print("track 0x%X app 0x%X" % (track, app))
    dump(h, app, "before")

    r = wt.RECT()
    u.GetClientRect(hwnd, ctypes.byref(r))
    o = wt.POINT(0, 0)
    u.ClientToScreen(hwnd, ctypes.byref(o))
    sx = o.x + int(r.right * 0.55)
    sy = o.y + int(r.bottom * 0.30)
    print("client %dx%d origin (%d,%d) -> drag at (%d,%d)" %
          (r.right, r.bottom, o.x, o.y, sx, sy))
    u.SetCursorPos(sx, sy)
    time.sleep(0.5)
    p = wt.POINT()
    u.GetCursorPos(ctypes.byref(p))
    print("cursor now", p.x, p.y)
    capture(hwnd, out + "_before.png")

    u.mouse_event(MOUSEEVENTF_RIGHTDOWN if gesture == "rmb" else MOUSEEVENTF_MIDDLEDOWN,
                  0, 0, 0, 0)
    time.sleep(0.2)
    dump(h, app, "right after button down")
    for i in range(1, 25):
        u.SetCursorPos(sx, sy + dy * i // 24)
        time.sleep(0.04)
    time.sleep(0.4)
    dump(h, app, "during drag")
    u.mouse_event(MOUSEEVENTF_RIGHTUP if gesture == "rmb" else MOUSEEVENTF_MIDDLEUP,
                  0, 0, 0, 0)
    time.sleep(1.2)
    dump(h, app, "after release")
    capture(hwnd, out + "_after.png")

    proc.terminate()
    return 0


if __name__ == "__main__":
    sys.exit(main())
