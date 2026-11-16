#!/usr/bin/env python3
"""The player's part of docs/HEX.md "2D isometric" (hexagon maps in the isometric view), windowless.

  python3 tests/hex_iso_player_test.py ./build/engine/player/ote-player .
  python3 tests/hex_iso_player_test.py ./build/engine/player/ote-player . --shots OUT_DIR   # also keeps the screenshots (BMP)

Writes small hexagon games with an isometric style in a temp folder (both layouts), runs the player headless with --input
scripts and --saves on a temp folder, and judges pixels of the 480 x 270 screenshot and the Continue save:

  - every hexagon's top face is exactly the squashed hexagon of the contract (the rounding rule: tools/art/hex_iso_geom.py,
    checked here against the engine's real pixels), neighbours share their edges (no background between them);
  - heights lift a hexagon by heightStep per level, side faces show only under the edges whose neighbour in front is lower
    (and not under an edge facing an equal neighbour), a raised hexagon covers what is behind it;
  - picking: a tap near an edge picks the nearest hexagon; a tap on the lifted top face or on a side face picks the raised
    hexagon (the nearer one), not the ground behind it;
  - highlights (move range, attack ring) and the fog veil are hexagon shaped; the cursor outlines its hexagon;
  - structures of one hexagon and of seven (placeholder blocks with the owner's frame), units standing on raised ground;
  - art: a terrain cell is drawn with its top face on the box and its front faces below, taller cells rise above, a raised
    hexagon repeats the faces down to the lower neighbour, join pieces pick by the six neighbours, structure pictures stand
    on the front edge.
"""
import json
import math
import os
import shutil
import sys
import tempfile

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import hex_player_test as hp  # noqa: E402  (its game writers, runner and image reader)

TOOLS = os.path.join(HERE, '..', 'tools', 'art')
sys.path.insert(0, TOOLS)
from hex_iso_geom import IsoGeom  # noqa: E402

check = hp.check
near, mix, shot, fresh, save_of, play = hp.near, hp.mix, hp.shot, hp.fresh, hp.save_of, hp.play
IDLE = hp.IDLE
VIEW_W, VIEW_H = hp.VIEW_W, hp.VIEW_H
BACKGROUND = (0x16, 0x1a, 0x24)   # the style's "background"
BLUE, RED = hp.BLUE, hp.RED
STEP = 12
UNIT_W = 24
COLS, ROWS = 6, 5
WALL_K = {'pointy': {3: 0.70, 2: 0.52}, 'flat': {4: 0.70, 3: 0.61, 2: 0.52}}   # shades of a placeholder block's faces


class Iso:
    """Where things are on the screen (the contract, written again here with the shared integer rule)."""

    def __init__(self, layout, cols, rows, levels, step=STEP):
        self.layout, self.cols, self.rows, self.levels, self.step = layout, cols, rows, levels, step
        self.ig = IsoGeom(layout, 24, None, step)
        self.headroom = max(self.ig.h, self.ig.tile_width, 2 * UNIT_W)
        self.geo = hp.Geo(layout, cols, rows)   # neighbours / distance / cells only

    def at(self, levels):
        """The same layout for a map whose highest hexagon is `levels` above its lowest."""
        return Iso(self.layout, self.cols, self.rows, levels, self.step)

    def world(self):
        mw, mh = self.ig.map_size(self.cols, self.rows)
        return mw, mh + self.levels * self.step + self.step + self.headroom

    def cam(self):
        mw, mh = self.world()
        fx = -(VIEW_W - mw) / 2.0 if mw <= VIEW_W else 0.0
        fy = -(VIEW_H - mh) / 2.0 if mh <= VIEW_H else 0.0
        return math.floor(fx + 0.5), math.floor(fy + 0.5)

    def top_box(self, x, y, e=0):
        bx, by = self.ig.box_origin(x, y)
        cx, cy = self.cam()
        return bx - cx, by + (self.levels - e) * self.step + self.headroom - cy

    def middle(self, x, y, e=0):
        bx, by = self.top_box(x, y, e)
        return bx + self.ig.w // 2, by + self.ig.h // 2

    def face_pixels(self, x, y, e=0, rim=True):
        """Every screen pixel of the hexagon's top face; rim=False leaves out the lowest pixel of each column (the
        placeholder block draws it a shade darker)."""
        bx, by = self.top_box(x, y, e)
        for row in range(self.ig.h):
            l, r = self.ig.row_span(row)
            for px in range(l, r):
                if rim or row != self.ig.last_row(px):
                    yield bx + px, by + row


def write_iso_game(out, gid, layout, maps, art=None, big=False):
    g = hp.write_game(out, gid, layout, maps, art=art)
    gj = os.path.join(g, 'game.json')
    doc = json.load(open(gj))
    doc['styles'] = [{'id': 'classic', 'name': 'Classic'},
                     {'id': 'iso', 'name': 'Isometric', 'view': 'isometric', 'background': '#161a24'}]
    json.dump(doc, open(gj, 'w'), indent=1)
    return g


def make_iso_map(geo, heights=None, **kw):
    m = hp.make_map(geo, **kw)
    if heights is not None:
        rows = [[0] * geo.w for _ in range(geo.h)]
        for (x, y), v in heights.items():
            rows[y][x] = v
        m['heights'] = rows
    return m


HILL = (2, 2)
PLATEAU = [(4, 3), (5, 3)]
HEIGHTS = {HILL: 3, PLATEAU[0]: 1, PLATEAU[1]: 1}
LEVELS = 3
BLUE_UNIT, RED_UNIT = (4, 1), (0, 4)


def make_games(out, layout):
    geo = hp.Geo(layout, COLS, ROWS)
    maps = {
        'flat': make_iso_map(geo, units=[hp.unit('infantry', 'blue', 1, 2), hp.unit('infantry', 'red', 5, 4)]),
        'hills': make_iso_map(geo, heights=HEIGHTS, units=[hp.unit('infantry', 'blue', 3, 4), hp.unit('infantry', 'red', 0, 4)]),
        'onplateau': make_iso_map(geo, heights=HEIGHTS, units=[hp.unit('infantry', 'blue', 4, 3), hp.unit('infantry', 'red', 0, 4)]),
        'fog': make_iso_map(geo, units=[hp.unit('infantry', 'blue', 1, 2), hp.unit('infantry', 'red', 5, 4)], fog=True),
        'structures': make_iso_map(
            geo, heights={(5, 3): 1}, units=[hp.unit('infantry', 'blue', 0, 4), hp.unit('infantry', 'red', 5, 0)],
            structures=[{'type': 'hq', 'x': 2, 'y': 2, 'side': 'blue'}, {'type': 'city', 'x': 5, 'y': 3, 'side': 'red'}]),
    }
    write_iso_game(out, 'iso_' + layout, layout, maps)
    return Iso(layout, COLS, ROWS, 0)


def iso_shot(name, layout, map_id, **kw):
    return shot(name, 'iso_' + layout, map_id, args=['--style', 'iso'] + kw.pop('args', []), **kw)


def busy_cells(iso, units, radius=2):
    return {c for c in iso.geo.cells() if any(iso.geo.distance(c, u) <= radius for u in units)}


# ---- scenarios

def flat_map(layout, iso):
    iso = iso.at(0)
    """Level 0: every top face is its terrain colour on exactly the squashed hexagon; no gap between neighbours."""
    img, log = iso_shot('%s_flat' % layout, layout, 'flat', inp=IDLE)
    check('%s: the player did not complain about the isometric hexagon style' % layout, 'warn' not in log.lower() and 'error' not in log.lower(), log[-300:])
    busy = {(1, 2), (5, 4)}   # a unit's picture stays inside its own hexagon
    wrong = checked = 0
    for (x, y) in iso.geo.cells():
        if (x, y) in busy:
            continue
        want = hp.TERRAIN['abc'[iso.geo.colour_index(x, y)]][1]
        checked += 1
        wrong += sum(1 for px, py in iso.face_pixels(x, y, rim=False) if not near(img.px(px, py), want, 3))
    check('%s: %d top faces are exactly the squashed hexagon (%d wrong pixels)' % (layout, checked, wrong), checked > 12 and wrong == 0, 'wrong %d' % wrong)
    # no background inside the map's body (between the hexagons)
    ig = iso.ig
    x0, y0 = iso.top_box(0, 0)
    mw, mh = ig.map_size(COLS, ROWS)
    gaps = sum(1 for y in range(y0 + ig.h, y0 + mh - ig.h) for x in range(x0 + ig.w, x0 + mw - ig.w) if near(img.px(x, y), BACKGROUND, 3))
    check('%s: no gap between the hexagons' % layout, gaps == 0, 'background pixels %d' % gaps)
    # the rim of the map shows the side faces under the lowest hexagons: lit left, shaded right
    bottom_row = [(x, ROWS - 1) for x in range(COLS)]
    bad = n = 0
    near_units = busy_cells(iso, [(1, 2), (5, 4)], 1)
    for (x, y) in bottom_row:
        if (x, y) in near_units:
            continue
        want = hp.TERRAIN['abc'[iso.geo.colour_index(x, y)]][1]
        bx, by = iso.top_box(x, y)
        faces = WALL_K[layout]
        # pointy: a column under each lower edge; flat: the middle of the flat bottom edge (the slanted ones meet the
        # half-lower neighbours' top faces)
        for col in ((iso.ig.w // 4, iso.ig.w * 3 // 4) if layout == 'pointy' else (iso.ig.w // 2,)):
            last = iso.ig.last_row(col)
            k = faces[iso.ig.face_dir(col)]
            n += 1
            if not near(img.px(bx + col, by + last + 4), tuple(int(c * k) for c in want), 8):
                bad += 1
    check('%s: the map\'s rim shows the side faces (%d samples) in the block\'s shades' % (layout, n), n > 3 and bad == 0, 'wrong %d' % bad)
    return img


def hills(layout, iso):
    iso = iso.at(LEVELS)
    img, _ = iso_shot('%s_hills' % layout, layout, 'hills', inp=IDLE)
    ig = iso.ig
    colour = lambda c: hp.TERRAIN['abc'[iso.geo.colour_index(*c)]][1]
    busy = {(3, 4), (0, 4)}
    # 1. the hill is lifted three steps and is its colour exactly on the lifted top face
    wrong = sum(1 for px, py in iso.face_pixels(*HILL, e=3, rim=False) if not near(img.px(px, py), colour(HILL), 3))
    check('%s: a 3-level hexagon is drawn 3 steps (%d px) higher, exactly its hexagon (%d wrong)' % (layout, 3 * STEP, wrong), wrong == 0)
    # 2. side faces under each visible lower edge whose neighbour in front is lower
    bad = n = 0
    ground_bx, ground_by = iso.top_box(*HILL, e=3)
    for d in sorted(WALL_K[layout]):
        nx, ny = ig.neighbor(*HILL, d)
        if not (0 <= nx < COLS and 0 <= ny < ROWS) or HEIGHTS.get((nx, ny), 0) >= 3 or (nx, ny) in busy:
            continue
        # the middle column of the face
        l, r = {'pointy': {3: (0, ig.w // 2), 2: (ig.w // 2, ig.w)}, 'flat': {4: (0, ig.q), 3: (ig.q, ig.w - ig.q), 2: (ig.w - ig.q, ig.w)}}[layout][d]
        col = (l + r) // 2
        last = ig.last_row(col)
        k = WALL_K[layout][d]
        want = tuple(int(c * k) for c in colour(HILL))
        for dy in (3, 8):   # two rows inside the first level's face and the next level's face
            n += 1
            if not near(img.px(ground_bx + col, ground_by + last + dy), want, 8):
                bad += 1
    check('%s: the hill shows side faces under its lower edges (%d samples)' % (layout, n), n >= 4 and bad == 0, 'wrong %d' % bad)
    # 3. beside an equal neighbour no face: the plateau's edge towards its equal neighbour is covered by that neighbour's top
    bad = n = 0
    for a in PLATEAU:
        for d in sorted(WALL_K[layout]):
            b = ig.neighbor(*a, d)
            if b not in PLATEAU:
                continue
            l, r = {'pointy': {3: (0, ig.w // 2), 2: (ig.w // 2, ig.w)}, 'flat': {4: (0, ig.q), 3: (ig.q, ig.w - ig.q), 2: (ig.w - ig.q, ig.w)}}[layout][d]
            col = (l + r) // 2
            bx, by = iso.top_box(*a, e=1)
            n += 1
            if not near(img.px(bx + col, by + ig.last_row(col) + 2), colour(b), 3):
                bad += 1
    check('%s: no side face between two hexagons of the same height (%d edges)' % (layout, n), bad == 0, 'wrong %d' % bad)
    # 4. a raised hexagon covers what is behind it: the middle of a ground hexagon the hill reaches over shows the hill
    covers = bad = 0
    for c in iso.geo.cells():
        if c == HILL or c in busy or HEIGHTS.get(c, 0):
            continue
        mx, my = iso.middle(*c)
        hx, hy = iso.top_box(*HILL, e=3)
        if ig.inside(mx - hx, my - hy):
            covers += 1
            if not near(img.px(mx, my), colour(HILL), 3):
                bad += 1
    check('%s: ground behind the hill is covered by it (%d spots)' % (layout, covers), covers >= 1 and bad == 0, 'wrong %d' % bad)
    return img


def picking(layout, iso):
    """Taps pick the nearest hexagon on the ground, and the raised hexagon where it stands over the ground behind it."""
    geo = iso.geo
    ground, iso = iso.at(0), iso.at(LEVELS)
    unit_at, target = (1, 2), (3, 2)
    assert geo.distance(unit_at, target) <= 3
    tx, ty = ground.middle(*target)
    ux, uy = ground.middle(*unit_at)
    for d in range(6):
        nb = geo.neighbors(*target)[d]
        if nb is None or nb == unit_at:
            continue
        nx, ny = ground.middle(*nb)
        for frac, want in ((0.40, target), (0.60, nb)):
            if want == nb and (geo.distance(unit_at, nb) > 3):
                continue
            px, py = tx + (nx - tx) * frac, ty + (ny - ty) * frac
            sv = fresh('%s_pick_%d_%d' % (layout, d, int(frac * 10)))
            inp = '%s,tap:%d:%d,wait:3,tap:%d:%d,wait:12,confirm,wait:40' % (IDLE, ux, uy, round(px), round(py))
            play('iso_' + layout, 'flat', args=['--style', 'iso'], saves=sv, inp=inp)
            s = save_of(sv, 'iso_' + layout)
            cells = sorted(hp.unit_cells(s)) if s else None
            ok = cells is not None and any(c[0] == 0 and c[1:3] == want for c in cells)
            check('%s: a tap %d%% of the way from %s towards neighbour %d picks %s' % (layout, frac * 100, target, d, want), ok, str(cells))
    # the hill (3 levels): a tap on its lifted top face and a tap low on its side face both pick it
    ig = iso.ig
    bx, by = iso.top_box(*HILL, e=3)
    spots = {'top': (bx + ig.w // 2, by + ig.h // 2)}
    d = 3
    l, r = (0, ig.w // 2) if layout == 'pointy' else (ig.q, ig.w - ig.q)   # the lower-left (pointy) / front (flat) face
    col = (l + r) // 2
    spots['side'] = (bx + col, by + ig.last_row(col) + 14)
    for name, (px, py) in spots.items():
        ux, uy = iso.middle(3, 4)
        sv = fresh('%s_hillpick_%s' % (layout, name))
        inp = '%s,tap:%d:%d,wait:3,tap:%d:%d,wait:12,confirm,wait:40' % (IDLE, ux, uy, px, py)
        play('iso_' + layout, 'hills', args=['--style', 'iso'], saves=sv, inp=inp)
        s = save_of(sv, 'iso_' + layout)
        cells = sorted(hp.unit_cells(s)) if s else None
        check('%s: a tap on the hill\'s %s picks the hill (the nearer hexagon)' % (layout, name),
              cells is not None and any(c[0] == 0 and c[1:3] == HILL for c in cells), str(cells))


def highlights(layout, iso):
    iso = iso.at(0)
    """Select the infantry (move 3, range 1): the tints cover exactly the hexagons the rules give."""
    img, _ = iso_shot('%s_range' % layout, layout, 'flat', inp=IDLE + ',confirm,wait:3')
    unit_at, enemy = (1, 2), (5, 4)
    geo = iso.geo
    bad = n_reach = n_att = checked = 0
    for c in geo.cells():
        if geo.distance(c, unit_at) <= 2 or geo.distance(c, enemy) <= 1:
            continue
        d = geo.distance(c, unit_at)
        base = hp.TERRAIN['abc'[geo.colour_index(*c)]][1]
        want = base
        if d <= 3:
            want, n_reach = mix(base, (50, 110, 255), 120 / 255.0), n_reach + 1
        elif d == 4:
            want, n_att = mix(base, (255, 50, 50), 120 / 255.0), n_att + 1
        checked += 1
        bad += sum(1 for px, py in iso.face_pixels(*c, rim=False) if not near(img.px(px, py), want, 5))
    check('%s: move range (%d hexagons) blue and the ring (%d) red exactly on the hexagons (%d wrong pixels)' % (layout, n_reach, n_att, bad),
          n_reach >= 2 and n_att >= 2 and bad == 0, 'wrong %d' % bad)


def cursor(layout, iso):
    iso = iso.at(0)
    geo = iso.geo
    x, y = 1, 2
    inp = IDLE
    for key, dx, dy in (('right', 1, 0), ('down', 0, 1), ('left', -1, 0)):
        x, y, inp = x + dx, y + dy, inp + ',' + key
        img, _ = iso_shot('%s_cursor_%s' % (layout, key), layout, 'flat', inp=inp + ',wait:2')
        scores = {}
        for c in geo.cells():
            if abs(c[0] - x) <= 2 and abs(c[1] - y) <= 2 and c != (1, 2):   # (the unit's own picture is not the cursor)
                base = hp.TERRAIN['abc'[geo.colour_index(*c)]][1]
                scores[c] = sum(1 for px, py in iso.face_pixels(*c, rim=False) if not near(img.px(px, py), base, 40))
        best = max(scores, key=lambda c: scores[c])
        check('%s: after "%s" the cursor outlines hexagon %d,%d' % (layout, key, x, y), best == (x, y) and scores[(x, y)] >= 20, str(sorted(scores.items(), key=lambda kv: -kv[1])[:3]))


def fog(layout, iso):
    iso = iso.at(0)
    full, _ = iso_shot('%s_fog_all' % layout, layout, 'fog', args=['--show-all'], inp=IDLE)
    veiled, _ = iso_shot('%s_fog' % layout, layout, 'fog', inp=IDLE)
    unit_at, enemy = (1, 2), (5, 4)
    visible = {c for c in iso.geo.cells() if iso.geo.distance(c, unit_at) <= 2}
    bad = n = 0
    for c in iso.geo.cells():
        if c == unit_at or c == enemy or iso.geo.distance(c, unit_at) == 1 or iso.geo.distance(c, enemy) <= 1:
            continue
        n += 1
        for px, py in list(iso.face_pixels(*c, rim=False))[::7]:
            a, b = veiled.px(px, py), full.px(px, py)
            dimmed = sum(b) - sum(a) > 60
            if dimmed == (c in visible):
                bad += 1
    check('%s: with fog the hexagons out of sight are dimmed whole and exactly their shape (%d hexagons, %d wrong samples)' % (layout, n, bad),
          n > 10 and bad == 0, 'wrong %d' % bad)


def structures(layout, iso):
    iso = iso.at(1)
    img, _ = iso_shot('%s_structures' % layout, layout, 'structures', inp=IDLE)
    geo, ig = iso.geo, iso.ig
    wall = ig.h * 3 // 4
    hq = (2, 2)
    ring = [hq] + [n for n in geo.neighbors(*hq)]
    bad = 0
    for c in ring:
        mx, my = iso.middle(*c)
        px, py = mx - 8, my - wall + 2   # the letter sits on the middle one: sample left of it
        if not near(img.px(px, py), hp.GOLD, 14):
            bad += 1
    check('%s: the seven hexagons of the HQ are blocks %d px high in its colour' % (layout, wall), bad == 0, 'wrong %d' % bad)
    # the owner's frame along the lower outer edges (at the ground, drawn over the block's foot)
    frame_bad = n = 0
    for d in sorted(WALL_K[layout]):
        outer = geo.neighbors(*hq)[d]
        # direction d of the HQ's middle: the outer hexagon there has an uncovered neighbour in the same direction
        if outer is None:
            continue
        bx, by = iso.top_box(*outer)
        l, r = {'pointy': {3: (0, ig.w // 2), 2: (ig.w // 2, ig.w)}, 'flat': {4: (0, ig.q), 3: (ig.q, ig.w - ig.q), 2: (ig.w - ig.q, ig.w)}}[layout][d]
        col = (l + r) // 2
        n += 1
        if not any(near(img.px(bx + col + ox, by + ig.last_row(col) + oy), BLUE, 12) for ox in (-1, 0, 1) for oy in (-1, 0, 1)):
            frame_bad += 1
            print('   frame missing under direction', d, 'of the outer hexagon', outer, [img.px(bx + col, by + ig.last_row(col) + oy) for oy in (-3, -2, -1, 0, 1)])
    check('%s: the owner\'s frame runs along the HQ\'s lower outer edges (%d edges)' % (layout, n), n >= 2 and frame_bad == 0, 'missing %d' % frame_bad)
    # the city on a one-level hexagon: block top lifted by 1 step and the block's height
    mx, my = iso.middle(5, 3, e=1)
    city_top = (183, 183, 183)
    check('%s: a one-hexagon structure on raised ground stands on its top (%d + %d px up)' % (layout, STEP, wall),
          near(img.px(mx - 8, my - wall + 2), city_top, 14), str(img.px(mx - 8, my - wall + 2)))
    return img


def units_on_height(layout, iso):
    iso = iso.at(LEVELS)
    img, _ = iso_shot('%s_onplateau' % layout, layout, 'onplateau', inp=IDLE)
    cx, cy = iso.middle(4, 3, e=1)
    foot = cy + iso.ig.h // 4
    p = img.px(cx - 7, foot - 9)
    check('%s: the unit stands on the plateau (its block %d px above the ground middle)' % (layout, STEP),
          near(p, BLUE, 40), str(p))
    # and the same unit would not be there on the ground
    gx, gy = iso.middle(4, 3, e=0)
    q = img.px(gx - 7, gy + iso.ig.h // 4 - 9)
    check('%s: nothing of it at ground height' % layout, not near(q, BLUE, 12), str(q))


# ---- art

PLAIN_TOP, PLAIN_FACE, TREE = (200, 40, 200), (40, 60, 200), (255, 255, 0)
CITY_PIC, HQ_PIC = (250, 120, 30), (30, 220, 220)


def piece_colour(n):
    return hp.piece_colour(n)


def art_game(out, layout):
    ig = IsoGeom(layout, 24, None, STEP)
    cw, ch = ig.cell
    rise = ig.rise

    def tiles_pixel(x, y):
        col, row = x // cw, y // ch
        lx, ly = x % cw, y % ch
        if col < 8 and row < 8:   # the join block: the top face in the piece's colour, side faces a fixed dark
            ty = ly - rise
            if 0 <= ty < ig.h and ig.inside(lx, ty):
                return piece_colour(row * 8 + col) + (255,)
            last = ig.last_row(lx)
            if last >= 0 and rise + last < ly <= rise + last + ig.step:
                return (20, 20, 20, 255)
            return (0, 0, 0, 0)
        if (col, row) in ((8, 0), (9, 0)):   # plain / forest: top face, side faces; forest also a 'tree' stub above
            ty = ly - rise
            if 0 <= ty < ig.h and ig.inside(lx, ty):
                return PLAIN_TOP + (255,)
            last = ig.last_row(lx)
            if last >= 0 and rise + last < ly <= rise + last + ig.step:
                return PLAIN_FACE + (255,)
            if col == 9 and ly < rise and abs(lx - cw // 2) < 4 and ly >= rise - 8:
                return TREE + (255,)
            return (0, 0, 0, 0)
        return (0, 0, 0, 0)
    sw, sh = ig.single_size()
    rw, rh = ig.ring_size()
    files = {'art/iso_tiles.png': hp.png_bytes(cw * 10, ch * 8, tiles_pixel),
             'art/iso_city.png': hp.png_bytes(sw, sh, lambda x, y: CITY_PIC + (255,)),
             'art/iso_hq.png': hp.png_bytes(rw, rh, lambda x, y: HQ_PIC + (255,))}
    graphics = {
        'sheets': [{'id': 'iso_tiles', 'file': 'art/iso_tiles.png', 'cellWidth': cw, 'cellHeight': ch},
                   {'id': 'iso_city', 'file': 'art/iso_city.png', 'cellWidth': sw, 'cellHeight': sh},
                   {'id': 'iso_hq', 'file': 'art/iso_hq.png', 'cellWidth': rw, 'cellHeight': rh}],
        'styles': {'iso': {
            'terrain': {'plain': {'sheet': 'iso_tiles', 'cell': [8, 0]}, 'forest': {'sheet': 'iso_tiles', 'cell': [9, 0]},
                        'sea': {'sheet': 'iso_tiles', 'join': [0, 0], 'joinEdge': False}},
            'structures': {'city': {'sheet': 'iso_city', 'cell': [0, 0]}, 'hq': {'sheet': 'iso_hq', 'cell': [0, 0]}}}}}
    geo = hp.Geo(layout, COLS, ROWS)
    units = [hp.unit('infantry', 'blue', 5, 4), hp.unit('infantry', 'red', 0, 4)]
    roads = {(x, y) for (x, y) in geo.cells() if y > 0 and (x * 7 + y * 3) % 5 in (0, 1)}
    tiles = {c: ('c' if c in roads else 'a') for c in geo.cells()}
    tiles[(4, 0)] = 'b'   # the one forest hexagon (its tree stub rises above the top of the map)
    ground = hp.make_map(geo, units=units)
    ground['tiles'] = [''.join(tiles[(x, y)] for x in range(COLS)) for y in range(ROWS)]
    rows = [[0] * COLS for _ in range(ROWS)]
    rows[0][1] = 2   # a plain hexagon two levels up, with lower neighbours
    ground['heights'] = rows
    # the structures stand on the bottom rows so no hexagon in front covers a corner of their pictures
    built = hp.make_map(geo, units=[hp.unit('infantry', 'blue', 5, 0), hp.unit('infantry', 'red', 4, 0)],
                        structures=[{'type': 'hq', 'x': 3, 'y': 3, 'side': 'blue'}, {'type': 'city', 'x': 0, 'y': 4, 'side': 'red'}])
    built['tiles'] = [''.join('a' for _ in range(COLS)) for _ in range(ROWS)]
    write_iso_game(out, 'isoart_' + layout, layout, {'art': ground, 'artst': built}, art={'graphics': graphics, 'files': files})
    return roads


def art(layout):
    roads = art_game(hp.GAMES, layout)
    iso = Iso(layout, COLS, ROWS, 2)   # the 'art' map: its highest hexagon is two levels up
    geo, ig = iso.geo, iso.ig
    img, _ = shot('%s_art' % layout, 'isoart_' + layout, 'art', args=['--style', 'iso'], inp=IDLE)
    busy = busy_cells(iso, [(5, 4), (0, 4)], 1) | {(1, 0), (4, 0)} | {n for n in geo.neighbors(1, 0) if n}
    # 1. a plain cell: the art's top face exactly on the squashed hexagon, its tree stub above the box
    plains = [c for c in geo.cells() if c not in roads and c not in busy and c[1] >= 1]
    wrong = sum(1 for c in plains for px, py in iso.face_pixels(*c) if not near(img.px(px, py), PLAIN_TOP, 3))
    check('%s art: %d terrain pictures\' top faces are exactly the squashed hexagon of their box (%d wrong pixels)' % (layout, len(plains), wrong),
          len(plains) >= 2 and wrong == 0)
    bx, by = iso.top_box(4, 0)
    stub = img.px(bx + ig.w // 2, by - 4)
    check('%s art: a picture taller than the hexagon rises above its box' % layout, near(stub, TREE, 3), str(stub))
    # 2. the 2-level hexagon is lifted and repeats its side faces down to its lower neighbours
    hx, hy = iso.top_box(1, 0, e=2)
    wrong = sum(1 for px, py in iso.face_pixels(1, 0, e=2) if not near(img.px(px, py), PLAIN_TOP, 3))
    check('%s art: the raised hexagon\'s top face is on its lifted box (%d wrong)' % (layout, wrong), wrong == 0)
    col = ig.w // 4 if layout == 'pointy' else ig.w // 2
    last = ig.last_row(col)
    faces = [img.px(hx + col, hy + last + dy) for dy in range(1, 2 * STEP - 1)]
    check('%s art: its faces are repeated for each level down to the lower neighbour (%d rows of face)' % (layout, len(faces)),
          all(near(f, PLAIN_FACE, 3) for f in faces), str(faces))
    # 3. join pieces: a road hexagon shows the piece of its six neighbours
    bad = n = 0
    for c in sorted(roads):
        if c in busy:
            continue
        mask = 0
        for d, nb in enumerate(geo.neighbors(*c)):
            mask |= int(nb in roads) << d   # off the map does not count (joinEdge false)
        mx, my = iso.middle(*c)
        n += 1
        if not near(img.px(mx, my), piece_colour(mask), 3):
            bad += 1
    check('%s art: %d road hexagons show the right piece of the 8 x 8 block' % (layout, n), n >= 3 and bad == 0, 'wrong %d' % bad)
    # 4. structure pictures stand on the front edge of their hexagon (or of the seven)
    iso0 = iso.at(0)
    img, _ = shot('%s_artst' % layout, 'isoart_' + layout, 'artst', args=['--style', 'iso'], inp=IDLE)
    bx, by = iso0.top_box(0, 4)
    sw, sh = ig.single_size()
    bottom = by + ig.h
    # (a corner low on the right is covered by the neighbour in front, as it must be; the top corners are not)
    pts = (img.px(bx + 2, bottom - 3), img.px(bx + ig.w - 3, bottom - sh + 2), img.px(bx + ig.w // 2, bottom - sh + 1),
           img.px(bx + ig.w // 2, bottom - sh - 2))
    check('%s art: a one-hexagon structure\'s picture is as wide as the box, its bottom edge on the front edge' % layout,
          near(pts[0], CITY_PIC, 4) and near(pts[1], CITY_PIC, 4) and near(pts[2], CITY_PIC, 4) and not near(pts[3], CITY_PIC, 4), str(pts))
    mbx, mby = ig.box_origin(3, 3)
    camx, camy = iso0.cam()
    if layout == 'pointy':
        rbx, rby, rbw, rbh = mbx - ig.T, mby - 3 * ig.q, 3 * ig.T, 10 * ig.q
    else:
        rbx, rby, rbw, rbh = mbx - 3 * ig.q, mby - ig.T, 10 * ig.q, 3 * ig.T
    rw, rh = ig.ring_size()
    bottom = rby + rbh + iso0.headroom - camy
    left = rbx - camx
    pts = (img.px(left + rbw // 2, bottom - 6), img.px(left + 2, bottom - rh + 3), img.px(left + rbw - 3, bottom - rh + 3), img.px(left + rbw // 2, bottom + 1))
    check('%s art: the seven-hexagon picture is as wide as their box, its bottom edge on the box\'s bottom' % layout,
          rw == rbw and near(pts[0], HQ_PIC, 4) and near(pts[1], HQ_PIC, 4) and near(pts[2], HQ_PIC, 4) and not near(pts[3], HQ_PIC, 4), str(pts))


def main():
    args = sys.argv[1:]
    if len(args) < 2:
        print(__doc__)
        return 2
    hp.PLAYER, hp.REPO = os.path.abspath(args[0]), os.path.abspath(args[1])
    if '--shots' in args:
        hp.SHOTS = os.path.abspath(args[args.index('--shots') + 1])
        os.makedirs(hp.SHOTS, exist_ok=True)
    hp.MAP_W, hp.MAP_H = COLS, ROWS   # unit_cells() reads cell numbers with the map's width
    hp.TMP = tempfile.mkdtemp(prefix='ote_hexiso_test_')
    hp.GAMES = os.path.join(hp.TMP, 'games')
    try:
        for layout in ('pointy', 'flat'):
            iso = make_games(hp.GAMES, layout)
            flat_map(layout, iso)
            hills(layout, iso)
            picking(layout, iso)
            highlights(layout, iso)
            cursor(layout, iso)
            fog(layout, iso)
            structures(layout, iso)
            units_on_height(layout, iso)
            art(layout)
    finally:
        shutil.rmtree(hp.TMP, ignore_errors=True)
    print('\n%d passed, %d failed' % (hp.passed, len(hp.failures)))
    return 1 if hp.failures else 0


if __name__ == '__main__':
    sys.exit(main())
