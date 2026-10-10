#!/usr/bin/env python3
"""Background acceptance probe for the real MMS/MMV Win32 UI.

The tool starts a real MikuMikuDance.exe, drives it through its own menus and
buttons, and records what the actual windows look like and how they behave:

* the process is started with STARTUPINFO.dwFlags |= STARTF_USESHOWWINDOW and
  wShowWindow = SW_HIDE, which the host honours for its main window
  (src/window/window_init.cpp:437).  The host main window is therefore located
  by its exact class name "Polygon Movie Maker" plus this tool's own PID, never
  by visibility, and it is never moved off-screen.  Every window this tool
  causes to appear (the MMV editor, the parameter popup, the host's own message
  boxes) is hidden as soon as it is found, before anything else is done with
  it.  Running on a private desktop (--desktop private) remains available, but
  D3D9 cannot create a device for a window there on this machine, so the host
  stops with its own "CreateDevice Failed!" box;
* every capture is taken from the real HWND into a memory DIB and saved as PNG;
  the desktop is never grabbed with a screen capture API.  A window the plugin
  keeps hidden because its owner is hidden is asked to print itself with
  PrintWindow exactly as it is first.  Measured on this build that returns an
  all-black frame, WM_PRINT returns all-white, and printing each real child and
  compositing returns all-white too, so the window is then shown for the length
  of the capture only, parked at -4000,-4000 where no monitor reaches, and its
  hidden state is put back straight afterwards.  Every route, its ink ratio and
  the number of children composited are recorded in run.json, and a frame only
  counts as content when its ink share is between 0.02 and 0.98;
* the drop-down defect is measured by expanding the box with CB_SHOWDROPDOWN and
  then sampling CB_GETDROPPEDSTATE across consecutive host frames (each frame is
  forced with InvalidateRect + UpdateWindow on the host window), recording when
  the list closed and how long it stayed open;
* cross-process text follows the message marshalling rules: WM_GETTEXT and
  WM_SETTEXT are below WM_USER, so the kernel copies the buffer between
  processes and the pointer must be a local one; only LVM_GETITEMTEXT, which is
  above WM_USER, needs an LVITEM and a text buffer allocated inside the target;
* the MMV allocation window is opened through the host menu command the plugin
  registers (32601), a real .fx effect is assigned through the editor's own
  "Set effect" file dialog, a row is expanded and the parameter popup is opened
  through the "Parameters" button.  The shell's own Open dialog is the one
  window this tool cannot operate while hidden - its DirectUI controls ignore
  synthetic input on an invisible dialog - so it is the single window that
  stays on screen for the second or two it is open, and run.json says so;
* the probe records HWND, class, style, exstyle, rect, parent, owner,
  WM_NCHITTEST results (HTCAPTION / border codes), whether SetWindowPos keeps
  the requested position, and what an SC_MOVE keyboard move does;
* a crash box from the host is recorded with its own wording, captured, and
  reported as a host failure - it is never mistaken for a file dialog and the
  steps after it are marked skipped rather than reported as passing;
* the editor is captured at 800x640 and at a narrow width in all three UI
  languages.

Only processes started by this tool are terminated, and only the desktop it
created is closed.

Usage
-----
  python tools/run_mmv_layout_probe.py --exe <host>/MikuMikuDance.exe \
      --output .local/artifacts/mmv-curve/ui-probe [--model X.pmx] \
      [--effect X.fx] [--phase all|stage|open|matrix|drag] [--budget 55]

The run directory is <output>/<run-id>/ with run.json, run.log and shots/.
Each invocation stays inside --budget seconds (default 55); steps that do not
fit are recorded as "budget" and can be run in a later invocation with --phase.
"""

import argparse
import ctypes
import hashlib
import json
import os
import shutil
import sys
import time
from ctypes import wintypes
from pathlib import Path

if os.name != "nt":
    sys.exit("run_mmv_layout_probe.py is Windows only")

# Real model and effect names carry characters the console code page cannot
# encode (the console is GBK here), which would kill the run at the first print.
for stream in (sys.stdout, sys.stderr):
    if hasattr(stream, "reconfigure"):
        stream.reconfigure(encoding="utf-8", errors="replace")

user32 = ctypes.WinDLL("user32", use_last_error=True)
kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
gdi32 = ctypes.WinDLL("gdi32", use_last_error=True)

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------
WM_COMMAND = 0x0111
WM_CLOSE = 0x0010
WM_KEYDOWN = 0x0100
WM_KEYUP = 0x0101
WM_SYSCOMMAND = 0x0112
WM_NCHITTEST = 0x0084
WM_DROPFILES = 0x0233
WM_GETTEXT = 0x000D
WM_SETTEXT = 0x000C
WM_GETTEXTLENGTH = 0x000E
WM_PRINT = 0x0317
WM_PRINTCLIENT = 0x0318

SC_MOVE = 0xF010
HTCAPTION = 2
HTCLIENT = 1
HTCODES = {
    0: "HTNOWHERE", 1: "HTCLIENT", 2: "HTCAPTION", 3: "HTSYSMENU",
    4: "HTGROWBOX", 5: "HTMENU", 6: "HTHSCROLL", 7: "HTVSCROLL",
    8: "HTMINBUTTON", 9: "HTMAXBUTTON", 10: "HTLEFT", 11: "HTRIGHT",
    12: "HTTOP", 13: "HTTOPLEFT", 14: "HTTOPRIGHT", 15: "HTBOTTOM",
    16: "HTBOTTOMLEFT", 17: "HTBOTTOMRIGHT", 18: "HTBORDER", 19: "HTCLOSE",
}

GWL_STYLE = -16
GWL_EXSTYLE = -20
GWL_ID = -12
GW_OWNER = 4
GW_CHILD = 5
GW_HWNDNEXT = 2

# Captions the host uses for its own crash box.  When one of these shows up the
# run records it as a host failure instead of reporting the next step as fine.
FATAL_TITLES = ("Fatal error", "致命错误", "应用程序错误", "アプリケーション エラー")

# The host main window class, matched exactly together with this tool's own PID.
# The window is created hidden, so visibility cannot be used to find it.
HOST_MAIN_CLASS = "Polygon Movie Maker"

# Modules that make up the MME graphics plug-in.  --without-mme keeps them out
# of the staged copy so the run is MMV-only for UI work.
MME_MODULES = ("d3d9.dll", "mmeffect.dll", "mmhack.dll")

STARTF_USESHOWWINDOW = 0x00000001
SW_HIDE = 0
SW_SHOW = 5
BM_CLICK = 0x00F5

SWP_NOZORDER = 0x0004
SWP_NOACTIVATE = 0x0010
SWP_NOSIZE = 0x0001
SWP_NOMOVE = 0x0002
SWP_SHOWWINDOW = 0x0040

GA_PARENT = 1
GA_ROOT = 2

PW_CLIENTONLY = 0x00000001
PW_RENDERFULLCONTENT = 0x00000002
PRF_CHECKVISIBLE = 0x00000001
PRF_NONCLIENT = 0x00000002
PRF_CLIENT = 0x00000004
PRF_ERASEBKGND = 0x00000008
PRF_CHILDREN = 0x00000010
PRF_OWNED = 0x00000020

DIB_RGB_COLORS = 0

# A capture is only trusted when a meaningful share of it is inked: an all-white
# frame (0.0) and an all-black one (1.0) both mean nothing was drawn, and a
# handful of stray pixels is not a layout either.
INK_MIN = 0.02
INK_MAX = 0.98
BI_RGB = 0
MEM_COMMIT = 0x1000
MEM_RESERVE = 0x2000
MEM_RELEASE = 0x8000
PAGE_READWRITE = 0x04
PROCESS_ALL_ACCESS = 0x1F0FFF
PROCESS_QUERY_LIMITED = 0x1000
PROCESS_VM_READ = 0x0010
PROCESS_VM_WRITE = 0x0020
PROCESS_VM_OPERATION = 0x0008
PROCESS_QUERY_INFORMATION = 0x0400
# Ask only for what remote text reading needs: PROCESS_ALL_ACCESS is refused on
# some machines and then every cross-process read would silently return None.
REMOTE_PROCESS_RIGHTS = (PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION
                         | PROCESS_QUERY_INFORMATION)

DESKTOP_ALL_ACCESS = 0x000F01FF
CREATE_NEW_CONSOLE = 0x00000010

LVM_FIRST = 0x1000
LVM_GETITEMCOUNT = LVM_FIRST + 4
LVM_SETITEMSTATE = LVM_FIRST + 43
LVM_GETITEMTEXTA = LVM_FIRST + 45
LVM_GETITEMTEXTW = LVM_FIRST + 115
LVIS_FOCUSED = 0x0001
LVIS_SELECTED = 0x0002
LVIF_TEXT = 0x0001
LVIF_STATE = 0x0008

# Combo box messages for the drop-down probe.  The reported defect is that an
# expanded list closes again on every host frame, so the state is sampled across
# consecutive frames with timestamps.
CB_GETCOUNT = 0x0146
CB_GETCURSEL = 0x0147
CB_SHOWDROPDOWN = 0x014F
CB_GETDROPPEDSTATE = 0x0157
WM_PAINT = 0x000F

# The parameter panel's curve page must own these two children.  The plugin
# creates them with CreateWindowEx; if the class was registered against the DLL
# instance but the create call passes the EXE instance, they never appear.
MMV_PARAM_CANVAS_ID = 1020
MMV_PARAM_MMD_PANEL_ID = 1031
TCM_SETCURSEL = 0x130C
WS_VISIBLE = 0x10000000

# MMV plugin control ids (src/mmv/editor.cpp, enum EditorControlId).
MMV_ID_LIST = 1001
MMV_ID_DETAIL = 1003
MMV_ID_STATUS = 1004
MMV_ID_INFO = 1005
MMV_ID_SET_EFFECT = 1006
MMV_ID_PARAMETERS = 1011
MMV_ID_EXPAND = 1019
MMV_ID_EXPAND_BONES = 1020
MMV_ID_EXPAND_MATERIALS = 1021
MMV_EDITOR_CLASS = "MMVEditorWindow"
MMV_PARAM_CLASS = "MMVParameterEditorWindow"
MMV_PERSPECTIVE_CLASS = "MMVPerspectiveEditorWindow"
# Host menu: the plugin inserts popup 32600 whose single entry is 32601.
MMV_MENU_COMMAND = 32601
# Host command 260 cycles the UI language 0 (Japanese) -> 1 (English) -> 2 (Chinese).
LANGUAGE_COMMAND = 260
# Common file dialog: filename edit and OK.
FN_EDIT_ID = 0x480
# The shell's file dialog puts the file-name field in the cmb13 combo, so its
# edit reports 0x47C rather than the classic edt1 id 0x480.
FN_COMBO_ID = 0x47C
FILENAME_EDIT_IDS = (FN_EDIT_ID, FN_COMBO_ID)
IDOK = 1
IDCANCEL = 2

user32.SendMessageTimeoutW.restype = ctypes.c_long
user32.SendMessageTimeoutW.argtypes = [
    wintypes.HWND, wintypes.UINT, ctypes.c_size_t, ctypes.c_ssize_t,
    wintypes.UINT, wintypes.UINT, ctypes.POINTER(ctypes.c_size_t),
]
user32.SendMessageW.restype = ctypes.c_ssize_t
user32.SendMessageW.argtypes = [wintypes.HWND, wintypes.UINT, ctypes.c_size_t, ctypes.c_ssize_t]
user32.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, ctypes.c_size_t, ctypes.c_ssize_t]
user32.GetWindowLongPtrW.restype = ctypes.c_ssize_t
user32.GetWindowLongPtrW.argtypes = [wintypes.HWND, ctypes.c_int]
user32.SetWindowLongPtrW.restype = ctypes.c_ssize_t
user32.SetWindowLongPtrW.argtypes = [wintypes.HWND, ctypes.c_int, ctypes.c_ssize_t]
user32.SetWindowPos.argtypes = [
    wintypes.HWND, wintypes.HWND, ctypes.c_int, ctypes.c_int, ctypes.c_int,
    ctypes.c_int, wintypes.UINT,
]
user32.PrintWindow.argtypes = [wintypes.HWND, wintypes.HDC, wintypes.UINT]
user32.GetDC.argtypes = [wintypes.HWND]
user32.ReleaseDC.argtypes = [wintypes.HWND, wintypes.HDC]
user32.GetWindowRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
user32.GetClientRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
user32.GetWindowThreadProcessId.restype = wintypes.DWORD
user32.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
user32.IsWindow.argtypes = [wintypes.HWND]
user32.IsWindowVisible.argtypes = [wintypes.HWND]
user32.IsWindowEnabled.argtypes = [wintypes.HWND]
user32.GetParent.restype = wintypes.HWND
user32.GetParent.argtypes = [wintypes.HWND]
user32.GetClassNameW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
user32.GetWindowTextW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
user32.GetWindow.restype = wintypes.HWND
user32.GetWindow.argtypes = [wintypes.HWND, wintypes.UINT]
user32.ShowWindow.argtypes = [wintypes.HWND, ctypes.c_int]
user32.GetAncestor.restype = wintypes.HWND
user32.GetAncestor.argtypes = [wintypes.HWND, wintypes.UINT]
user32.InvalidateRect.argtypes = [wintypes.HWND, ctypes.c_void_p, wintypes.BOOL]
user32.UpdateWindow.argtypes = [wintypes.HWND]
gdi32.CreateCompatibleDC.restype = wintypes.HDC
gdi32.CreateCompatibleDC.argtypes = [wintypes.HDC]
gdi32.SelectObject.restype = wintypes.HGDIOBJ
gdi32.SelectObject.argtypes = [wintypes.HDC, wintypes.HGDIOBJ]
gdi32.DeleteObject.argtypes = [wintypes.HGDIOBJ]
gdi32.DeleteDC.argtypes = [wintypes.HDC]
gdi32.PatBlt.argtypes = [wintypes.HDC, ctypes.c_int, ctypes.c_int, ctypes.c_int,
                        ctypes.c_int, wintypes.DWORD]
gdi32.CreateDIBSection.restype = wintypes.HBITMAP
gdi32.CreateDIBSection.argtypes = [
    wintypes.HDC, ctypes.c_void_p, wintypes.UINT, ctypes.POINTER(ctypes.c_void_p),
    wintypes.HANDLE, wintypes.DWORD,
]
kernel32.GlobalAlloc.restype = wintypes.HGLOBAL
kernel32.GlobalAlloc.argtypes = [wintypes.UINT, ctypes.c_size_t]
kernel32.GlobalLock.restype = ctypes.c_void_p
kernel32.GlobalLock.argtypes = [wintypes.HGLOBAL]
kernel32.GlobalUnlock.argtypes = [wintypes.HGLOBAL]
kernel32.OpenProcess.restype = wintypes.HANDLE
kernel32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
kernel32.VirtualAllocEx.restype = ctypes.c_void_p
kernel32.VirtualAllocEx.argtypes = [wintypes.HANDLE, ctypes.c_void_p, ctypes.c_size_t,
                                    wintypes.DWORD, wintypes.DWORD]
kernel32.VirtualFreeEx.argtypes = [wintypes.HANDLE, ctypes.c_void_p, ctypes.c_size_t,
                                   wintypes.DWORD]
kernel32.WriteProcessMemory.argtypes = [wintypes.HANDLE, ctypes.c_void_p, ctypes.c_void_p,
                                        ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
kernel32.ReadProcessMemory.argtypes = [wintypes.HANDLE, ctypes.c_void_p, ctypes.c_void_p,
                                       ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
kernel32.CreateProcessW.argtypes = [
    wintypes.LPCWSTR, wintypes.LPWSTR, ctypes.c_void_p, ctypes.c_void_p, wintypes.BOOL,
    wintypes.DWORD, ctypes.c_void_p, wintypes.LPCWSTR, ctypes.c_void_p, ctypes.c_void_p,
]
user32.CreateDesktopW.restype = wintypes.HANDLE
user32.CreateDesktopW.argtypes = [wintypes.LPCWSTR, wintypes.LPCWSTR, ctypes.c_void_p,
                                  wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p]
user32.CloseDesktop.argtypes = [wintypes.HANDLE]


# EnumWindows / EnumChildWindows / EnumDesktopWindows callback type.  The
# factory result is used directly: subclassing it needs _flags_ on 3.14+.
WNDENUMPROC = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
user32.EnumDesktopWindows.argtypes = [wintypes.HANDLE, WNDENUMPROC, wintypes.LPARAM]
user32.EnumWindows.argtypes = [WNDENUMPROC, wintypes.LPARAM]
user32.EnumChildWindows.argtypes = [wintypes.HWND, WNDENUMPROC, wintypes.LPARAM]


class DROPFILES(ctypes.Structure):
    _fields_ = [
        ("pFiles", wintypes.DWORD),
        ("pt", wintypes.POINT),
        ("fNC", wintypes.BOOL),
        ("fWide", wintypes.BOOL),
    ]


class LVITEMW(ctypes.Structure):
    _fields_ = [
        ("mask", wintypes.UINT),
        ("iItem", ctypes.c_int),
        ("iSubItem", ctypes.c_int),
        ("state", wintypes.UINT),
        ("stateMask", wintypes.UINT),
        ("pszText", ctypes.c_void_p),
        ("cchTextMax", ctypes.c_int),
        ("iImage", ctypes.c_int),
        ("lParam", ctypes.c_void_p),
        ("iIndent", ctypes.c_int),
        ("iGroupId", ctypes.c_int),
        ("cColumns", wintypes.UINT),
        ("puColumns", ctypes.c_void_p),
        ("piColFmt", ctypes.c_void_p),
        ("iGroup", ctypes.c_int),
    ]


class BITMAPINFOHEADER(ctypes.Structure):
    _fields_ = [
        ("biSize", wintypes.DWORD),
        ("biWidth", ctypes.c_long),
        ("biHeight", ctypes.c_long),
        ("biPlanes", wintypes.WORD),
        ("biBitCount", wintypes.WORD),
        ("biCompression", wintypes.DWORD),
        ("biSizeImage", wintypes.DWORD),
        ("biXPelsPerMeter", ctypes.c_long),
        ("biYPelsPerMeter", ctypes.c_long),
        ("biClrUsed", wintypes.DWORD),
        ("biClrImportant", wintypes.DWORD),
    ]


class BITMAPINFO(ctypes.Structure):
    _fields_ = [("bmiHeader", BITMAPINFOHEADER), ("bmiColors", wintypes.DWORD * 3)]


class STARTUPINFOW(ctypes.Structure):
    _fields_ = [
        ("cb", wintypes.DWORD), ("lpReserved", wintypes.LPWSTR),
        ("lpDesktop", wintypes.LPWSTR), ("lpTitle", wintypes.LPWSTR),
        ("dwX", wintypes.DWORD), ("dwY", wintypes.DWORD),
        ("dwXSize", wintypes.DWORD), ("dwYSize", wintypes.DWORD),
        ("dwXCountChars", wintypes.DWORD), ("dwYCountChars", wintypes.DWORD),
        ("dwFillAttribute", wintypes.DWORD), ("dwFlags", wintypes.DWORD),
        ("wShowWindow", wintypes.WORD), ("cbReserved2", wintypes.WORD),
        ("lpReserved2", ctypes.POINTER(ctypes.c_byte)),
        ("hStdInput", wintypes.HANDLE), ("hStdOutput", wintypes.HANDLE),
        ("hStdError", wintypes.HANDLE),
    ]


class PROCESS_INFORMATION(ctypes.Structure):
    _fields_ = [
        ("hProcess", wintypes.HANDLE), ("hThread", wintypes.HANDLE),
        ("dwProcessId", wintypes.DWORD), ("dwThreadId", wintypes.DWORD),
    ]


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


class Probe:
    """One probe run: staging, launch, driving the real UI, capture, report."""

    def __init__(self, args):
        self.args = args
        self.start = time.time()
        self.output = Path(args.output).resolve()
        run_id = args.run_id or time.strftime("%Y%m%d-%H%M%S")
        self.run_dir = self.output / run_id
        self.shots = self.run_dir / "shots"
        self.log_path = self.run_dir / "run.log"
        self.report_path = self.run_dir / "run.json"
        self.log_lines = []
        self.report = {
            "runId": run_id,
            "startedAt": time.strftime("%Y-%m-%d %H:%M:%S"),
            "cwd": os.getcwd(),
            "python": sys.version.split()[0],
            "exe": None,
            "desktop": args.desktop,
            "phase": args.phase,
            "budgetSeconds": args.budget,
            "steps": [],
            "windows": [],
            "captures": [],
            "files": {},
            "errors": [],
            "skipped": [],
            "limitations": [],
        }
        self.desktop = None
        self.desktop_name = None
        self.process = None
        self.pid = None
        self.main = None
        self.staged = None
        self.phase_failed = False
        self.host_fatal = False
        self.remote_alloc_failed = False
        self.hidden_printwindow_ok = False

    # -- logging ------------------------------------------------------------
    def log(self, message):
        stamp = f"[{time.time() - self.start:6.2f}s]"
        line = f"{stamp} {message}"
        self.log_lines.append(line)
        print(line, flush=True)

    def step(self, name, status, detail=None):
        entry = {"name": name, "status": status, "at": round(time.time() - self.start, 3)}
        if detail:
            entry["detail"] = detail
        self.report["steps"].append(entry)
        self.log(f"STEP {name}: {status}" + (f" - {detail}" if detail else ""))
        return entry

    def error(self, message):
        self.report["errors"].append({"at": round(time.time() - self.start, 3), "message": message})
        self.log(f"ERROR {message}")

    def remaining(self):
        return self.args.budget - (time.time() - self.start)

    def budget_ok(self, need=2.0):
        if self.remaining() < need:
            self.log(f"budget exhausted ({self.remaining():.1f}s left), stopping")
            return False
        return True

    # -- staging ------------------------------------------------------------
    def stage(self):
        """Copy the exe directory into the run directory, then overlay
        <release>/compat/MMV.dll when the exe sits in a release host folder.
        With --without-mme the MME modules are left out of the copy, so the run
        is MMV-only for UI work and the host falls back to the system DirectX 9.
        The source release tree is never modified."""
        exe = Path(self.args.exe).resolve()
        if not exe.is_file():
            self.error(f"exe not found: {exe}")
            return False
        source_dir = exe.parent
        target = self.run_dir / "run" / "host"
        if target.exists():
            shutil.rmtree(target)
        target.parent.mkdir(parents=True, exist_ok=True)
        excluded = []
        if self.args.without_mme:
            target.mkdir(parents=True, exist_ok=True)
            for item in source_dir.iterdir():
                if item.name.lower() in MME_MODULES:
                    excluded.append(item.name)
                    continue
                if item.is_dir():
                    shutil.copytree(item, target / item.name)
                else:
                    shutil.copy2(item, target / item.name)
        else:
            shutil.copytree(source_dir, target)
        staged_exe = target / exe.name
        compat = source_dir.parent / "compat" / "MMV.dll"
        overlay = None
        if compat.is_file():
            shutil.copy2(compat, target / "MMV.dll")
            overlay = str(compat)
        self.staged = staged_exe
        self.report["exe"] = str(staged_exe)
        self.report["sourceExe"] = str(exe)
        self.report["mmeModules"] = {
            "withoutMme": bool(self.args.without_mme),
            "excluded": excluded,
            "note": ("MMV-only staging: the MME modules are absent, so the host "
                     "uses the system DirectX 9 and no MME rendering happens. "
                     "Real MME rendering is a separate acceptance run."
                     if self.args.without_mme else "full staging, MME present"),
        }
        self.report["files"]["stagedExe"] = {"path": str(staged_exe), "sha256": sha256(staged_exe)}
        host_mmv = target / "MMV.dll"
        if host_mmv.is_file():
            self.report["files"]["stagedMmv"] = {"path": str(host_mmv), "sha256": sha256(host_mmv)}
        self.report["compatOverlay"] = overlay
        self.step("stage", "ok",
                  f"{target} (compat overlay: {overlay or 'none'}; "
                  f"MME modules excluded: {excluded or 'none'})")
        return True

    def isolate_accessory(self, accessory):
        """Copy the accessory on its own into this run's asset directory.  A
        drop of <name>.x from its original folder also exposes the same-named
        <name>.fx, which MMD hands to MME; that combination crashed the host
        inside MMEffect.dll.  Only the accessory file is copied and the source
        folder is never touched, so nothing is deleted from the material."""
        source = Path(accessory).resolve()
        assets = self.run_dir / "assets"
        assets.mkdir(parents=True, exist_ok=True)
        isolated = assets / source.name
        shutil.copy2(source, isolated)
        siblings = sorted(item.name for item in source.parent.iterdir()
                          if item.is_file() and item.name != source.name)
        same_stem_fx = [name for name in siblings
                        if Path(name).stem.lower() == source.stem.lower()
                        and name.lower().endswith(".fx")]
        self.report["accessoryIsolation"] = {
            "source": str(source),
            "isolated": str(isolated),
            "sameStemFxInSourceFolder": same_stem_fx,
            "siblingsNotCopied": siblings,
            "note": "only the accessory file itself is copied; a same-named .fx "
                    "is the MME effect that crashed the host and is deliberately "
                    "left behind, and the source folder is never modified",
        }
        self.log(f"ISOLATED accessory -> {isolated} (same-stem .fx left behind: "
                 f"{same_stem_fx or 'none'})")
        return isolated

    # -- desktop + process --------------------------------------------------
    def create_desktop(self):
        if self.args.desktop != "private":
            self.desktop_name = None
            self.step("desktop", "ok", "default desktop (operator desktop is used)")
            return True
        self.desktop_name = f"MMVProbe-{os.getpid()}-{int(time.time())}"
        self.desktop = user32.CreateDesktopW(self.desktop_name, None, None, 0,
                                             DESKTOP_ALL_ACCESS, None)
        if not self.desktop:
            err = ctypes.get_last_error()
            self.error(f"CreateDesktopW failed (err={err}); falling back to the default desktop")
            self.desktop_name = None
            self.step("desktop", "fallback", f"CreateDesktopW err={err}")
            return True
        self.step("desktop", "ok", f"private desktop {self.desktop_name}")
        return True

    def launch(self):
        exe = self.staged
        cmdline = f'"{exe}"'
        startup = STARTUPINFOW()
        startup.cb = ctypes.sizeof(startup)
        if self.desktop_name:
            startup.lpDesktop = self.desktop_name
        # Background discipline: ask the host for a hidden main window through
        # the ordinary nCmdShow path, which src/window/window_init.cpp:437
        # honours for SW_HIDE.  Nothing is moved off-screen afterwards.
        startup.dwFlags |= STARTF_USESHOWWINDOW
        startup.wShowWindow = SW_HIDE
        info = PROCESS_INFORMATION()
        flags = CREATE_NEW_CONSOLE
        ok = kernel32.CreateProcessW(str(exe), ctypes.c_wchar_p(cmdline), None, None, False,
                                     flags, None, str(exe.parent), ctypes.byref(startup),
                                     ctypes.byref(info))
        if not ok:
            err = ctypes.get_last_error()
            self.error(f"CreateProcessW failed (err={err})")
            return False
        self.process = info
        self.pid = info.dwProcessId
        self.step("launch", "ok", f"pid={self.pid} desktop={self.desktop_name or 'default'}")
        return True

    # -- window discovery ---------------------------------------------------
    def enum_windows(self, root=None):
        found = []

        def collect(hwnd, _):
            found.append(hwnd)
            return True

        callback = WNDENUMPROC(collect)
        if root:
            user32.EnumChildWindows(root, callback, 0)
        elif self.desktop:
            user32.EnumDesktopWindows(self.desktop, callback, 0)
        else:
            user32.EnumWindows(callback, 0)
        return [h for h in found if h]

    def process_windows(self):
        result = []
        for hwnd in self.enum_windows():
            pid = wintypes.DWORD(0)
            user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
            if pid.value == self.pid:
                result.append(hwnd)
        return result

    def class_name(self, hwnd):
        buffer = ctypes.create_unicode_buffer(256)
        user32.GetClassNameW(hwnd, buffer, 256)
        return buffer.value

    def title(self, hwnd):
        buffer = ctypes.create_unicode_buffer(512)
        user32.GetWindowTextW(hwnd, buffer, 512)
        return buffer.value

    def rect(self, hwnd):
        r = wintypes.RECT()
        user32.GetWindowRect(hwnd, ctypes.byref(r))
        return [r.left, r.top, r.right - r.left, r.bottom - r.top]

    def find_top(self, class_names=None, timeout=30.0, title_contains=None, largest=False,
                 skip_dialogs=True, visible_required=True):
        deadline = time.time() + timeout
        while time.time() < deadline:
            if self.remaining() < 1:
                return None
            if self.check_fatal("waiting for a window"):
                return None
            candidates = []
            for hwnd in self.process_windows():
                if visible_required and user32.IsWindowVisible(hwnd) == 0:
                    continue
                name = self.class_name(hwnd)
                if class_names and name not in class_names:
                    continue
                if skip_dialogs and not class_names and name == "#32770":
                    continue  # startup/error boxes are not the main window
                if title_contains and title_contains not in self.title(hwnd):
                    continue
                candidates.append(hwnd)
            if candidates:
                if largest:
                    return max(candidates, key=lambda h: self.rect(h)[2] * self.rect(h)[3])
                return candidates[0]
            time.sleep(0.25)
        return None

    def dialog_report(self, dialog):
        """Caption plus every child control text, so an unexpected startup box
        is recorded with the wording the host itself produced."""
        children = []
        for child in self.children(dialog):
            children.append({
                "class": self.class_name(child),
                "id": user32.GetWindowLongPtrW(child, GWL_ID),
                "text": self.remote_text(child, 512) or self.title(child),
            })
        return {"hwnd": f"0x{dialog:X}", "class": self.class_name(dialog),
                "title": self.title(dialog), "rect": self.rect(dialog),
                "children": children}

    def note_dialogs(self, where):
        for hwnd in self.process_windows():
            if self.class_name(hwnd) == "#32770" and user32.IsWindowVisible(hwnd):
                entry = self.dialog_report(hwnd)
                entry["where"] = where
                self.report.setdefault("dialogs", []).append(entry)
                self.log(f"DIALOG at {where}: {json.dumps(entry, ensure_ascii=False)}")
                return hwnd
        return None

    def record_startup_blocker(self):
        """A Direct3D device that cannot be created stops the host before its
        main window exists; the wording of its own message box is the evidence."""
        text = json.dumps(self.report.get("dialogs", []), ensure_ascii=False)
        if "CreateDevice Failed" in text:
            self.report["startupBlocker"] = {
                "kind": "Direct3D device creation failed",
                "desktop": self.desktop_name or "default",
                "hostMessage": "CreateDevice Failed!",
                "hint": "run again with --desktop default: D3D9 cannot create a "
                        "device for a window on this private desktop",
            }
            self.error("host reported 'CreateDevice Failed!' - D3D9 cannot create a device "
                       "on this desktop; retry with --desktop default")

    # -- host crash detection ----------------------------------------------
    def find_fatal_dialog(self):
        for hwnd in self.process_windows():
            if self.class_name(hwnd) != "#32770" or not user32.IsWindowVisible(hwnd):
                continue
            title = self.title(hwnd)
            if any(marker in title for marker in FATAL_TITLES):
                return hwnd
        return None

    def check_fatal(self, where):
        """The host's own crash box is the end of the story for this run: record
        who raised it, where, and with which wording, then stop driving the UI."""
        if self.host_fatal:
            return True
        dialog = self.find_fatal_dialog()
        if not dialog:
            return False
        self.host_fatal = True
        entry = self.dialog_report(dialog)
        entry["where"] = where
        entry["caption"] = self.title(dialog)
        self.report["hostFatal"] = entry
        self.step("host-fatal", "raised", f"at {where}: {self.title(dialog)!r}")
        self.capture(dialog, f"host-fatal-at-{where}", "host fatal error box")
        self.log(f"HOST FATAL at {where}: {json.dumps(entry, ensure_ascii=False)}")
        self.error(f"the host raised its fatal error box while running '{where}'")
        return True

    def wait_main_window(self):
        # Exact class plus this tool's own PID.  The host is started with
        # SW_HIDE on purpose, so the window is invisible and visibility must not
        # take part in recognising it, and it is never moved off-screen.
        deadline = time.time() + min(45.0, max(5.0, self.remaining() - 5))
        while time.time() < deadline:
            if self.remaining() < 1:
                break
            if self.check_fatal("waiting for the host main window"):
                break
            for hwnd in self.process_windows():
                if self.class_name(hwnd) == HOST_MAIN_CLASS:
                    self.main = hwnd
                    break
            if self.main:
                break
            time.sleep(0.25)
        if not self.main:
            self.note_dialogs("no main window")
            self.record_startup_blocker()
            self.error(f"the host main window (class {HOST_MAIN_CLASS!r}) did not appear")
            self.step("main-window", "missing")
            return False
        self.note_dialogs("main window found")
        visible = bool(user32.IsWindowVisible(self.main))
        self.report["backgroundMode"] = {
            "startupFlags": "STARTF_USESHOWWINDOW | SW_HIDE",
            "launcher": "CreateProcessW STARTUPINFO.wShowWindow=SW_HIDE "
                        "(src/window/window_init.cpp:437)",
            "mainWindowVisible": visible,
            "offScreenMove": False,
        }
        if visible:
            self.error("the host main window is visible although SW_HIDE was requested")
        self.step("main-window", "ok",
                  f"hwnd=0x{self.main:X} class={self.class_name(self.main)} "
                  f"visible={visible} title={self.title(self.main)!r} "
                  f"rect={self.rect(self.main)}")
        return True

    # -- messaging ----------------------------------------------------------
    def send(self, hwnd, message, wparam=0, lparam=0, timeout=4000):
        result = ctypes.c_size_t(0)
        ok = user32.SendMessageTimeoutW(hwnd, message, wparam, lparam, 0x0002, timeout,
                                        ctypes.byref(result))
        return None if not ok else result.value

    def post(self, hwnd, message, wparam=0, lparam=0):
        return user32.PostMessageW(hwnd, message, wparam, lparam)

    def command(self, hwnd, command_id):
        return self.send(hwnd, WM_COMMAND, command_id, 0)

    def info(self, hwnd):
        style = user32.GetWindowLongPtrW(hwnd, GWL_STYLE)
        exstyle = user32.GetWindowLongPtrW(hwnd, GWL_EXSTYLE)
        return {
            "hwnd": f"0x{hwnd:X}",
            "class": self.class_name(hwnd),
            "title": self.title(hwnd),
            "style": f"0x{style & 0xFFFFFFFFFFFFFFFF:X}",
            "exstyle": f"0x{exstyle & 0xFFFFFFFFFFFFFFFF:X}",
            "visible": bool(user32.IsWindowVisible(hwnd)),
            "enabled": bool(user32.IsWindowEnabled(hwnd)),
            "rect": self.rect(hwnd),
            "parent": f"0x{user32.GetParent(hwnd) or 0:X}",
            "owner": f"0x{user32.GetWindow(hwnd, GW_OWNER) or 0:X}",
        }

    def record_window(self, role, hwnd):
        data = self.info(hwnd)
        data["role"] = role
        self.report["windows"].append(data)
        self.log(f"WINDOW {role}: {json.dumps(data, ensure_ascii=False)}")
        return data

    # -- remote text --------------------------------------------------------
    def _alloc_remote(self, size):
        handle = kernel32.OpenProcess(REMOTE_PROCESS_RIGHTS, False, self.pid)
        if not handle:
            if not self.remote_alloc_failed:
                self.remote_alloc_failed = True
                self.error(f"OpenProcess({REMOTE_PROCESS_RIGHTS:#x}) failed: "
                           f"{ctypes.WinError(ctypes.get_last_error())}")
            return None, None
        address = kernel32.VirtualAllocEx(handle, None, size, MEM_COMMIT | MEM_RESERVE,
                                          PAGE_READWRITE)
        if not address:
            if not self.remote_alloc_failed:
                self.remote_alloc_failed = True
                self.error(f"VirtualAllocEx({size}) failed: "
                           f"{ctypes.WinError(ctypes.get_last_error())}")
            kernel32.CloseHandle(handle)
            return None, None
        return handle, address

    def remote_text(self, control, limit=512):
        """Read a control's text with WM_GETTEXT.  WM_GETTEXT sits below
        WM_USER, so the kernel marshals its buffer pointer between processes:
        the address must point at this process's buffer, and allocating memory
        inside the target would make WM_GETTEXT write into the wrong place."""
        if not control:
            return None
        buffer = ctypes.create_unicode_buffer(limit)
        length = self.send(control, WM_GETTEXT, limit, ctypes.addressof(buffer))
        if not length:
            return ""
        return buffer.value

    def listview_text(self, listview, row, column=0, limit=256):
        """Read one ListView cell through a remote LVITEMW + text buffer."""
        if not listview:
            return None
        item_size = ctypes.sizeof(LVITEMW)
        handle, base = self._alloc_remote(item_size + limit * 2)
        if not handle:
            return None
        try:
            text_address = base + item_size
            item = LVITEMW()
            item.mask = LVIF_TEXT
            item.iItem = row
            item.iSubItem = column
            item.pszText = text_address
            item.cchTextMax = limit
            written = ctypes.c_size_t(0)
            kernel32.WriteProcessMemory(handle, ctypes.c_void_p(base),
                                        ctypes.byref(item), item_size,
                                        ctypes.byref(written))
            self.send(listview, LVM_GETITEMTEXTW, row, base)
            buffer = ctypes.create_string_buffer(limit * 2)
            read = ctypes.c_size_t(0)
            kernel32.ReadProcessMemory(handle, ctypes.c_void_p(text_address), buffer,
                                       limit * 2, ctypes.byref(read))
            raw = buffer.raw
            # Cut at the first UTF-16 NUL: scanning whole code units avoids the
            # false positive that a 0x00 high byte of an ASCII character gives
            # when searching for a byte pair.
            units = []
            for index in range(0, limit * 2 - 1, 2):
                unit = raw[index:index + 2]
                if unit == b"\x00\x00":
                    break
                units.append(unit)
            return b"".join(units).decode("utf-16-le", "replace")
        except OSError as exc:
            self.error(f"listview_text(row={row}, column={column}) failed: {exc}")
            return None
        finally:
            kernel32.VirtualFreeEx(handle, base, 0, MEM_RELEASE)
            kernel32.CloseHandle(handle)

    def children(self, parent):
        return self.enum_windows(root=parent)

    def child_by_id(self, parent, control_id):
        for child in self.children(parent):
            if user32.GetWindowLongPtrW(child, GWL_ID) == control_id:
                return child
        return None

    def direct_children(self, parent):
        """Immediate children only.  EnumChildWindows returns the whole subtree,
        so using it for a recursive walk would report every node many times."""
        result = []
        child = user32.GetWindow(parent, GW_CHILD)
        while child:
            result.append(child)
            child = user32.GetWindow(child, GW_HWNDNEXT)
        return result

    def descendants(self, parent, depth=3):
        """Every control under `parent` down to `depth`, with its nesting level.
        A message box or file dialog nests its controls, so a flat child scan
        misses them."""
        found = []

        def walk(hwnd, level):
            if level > depth:
                return
            for child in self.direct_children(hwnd):
                found.append((level, child))
                walk(child, level + 1)

        walk(parent, 1)
        return found

    def dump_descendants(self, hwnd, label, depth=4):
        """Record the control tree of a window, so the report shows what the
        dialog really contains instead of what it was assumed to contain.
        Controls that carry neither an interesting class nor a control id are
        dropped, because a file dialog nests hundreds of shell internals."""
        keep = ("Edit", "Button", "ComboBox", "ComboBoxEx32", "ListBox",
                "SysListView32", "SysTreeView32", "msctls_progress32")
        everything = self.descendants(hwnd, depth)
        rows = []
        for level, child in everything:
            name = self.class_name(child)
            control_id = user32.GetWindowLongPtrW(child, GWL_ID)
            if name not in keep and control_id <= 0:
                continue
            rows.append({
                "level": level,
                "class": name,
                "id": control_id,
                "hwnd": f"0x{child:X}",
                "visible": bool(user32.IsWindowVisible(child)),
                "text": (self.remote_text(child, 200) or "")[:200],
            })
        self.report.setdefault("controlTrees", []).append(
            {"label": label, "descendantCount": len(everything), "rows": rows})
        self.log(f"CONTROLTREE {label}: {len(everything)} descendants, "
                 f"{len(rows)} recorded: {json.dumps(rows, ensure_ascii=False)}")
        return rows

    # -- drop-down probe ----------------------------------------------------
    def combobox_nodes(self, root):
        """Every combo box under `root`."""
        return [(level, child) for level, child in self.descendants(root, 4)
                if self.class_name(child) == "ComboBox"]

    def _sample_dropdown(self, box, observe, interval):
        """Expand the box and watch CB_GETDROPPEDSTATE across host frames.  The
        request is verified first: a window that is not visible can ignore
        CB_SHOWDROPDOWN, and then no timing may be claimed for it."""
        self.send(box, CB_SHOWDROPDOWN, 1, 0)
        opened = self.send(box, CB_GETDROPPEDSTATE, 0, 0)
        if not opened:
            return {"openWasRequested": False, "droppedRightAfterShow": 0,
                    "hostFramesForced": 0, "samples": [],
                    "closedAfterSeconds": None, "openForSeconds": 0.0,
                    "result": "CB_SHOWDROPDOWN had no effect"}
        start = time.time()
        samples = []
        frames = 0
        closed_at = None
        while time.time() - start < observe:
            # One host frame: the host is made to render, so each sample really
            # is "the state after N consecutive host frame refreshes".
            user32.InvalidateRect(self.main, None, False)
            user32.UpdateWindow(self.main)
            frames += 1
            state = self.send(box, CB_GETDROPPEDSTATE, 0, 0)
            samples.append({"t": round(time.time() - start, 3),
                            "hostFrame": frames, "dropped": state})
            if not state:
                closed_at = round(time.time() - start, 3)
                break
            time.sleep(interval)
        self.send(box, CB_SHOWDROPDOWN, 0, 0)
        return {
            "openWasRequested": True,
            "droppedRightAfterShow": opened,
            "hostFramesForced": frames,
            "samples": samples,
            "closedAfterSeconds": closed_at,
            "openForSeconds": closed_at if closed_at is not None
                              else round(time.time() - start, 3),
            "result": "closed" if closed_at is not None else "stayed open",
        }

    def _style_chain(self, hwnd):
        """The window and its real parents, innermost first.  Owners are not
        included, so walking up never reaches the host main window."""
        chain = []
        current = hwnd
        while current:
            chain.append(current)
            if user32.GetAncestor(current, GA_ROOT) == current:
                break
            current = user32.GetAncestor(current, GA_PARENT)
        return chain

    def _dropdown_with_style_bit(self, box, observe, interval):
        """Fallback for a combo that ignores CB_SHOWDROPDOWN while hidden: set
        the WS_VISIBLE style bit, which is what the control checks, without
        showing anything.  The risk is contained by parking the whole ancestor
        chain at -4000,-4000 first, recording every style, and restoring both
        style and position afterwards."""
        chain = self._style_chain(box)
        top = chain[-1]
        original = {hwnd: user32.GetWindowLongPtrW(hwnd, GWL_STYLE) for hwnd in chain}
        top_rect = self.rect(top)
        entry = {
            "how": "WS_VISIBLE style bit, no show",
            "chain": [f"0x{hwnd:X}" for hwnd in chain],
            "stylesBefore": [f"0x{original[hwnd] & 0xFFFFFFFF:X}" for hwnd in chain],
            "parkedTopLevelAt": [-4000, -4000],
        }
        user32.SetWindowPos(top, None, -4000, -4000, top_rect[2], top_rect[3],
                            SWP_NOZORDER | SWP_NOACTIVATE)
        time.sleep(0.2)
        for hwnd in chain:
            user32.SetWindowLongPtrW(hwnd, GWL_STYLE, original[hwnd] | WS_VISIBLE)
        time.sleep(0.2)
        entry["visibleWhileBitSet"] = bool(user32.IsWindowVisible(box))
        entry.update(self._sample_dropdown(box, observe, interval))
        self.send(box, CB_SHOWDROPDOWN, 0, 0)
        for hwnd in chain:
            user32.SetWindowLongPtrW(hwnd, GWL_STYLE, original[hwnd])
        user32.SetWindowPos(top, None, top_rect[0], top_rect[1], top_rect[2],
                            top_rect[3], SWP_NOZORDER | SWP_NOACTIVATE)
        time.sleep(0.2)
        entry["stylesRestored"] = all(
            user32.GetWindowLongPtrW(hwnd, GWL_STYLE) == original[hwnd]
            for hwnd in chain)
        entry["visibleAfterRestore"] = bool(user32.IsWindowVisible(box))
        self.log("DROPDOWN-FALLBACK " + json.dumps(
            {key: value for key, value in entry.items() if key != "samples"},
            ensure_ascii=False))
        return entry

    def dropdown_probe(self, root, role, observe=3.0, interval=0.05):
        """Reported defect: an expanded drop-down closes again with every host
        frame.  Expand the box, keep the host rendering frames, sample
        CB_GETDROPPEDSTATE across them and record how long it stayed open."""
        boxes = self.combobox_nodes(root)
        entry = {
            "role": role,
            "root": f"0x{root:X}",
            "observeSeconds": observe,
            "intervalSeconds": interval,
            "combos": [{"level": level, "hwnd": f"0x{child:X}",
                        "id": user32.GetWindowLongPtrW(child, GWL_ID),
                        "items": self.send(child, CB_GETCOUNT, 0, 0),
                        "currentSel": self.send(child, CB_GETCURSEL, 0, 0),
                        "dropped": self.send(child, CB_GETDROPPEDSTATE, 0, 0)}
                       for level, child in boxes],
        }
        self.report.setdefault("dropdownProbes", []).append(entry)
        if not boxes:
            entry["result"] = "no combo box"
            self.log(f"DROPDOWN {role}: no ComboBox under 0x{root:X}")
            return entry

        _level, box = boxes[0]
        entry["hwnd"] = f"0x{box:X}"
        first = self._sample_dropdown(box, observe, interval)
        entry["attempts"] = [dict(first, how="CB_SHOWDROPDOWN as the window is")]
        entry.update(first)
        if not first["openWasRequested"]:
            fallback = self._dropdown_with_style_bit(box, observe, interval)
            entry["attempts"].append(fallback)
            for key, value in fallback.items():
                if key not in ("how", "chain", "stylesBefore", "parkedTopLevelAt"):
                    entry[key] = value
        if not entry.get("openWasRequested"):
            # Nothing opened, so nothing about open/close timing is claimed.
            entry["result"] = ("the drop-down never opened, so no behaviour was "
                               "observed on this window")
            self.report["limitations"].append(
                f"dropdown probe on {role}: CB_SHOWDROPDOWN had no effect even "
                f"after setting WS_VISIBLE, so no open/close timing was measured")
        self.log(f"DROPDOWN {role}: hwnd=0x{box:X} result={entry['result']} "
                 f"openFor={entry.get('openForSeconds')}s "
                 f"frames={entry.get('hostFramesForced')}")
        return entry

    # -- parameter panel ----------------------------------------------------
    def activate_curve_entry(self, popup):
        """Open the parameter panel's curve page.  The entry is found by its
        caption and, failing that, by the tab control; what was used is recorded
        either way."""
        candidates = []
        for level, child in self.descendants(popup, 4):
            name = self.class_name(child)
            if name in ("Button", "SysTabControl32", "Static", "ToolbarWindow32"):
                candidates.append({
                    "level": level, "class": name,
                    "id": user32.GetWindowLongPtrW(child, GWL_ID),
                    "hwnd": f"0x{child:X}",
                    "text": (self.remote_text(child, 160) or "").strip(),
                })
        self.report.setdefault("paramEntries", []).append(
            {"popup": f"0x{popup:X}", "candidates": candidates})
        chosen = None
        for item in candidates:
            if any(key in item["text"] for key in ("曲线", "Curve", "curve", "カーブ")):
                chosen = item
                break
        if chosen is None:
            for item in candidates:
                if item["class"] == "SysTabControl32":
                    chosen = item
                    break
        if chosen is None:
            self.step("param-curve-entry", "missing",
                      "no curve entry and no tab control in the parameter panel")
            return None
        hwnd = int(chosen["hwnd"], 16)
        if chosen["class"] == "SysTabControl32":
            self.send(hwnd, TCM_SETCURSEL, 1, 0)
        else:
            self.send(hwnd, BM_CLICK, 0, 0)
        time.sleep(0.6)
        self.step("param-curve-entry", "activated",
                  f"{chosen['class']} id={chosen['id']} text={chosen['text']!r}")
        return chosen

    def verify_param_children(self, popup):
        """After the curve page is open, the custom canvas (1020) and the MMD
        panel (1031) have to exist as children of the parameter panel."""
        self.dump_descendants(popup, "parameter-window")
        found = {}
        for level, child in self.descendants(popup, 5):
            control_id = user32.GetWindowLongPtrW(child, GWL_ID)
            if control_id in (MMV_PARAM_CANVAS_ID, MMV_PARAM_MMD_PANEL_ID):
                found[control_id] = {
                    "hwnd": f"0x{child:X}", "class": self.class_name(child),
                    "level": level, "rect": self.rect(child),
                    "visible": bool(user32.IsWindowVisible(child)),
                    "style": f"0x{user32.GetWindowLongPtrW(child, GWL_STYLE) & 0xFFFFFFFF:X}",
                }
        expected = (MMV_PARAM_CANVAS_ID, MMV_PARAM_MMD_PANEL_ID)
        missing = [value for value in expected if value not in found]
        self.report.setdefault("paramChildren", []).append(
            {"popup": f"0x{popup:X}", "found": found, "missing": missing})
        self.step("param-children", "ok" if not missing else "missing",
                  f"canvas {MMV_PARAM_CANVAS_ID}: "
                  f"{'present' if MMV_PARAM_CANVAS_ID in found else 'absent'}, "
                  f"mmd-panel {MMV_PARAM_MMD_PANEL_ID}: "
                  f"{'present' if MMV_PARAM_MMD_PANEL_ID in found else 'absent'} "
                  f"({json.dumps(found, ensure_ascii=False)})")
        if missing:
            self.report["limitations"].append(
                f"the parameter panel has no child with control id {missing}")
        return found

    def hide_window(self, hwnd, role):
        """Keep the run out of the operator's way: nothing this tool causes to
        appear stays on screen."""
        was = bool(user32.IsWindowVisible(hwnd))
        if was:
            user32.ShowWindow(hwnd, SW_HIDE)
            time.sleep(0.4)
        now = bool(user32.IsWindowVisible(hwnd))
        self.report.setdefault("backgroundWindows", []).append(
            {"role": role, "hwnd": f"0x{hwnd:X}", "visibleBeforeHide": was,
             "visibleAfterHide": now})
        self.log(f"BACKGROUND {role}: visible {was} -> {now}")
        return now

    # -- capture ------------------------------------------------------------
    @staticmethod
    def usable_ink(ratio):
        """True when a frame carries real content rather than a blank fill."""
        return ratio is not None and INK_MIN <= ratio <= INK_MAX

    @staticmethod
    def ink_ratio(buffer, width, height):
        """Share of sampled pixels that are not white.  A blank frame is 0.0
        (nothing drawn on white) or 1.0 (an all-black frame)."""
        pixel_count = width * height
        step = max(1, pixel_count // 4000)
        ink = 0
        total = 0
        for index in range(0, pixel_count, step):
            base = index * 4
            b, g, r = buffer[base], buffer[base + 1], buffer[base + 2]
            if not (abs(b - 255) < 8 and abs(g - 255) < 8 and abs(r - 255) < 8):
                ink += 1
            total += 1
        return round(ink / total, 4) if total else 0.0

    def capture(self, hwnd, name, note=""):
        rect = self.rect(hwnd)
        width, height = rect[2], rect[3]
        if width <= 0 or height <= 0:
            self.error(f"capture {name}: window has an empty rect {rect}")
            return None
        screen_dc = user32.GetDC(None)

        def surface(w, h):
            """A top-down 32bpp DIB and the DC that draws into it, whitened."""
            dc = gdi32.CreateCompatibleDC(screen_dc)
            info = BITMAPINFO()
            info.bmiHeader.biSize = ctypes.sizeof(BITMAPINFOHEADER)
            info.bmiHeader.biWidth = w
            info.bmiHeader.biHeight = -h  # top-down
            info.bmiHeader.biPlanes = 1
            info.bmiHeader.biBitCount = 32
            info.bmiHeader.biCompression = BI_RGB
            bits = ctypes.c_void_p()
            bitmap = gdi32.CreateDIBSection(screen_dc, ctypes.byref(info),
                                            DIB_RGB_COLORS, ctypes.byref(bits), None, 0)
            if not dc or not bitmap or not bits:
                if bitmap:
                    gdi32.DeleteObject(bitmap)
                if dc:
                    gdi32.DeleteDC(dc)
                return None
            old = gdi32.SelectObject(dc, bitmap)
            gdi32.PatBlt(dc, 0, 0, w, h, 0x00FF0062)  # WHITENESS
            return {"dc": dc, "bitmap": bitmap, "old": old, "bits": bits, "w": w, "h": h}

        def close(handle):
            if not handle:
                return
            gdi32.SelectObject(handle["dc"], handle["old"])
            gdi32.DeleteObject(handle["bitmap"])
            gdi32.DeleteDC(handle["dc"])

        def read(handle):
            return ctypes.string_at(handle["bits"], handle["w"] * handle["h"] * 4)

        def paste(image, blob, x, y, w, h):
            """Overlay a child's rows on the parent image, clipped to it."""
            x0, y0 = max(0, x), max(0, y)
            x1, y1 = min(width, x + w), min(height, y + h)
            if x0 >= x1 or y0 >= y1:
                return
            run = (x1 - x0) * 4
            for row in range(y0, y1):
                src = ((row - y) * w + (x0 - x)) * 4
                dst = (row * width + x0) * 4
                image[dst:dst + run] = blob[src:src + run]

        parent = surface(width, height)
        if not parent:
            user32.ReleaseDC(None, screen_dc)
            self.error(f"capture {name}: CreateDIBSection failed")
            return None

        # MMV creates its editor window only while the owner window is hidden,
        # so the window exists but was never shown.  The hidden route is tried
        # first; the window is shown only if that returns an empty frame, and
        # then only for the length of the capture, parked at -4000,-4000 where
        # no monitor reaches.
        was_hidden = not user32.IsWindowVisible(hwnd)
        parked = None
        results = []

        def attempt(label, call):
            """Run one capture route and record what it produced."""
            gdi32.PatBlt(parent["dc"], 0, 0, width, height, 0x00FF0062)
            try:
                call()
            except Exception as exc:
                results.append({"method": label, "inkRatio": None,
                                "error": str(exc), "image": None})
                return None
            data = read(parent)
            ratio = self.ink_ratio(data, width, height)
            results.append({"method": label, "inkRatio": ratio, "image": data})
            return ratio

        # WM_PRINT is a system message, so the kernel marshals the HDC; a local
        # HDC is the correct argument.  PRF_CHECKVISIBLE is never set: it is
        # exactly the flag that makes an invisible window skip drawing.
        PRINT_CHILDREN = PRF_CLIENT | PRF_NONCLIENT | PRF_CHILDREN | PRF_ERASEBKGND | PRF_OWNED
        PRINT_SELF = PRF_CLIENT | PRF_NONCLIENT | PRF_ERASEBKGND

        def route_print_all(handle):
            """One WM_PRINT for the whole window."""
            self.send(hwnd, WM_PRINT, handle["dc"], PRINT_CHILDREN)
            return read(handle)

        def route_child_composite(handle):
            """Draw the frame, then draw every child control on its own and paste
            it in place.  A hidden window's own WM_PRINT frequently paints only
            its background, while each child class still draws its own content
            (button captions, list rows, static text) when asked directly."""
            self.send(hwnd, WM_PRINT, handle["dc"], PRINT_SELF)
            image = bytearray(read(handle))
            painted = 0
            for child in self.direct_children(hwnd):
                child_rect = self.rect(child)
                cw, ch = child_rect[2], child_rect[3]
                if cw <= 0 or ch <= 0:
                    continue
                sub = surface(cw, ch)
                if not sub:
                    continue
                self.send(child, WM_PRINT, sub["dc"], PRINT_CHILDREN)
                paste(image, read(sub), child_rect[0] - rect[0],
                      child_rect[1] - rect[1], cw, ch)
                close(sub)
                painted += 1
            composite_counts.append(painted)
            return bytes(image)

        def route_print_window(handle, flags=0):
            user32.PrintWindow(hwnd, handle["dc"], flags)
            return read(handle)

        composite_counts = []
        routes = [
            ("PrintWindow(PW_RENDERFULLCONTENT)",
             lambda handle: route_print_window(handle, PW_RENDERFULLCONTENT)),
            ("PrintWindow", lambda handle: route_print_window(handle, 0)),
            ("WM_PRINT(all flags)", route_print_all),
            ("WM_PRINT+per-child composite", route_child_composite),
        ]

        hidden_routes_ok = False
        if was_hidden:
            # Keep the owner-hidden path: the plugin never showed this window,
            # so PrintWindow is asked to print it exactly as it is.
            for label, flags in (("PrintWindow(PW_RENDERFULLCONTENT) while hidden",
                                  PW_RENDERFULLCONTENT),
                                 ("PrintWindow while hidden", 0)):
                ratio = attempt(label, lambda flags=flags:
                                user32.PrintWindow(hwnd, parent["dc"], flags))
                if self.usable_ink(ratio):
                    hidden_routes_ok = True
                    break
            if not hidden_routes_ok and not self.hidden_printwindow_ok:
                parked = rect
                user32.SetWindowPos(hwnd, None, -4000, -4000, width, height,
                                    SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW)
                time.sleep(0.35)

        for label, route in routes:
            attempt(label, lambda route=route: route(parent))

        if was_hidden:
            user32.ShowWindow(hwnd, SW_HIDE)
            restore = parked if parked else rect
            user32.SetWindowPos(hwnd, None, restore[0], restore[1], restore[2],
                                restore[3], SWP_NOZORDER | SWP_NOACTIVATE)
            time.sleep(0.2)
            # A window that has been painted at least once may start answering
            # the hidden route, which is what makes a fully hidden capture
            # possible; the result is recorded and reused by later captures.
            after = attempt("PrintWindow(PW_RENDERFULLCONTENT) after hiding again",
                            lambda: user32.PrintWindow(hwnd, parent["dc"],
                                                       PW_RENDERFULLCONTENT))
            if self.usable_ink(after):
                self.hidden_printwindow_ok = True

        close(parent)
        user32.ReleaseDC(None, screen_dc)

        self.report.setdefault("captureVisibility", []).append(
            {"name": name, "hiddenBefore": was_hidden,
             "shownOffScreenDuringCapture": bool(parked),
             "hiddenRouteWorked": hidden_routes_ok,
             "hiddenPrintWindowWorksNow": bool(self.hidden_printwindow_ok),
             "hiddenAfter": not user32.IsWindowVisible(hwnd)})

        attempts = []
        for item in results:
            entry = {"method": item["method"], "inkRatio": item["inkRatio"]}
            if item.get("error"):
                entry["error"] = item["error"]
            attempts.append(entry)
        if composite_counts:
            for entry in attempts:
                if entry["method"].startswith("WM_PRINT+per-child"):
                    entry["paintedChildren"] = composite_counts[0]
        good = [item for item in results if self.usable_ink(item["inkRatio"])]
        if good:
            best = good[0]
        else:
            drawn = [item for item in results if item["image"] is not None]
            best = max(drawn, key=lambda item: -abs((item["inkRatio"] or 0.0) - 0.5),
                       default=results[0])
        buffer = best["image"] or b"\x00" * (width * height * 4)
        ratio = best["inkRatio"]
        method = best["method"]
        blank = not self.usable_ink(ratio)

        path = self.shots / f"{name}.png"
        saved = False
        try:
            from PIL import Image
            image = Image.frombuffer("RGBA", (width, height), buffer, "raw", "BGRA", 0, 1)
            image.convert("RGB").save(path)
            saved = True
        except Exception as exc:  # PIL is optional; a raw dump still proves the pixels
            dump = self.shots / f"{name}.bmp"
            with open(dump, "wb") as handle:
                header = BITMAPINFOHEADER()
                header.biSize = ctypes.sizeof(BITMAPINFOHEADER)
                header.biWidth = width
                header.biHeight = height
                header.biPlanes = 1
                header.biBitCount = 32
                handle.write(b"BM" + b"\x00" * 12)
                handle.write(bytes(header))
                handle.write(buffer)
            self.log(f"PIL unavailable ({exc}); wrote {dump.name}")
        entry = {
            "name": name, "note": note, "hwnd": f"0x{hwnd:X}", "rect": rect,
            "method": method, "path": str(path if saved else (self.shots / f"{name}.bmp")),
            "inkRatio": ratio, "attempts": attempts, "blank": blank,
        }
        self.report["captures"].append(entry)
        self.log(f"CAPTURE {name}: {width}x{height} {method} ink={ratio} "
                 f"blank={blank} -> {entry['path']}")
        if blank:
            self.report["limitations"].append(
                f"capture {name}: every capture route returned an empty frame "
                f"({json.dumps(attempts)})")
        return entry

    # -- behaviour probes ---------------------------------------------------
    def nchittest(self, hwnd, role):
        rect = self.rect(hwnd)
        x, y, w, h = rect
        inset = 2
        points = {
            "center": (x + w // 2, y + h // 2),
            "title": (x + w // 2, y + max(4, 10)),
            "left": (x + inset, y + h // 2),
            "right": (x + w - inset - 1, y + h // 2),
            "top": (x + w // 2, y + inset),
            "bottom": (x + w // 2, y + h - inset - 1),
            "topleft": (x + inset, y + inset),
            "topright": (x + w - inset - 1, y + inset),
            "bottomleft": (x + inset, y + h - inset - 1),
            "bottomright": (x + w - inset - 1, y + h - inset - 1),
        }
        results = {}
        for label, (px, py) in points.items():
            code = self.send(hwnd, WM_NCHITTEST, 0, (py << 16) | (px & 0xFFFF))
            results[label] = {"point": [px, py],
                              "code": code,
                              "name": HTCODES.get(code, f"HT{code}")}
        self.report.setdefault("hitTests", []).append({"role": role, **self.info(hwnd),
                                                       "points": results})
        self.log(f"HITTEST {role}: " + ", ".join(
            f"{k}={v['name']}" for k, v in results.items()))
        return results

    def setwindowpos_hold(self, hwnd, role, dx=37, dy=23):
        before = self.rect(hwnd)
        target = [before[0] + dx, before[1] + dy, before[2], before[3]]
        user32.SetWindowPos(hwnd, None, target[0], target[1], target[2], target[3],
                            SWP_NOZORDER | SWP_NOACTIVATE)
        time.sleep(0.6)
        after = self.rect(hwnd)
        held = after[:2] == target[:2]
        entry = {"role": role, "requested": target, "before": before, "after": after,
                 "positionHeld": held}
        self.report.setdefault("moveTests", []).append(entry)
        self.step(f"setwindowpos:{role}", "ok" if held else "reverted",
                  f"requested={target[:2]} after={after[:2]}")
        return entry

    def sc_move_keyboard(self, hwnd, role, presses=4):
        before = self.rect(hwnd)
        self.post(hwnd, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, 0)
        time.sleep(0.5)
        for _ in range(presses):
            self.post(hwnd, WM_KEYDOWN, 0x27, 0)  # VK_RIGHT
            self.post(hwnd, WM_KEYUP, 0x27, 0)
            time.sleep(0.12)
        self.post(hwnd, WM_KEYDOWN, 0x0D, 0)  # VK_RETURN closes the move loop
        self.post(hwnd, WM_KEYUP, 0x0D, 0)
        time.sleep(0.8)
        after = self.rect(hwnd)
        moved = after[0] != before[0] or after[1] != before[1]
        entry = {"role": role, "before": before, "after": after, "moved": moved,
                 "method": "PostMessage WM_SYSCOMMAND SC_MOVE|HTCAPTION + VK_RIGHT x"
                           f"{presses} + VK_RETURN"}
        self.report.setdefault("keyboardMoves", []).append(entry)
        self.step(f"sc_move:{role}", "moved" if moved else "no-move",
                  f"before={before[:2]} after={after[:2]}")
        return entry

    # -- host actions -------------------------------------------------------
    def drop_files(self, hwnd, paths):
        """Send a real WM_DROPFILES with an HDROP built in our heap.  The host
        accepts drops on its main window and dispatches on the extension."""
        text = "\x00".join(str(p) for p in paths) + "\x00\x00"
        encoded = text.encode("utf-16-le")
        header = DROPFILES()
        header.pFiles = ctypes.sizeof(DROPFILES)
        header.fWide = True
        blob = bytes(header) + encoded
        handle = kernel32.GlobalAlloc(0x0042, len(blob))  # GMEM_MOVEABLE|GMEM_ZEROINIT
        if not handle:
            self.error("GlobalAlloc for the drop handle failed")
            return False
        pointer = kernel32.GlobalLock(handle)
        ctypes.memmove(pointer, blob, len(blob))
        kernel32.GlobalUnlock(handle)
        # HDROP is the HGLOBAL itself; the host calls DragQueryFileW on it.
        # Posted, not sent: loading a model takes seconds and the host must not
        # be forced to answer inside a send timeout.
        self.post(hwnd, WM_DROPFILES, handle, 0)
        return True

    def load_model(self, model):
        if not model:
            self.report["limitations"].append("no --model given: the model list stays empty")
            self.step("load-model", "skipped", "no --model")
            return False
        if not Path(model).is_file():
            self.error(f"model not found: {model}")
            return False
        self.drop_files(self.main, [model])
        # Loading is asynchronous from our point of view; poll for a modal box,
        # which the host shows both after a successful load (its model
        # information panel) and when a file cannot be read.
        deadline = time.time() + 20.0
        dialog = None
        while time.time() < deadline:
            dialog = self.find_process_dialog()
            if dialog:
                break
            time.sleep(0.5)
        if dialog:
            self.hide_window(dialog, "model-drop-dialog")
            info = self.dialog_report(dialog)
            info["where"] = "after the model drop"
            self.report.setdefault("hostDialogs", []).append(info)
            self.log(f"host dialog appeared after the drop: {self.title(dialog)!r}")
            self.capture(dialog, "model-drop-dialog", "host box shown after the model drop")
        # The scene as the host sees it: the model panel is the only place that
        # shows whether the drop was accepted at all.
        self.capture(self.main, "host-main-after-load", "host main window after the drop")
        self.step("load-model", "dropped", model)
        return True

    def find_process_dialog(self):
        """The first ordinary visible modal box.  A crash box is never returned:
        it is recorded as a host failure and left on screen as evidence."""
        if self.check_fatal("polling for a modal dialog"):
            return None
        for hwnd in self.process_windows():
            if self.class_name(hwnd) == "#32770" and user32.IsWindowVisible(hwnd):
                return hwnd
        return None

    def press_default_button(self, dialog):
        """Press the box's own OK button.  The host's model-information panel
        treats WM_CLOSE as "cancel" and aborts the load, so the real button has
        to be found and clicked, and its control id reported."""
        buttons = []
        for _level, child in self.descendants(dialog, 2):
            if self.class_name(child) != "Button":
                continue
            buttons.append({
                "hwnd": f"0x{child:X}",
                "id": user32.GetWindowLongPtrW(child, GWL_ID),
                "text": (self.remote_text(child, 128) or self.title(child)).strip(),
            })
        chosen = None
        for item in buttons:
            if item["id"] == IDOK:
                chosen = item
                break
        if chosen is None:
            for item in buttons:
                if item["text"].lower() in ("ok", "确定", "是", "はい"):
                    chosen = item
                    break
        self.log(f"DIALOG BUTTONS {json.dumps(buttons, ensure_ascii=False)}")
        if chosen is None:
            return buttons, None
        self.send(int(chosen["hwnd"], 16), BM_CLICK, 0, 0)
        return buttons, chosen

    def dismiss_dialogs(self):
        """Answer unexpected modal boxes the way the operator would, so the host
        is not stuck behind them.  Each box is hidden first, then its text and
        button ids are recorded, and only the OK button is pressed - WM_CLOSE is
        never used because it cancels a pending load."""
        closed = 0
        for _ in range(4):
            dialog = self.find_process_dialog()
            if not dialog:
                break
            caption = self.title(dialog)
            self.hide_window(dialog, f"dialog-{caption}")
            buttons, chosen = self.press_default_button(dialog)
            self.report.setdefault("dismissedDialogs", []).append(
                {"title": caption, "buttons": buttons, "pressed": chosen})
            if chosen is None:
                self.error(f"dialog {caption!r} has no OK button; left open")
                break
            self.log(f"dismissed host dialog {caption!r} with button "
                     f"id={chosen['id']} text={chosen['text']!r}")
            time.sleep(0.6)
            closed += 1
        return closed

    def load_accessory(self, accessory):
        if not accessory:
            return False
        if not Path(accessory).is_file():
            self.error(f"accessory not found: {accessory}")
            return False
        staged = (self.isolate_accessory(accessory) if self.args.isolate_accessory
                  else Path(accessory))
        self.drop_files(self.main, [staged])
        time.sleep(3.0)
        self.dismiss_dialogs()
        self.step("load-accessory", "dropped", f"{staged} (source: {accessory})")
        return True

    def open_mmv_editor(self, timeout=8.0):
        self.command(self.main, MMV_MENU_COMMAND)
        if self.check_fatal(f"WM_COMMAND {MMV_MENU_COMMAND} (open MMV)"):
            self.step("mmv-editor", "blocked", "the host raised its fatal error box")
            return None
        # The editor is owned by the hidden host window, so it is created
        # without ever becoming visible: it has to be found by class, not sight.
        editor = self.find_top(class_names={MMV_EDITOR_CLASS}, timeout=timeout,
                               visible_required=False)
        if not editor:
            self.step("mmv-editor", "missing",
                      f"WM_COMMAND {MMV_MENU_COMMAND} did not create {MMV_EDITOR_CLASS}")
            return None
        self.step("mmv-editor", "ok", f"hwnd=0x{editor:X} rect={self.rect(editor)}")
        # The editor is created with WS_VISIBLE, so hide it at once: the run
        # must not put a window on the operator's desktop.
        self.hide_window(editor, "mmv-editor")
        return editor

    def find_mmv_listview(self, editor):
        """The editor's list, by control id first and by class second: the
        plugin rebuilds its controls when an effect is assigned."""
        listview = self.child_by_id(editor, MMV_ID_LIST)
        if listview:
            return listview
        for _level, child in self.descendants(editor, 3):
            if self.class_name(child) == "SysListView32":
                return child
        return None

    def mmv_rows(self, editor, wait=0.0):
        listview = self.find_mmv_listview(editor)
        if not listview:
            return None, []
        # The editor fills its list from a worker thread, so the first read can
        # legitimately come back empty while the scene is still being scanned.
        deadline = time.time() + wait
        while True:
            count = self.send(listview, LVM_GETITEMCOUNT, 0, 0) or 0
            if count or time.time() >= deadline:
                break
            time.sleep(0.3)
        rows = []
        for row in range(min(count, 6)):
            rows.append({
                "row": row,
                "object": self.listview_text(listview, row, 0),
                "effect": self.listview_text(listview, row, 1),
                "status": self.listview_text(listview, row, 2),
            })
        self.step("mmv-rows", "ok" if count else "empty", f"count={count} {rows}")
        return listview, rows

    def select_row(self, listview, row):
        # LVM_SETITEMSTATE takes an LVITEM* that the *target* process reads, so
        # the struct has to live in the target's address space: a local pointer
        # would make the host dereference a foreign address.
        item = LVITEMW()
        item.mask = LVIF_STATE
        item.iItem = row
        item.state = LVIS_SELECTED | LVIS_FOCUSED
        item.stateMask = LVIS_SELECTED | LVIS_FOCUSED
        size = ctypes.sizeof(LVITEMW)
        handle, base = self._alloc_remote(size)
        if not handle:
            self.step("select-row", "failed", "no remote buffer for LVITEMW")
            return False
        try:
            kernel32.WriteProcessMemory(handle, ctypes.c_void_p(base),
                                        ctypes.byref(item), size, None)
            self.send(listview, LVM_SETITEMSTATE, row, base)
        finally:
            kernel32.VirtualFreeEx(handle, base, 0, MEM_RELEASE)
            kernel32.CloseHandle(handle)
        self.step("select-row", "ok", f"row={row}")
        return True

    def is_file_dialog(self, hwnd):
        """A real GetOpenFileName box, not the host's crash box or any other
        message box: it owns a filename edit or the shell combo box that holds
        it."""
        if self.class_name(hwnd) != "#32770" or not user32.IsWindow(hwnd):
            return False
        for _level, child in self.descendants(hwnd, 3):
            if self.class_name(child) == "Edit":
                return True
        for _level, child in self.descendants(hwnd, 2):
            if self.class_name(child) in ("ComboBoxEx32", "ComboBox", "ToolbarWindow32",
                                          "DirectUIHWND"):
                return True
        return False

    def find_filename_edit(self, dialog):
        """The file-name field is not always a direct child: the explorer-style
        box keeps it inside ComboBoxEx32 -> ComboBox -> Edit, so the whole tree
        is searched and every candidate is reported."""
        candidates = []
        for level, child in self.descendants(dialog, 4):
            if self.class_name(child) == "Edit":
                candidates.append((level, child,
                                   user32.GetWindowLongPtrW(child, GWL_ID)))
        self.log("FILE-DIALOG edits: " + json.dumps(
            [{"level": lvl, "id": cid, "hwnd": f"0x{h:X}"}
             for lvl, h, cid in candidates], ensure_ascii=False))
        for _level, child, control_id in candidates:
            if control_id in FILENAME_EDIT_IDS:
                return child, control_id
        if candidates:
            return candidates[0][1], candidates[0][2]
        return None, None

    def drive_file_dialog(self, path, timeout=6.0):
        """Answer the editor's own GetOpenFileName with a real path."""
        deadline = time.time() + timeout
        dialog = None
        while time.time() < deadline:
            if self.check_fatal("waiting for the file dialog"):
                self.step("file-dialog", "blocked", "host fatal error box instead of a dialog")
                return False
            for hwnd in self.process_windows():
                if self.is_file_dialog(hwnd):
                    dialog = hwnd
                    break
            if dialog:
                break
            time.sleep(0.2)
        if not dialog:
            self.step("file-dialog", "missing", f"no file dialog (#32770 holding an edit "
                                                f"or {FN_EDIT_ID:#x}) appeared")
            return False
        self.step("file-dialog", "ok", f"hwnd=0x{dialog:X} title={self.title(dialog)!r}")
        self.dump_descendants(dialog, "file-dialog")
        # The shell's chooser is the one window this tool cannot operate while
        # hidden: its DirectUI controls ignore synthetic input on an invisible
        # dialog, so it stays on screen for the second or two it is open.
        self.report["limitations"].append(
            "the shell's Open dialog is driven while visible: it is the only "
            "window in this flow that cannot be operated while hidden")
        edit, control_id = self.find_filename_edit(dialog)
        if not edit:
            self.step("file-dialog-name", "missing",
                      "the file dialog has no Edit control to write the path into")
            self.error("the file name could not be typed into the file dialog, so the "
                       "path was never selected")
            self.send(dialog, WM_COMMAND, IDCANCEL, 0)
            time.sleep(0.6)
            return False
        buffer = ctypes.create_unicode_buffer(str(path))
        # WM_SETTEXT is a system message too, so the kernel marshals the text
        # pointer: this must be a buffer in our own process.
        self.send(edit, WM_SETTEXT, 0, ctypes.addressof(buffer))
        echoed = self.remote_text(edit, 512)
        if echoed != str(path):
            self.step("file-dialog-name", "rejected",
                      f"the edit (id={control_id}) reads {echoed!r}")
            self.error("the file dialog did not accept the path text")
            self.send(dialog, WM_COMMAND, IDCANCEL, 0)
            time.sleep(0.6)
            return False
        self.step("file-dialog-name", "set", f"edit id={control_id}: {path}")
        # Press the dialog's own OK button instead of posting WM_COMMAND: the
        # shell's chooser is a DirectUI window, and only the button click runs
        # the same path a real confirmation does.
        buttons, chosen = self.press_default_button(dialog)
        if chosen is None:
            self.error("the file dialog has no OK button to press")
            return False
        self.log(f"confirmed the file dialog with button id={chosen['id']} "
                 f"text={chosen['text']!r}")
        deadline = time.time() + 6.0
        while time.time() < deadline:
            if not user32.IsWindow(dialog):
                break
            time.sleep(0.2)
        time.sleep(1.5)
        return True

    def assign_effect(self, editor, effect):
        if not effect:
            return False
        if not Path(effect).is_file():
            self.error(f"effect not found: {effect}")
            return False
        self.command(editor, MMV_ID_SET_EFFECT)
        time.sleep(0.8)
        if self.check_fatal(f"WM_COMMAND {MMV_ID_SET_EFFECT} (set effect)"):
            self.step("assign-effect", "blocked", "the host raised its fatal error box")
            return False
        if not self.drive_file_dialog(effect):
            self.error("the Set effect dialog could not be driven")
            return False
        time.sleep(3.0)
        # Only a changed effect column proves the assignment happened; the file
        # dialog closing by itself does not.
        listview = self.find_mmv_listview(editor)
        applied = self.listview_text(listview, 0, 1) if listview else None
        if not applied or applied == "无":
            self.step("assign-effect", "not-applied",
                      f"{effect}: the effect column still reads {applied!r}")
            self.error("the effect file was chosen but the effect column did not change")
            return False
        self.step("assign-effect", "ok", f"{effect} -> row 0 effect={applied!r}")
        return True

    def open_parameters(self, editor, timeout=8.0):
        self.command(editor, MMV_ID_PARAMETERS)
        classes = {MMV_PARAM_CLASS, MMV_PERSPECTIVE_CLASS}
        popup = self.find_top(class_names=classes, timeout=timeout,
                              visible_required=False)
        if not popup:
            if self.check_fatal(f"WM_COMMAND {MMV_ID_PARAMETERS} (parameters)"):
                self.step("parameter-window", "blocked",
                          "the host raised its fatal error box")
                return None
            self.step("parameter-window", "missing",
                      f"WM_COMMAND {MMV_ID_PARAMETERS} created neither "
                      f"{MMV_PARAM_CLASS} nor {MMV_PERSPECTIVE_CLASS}")
            return None
        self.step("parameter-window", "ok",
                  f"hwnd=0x{popup:X} class={self.class_name(popup)} rect={self.rect(popup)}")
        self.activate_curve_entry(popup)
        self.verify_param_children(popup)
        self.dropdown_probe(popup, "parameter-window")
        self.hide_window(popup, "parameter-window")
        return popup

    def status_texts(self, editor):
        data = {}
        for name, control_id in (("detail", MMV_ID_DETAIL), ("status", MMV_ID_STATUS),
                                 ("info", MMV_ID_INFO)):
            control = self.child_by_id(editor, control_id)
            data[name] = self.remote_text(control, 256) if control else None
        self.report["editorTexts"] = data
        self.log(f"EDITORTEXT {json.dumps(data, ensure_ascii=False)}")
        return data

    # -- matrix -------------------------------------------------------------
    def resize(self, hwnd, width, height):
        user32.SetWindowPos(hwnd, None, 0, 0, width, height,
                            SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE)
        time.sleep(0.6)

    def language_matrix(self, editor_holder):
        for language in (0, 1, 2):
            if not self.budget_ok(6):
                self.report["skipped"].append(f"language {language} (budget)")
                continue
            self.command(self.main, LANGUAGE_COMMAND)
            time.sleep(1.2)
            editor = editor_holder.get("editor")
            if editor and user32.IsWindow(editor):
                self.post(editor, WM_CLOSE)
                time.sleep(1.0)
            editor = self.open_mmv_editor()
            if not editor:
                continue
            editor_holder["editor"] = editor
            self.record_window(f"editor-lang{language}", editor)
            self.mmv_rows(editor)
            for label, width, height in (("800x640", 800, 640), ("narrow", 470, 640)):
                self.resize(editor, width, height)
                self.capture(editor, f"editor-lang{language}-{label}",
                             f"MMV allocation window, language {language}, {label}")
            popup = self.open_parameters(editor)
            if popup:
                self.record_window(f"parameters-lang{language}", popup)
                self.capture(popup, f"parameters-lang{language}",
                             f"MMV parameter window, language {language}")
                self.nchittest(popup, f"parameters-lang{language}")
                self.post(popup, WM_CLOSE)
                time.sleep(0.8)

    # -- teardown -----------------------------------------------------------
    def close_app(self):
        closed = False
        if self.main and user32.IsWindow(self.main):
            self.post(self.main, WM_CLOSE)
            deadline = time.time() + 8
            while time.time() < deadline:
                if kernel32.WaitForSingleObject(self.process.hProcess, 200) == 0:
                    closed = True
                    break
                time.sleep(0.2)
        if not closed and self.process:
            self.log("graceful close timed out; terminating only the process this tool started")
            kernel32.TerminateProcess(self.process.hProcess, 1)
            kernel32.WaitForSingleObject(self.process.hProcess, 5000)
        self.step("close", "ok" if closed else "terminated", f"pid={self.pid}")
        if self.process:
            kernel32.CloseHandle(self.process.hThread)
            kernel32.CloseHandle(self.process.hProcess)
        if self.desktop:
            user32.CloseDesktop(self.desktop)
            self.desktop = None

    def write_report(self):
        self.report["durationSeconds"] = round(time.time() - self.start, 2)
        self.report["finishedAt"] = time.strftime("%Y-%m-%d %H:%M:%S")
        self.report["ok"] = not self.report["errors"]
        self.report_path.write_text(json.dumps(self.report, ensure_ascii=False, indent=2),
                                    encoding="utf-8")
        self.log_path.write_text("\n".join(self.log_lines) + "\n", encoding="utf-8")
        print(f"\nreport: {self.report_path}")
        print(f"log:    {self.log_path}")
        for capture in self.report["captures"]:
            print(f"  shot: {capture['path']} ink={capture['inkRatio']}")
        if self.report["errors"]:
            print("errors:")
            for item in self.report["errors"]:
                print(f"  - {item['message']}")

    # -- orchestration ------------------------------------------------------
    def run(self):
        self.run_dir.mkdir(parents=True, exist_ok=True)
        self.shots.mkdir(parents=True, exist_ok=True)
        if not self.stage():
            self.write_report()
            return 1
        if self.args.phase == "stage":
            self.write_report()
            return 0
        if not self.create_desktop():
            self.write_report()
            return 1
        if not self.launch():
            self.close_app()
            self.write_report()
            return 1
        try:
            if not self.wait_main_window():
                return 1
            self.record_window("host-main", self.main)
            self.capture(self.main, "host-main", "host main window")
            self.load_model(self.args.model)
            self.load_accessory(self.args.accessory)
            self.dismiss_dialogs()
            if not self.budget_ok(10):
                self.report["skipped"].append("mmv flow (budget)")
                return 1
            editor = self.open_mmv_editor()
            if not editor:
                return 1
            self.record_window("mmv-editor", editor)
            self.capture(editor, "mmv-editor-initial", "MMV allocation window, first show")
            self.dropdown_probe(editor, "mmv-editor")
            listview, rows = self.mmv_rows(editor, wait=6.0)
            if listview and rows and not self.host_fatal:
                self.select_row(listview, 0)
                self.command(editor, MMV_ID_EXPAND_MATERIALS)
                time.sleep(0.8)
                self.check_fatal(f"WM_COMMAND {MMV_ID_EXPAND_MATERIALS} (expand materials)")
                if not self.host_fatal:
                    self.command(editor, MMV_ID_EXPAND_BONES)
                    time.sleep(0.8)
                    self.check_fatal(f"WM_COMMAND {MMV_ID_EXPAND_BONES} (expand bones)")
                self.mmv_rows(editor)
                if self.args.phase in ("all", "open") and not self.host_fatal:
                    if self.budget_ok(12):
                        self.assign_effect(editor, self.args.effect)
                        self.mmv_rows(editor)
                        self.status_texts(editor)
                    else:
                        self.report["skipped"].append("effect assignment (budget)")
            elif not self.host_fatal:
                self.report["limitations"].append(
                    "the MMV list had no rows, so no effect could be assigned")
            if self.host_fatal:
                self.report["skipped"].append(
                    "every step after the host crash box was skipped")
                return 2

            holder = {"editor": editor}
            if self.args.phase in ("all", "open"):
                popup = self.open_parameters(editor)
                if popup:
                    self.record_window("parameters", popup)
                    self.capture(popup, "parameters-initial", "MMV parameter window, first show")
                    self.nchittest(popup, "parameters")
            if self.args.phase in ("all", "drag") and not self.host_fatal and self.budget_ok(8):
                self.nchittest(editor, "mmv-editor")
                self.setwindowpos_hold(editor, "mmv-editor")
                if self.args.allow_sc_move:
                    self.sc_move_keyboard(editor, "mmv-editor")
                else:
                    self.report["limitations"].append("SC_MOVE keyboard move disabled (--no-sc-move)")
            if self.args.phase in ("all", "matrix") and not self.host_fatal and self.budget_ok(20):
                self.language_matrix(holder)
            elif self.args.phase in ("all", "matrix"):
                self.report["skipped"].append("language/size matrix (budget)")
            return 0 if not self.report["errors"] else 2
        finally:
            self.close_app()
            self.write_report()


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--exe", required=True,
                        help="MikuMikuDance.exe to run (staged into <output>/<run>/run/host)")
    parser.add_argument("--output", required=True,
                        help="artifact directory, e.g. .local/artifacts/mmv-curve/ui-probe")
    parser.add_argument("--model", default=None, help="PMX/PMD dropped on the host window")
    parser.add_argument("--accessory", default=None, help=".x accessory dropped on the host")
    parser.add_argument("--effect", default=r"D:\MMD-Next\MMV-Bullet3\effects\Bullet3_mmv.fx",
                        help=".fx assigned through the MMV editor's Set effect dialog")
    parser.add_argument("--phase", default="all",
                        choices=["all", "stage", "open", "matrix", "drag"])
    parser.add_argument("--desktop", default="private", choices=["private", "default"])
    parser.add_argument("--budget", type=float, default=55.0,
                        help="seconds this invocation may use (steps past it are skipped)")
    parser.add_argument("--run-id", default=None)
    parser.add_argument("--no-sc-move", dest="allow_sc_move", action="store_false",
                        help="skip the SC_MOVE keyboard drag probe")
    parser.add_argument("--without-mme", dest="without_mme", action="store_true",
                        help="stage the runtime without d3d9.dll/MMEffect.dll/MMHack.dll, "
                             "so the run is MMV-only for UI work and the host uses the "
                             "system DirectX 9; real MME rendering is a separate run")
    parser.add_argument("--no-isolate-accessory", dest="isolate_accessory",
                        action="store_false",
                        help="drop the accessory from its original folder instead of "
                             "copying it alone into <run>/assets (the copy keeps a "
                             "same-named .fx from being picked up as an MME effect)")
    args = parser.parse_args()
    probe = Probe(args)
    sys.exit(probe.run())


if __name__ == "__main__":
    main()
