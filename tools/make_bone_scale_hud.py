#!/usr/bin/env python3
"""Rebuild the bone-scale button rows of the HUD sprite sheet.

The bone-scale panel needs four 40x30 buttons per state: one whole-scaling
button plus one per axis (X, Y, Z). They live in the blank left area of the
512x512 sheet `res/assets/hud_sprites.png`:

  normal row    y 401..430  cells at x = 1, 41, 81, 121  (whole, X, Y, Z)
  highlight row y 431..460  same layout, yellow arrows

Every button shows the move-row arrows turned 45 degrees: two double-headed
diagonal arrows whose four heads point at the corners of the cell. The axis
letter stays upright on top of the arrows, exactly as the move row layers its
letter over the arrow art; the whole-scaling button carries no letter.

The arrows are rebuilt from pixel geometry in the move row's style: white
fill (yellow in the highlight row), a one-pixel black stroke, and soft
anti-aliased edges from supersampled drawing. Both arrows are unioned into
one shape before the stroke is taken from its outer contour, so the crossed
centre of a button holds no inner black seam. The X/Y/Z letters are copied
pixel for pixel from the unbadged move row (y 31..60) of the same sheet, so
they keep their original colours, edges and upright orientation.

Each row is cleared and redrawn from scratch, so the script is idempotent and
running it twice writes identical bytes. Nothing outside x 1..160 of the two
rows is touched; the accessory ".x" mark stays as it is.

Usage:
  python tools/make_bone_scale_hud.py inspect
  python tools/make_bone_scale_hud.py preview [--zoom N]
  python tools/make_bone_scale_hud.py apply
  python tools/make_bone_scale_hud.py embed

`apply` rewrites the sprite sheet, `embed` copies the sheet bytes into the
PNG 102 record of res/gen_resources(.x64).res, which is what the application
links. Run `apply` before `embed`.
"""
import argparse
import math
import os
import sys

from PIL import Image, ImageDraw

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
if TOOLS_DIR not in sys.path:
    sys.path.insert(0, TOOLS_DIR)

# Reuse the .res record helpers and the PNG 102 embedding of the accessory
# row generator: both scripts patch the same two resource files.
from make_accessory_hud_rows import (  # noqa: E402
    PNG_RESOURCE_ID,
    RES_FILES,
    embed_sheet,
    read_res_records,
)

ROOT = os.path.dirname(TOOLS_DIR)
SHEET = os.path.join(ROOT, "res", "assets", "hud_sprites.png")
PREVIEW_DIR = os.path.join(ROOT, ".local", "artifacts", "bone-scale-icons")

CELL_WIDTH = 40
CELL_HEIGHT = 30
CELL_X = (1, 41, 81, 121)
CELL_LABELS = ("whole", "X", "Y", "Z")

ROW_NORMAL = 401
ROW_HIGHLIGHT = 431
ROWS = (ROW_NORMAL, ROW_HIGHLIGHT)

# The unbadged move row holds the upright axis letters over white arrows, so
# its letters can be lifted without picking up arrow pixels.
LETTER_SOURCE_ROW = 31
LETTER_SOURCE_CELLS = (0, 1, 2)        # X, Y, Z
LETTER_HUES = ("R", "G", "B")

ARROW_FILL = (255, 255, 255, 255)          # normal state arrows
ARROW_FILL_HIGHLIGHT = (255, 255, 0, 255)  # highlight state arrows
STROKE = (0, 0, 0, 255)
CLEAR = (255, 255, 255, 0)                 # the sheet's transparent colour

# Arrow geometry in cell pixels. The move row's proportions are kept (4 px
# white shaft, 6 px head, 1 px black stroke); the head is sharpened to a
# 50-degree wedge because a blunt head rotated 45 degrees reads as a square
# corner instead of an arrow.
TIP = 19.0          # cell-centre distance of each head tip along the arrow
HEAD = 9.5          # tip to the flare's widest point
SLOPE = 2.5         # flare-to-shaft slope length
SHAFT_HALF = 2.0
HEAD_HALF = 4.5
STROKE_WIDTH = 1.0
SUPERSAMPLE = 8

VIEWPORT = (86, 140, 60, 255)   # preview background: a typical viewport green
DARK_BG = (52, 56, 64, 255)     # preview background: a dark HUD panel
ZOOM = 8


def cell_centre():
    return (CELL_WIDTH / 2.0 - 0.5, CELL_HEIGHT / 2.0 - 0.5)


def hue_hit(pixel, hue):
    r, g, b, a = pixel
    if a == 0:
        return False
    if hue == "R":
        return r > g and r > b
    if hue == "G":
        return g > r and g > b
    return b > r and b > g


def stroke_hit(pixel):
    r, g, b, a = pixel
    return a >= 120 and max(r, g, b) <= 110


def letter_pixels(sheet, cell):
    """Return {cell position (x, y): RGBA} for one axis letter.

    The letter is taken from the unbadged move row: its hue pixels first
    (fill and the fill/stroke blends), then the dark stroke pixels that hug
    them. Arrow pixels stay out because white arrow fill is not hue matched
    and arrow strokes never sit in the letter's one-pixel shadow.
    """
    px = sheet.load()
    left = CELL_X[cell]
    hue = LETTER_HUES[cell]
    seeds, ring = set(), set()
    for y in range(LETTER_SOURCE_ROW, LETTER_SOURCE_ROW + CELL_HEIGHT):
        for x in range(left, left + CELL_WIDTH):
            pixel = px[x, y]
            if hue_hit(pixel, hue):
                seeds.add((x, y))
            elif stroke_hit(pixel):
                ring.add((x, y))
    keep = set(seeds)
    for x, y in ring:
        if any((x + dx, y + dy) in seeds
               for dx in (-1, 0, 1) for dy in (-1, 0, 1)):
            keep.add((x, y))
    seed_x = [x - left for x, _ in seeds]
    seed_y = [y - LETTER_SOURCE_ROW for _, y in seeds]
    cx, cy = cell_centre()
    dx = round(cx - (min(seed_x) + max(seed_x)) / 2.0)
    dy = round(cy - (min(seed_y) + max(seed_y)) / 2.0)
    return {(x - left + dx, y - LETTER_SOURCE_ROW + dy): px[x, y]
            for x, y in keep}


def letter_stats(sheet, cell):
    pixels = letter_pixels(sheet, cell)
    xs = [x for x, _ in pixels]
    ys = [y for _, y in pixels]
    return len(pixels), (min(xs), min(ys), max(xs), max(ys))


def arrow_outline():
    """White-fill outline of one double-headed arrow in (u, v) space."""
    waist = TIP - HEAD - SLOPE
    return [
        (TIP, 0.0),
        (TIP - HEAD, HEAD_HALF),
        (waist, SHAFT_HALF),
        (-waist, SHAFT_HALF),
        (-(TIP - HEAD), HEAD_HALF),
        (-TIP, 0.0),
        (-(TIP - HEAD), -HEAD_HALF),
        (-waist, -SHAFT_HALF),
        (waist, -SHAFT_HALF),
        (TIP - HEAD, -HEAD_HALF),
    ]


def rotated_outline(angle):
    rad = math.radians(angle)
    cos, sin = math.cos(rad), math.sin(rad)
    cx, cy = cell_centre()
    return [(u * cos - v * sin + cx, u * sin + v * cos + cy)
            for u, v in arrow_outline()]


def render_arrows(fill):
    """Merge both diagonal arrows first, then stroke the merged outline.

    The two fill polygons are unioned into one body layer, and the stroke
    layer is that same union dilated by STROKE_WIDTH. Painting the body over
    the stroke leaves the black band only around the outer contour of the
    whole crossed-arrows shape, so the crossing centre carries no inner
    black seam the way two separately stroked arrows would.
    """
    ss = SUPERSAMPLE
    size = (CELL_WIDTH * ss, CELL_HEIGHT * ss)
    stroke_layer = Image.new("RGBA", size, (0, 0, 0, 0))
    body_layer = Image.new("RGBA", size, (0, 0, 0, 0))
    stroke_draw = ImageDraw.Draw(stroke_layer)
    body_draw = ImageDraw.Draw(body_layer)
    for angle in (45, 135):
        points = [(x * ss, y * ss) for x, y in rotated_outline(angle)]
        for step in range(24):
            a = 2.0 * math.pi * step / 24.0
            dx = math.cos(a) * STROKE_WIDTH * ss
            dy = math.sin(a) * STROKE_WIDTH * ss
            stroke_draw.polygon([(x + dx, y + dy) for x, y in points], fill=STROKE)
        body_draw.polygon(points, fill=fill)
    merged = Image.alpha_composite(stroke_layer, body_layer)
    return merged.resize((CELL_WIDTH, CELL_HEIGHT), Image.BOX)


def build(sheet):
    """Return a copy of the sheet with both bone-scale rows rebuilt."""
    result = sheet.copy()
    px = result.load()
    clear = Image.new("RGBA", (CELL_WIDTH * len(CELL_X), CELL_HEIGHT), CLEAR)
    for row in ROWS:
        result.paste(clear, (CELL_X[0], row))

    letters = [letter_pixels(sheet, cell) for cell in LETTER_SOURCE_CELLS]
    for row in ROWS:
        fill = ARROW_FILL if row == ROW_NORMAL else ARROW_FILL_HIGHLIGHT
        art = render_arrows(fill)
        for cell in range(len(CELL_X)):
            left = CELL_X[cell]
            for y in range(CELL_HEIGHT):
                for x in range(CELL_WIDTH):
                    pixel = art.getpixel((x, y))
                    px[left + x, row + y] = pixel if pixel[3] else CLEAR
            if cell == 0:
                continue
            for (x, y), pixel in letters[cell - 1].items():
                px[left + x, row + y] = pixel
    return result


def magnify(image, rows, zoom=ZOOM, background=VIEWPORT):
    """Magnify four whole cells of each row over `background`."""
    width = CELL_WIDTH * zoom * len(CELL_X)
    height = CELL_HEIGHT * zoom * len(rows)
    canvas = Image.new("RGBA", (width, height), background)
    for index, row in enumerate(rows):
        for column, left in enumerate(CELL_X):
            tile = image.crop((left, row, left + CELL_WIDTH, row + CELL_HEIGHT))
            canvas.alpha_composite(
                tile.resize((CELL_WIDTH * zoom, CELL_HEIGHT * zoom), Image.NEAREST),
                (column * CELL_WIDTH * zoom, index * CELL_HEIGHT * zoom))
    return canvas


def embed_state(sheet):
    """Report whether each .res PNG 102 record already holds `sheet`."""
    with open(sheet, "rb") as stream:
        want = stream.read()
    out = []
    for name in RES_FILES:
        path = os.path.join(ROOT, "res", name)
        with open(path, "rb") as stream:
            data = stream.read()
        blob = None
        for kind, record_name, language, head, payload in read_res_records(data):
            if kind == "PNG" and record_name == PNG_RESOURCE_ID:
                blob = (language, payload)
        if blob is None:
            out.append("%s: no PNG %d record" % (name, PNG_RESOURCE_ID))
        else:
            out.append("%s: PNG %d lang 0x%03X holds %d bytes, %s sheet"
                       % (name, PNG_RESOURCE_ID, blob[0], len(blob[1]),
                          "matches" if blob[1] == want else "DIFFERS from"))
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=["inspect", "preview", "apply", "embed"])
    parser.add_argument("--zoom", type=int, default=ZOOM)
    args = parser.parse_args()

    sheet = Image.open(SHEET).convert("RGBA")
    if sheet.size != (512, 512):
        raise SystemExit("unexpected sheet size %dx%d" % sheet.size)

    if args.mode == "inspect":
        for row in ROWS:
            state = "normal" if row == ROW_NORMAL else "highlight"
            print("row %d (%s): cells %s at x %s"
                  % (row, state, ", ".join(CELL_LABELS), CELL_X))
        for cell in LETTER_SOURCE_CELLS:
            count, box = letter_stats(sheet, cell)
            print("letter %s from move row cell %d: %d pixels, box x%d..%d y%d..%d"
                  % (CELL_LABELS[cell + 1], cell, count, box[0], box[2],
                     box[1], box[3]))
        for row in ROWS:
            px = sheet.load()
            for cell in range(len(CELL_X)):
                left = CELL_X[cell]
                drawn = 0
                letters = 0
                clear = 0
                for y in range(row, row + CELL_HEIGHT):
                    for x in range(left, left + CELL_WIDTH):
                        pixel = px[x, y]
                        if pixel[3] == 0:
                            clear += 1
                        else:
                            drawn += 1
                            if any(hue_hit(pixel, hue) for hue in "RGB"):
                                letters += 1
                print("row %d cell %d (%s): %d drawn px, %d hue px, %d clear px"
                      % (row, cell, CELL_LABELS[cell], drawn, letters, clear))
        for line in embed_state(SHEET):
            print(line)
        return 0

    if args.mode == "preview":
        os.makedirs(PREVIEW_DIR, exist_ok=True)
        built = build(sheet)
        magnify(built, ROWS, zoom=args.zoom).save(
            os.path.join(PREVIEW_DIR, "preview.png"))
        magnify(built, ROWS, zoom=args.zoom, background=DARK_BG).save(
            os.path.join(PREVIEW_DIR, "preview-dark.png"))
        magnify(built, (LETTER_SOURCE_ROW, ROW_NORMAL),
                zoom=args.zoom).save(os.path.join(PREVIEW_DIR, "preview-move.png"))
        magnify(built, (LETTER_SOURCE_ROW - 30, ROW_HIGHLIGHT),
                zoom=args.zoom).save(
                    os.path.join(PREVIEW_DIR, "preview-move-highlight.png"))
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
