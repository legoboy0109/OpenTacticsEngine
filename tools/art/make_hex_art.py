#!/usr/bin/env python3
"""Draws the hexagon starter art set (docs/HEX.md "Starter content", docs/GRAPHICS.md "Starter art").

usage: python3 tools/art/make_hex_art.py [REPO_ROOT [GAME_FOLDER ...]]

For each hexagon game folder below it reads game.json (grid layout), data/terrain.json,
data/structures.json and data/units.json, then writes art/tiles.png (terrain, join blocks of 8 x 8
pieces), art/structures.png (one-hexagon structures), art/structures_ring.png (structures with
"rings": 1), art/units.png (the square starter's unit sprites, unchanged) and data/graphics.json.
Styles whose view is "isometric" get art/iso_*.png (art_hex_iso.py), styles of the two 3D kinds get art/ground3d.png and
art/stand.png (art_hex_3d.py; their terrain, structures and units otherwise use the top-down pictures and the isometric
unit sprites). Everything is drawn from shapes with a fixed palette and fixed random seeds, so
re-running reproduces identical files. Needs Python 3 and Pillow (like make_starter_art.py).

Sheet layout of art/tiles.png (cells are the hexagon's box: 24 x 28 pointy, 28 x 24 flat; 16 x 26 cells):
  row 0           plain (3 variants, columns 0-2), forest (3), mountain (4)
  rows 2-9        road: one 8 x 8 block, piece n at column n % 8, row 2 + n / 8
  rows 10-17      water: frame 0 block at column 0, frame 1 block at column 8
  rows 18-25      bridge: frame 0 at column 0, frame 1 at column 8
"""
import json
import os
import sys

sys.dont_write_bytecode = True  # keep tools/art free of __pycache__
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import art_hex_3d  # noqa: E402
import art_hex_iso as hiso  # noqa: E402
import art_hex_structures as hst  # noqa: E402
import art_hex_tiles as htl  # noqa: E402
import art_tiles as tl  # noqa: E402
import art_iso  # noqa: E402
import art_units as un  # noqa: E402
from hex_geom import Geom  # noqa: E402
from hex_iso_geom import IsoGeom  # noqa: E402
from pix import Pix  # noqa: E402

GAMES = ['games/demo-hex', 'editor/templates/hex-tactics-pointy', 'editor/templates/hex-tactics-flat']
T = 24
BLOCK = 8                       # join blocks are 8 x 8 pieces on hexagons
PIECES = BLOCK * BLOCK
TILE_CELLS = {'plain0': (0, 0), 'plain1': (1, 0), 'plain2': (2, 0), 'forest': (3, 0), 'mountain': (4, 0)}
ROAD_AT = (0, 2)
WATER_AT = (0, 10)
BRIDGE_AT = (0, 18)
FRAMES = 2
FRAME_MS = 500
SHEET_COLS, SHEET_ROWS = BLOCK * FRAMES, 26


def strip_comments(text):
    """Removes // line comments that are not inside strings (same as make_starter_art.py)."""
    out, i, in_str = [], 0, False
    while i < len(text):
        ch = text[i]
        if in_str:
            out.append(ch)
            if ch == '\\':
                out.append(text[i + 1])
                i += 1
            elif ch == '"':
                in_str = False
        elif ch == '"':
            in_str = True
            out.append(ch)
        elif ch == '/' and text[i:i + 2] == '//':
            while i < len(text) and text[i] != '\n':
                i += 1
            continue
        else:
            out.append(ch)
        i += 1
    return ''.join(out)


def load_json(path):
    with open(path, encoding='utf-8') as f:
        return json.loads(strip_comments(f.read()))


def build_tile_sheet(g):
    sheet = Pix(SHEET_COLS * g.w, SHEET_ROWS * g.h)

    def put(tile, col, row):
        sheet.paste(tile, col * g.w, row * g.h)
    for v in range(3):
        put(htl.plain(g, v), *TILE_CELLS['plain%d' % v])
    put(htl.forest(g), *TILE_CELLS['forest'])
    put(htl.mountain(g), *TILE_CELLS['mountain'])
    for n in range(PIECES):
        col, row = n % BLOCK, n // BLOCK         # piece n: column n % 8, row n / 8 of its block
        put(htl.road_piece(g, n), ROAD_AT[0] + col, ROAD_AT[1] + row)
        for f in range(FRAMES):
            put(htl.water_piece(g, n, f, tl.RIVER), WATER_AT[0] + BLOCK * f + col, WATER_AT[1] + row)
            put(htl.bridge_piece(g, n, f), BRIDGE_AT[0] + BLOCK * f + col, BRIDGE_AT[1] + row)
    return sheet


def terrain_looks(terrain_ids):
    def frames(at):
        return [[at[0] + BLOCK * f, at[1]] for f in range(FRAMES)]
    ids = set(terrain_ids)
    known = {
        'plain': {'sheet': 'tiles', 'variants': [list(TILE_CELLS['plain%d' % v]) for v in range(3)]},
        'forest': {'sheet': 'tiles', 'cell': list(TILE_CELLS['forest'])},
        'mountain': {'sheet': 'tiles', 'cell': list(TILE_CELLS['mountain'])},
        'road': {'sheet': 'tiles', 'join': list(ROAD_AT), 'joinWith': [], 'joinEdge': True},
        # Water joins with itself and a bridge (no bank under a bridge) and the map edge; animated join.
        'water': {'sheet': 'tiles', 'join': frames(WATER_AT), 'frameMs': FRAME_MS,
                  'joinWith': [t for t in ('bridge',) if t in ids], 'joinEdge': True},
        # A bridge joins everything except the water it crosses, so its deck runs to the bank.
        'bridge': {'sheet': 'tiles', 'join': frames(BRIDGE_AT), 'frameMs': FRAME_MS,
                   'joinExcept': [t for t in ('water',) if t in ids], 'joinEdge': False},
    }
    out = {t: known[t] for t in terrain_ids if t in known}
    return out, [t for t in terrain_ids if t not in known]


def make_iso_art(path, root, g, layout, terrain, structs, units_here, sheets, art_dir):
    """The isometric starter set (art_hex_iso.py) for the styles of game.json whose view is "isometric": the sheets are
    added to `sheets`, the returned dict is graphics.json's "styles" (every such style gets the same set). The art is
    drawn for the default measures (tileWidth 2 x tileSize, heightStep tileWidth / 4)."""
    iso_ids = [s['id'] for s in load_json(os.path.join(path, 'game.json')).get('styles', []) if s.get('view') == 'isometric']
    if not iso_ids:
        return {}
    ig = IsoGeom(layout, T)
    hiso.terrain_sheet(g, ig, sys.modules[__name__]).image().save(os.path.join(art_dir, 'iso_tiles.png'))
    cw, ch = ig.cell
    sheets.append({'id': 'iso_tiles', 'file': 'art/iso_tiles.png', 'cellWidth': cw, 'cellHeight': ch})
    iso_set = {'terrain': {t: dict(look, sheet='iso_tiles') for t, look in terrain.items()}}
    structure_looks = {}
    for sheet_id, rings, size in (('iso_structures', 0, ig.single_size()), ('iso_structures_ring', 1, ig.ring_size())):
        ids = [sid for sid, r in structs if r == rings and sid in hiso.PICTURES and hiso.PICTURES[sid][1] == rings]
        if not ids:
            continue
        pix = Pix(size[0] * len(ids), size[1])
        for i, sid in enumerate(ids):
            pix.paste(hiso.PICTURES[sid][0](g, ig), i * size[0], 0)
            structure_looks[sid] = {'sheet': sheet_id, 'cell': [i, 0]}
        pix.image().save(os.path.join(art_dir, sheet_id + '.png'))
        sheets.append({'id': sheet_id, 'file': 'art/%s.png' % sheet_id, 'cellWidth': size[0], 'cellHeight': size[1]})
    if structure_looks:
        iso_set['structures'] = structure_looks
    if units_here:
        art_iso.unit_sheet(units_here).image().save(os.path.join(art_dir, 'iso_units.png'))
        sheets.append({'id': 'iso_units', 'file': 'art/iso_units.png', 'cellWidth': art_iso.UNIT_W, 'cellHeight': art_iso.UNIT_H})
        iso_set['units'] = {uid: {'sheet': 'iso_units', 'frames': [[0, row], [1, row]], 'frameMs': 600}
                            for row, uid in enumerate(units_here)}
    return {sid: iso_set for sid in iso_ids}


def make_game(root, repo):
    path = os.path.join(repo, root)
    layout = load_json(os.path.join(path, 'game.json'))['grid']['layout']
    g = Geom(layout, T)
    terrain_ids = [t['id'] for t in load_json(os.path.join(path, 'data/terrain.json'))['terrain']]
    unit_ids = [u['id'] for u in load_json(os.path.join(path, 'data/units.json'))['units']]
    structs = [(s['id'], s.get('rings', 0)) for s in load_json(os.path.join(path, 'data/structures.json'))['structures']]

    terrain, no_terrain = terrain_looks(terrain_ids)
    units_here = [u for u in un.ORDER if u in unit_ids]
    no_units = [u for u in unit_ids if u not in un.UNITS]
    for what, ids in (('terrain', no_terrain), ('unit', no_units)):
        if ids:
            print('  note: %s has no starter art for %s %s (they keep the placeholder look)' % (root, what, ids))

    art_dir = os.path.join(path, 'art')
    os.makedirs(art_dir, exist_ok=True)
    build_tile_sheet(g).image().save(os.path.join(art_dir, 'tiles.png'))
    sheets = [{'id': 'tiles', 'file': 'art/tiles.png', 'cellWidth': g.w, 'cellHeight': g.h}]

    # Structures: one sheet per size (one hexagon / seven), cells in one row in file order.
    structure_looks = {}
    for sheet_id, rings in (('structures', 0), ('structures_ring', 1)):
        ids = [sid for sid, r in structs if r == rings and sid in hst.PICTURES and hst.PICTURES[sid][1] == rings]
        if not ids:
            continue
        cw, ch = hst.cell_size(g) if rings == 0 else g.ring_size()
        pix = Pix(cw * len(ids), ch)
        for i, sid in enumerate(ids):
            pix.paste(hst.PICTURES[sid][0](g), i * cw, 0)
            structure_looks[sid] = {'sheet': sheet_id, 'cell': [i, 0]}
        pix.image().save(os.path.join(art_dir, sheet_id + '.png'))
        sheets.append({'id': sheet_id, 'file': 'art/%s.png' % sheet_id, 'cellWidth': cw, 'cellHeight': ch})
    no_structs = [sid for sid, _ in structs if sid not in structure_looks]
    if no_structs:
        print('  note: %s has no starter art for structure %s (they keep the placeholder look)' % (root, no_structs))

    # Units are the square set's sprites, unchanged: drawn on hexagons as on squares.
    unit_sheet = Pix(2 * T, len(units_here) * T)
    unit_looks = {}
    for row, uid in enumerate(units_here):
        for col in range(2):
            unit_sheet.paste(un.UNITS[uid](col), col * T, row * T)
        unit_looks[uid] = {'sheet': 'units', 'frames': [[0, row], [1, row]], 'frameMs': 600}
    if units_here:
        unit_sheet.image().save(os.path.join(art_dir, 'units.png'))
        sheets.append({'id': 'units', 'file': 'art/units.png', 'cellWidth': T, 'cellHeight': T})

    doc = {'sheets': sheets, 'unitMarker': True, 'terrain': terrain, 'units': unit_looks}
    if structure_looks:
        doc['structures'] = structure_looks
    iso_styles = make_iso_art(path, root, g, layout, terrain, structs, units_here, sheets, art_dir)
    three_d = [s['id'] for s in load_json(os.path.join(path, 'game.json')).get('styles', []) if s.get('view') in ('isometric-3d', '3d')]
    iso_styles.update(art_hex_3d.make_3d_art(three_d, g, terrain, structure_looks, units_here, sheets, art_dir,
                                             lambda pix, name: pix.image().save(os.path.join(art_dir, name))))
    if iso_styles:
        doc['styles'] = iso_styles
    text = ('// Starter art. Generated by tools/art/make_hex_art.py (re-run it to regenerate; edits are overwritten).\n'
            + json.dumps(doc, indent=2) + '\n')
    with open(os.path.join(path, 'data/graphics.json'), 'w', encoding='utf-8') as f:
        f.write(text)
    print('%s: %s, %d terrain, %d structures, %d units' % (
        root, layout, len(terrain), len(structure_looks), len(unit_looks)))


def main():
    repo = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
    for g in (sys.argv[2:] or GAMES):
        make_game(g, repo)


if __name__ == '__main__':
    main()
