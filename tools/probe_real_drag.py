# -*- coding: utf-8 -*-
# probe_real_drag.py - bring a window to the foreground (with the sandbox's
# foreground-lock workaround), perform a REAL mouse drag over the 3D viewport,
# and save before/after PNGs.
#
# The workaround for SetForegroundWindow's foreground lock: press and release
# ALT, which temporarily clears the lock for the calling process.
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
k = ctypes.windll.kernel32

VK_MENU = 0x12
KEYEVENTF_KEYUP = 0x0002
MOUSEEVENTF_RIGHTDOWN, MOUSEEVENTF_RIGHTUP = 0x0008, 0x0010
MOUSEEVENTF_MIDDLEDOWN, MOUSEEVENTF_MIDDLEUP = 0x0020, 0x0040
MOUSEEVENTF_WHEEL = 0x0800


def force_foreground(hwnd):
    fg = u.GetForegroundWindow()
    tid = u.GetWindowThreadProcessId(hwnd, None)
    mytid = k.GetCurrentThreadId()
    attached = False
    if tid != mytid:
        attached = bool(u.AttachThreadInput(mytid, tid, True))
    u.keybd_event(VK_MENU, 0, 0, 0)
    u.keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0)
    u.BringWindowToTop(hwnd)
    u.SetForegroundWindow(hwnd)
    if attached:
        u.AttachThreadInput(mytid, tid, False)
    time.sleep(0.3)
    return u.GetForegroundWindow() == hwnd


def main():
    exe = os.path.abspath(sys.argv[1])
    prefix = os.path.abspath(sys.argv[2])
    gesture = sys.argv[3] if len(sys.argv) > 3 else "rmb"
    dx = int(sys.argv[4]) if len(sys.argv) > 4 else 0
    dy = int(sys.argv[5]) if len(sys.argv) > 5 else -160
    wait = float(sys.argv[6]) if len(sys.argv) > 6 else 10.0
    proc = subprocess.Popen([exe], cwd=os.path.dirname(exe))
    print("pid", proc.pid)
    time.sleep(wait)
    hits = find_window(proc.pid, None)
    hits.sort(key=lambda t: -t[2])
    hwnd = hits[0][0]
    print("foreground ok:", force_foreground(hwnd))
    time.sleep(1.5)

    r = wt.RECT()
    u.GetClientRect(hwnd, ctypes.byref(r))
    o = wt.POINT(0, 0)
    u.ClientToScreen(hwnd, ctypes.byref(o))
    x0, y0 = o.x + int(r.right * 0.6), o.y + int(r.bottom * 0.28)
    print("client %dx%d origin (%d,%d) drag (%d,%d)" %
          (r.right, r.bottom, o.x, o.y, x0, y0))
    u.SetCursorPos(x0, y0)
    time.sleep(0.5)
    capture(hwnd, prefix + "_before.png")

    if gesture == "wheel":
        for _ in range(3):
            u.mouse_event(MOUSEEVENTF_WHEEL, 0, 0, 120, 0)
            time.sleep(0.3)
    else:
        flag = MOUSEEVENTF_RIGHTDOWN if gesture == "rmb" else MOUSEEVENTF_MIDDLEDOWN
        upflag = MOUSEEVENTF_RIGHTUP if gesture == "rmb" else MOUSEEVENTF_MIDDLEUP
        u.mouse_event(flag, 0, 0, 0, 0)
        for i in range(1, 25):
            u.SetCursorPos(x0 + dx * i // 24, y0 + dy * i // 24)
            time.sleep(0.05)
        time.sleep(0.3)
        u.mouse_event(upflag, 0, 0, 0, 0)
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
