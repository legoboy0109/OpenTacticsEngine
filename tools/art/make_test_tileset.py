#!/usr/bin/env python3
"""A small, original test tileset for the tile layer (docs/GRAPHICS.md "Tile layer (hand-painted maps)").

  python3 tools/art/make_test_tileset.py OUT.png        # writes the sheet: 8 x 4 tiles of 16 x 16 pixels (128 x 64)

Deterministic (the same bytes every run), no dependencies beyond the standard library. Tile n (counted row by row from 0) is a flat
colour (`tile_colour(n)`, 32 different ones) with, so that a picture can be told from its mirror image or a turned one:

  - a 2 x 2 black mark in its top-left corner,
  - a 2 x 2 white mark in its top-right corner,
  - the number n in 3 x 5 pixel digits, in the middle, in black or white (whichever reads better).

Tiles 8, 9 and 10 are three blues, meant as the frames of an animated "water" tile (tests give tile 8 an animation).
Importable: `tile_colour`, `tile_pixel`, `TILE`, `COLS`, `ROWS`, `write_sheet`.
"""
import struct
import sys
import zlib

TILE, COLS, ROWS = 16, 8, 4

# 3 x 5 digits, one string of 15 characters per digit (row by row).
DIGITS = ['111101101101111', '010110010010111', '111001111100111', '111001111001111', '101101111001001',
          '111100111001111', '111100111101111', '111001001001001', '111101111101111', '111101111001111']


def tile_colour(n):
    """The flat colour of tile n: waters (8..10) blue, otherwise spread round the colour wheel."""
    if n in (8, 9, 10):
        return {8: (40, 90, 200), 9: (60, 140, 220), 10: (90, 190, 235)}[n]
    # Eight hues x four brightnesses, none close to the blues above.
    hue = (n * 5) % 8
    shade = n // 8
    base = [(200, 60, 60), (220, 140, 40), (210, 200, 50), (90, 190, 80), (60, 170, 150), (150, 90, 200), (210, 90, 160), (150, 120, 80)][hue]
    k = (0.55, 0.75, 0.9, 1.0)[shade]
    return tuple(int(c * k) for c in base)


def tile_pixel(n, x, y):
    """The RGBA of pixel (x, y) of tile n."""
    if x < 2 and y < 2:
        return (0, 0, 0, 255)
    if x >= TILE - 2 and y < 2:
        return (255, 255, 255, 255)
    col = tile_colour(n)
    ink = (0, 0, 0, 255) if sum(col) > 380 else (255, 255, 255, 255)
    text = '%d' % n
    width = len(text) * 4 - 1
    x0, y0 = (TILE - width) // 2, (TILE - 5) // 2 + 1
    for i, ch in enumerate(text):
        gx, gy = x - (x0 + i * 4), y - y0
        if 0 <= gx < 3 and 0 <= gy < 5 and DIGITS[int(ch)][gy * 3 + gx] == '1':
            return ink
    return col + (255,)


def sheet_pixels():
    rows = []
    for y in range(ROWS * TILE):
        row = bytearray()
        for x in range(COLS * TILE):
            n = (y // TILE) * COLS + x // TILE
            row += bytes(tile_pixel(n, x % TILE, y % TILE))
        rows.append(bytes(row))
    return rows


def write_sheet(path):
    rows = sheet_pixels()
    raw = b''.join(b'\0' + r for r in rows)

    def chunk(tag, data):
        c = struct.pack('>I', len(data)) + tag + data
        return c + struct.pack('>I', zlib.crc32(tag + data) & 0xffffffff)

    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', COLS * TILE, ROWS * TILE, 8, 6, 0, 0, 0)) \
        + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b'')
    with open(path, 'wb') as f:
        f.write(png)


if __name__ == '__main__':
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    write_sheet(sys.argv[1])
