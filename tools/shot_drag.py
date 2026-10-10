# -*- coding: utf-8 -*-
# shot_drag.py - launch an MMD-family executable, optionally perform a real
# mouse drag inside the 3D viewport, and save before/after PNGs.  Used to
# verify viewport view operations (RMB orbit, MMB pan, wheel) empirically.
#
# usage:
#   python tools/shot_drag.py <exe> <outPrefix> [--gesture rmb|mmb|wheel|none]
#        [--wait 10] [--dx 0] [--dy -140] [--title MikuMikuDance]
#
# MMD polls the cursor position and the async button state each frame, so the
# gesture is driven with real input events (SetCursorPos + mouse_event).
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

MOUSEEVENTF_RIGHTDOWN = 0x0008
MOUSEEVENTF_RIGHTUP = 0x0010
MOUSEEVENTF_MIDDLEDOWN = 0x0020
MOUSEEVENTF_MIDDLEUP = 0x0040
MOUSEEVENTF_WHEEL = 0x0800


def force_foreground(hwnd):
    """SetForegroundWindow only works for the current foreground thread, so
    attach to the target's input queue first."""
    fg = u.GetForegroundWindow()
    if fg == hwnd:
        return True
    target_thread = u.GetWindowThreadProcessId(hwnd, None)
    this_thread = ctypes.windll.kernel32.GetCurrentThreadId()
    attached = False
    if target_thread != this_thread:
        attached = bool(u.AttachThreadInput(this_thread, target_thread, True))
    try:
        u.ShowWindow(hwnd, 9)
        u.SetWindowPos(hwnd, -1, 0, 0, 0, 0, 0x0001 | 0x0002 | 0x0040)
        u.SetForegroundWindow(hwnd)
        u.BringWindowToTop(hwnd)
        u.SetActiveWindow(hwnd)
        u.SetFocus(hwnd)
    finally:
        if attached:
            u.AttachThreadInput(this_thread, target_thread, False)
    return u.GetForegroundWindow() == hwnd


def arg(name, default=None):
    if name in sys.argv:
        return sys.argv[sys.argv.index(name) + 1]
    return default


def main():
    exe = os.path.abspath(sys.argv[1])
    prefix = os.path.abspath(sys.argv[2])
    wait = float(arg("--wait", "10"))
    gesture = arg("--gesture", "rmb")
    dx = int(arg("--dx", "0"))
    dy = int(arg("--dy", "-140"))
    title = arg("--title", None)

    proc = subprocess.Popen([exe], cwd=os.path.dirname(exe))
    print("launched", proc.pid)
    time.sleep(wait)
    if proc.poll() is not None:
        print("exited early", proc.returncode)
        return 1
    hits = find_window(proc.pid, title)
    if not hits:
        print("no window")
        proc.terminate()
        return 1
    hits.sort(key=lambda t: -t[2])
    hwnd = hits[0][0]
    ok = force_foreground(hwnd)
    print("foreground ok:", ok)
    time.sleep(2.0)

    r = wt.RECT()
    u.GetClientRect(hwnd, ctypes.byref(r))
    origin = wt.POINT(0, 0)
    u.ClientToScreen(hwnd, ctypes.byref(origin))
    cw, ch = r.right, r.bottom
    print("client %dx%d at (%d,%d)" % (cw, ch, origin.x, origin.y))

    # 3D viewport centre: left of the sidebar, above the bottom panel.
    view_x = int(cw * 0.62)
    view_y = int(ch * 0.35)
    sx, sy = origin.x + view_x, origin.y + view_y
    print("viewport point at screen (%d,%d)" % (sx, sy))

    capture(hwnd, prefix + "_before.png")

    if gesture != "none":
        u.SetCursorPos(sx, sy)
        time.sleep(0.4)
        if gesture in ("rmb", "mmb"):
            u.mouse_event(MOUSEEVENTF_RIGHTDOWN if gesture == "rmb"
                          else MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, 0)
            steps = 24
            for i in range(1, steps + 1):
                u.SetCursorPos(sx + dx * i // steps, sy + dy * i // steps)
                time.sleep(0.03)
            time.sleep(0.3)
            u.mouse_event(MOUSEEVENTF_RIGHTUP if gesture == "rmb"
                          else MOUSEEVENTF_MIDDLEUP, 0, 0, 0, 0)
        elif gesture == "wheel":
            notches = int(arg("--notches", "3"))
            for _ in range(notches):
                u.mouse_event(MOUSEEVENTF_WHEEL, 0, 0, 120, 0)
                time.sleep(0.3)
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
