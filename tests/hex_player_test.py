#!/usr/bin/env python3
"""The player's part of docs/HEX.md (hexagon maps in the top-down view), windowless.

  python3 tests/hex_player_test.py ./build/engine/player/ote-player .
  python3 tests/hex_player_test.py ./build/engine/player/ote-player . --shots OUT_DIR   # also keeps the screenshots (BMP)

Writes small hexagon games in a temp folder (both layouts; the art ones get a tiny sheet written here, byte by byte),
runs the player with --input scripts and --saves on a temp folder, and judges pixels and the Continue save:

  - the map is drawn as hexagons: every cell's middle shows its terrain, the corners of a cell's box show the
    neighbour (three terrains are coloured so that no two neighbours match) or the background;
  - the cursor goes by column and row and the highlight is on that hexagon;
  - the move range and the attack ring are the cells the rules give (distance on the contract's neighbour order,
    written again here independently of the engine), tinted as hexagons;
  - a tap on a hexagon picks that hexagon, also next to a shared edge; a unit moves and attacks (the save tells);
  - a structure with rings is seven hexagons framed on their outer edges only;
  - with fog on, the veil is hexagon shaped (no dimmed corner where a visible neighbour sits);
  - art: sheet cells drawn in the box without clipping, 8 x 8 join blocks pick the piece of the six neighbours, a
    ring structure's picture fills the box across all seven hexagons with its bottom edge on the box's bottom.

Pixels are read from the 480 x 270 screenshot; where the map sits on the screen follows from the camera rule (a map
smaller than the viewport is centred), which this file repeats.
"""
import json
import math
import os
import shutil
import struct
import sys
import tempfile
import zlib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import run_text, check, failures, save_of, summary, write_json  # noqa: E402

PLAYER = ''
REPO = ''
GAMES = ''
SHOTS = ''
W, H = 480, 270
TILE = 24
VIEW_W, VIEW_H = 352, 259  # the map viewport: the window minus the panel (128) and the hint strip (11)
BACKGROUND = (10, 10, 16)

# Terrain colours: no two neighbouring hexagons share one when the map is coloured with colour_index() below.
TERRAIN = {'a': ('plain', (150, 190, 90)), 'b': ('forest', (40, 110, 60)), 'c': ('sea', (60, 100, 190))}
BLUE, RED = (0x3d, 0x85, 0xc6), (0xcc, 0, 0)
GOLD = (0xd4, 0xaf, 0x37)


# ---- geometry, written again from docs/HEX.md (not shared with the engine on purpose)

class Geo:
    def __init__(self, layout, w, h, tile=TILE):
        self.pointy = layout == 'pointy'
        self.w, self.h, self.T = w, h, tile
        self.L = int(round(tile * 1.1547005 / 4.0)) * 4
        self.q = self.L // 4

    def box_size(self):
        return (self.T, self.L) if self.pointy else (self.L, self.T)

    def box(self, x, y):
        if self.pointy:
            return x * self.T + (self.T // 2 if y & 1 else 0), y * 3 * self.q
        return x * 3 * self.q, y * self.T + (self.T // 2 if x & 1 else 0)

    def middle(self, x, y):
        bx, by = self.box(x, y)
        bw, bh = self.box_size()
        return bx + bw // 2, by + bh // 2

    def map_size(self):
        if self.pointy:
            return self.w * self.T + self.T // 2, (self.h - 1) * 3 * self.q + self.L
        return (self.w - 1) * 3 * self.q + self.L, self.h * self.T + self.T // 2

    def cam(self):
        """The camera of a map smaller than the viewport: centred; the player rounds half up."""
        mw, mh = self.map_size()
        fx = -(VIEW_W - mw) / 2.0 if mw <= VIEW_W else 0.0
        fy = -(VIEW_H - mh) / 2.0 if mh <= VIEW_H else 0.0
        return math.floor(fx + 0.5), math.floor(fy + 0.5)

    def screen_box(self, x, y):
        bx, by = self.box(x, y)
        cx, cy = self.cam()
        return bx - cx, by - cy

    def screen_middle(self, x, y):
        mx, my = self.middle(x, y)
        cx, cy = self.cam()
        return mx - cx, my - cy

    def neighbors(self, x, y):
        """The six neighbours in the contract's order (None = off the map)."""
        if self.pointy:
            d = [(0, -1), (1, 0), (0, 1), (-1, 1), (-1, 0), (-1, -1)] if y % 2 == 0 else \
                [(1, -1), (1, 0), (1, 1), (0, 1), (-1, 0), (0, -1)]
        else:
            d = [(0, -1), (1, -1), (1, 0), (0, 1), (-1, 0), (-1, -1)] if x % 2 == 0 else \
                [(0, -1), (1, 0), (1, 1), (0, 1), (-1, 1), (-1, 0)]
        out = []
        for dx, dy in d:
            nx, ny = x + dx, y + dy
            out.append((nx, ny) if 0 <= nx < self.w and 0 <= ny < self.h else None)
        return out

    def cube(self, x, y):
        if self.pointy:
            q, r = x - (y - (y & 1)) // 2, y
        else:
            q, r = x, y - (x - (x & 1)) // 2
        return q, r, -q - r

    def distance(self, a, b):
        ca, cb = self.cube(*a), self.cube(*b)
        return max(abs(ca[0] - cb[0]), abs(ca[1] - cb[1]), abs(ca[2] - cb[2]))

    def cells(self):
        return [(x, y) for y in range(self.h) for x in range(self.w)]

    def nearest(self, px, py):
        """The cell whose middle is nearest to a point in map pixels (the contract's picking); None off the map."""
        best, bc = 1e18, None
        for y in range(-1, self.h + 1):
            for x in range(-1, self.w + 1):
                mx, my = self.middle(x, y)
                d = (mx - px) ** 2 + (my - py) ** 2
                if d < best:
                    best, bc = d, (x, y)
        x, y = bc
        return bc if 0 <= x < self.w and 0 <= y < self.h else None

    def colour_index(self, x, y):
        """0..2, different for neighbours (the triangular lattice is 3-colourable)."""
        q, r, _ = self.cube(x, y)
        return (q - r) % 3


# ---- the test games


def png_bytes(width, height, pixel):
    """A PNG (RGBA 8 bit) of width x height; pixel(x, y) -> (r, g, b, a)."""
    raw = b''.join(b'\0' + b''.join(bytes(pixel(x, y)) for x in range(width)) for y in range(height))

    def chunk(kind, data):
        c = struct.pack('>I', len(data)) + kind + data
        return c + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0)) +
            chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))


def unit(type_, side, x, y):
    return {'type': type_, 'side': side, 'x': x, 'y': y}


def tile_rows(geo, override=None):
    """Tiles: every cell coloured by colour_index (a/b/c), `override` {(x, y): char} on top."""
    rows = []
    for y in range(geo.h):
        rows.append(''.join((override or {}).get((x, y), 'abc'[geo.colour_index(x, y)]) for x in range(geo.w)))
    return rows


def write_game(out, gid, layout, maps, art=None, big=False):
    """A hexagon game. `maps` = {id: map dict}; `art` = None or the graphics.json parts (see art_game)."""
    g = os.path.join(out, gid)
    shutil.rmtree(g, ignore_errors=True)
    write_json(g + '/game.json', {
        'formatVersion': 1, 'name': 'Hex Player Test ' + layout, 'grid': {'type': 'hex', 'layout': layout},
        'view': {'width': W, 'height': H, 'tileSize': TILE},
        'stats': [{'id': 'hp', 'name': 'HP'}, {'id': 'mov', 'name': 'Move'}],
        'roles': {'hp': 'hp', 'move': 'mov'},
        'resources': [{'id': 'funds', 'name': 'Funds'}],
        'moveTypes': [{'id': 'foot', 'name': 'Foot'}],
        'rules': {'turnOrder': 'sides', 'counterAttacks': True, 'defeat': ['noUnits'],
                  'capture': {'enabled': True, 'amount': 'a.hp / 10'}},
        'formulas': {'damage': 'vs * (a.hp / a.maxhp)'},
        'maps': [{'id': m, 'name': m, 'file': 'maps/%s.json' % m} for m in maps]})
    write_json(g + '/data/terrain.json', {'terrain': [
        {'id': name, 'name': name, 'color': '#%02x%02x%02x' % col, 'glyph': ' '} for name, col in TERRAIN.values()]})
    write_json(g + '/data/units.json', {'units': [
        {'id': 'infantry', 'name': 'Infantry', 'glyph': 'i', 'moveType': 'foot', 'stats': {'hp': 100, 'mov': 3},
         'weapons': ['rifle'], 'canCapture': True, 'vision': 2}]})
    write_json(g + '/data/weapons.json', {'weapons': [{'id': 'rifle', 'name': 'Rifle', 'versus': {'infantry': 55}}]})
    write_json(g + '/data/structures.json', {'structures': [
        {'id': 'city', 'name': 'City', 'color': '#b7b7b7', 'glyph': 'c', 'capturePoints': 20},
        {'id': 'hq', 'name': 'Headquarters', 'color': '#%02x%02x%02x' % GOLD, 'glyph': 'H', 'rings': 1,
         'capturePoints': 20}]})
    for mid, m in maps.items():
        write_json(g + '/maps/%s.json' % mid, m)
    if art:
        write_json(g + '/data/graphics.json', art['graphics'])
        for name, data in art['files'].items():
            os.makedirs(os.path.dirname(os.path.join(g, name)), exist_ok=True)
            open(os.path.join(g, name), 'wb').write(data)
    return g


def make_map(geo, tiles=None, units=(), structures=(), fog=False, name='Map'):
    sides = [{'id': 'blue', 'name': 'Blue Army', 'color': '#3d85c6', 'controller': 'human'},
             {'id': 'red', 'name': 'Red Army', 'color': '#cc0000', 'controller': 'ai'}]
    m = {'name': name, 'grid': {'type': 'hex', 'width': geo.w, 'height': geo.h},
         'legend': {k: v[0] for k, v in TERRAIN.items()}, 'tiles': tile_rows(geo, tiles),
         'sides': sides, 'structures': list(structures), 'units': list(units)}
    if fog:
        m['fog'] = True
    return m


MAP_W, MAP_H = 9, 8
BIG_W, BIG_H = 30, 22


def plain_game(out, layout):
    geo = Geo(layout, MAP_W, MAP_H)
    # blue's infantry (the cursor starts on it) in the middle; red's far in a corner
    maps = {
        'open': make_map(geo, units=[unit('infantry', 'blue', 4, 3), unit('infantry', 'red', 8, 7)]),
        # the HQ (7 hexagons) of blue, a one-hexagon city of red; units out of the way
        'hq': make_map(geo, units=[unit('infantry', 'blue', 7, 1), unit('infantry', 'red', 8, 7)],
                       structures=[{'type': 'hq', 'x': 3, 'y': 3, 'side': 'blue'},
                                   {'type': 'city', 'x': 6, 'y': 5, 'side': 'red'}]),
        # an enemy three cells away to attack
        'fight': make_map(geo, units=[unit('infantry', 'blue', 2, 3), unit('infantry', 'red', 5, 3)]),
        'fog': make_map(geo, units=[unit('infantry', 'blue', 2, 3), unit('infantry', 'red', 8, 7)], fog=True),
    }
    big = Geo(layout, BIG_W, BIG_H)  # larger than the viewport: the camera scrolls
    maps['big'] = make_map(big, units=[unit('infantry', 'blue', BIG_W - 1, BIG_H - 1), unit('infantry', 'red', 0, 0)])
    write_game(out, 'hex_' + layout, layout, maps)
    return geo


# ---- art games: a sheet written here, cell by cell

JOIN_BLOCK = (0, 0)  # the 8 x 8 block of road pieces at the sheet's top-left


def piece_colour(n):
    return (20 + n * 3, 250 - n * 3, 60 + (n % 8) * 20)


PLAIN_ART = (200, 40, 200)
FOREST_ART = (240, 200, 40)
CITY_ART = (250, 120, 30)
HQ_ART = (30, 220, 220)


def art_game(out, layout):
    geo = Geo(layout, MAP_W, MAP_H)
    bw, bh = geo.box_size()
    big_w, big_h = (3 * geo.T, 10 * geo.q) if geo.pointy else (10 * geo.q, 3 * geo.T)

    def tiles_pixel(x, y):
        col, row = x // bw, y // bh
        lx, ly = x % bw, y % bh
        if col < 8 and row < 8:  # the join block: one opaque colour per piece
            return piece_colour(row * 8 + col) + (255,)
        if (col, row) == (8, 0):  # plain: opaque, the top-left 3 x 3 transparent
            return (0, 0, 0, 0) if lx < 3 and ly < 3 else PLAIN_ART + (255,)
        if (col, row) == (9, 0):  # forest: opaque, the top-right 3 x 3 transparent
            return (0, 0, 0, 0) if lx >= bw - 3 and ly < 3 else FOREST_ART + (255,)
        if (col, row) == (8, 1):  # the city's picture: the whole box
            return CITY_ART + (255,)
        return (0, 0, 0, 0)

    sheet_w, sheet_h = bw * 10, bh * 8
    files = {'art/tiles.png': png_bytes(sheet_w, sheet_h, tiles_pixel),
             'art/hq.png': png_bytes(big_w, big_h, lambda x, y: HQ_ART + (255,))}
    graphics = {
        'sheets': [{'id': 'tiles', 'file': 'art/tiles.png', 'cellWidth': bw, 'cellHeight': bh},
                   {'id': 'hq', 'file': 'art/hq.png', 'cellWidth': big_w, 'cellHeight': big_h}],
        'unitMarker': True,
        'terrain': {'plain': {'sheet': 'tiles', 'cell': [8, 0]}, 'forest': {'sheet': 'tiles', 'cell': [9, 0]}},
        'structures': {'city': {'sheet': 'tiles', 'cell': [8, 1]}, 'hq': {'sheet': 'hq', 'cell': [0, 0]}}}
    # roads join with themselves: the road terrain is the 'sea' slot of the plain game, art only for it
    graphics['terrain']['sea'] = {'sheet': 'tiles', 'join': list(JOIN_BLOCK), 'joinEdge': False}
    # Roads (the 'sea' slot) in a scattered pattern with a few straight runs; the top row has plain and forest only.
    road_cells = {(x, y) for (x, y) in geo.cells() if y > 0 and ((x * 7 + y * 3) % 5 in (0, 1) or (x in (2, 3) and y > 2))}
    tiles = {c: ('c' if c in road_cells else ('b' if c[1] == 0 and c[0] % 4 == 0 else 'a')) for c in geo.cells()}
    m = make_map(geo, units=[unit('infantry', 'blue', 8, 2), unit('infantry', 'red', 8, 7)],
                 structures=[{'type': 'hq', 'x': 5, 'y': 3, 'side': 'blue'}, {'type': 'city', 'x': 1, 'y': 5, 'side': 'red'}])
    m['tiles'] = [''.join(tiles[(x, y)] for x in range(geo.w)) for y in range(geo.h)]
    write_game(out, 'hexart_' + layout, layout, {'art': m}, art={'graphics': graphics, 'files': files})
    return geo, road_cells, m


# ---- running the player

def play(game, map_id, args=(), saves=None, frames=None, interval=4, inp=None, shot=None, dt=0.016):
    frames = frames or frames_for(inp or 'wait:1', interval)
    cmd = [PLAYER, '--games', GAMES, '--assets', os.path.join(REPO, 'assets'), '--headless', '--no-audio',
           '--no-settings', '--frames', str(frames), '--input-interval', str(interval), '--fixed-dt', str(dt),
           '--game', game, '--map', map_id]
    cmd += ['--saves', saves] if saves else ['--no-saving']
    if inp:
        cmd += ['--input', inp]
    if shot:
        cmd += ['--screenshot', shot]
    p = run_text(cmd + list(args))
    return p.returncode, p.stdout


class Image:
    def __init__(self, path):
        data = open(path, 'rb').read()
        offset = struct.unpack_from('<I', data, 10)[0]
        self.w, h = struct.unpack_from('<ii', data, 18)
        bpp = struct.unpack_from('<H', data, 28)[0] // 8
        self.h = abs(h)
        stride = (self.w * bpp + 3) & ~3
        rows = [data[offset + r * stride: offset + r * stride + self.w * bpp] for r in range(self.h)]
        if h > 0:
            rows.reverse()
        self.rows, self.bpp = rows, bpp

    def px(self, x, y):
        if not (0 <= x < self.w and 0 <= y < self.h):
            return None
        r = self.rows[y]
        i = x * self.bpp
        return (r[i + 2], r[i + 1], r[i])


def near(a, b, tol=30):
    return a is not None and max(abs(a[0] - b[0]), abs(a[1] - b[1]), abs(a[2] - b[2])) <= tol


def mix(base, tint, alpha):
    return tuple(int(round(base[i] * (1 - alpha) + tint[i] * alpha)) for i in range(3))


def shot(name, game, map_id, **kw):
    path = os.path.join(TMP, name + '.bmp')
    _, log = play(game, map_id, shot=path, **kw)
    if SHOTS:
        shutil.copy(path, os.path.join(SHOTS, name + '.bmp'))
    return Image(path), log


def fresh(name):
    p = os.path.join(TMP, name)
    shutil.rmtree(p, ignore_errors=True)
    return p


def unit_cells(save):
    """[(side index, x, y, hp)] of the units in a Continue save."""
    out = []
    for u in save['battle']['units']:
        if not u.get('alive', True):
            continue
        cell = u['cell']
        out.append((u['side'], cell % MAP_W, cell // MAP_W, u['hp']))
    return out


# ---- scenarios

IDLE = 'wait:14'  # the turn banner plays first (38 frames); it takes the first inputs otherwise


def frames_for(inp, interval=4, slack=40):
    """Frames a script needs: every token takes `interval` frames, wait:N takes N intervals."""
    n = 0
    for tok in inp.split(','):
        n += int(tok[5:]) * interval if tok.startswith('wait:') else interval
    return n + slack


def terrain_rgb(geo, x, y):
    return TERRAIN['abc'[geo.colour_index(x, y)]][1]


def voronoi_owner(geo, sx, sy):
    cx, cy = geo.cam()
    return geo.nearest(sx + cx + 0.5, sy + cy + 0.5)


def draws_hexagons(layout, geo):
    img, log = shot('%s_open' % layout, 'hex_' + layout, 'open', inp=IDLE)
    occupied = {(4, 3), (8, 7)}
    busy = set()
    for c in geo.cells():
        if any(geo.distance(c, o) <= 1 for o in occupied):
            busy.add(c)
    bad_mid = bad_corner = bad_side = checked = 0
    for (x, y) in geo.cells():
        if (x, y) in busy:
            continue
        sx, sy = geo.screen_middle(x, y)
        want = terrain_rgb(geo, x, y)
        checked += 1
        if not near(img.px(sx - 4, sy - 5), want, 8):
            bad_mid += 1
        bx, by = geo.screen_box(x, y)
        bw, bh = geo.box_size()
        # the box's corners are outside the hexagon: never this cell's own colour (a neighbour's or the background)
        for cx, cy in ((bx, by), (bx + bw - 1, by), (bx, by + bh - 1), (bx + bw - 1, by + bh - 1)):
            if near(img.px(cx, cy), want, 6):
                bad_corner += 1
        # the hexagon touches its box: the middle of the side that is upright (pointy: left/right; flat: top/bottom)
        # is inside (the faint outline darkens it a little)
        sides = ((bx, sy), (bx + bw - 1, sy)) if geo.pointy else ((sx, by), (sx, by + bh - 1))
        for px, py in sides:
            if not near(img.px(px, py), want, 24):
                bad_side += 1
    check('%s: %d cell middles show their terrain' % (layout, checked), bad_mid == 0, 'wrong: %d' % bad_mid)
    check('%s: the corners of every cell box show a neighbour or the background, never the cell itself' % layout,
          bad_corner == 0, 'wrong: %d' % bad_corner)
    check('%s: the hexagon touches its box on the upright (or flat) sides' % layout, bad_side == 0, 'wrong: %d' % bad_side)
    # every pixel inside the map's rectangle is terrain (no gaps between hexagons): count background pixels there
    mw, mh = geo.map_size()
    ox, oy = geo.screen_box(0, 0)
    gaps = 0
    for y in range(oy + geo.L, oy + mh - geo.L):
        for x in range(ox + geo.L, ox + mw - geo.L):
            if near(img.px(x, y), BACKGROUND, 4):
                gaps += 1
    check('%s: no gaps between the hexagons' % layout, gaps == 0, 'background pixels inside the map: %d' % gaps)


def cursor_score(img, geo, cell, skip=()):
    """Count of bright (cursor-coloured) pixels owned by a cell."""
    sx, sy = geo.screen_box(*cell)
    bw, bh = geo.box_size()
    n = 0
    for y in range(sy, sy + bh):
        for x in range(sx, sx + bw):
            p = img.px(x, y)
            if p and p[0] > 215 and p[1] > 200 and p[2] > 110 and voronoi_owner(geo, x, y) == cell:
                n += 1
    return n


def cursor_moves(layout, geo):
    # from the blue infantry at (4, 3): right, right, down, left, up, up -> by column and row, as on squares
    x, y = 4, 3
    steps = [('right', 1, 0), ('right', 1, 0), ('down', 0, 1), ('left', -1, 0), ('up', 0, -1), ('up', 0, -1)]
    inp = IDLE
    for i, (key, dx, dy) in enumerate(steps):
        x, y = x + dx, y + dy
        inp += ',' + key
        img, _ = shot('%s_cursor%d' % (layout, i), 'hex_' + layout, 'open', inp=inp + ',wait:2')
        scores = {c: cursor_score(img, geo, c) for c in geo.cells() if abs(c[0] - x) <= 2 and abs(c[1] - y) <= 2}
        best = max(scores, key=lambda c: scores[c])
        others = max(v for c, v in scores.items() if c != (x, y))
        check('%s: after "%s" the cursor outlines hexagon %d,%d' % (layout, key, x, y),
              best == (x, y) and scores[(x, y)] >= 20 and others * 3 <= scores[(x, y)], 'scores %s, wanted %s; strongest other %d' % (scores[(x, y)], (x, y), others))
    return


def ranges(layout, geo):
    """Select the infantry (move 3, range 1): blue where it can go, red on the ring around, nothing elsewhere."""
    img, _ = shot('%s_range' % layout, 'hex_' + layout, 'open', inp=IDLE + ',confirm,wait:3')
    unit_at, enemy = (4, 3), (8, 7)
    bad = []
    n_reach = n_att = 0
    for c in geo.cells():
        if c in (unit_at, enemy) or geo.distance(c, unit_at) == 0:
            continue
        d = geo.distance(c, unit_at)
        base = terrain_rgb(geo, *c)
        want = base
        if d <= 3:
            want = mix(base, (50, 110, 255), 105 / 255.0)
            n_reach += 1
        elif d == 4:
            want = mix(base, (255, 50, 50), 105 / 255.0)
            n_att += 1
        sx, sy = geo.screen_middle(*c)
        if not near(img.px(sx - 4, sy - 5), want, 6):
            bad.append((c, d))
    check('%s: move range (%d cells) is blue and the ring of 4 (%d cells) red, as the rules give' % (layout, n_reach, n_att),
          not bad, str(bad[:6]))
    # the tint is hexagon shaped: the corner pixels of a box inside the range show the neighbour's tint, not this cell's
    wrong = 0
    for c in geo.cells():
        if geo.distance(c, unit_at) != 3:  # the outer edge of the range: neighbours at 4 have the red tint
            continue
        bx, by = geo.screen_box(*c)
        bw, bh = geo.box_size()
        for cx, cy in ((bx, by), (bx + bw - 1, by + bh - 1)):
            owner = voronoi_owner(geo, cx, cy)
            if owner is None or owner == unit_at:
                continue
            d = geo.distance(owner, unit_at)
            blue_tint = mix(terrain_rgb(geo, *owner), (50, 110, 255), 105 / 255.0)
            if near(img.px(cx, cy), blue_tint, 6) != (d <= 3):
                wrong += 1
    check('%s: the tint stops at the hexagon edge (box corners follow the neighbour)' % layout, wrong == 0, 'wrong %d' % wrong)


def picking(layout, geo):
    # tap the infantry's hexagon, then a reachable one 3 cells away (a tap just inside its edge), then Wait
    unit_at = (4, 3)
    target = (6, 4)
    assert geo.distance(unit_at, target) <= 3
    tx, ty = geo.screen_middle(*target)
    # a point near the shared edge with the neighbour to the right/lower-right, but on the target's side
    for dir_i in (0, 1, 2, 3, 4, 5):
        nb = geo.neighbors(*target)[dir_i]
        if nb is None or nb == unit_at:
            continue
        nx, ny = geo.screen_middle(*nb)
        px, py = tx + (nx - tx) * 0.40, ty + (ny - ty) * 0.40  # 40% of the way to the neighbour: still the target's
        ux, uy = geo.screen_middle(*unit_at)
        sv = fresh('%s_pick_%d' % (layout, dir_i))
        inp = '%s,tap:%d:%d,wait:3,tap:%d:%d,wait:12,confirm,wait:40' % (IDLE, ux, uy, round(px), round(py))
        play('hex_' + layout, 'open', saves=sv, inp=inp)
        s = save_of(sv, 'hex_' + layout)
        cells = sorted(unit_cells(s)) if s else None
        check('%s: a tap near the edge towards neighbour %d picks the nearer hexagon and the unit moves there' % (layout, dir_i),
              cells is not None and (0, target[0], target[1], 100) in cells, str(cells))


def hq_frame(layout, geo, name, hq=(3, 3), fill=GOLD, owner=BLUE):
    """The HQ (rings: 1) of the 'hq' map: seven hexagons in its colour, framed on the outer edges only."""
    img, _ = shot(name, 'hex_' + layout, 'hq', inp=IDLE)
    ring = [hq] + [n for n in geo.neighbors(*hq)]
    bad_fill = 0
    for i, c in enumerate(ring):
        mx, my = geo.screen_middle(*c)
        if not near(img.px(mx - 8, my), fill, 14):
            bad_fill += 1
    check('%s: all seven hexagons of the HQ are drawn in its colour' % name, bad_fill == 0, 'wrong: %d' % bad_fill)
    # frame: the outer side of each outer hexagon has the owner's colour; the seam between the middle hexagon and a
    # neighbour does not. Sample the middle of each hexagon's edge towards direction d (inside the hexagon by 1 px).
    outer_bad = inner_bad = 0
    mx, my = geo.screen_middle(*hq)
    for i, c in enumerate(ring[1:]):
        cx, cy = geo.screen_middle(*c)
        # the edge of hexagon c that points away from the middle: the direction from the HQ middle to c, then past c
        dx, dy = cx - mx, cy - my
        norm = math.hypot(dx, dy)
        half = geo.T / 2.0 - 1.0  # the inradius is T / 2: this is just inside the edge, in the 2 pixel frame band
        ex, ey = cx + dx / norm * half, cy + dy / norm * half
        if not any(near(img.px(round(ex) + ox, round(ey) + oy), owner, 12) for ox in (-1, 0, 1) for oy in (-1, 0, 1)):
            outer_bad += 1
        sx, sy = mx + dx / norm * half, my + dy / norm * half  # inside the middle hexagon, just short of the seam
        if any(near(img.px(round(sx) + ox, round(sy) + oy), owner, 12) for ox in (-1, 0, 1) for oy in (-1, 0, 1)):
            inner_bad += 1
    check('%s: the owner frame is on the outer edges of the seven hexagons' % name, outer_bad == 0, 'missing %d' % outer_bad)
    check('%s: no frame on the seams inside the group' % name, inner_bad == 0, 'frame on %d seams' % inner_bad)
    return img


def scrolling(layout):
    """A map bigger than the viewport: the camera keeps the cursor (the blue unit in the far corner) in view and stops at
    the real map size (width and height in pixels as the contract gives them)."""
    geo = Geo(layout, BIG_W, BIG_H)
    img, _ = shot('%s_big' % layout, 'hex_' + layout, 'big', inp=IDLE)
    mw, mh = geo.map_size()
    camx, camy = mw - VIEW_W, mh - VIEW_H  # the last position the camera can take
    bad = []
    n = 0
    for (x, y) in geo.cells():
        if x < BIG_W - 8 or y < BIG_H - 7 or (x, y) == (BIG_W - 1, BIG_H - 1):
            continue
        mx, my = geo.middle(x, y)
        sx, sy = mx - camx, my - camy
        if not (0 <= sx - 4 < VIEW_W and 0 <= sy - 5 < VIEW_H):
            continue
        n += 1
        if not near(img.px(sx - 4, sy - 5), terrain_rgb(geo, x, y), 8):
            bad.append((x, y))
    check('%s: the map is bigger than the screen, scrolled to its real end (%d cells checked)' % (layout, n), n > 30 and not bad, str(bad[:5]))
    # the last column's right edge is at the viewport's right edge (hexagon widest at the middle of its row)
    mx, my = geo.middle(BIG_W - 1, BIG_H - 3)  # (an odd row: pointy even rows end half a hexagon sooner)
    edge = img.px(VIEW_W - 1, my - camy)
    check('%s: the map ends exactly at the right edge of the viewport' % layout, near(edge, terrain_rgb(geo, BIG_W - 1, BIG_H - 3), 24), str(edge))


def move_and_attack(layout, geo):
    sv = fresh('%s_fight' % layout)
    # blue (2,3) walks right two cells to (4,3), next to red (5,3) (distance 1), and attacks it: menu Attack first.
    inp = IDLE + ',confirm,right,right,confirm,wait:10,confirm,wait:60,confirm,wait:90'
    play('hex_' + layout, 'fight', saves=sv, inp=inp)
    s = save_of(sv, 'hex_' + layout)
    cells = sorted(unit_cells(s)) if s else None
    blue = [c for c in cells if c[0] == 0] if cells else []
    red = [c for c in cells if c[0] == 1] if cells else []
    check('%s: the infantry walked to hexagon 4,3' % layout, blue and blue[0][1:3] == (4, 3), str(cells))
    check('%s: and attacked the enemy next to it (its hit points dropped)' % layout, red and red[0][3] < 100, str(cells))


def fog_hexagons(layout, geo):
    full, _ = shot('%s_fog_all' % layout, 'hex_' + layout, 'fog', args=['--show-all'], inp=IDLE)
    fog, log = shot('%s_fog' % layout, 'hex_' + layout, 'fog', inp=IDLE)
    unit_at = (2, 3)
    visible = {c for c in geo.cells() if geo.distance(c, unit_at) <= 2}

    def dimmed(a, b):  # the fog picture is clearly darker than the same state without fog
        return a is not None and b is not None and sum(b) - sum(a) > 60

    bad_mid = bad_corner = 0
    for c in geo.cells():
        if geo.distance(c, unit_at) == 0 or c == (8, 7):
            continue
        sx, sy = geo.screen_middle(*c)
        if dimmed(fog.px(sx - 4, sy - 5), full.px(sx - 4, sy - 5)) == (c in visible):
            bad_mid += 1
        bx, by = geo.screen_box(*c)
        bw, bh = geo.box_size()
        for cx, cy in ((bx, by), (bx + bw - 1, by), (bx, by + bh - 1), (bx + bw - 1, by + bh - 1)):
            owner = voronoi_owner(geo, cx, cy)
            if owner is None or owner == unit_at or geo.distance(owner, unit_at) == 0:
                continue
            if abs(cx - (bx + bw // 2)) < 3:  # skip pixels on a seam the rounding could flip
                continue
            if dimmed(fog.px(cx, cy), full.px(cx, cy)) == (owner in visible):
                bad_corner += 1
    check('%s: with fog the hexagons the blue infantry does not see are dimmed, the others are not' % layout,
          bad_mid == 0, 'wrong: %d' % bad_mid)
    check('%s: the veil is hexagon shaped (a box corner is dimmed as the neighbour it belongs to is)' % layout,
          bad_corner <= 4, 'wrong: %d' % bad_corner)


def art(layout):
    geo, roads, m = art_game(GAMES, layout)
    img, log = shot('%s_art' % layout, 'hexart_' + layout, 'art', inp=IDLE)
    bw, bh = geo.box_size()
    # art cells are drawn in the box as they are: the top-right pixel of a row-0 plain cell is opaque art outside
    # the hexagon (not cut), the top-left 3 x 3 is transparent (the background shows).
    bad_open = bad_clear = bad_forest = n = 0
    top = geo.screen_box(0, 0)[1]  # (in the flat layout odd columns hang lower: their corners are covered by neighbours)
    for x in range(geo.w):
        if m['tiles'][0][x] != 'a' or geo.screen_box(x, 0)[1] != top:
            continue
        bx, by = geo.screen_box(x, 0)
        n += 1
        if not near(img.px(bx + bw - 1, by), PLAIN_ART, 4):
            bad_open += 1
        if not near(img.px(bx, by), BACKGROUND, 4):
            bad_clear += 1
    for x in range(geo.w):
        if m['tiles'][0][x] != 'b' or geo.screen_box(x, 0)[1] != top:
            continue
        bx, by = geo.screen_box(x, 0)
        if not (near(img.px(bx + bw - 1, by), BACKGROUND, 4) and near(img.px(bx, by), FOREST_ART, 4)):
            bad_forest += 1
    check('%s art: terrain pictures fill the whole box, not cut to the hexagon (%d cells)' % (layout, n), n >= 2 and bad_open == 0, 'bad %d' % bad_open)
    check('%s art: transparent corners show the background' % layout, bad_clear == 0 and bad_forest == 0, '%d %d' % (bad_clear, bad_forest))
    # join: every road hexagon shows the piece of its six neighbours (bit i = neighbour i is road)
    covered = set()
    for s in m['structures']:
        covered.add((s['x'], s['y']))
        if s['type'] == 'hq':
            covered.update(n for n in geo.neighbors(s['x'], s['y']) if n)
    covered.update((u['x'], u['y']) for u in m['units'])
    bad_join, seen = [], set()
    for (x, y) in sorted(roads):
        if (x, y) in covered or any(geo.distance((x, y), c) <= 1 for c in covered):
            continue
        mask = 0
        for i, nb in enumerate(geo.neighbors(x, y)):
            if nb is not None and nb in roads:
                mask |= 1 << i
        seen.add(mask)
        sx, sy = geo.screen_middle(x, y)
        if not near(img.px(sx, sy), piece_colour(mask), 3):
            bad_join.append(((x, y), mask, img.px(sx, sy)))
    check('%s art: %d road hexagons (%d different masks) show the right piece of the 8 x 8 block' % (layout, len(roads), len(seen)),
          not bad_join and len(seen) >= 6, str(bad_join[:4]) + ' masks %s' % sorted(seen))
    # the ring structure's picture: across the box of all seven hexagons, bottom edge on the box's bottom
    hq = (5, 3)
    if geo.pointy:
        mbx, mby = geo.screen_box(*hq)
        box = (mbx - geo.T, mby - 3 * geo.q, 3 * geo.T, 10 * geo.q)
    else:
        mbx, mby = geo.screen_box(*hq)
        box = (mbx - 3 * geo.q, mby - geo.T, 10 * geo.q, 3 * geo.T)
    bx, by, w, h = box
    inside = [(bx + 2, by + 2), (bx + w - 3, by + 2), (bx + 2, by + h - 3), (bx + w - 3, by + h - 3), (bx + w // 2, by + h - 1)]
    outside = [(bx + w // 2, by + h), (bx + w // 2, by - 1), (bx - 1, by + h // 2), (bx + w, by + h // 2)]
    check('%s art: the ring structure\'s picture covers its box (corners and bottom row)' % layout,
          all(near(img.px(*p), HQ_ART, 4) or near(img.px(*p), BLUE, 12) for p in inside), str([img.px(*p) for p in inside]))
    check('%s art: and nothing of it is outside the box' % layout, not any(near(img.px(*p), HQ_ART, 4) for p in outside),
          str([img.px(*p) for p in outside]))
    # the one-hexagon city's picture in its box
    cbx, cby = geo.screen_box(1, 5)
    check('%s art: a one-hexagon structure is drawn in its hexagon\'s box' % layout,
          near(img.px(cbx + bw // 2 - 6, cby + bh // 2), CITY_ART, 4) and near(img.px(cbx + bw - 2, cby + bh - 2), CITY_ART, 14),
          str((img.px(cbx + bw // 2 - 6, cby + bh // 2), img.px(cbx + bw - 2, cby + bh - 2))))


def main():
    global PLAYER, REPO, GAMES, SHOTS, TMP
    args = sys.argv[1:]
    if len(args) < 2:
        print(__doc__)
        return 2
    PLAYER, REPO = os.path.abspath(args[0]), os.path.abspath(args[1])
    if '--shots' in args:
        SHOTS = os.path.abspath(args[args.index('--shots') + 1])
        os.makedirs(SHOTS, exist_ok=True)
    TMP = tempfile.mkdtemp(prefix='ote_hex_test_')
    GAMES = os.path.join(TMP, 'games')
    try:
        for layout in ('pointy', 'flat'):
            geo = plain_game(GAMES, layout)
            draws_hexagons(layout, geo)
            cursor_moves(layout, geo)
            ranges(layout, geo)
            picking(layout, geo)
            hq_frame(layout, geo, layout + '_hq')
            move_and_attack(layout, geo)
            scrolling(layout)
            fog_hexagons(layout, geo)
            art(layout)
    finally:
        shutil.rmtree(TMP, ignore_errors=True)
    summary(blank_line=True)
    return 1 if failures else 0


TMP = ''

if __name__ == '__main__':
    sys.exit(main())
