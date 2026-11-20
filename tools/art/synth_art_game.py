#!/usr/bin/env python3
"""Makes a game with many big art sheets, to measure and test the player's art memory (docs/GRAPHICS.md "Art memory").

  python3 tools/art/synth_art_game.py games/demo-srpg OUT_DIR --sheets 300 --size 1024 --used 6

Copies the game folder SRC to OUT_DIR, then adds `--sheets` generated PNG sheets of `--size` x `--size` pixels (32 x 32 cells,
each cell a flat colour of its own, so a wrong sheet or cell shows in a screenshot) and registers them in data/graphics.json
as "filler_000", "filler_001" ... The first `--used` of them are drawn: they replace the look of the game's terrains, then
its unit types (default style), one sheet each. The rest are declared but nothing uses them, like the art of a big game
that is not on screen. The PNGs compress very well (a big game's 640 sheets are 18 MB on disk and 1.4 GB decoded).

As a module: add_filler_sheets(game_dir, count, size, used) does the second step on a game folder that already exists.
Standard library only; never run on a private game (it rewrites data/graphics.json of the folder it is given).
"""
import argparse
import json
import os
import shutil
import struct
import sys
import zlib


def write_png(path, width, height, seed, cell=32):
    """A PNG of flat-coloured cells (cell x cell pixels each): the colour depends on the sheet's seed and the cell's place."""
    def chunk(kind, data):
        body = kind + data
        return struct.pack('>I', len(data)) + body + struct.pack('>I', zlib.crc32(body) & 0xffffffff)

    cols = (width + cell - 1) // cell
    raw = bytearray()
    for cy in range((height + cell - 1) // cell):
        row = bytearray()
        for cx in range(cols):
            col = bytes(((seed * 53 + cx * 29 + cy * 7) % 200 + 40, (seed * 31 + cx * 11 + cy * 41) % 200 + 40,
                         (seed * 97 + cx * 5 + cy * 19) % 200 + 40, 255))
            row += col * cell
        line = b'\x00' + bytes(row[:width * 4])
        raw += line * min(cell, height - cy * cell)
    data = chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(bytes(raw), 1)) + chunk(b'IEND', b'')
    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n' + data)


def read_jsonc(path):
    with open(path) as f:
        return json.loads(''.join(l for l in f if not l.lstrip().startswith('//')))


def add_filler_sheets(game_dir, count, size, used=0, cell=32):
    """Adds `count` sheets of size x size to the game in game_dir; the first `used` replace terrain, then unit looks."""
    art = os.path.join(game_dir, 'art')
    os.makedirs(art, exist_ok=True)
    path = os.path.join(game_dir, 'data', 'graphics.json')
    gfx = read_jsonc(path)
    ids = []
    for i in range(count):
        sid = 'filler_%03d' % i
        write_png(os.path.join(art, sid + '.png'), size, size, i + 1, cell)
        gfx['sheets'].append({'id': sid, 'file': 'art/%s.png' % sid, 'cellWidth': cell, 'cellHeight': cell})
        ids.append(sid)
    targets = [('terrain', k) for k in gfx.get('terrain', {})] + [('units', k) for k in gfx.get('units', {})]
    for n, sid in enumerate(ids[:used]):
        if n >= len(targets):
            break
        group, key = targets[n]
        gfx[group][key] = {'sheet': sid, 'cell': [n % 4, 0]}
    with open(path, 'w') as f:
        json.dump(gfx, f, indent=1)
    return ids


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('src')
    ap.add_argument('dest')
    ap.add_argument('--sheets', type=int, default=300)
    ap.add_argument('--size', type=int, default=1024)
    ap.add_argument('--used', type=int, default=6)
    a = ap.parse_args()
    if os.path.exists(a.dest):
        sys.exit('%s exists already' % a.dest)
    shutil.copytree(a.src, a.dest)
    add_filler_sheets(a.dest, a.sheets, a.size, a.used)
    print('%s: %d sheets of %dx%d added (%d drawn)' % (a.dest, a.sheets, a.size, a.size, min(a.used, a.sheets)))


if __name__ == '__main__':
    main()
