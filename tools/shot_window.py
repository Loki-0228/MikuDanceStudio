# -*- coding: utf-8 -*-
# shot_window.py - launch an MMD-family executable, wait, and save a PNG of
# its main window (PrintWindow with PW_RENDERFULLCONTENT, which captures
# Direct3D swapchain content on Windows 8.1+).  Used to compare the port's
# default viewport against the reference screenshots.
#
# usage: python tools/shot_window.py <exe> <out.png> [wait-seconds] [title]
import ctypes
import ctypes.wintypes as wt
import os
import struct
import subprocess
import sys
import time

sys.stdout.reconfigure(encoding="utf-8")


def find_window(pid, title=None):
    u = ctypes.windll.user32
    hits = []

    def cb(h, l):
        if not u.IsWindowVisible(h):
            return True
        owner = wt.DWORD(0)
        u.GetWindowThreadProcessId(h, ctypes.byref(owner))
        if owner.value == pid:
            buf = ctypes.create_unicode_buffer(512)
            u.GetWindowTextW(h, buf, 512)
            if title is None or title in buf.value:
                r = wt.RECT()
                u.GetWindowRect(h, ctypes.byref(r))
                area = (r.right - r.left) * (r.bottom - r.top)
                hits.append((h, buf.value, area))
        return True

    WNDENUMPROC = ctypes.WINFUNCTYPE(ctypes.c_bool, wt.HWND, wt.LPARAM)
    u.EnumWindows(WNDENUMPROC(cb), 0)
    return hits


def save_png(path, width, height, bgra):
    """Minimal PNG writer (BGRA -> RGB, no filtering)."""
    import zlib

    raw = bytearray()
    stride = width * 4
    for y in range(height):
        raw.append(0)
        row = bgra[y * stride:(y + 1) * stride]
        for x in range(width):
            b, g, r = row[x * 4], row[x * 4 + 1], row[x * 4 + 2]
            raw += bytes((r, g, b))

    def chunk(tag, data):
        out = struct.pack(">I", len(data)) + tag + data
        out += struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
        return out

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 6))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as fh:
        fh.write(png)


def capture(hwnd, path):
    u = ctypes.windll.user32
    g = ctypes.windll.gdi32
    rect = wt.RECT()
    u.GetWindowRect(hwnd, ctypes.byref(rect))
    w = rect.right - rect.left
    h = rect.bottom - rect.top
    hdc = u.GetWindowDC(hwnd)
    memdc = g.CreateCompatibleDC(hdc)
    bmp = g.CreateCompatibleBitmap(hdc, w, h)
    g.SelectObject(memdc, bmp)
    PW_RENDERFULLCONTENT = 0x00000002
    ok = u.PrintWindow(hwnd, memdc, PW_RENDERFULLCONTENT)
    if not ok:
        ok = u.PrintWindow(hwnd, memdc, 0)

    class BITMAPINFOHEADER(ctypes.Structure):
        _fields_ = [("biSize", wt.DWORD), ("biWidth", ctypes.c_long),
                    ("biHeight", ctypes.c_long), ("biPlanes", wt.WORD),
                    ("biBitCount", wt.WORD), ("biCompression", wt.DWORD),
                    ("biSizeImage", wt.DWORD), ("biXPelsPerMeter", ctypes.c_long),
                    ("biYPelsPerMeter", ctypes.c_long), ("biClrUsed", wt.DWORD),
                    ("biClrImportant", wt.DWORD)]

    bi = BITMAPINFOHEADER()
    bi.biSize = ctypes.sizeof(bi)
    bi.biWidth = w
    bi.biHeight = -h          # top-down
    bi.biPlanes = 1
    bi.biBitCount = 32
    bi.biCompression = 0
    buf = (ctypes.c_char * (w * h * 4))()
    got = g.GetDIBits(memdc, bmp, 0, h, buf, ctypes.byref(bi), 0)
    g.DeleteObject(bmp)
    g.DeleteDC(memdc)
    u.ReleaseDC(hwnd, hdc)
    if not got:
        print("GetDIBits failed")
        return False
    save_png(path, w, h, bytes(buf))
    print("saved %s (%dx%d) PrintWindow=%s" % (path, w, h, bool(ok)))
    return True


def main():
    exe = os.path.abspath(sys.argv[1])
    out = os.path.abspath(sys.argv[2])
    wait = float(sys.argv[3]) if len(sys.argv) > 3 else 8.0
    title = sys.argv[4] if len(sys.argv) > 4 else None
    proc = subprocess.Popen([exe], cwd=os.path.dirname(exe))
    print("launched", proc.pid)
    time.sleep(wait)
    if proc.poll() is not None:
        print("exited early", proc.returncode)
        return 1
    hits = find_window(proc.pid, title)
    print("windows:", hits)
    if not hits:
        proc.terminate()
        return 1
    # largest window is the main one
    hits.sort(key=lambda t: -t[2])
    hwnd = hits[0][0]
    u = ctypes.windll.user32
    # Park it at a known on-screen position so PrintWindow has real pixels.
    u.ShowWindow(hwnd, 9)          # SW_RESTORE
    u.SetWindowPos(hwnd, 0, 0, 0, 0, 0, 0x0001 | 0x0004 | 0x0010)  # NOSIZE|NOZORDER|NOACTIVATE
    u.SetForegroundWindow(hwnd)
    time.sleep(3.0)
    ok = capture(hwnd, out)
    # A second grab guards against a stale/partial first PrintWindow.
    time.sleep(1.0)
    ok = capture(hwnd, out.replace(".png", "-b.png")) and ok
    proc.terminate()
    try:
        proc.wait(timeout=5)
    except Exception:
        proc.kill()
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
