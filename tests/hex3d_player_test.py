#!/usr/bin/env python3
"""The player's part of docs/HEX.md "3D isometric and full 3D" (hexagon maps in the two 3D kinds of view), windowless.

  python3 tests/hex3d_player_test.py ./build/engine/player/ote-player .
  python3 tests/hex3d_player_test.py ./build/engine/player/ote-player . --shots OUT_DIR   # also keeps the screenshots (BMP)

Writes small hexagon games with a "3D Isometric" and a "Full 3D" style in a temp folder (both layouts; flat-colour
placeholder terrain, so a lit block top shows its terrain colour exactly), runs the player headless (software 3D, and the
hardware backend where this machine has one) with --input scripts, and judges pixels of the 480 x 270 screenshot and the
Continue save. Where things are on the screen is worked out here again (the 3D space of docs/HEX.md: the top-down picture in
squares; the camera rules of docs/VIEWS.md), independently of the engine's classes:

  - every hexagon's top shows its terrain colour at the projected middle, at every one of the six turns of the 3D isometric
    view (60 degrees each) and in the full 3D view; heights lift a hexagon by heightStep per level and show side faces;
  - a tap on a hexagon (middle, or near an edge) picks the nearest hexagon, also on raised ground and at every turn; the
    direction buttons go by column and row (the nearest quarter turn of the camera);
  - highlights (move range, attack ring) lie on the tops as hexagons, the fog veil darkens whole hexagons, the cursor
    outlines its hexagon;
  - structures of one hexagon and of seven (placeholder blocks with the owner's frame), units visible on every turn;
  - standing terrain (a picture standing on a forest) rises above its ground at every turn of both kinds of view, and a game
    without it draws no such thing;
  - the hardware backend draws what the software one draws (a few edge pixels may differ).
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
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import failures, summary  # noqa: E402

check = hp.check
near, mix, fresh, save_of, play = hp.near, hp.mix, hp.fresh, hp.save_of, hp.play
IDLE = hp.IDLE
BLUE, RED, GOLD = hp.BLUE, hp.RED, hp.GOLD
VIEW_W, VIEW_H = hp.VIEW_W, hp.VIEW_H
COLS, ROWS = 7, 6
LAYOUTS = ('pointy', 'flat')
KINDS = (('i3', 'i3'), ('f3', 'f3'))   # (kind, style id of the test games: 'i3' is 3D isometric, 'f3' full 3D)
STEP = 12                       # heightStep default: tileWidth / 4
HEIGHTS = {(2, 2): 3, (4, 3): 1, (5, 3): 1}
LEVELS = 3
WALL = 0.45                     # a placeholder structure's block, in squares (map3d_sizes.h kWall)
MAGENTA, SAND, GREEN = (255, 0, 255), (200, 200, 120), (0, 170, 0)
SHOTS = ''
BACKENDS = ['software']


# ---- where things are: the contract's 3D space, written again here

class Space:
    """Squares: one top-down tileSize is one unit; X = flat pixel / T - map width / 2."""

    def __init__(self, geo):
        self.geo = geo
        mw, mh = geo.map_size()
        self.W, self.H = mw / float(geo.T), mh / float(geo.T)

    def at(self, x, y):
        mx, my = self.geo.middle(x, y)
        return mx / float(self.geo.T) - self.W / 2, my / float(self.geo.T) - self.H / 2


class Iso3(Space):
    """The 3D isometric view: 48 px per square (tileWidth), 2:1 squash, a level is `step` px, turns of 60 degrees."""
    PIXELS, SIN = 48.0, 0.5

    def __init__(self, geo, levels, step=STEP, unit_w=24):
        Space.__init__(self, geo)
        self.cos = math.sqrt(1 - self.SIN ** 2)
        self.stepY = step / (self.PIXELS * self.cos)
        self.levels = levels
        h = (levels + 1) * self.stepY
        self.R = math.ceil(math.sqrt(self.W ** 2 / 4 + self.H ** 2 / 4 + h * h) * self.PIXELS) + unit_w * 2
        self.size = int(self.R) * 2

    def world(self, turn, X, Y, Z):
        yaw = turn * math.pi / 3
        s, c = math.sin(yaw), math.cos(yaw)
        return ((c * X - s * Z) * self.PIXELS + self.R, ((s * X + c * Z) * self.SIN - Y * self.cos) * self.PIXELS + self.R)

    def top(self, turn, x, y, e, extra=0.0):
        X, Z = self.at(x, y)
        return self.world(turn, X, (e * self.stepY) + extra, Z)

    def screen(self, turn, cam, x, y, e=0, extra=0.0):
        wx, wy = self.top(turn, x, y, e, extra)
        return int(round(wx)) - cam[0], int(round(wy)) - cam[1]

    def hidden(self, turn, elev, x, y):
        """A hexagon some higher hexagon stands in front of (conservative: any higher block within two steps: a 3-level hill is 36 px, a row is 21 px)."""
        e = elev.get((x, y), 0)
        for (hx, hy), he in elev.items():
            if he > e and self.geo.distance((x, y), (hx, hy)) <= 2:
                return True
        return False


class Free3(Space):
    """The full 3D view at its start: the camera behind the cursor's hexagon, 25 degrees... default pitch 50, distance 11."""
    FOV, STEP = math.radians(35), 0.5

    def __init__(self, geo, levels):
        Space.__init__(self, geo)
        self.levels = levels
        self.yaw, self.pitch, self.dist = 0.0, 0.8726646, 11.0

    def basis(self, focus):
        s, c, sp, cp = math.sin(self.yaw), math.cos(self.yaw), math.sin(self.pitch), math.cos(self.pitch)
        eye = (focus[0] + s * cp * self.dist, focus[1] + sp * self.dist, focus[2] + c * cp * self.dist)
        return eye, (-s * cp, -sp, -c * cp), (c, 0.0, -s), (-s * sp, cp, -c * sp)

    def clamp(self, f):
        inset = self.dist * 0.2
        hx, hz = max(0.0, self.W / 2 - inset), max(0.0, self.H / 2 - inset)
        return [max(-hx, min(f[0], hx)), f[1], max(-hz, min(f[2], hz))]

    def proj_at(self, focus, X, Y, Z):
        eye, f, r, u = self.basis(focus)
        dx, dy, dz = X - eye[0], Y - eye[1], Z - eye[2]
        zc = dx * f[0] + dy * f[1] + dz * f[2]
        k = (VIEW_H * 0.5 / math.tan(self.FOV / 2)) / zc
        return VIEW_W * 0.5 + (dx * r[0] + dz * r[2]) * k, VIEW_H * 0.5 - (dx * u[0] + dy * u[1] + dz * u[2]) * k

    def camera_focus(self, cursor, elev):
        """The focus after the camera has settled: it starts on the first hexagon and moves just far enough that the cursor's
        point is inside the margins (22 % across, 24 % down) of the picture."""
        X0, Z0 = self.at(0, 0)
        focus = self.clamp([X0, elev.get((0, 0), 0) * self.STEP, Z0])
        X, Z = self.at(*cursor)
        Y = elev.get(cursor, 0) * self.STEP
        mx, my = VIEW_W * 0.22, VIEW_H * 0.24
        for _ in range(400):
            sx, sy = self.proj_at(focus, X, Y, Z)
            qx, qy = max(mx, min(sx, VIEW_W - mx)), max(my, min(sy, VIEW_H - my))
            if (qx, qy) != (sx, sy):
                eye, f, r, u = self.basis(focus)
                a, c = (qx - VIEW_W * 0.5) / (VIEW_H * 0.5 / math.tan(self.FOV / 2)), (VIEW_H * 0.5 - qy) / (VIEW_H * 0.5 / math.tan(self.FOV / 2))
                d = (f[0] + r[0] * a + u[0] * c, f[1] + u[1] * c, f[2] + r[2] * a + u[2] * c)
                t = (Y - eye[1]) / d[1]
                gX, gZ = eye[0] + d[0] * t, eye[2] + d[2] * t
                focus[0] += (X - gX) * 0.256
                focus[2] += (Z - gZ) * 0.256
            focus[1] += (Y - focus[1]) * 0.096
            focus = self.clamp(focus)
        return tuple(focus)

    def project(self, cursor, elev, X, Y, Z):
        return self.proj_at(self.camera_focus(cursor, elev), X, Y, Z)

    def screen(self, cursor, elev, x, y, e=None, extra=0.0):
        X, Z = self.at(x, y)
        sx, sy = self.project(cursor, elev, X, (elev.get((x, y), 0) if e is None else e) * self.STEP + extra, Z)
        return int(round(sx)), int(round(sy))


# ---- the test games

def write_3d_game(out, gid, layout, maps, art=None):
    g = hp.write_game(out, gid, layout, maps, art=art)
    gj = os.path.join(g, 'game.json')
    doc = json.load(open(gj))
    doc['styles'] = [{'id': 'classic', 'name': 'Classic'},
                     {'id': 'i3', 'name': '3D Isometric', 'view': 'isometric-3d', 'background': '#161a24'},
                     {'id': 'f3', 'name': 'Full 3D', 'view': '3d', 'background': '#161a24'}]
    json.dump(doc, open(gj, 'w'), indent=1)
    return g


def make3d_map(geo, heights=None, **kw):
    m = hp.make_map(geo, **kw)
    if heights is not None:
        rows = [[0] * geo.w for _ in range(geo.h)]
        for (x, y), v in heights.items():
            rows[y][x] = v
        m['heights'] = rows
    return m


BLUE_AT, RED_AT = (3, 4), (6, 0)


def make_games(out, layout):
    geo = hp.Geo(layout, COLS, ROWS)
    units = [hp.unit('infantry', 'blue', *BLUE_AT), hp.unit('infantry', 'red', *RED_AT)]
    maps = {
        'flat': make3d_map(geo, units=units),
        'hills': make3d_map(geo, heights=HEIGHTS, units=units),
        'fog': make3d_map(geo, units=units, fog=True),
        'structures': make3d_map(geo, heights={(5, 3): 1}, units=[hp.unit('infantry', 'blue', 0, 5), hp.unit('infantry', 'red', 6, 0)],
                                 structures=[{'type': 'hq', 'x': 2, 'y': 2, 'side': 'blue'},
                                             {'type': 'city', 'x': 5, 'y': 3, 'side': 'red'}]),
    }
    write_3d_game(out, 'h3_' + layout, layout, maps)
    return geo


def stand_game(out, layout, with_stand):
    """Flat-colour art: sand ground, a green forest ground, and (with_stand) a magenta picture standing on every forest."""
    geo = hp.Geo(layout, COLS, ROWS)
    bw, bh = geo.box_size()
    files = {'art/sand.png': hp.png_bytes(bw, bh, lambda x, y: SAND + (255,)),
             'art/green.png': hp.png_bytes(bw, bh, lambda x, y: GREEN + (255,)),
             'art/tree.png': hp.png_bytes(24, 32, lambda x, y: MAGENTA + (255,))}
    sheets = [{'id': 'sand', 'file': 'art/sand.png', 'cellWidth': bw, 'cellHeight': bh},
              {'id': 'green', 'file': 'art/green.png', 'cellWidth': bw, 'cellHeight': bh},
              {'id': 'tree', 'file': 'art/tree.png', 'cellWidth': 24, 'cellHeight': 32}]
    forest = {'sheet': 'green', 'cell': [0, 0]}
    if with_stand:
        forest['stand'] = {'sheet': 'tree', 'cell': [0, 0]}
    terrain = {'plain': {'sheet': 'sand', 'cell': [0, 0]}, 'forest': forest, 'sea': {'sheet': 'sand', 'cell': [0, 0]}}
    graphics = {'sheets': sheets, 'unitMarker': True, 'terrain': {k: {kk: vv for kk, vv in v.items() if kk != 'stand'} for k, v in terrain.items()},
                'styles': {'i3': {'terrain': terrain}, 'f3': {'terrain': terrain}}}
    tiles = {(x, y): 'a' for x in range(COLS) for y in range(ROWS)}
    tiles[(3, 2)] = 'b'
    m = hp.make_map(geo, units=[hp.unit('infantry', 'blue', 3, 4), hp.unit('infantry', 'red', 6, 0)])  # the cursor starts near the forest
    m['tiles'] = [''.join(tiles[(x, y)] for x in range(COLS)) for y in range(ROWS)]
    write_3d_game(out, 'stand%d_%s' % (with_stand, layout), layout, {'forest': m}, art={'graphics': graphics, 'files': files})
    return geo


# ---- running and reading

class Pic(hp.Image):
    def count(self, colour, tol=10, box=None):
        """Pixels within `tol` of a colour in the map area (or the box (x0, y0, x1, y1)); also the topmost row."""
        x0, y0, x1, y1 = box or (0, 0, 340, 250)
        n, top = 0, 10 ** 9
        for y in range(max(0, y0), min(self.h, y1)):
            for x in range(max(0, x0), min(self.w, x1)):
                if near(self.px(x, y), colour, tol):
                    n += 1
                    top = min(top, y)
        return n, top


def view(name, game, map_id, style, inp='', backend='software', **kw):
    path = os.path.join(hp.TMP, '%s_%s.bmp' % (name, backend))
    args = ['--style', style, '--3d', backend] + kw.pop('args', [])
    _, log = play(game, map_id, args=args, shot=path, inp=inp or IDLE, **kw)
    if SHOTS:
        shutil.copy(path, os.path.join(SHOTS, '%s_%s.bmp' % (name, backend)))
    return Pic(path), log


def turns(kind, n):
    """The --input that turns the view n steps: a sixth per press in 3D isometric."""
    return ',turnright,wait:2' * n + ',wait:30'


def terrain_rgb(geo, x, y):
    return hp.terrain_rgb(geo, x, y)


def unit_zone(geo):
    """The two units' hexagons and their neighbours: a unit's block and marker overlap the probe spots of those."""
    return {c for c in at_cells(geo) if min(geo.distance(c, BLUE_AT), geo.distance(c, RED_AT)) <= 1}


def at_cells(geo):
    return [(x, y) for y in range(geo.h) for x in range(geo.w)]


# ---- where a scene's hexagons are on the screen

CAMS = {}   # (layout, map, turn) -> (camera, matched, checked, picture): the 3D isometric camera, read off the picture


def anchor(img, iso, turn, elev):
    """A first guess of the camera from the blue unit's block (its bottom middle is where it stands); None without one block."""
    xs, ys = [], []
    for y in range(0, 250):
        for x in range(0, 340):
            if near(img.px(x, y), BLUE, 10):
                xs.append(x)
                ys.append(y)
    if not xs or max(xs) - min(xs) > 30 or max(ys) - min(ys) > 30:
        return None
    wx, wy = iso.top(turn, BLUE_AT[0], BLUE_AT[1], elev.get(BLUE_AT, 0))
    return int(round(wx - (min(xs) + max(xs)) / 2.0)), int(round(wy - max(ys)))


def locate(img, iso, turn, elev, geo, skip):
    """The scroll position of the 3D isometric view: the one offset at which the hexagon tops of every cell not in `skip`
    show their terrain at their projected middles (the camera's follow rules are the player's business, not this test's).
    Returns (camera, matched, checked)."""
    cells = [c for c in at_cells(geo) if c not in skip]
    world = {c: iso.top(turn, c[0], c[1], elev.get(c, 0)) for c in cells}
    want = {c: terrain_rgb(geo, *c) for c in cells}

    # Five samples per hexagon: the middle and four spots 3 px inside its edges. Only the right offset (to a pixel or so) has
    # all five in the hexagon's own colour, since every neighbour has another colour.
    sx_, sy_ = (21, 11) if geo.pointy else (25, 9)
    probes = [(0, 0), (-sx_, 0), (sx_, 0), (0, -sy_), (0, sy_)]

    def score(cx, cy, subset, spots=probes):
        ok = seen = 0
        for c in subset:
            px, py = int(round(world[c][0])) - cx, int(round(world[c][1])) - cy
            if 3 + sx_ <= px < VIEW_W - 3 - sx_ and 3 + sy_ <= py < VIEW_H - 3 - sy_:
                seen += 1
                ok += all(near(img.px(px + dx, py + dy), want[c], 6) for dx, dy in spots)
        return ok, seen

    # Where to look: around the blue unit's block when the picture has one alone (its feet are the hexagon's middle), else
    # everywhere the camera can be, first by the hexagons' middles alone (forgiving to ten pixels), then exactly.
    seed = anchor(img, iso, turn, elev)
    if seed:
        cx0, cy0 = seed
        top = None
        for cx in range(cx0 - 12, cx0 + 13):
            for cy in range(cy0 - 12, cy0 + 13):
                ok, seen = score(cx, cy, cells)
                if top is None or ok > top[0]:
                    top = (ok, seen, cx, cy)
    else:
        best = None
        for cx in range(-100, iso.size - VIEW_W + 101, 4):
            for cy in range(-100, iso.size - VIEW_H + 101, 4):
                ok, seen = score(cx, cy, cells, [(0, 0)])
                if best is None or ok > best[0]:
                    best = (ok, cx, cy)
        top = None
        for cx in range(best[1] - 8, best[1] + 9):
            for cy in range(best[2] - 8, best[2] + 9):
                ok, seen = score(cx, cy, cells)
                if top is None or ok > top[0]:
                    top = (ok, seen, cx, cy)
    return (top[2], top[3]), top[0], top[1]


def base_shot(layout, geo, map_id, turn, elev, skip):
    key = (layout, map_id, turn)
    if key not in CAMS:
        iso = Iso3(geo, max(elev.values()) if elev else 0)
        img, _ = view('%s_%s_t%d' % (layout, map_id, turn), 'h3_' + layout, map_id, 'i3', IDLE + turns('i3', turn))
        cam, ok, seen = locate(img, iso, turn, elev, geo, skip)
        CAMS[key] = (cam, ok, seen, img)
    return CAMS[key]


class Where:
    """Screen positions of a scene: `where(x, y)` = the middle of that hexagon's top (extra: squares above it)."""

    def __init__(self, layout, geo, kind, turn, elev, base_map, skip=(), levels=None, cursor=BLUE_AT):
        self.kind, self.turn, self.elev, self.geo = kind, turn, elev, geo
        lv = max(elev.values()) if elev else 0
        self.cursor = cursor
        if kind == 'i3':
            self.iso = Iso3(geo, lv)
            self.cam = base_shot(layout, geo, base_map, turn, elev, skip)[0]
        else:
            self.free = Free3(geo, lv)

    def __call__(self, x, y, e=None, extra=0.0):
        e = self.elev.get((x, y), 0) if e is None else e
        if self.kind == 'i3':
            return self.iso.screen(self.turn, self.cam, x, y, e, extra)
        return self.free.screen(self.cursor, self.elev, x, y, e, extra)

    def on_screen(self, p, margin=4):
        return margin <= p[0] < VIEW_W - margin and margin <= p[1] < VIEW_H - margin


def tag_of(layout, kind, turn=0):
    return '%s: %s' % (layout, 'full 3D' if kind == 'f3' else '3D isometric, turn %d of 6' % turn)


# ---- the checks

def tops(layout, geo):
    """Every cell's top shows its terrain colour at its projected middle: all six turns, then the free camera."""
    busy = unit_zone(geo)
    for turn in range(6):
        cam, ok, seen, _ = base_shot(layout, geo, 'flat', turn, {}, busy)
        check('%s: every hexagon top on the screen shows its terrain at its projected middle (%d of %d)' % (tag_of(layout, 'i3', turn), ok, seen),
              seen >= 6 and ok == seen, (ok, seen, cam))
    where = Where(layout, geo, 'f3', 0, {}, 'flat')
    img, _ = view('%s_flat_free' % layout, 'h3_' + layout, 'flat', 'f3')
    bad, seen = [], 0
    for c in at_cells(geo):
        p = where(*c)
        if c in busy or not where.on_screen(p):
            continue
        seen += 1
        if not near(img.px(*p), terrain_rgb(geo, *c), 6):
            bad.append((c, p, img.px(*p)))
    check('%s: every hexagon top shows its terrain at its projected middle (%d seen)' % (tag_of(layout, 'f3'), seen), seen >= 25 and not bad, bad[:3])


def hills(layout, geo):
    """Heights lift a hexagon by heightStep per level; side faces show; raised and clear hexagons keep their colours."""
    busy = unit_zone(geo)
    for turn in range(6):
        iso = Iso3(geo, LEVELS)
        skip = busy | {c for c in at_cells(geo) if iso.hidden(turn, HEIGHTS, *c)}
        cam, ok, seen, img = base_shot(layout, geo, 'hills', turn, HEIGHTS, skip)
        check('%s: hills: every uncovered hexagon top shows its terrain at its projected, lifted middle (%d of %d)' % (tag_of(layout, 'i3', turn), ok, seen),
              seen >= 4 and ok == seen, (ok, seen, cam))
        # the hill: three levels up, so its top is 3 x heightStep above where its ground middle would be
        (gx, gy), (tx, ty) = iso.screen(turn, cam, 2, 2, 0), iso.screen(turn, cam, 2, 2, 3)
        check('%s: the hill is lifted 3 x heightStep on the screen' % tag_of(layout, 'i3', turn), abs((gy - ty) - 3 * STEP) <= 1 and abs(gx - tx) <= 1, (gy - ty, gx - tx))
        # a side face shows under the hill's top on at least one side: darker than the top, neither background nor ground
        found = 0
        for dx, dy in ((0, 22), (-16, 18), (16, 18), (0, 30)):
            q = img.px(tx + dx, ty + dy)
            if q is not None and sum(q) < sum(terrain_rgb(geo, 2, 2)) - 30 and not near(q, (0x16, 0x1a, 0x24), 4):
                found += 1
        check('%s: a side face of the hill shows under its top' % tag_of(layout, 'i3', turn), found >= 1, found)
    where = Where(layout, geo, 'f3', 0, HEIGHTS, 'hills')
    img, _ = view('%s_hills_free' % layout, 'h3_' + layout, 'hills', 'f3')
    bad = [(c, where(*c), img.px(*where(*c))) for c in HEIGHTS if not near(img.px(*where(*c)), terrain_rgb(geo, *c), 6)]
    check('%s: raised hexagons show their terrain at their projected top' % tag_of(layout, 'f3'), not bad, bad)


def tap(layout, geo, kind, turn, elev, map_id, base_map, spots, label):
    """Select the infantry by tapping it, tap a spot, confirm the first menu item: the save says where the infantry went."""
    skip = {BLUE_AT, RED_AT}
    if elev:
        iso = Iso3(geo, LEVELS)
        skip |= {c for c in at_cells(geo) if iso.hidden(turn, elev, *c)}
    where = Where(layout, geo, kind, turn, elev, base_map, skip)
    style = kind  # the style ids of the test games are 'i3' and 'f3'
    for i, (name, fn, want) in enumerate(spots):
        ux, uy = where(*BLUE_AT)
        px, py = fn(where)
        sv = fresh('%s_%s_pick%d_%s%d' % (layout, label, i, kind, turn))
        inp = '%s%s,tap:%d:%d,wait:3,tap:%d:%d,wait:12,confirm,wait:40' % (IDLE, turns(kind, turn) if kind == 'i3' else '', ux, uy, round(px), round(py))
        play('h3_' + layout, map_id, args=['--style', style], saves=sv, inp=inp)
        s = save_of(sv, 'h3_' + layout)
        cells = sorted(hp.unit_cells(s)) if s else None
        check('%s: a tap on %s picks %s (%s)' % (tag_of(layout, kind, turn), name, want, label),
              cells is not None and any(c[0] == 0 and c[1:3] == want for c in cells), '%s tapped %s -> %s' % (name, (px, py), cells))


def picking(layout, geo):
    """A tap on a hexagon's middle, or 40 % of the way to a neighbour, picks it; 60 % of the way picks the neighbour: every
    turn, in both kinds; on raised ground the hill's lifted top and its side face pick the hill."""
    target = (3, 2)
    nbs = [n for n in geo.neighbors(*target) if n is not None and geo.distance(BLUE_AT, n) <= 3]
    for kind, _ in KINDS:
        for turn in ((0, 2, 5) if kind == 'i3' else [0]):
            nb = nbs[turn % len(nbs)]

            def towards(f):
                return lambda where: (where(*target)[0] + (where(*nb)[0] - where(*target)[0]) * f, where(*target)[1] + (where(*nb)[1] - where(*target)[1]) * f)
            tap(layout, geo, kind, turn, {}, 'flat', 'flat', [
                ('the middle of %s' % (target,), lambda w: w(*target), target),
                ('the point 40%% of the way from %s to %s' % (target, nb), towards(0.4), target),
                ('the point 60%% of the way from %s to %s' % (target, nb), towards(0.6), nb)], 'flat map')
    for kind, _ in KINDS:
        for turn in ((0, 3) if kind == 'i3' else [0]):
            spots = [('the lifted top of the hill', lambda w: w(2, 2), (2, 2))]
            if kind == 'i3':
                spots.append(('a spot low on the hill\'s side face', lambda w: (w(2, 2)[0], w(2, 2)[1] + 22), (2, 2)))
            tap(layout, geo, kind, turn, HEIGHTS, 'hills', 'hills', spots, 'hills')


def buttons(layout, geo):
    """The direction buttons go by column and row; a turned view uses the nearest quarter turn of the camera."""
    start = BLUE_AT
    # (turn, key, the step it makes): 0, 3, 2, 1, 4 sixths are 0, 2, 1, 1, 3 quarter turns
    cases = [(0, 'right', (1, 0)), (3, 'right', (-1, 0)), (2, 'right', (0, -1)), (1, 'down', (1, 0)), (4, 'right', (0, 1))]
    for turn, key, (dx, dy) in cases:
        want = (start[0] + dx, start[1] + dy)
        sv = fresh('%s_btn_%d' % (layout, turn))
        inp = '%s%s,confirm,wait:3,%s,confirm,wait:12,confirm,wait:40' % (IDLE, turns('i3', turn), key)
        play('h3_' + layout, 'flat', args=['--style', 'i3'], saves=sv, inp=inp)
        s = save_of(sv, 'h3_' + layout)
        cells = sorted(hp.unit_cells(s)) if s else None
        check('%s: "%s" moves the cursor one hexagon %s (by column and row, nearest quarter turn)' % (tag_of(layout, 'i3', turn), key, (dx, dy)),
              cells is not None and any(c[0] == 0 and c[1:3] == want for c in cells), '%s -> %s' % (want, cells))


def fog(layout, geo):
    """The veil darkens whole hexagons: those out of the unit's sight (distance > 2) are 0.45 of their colour darker."""
    busy = {BLUE_AT, RED_AT}
    for kind, style in KINDS:
        for turn in ((0, 3) if kind == 'i3' else [0]):
            where = Where(layout, geo, kind, turn, {}, 'flat', busy)
            img, _ = view('%s_fog_%s%d' % (layout, kind, turn), 'h3_' + layout, 'fog', style, IDLE + (turns(kind, turn) if kind == 'i3' else ''))
            n_dim = n_lit = bad = 0
            for c in at_cells(geo):
                p = where(*c)
                if c in busy or not where.on_screen(p):
                    continue
                base = terrain_rgb(geo, *c)
                lit = geo.distance(BLUE_AT, c) <= 2
                want = base if lit else tuple(int(round(v * (1 - 115 / 255.0))) for v in base)
                n_lit += lit
                n_dim += not lit
                bad += not near(img.px(*p), want, 9)
            check('%s: fog darkens the hexagons out of sight and only those (%d dark, %d lit)' % (tag_of(layout, kind, turn), n_dim, n_lit),
                  bad == 0 and n_dim > 5 and n_lit > 3, 'wrong: %d' % bad)


def highlights(layout, geo):
    """Select the infantry: blue on every hexagon it can reach, red on the ring beyond, as translucent hexagons on the tops."""
    busy = {BLUE_AT, RED_AT}
    for kind, style in KINDS:
        for turn in ((0, 4) if kind == 'i3' else [0]):
            where = Where(layout, geo, kind, turn, {}, 'flat', busy)
            img, _ = view('%s_range_%s%d' % (layout, kind, turn), 'h3_' + layout, 'flat', style,
                          IDLE + (turns(kind, turn) if kind == 'i3' else '') + ',confirm,wait:6')
            bad, nb, nr = [], 0, 0
            for c in at_cells(geo):
                d = geo.distance(BLUE_AT, c)
                p = where(*c)
                if c in busy or not where.on_screen(p):
                    continue
                base = terrain_rgb(geo, *c)
                want = mix(base, (50, 110, 255), 120 / 255.0) if d <= 3 else mix(base, (255, 50, 50), 120 / 255.0) if d == 4 else base
                nb += d <= 3
                nr += d == 4
                if not near(img.px(*p), want, 10):
                    bad.append((c, d, img.px(*p), want))
            check('%s: move range blue, attack ring red, nothing else (%d + %d hexagons)' % (tag_of(layout, kind, turn), nb, nr), not bad and nb > 8 and nr > 2, bad[:3])


def cursor_yellow(img, centre, radius=34):
    n = 0
    for y in range(centre[1] - radius, centre[1] + radius):
        for x in range(centre[0] - radius, centre[0] + radius):
            p = img.px(x, y)
            if p and p[0] > 180 and p[1] > 180 and p[2] < 130:
                n += 1
    return n


def cursor(layout, geo):
    """The cursor outlines its hexagon, on top of the block (the one on the hill's slope too)."""
    for kind, style in KINDS:
        where = Where(layout, geo, kind, 0, HEIGHTS, 'hills', {BLUE_AT, RED_AT})
        # the cursor starts on the blue unit's hexagon, plain ground (the pulsing yellow shows best on it)
        assert terrain_rgb(geo, *BLUE_AT) == hp.TERRAIN['a'][1]
        img, _ = view('%s_cursor_%s' % (layout, kind), 'h3_' + layout, 'hills', style, IDLE + ',wait:3')
        a = cursor_yellow(img, where(*BLUE_AT))
        b = cursor_yellow(img, where(0, 5))
        check('%s: the cursor outlines its hexagon (%d yellow pixels there, %d at a far one)' % (tag_of(layout, kind), a, b), a >= 25 and b <= 2, (a, b))


def structures(layout, geo):
    """The HQ (seven hexagons) and a city (one) of the 'structures' map: placeholder blocks, owner's frame, tops in colour."""
    elev = {(5, 3): 1}
    hq, city = (2, 2), (5, 3)
    ring = [hq] + [n for n in geo.neighbors(*hq)]
    skip = set(ring) | {city, (0, 5), (6, 0)}
    for kind, style in KINDS:
        for turn in ((0, 2, 3) if kind == 'i3' else [0]):
            where = Where(layout, geo, kind, turn, elev, 'structures', skip, cursor=(0, 5))
            img, _ = view('%s_struct_%s%d' % (layout, kind, turn), 'h3_' + layout, 'structures', style, IDLE + (turns(kind, turn) if kind == 'i3' else ''))
            tag = tag_of(layout, kind, turn)
            bad = [(c, img.px(*where(*c, extra=WALL))) for c in ring if where.on_screen(where(*c, extra=WALL)) and not near(img.px(*where(*c, extra=WALL)), GOLD, 10)]
            on = sum(1 for c in ring if where.on_screen(where(*c, extra=WALL)))
            check('%s: the HQ\'s hexagons show its block top in its colour (%d of 7 on the screen)' % (tag, on), on >= 4 and not bad, bad[:2])
            p = where(*city, extra=WALL)
            if where.on_screen(p, 24):  # (not at the very edge: the block's top is cut there)
                check('%s: a one-hexagon city shows its block top, raised with its ground' % tag, near(img.px(*p), (0xb7, 0xb7, 0xb7), 10), img.px(*p))
            n_blue, n_red = img.count(BLUE, 10)[0], img.count(RED, 10)[0]
            red_seen = where.on_screen(where(*city, extra=WALL), 30)  # the red city (and its frame) may lie off the picture
            check('%s: the owners\' frames and units are drawn (blue %d px, red %d px)' % (tag, n_blue, n_red), n_blue > 60 and (n_red > 30 or not red_seen), (n_blue, n_red))


def units(layout, geo):
    """Units stand on their hexagons in every view and turn: the side's colour above the feet."""
    busy = {BLUE_AT, RED_AT}
    for kind, style in KINDS:
        for turn in (range(6) if kind == 'i3' else [0]):
            where = Where(layout, geo, kind, turn, {}, 'flat', busy)
            img, _ = view('%s_units_%s%d' % (layout, kind, turn), 'h3_' + layout, 'flat', style, IDLE + (turns(kind, turn) if kind == 'i3' else ''))
            sx, sy = where(*BLUE_AT)
            n, top = img.count(BLUE, 12, (sx - 30, sy - 50, sx + 30, sy + 12))
            check('%s: the blue unit stands on its hexagon (%d px above its feet)' % (tag_of(layout, kind, turn), n), n > 60 and top < sy - 6, (n, top, sy))


def standing(layout):
    """A picture standing on a forest: visible above its green ground at every turn; none without the key."""
    for with_stand in (1, 0):
        stand_game(hp.GAMES, layout, with_stand)
        for kind, style in KINDS:
            for turn in (range(6) if kind == 'i3' else [0, 6, 12, 18]):
                inp = IDLE + (turns(kind, turn) if kind == 'i3' else ',turnright' * turn + ',wait:40')
                img, _ = view('%s_stand%d_%s%d' % (layout, with_stand, kind, turn), 'stand%d_%s' % (with_stand, layout), 'forest', style, inp)
                m, mtop = img.count(MAGENTA, 12)
                g, gtop = img.count(GREEN, 12)
                tag = '%s%s' % (tag_of(layout, kind, turn), '' if kind == 'i3' else ', camera turned %d degrees' % (turn * 15))
                if with_stand:
                    check('%s: the standing picture is drawn and rises above its ground (%d px)' % (tag, m), m > 80 and (g == 0 or mtop < gtop - 3), (m, mtop, g, gtop))  # (the picture may hide its ground entirely)
                else:
                    check('%s: without "stand" nothing stands there (%d magenta px)' % (tag, m), m == 0 and g > 40, (m, g))


def backends(layout, geo):
    """The hardware backend draws what the software one draws (a few edge pixels may differ)."""
    if 'hardware' not in BACKENDS:
        return
    scenes = [('hills', 'i3', IDLE + turns('i3', 2)), ('hills', 'f3', IDLE), ('structures', 'i3', IDLE + turns('i3', 5)),
              ('structures', 'f3', IDLE + ',confirm,wait:6'), ('fog', 'i3', IDLE), ('fog', 'f3', IDLE + ',turnright,turnright,wait:30')]
    for map_id, style, inp in scenes:
        a, _ = view('%s_bk_%s_%s' % (layout, map_id, style), 'h3_' + layout, map_id, style, inp, 'software')
        b, _ = view('%s_bk_%s_%s' % (layout, map_id, style), 'h3_' + layout, map_id, style, inp, 'hardware')
        d = sum(1 for y in range(a.h) for x in range(a.w) if not near(a.px(x, y), b.px(x, y), 8))
        check('%s: hardware draws what software draws on %s / %s (%d of %d px differ)' % (layout, map_id, style, d, a.w * a.h), d <= 0.002 * a.w * a.h, d)


def main():
    global SHOTS
    args = sys.argv[1:]
    if len(args) < 2:
        print(__doc__)
        return 2
    hp.PLAYER, hp.REPO = os.path.abspath(args[0]), os.path.abspath(args[1])
    if '--shots' in args:
        SHOTS = os.path.abspath(args[args.index('--shots') + 1])
        os.makedirs(SHOTS, exist_ok=True)
    hp.MAP_W, hp.MAP_H = COLS, ROWS
    hp.TMP = tempfile.mkdtemp(prefix='ote_hex3d_test_')
    hp.GAMES = os.path.join(hp.TMP, 'games')
    try:
        for layout in LAYOUTS:
            geo = make_games(hp.GAMES, layout)
            if layout == LAYOUTS[0]:  # is there a graphics driver? (as in render3d_backends_test.py)
                _, log = view('probe', 'h3_' + layout, 'flat', 'i3', 'wait:2', 'hardware')
                if '3D drawing: OpenGL' in log or '3D drawing: Direct3D 11' in log:
                    BACKENDS.append('hardware')
                else:
                    print('SKIP hardware backend: none here')
            tops(layout, geo)
            hills(layout, geo)
            picking(layout, geo)
            buttons(layout, geo)
            highlights(layout, geo)
            fog(layout, geo)
            cursor(layout, geo)
            structures(layout, geo)
            units(layout, geo)
            standing(layout)
            backends(layout, geo)
    finally:
        shutil.rmtree(hp.TMP, ignore_errors=True)
    summary(blank_line=True)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
