# -*- coding: utf-8 -*-
# shot_click.py - launch an MMD-family executable, click a control by its
# resource id (GetDlgItem on the main window), and save before/after PNGs plus
# the control's class/text.  Used to compare panel-mode views against the
# reference without needing foreground activation (BM_CLICK is a posted
# WM_COMMAND, which these Win32 panels honour).
#
# usage: python tools/shot_click.py <exe> <prefix> <ctrlId> [--wait N]
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
BM_CLICK = 0x00F5
WM_COMMAND = 0x0111


def main():
    exe = os.path.abspath(sys.argv[1])
    prefix = os.path.abspath(sys.argv[2])
    ctrl = int(sys.argv[3], 0)
    wait = float(sys.argv[sys.argv.index("--wait") + 1]) if "--wait" in sys.argv else 11.0
    proc = subprocess.Popen([exe], cwd=os.path.dirname(exe))
    print("pid", proc.pid)
    time.sleep(wait)
    hits = find_window(proc.pid, None)
    hits.sort(key=lambda t: -t[2])
    hwnd = hits[0][0]
    u.ShowWindow(hwnd, 9)
    u.SetWindowPos(hwnd, 0, 0, 0, 0, 0, 0x0001 | 0x0004)
    time.sleep(2.0)
    capture(hwnd, prefix + "_before.png")

    item = u.GetDlgItem(hwnd, ctrl)
    print("GetDlgItem(%d) -> %s" % (ctrl, item))
    if item:
        cls = ctypes.create_unicode_buffer(64)
        u.GetClassNameW(item, cls, 64)
        txt = ctypes.create_unicode_buffer(128)
        u.GetWindowTextW(item, txt, 128)
        print("  class=%s text=%r" % (cls.value, txt.value))
        # PostMessage, not SendMessage: the panel handler can pump a modal
        # dialog, which deadlocks a cross-process SendMessage.
        u.PostMessageW(item, BM_CLICK, 0, 0)
    time.sleep(2.5)
    capture(hwnd, prefix + "_after.png")
    proc.terminate()
    try:
        proc.wait(timeout=5)
    except Exception:
        proc.kill()
    return 0


if __name__ == "__main__":
    sys.exit(main())
