#!/usr/bin/env python3
"""Draws the engine's original starter art set (docs/GRAPHICS.md "Starter art").

usage: python3 tools/art/make_starter_art.py [REPO_ROOT]

For each game folder below it reads data/terrain.json, data/structures.json
(optional), data/units.json and game.json, then writes art/tiles.png,
art/units.png, art/structures*.png (one sheet per footprint size) and a tinted
copy of the tile and structure sheets for styles that have a tint here, plus
data/graphics.json. No input images; everything is drawn from shapes with a
fixed palette and fixed random seeds, so re-running reproduces identical files.
Needs Python 3 and Pillow.
"""
import json
import os
import sys

sys.dont_write_bytecode = True  # keep tools/art free of __pycache__

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import art_structures as st  # noqa: E402
import art_tiles as tl  # noqa: E402
import art_units as un  # noqa: E402
from pix import Pix  # noqa: E402

GAMES = [
    'games/demo-srpg', 'games/demo-economy',
    'editor/templates/strategy-rpg', 'editor/templates/economy-tactics', 'editor/templates/blank',
]
T = 24

# Tile sheet layout: (column, row) of each tile; join blocks are 4 x 4 cells.
TILE_CELLS = {
    'plain0': (0, 0), 'plain1': (1, 0), 'plain2': (2, 0), 'forest': (3, 0),
    'mountain': (4, 0), 'wall': (5, 0),
    'road': (0, 2),
}
# Animated join blocks: one 4 x 4 block per frame, frames side by side, one band of 4 rows per terrain.
JOIN_FRAMES = 2
WATER_BLOCKS = {'sea': (0, 6), 'water': (0, 10), 'bridge': (0, 14)}  # (column, row) of frame 0
SHEET_COLS, SHEET_ROWS = 4 * JOIN_FRAMES, 18
FRAME_MS = 500


def strip_comments(text):
    """Removes // line comments that are not inside strings."""
    out, i, in_str = [], 0, False
    while i < len(text):
        ch = text[i]
        if in_str:
            out.append(ch)
            if ch == '\\':
                out.append(text[i + 1]); i += 1
            elif ch == '"':
                in_str = False
        elif ch == '"':
            in_str = True; out.append(ch)
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


def build_tile_sheet():
    sheet = Pix(SHEET_COLS * T, SHEET_ROWS * T)
    def put(name, tile, dc=0, dr=0):
        c, r = TILE_CELLS[name]
        sheet.paste(tile, (c + dc) * T, (r + dr) * T)
    put('plain0', tl.grass(1, tufts=6))
    put('plain1', tl.grass(2, tufts=9))
    put('plain2', tl.grass(3, tufts=5, flowers=3))
    put('forest', tl.forest())
    put('mountain', tl.mountain())
    put('wall', tl.wall_tile())
    for name, pal in (('sea', tl.SEA), ('water', tl.RIVER)):
        for f in range(JOIN_FRAMES):
            for n in range(16):
                sheet.paste(tl.water_piece(n, f, pal), (WATER_BLOCKS[name][0] + 4 * f + n % 4) * T,
                            (WATER_BLOCKS[name][1] + n // 4) * T)
    for n in range(16):
        put('road', tl.road_piece(n), n % 4, n // 4)
        for f in range(JOIN_FRAMES):
            sheet.paste(tl.bridge_piece(n, f), (WATER_BLOCKS['bridge'][0] + 4 * f + n % 4) * T,
                        (WATER_BLOCKS['bridge'][1] + n // 4) * T)
    return sheet


# Tints for the alternative tile sheets of visual styles.
def night(r, g, b):
    return r * 0.42, g * 0.52, b * 0.80 + 14


def sepia(r, g, b):
    """Old-map look: warm browns with more contrast; water keeps a faded blue so rivers still read."""
    lum = (0.30 * r + 0.59 * g + 0.11 * b) / 255.0
    lum = max(0.0, min(1.0, (lum - 0.5) * 1.6 + 0.5))
    if b > r + 25:
        return 55 + 95 * lum, 85 + 95 * lum, 110 + 100 * lum
    return 55 + 185 * lum, 38 + 160 * lum, 20 + 115 * lum


STYLE_TINTS = {'night': night, 'parchment': sepia}


def terrain_looks(terrain_ids):
    """Terrain id -> look (dict for graphics.json), for the ids this set knows."""
    def cell(name):
        return list(TILE_CELLS[name])
    def join_frames(name):
        c, r = WATER_BLOCKS[name]
        return [[c + 4 * f, r] for f in range(JOIN_FRAMES)]
    known = {
        'plain': {'sheet': 'tiles', 'variants': [cell('plain0'), cell('plain1'), cell('plain2')]},
        'forest': {'sheet': 'tiles', 'cell': cell('forest')},
        'mountain': {'sheet': 'tiles', 'cell': cell('mountain')},
        'wall': {'sheet': 'tiles', 'cell': cell('wall')},
    }
    ids = set(terrain_ids)
    road_with = [t for t in ('bridge',) if t in ids]  # buildings are structures now: roads run under them
    # Water joins with itself (banks only where the neighbour is not water), with the map edge, and with a
    # bridge (no bank under it). Its look is an animated join: one block per frame.
    for wid in ('sea', 'water'):
        if wid in ids:
            known[wid] = {'sheet': 'tiles', 'join': join_frames(wid), 'frameMs': FRAME_MS,
                          'joinWith': [t for t in ('bridge',) if t in ids], 'joinEdge': True}
    if 'road' in ids:
        known['road'] = {'sheet': 'tiles', 'join': cell('road'), 'joinWith': road_with, 'joinEdge': True}
    if 'bridge' in ids:
        # Joins with every terrain except the water it crosses: the deck then runs to the edge of the
        # square beside land (and beside the next bridge square) and shows rails with water on the rest.
        water = [t for t in ('sea', 'water') if t in ids]
        known['bridge'] = {'sheet': 'tiles', 'join': join_frames('bridge'), 'frameMs': FRAME_MS,
                           'joinExcept': water, 'joinEdge': False}
    out, missing = {}, []
    for tid in terrain_ids:
        if tid in known:
            out[tid] = known[tid]
        else:
            missing.append(tid)
    return out, missing


def structure_sheets(structs):
    """structs: list of (id, width, height) from data/structures.json.
    Returns (sheets, looks, missing): sheets = [(sheet id, Pix, cell w, cell h)], one per footprint
    size, cells in one row in file order; looks = structure id -> look dict (untinted sheet ids)."""
    groups, missing = {}, []
    for sid, w, h in structs:
        if (w, h) in st.PICTURES.get(sid, {}):
            groups.setdefault((w, h), []).append(sid)
        else:
            missing.append(sid)
    sheets, looks = [], {}
    for (w, h), ids in sorted(groups.items()):
        cw, ch = st.cell_size(w, h)
        sheet_id = 'structures' if (w, h) == (1, 1) else 'structures_%dx%d' % (w, h)
        pix = Pix(cw * len(ids), ch)
        for i, sid in enumerate(ids):
            pix.paste(st.PICTURES[sid][(w, h)](), i * cw, 0)
            looks[sid] = {'sheet': sheet_id, 'cell': [i, 0]}
        sheets.append((sheet_id, pix, cw, ch))
    return sheets, looks, missing


def make_game(root, repo):
    path = os.path.join(repo, root)
    terrain_ids = [t['id'] for t in load_json(os.path.join(path, 'data/terrain.json'))['terrain']]
    unit_ids = [u['id'] for u in load_json(os.path.join(path, 'data/units.json'))['units']]
    style_ids = [s['id'] for s in load_json(os.path.join(path, 'game.json')).get('styles', [])]
    struct_path = os.path.join(path, 'data/structures.json')
    structs = []
    if os.path.exists(struct_path):
        structs = [(s['id'], s.get('width', 1), s.get('height', 1)) for s in load_json(struct_path)['structures']]

    terrain, no_terrain = terrain_looks(terrain_ids)
    units_here = [u for u in un.ORDER if u in unit_ids]
    no_units = [u for u in unit_ids if u not in un.UNITS]
    struct_sheets, structure_looks, no_structs = structure_sheets(structs)
    for what, ids in (('terrain', no_terrain), ('unit', no_units), ('structure', no_structs)):
        if ids:
            print('  note: %s has no starter art for %s %s (they keep the placeholder look)' % (root, what, ids))

    art_dir = os.path.join(path, 'art')
    os.makedirs(art_dir, exist_ok=True)
    tiles = build_tile_sheet()
    tiles.image().save(os.path.join(art_dir, 'tiles.png'))
    sheets = [{'id': 'tiles', 'file': 'art/tiles.png', 'cellWidth': T, 'cellHeight': T}]

    for sheet_id, pix, cw, ch in struct_sheets:
        pix.image().save(os.path.join(art_dir, sheet_id + '.png'))
        sheets.append({'id': sheet_id, 'file': 'art/%s.png' % sheet_id, 'cellWidth': cw, 'cellHeight': ch})

    unit_sheet = Pix(2 * T, len(units_here) * T)
    unit_looks = {}
    for row, uid in enumerate(units_here):
        for col in range(2):
            unit_sheet.paste(un.UNITS[uid](col), col * T, row * T)
        unit_looks[uid] = {'sheet': 'units', 'frames': [[0, row], [1, row]], 'frameMs': 600}
    if units_here:
        unit_sheet.image().save(os.path.join(art_dir, 'units.png'))
        sheets.append({'id': 'units', 'file': 'art/units.png', 'cellWidth': T, 'cellHeight': T})

    styles = {}
    for sid in style_ids:
        tint = STYLE_TINTS.get(sid)
        if not tint:
            continue
        sheet_id = 'tiles_' + sid
        tiles.mapped(tint).image().save(os.path.join(art_dir, sheet_id + '.png'))
        sheets.append({'id': sheet_id, 'file': 'art/%s.png' % sheet_id, 'cellWidth': T, 'cellHeight': T})
        styles[sid] = {'terrain': {tid: dict(look, sheet=sheet_id) for tid, look in terrain.items()}}
        tinted = {}
        for base_id, pix, cw, ch in struct_sheets:
            tid = '%s_%s' % (base_id, sid)
            pix.mapped(tint).image().save(os.path.join(art_dir, tid + '.png'))
            sheets.append({'id': tid, 'file': 'art/%s.png' % tid, 'cellWidth': cw, 'cellHeight': ch})
            for stid, look in structure_looks.items():
                if look['sheet'] == base_id:
                    tinted[stid] = dict(look, sheet=tid)
        if tinted:
            styles[sid]['structures'] = tinted

    doc = {'sheets': sheets, 'unitMarker': True, 'terrain': terrain, 'units': unit_looks}
    if structure_looks:
        doc['structures'] = structure_looks
    if styles:
        doc['styles'] = styles
    text = ('// Starter art. Generated by tools/art/make_starter_art.py (re-run it to regenerate; edits are overwritten).\n'
            + json.dumps(doc, indent=2) + '\n')
    with open(os.path.join(path, 'data/graphics.json'), 'w', encoding='utf-8') as f:
        f.write(text)
    print('%s: %d terrain, %d structures, %d units, %d style override(s)' % (
        root, len(terrain), len(structure_looks), len(unit_looks), len(styles)))


def main():
    repo = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
    for g in GAMES:
        make_game(g, repo)


main()
