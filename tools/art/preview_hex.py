#!/usr/bin/env python3
"""Composites a hexagon game's map from its generated sheets, using docs/HEX.md's geometry.

usage: python3 tools/art/preview_hex.py GAME_DIR MAP_ID OUT.png [--scale N] [--frame 0|1] [--no-units]

A stand-in for the player's hexagon drawing so the art can be looked at (and tested): terrain first
(variants, join pieces by the six-bit neighbour mask, animated frame), then structures and units sorted by the
bottom row of their picture. Writes a PNG; prints nothing else. Needs Pillow.
"""
import json
import os
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from PIL import Image  # noqa: E402

from hex_geom import Geom  # noqa: E402
from make_hex_art import load_json  # noqa: E402


class Sheets:
    def __init__(self, game_dir, doc):
        self.images, self.cells = {}, {}
        for s in doc['sheets']:
            self.images[s['id']] = Image.open(os.path.join(game_dir, s['file'])).convert('RGBA')
            self.cells[s['id']] = (s['cellWidth'], s['cellHeight'])

    def cell(self, sheet, col, row):
        w, h = self.cells[sheet]
        return self.images[sheet].crop((col * w, row * h, (col + 1) * w, (row + 1) * h))


def join_mask(g, tiles, look, ids_same, x, y, width, height):
    """Six-bit mask: bit i = neighbour i counts as the same (docs/HEX.md "Join pieces on hexagons")."""
    m = 0
    for d in range(6):
        nx, ny = g.neighbor(x, y, d)
        if 0 <= nx < width and 0 <= ny < height:
            same = ids_same(tiles[ny][nx])
        else:
            same = look.get('joinEdge', True)
        m |= int(same) << d
    return m


def render(game_dir, map_id, frame=0, units=True):
    game = load_json(os.path.join(game_dir, 'game.json'))
    g = Geom(game['grid']['layout'], game['view']['tileSize'])
    doc = load_json(os.path.join(game_dir, 'data/graphics.json'))
    sheets = Sheets(game_dir, doc)
    entry = [m for m in game['maps'] if m['id'] == map_id][0]
    mp = load_json(os.path.join(game_dir, entry['file']))
    width, height = mp['grid']['width'], mp['grid']['height']
    legend = mp['legend']
    tiles = [[legend[ch] for ch in row] for row in mp['tiles']]
    mw, mh = g.map_size(width, height)
    img = Image.new('RGBA', (mw, mh), (24, 26, 36, 255))

    def blit(im, x, y):
        img.alpha_composite(im, (x, y)) if x >= 0 and y >= 0 and x + im.width <= mw and y + im.height <= mh else \
            img.paste(im, (x, y), im)

    for y in range(height):
        for x in range(width):
            tid = tiles[y][x]
            look = doc['terrain'].get(tid)
            if not look:
                continue
            ox, oy = g.box_origin(x, y)
            if 'join' in look:
                same_ids = {tid} | set(look.get('joinWith', []))
                if 'joinExcept' in look:
                    ex = set(look['joinExcept'])
                    same = lambda t, ex=ex: t not in ex
                else:
                    same = lambda t, s=same_ids: t in s
                n = join_mask(g, tiles, look, same, x, y, width, height)
                blocks = look['join'] if isinstance(look['join'][0], list) else [look['join']]
                c, r = blocks[frame % len(blocks)]
                im = sheets.cell(look['sheet'], c + n % 8, r + n // 8)
            elif 'variants' in look:
                c, r = look['variants'][(x * 7 + y * 13) % len(look['variants'])]
                im = sheets.cell(look['sheet'], c, r)
            else:
                im = sheets.cell(look['sheet'], *look['cell'])
            blit(im, ox, oy)

    items = []   # (bottom row of the picture, kind order, image, x, y)
    for s in mp.get('structures', []):
        look = doc['structures'][s['type']]
        sdef = [d for d in load_json(os.path.join(game_dir, 'data/structures.json'))['structures'] if d['id'] == s['type']][0]
        im = sheets.cell(look['sheet'], *look['cell'])
        ox, oy = g.box_origin(s['x'], s['y'])
        if sdef.get('rings'):
            rw, rh = g.ring_size()
            ox, oy = ox - (rw - g.w) // 2, oy - (rh - g.h) // 2
            by = oy + rh
        else:
            by = oy + g.h
            oy = by - im.height
        items.append((by, 0, im, ox, oy))
    if units:
        for u in mp.get('units', []):
            look = doc['units'][u['type']]
            im = sheets.cell(look['sheet'], *look['frames'][frame % len(look['frames'])])
            ox, oy = g.box_origin(u['x'], u['y'])
            cx, cy = ox + g.w // 2, oy + g.h // 2
            items.append((oy + g.h, 1, im, cx - im.width // 2, cy - im.height // 2))
    for _, _, im, x, y in sorted(items, key=lambda t: (t[0], t[1])):
        img.alpha_composite(im, (x, y)) if x >= 0 and y >= 0 and x + im.width <= mw and y + im.height <= mh else \
            img.paste(im, (x, y), im)
    return img


def main():
    args = sys.argv[1:]
    scale, frame, units = 2, 0, True
    if '--scale' in args:
        i = args.index('--scale')
        scale = int(args[i + 1])
        del args[i:i + 2]
    if '--frame' in args:
        i = args.index('--frame')
        frame = int(args[i + 1])
        del args[i:i + 2]
    if '--no-units' in args:
        args.remove('--no-units')
        units = False
    game_dir, map_id, out = args
    img = render(game_dir, map_id, frame, units)
    if scale != 1:
        img = img.resize((img.width * scale, img.height * scale), Image.NEAREST)
    img.save(out)


if __name__ == '__main__':
    main()
