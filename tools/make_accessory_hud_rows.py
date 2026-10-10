#!/usr/bin/env python3
"""Rebuild the accessory-state operation-button rows of the HUD sprite sheet.

The six operation buttons live in `res/assets/hud_sprites.png` as three cells of
40x30 pixels per row. A cell starts at x = 1 + 40 * column, the pitch the
renderer samples. The camera/accessory panel paints two of those rows:

  rotate row   camera v 0.412109..0.46875   accessory v 0.666016..0.722656
  move row     camera v 0.177734..0.234375  accessory v 0.724609..0.78125

In the camera state each button carries a light grey camera plate at its lower
left. In the accessory state the port replaces that plate with a ".x" mark: the
third coordinate state moves accessories, and MMD accessory files use the .x
extension.

This script copies the matching unbadged button rows, then paints a complete
mark in their lower-left margin. Starting with clean artwork avoids leaving
pieces of the camera plate or its shadow. The mark uses the plate's greys,
with its darker edge inside the strokes and no background or drop shadow.

Usage:
  python tools/make_accessory_hud_rows.py inspect
  python tools/make_accessory_hud_rows.py preview [--before PATH]
  python tools/make_accessory_hud_rows.py apply
  python tools/make_accessory_hud_rows.py embed

`apply` rewrites the sprite sheet, `embed` copies the sheet bytes into the PNG
102 record of res/gen_resources(.x64).res, which is what the application links.
Run `apply` before `embed`.
"""
import argparse
import os
import struct
import sys

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHEET = os.path.join(ROOT, "res", "assets", "hud_sprites.png")
RES_FILES = ("gen_resources.res", "gen_resources_x64.res")
PNG_RESOURCE_ID = 102
LANG_JP = 0x411
PREVIEW_DIR = os.path.join(ROOT, ".local", "artifacts", "accessory-mark")

CELL_PITCH = 40
CELL_WIDTH = 40
CELL_HEIGHT = 30
CELL_X0 = 1

# (unbadged row, accessory row) pairs: rotate first, then move.
ROW_PAIRS = ((151, 341), (31, 371))

FACE = (180, 180, 180, 255)   # the camera plate's face
EDGE = (119, 119, 119, 255)   # the camera plate's edge grey

# Complete ".x" footprint in the lower-left margin of every 40x30 cell.
# It stays clear of all six coloured axis letters and fits in the sampled row.
DOT = (0, 26, 2, 2)          # x, y, width, height
CROSS = (3, 21, 7, 7)        # x, y, width, height
CROSS_STROKE = 2              # diagonal thickness in pixels

ZOOM = 10
VIEWPORT = (86, 140, 62, 255)  # preview background: a typical viewport green


def cells():
    return range(3)


def cell_left(cell):
    return CELL_X0 + cell * CELL_PITCH


def mark_shape():
    """Return the mark's pixel sets in cell coordinates: (face, edge)."""
    dx, dy, dw, dh = DOT
    shapes = [{(x, y) for y in range(dy, dy + dh) for x in range(dx, dx + dw)}]
    cx, cy, cw, ch = CROSS
    cross = set()
    for y in range(ch):
        for x in range(cw):
            if abs(x - y) < CROSS_STROKE or abs((cw - 1 - x) - y) < CROSS_STROKE:
                cross.add((cx + x, cy + y))
    shapes.append(cross)

    face, edge = set(), set()
    for shape in shapes:
        # Shade within the glyph, rather than adding an opaque fringe outside
        # it. Every stroke is painted, independently of the underlying button.
        boundary = {(x, y) for x, y in shape
                    if (x + 1, y) not in shape or (x, y + 1) not in shape}
        face |= shape - boundary
        edge |= boundary
    return face, edge


def build(image):
    """Return a copy of the sheet with both accessory rows rebuilt."""
    result = image.copy()
    face, edge = mark_shape()
    for source_row, accessory_row in ROW_PAIRS:
        for cell in cells():
            left = cell_left(cell)
            result.paste(image.crop((left, source_row, left + CELL_WIDTH,
                                     source_row + CELL_HEIGHT)),
                         (left, accessory_row))
            for x, y in face:
                result.putpixel((left + x, accessory_row + y), FACE)
            for x, y in edge:
                result.putpixel((left + x, accessory_row + y), EDGE)
    return result


def magnify(image, rows, cells_to_show=None, zoom=ZOOM):
    """Magnify whole cells of the given rows over a mid-green background."""
    if cells_to_show is None:
        cells_to_show = list(cells())
    width = CELL_WIDTH * zoom * len(cells_to_show)
    height = CELL_HEIGHT * zoom * len(rows)
    canvas = Image.new("RGBA", (width, height), VIEWPORT)
    for row_index, row in enumerate(rows):
        for column, cell in enumerate(cells_to_show):
            tile = image.crop((cell_left(cell), row,
                               cell_left(cell) + CELL_WIDTH, row + CELL_HEIGHT))
            canvas.alpha_composite(
                tile.resize((CELL_WIDTH * zoom, CELL_HEIGHT * zoom), Image.NEAREST),
                (column * CELL_WIDTH * zoom, row_index * CELL_HEIGHT * zoom))
    return canvas


# ------------------------------------------------------- .res record patching
def res_field(data, offset):
    """Read one type/name field of a .res record header at `offset`."""
    ordinal, value = struct.unpack_from("<HH", data, offset)
    if ordinal == 0xFFFF:
        return value, offset + 4
    length = 0
    while struct.unpack_from("<H", data, offset + length)[0] != 0:
        length += 2
    text = data[offset:offset + length].decode("utf-16le")
    total = length + 2
    total += (-total) % 4
    return text, offset + total


def read_res_records(data):
    """Split a .res file into records: [(type, name, language, head, blob)].

    The head bytes are kept verbatim: rc.exe writes an all-zero sentinel record
    first, and rewriting every head from scratch would change it.
    """
    records = []
    offset = 0
    while offset < len(data):
        size, header_size = struct.unpack_from("<II", data, offset)
        kind, cursor = res_field(data, offset + 8)
        name, cursor = res_field(data, cursor)
        cursor += (-(cursor - offset - 8)) % 4
        # The 16-byte trailer is (DWORD 0, WORD 0x1030, WORD language, DWORD, DWORD).
        language = struct.unpack_from("<H", data, cursor + 6)[0]
        head = data[offset + 8:offset + header_size]
        records.append((kind, name, language, head,
                        data[offset + header_size:offset + header_size + size]))
        offset += header_size + size + (-size) % 4
    return records


def write_res_records(path, records):
    out = bytearray()
    for kind, name, language, head, blob in records:
        out += struct.pack("<II", len(blob), 8 + len(head))
        out += head + blob
        out += b"\x00" * ((-len(blob)) % 4)
    with open(path, "wb") as stream:
        stream.write(bytes(out))
    return len(out)


def embed_sheet():
    """Put the current sheet bytes into the PNG 102 record of both .res files."""
    with open(SHEET, "rb") as stream:
        sheet = stream.read()
    for name in RES_FILES:
        path = os.path.join(ROOT, "res", name)
        with open(path, "rb") as stream:
            data = stream.read()
        records = read_res_records(data)
        replaced = 0
        updated = []
        for kind, record_name, language, head, blob in records:
            if kind == "PNG" and record_name == PNG_RESOURCE_ID and language == LANG_JP:
                updated.append((kind, record_name, language, head, sheet))
                replaced += 1
            else:
                updated.append((kind, record_name, language, head, blob))
        if replaced != 1:
            raise SystemExit("%s: found %d PNG %d records" % (name, replaced, PNG_RESOURCE_ID))
        size = write_res_records(path, updated)
        print("%s: PNG %d = %d bytes, file %d bytes" % (name, PNG_RESOURCE_ID, len(sheet), size))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=["inspect", "preview", "apply", "embed"])
    parser.add_argument("--before", help="extra sheet to preview for comparison")
    parser.add_argument("--zoom", type=int, default=ZOOM)
    args = parser.parse_args()

    sheet = Image.open(SHEET).convert("RGBA")
    if sheet.size != (512, 512):
        raise SystemExit("unexpected sheet size %dx%d" % sheet.size)

    if args.mode == "inspect":
        for source_row, accessory_row in ROW_PAIRS:
            print("unbadged row %3d -> accessory row %3d"
                  % (source_row, accessory_row))
        face, edge = mark_shape()
        xs = [x for x, _ in face | edge]
        ys = [y for _, y in face | edge]
        print("mark: x %d..%d y %d..%d, %d face pixels, %d edge pixels"
              % (min(xs), max(xs), min(ys), max(ys), len(face), len(edge)))
        for _, row in ROW_PAIRS:
            for cell in cells():
                left = cell_left(cell)
                missing = sum(
                    sheet.getpixel((left + x, row + y)) != colour
                    for pixels, colour in ((face, FACE), (edge, EDGE))
                    for x, y in pixels)
                print("row %d cell %d: %d missing mark pixels"
                      % (row, cell, missing))
        return 0

    if args.mode == "preview":
        os.makedirs(PREVIEW_DIR, exist_ok=True)
        rows = (ROW_PAIRS[0][0], ROW_PAIRS[0][1], ROW_PAIRS[1][1], ROW_PAIRS[1][0])
        magnify(build(sheet), rows, zoom=args.zoom).save(
            os.path.join(PREVIEW_DIR, "after.png"))
        magnify(sheet, rows, zoom=args.zoom).save(
            os.path.join(PREVIEW_DIR, "before.png"))
        if args.before:
            magnify(Image.open(args.before).convert("RGBA"), rows, zoom=args.zoom).save(
                os.path.join(PREVIEW_DIR, "before-mark.png"))
        print("preview:", PREVIEW_DIR)
        return 0

    if args.mode == "embed":
        embed_sheet()
        return 0

    build(sheet).save(SHEET)
    print("rewrote", SHEET)
    return 0


if __name__ == "__main__":
    sys.exit(main())
