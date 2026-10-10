# Dual-process bone-record dumper v2.
#   python mdump2.py <exe> <pmm> <out.json> [--orig]
# Original x64: app pointer at module_base + 0x1445F8 (WinMain global).
# Port: scan the module image for the g_Block pointer (validated against
# the pinned model-slot layout).  Model slots at app+0xBE8 (255 ptrs),
# slotIdx at app+5088; model: boneTable +10056, boneCount +12560.
import ctypes, ctypes.wintypes as wt, json, struct, sys, time

k32 = ctypes.WinDLL("kernel32", use_last_error=True)
u32 = ctypes.WinDLL("user32", use_last_error=True)

PM_READ = 0x0410
MEM_COMMIT = 0x1000
TH32CS_SNAPMODULE = 0x0008

APP_MODEL_SLOTS = 0xBE8
APP_SLOT_COUNT = 5088
M_BONETABLE = 10056
M_BONECOUNT = 12560
M_NAME = 8896
M_IKCHAINS = 10064
M_IKCOUNT = 12564
STRIDE = 624

F_NAME = 0
F_PARENT = 48
F_MATINIT = 60
F_MATLOCAL = 124
F_MATWORLD = 188
F_MATXTRA = 252
F_POSITION = 316
F_ROTQUAT = 340
F_ROTQUAT2 = 356
F_PHYOFF = 372
F_PHYQUAT = 384
F_TYPE = 492
F_TAILIDX = 496
F_HASRB = 500
F_PHYSDIS = 501
F_LAYER = 504
F_FLAGS = 508
F_RATIO = 512
F_HASFLAG = 612
F_SLOTIDX = 616


class MEMORY_BASIC_INFORMATION(ctypes.Structure):
    _fields_ = [("BaseAddress", ctypes.c_ulonglong),
                ("AllocationBase", ctypes.c_ulonglong),
                ("AllocationProtect", ctypes.c_ulong),
                ("__al1", ctypes.c_ulong),
                ("RegionSize", ctypes.c_ulonglong),
                ("State", ctypes.c_ulong),
                ("Protect", ctypes.c_ulong),
                ("Type", ctypes.c_ulong),
                ("__al2", ctypes.c_ulong)]


class MODULEENTRY32W(ctypes.Structure):
    _fields_ = [("dwSize", ctypes.c_ulong),
                ("th32ModuleID", ctypes.c_ulong),
                ("th32ProcessID", ctypes.c_ulong),
                ("GlblcntUsage", ctypes.c_ulong),
                ("ProccntUsage", ctypes.c_ulong),
                ("modBaseAddr", ctypes.c_void_p),
                ("modBaseSize", ctypes.c_ulong),
                ("hModule", ctypes.c_void_p),
                ("szModule", ctypes.c_wchar * 256),
                ("szExePath", ctypes.c_wchar * 260)]


def rpm(h, addr, size):
    buf = ctypes.create_string_buffer(size)
    got = ctypes.c_size_t()
    if not k32.ReadProcessMemory(h, ctypes.c_void_p(addr), buf, size,
                                 ctypes.byref(got)):
        return None
    return buf.raw[:got.value]


def qword(h, addr):
    d = rpm(h, addr, 8)
    return struct.unpack("<Q", d)[0] if d and len(d) == 8 else 0


def dword(h, addr):
    d = rpm(h, addr, 4)
    return struct.unpack("<I", d)[0] if d and len(d) == 4 else 0


def main_module(h, pid):
    snap = k32.CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid)
    me = MODULEENTRY32W()
    me.dwSize = ctypes.sizeof(me)
    mods = []
    if k32.Module32FirstW(snap, ctypes.byref(me)):
        while True:
            mods.append((me.modBaseAddr or 0, me.modBaseSize))
            if not k32.Module32NextW(snap, ctypes.byref(me)):
                break
    k32.CloseHandle(snap)
    return mods[0] if mods else (0, 0)


def regions(h, lo, hi):
    mbi = MEMORY_BASIC_INFORMATION()
    addr = lo
    out = []
    while addr < hi:
        if k32.VirtualQueryEx(h, ctypes.c_void_p(addr), ctypes.byref(mbi),
                               ctypes.sizeof(mbi)) == 0:
            break
        base, size = mbi.BaseAddress, mbi.RegionSize
        if mbi.State == MEM_COMMIT and mbi.Protect in (0x02, 0x04, 0x20, 0x40):
            out.append((base, size))
        addr = base + size
    return out


def valid_model(h, m):
    if not (0x10000 < m < 0x7FFFFFFFFFFF):
        return False
    bc = dword(h, m + M_BONECOUNT)
    if not (1 <= bc <= 65535):
        return False
    bt = qword(h, m + M_BONETABLE)
    if not (0x10000 < bt < 0x7FFFFFFFFFFF):
        return False
    b0 = rpm(h, bt, STRIDE)
    if b0 is None or len(b0) < STRIDE:
        return False
    if not b0[:20].split(b"\x00")[0]:
        return False
    if struct.unpack_from("<f", b0, F_MATINIT + 60)[0] != 1.0:
        return False
    for e in (3, 7, 11):
        if struct.unpack_from("<f", b0, F_MATINIT + 4 * e)[0] != 0.0:
            return False
    return True


def find_app_orig(h, modbase):
    app = qword(h, modbase + 0x1445F8)
    return app if app else 0


def valid_app(h, v):
    cnt = rpm(h, v + APP_SLOT_COUNT, 1)
    if cnt is None or not (1 <= cnt[0] <= 254):
        return False
    for i in range(cnt[0]):
        slot = qword(h, v + APP_MODEL_SLOTS + 8 * i)
        if not slot or not valid_model(h, slot):
            return False
    return True


def find_app_port(h, modbase, modsize):
    for base, size in regions(h, modbase, modbase + modsize):
        for off in range(0, size, 0x10000):
            data = rpm(h, base + off, min(0x10000, size - off))
            if data is None:
                continue
            for o in range(0, len(data) - 8, 8):
                v = struct.unpack_from("<Q", data, o)[0]
                if v < 0x100000000 or v > 0x7FF000000000:
                    continue
                if valid_app(h, v):
                    return v
    return 0


def dump_models(h, app):
    cnt = rpm(h, app + APP_SLOT_COUNT, 1)[0]
    models = []
    for i in range(cnt):
        m = qword(h, app + APP_MODEL_SLOTS + 8 * i)
        if not m:
            continue
        if not valid_model(h, m):
            continue
        models.append(m)
    return models


def dump_bones(h, bt, bc):
    raw = rpm(h, bt, STRIDE * bc)
    if raw is None:
        return []
    out = []
    for i in range(bc):
        b = raw[i * STRIDE:(i + 1) * STRIDE]
        if len(b) < STRIDE:
            break
        out.append({
            "i": i,
            "name": b[:20].split(b"\x00")[0].decode("cp932", "replace"),
            "name_hex": b[:20].split(b"\x00")[0].hex(),
            "parent": struct.unpack_from("<i", b, F_PARENT)[0],
            "type": b[F_TYPE],
            "flags": struct.unpack_from("<H", b, F_FLAGS)[0],
            "layer": struct.unpack_from("<i", b, F_LAYER)[0],
            "tailIdx": struct.unpack_from("<i", b, F_TAILIDX)[0],
            "hasRB": b[F_HASRB],
            "physDis": b[F_PHYSDIS],
            "hasFlag": b[F_HASFLAG],
            "slotIdx": struct.unpack_from("<i", b, F_SLOTIDX)[0],
            "ratio": struct.unpack_from("<f", b, F_RATIO)[0],
            "pos": list(struct.unpack_from("<3f", b, F_POSITION)),
            "matInit": list(struct.unpack_from("<16f", b, F_MATINIT)),
            "matLocal": list(struct.unpack_from("<16f", b, F_MATLOCAL)),
            "matWorld": list(struct.unpack_from("<16f", b, F_MATWORLD)),
            "matExtra": list(struct.unpack_from("<16f", b, F_MATXTRA)),
            "rotQuat": list(struct.unpack_from("<4f", b, F_ROTQUAT)),
            "rotQuat2": list(struct.unpack_from("<4f", b, F_ROTQUAT2)),
            "phyOff": list(struct.unpack_from("<3f", b, F_PHYOFF)),
            "phyQuat": list(struct.unpack_from("<4f", b, F_PHYQUAT)),
        })
    return out


def dump_ik(h, m):
    ikt = qword(h, m + M_IKCHAINS)
    ikc = dword(h, m + M_IKCOUNT)
    if not (1 <= ikc <= 1000) or not (0x10000 < ikt < 0x7FFFFFFFFFFF):
        return []
    raw = rpm(h, ikt, ikc * 32)
    if raw is None:
        return []
    return [raw[i * 32:(i + 1) * 32].hex() for i in range(ikc)]


def all_dialogs(pid):
    out = []

    @ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
    def cb(h, lp):
        wpid = wt.DWORD()
        u32.GetWindowThreadProcessId(h, ctypes.byref(wpid))
        if wpid.value != pid:
            return True
        cn = ctypes.create_unicode_buffer(64)
        u32.GetClassNameW(h, cn, 64)
        if cn.value == "#32770":
            out.append(h)
        return True

    u32.EnumWindows(cb, 0)
    return out


def enum_children(dlg):
    kids = []

    @ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
    def cb(h, lp):
        cn = ctypes.create_unicode_buffer(64)
        u32.GetClassNameW(h, cn, 64)
        txt = ctypes.create_unicode_buffer(256)
        u32.GetWindowTextW(h, txt, 256)
        kids.append((h, cn.value, txt.value))
        return True

    u32.EnumChildWindows(dlg, cb, 0)
    return kids


def main_window(pid):
    out = []

    @ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
    def cb(h, lp):
        wpid = wt.DWORD()
        u32.GetWindowThreadProcessId(h, ctypes.byref(wpid))
        if wpid.value != pid or not u32.IsWindowVisible(h):
            return True
        cn = ctypes.create_unicode_buffer(64)
        u32.GetClassNameW(h, cn, 64)
        if cn.value == "#32770":
            return True
        txt = ctypes.create_unicode_buffer(128)
        u32.GetWindowTextW(h, txt, 128)
        if txt.value:
            out.append(h)
        return True

    u32.EnumWindows(cb, 0)
    return out[0] if out else 0


def main():
    exe, pmm, out = sys.argv[1], sys.argv[2], sys.argv[3]
    is_orig = "--orig" in sys.argv
    advance = 0
    for a in sys.argv[4:]:
        if a.startswith("--adv="):
            advance = int(a.split("=", 1)[1])
    import os
    from subprocess import Popen
    proc = Popen([exe], cwd=os.path.dirname(pmm))
    pid = proc.pid
    print("pid", pid)
    time.sleep(5)
    h = k32.OpenProcess(PM_READ, False, pid)
    main_h = 0
    for _ in range(30):
        main_h = main_window(pid)
        if main_h:
            break
        time.sleep(0.5)
    if not main_h:
        print("no main window")
        proc.kill()
        return 1
    dlg = 0
    u32.PostMessageW(main_h, 0x111, 205, 0)   # exactly once: a second
    # post while the first dialog is still opening leaves a stuck modal pair
    for attempt in range(40):
        time.sleep(1.0)
        for d in all_dialogs(pid):
            kids = enum_children(d)
            if any(cn == "Edit" for _, cn, _ in kids) and \
               any(cn == "ComboBox" for _, cn, _ in kids):
                dlg = d
                break
        if dlg:
            break
    if not dlg:
        print("no load dialog")
        proc.kill()
        return 1
    kids = enum_children(dlg)
    edit = next(x for x, cn, _ in kids if cn == "Edit")
    time.sleep(2.0)
    # WM_CHAR the path into the edit, then a real ENTER key sequence.
    # (WM_SETTEXT + BM_CLICK only works on the original; the port's
    # hooked common dialog needs the typed-keystroke path.)
    for ch in pmm:
        u32.PostMessageW(edit, 0x0102, ord(ch), 0)
    time.sleep(0.5)
    u32.PostMessageW(edit, 0x0100, 0x0D, 0x001C0001)
    u32.PostMessageW(edit, 0x0102, 0x0D, 0x001C0001)
    u32.PostMessageW(edit, 0x0101, 0x0D, 0xC01C0001)
    print("path typed; waiting for load")
    # wait until the title carries the pmm name and slot count is stable
    title_ok = False
    stable = 0
    last = None
    app_cache = 0
    t0 = time.time()
    while time.time() - t0 < 150:
        time.sleep(2.0)
        print("  [loop] tick", flush=True)
        buf = ctypes.create_unicode_buffer(512)
        # GetWindowTextW blocks cross-process on a busy UI thread; use a
        # timed WM_GETTEXT instead.
        res = ctypes.c_ulonglong()
        u32.SendMessageTimeoutW(main_h, 0x000D, 512, buf, 0x0002, 300,
                                ctypes.byref(res))
        if pmm.lower() in buf.value.lower():
            title_ok = True
        print("  [loop] title read", title_ok, flush=True)
        modbase, modsize = main_module(h, pid)
        print("  [loop] module", hex(modbase), flush=True)
        sig = None
        if modbase:
            app = app_cache or (find_app_orig(h, modbase) if is_orig else
                                find_app_port(h, modbase, modsize))
            if app:
                cnt = rpm(h, app + APP_SLOT_COUNT, 1)
                sig = (cnt[0] if cnt else 0)
                if sig and (last is None or sig == last):
                    app_cache = app
                    stable += 1
                    if stable >= 3:
                        if advance:
                            print("advancing", advance, "frames", flush=True)
                            for _ in range(advance):
                                u32.PostMessageW(main_h, 0x0100, 0x27,
                                                 0x014D0001)
                                u32.PostMessageW(main_h, 0x0102, 0x27,
                                                 0x014D0001)
                                u32.PostMessageW(main_h, 0x0101, 0x27,
                                                 0xC14D0001)
                                time.sleep(0.8)
                            time.sleep(3.0)
                        models = dump_models(h, app)
                        data = {"pid": pid, "exe": exe, "app": app,
                                "models": []}
                        for m in models:
                            bc = dword(h, m + M_BONECOUNT)
                            name = rpm(h, m + M_NAME, 20)
                            data["models"].append({
                                "addr": m,
                                "name": name.split(b"\x00")[0].decode(
                                    "cp932", "replace"),
                                "name_hex": name.split(b"\x00")[0].hex(),
                                "boneCount": bc,
                                "bones": dump_bones(h, qword(h, m + M_BONETABLE), bc),
                                "ik": dump_ik(h, m),
                            })
                            print("  model", data["models"][-1]["name"], bc)
                        json.dump(data, open(out, "w", encoding="utf-8"))
                        print("wrote", out, len(data["models"]), "models")
                        proc.kill()
                        return 0
                elif sig != last:
                    stable = 0
                last = sig
        print("  waiting... title_ok", title_ok, "slots", last,
              "stable", stable, flush=True)
    print("timeout")
    proc.kill()
    return 1


if __name__ == "__main__":
    sys.exit(main())
