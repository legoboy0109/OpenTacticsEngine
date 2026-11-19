#!/usr/bin/env python3
"""The player's part of docs/GRAPHICS.md "Standing terrain": a terrain's standing picture or model in the two 3D kinds
of view, windowless, with both 3D backends.

  python3 tests/terrain3d_player_test.py ./build/engine/player/ote-player .
  python3 tests/terrain3d_player_test.py ./build/engine/player/ote-player . --baseline ./other/ote-player   # also: 2D pictures of
                                                                         # the demos equal those of another (older) player build

Writes small games in a temp folder (copies of demo-economy with flat-colour art: the ground of a forest is green, what
stands on it is magenta, plain ground is sand, the blue side is orange) and runs the player with --input scripts. Checks,
by counting pixels of those exact colours:
  - the standing part is visible and rises ABOVE its ground (picture and model, 3D isometric and full 3D, both backends);
  - without the "stand" key there is none, and the 2D styles of the same game (top-down and 2D isometric) draw byte for
    byte the same pictures with or without it;
  - a unit standing on such a cell stays visible (at least 70% of the pixels it has on plain ground);
  - fog dims it (0.55 of its colour), the maker's --show-all does not;
  - a unit's move range tints it blue;
  - the four quarter turns of 3D isometric and four 90 degree turns of the full 3D camera each show it;
  - the hardware backend draws what the software one draws (a few edge pixels may differ).
The hardware scenes are skipped on a machine without a usable graphics driver (as in render3d_backends_test.py).
"""
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import check_dash, failures, summary  # noqa: E402
check = check_dash  # this script prints the "PASS name" / "FAIL name -> detail" layout

PLAYER = REPO = ''
BASELINE = ''

MAGENTA, GREEN, ORANGE = (255, 0, 255), (0, 170, 0), (255, 208, 0)
DIM = 0.55


# ---- files of the test games

def png(w, h, rgba):
    def chunk(kind, data):
        body = kind + data
        return struct.pack('>I', len(data)) + body + struct.pack('>I', zlib.crc32(body) & 0xFFFFFFFF)
    rows = b''.join(b'\x00' + bytes(rgba) * w for _ in range(h))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)) +
            chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))


def box_glb(color):
    """A 0.6 x 1 x 0.6 box standing on Y = 0 (one material, flat colour), as a .glb."""
    hw, h = 0.3, 1.0
    faces = [((0, 0, 1), [(-hw, 0, hw), (hw, 0, hw), (hw, h, hw), (-hw, h, hw)]),
             ((0, 0, -1), [(hw, 0, -hw), (-hw, 0, -hw), (-hw, h, -hw), (hw, h, -hw)]),
             ((1, 0, 0), [(hw, 0, hw), (hw, 0, -hw), (hw, h, -hw), (hw, h, hw)]),
             ((-1, 0, 0), [(-hw, 0, -hw), (-hw, 0, hw), (-hw, h, hw), (-hw, h, -hw)]),
             ((0, 1, 0), [(-hw, h, hw), (hw, h, hw), (hw, h, -hw), (-hw, h, -hw)])]
    pos, nor, idx = [], [], []
    for n, quad in faces:
        base = len(pos)
        for p in quad:
            pos.append(p)
            nor.append(n)
        idx += [base, base + 1, base + 2, base, base + 2, base + 3]
    pb = b''.join(struct.pack('<3f', *p) for p in pos)
    nb = b''.join(struct.pack('<3f', *p) for p in nor)
    ib = struct.pack('<%dH' % len(idx), *idx)
    ib += b'\0' * (-len(ib) % 4)
    blob = pb + nb + ib
    doc = {'asset': {'version': '2.0'}, 'scene': 0, 'scenes': [{'nodes': [0]}], 'nodes': [{'mesh': 0}],
           'meshes': [{'primitives': [{'attributes': {'POSITION': 0, 'NORMAL': 1}, 'indices': 2, 'material': 0}]}],
           'materials': [{'name': 'body', 'pbrMetallicRoughness': {'baseColorFactor': [c / 255.0 for c in color] + [1.0]}}],
           'accessors': [{'bufferView': 0, 'componentType': 5126, 'count': len(pos), 'type': 'VEC3',
                          'min': [-hw, 0, -hw], 'max': [hw, h, hw]},
                         {'bufferView': 1, 'componentType': 5126, 'count': len(pos), 'type': 'VEC3'},
                         {'bufferView': 2, 'componentType': 5123, 'count': len(idx), 'type': 'SCALAR'}],
           'bufferViews': [{'buffer': 0, 'byteOffset': 0, 'byteLength': len(pb)},
                           {'buffer': 0, 'byteOffset': len(pb), 'byteLength': len(nb)},
                           {'buffer': 0, 'byteOffset': len(pb) + len(nb), 'byteLength': len(ib)}],
           'buffers': [{'byteLength': len(blob)}]}
    js = json.dumps(doc, separators=(',', ':')).encode()
    js += b' ' * (-len(js) % 4)
    body = struct.pack('<II', len(js), 0x4E4F534A) + js + struct.pack('<II', len(blob), 0x004E4942) + blob
    return b'glTF' + struct.pack('<II', 2, 12 + len(body)) + body


def strip_comments(text):
    return ''.join(l for l in text.splitlines(True) if not l.lstrip().startswith('//'))


def map_json(tiles, units, fog=False):
    return {'name': 'stand', 'grid': {'type': 'square', 'width': len(tiles[0]), 'height': len(tiles)}, 'fog': fog,
            'legend': {'.': 'plain', 'f': 'forest', 'm': 'mountain', 'r': 'road', '~': 'sea'}, 'tiles': tiles,
            'sides': [{'id': 'blue', 'name': 'Blue', 'color': '#ffd000', 'controller': 'human', 'resources': {'funds': 100}},
                      {'id': 'red', 'name': 'Red', 'color': '#cc0000', 'controller': 'ai', 'resources': {'funds': 100}}],
            'units': units}


STYLES3D = ('isometric-3d', 'full-3d')


def graphics(stand):
    """Flat-colour art for the two 3D styles; `stand` is the forest's "stand" (None = the game without it)."""
    sheets = [{'id': 'sand', 'file': 'art/sand.png', 'cellWidth': 24, 'cellHeight': 24},
              {'id': 'green', 'file': 'art/green.png', 'cellWidth': 24, 'cellHeight': 24},
              {'id': 'tree', 'file': 'art/tree.png', 'cellWidth': 24, 'cellHeight': 32}]
    terrain = {'plain': {'sheet': 'sand', 'cell': [0, 0]}, 'road': {'sheet': 'sand', 'cell': [0, 0]},
               'sea': {'sheet': 'sand', 'cell': [0, 0]}, 'mountain': {'sheet': 'sand', 'cell': [0, 0]},
               'forest': {'sheet': 'green', 'cell': [0, 0]}}
    if stand:
        terrain['forest'] = dict(terrain['forest'], stand=stand)
    # The top-down and 2D isometric styles: flat pictures too (and never a stand, which they refuse).
    flat = {t: dict(look) for t, look in terrain.items()}
    for look in flat.values():
        look.pop('stand', None)
    return {'sheets': sheets, 'unitMarker': True, 'terrain': flat,
            'styles': {s: {'terrain': terrain} for s in STYLES3D}}


def make_game(root, name, stand, models=False):
    dst = os.path.join(root, name)
    shutil.copytree(os.path.join(REPO, 'games', 'demo-economy'), dst)
    shutil.rmtree(os.path.join(dst, 'art'))
    os.makedirs(os.path.join(dst, 'art'))
    os.makedirs(os.path.join(dst, 'models'))
    for sheet, color in (('sand', (200, 200, 120, 255)), ('green', GREEN + (255,)), ('tree', MAGENTA + (255,))):
        w, h = (24, 32) if sheet == 'tree' else (24, 24)
        open(os.path.join(dst, 'art', sheet + '.png'), 'wb').write(png(w, h, color))
    open(os.path.join(dst, 'models', 'tree.glb'), 'wb').write(box_glb(MAGENTA))
    # The maps: a single forest square in the middle of a small plain map (no fog) / with fog / with a unit on it / beside a unit.
    plain = ['..........'] * 6
    forest_mid = ['..........', '..........', '....f.....', '..........', '..........', '..........']
    maps = {
        'lone': map_json(forest_mid, [{'type': 'infantry', 'side': 'blue', 'x': 0, 'y': 5}, {'type': 'infantry', 'side': 'red', 'x': 9, 'y': 0}]),
        'unit_on': map_json(forest_mid, [{'type': 'infantry', 'side': 'blue', 'x': 4, 'y': 2}, {'type': 'infantry', 'side': 'red', 'x': 9, 'y': 0}]),
        'unit_on_plain': map_json(plain, [{'type': 'infantry', 'side': 'blue', 'x': 4, 'y': 2}, {'type': 'infantry', 'side': 'red', 'x': 9, 'y': 0}]),
        'fogged': map_json(forest_mid[:2] + ['.....f....'] + forest_mid[3:], [{'type': 'infantry', 'side': 'blue', 'x': 0, 'y': 5}, {'type': 'infantry', 'side': 'red', 'x': 9, 'y': 0}], fog=True),
        'range': map_json(forest_mid, [{'type': 'infantry', 'side': 'blue', 'x': 3, 'y': 2}, {'type': 'infantry', 'side': 'red', 'x': 9, 'y': 5}]),
    }
    for mid, m in maps.items():
        json.dump(m, open(os.path.join(dst, 'maps', mid + '.json'), 'w'))
    g = json.loads(strip_comments(open(os.path.join(dst, 'game.json')).read()))
    g['maps'] = [{'id': mid, 'name': mid, 'file': 'maps/%s.json' % mid} for mid in maps]
    json.dump(g, open(os.path.join(dst, 'game.json'), 'w'), indent=1)
    gfx = graphics(stand)
    json.dump(gfx, open(os.path.join(dst, 'data', 'graphics.json'), 'w'), indent=1)
    return dst


# ---- running the player and reading pictures

def read_bmp(path):
    data = open(path, 'rb').read()
    off = struct.unpack_from('<I', data, 10)[0]
    w, h = struct.unpack_from('<ii', data, 18)
    bpp = struct.unpack_from('<H', data, 28)[0] // 8
    stride = (w * bpp + 3) & ~3
    rows = [data[off + r * stride: off + r * stride + w * bpp] for r in range(abs(h))]
    if h > 0:
        rows.reverse()
    return w, abs(h), rows, bpp


def shoot(games, game, map_id, style, inp, out, backend='software', extra=(), player=None):
    cmd = [player or PLAYER, '--headless', '--no-audio', '--no-settings', '--no-saving', '--3d', backend, '--fixed-dt', '0.0166',
           '--games', games, '--assets', os.path.join(REPO, 'assets'), '--game', game, '--map', map_id, '--style', style,
           '--input', inp, '--frames', '400', '--screenshot', out] + list(extra)
    res = subprocess.run(cmd, capture_output=True, text=True, timeout=180)
    log = res.stdout + res.stderr
    return res.returncode == 0, log


class Pic:
    def __init__(self, path):
        self.w, self.h, self.rows, self.bpp = read_bmp(path)
        self.raw = open(path, 'rb').read()

    def px(self, x, y):
        r = self.rows[y]
        i = x * self.bpp
        return r[i + 2], r[i + 1], r[i]  # BMP is BGR

    def find(self, near, tol=14, region=None):
        """(count, min y, max y, min x, max x) of the pixels within tol of the colour `near` (in the 2D-free left part
        of the picture: the 3D map area, the right panel is skipped)."""
        n, y0, y1, x0, x1 = 0, 10 ** 9, -1, 10 ** 9, -1
        W = 340  # the map area (the info panel starts to the right of it)
        for y in range(0, self.h - 20):
            row = self.rows[y]
            for x in range(0, min(W, self.w)):
                i = x * self.bpp
                if abs(row[i + 2] - near[0]) <= tol and abs(row[i + 1] - near[1]) <= tol and abs(row[i] - near[2]) <= tol:
                    n += 1
                    if y < y0: y0 = y
                    if y > y1: y1 = y
                    if x < x0: x0 = x
                    if x > x1: x1 = x
        return n, y0, y1, x0, x1


def differ(a, b, tolerance=8):
    d = 0
    for ra, rb in zip(a.rows, b.rows):
        if ra == rb:
            continue
        for i in range(0, len(ra), a.bpp):
            if any(abs(ra[i + k] - rb[i + k]) > tolerance for k in range(3)):
                d += 1
    return d


def turns(style, k):
    """The --input for the k-th turn of the camera (a quarter turn in 3D isometric, 90 degrees in full 3D)."""
    if style == 'full-3d':
        return ',turnright' * (6 * k)
    return (',turnright,wait:30') * k


def main():
    global PLAYER, REPO, BASELINE
    args = [a for a in sys.argv[1:]]
    if '--baseline' in args:
        i = args.index('--baseline')
        BASELINE = os.path.abspath(args[i + 1])
        del args[i:i + 2]
    if len(args) < 2:
        print(__doc__)
        return 2
    PLAYER, REPO = os.path.abspath(args[0]), os.path.abspath(args[1])
    with tempfile.TemporaryDirectory() as tmp:
        games = os.path.join(tmp, 'games')
        os.makedirs(games)
        picture = {'sheet': 'tree', 'cell': [0, 0]}
        make_game(games, 'withpic', picture)
        make_game(games, 'withmodel', {'model': 'models/tree.glb', 'height': 1.2})
        make_game(games, 'without', None)
        shot = lambda g, m, s, i, b='software', extra=(), player=None: _shoot(tmp, games, g, m, s, i, b, extra, player)

        backends = ['software']
        ok, log = shoot(games, 'withpic', 'lone', 'isometric-3d', 'wait:5', os.path.join(tmp, 'probe.bmp'), 'hardware')
        if ok and ('3D drawing: OpenGL' in log or '3D drawing: Direct3D 11' in log):
            backends.append('hardware')
        else:
            print('SKIP hardware backend: none here')

        for backend in backends:
            for style in STYLES3D:
                tag = '%s, %s' % (style, backend)
                for game, what in (('withpic', 'picture'), ('withmodel', 'model')):
                    p = shot(game, 'lone', style, 'wait:20', backend)
                    ground = p.find(GREEN)
                    stand = p.find(MAGENTA, 70) if what == 'model' else p.find(MAGENTA)
                    check('%s: a standing %s is drawn (%d px)' % (tag, what, stand[0]), stand[0] > 120, stand)
                    check('%s: the %s rises above its ground picture' % (tag, what), stand[0] > 0 and ground[0] > 0 and stand[1] < ground[1] - 3, (stand, ground))
                none = shot('without', 'lone', style, 'wait:20', backend)
                check('%s: without a "stand" there is nothing standing' % tag, none.find(MAGENTA, 70)[0] == 0 and none.find(GREEN)[0] > 100, (none.find(MAGENTA, 70), none.find(GREEN)))

                # A unit on the cell stays visible.
                for game, what in (('withpic', 'picture'), ('withmodel', 'model')):
                    on = shot(game, 'unit_on', style, 'wait:20', backend)
                    ref = shot('without', 'unit_on_plain', style, 'wait:20', backend)
                    a, b = on.find(ORANGE, 30)[0], ref.find(ORANGE, 30)[0]
                    check('%s: a unit on a cell with a standing %s stays visible (%d px, %d on plain ground)' % (tag, what, a, b), a >= 0.7 * b and a > 60, (a, b))

                # Fog dims it; --show-all does not.
                fog = shot('withpic', 'fogged', style, 'wait:20', backend)
                clear = shot('withpic', 'fogged', style, 'wait:20', backend, ['--show-all'])
                dim = tuple(int(c * DIM) for c in MAGENTA)
                check('%s: fog dims the standing picture' % tag, fog.find(dim, 22)[0] > 100 and fog.find(MAGENTA, 30)[0] == 0, (fog.find(dim, 22), fog.find(MAGENTA, 30)))
                check('%s: --show-all draws it undimmed' % tag, clear.find(MAGENTA, 30)[0] > 100 and clear.find(dim, 12)[0] == 0, (clear.find(MAGENTA, 30), clear.find(dim, 12)))
                fogm = shot('withmodel', 'fogged', style, 'wait:20', backend)
                clearm = shot('withmodel', 'fogged', style, 'wait:20', backend, ['--show-all'])
                # a lit model: every shade of it is darker under fog, so count the bright (lit) magenta pixels: none under fog.
                bright = lambda p: p.find((255, 0, 255), 40)[0]
                check('%s: fog dims a standing model (%d bright px under fog, %d without)' % (tag, bright(fogm), bright(clearm)), bright(clearm) > 100 and bright(fogm) < bright(clearm) // 4, (bright(fogm), bright(clearm)))

                # In move range the picture is tinted blue.
                rng = shot('withpic', 'range', style, 'wait:10,confirm,wait:10', backend)
                tint = (int(255 * 0.62), 0, 255)
                check('%s: the move range tints the standing picture' % tag, rng.find(tint, 14)[0] > 60, rng.find(tint, 14))

                # Every turn of the camera shows it.
                for k in range(4):
                    t = shot('withpic', 'lone', style, 'wait:20' + turns(style, k) + ',wait:40', backend)
                    st, gr = t.find(MAGENTA), t.find(GREEN)
                    check('%s, turn %d: the standing picture is drawn and rises above the ground' % (tag, k), st[0] > 120 and gr[0] > 0 and st[1] < gr[1] - 3, (st, gr))
                    tm = shot('withmodel', 'lone', style, 'wait:20' + turns(style, k) + ',wait:40', backend)
                    check('%s, turn %d: the standing model is drawn' % (tag, k), tm.find(MAGENTA, 70)[0] > 120, tm.find(MAGENTA, 70))

        if 'hardware' in backends:
            for style in STYLES3D:
                for game in ('withpic', 'withmodel'):
                    s = shot(game, 'unit_on', style, 'wait:20', 'software')
                    h = shot(game, 'unit_on', style, 'wait:20', 'hardware')
                    d = differ(s, h)
                    check('%s, %s: hardware draws what software draws (%d of %d px differ)' % (style, game, d, s.w * s.h), d <= 0.002 * s.w * s.h, d)

        # 2D styles: byte for byte the same with and without the key; and against another build when asked.
        for style in ('classic', 'isometric'):
            for m in ('lone', 'unit_on'):
                a = shot('withpic', m, style, 'wait:20')
                b = shot('without', m, style, 'wait:20')
                check('the 2D style %s is identical with and without a standing part (%s)' % (style, m), a.raw == b.raw)
        if BASELINE:
            for game in ('demo-economy', 'demo-srpg'):
                mp = 'twin_rivers' if game == 'demo-economy' else 'bridge'
                for style in ('classic', 'isometric', 'night' if game == 'demo-economy' else 'parchment'):
                    real = os.path.join(REPO, 'games')
                    a = Pic(_run(tmp, real, game, mp, style, 'wait:30,skip,wait:20', PLAYER))
                    b = Pic(_run(tmp, real, game, mp, style, 'wait:30,skip,wait:20', BASELINE))
                    check('%s, style %s: the same picture as the baseline build' % (game, style), a.raw == b.raw)
    summary(blank_line=True)
    return 1 if failures else 0


_n = [0]


def _run(tmp, games, game, map_id, style, inp, player, backend='software', extra=()):
    _n[0] += 1
    out = os.path.join(tmp, 's%d.bmp' % _n[0])
    ok, log = shoot(games, game, map_id, style, inp, out, backend, extra, player)
    if not ok:
        raise SystemExit('the player failed on %s/%s/%s:\n%s' % (game, map_id, style, log[-800:]))
    return out


def _shoot(tmp, games, game, map_id, style, inp, backend, extra, player):
    return Pic(_run(tmp, games, game, map_id, style, inp, player, backend, extra))


if __name__ == '__main__':
    sys.exit(main())
