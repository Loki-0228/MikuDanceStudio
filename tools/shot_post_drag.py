# -*- coding: utf-8 -*-
# shot_post_drag.py - drive a mouse drag into an MMD-family window with real
# posted messages (WM_*BUTTONDOWN / WM_MOUSEMOVE / WM_*BUTTONUP) and save
# before/after PNGs.  Works without foreground activation, which the sandbox
# cannot grant, so it can exercise the port's own WM_MOUSEMOVE-driven path.
#
# usage: python tools/shot_post_drag.py <exe> <outPrefix> [rmb|mmb|wheel|lmb]
#        [dx] [dy] [--wait N] [--at fx,fy] [--ctrl] [--shift]
import ctypes
import ctypes.wintypes as wt
import os
import subprocess
import sys
import time

sys.stdout.reconfigure(encoding="utf-8")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shot_window import capture, find_window  # noqa: E402

u = ctypes.windll.user32

WM_MOUSEMOVE = 0x0200
WM_LBUTTONDOWN, WM_LBUTTONUP = 0x0201, 0x0202
WM_RBUTTONDOWN, WM_RBUTTONUP = 0x0204, 0x0205
WM_MBUTTONDOWN, WM_MBUTTONUP = 0x0207, 0x0208
WM_MOUSEWHEEL = 0x020A
MK_LBUTTON, MK_RBUTTON, MK_MBUTTON = 0x0001, 0x0002, 0x0010
MK_CONTROL, MK_SHIFT = 0x0008, 0x0004


def lparam(x, y):
    return (y << 16) | (x & 0xFFFF)


def main():
    exe = os.path.abspath(sys.argv[1])
    prefix = os.path.abspath(sys.argv[2])
    gesture = sys.argv[3] if len(sys.argv) > 3 else "rmb"
    dx = int(sys.argv[4]) if len(sys.argv) > 4 else 0
    dy = int(sys.argv[5]) if len(sys.argv) > 5 else -160

    def opt(name, default):
        return sys.argv[sys.argv.index(name) + 1] if name in sys.argv else default

    wait = float(opt("--wait", "10"))
    at = opt("--at", "0.62,0.30")
    fx, fy = (float(v) for v in at.split(","))
    mods = 0
    if "--ctrl" in sys.argv:
        mods |= MK_CONTROL
    if "--shift" in sys.argv:
        mods |= MK_SHIFT

    proc = subprocess.Popen([exe], cwd=os.path.dirname(exe))
    print("launched", proc.pid)
    time.sleep(wait)
    if proc.poll() is not None:
        print("exited early", proc.returncode)
        return 1
    hits = find_window(proc.pid, None)
    if not hits:
        print("no window")
        proc.terminate()
        return 1
    hits.sort(key=lambda t: -t[2])
    hwnd = hits[0][0]

    r = wt.RECT()
    u.GetClientRect(hwnd, ctypes.byref(r))
    x0, y0 = int(r.right * fx), int(r.bottom * fy)
    print("client %dx%d drag origin (%d,%d)" % (r.right, r.bottom, x0, y0))

    capture(hwnd, prefix + "_before.png")

    # Move in first so the stored position is current.
    u.PostMessageW(hwnd, WM_MOUSEMOVE, mods, lparam(x0, y0))
    time.sleep(0.4)

    down, up, flag = {
        "lmb": (WM_LBUTTONDOWN, WM_LBUTTONUP, MK_LBUTTON),
        "rmb": (WM_RBUTTONDOWN, WM_RBUTTONUP, MK_RBUTTON),
        "mmb": (WM_MBUTTONDOWN, WM_MBUTTONUP, MK_MBUTTON),
    }.get(gesture, (0, 0, 0))

    if gesture == "wheel":
        for _ in range(int(opt("--notches", "3"))):
            screen = wt.POINT(x0, y0)
            u.ClientToScreen(hwnd, ctypes.byref(screen))
            u.PostMessageW(hwnd, WM_MOUSEWHEEL, (120 << 16) | mods,
                           lparam(screen.x, screen.y))
            time.sleep(0.25)
    else:
        u.PostMessageW(hwnd, down, mods | flag, lparam(x0, y0))
        steps = 24
        for i in range(1, steps + 1):
            u.PostMessageW(hwnd, WM_MOUSEMOVE, mods | flag,
                           lparam(x0 + dx * i // steps, y0 + dy * i // steps))
            time.sleep(0.04)
        time.sleep(0.4)
        u.PostMessageW(hwnd, up, mods, lparam(x0 + dx, y0 + dy))

    time.sleep(1.5)
    capture(hwnd, prefix + "_after.png")
    proc.terminate()
    try:
        proc.wait(timeout=5)
    except Exception:
        proc.kill()
    return 0


if __name__ == "__main__":
    sys.exit(main())
