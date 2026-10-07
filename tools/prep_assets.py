#!/usr/bin/env python3
"""Generate Source/assets: 1-bit tiles and sprites, Nontendo fonts (+ symbols
borrowed from other SDK fonts), UI icons, native button glyphs and UI sounds.
Needs the Playdate SDK (PLAYDATE_SDK_PATH) for its fonts, and Pillow."""
import os
import shutil
import sys
from PIL import Image

SDK = os.environ.get("PLAYDATE_SDK_PATH") or os.path.expanduser("~/Developer/PlaydateSDK")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # repo root
SRC = os.path.join(ROOT, "assets")
DST = os.path.join(ROOT, "Source", "assets")

shutil.rmtree(DST, ignore_errors=True)
os.makedirs(os.path.join(DST, "gfx"))
os.makedirs(os.path.join(DST, "fonts"))

# Tiles: use the hand-made 1-bit atlas under the name the game loads first.
Image.open(os.path.join(SRC, "gfx", "tiles_16_mono.png")).convert("1", dither=Image.NONE) \
    .save(os.path.join(DST, "gfx", "tiles_16.png"))

# Sprites: colour RGBA -> dithered 1-bit with alpha mask.
spr = Image.open(os.path.join(SRC, "gfx", "sprites_16.png")).convert("RGBA")
lum = spr.convert("L").convert("1")  # Floyd-Steinberg
alpha = spr.getchannel("A").point(lambda a: 255 if a >= 128 else 0)
out = Image.merge("RGBA", (lum.convert("L"),) * 3 + (alpha,))
out.save(os.path.join(DST, "gfx", "sprites_16.png"))

# Font page: white glyphs on black -> white glyphs with alpha.
page = Image.open(os.path.join(SRC, "fonts", "prince.png")).convert("L")
mask = page.point(lambda v: 255 if v >= 128 else 0)
white = Image.new("L", page.size, 255)
Image.merge("RGBA", (white, white, white, mask)).save(os.path.join(DST, "fonts", "prince.png"))
shutil.copy(os.path.join(SRC, "fonts", "prince.bmfnt"), os.path.join(DST, "fonts", "prince.bmfnt"))

shutil.copytree(os.path.join(SRC, "sounds"), os.path.join(DST, "sounds"))
print("assets prepared in", DST)

# ---------------------------------------------------------------------------
# UI assets: Nontendo fonts (+ symbols borrowed from other SDK fonts), icons, 8px tiles,
# UI click sounds.
import re
import struct
import wave
import math
import art

SDK_FONTS = os.path.join(SDK, "Resources", "Fonts", "Nontendo")


def ascii_art(rows, scale=1):
    w = max(len(r) for r in rows)
    im = Image.new("RGBA", (w * scale, len(rows) * scale), (0, 0, 0, 0))
    for y, r in enumerate(rows):
        for x, ch in enumerate(r):
            if ch in "#o":
                col = (0, 0, 0, 255) if ch == "#" else (255, 255, 255, 255)
                for dy in range(scale):
                    for dx in range(scale):
                        im.putpixel((x * scale + dx, y * scale + dy), col)
    return im


FONT_ROOT = os.path.join(SDK, "Resources", "Fonts")


def load_pd_font(rel):
    """Playdate .fnt (relative to the SDK Fonts dir) -> {char: (RGBA cell, advance)}, tracking."""
    path = os.path.join(FONT_ROOT, rel)
    d, base = os.path.dirname(path), os.path.basename(path)[:-4]
    table = [f for f in os.listdir(d) if f.startswith(base + "-table-") and f.endswith(".png")][0]
    cw, ch = map(int, re.findall(r"-table-(\d+)-(\d+)\.png", table)[0])
    img = Image.open(os.path.join(d, table)).convert("RGBA")
    cols = img.width // cw
    glyphs, tracking, i = {}, 0, 0
    for line in open(path, encoding="utf-8", errors="ignore"):
        line = line.rstrip("\n")
        if line.startswith("tracking="):
            tracking = int(line.split("=")[1])
            continue
        if "\t" not in line or line.startswith("--"):
            continue
        parts = line.split("\t")
        c = " " if parts[0] == "space" else parts[0]
        x, y = (i % cols) * cw, (i // cols) * ch
        glyphs[c] = (img.crop((x, y, x + cw, y + ch)), int(parts[-1]))
        i += 1
    return glyphs, tracking


def ink_rows(im):
    a, l = im.getchannel("A"), im.convert("L")
    return [y for y in range(im.height)
            if any(a.getpixel((x, y)) > 128 and l.getpixel((x, y)) < 128 for x in range(im.width))]


def baseline(glyphs):
    return ink_rows(glyphs["H"][0])[-1]


def borrow(sources, target):
    """Take symbols from other SDK fonts, aligned to the target font's baseline.
    sources: [(font path, chars)]. Returns {char: (RGBA cell, advance, yoffset)}."""
    tb = baseline(target)
    out = {}
    for rel, chars in sources:
        g, tracking = load_pd_font(rel)
        b = baseline(g)
        for c in chars:
            im, adv = g[c]
            out[c] = (im, adv + tracking, tb - b)
    return out


def convert_font(name, out_name, extra, scale=1):
    """Playdate .fnt + table -> white-on-alpha atlas + AngelCode XML.
    extra: {char: (RGBA cell, advance, yoffset)} at 1x, scaled by `scale`."""
    fnt = os.path.join(SDK_FONTS, name + ".fnt")
    table = [f for f in os.listdir(SDK_FONTS) if f.startswith(name + "-table-")][0]
    cw, ch = map(int, re.findall(r"-table-(\d+)-(\d+)\.png", table)[0])
    src = Image.open(os.path.join(SDK_FONTS, table)).convert("RGBA")
    glyphs, tracking = [], 0
    for line in open(fnt, encoding="utf-8"):
        line = line.rstrip("\n")
        if line.startswith("tracking="):
            tracking = int(line.split("=")[1])
            continue
        if "\t" not in line or line.startswith("--"):
            continue
        parts = line.split("\t")
        c = " " if parts[0] == "space" else parts[0]
        glyphs.append((c, int(parts[-1])))
    cols = src.width // cw

    cells = []  # (char, glyph RGBA image, advance, yoffset)
    for i, (c, w) in enumerate(glyphs):
        if len(c) != 1 or ord(c) > 255:
            continue
        x, y = (i % cols) * cw, (i // cols) * ch
        cells.append((c, src.crop((x, y, x + cw, y + ch)), w + tracking, 0))
    have = {c for c, *_ in cells}
    for c, (im, adv, yoff) in extra.items():
        if c in have:
            continue
        if scale != 1:
            im = im.resize((im.width * scale, im.height * scale), Image.NEAREST)
        cells.append((c, im, adv * scale, yoff * scale))

    cell_w = max(cw, max(g.width for _, g, _, _ in cells))
    cell_h = max(ch, max(g.height for _, g, _, _ in cells))
    per_row = 16
    rows_n = (len(cells) + per_row - 1) // per_row
    atlas = Image.new("RGBA", (per_row * cell_w, rows_n * cell_h), (0, 0, 0, 0))
    xml = ['<?xml version="1.0"?>', "<font>",
           f'  <common lineHeight="{ch}" base="{ch}" />',
           f'  <pages><page id="0" file="{out_name}.png" /></pages>']
    for i, (c, g, adv, yoff) in enumerate(cells):
        ax, ay = (i % per_row) * cell_w, (i // per_row) * cell_h
        a = g.getchannel("A")
        l = g.convert("L")
        for yy in range(g.height):
            for xx in range(g.width):
                if a.getpixel((xx, yy)) > 128 and l.getpixel((xx, yy)) < 128:
                    atlas.putpixel((ax + xx, ay + yy), (255, 255, 255, 255))
        xml.append(f'  <char id="{ord(c)}" x="{ax}" y="{ay}" width="{g.width}" height="{g.height}" '
                   f'xoffset="0" yoffset="{yoff}" xadvance="{adv}" />')
    xml.append("</font>")
    atlas.save(os.path.join(DST, "fonts", out_name + ".png"))
    with open(os.path.join(DST, "fonts", out_name + ".bmfnt"), "w") as f:
        f.write("\n".join(xml) + "\n")


# Nontendo lacks $ + < > # & *. Borrow them from SDK fonts with matching metrics:
#   bold  -> Pedallica (10px caps, 2px stems)
#   light -> Newsleak Serif (1px stems); its & is serifed, so & comes from Bitmore
NONTENDO_BOLD, _ = load_pd_font("Nontendo/Nontendo-Bold.fnt")
NONTENDO_LIGHT, _ = load_pd_font("Nontendo/Nontendo-Light.fnt")
BOLD_SYMBOLS = borrow([("Pedallica/font-pedallica.fnt", "$+<>#&*")], NONTENDO_BOLD)
LIGHT_SYMBOLS = borrow([("Newsleak Serif/Newsleak-Serif.fnt", "$+<>#*'"),
                        ("Bitmore/font-Bitmore.fnt", "&")], NONTENDO_LIGHT)

convert_font("Nontendo-Bold", "nontendo_bold", BOLD_SYMBOLS)
convert_font("Nontendo-Light", "nontendo_light", LIGHT_SYMBOLS)
convert_font("Nontendo-Bold-2x", "nontendo_bold_2x", BOLD_SYMBOLS, scale=2)

# Icons: normal row block, then an inverted copy (ink white, highlights black)
n = len(art.ICONS)
per_row = 16
block_rows = (n + per_row - 1) // per_row
icons = Image.new("RGBA", (per_row * 16, block_rows * 2 * 16), (0, 0, 0, 0))
for i, (name, rows) in enumerate(art.ICONS):
    rows = [(r + "." * 16)[:16] for r in rows] + ["." * 16] * max(0, 16 - len(rows))
    if any(len(r) != 16 for r in rows[:16]):
        raise SystemExit(f"icon {name} malformed")
    g = ascii_art(rows[:16])
    inv = Image.new("RGBA", g.size, (0, 0, 0, 0))
    for y in range(16):
        for x in range(16):
            r, gg, b, a = g.getpixel((x, y))
            if a:
                inv.putpixel((x, y), (255 - r, 255 - gg, 255 - b, 255))
    x, y = (i % per_row) * 16, (i // per_row) * 16
    icons.paste(g, (x, y))
    icons.paste(inv, (x, y + block_rows * 16))
icons.save(os.path.join(DST, "gfx", "icons.png"))
with open(os.path.join(ROOT, "src", "ui_icons.h"), "w") as f:
    f.write("/* Generated by prep_assets.py from art.py - do not edit */\n")
    f.write("#ifndef UI_ICONS_H\n#define UI_ICONS_H\n\nenum {\n")
    for name, _ in art.ICONS:
        f.write(f"    ICON_{name.upper()},\n")
    f.write(f"    ICON_COUNT\n}};\n\n#define ICON_INVERT_OFFSET {block_rows * per_row}\n\n#endif\n")

# 8px tiles for the far zoom: 2x2 box average + 4x4 ordered dither
BAYER = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]
full = Image.open(os.path.join(DST, "gfx", "tiles_16.png")).convert("L")
half = Image.new("L", (full.width // 2, full.height // 2), 0)
fp = full.load()
hp = half.load()
for y in range(half.height):
    for x in range(half.width):
        whites = sum(1 for dy in (0, 1) for dx in (0, 1) if fp[x * 2 + dx, y * 2 + dy] >= 128)
        level = whites * 4  # 0..16
        hp[x, y] = 255 if BAYER[y & 3][x & 3] < level else 0
half.convert("1", dither=Image.NONE).save(os.path.join(DST, "gfx", "tiles_8.png"))

# UI click sounds (short sine blips)
os.makedirs(os.path.join(DST, "sounds", "ui"), exist_ok=True)


def blip(path, freqs, dur=0.03, vol=0.35):
    rate = 22050
    frames = bytearray()
    n = int(rate * dur)
    for i in range(n):
        t = i / rate
        f = freqs[min(len(freqs) - 1, i * len(freqs) // n)]
        env = (1 - i / n) ** 2
        frames += struct.pack("<h", int(32767 * vol * env * math.sin(2 * math.pi * f * t)))
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(bytes(frames))


blip(os.path.join(DST, "sounds", "ui", "move.wav"), [1800], 0.012, 0.25)
blip(os.path.join(DST, "sounds", "ui", "select.wav"), [1200, 1800], 0.06)
blip(os.path.join(DST, "sounds", "ui", "back.wav"), [1400, 900], 0.06)
blip(os.path.join(DST, "sounds", "ui", "error.wav"), [220, 180], 0.15, 0.4)
blip(os.path.join(DST, "sounds", "ui", "zoom.wav"), [900], 0.02, 0.3)
print("ui assets prepared")

# ---------------------------------------------------------------------------
# Native Playdate button glyphs (from the SDK's Asheville Sans 14 Light), 20x20
ASH = os.path.join(SDK, "Resources", "Fonts", "Asheville", "Asheville Sans 14 Light")
NATIVE_GLYPHS = [("a", "\u24b6"), ("b", "\u24b7"), ("dpad", "\u271b"), ("up", "\u2b06\ufe0f"),
                 ("right", "\u27a1\ufe0f"), ("down", "\u2b07\ufe0f"), ("left", "\u2b05\ufe0f"),
                 ("crank", "\U0001f3a3"), ("menu", "\U0001f7e8")]
ash_tbl = [f for f in os.listdir(ASH) if "-table-" in f][0]
gcw, gch = map(int, re.findall(r"-table-(\d+)-(\d+)\.png", ash_tbl)[0])
ash_img = Image.open(os.path.join(ASH, ash_tbl)).convert("RGBA")
ash_cols = ash_img.width // gcw
ash = {}
idx = 0
for line in open(os.path.join(ASH, [f for f in os.listdir(ASH) if f.endswith(".fnt")][0]), encoding="utf-8"):
    if "\t" not in line or line.startswith("--"):
        continue
    parts = line.rstrip("\n").split("\t")
    x, y = (idx % ash_cols) * gcw, (idx // ash_cols) * gch
    ash[parts[0]] = (ash_img.crop((x, y, x + gcw, y + gch)), int(parts[-1]))
    idx += 1

gl_atlas = Image.new("RGBA", (len(NATIVE_GLYPHS) * gcw, gch * 2), (0, 0, 0, 0))
widths = []
for i, (name, ch) in enumerate(NATIVE_GLYPHS):
    g, adv = ash[ch]
    widths.append(adv)
    for yy in range(gch):
        for xx in range(gcw):
            r, gg, b, a = g.getpixel((xx, yy))
            if a < 128:
                continue
            ink = r < 128  # black pixel in the glyph
            gl_atlas.putpixel((i * gcw + xx, yy), (0, 0, 0, 255) if ink else (255, 255, 255, 255))
            gl_atlas.putpixel((i * gcw + xx, gch + yy), (255, 255, 255, 255) if ink else (0, 0, 0, 255))
gl_atlas.save(os.path.join(DST, "gfx", "glyphs.png"))
with open(os.path.join(ROOT, "src", "ui_glyphs.h"), "w") as f:
    f.write("/* Generated by prep_assets.py: native Playdate button glyphs - do not edit */\n")
    f.write("#ifndef UI_GLYPHS_H\n#define UI_GLYPHS_H\n\nenum {\n")
    for name, _ in NATIVE_GLYPHS:
        f.write(f"    GLYPH_{name.upper()},\n")
    f.write(f"    GLYPH_COUNT\n}};\n\n#define GLYPH_W {gcw}\n#define GLYPH_H {gch}\n")
    f.write("static const int k_glyph_advance[GLYPH_COUNT] = { " + ", ".join(map(str, widths)) + " };\n\n#endif\n")
print("native glyphs extracted")

