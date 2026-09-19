#!/usr/bin/env python3
"""
extract_tiles.py - Extract Micropolis / VT City 16x16 1-bit tiles and sprites
into PNG tile atlases, individual PNG tiles, and JSON metadata.

Extracts:
1. All 969 tiles (0..959 map tiles, 960..965 sprites, 966..968 toolbar icons)
   from vt_tiles.h and INDEX.txt.
2. assets/gfx/tiles_16.png (512x512 tile atlas: 32 columns x 31 rows of 16x16 tiles).
3. assets/gfx/sprites_16.png (Transparent sprite sheet for moving objects & toolbar icons).
4. assets/gfx/tiles/ (Individual 16x16 PNG for each tile).
5. assets/gfx/tileset.json (Complete tile index and metadata).
"""

import os
import re
import sys
import json
from PIL import Image

def get_category(tile_id, name):
    name_lower = name.lower()
    if "sprite" in name_lower:
        return "sprite"
    if "toolbar" in name_lower:
        return "toolbar"
    if tile_id == 0:
        return "dirt"
    if 2 <= tile_id <= 20:
        return "river"
    if 21 <= tile_id <= 36:
        return "trees"
    if 37 <= tile_id <= 43:
        return "woods"
    if 44 <= tile_id <= 47:
        return "rubble"
    if 48 <= tile_id <= 51:
        return "flood"
    if tile_id == 52:
        return "radioactive"
    if 56 <= tile_id <= 63:
        return "fire"
    if 64 <= tile_id <= 207:
        return "road"
    if 208 <= tile_id <= 223:
        return "power"
    if 224 <= tile_id <= 239:
        return "rail"
    if 240 <= tile_id <= 422:
        return "residential"
    if 423 <= tile_id <= 611:
        return "commercial"
    if 612 <= tile_id <= 692:
        return "industrial"
    if 693 <= tile_id <= 708:
        return "seaport"
    if 709 <= tile_id <= 744:
        return "airport"
    if 745 <= tile_id <= 760:
        return "coal_power"
    if 761 <= tile_id <= 769:
        return "fire_dept"
    if 770 <= tile_id <= 778:
        return "police_dept"
    if 779 <= tile_id <= 810:
        return "stadium"
    if 811 <= tile_id <= 826:
        return "nuclear_power"
    if 827 <= tile_id <= 959:
        return "animated"
    return "misc"

def main():
    repo_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    vt_tiles_path = os.path.join(repo_dir, "reference/vtcity/src/headers/vt_tiles.h")
    index_path = os.path.join(repo_dir, "reference/vtcity/src/glyphs/INDEX.txt")
    out_gfx_dir = os.path.join(repo_dir, "assets/gfx")
    out_tiles_dir = os.path.join(out_gfx_dir, "tiles")

    os.makedirs(out_gfx_dir, exist_ok=True)
    os.makedirs(out_tiles_dir, exist_ok=True)

    if not os.path.isfile(vt_tiles_path):
        print(f"Error: {vt_tiles_path} not found", file=sys.stderr)
        sys.exit(1)

    # 1. Parse INDEX.txt
    tile_names = {}
    if os.path.isfile(index_path):
        with open(index_path, "r", encoding="utf-8", errors="replace") as f:
            for line in f:
                line = line.strip()
                # Matches format: 750   tiles-07  93   '}'   coal power plant
                m = re.match(r"^(\d+)\s+tiles-\d+\s+\d+\s+'[^']*'\s+(.*)$", line)
                if m:
                    tid = int(m.group(1))
                    tname = m.group(2).strip()
                    tile_names[tid] = tname

    # 2. Parse vt_tiles.h
    with open(vt_tiles_path, "r", encoding="utf-8") as f:
        content = f.read()

    m = re.search(r"VtTileBits\[VT_NALLTILE\]\[16\]\[2\]\s*=\s*\{(.*?)\n\};", content, re.DOTALL)
    if not m:
        print("Error: Could not find VtTileBits in vt_tiles.h", file=sys.stderr)
        sys.exit(1)

    lines = [l.strip() for l in m.group(1).split("\n") if l.strip().startswith("{{")]
    num_tiles = len(lines)
    print(f"Found {num_tiles} tiles in vt_tiles.h")

    tiles_data = []
    for l in lines:
        rows = re.findall(r"\{(\d+),\s*(\d+)\}", l)
        tiles_data.append([(int(r[0]), int(r[1])) for r in rows])

    # Atlas settings: 32 tiles per row (512 pixels wide)
    tiles_per_row = 32
    rows_needed = (num_tiles + tiles_per_row - 1) // tiles_per_row
    atlas_w = 512
    atlas_h = 512  # power of two

    # Create master tile atlas: RGBA with white background and black ink
    atlas_img = Image.new("RGBA", (atlas_w, atlas_h), (255, 255, 255, 255))
    atlas_pix = atlas_img.load()

    # Create 1-bit monochrome atlas (0 = black ink, 1 = white paper)
    atlas_1bit = Image.new("1", (atlas_w, atlas_h), 1)
    atlas_1bit_pix = atlas_1bit.load()

    # Create spritesheet for moving objects (tiles 960..965) and toolbar icons (966..968)
    # 9 items * 16 = 144x16 (or 160x16)
    sprite_sheet = Image.new("RGBA", (160, 16), (0, 0, 0, 0))
    sprite_pix = sprite_sheet.load()

    metadata = {
        "tile_width": 16,
        "tile_height": 16,
        "tiles_per_row": tiles_per_row,
        "total_tiles": num_tiles,
        "atlas_width": atlas_w,
        "atlas_height": atlas_h,
        "tiles": []
    }

    print(f"Extracting {num_tiles} tiles to {out_tiles_dir} and creating atlases...")

    for t, rows in enumerate(tiles_data):
        tile_name = tile_names.get(t, f"tile_{t}")
        category = get_category(t, tile_name)

        # Atlas grid coordinates
        col = t % tiles_per_row
        row = t // tiles_per_row
        ax = col * 16
        ay = row * 16

        # Individual tile image
        single_img = Image.new("RGBA", (16, 16), (255, 255, 255, 255))
        single_pix = single_img.load()

        for y in range(16):
            b0, b1 = rows[y]
            for x in range(8):
                bit0 = (b0 >> x) & 1
                bit1 = (b1 >> x) & 1

                # Left 8 pixels (x)
                if bit0:
                    atlas_pix[ax + x, ay + y] = (0, 0, 0, 255)
                    atlas_1bit_pix[ax + x, ay + y] = 0
                    single_pix[x, y] = (0, 0, 0, 255)

                # Right 8 pixels (8 + x)
                if bit1:
                    atlas_pix[ax + 8 + x, ay + y] = (0, 0, 0, 255)
                    atlas_1bit_pix[ax + 8 + x, ay + y] = 0
                    single_pix[8 + x, y] = (0, 0, 0, 255)

        # If it is a sprite tile (960..968), also populate transparent sprite sheet
        if 960 <= t <= 968:
            s_idx = t - 960
            sx_base = s_idx * 16
            for y in range(16):
                b0, b1 = rows[y]
                for x in range(8):
                    if (b0 >> x) & 1:
                        sprite_pix[sx_base + x, y] = (0, 0, 0, 255)
                    if (b1 >> x) & 1:
                        sprite_pix[sx_base + 8 + x, y] = (0, 0, 0, 255)

        # Save individual tile PNG
        single_path = os.path.join(out_tiles_dir, f"tile_{t:04d}.png")
        single_img.save(single_path)

        metadata["tiles"].append({
            "id": t,
            "name": tile_name,
            "category": category,
            "col": col,
            "row": row,
            "x": ax,
            "y": ay,
            "width": 16,
            "height": 16
        })

    # Save atlases
    atlas_rgba_path = os.path.join(out_gfx_dir, "tiles_16.png")
    atlas_img.save(atlas_rgba_path)
    print(f"Saved RGBA atlas: {atlas_rgba_path} ({atlas_w}x{atlas_h})")

    atlas_1bit_path = os.path.join(out_gfx_dir, "tiles_16_mono.png")
    atlas_1bit.save(atlas_1bit_path)
    print(f"Saved 1-bit atlas: {atlas_1bit_path}")

    sprite_sheet_path = os.path.join(out_gfx_dir, "sprites_16.png")
    sprite_sheet.save(sprite_sheet_path)
    print(f"Saved sprite sheet: {sprite_sheet_path}")

    json_path = os.path.join(out_gfx_dir, "tileset.json")
    with open(json_path, "w", encoding="utf-8") as f:
        json.dump(metadata, f, indent=2)
    print(f"Saved metadata: {json_path}")

    # Also generate a C header mapping tile IDs and sprites
    header_path = os.path.join(repo_dir, "src/micropolis/headers/tileset_atlas.h")
    with open(header_path, "w", encoding="utf-8") as f:
        f.write("/* tileset_atlas.h - Generated by extract_tiles.py. Do not edit directly. */\n")
        f.write("#ifndef TILESET_ATLAS_H\n#define TILESET_ATLAS_H\n\n")
        f.write(f"#define TILE_SIZE 16\n")
        f.write(f"#define TILES_PER_ROW {tiles_per_row}\n")
        f.write(f"#define TOTAL_TILES {num_tiles}\n")
        f.write(f"#define ATLAS_WIDTH {atlas_w}\n")
        f.write(f"#define ATLAS_HEIGHT {atlas_h}\n\n")
        f.write("/* Sprites & Toolbar Tile IDs */\n")
        f.write("#define SPRITE_TRAIN_TILE    960\n")
        f.write("#define SPRITE_COPTER_TILE   961\n")
        f.write("#define SPRITE_PLANE_TILE    962\n")
        f.write("#define SPRITE_SHIP_TILE     963\n")
        f.write("#define SPRITE_MONSTER_TILE  964\n")
        f.write("#define SPRITE_TORNADO_TILE  965\n")
        f.write("#define TOOLBAR_VILLA_TILE   966\n")
        f.write("#define TOOLBAR_SHOP_TILE    967\n")
        f.write("#define TOOLBAR_FACTORY_TILE 968\n\n")
        f.write("static inline int tile_atlas_x(int tile_id) {\n")
        f.write(f"  return (tile_id % {tiles_per_row}) * 16;\n")
        f.write("}\n\n")
        f.write("static inline int tile_atlas_y(int tile_id) {\n")
        f.write(f"  return (tile_id / {tiles_per_row}) * 16;\n")
        f.write("}\n\n")
        f.write("#endif /* TILESET_ATLAS_H */\n")
    print(f"Saved C header: {header_path}")
    print("Done extraction successfully!")

if __name__ == "__main__":
    main()
