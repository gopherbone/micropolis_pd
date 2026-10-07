#!/usr/bin/env python3
"""Mirror Source/**/*.png into <out>/**/*.raw (w h hasmask, then 0=black 1=white 2=clear)."""
import os, sys
from PIL import Image
src, out = sys.argv[1], sys.argv[2]
for root, _, files in os.walk(src):
    for fn in files:
        if not fn.endswith(".png"):
            continue
        im = Image.open(os.path.join(root, fn))
        has_alpha = im.mode in ("RGBA", "LA") or "transparency" in im.info
        rgba = im.convert("RGBA")
        lum = rgba.convert("L")
        a = rgba.getchannel("A")
        w, h = im.size
        data = bytearray()
        for y in range(h):
            for x in range(w):
                if has_alpha and a.getpixel((x, y)) < 128:
                    data.append(2)
                else:
                    data.append(1 if lum.getpixel((x, y)) >= 128 else 0)
        rel = os.path.relpath(os.path.join(root, fn[:-4] + ".raw"), src)
        dst = os.path.join(out, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        with open(dst, "wb") as f:
            f.write(b"%d %d %d\n" % (w, h, 1 if has_alpha else 0))
            f.write(bytes(data))
