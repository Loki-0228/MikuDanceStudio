#!/usr/bin/env python3
"""Compile the .rc template sources with rc.exe and prove the produced
templates still match the originals byte-for-byte.

  res/templates/dialog/dialogs.rc   48 RT_DIALOG DIALOGEX (ids 600..817)
  res/templates/menu/menus.rc       2 RT_MENU (SAMPLE02 / SAMPLE02E)

These .rc files are the source of truth for the templates: gen_resources.py
compiles them at gen/verify time and embeds the produced blobs into
gen_resources(.x64)?.res in the original .rsrc record order.  This script's
verify re-compiles the .rc files independently and byte-compares every
template against the ones embedded in the committed .res - the byte anchor
to the original MikuMikuDance v9.32 .rsrc dump.

Statement forms in the .rc files are load-bearing: DIALOGEX plus the generic
CONTROL statement with exact numeric styles let rc.exe re-emit the original
template bytes verbatim (verified against 10.0.19041.0 and 10.0.26100.0).
The convenience statements (EDITTEXT, PUSHBUTTON, ...) OR in implicit
default styles and will break the round-trip.

Usage:
  python scripts/dialog_rc.py verify [--rc-exe PATH]
                                     # rc.exe: newest Windows Kits install,
                                     # RC_EXE env var, or --rc-exe override
"""
import argparse
import glob
import os
import re
import struct
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RES = os.path.join(ROOT, "res")
DIALOG_RC = os.path.join(RES, "templates", "dialog", "dialogs.rc")
MENU_RC = os.path.join(RES, "templates", "menu", "menus.rc")
ANCHOR_RES = os.path.join(RES, "gen_resources.res")  # x86; x64 shares templates

RT_MENU, RT_DIALOG = 4, 5


# ---------------------------------------------------------------------------
# rc.exe leg
# ---------------------------------------------------------------------------
def find_rcexe(override=None):
    """Newest SDK rc.exe; explicit override / RC_EXE env win."""
    for cand in (override, os.environ.get("RC_EXE")):
        if cand and os.path.isfile(cand):
            return cand
    hits = []
    for kits in (r"C:\Program Files (x86)\Windows Kits",
                 r"C:\Program Files\Windows Kits"):
        for p in glob.glob(kits + r"\10\bin\*\x64\rc.exe"):
            m = re.search(r"(\d+)\.(\d+)\.(\d+)\.(\d+)", p)
            hits.append(((tuple(map(int, m.groups())) if m else (0,)), p))
    return max(hits)[1] if hits else None


def compile_res(rc_path, rcexe):
    """Compile one .rc with rc.exe /r and return the .res bytes."""
    work = tempfile.mkdtemp(prefix="dialog_rc_")
    out = os.path.join(work, "out.res")
    r = subprocess.run([rcexe, "/r", "/nologo", "/fo", out, rc_path],
                       capture_output=True)
    if r.returncode != 0:
        sys.stderr.write(r.stdout.decode("mbcs", "replace"))
        sys.stderr.write(r.stderr.decode("mbcs", "replace"))
        raise RuntimeError(f"rc.exe failed on {rc_path} (exit {r.returncode})")
    return open(out, "rb").read()


def parse_res(data):
    """COFF .res (as emitted by rc.exe) -> {(type, name): [blobs]}.

    Type/name keys are ("ord", value) or ("str", value), mirroring the
    ordinal-or-UTF-16-string fields of the record header.
    """
    i = 32                                          # leading zero sentinel
    out = {}

    def ordname(j):
        w, = struct.unpack_from("<H", data, j)
        if w == 0xFFFF:
            return ("ord", struct.unpack_from("<H", data, j + 2)[0]), j + 4
        e = j
        while data[e:e + 2] != b"\x00\x00":
            e += 2
        return ("str", data[j:e].decode("utf-16-le")), e + 2

    while i + 8 <= len(data):
        if data[i:i + 8] == b"\x00" * 8:
            break
        dsize, hsize = struct.unpack_from("<II", data, i)
        typ, j = ordname(i + 8)
        name, j = ordname(j)
        out.setdefault((typ, name), []).append(data[i + hsize:i + hsize + dsize])
        i = (i + hsize + dsize + 3) & ~3
    return out


def compile_templates(rcexe):
    """Compile both .rc sources -> {(type_key, name_key): template blob}."""
    blobs = {}
    for rc_path, rt in ((DIALOG_RC, RT_DIALOG), (MENU_RC, RT_MENU)):
        for (typ, name), found in parse_res(compile_res(rc_path, rcexe)).items():
            if typ == ("ord", rt):
                blobs[(typ, name)] = found[0]
    return blobs


# ---------------------------------------------------------------------------
# verify: .rc compilation == templates embedded in the anchor .res
# ---------------------------------------------------------------------------
def _templates_from_res(recs):
    return {key: v[0] for key, v in recs.items()
            if key[0] in (("ord", RT_DIALOG), ("ord", RT_MENU))}


def cmd_verify(rc_exe):
    rcexe = find_rcexe(rc_exe)
    if not rcexe:
        print("verify: rc.exe not found (set RC_EXE or pass --rc-exe)")
        return 2
    print(f"verify: rc.exe = {rcexe}")
    anchor = _templates_from_res(parse_res(open(ANCHOR_RES, "rb").read()))
    produced = compile_templates(rcexe)
    ok = True
    for key in sorted(anchor, key=str):
        want = anchor[key]
        got = produced.get(key)
        rel = key[1]
        if got is None:
            print(f"  {rel}: MISSING from rc.exe output")
            ok = False
        elif got != want:
            diff = next((i for i, (a, b) in enumerate(zip(got, want))
                         if a != b), min(len(got), len(want)))
            print(f"  {rel}: BYTE MISMATCH (first diff at {diff}, "
                  f"{len(got)} vs {len(want)} bytes)")
            ok = False
    extra = [k for k in produced if k not in anchor]
    for key in extra:
        print(f"  {key[1]}: EXTRA template not present in anchor")
        ok = False
    if ok:
        n_dlg = sum(1 for k in anchor if k[0] == ("ord", RT_DIALOG))
        n_menu = sum(1 for k in anchor if k[0] == ("ord", RT_MENU))
        print(f"verify: {n_dlg} dialogs + {n_menu} menus byte-identical to "
              f"{os.path.relpath(ANCHOR_RES, ROOT)}")
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("cmd", choices=["verify"])
    ap.add_argument("--rc-exe", help="path to rc.exe (else newest Windows "
                                     "Kits install, or RC_EXE env)")
    args = ap.parse_args()
    return cmd_verify(args.rc_exe)


if __name__ == "__main__":
    sys.exit(main())
