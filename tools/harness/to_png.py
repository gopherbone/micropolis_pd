#!/usr/bin/env python3
"""Convert harness .pgm screenshots to 2x PNGs."""
import glob, os, sys
from PIL import Image
for p in glob.glob(os.path.join(sys.argv[1], "*.pgm")):
    im = Image.open(p)
    im.resize((im.width * 2, im.height * 2), Image.NEAREST).save(p[:-4] + ".png")
    os.remove(p)
