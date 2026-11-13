#!/usr/bin/env python3
"""Shrinks the design-review screenshots (flat UI colours) to palette PNGs: 64 colours, fewer for the long
pages that would still be over the size budget. Run it on freshly generated screenshots (npm run screenshots).

usage: python3 tools/compress_screenshots.py [folder=screenshots] [budget_kb=22]
Needs Pillow.
"""
import sys
from pathlib import Path

from PIL import Image

folder = Path(sys.argv[1] if len(sys.argv) > 1 else "screenshots")
budget = int(sys.argv[2]) * 1024 if len(sys.argv) > 2 else 22 * 1024
before = after = 0
for png in sorted(folder.glob("*.png")):
    before += png.stat().st_size
    img = Image.open(png).convert("RGB")
    if img.width > 1200:  # nothing in the set needs to be wider than the 1180 px desktop shots
        img = img.resize((1180, round(img.height * 1180 / img.width)), Image.LANCZOS)
    for colors in (64, 48, 32, 24, 20):
        img.quantize(colors=colors, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE).save(png, optimize=True)
        if png.stat().st_size <= budget:
            break
    after += png.stat().st_size
print(f"{len(list(folder.glob('*.png')))} files: {before // 1024} KB -> {after // 1024} KB")
