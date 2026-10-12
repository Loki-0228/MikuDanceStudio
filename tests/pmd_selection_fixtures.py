# MAINTENANCE: regression-keep - original PMD fixtures and rendered-colour comparison.
"""Generate original PMD fixtures for pmd_selection_render_tests (no model assets)."""
from pathlib import Path
import struct
import sys
import re


def make_fixture(directory: Path, toon_index: int, toon_names: list[str], normal_z=-1):
    directory.mkdir(parents=True, exist_ok=True)
    pack = struct.pack
    text = lambda value, size: value.encode("ascii").ljust(size, b"\0")
    data = b"Pmd" + pack("<f", 1.0) + text("toon fixture", 20) + bytes(256)
    data += pack("<I", 4)
    for x, y, u, v in ((-6, 4, 0, 1), (-6, 16, 0, 0), (6, 4, 1, 1), (6, 16, 1, 0)):
        data += pack("<8fHHBB", x, y, 0, 0, 0, normal_z, u, v, 0, 0, 100, 0)
    data += pack("<I6HI", 6, 0, 1, 2, 2, 1, 3, 1)
    data += pack("<11fBBI", .8, .8, .8, 1, 1, 0, 0, 0, .4, .4, .4, toon_index, 0, 6)
    data += text("color.bmp", 20)
    data += pack("<H", 1) + text("root", 20) + pack("<HHBH3f", 65535, 0, 0, 0, 0, 0, 0)
    data += pack("<HHBBIB", 0, 0, 0, 0, 0, 0)  # IK, morph, display, English flag
    data += b"".join(text(name, 100) for name in toon_names)
    data += pack("<II", 0, 0)  # rigid bodies, joints
    (directory / "fixture.pmd").write_bytes(data)
    # All colour comes from the main texture; losing it leaves a grey/white quad.
    pixels = b"".join(bytes((30, 70, 220) if x < 8 else (220, 180, 30))
                      for y in range(16) for x in range(16))
    bitmap = pack("<2sIHHI", b"BM", 54 + len(pixels), 0, 0, 54)
    bitmap += pack("<IiiHHIIiiII", 40, 16, 16, 1, 24, 0, len(pixels), 0, 0, 0, 0)
    (directory / "color.bmp").write_bytes(bitmap + pixels)
    (directory / "shade.bmp").write_bytes(bitmap + bytes((160, 160, 160)) * 256)


def generate(root: Path):
    names = [f"toon{i:02}.bmp" for i in range(1, 11)]
    make_fixture(root / "ordered", 2, names)
    remapped = names.copy()
    remapped[0] = "toon03.bmp"
    make_fixture(root / "remapped", 0, remapped)
    missing = names.copy()
    missing[0] = "missing.bmp"
    make_fixture(root / "missing", 0, missing)
    make_fixture(root / "fallback-reference", 9, names)
    reverse = names.copy()
    reverse[9] = "toon01.bmp"
    make_fixture(root / "reverse", 9, reverse)
    make_fixture(root / "reverse-reference", 0, names)
    last = names.copy()
    last[0] = "toon10.bmp"
    make_fixture(root / "last", 0, last)
    make_fixture(root / "none", 255, names)
    custom = names.copy()
    custom[0] = "shade.bmp"
    make_fixture(root / "custom", 0, custom)
    make_fixture(root / "shadow-ordered", 2, names, normal_z=1)
    make_fixture(root / "shadow-remapped", 0, remapped, normal_z=1)


def compare_samples(reference: Path, actual: Path):
    pattern = r"^SAMPLES step=(\d+) ((?:[0-9a-f]{6} ?){4})$"
    def samples(path):
        values = re.findall(pattern, path.read_text(encoding="utf-8", errors="replace"), re.M)
        if [int(step) for step, _ in values] != list(range(8)):
            raise AssertionError(f"Missing render samples: {path}")
        return values
    if samples(reference) != samples(actual):
        raise AssertionError(f"Toon remapping changed rendered colours: {reference} vs {actual}")


if __name__ == "__main__":
    if len(sys.argv) == 4 and sys.argv[1] == "--compare":
        compare_samples(Path(sys.argv[2]), Path(sys.argv[3]))
        print("PASS identical rendered colours across all 8 UI states")
    else:
        generate(Path(sys.argv[1]).resolve())
