#!/usr/bin/env python3
"""The player and a map's tile layer (docs/GRAPHICS.md "Tile layer (hand-painted maps)"), windowless.

  python3 tests/tiles_player_test.py ./build/engine/player/ote-player .
  python3 tests/tiles_player_test.py ./build/engine/player/ote-player . --shots OUT_DIR   # keep the screenshots (BMP and PNG)

Builds a game from tests/fixtures/tiles in a temp folder (its sheet is made by tools/art/make_test_tileset.py: 32 tiles of 16 x 16,
each a flat colour with a black mark top-left, a white mark top-right and its number in the middle), runs the player headless (no
audio, no settings, no saving) and reads screenshots:

  - 2D view: the tile in each square, exact colours of known tiles (and of the terrain where the square has no tile), not mirrored
    or turned (the corner marks), units / cursor / highlights on top; the animated water tile has changed between two shots taken at
    different times; the map change of round 2 shows in the picture (a tile replaced, a tile cleared, a tile kept);
  - 2D isometric, 3D isometric and full 3D: before / after pictures (their pixels are checked loosely: something changed where the
    squares are; the files are for looking at), and a portrait-shaped (270 x 480) shot of the 2D view;
  - the player's log says nothing is wrong with the layer.
"""
import importlib.util
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True

PLAYER = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else ''
REPO = os.path.abspath(sys.argv[2]) if len(sys.argv) > 2 else '.'
SHOTS = sys.argv[sys.argv.index('--shots') + 1] if '--shots' in sys.argv else ''
TMP = tempfile.mkdtemp(prefix='ote-tiles-')
passed, failures = 0, []

spec = importlib.util.spec_from_file_location('make_test_tileset', os.path.join(REPO, 'tools/art/make_test_tileset.py'))
TS = importlib.util.module_from_spec(spec)
spec.loader.exec_module(TS)

IDLE = 'wait:14'  # the turn banner plays first; it takes the first inputs otherwise
# Move the cursor off the units and end the turn from the empty square's menu; red plays, round 2 begins (the map change event).
END_TURN = [IDLE, 'right', 'wait:2', 'right', 'wait:2', 'confirm', 'wait:3', 'confirm', 'wait:200']
SELECT = [IDLE, 'confirm', 'wait:20']  # select the hero: its move range is highlighted


def read_json(path):
    with open(path) as f:
        return json.load(f)


def write_json(path, obj):
    with open(path, 'w') as f:
        json.dump(obj, f, indent=1)


def make_game(gid, view=(480, 270)):
    g = os.path.join(TMP, gid)
    shutil.copytree(os.path.join(REPO, 'tests/fixtures/tiles'), g)
    game = read_json(g + '/game.json')
    game['view']['width'], game['view']['height'] = view
    write_json(g + '/game.json', game)
    os.makedirs(g + '/art', exist_ok=True)  # the fixture has no art folder in git (the sheet is generated)
    TS.write_sheet(g + '/art/tiles.png')
    return g


def ticks(script):
    return sum(int(t[5:]) if t.startswith('wait:') else 1 for t in script.split(','))


def inp(*parts):
    flat = []
    for p in parts:
        flat.extend(p if isinstance(p, list) else [p])
    return ','.join(flat)


def run(gid, script, style, shot, extra_frames=90):
    cmd = [PLAYER, '--games', TMP, '--assets', os.path.join(REPO, 'assets'), '--headless', '--no-audio', '--no-settings', '--no-saving',
           '--frames', str(ticks(script) * 4 + extra_frames), '--input-interval', '4', '--fixed-dt', '0.016', '--game', gid,
           '--map', 'tiled', '--style', style, '--3d', 'software', '--screenshot', shot, '--input', script]
    p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True, timeout=300)
    return p.returncode, p.stdout


class Image:
    def __init__(self, path):
        data = open(path, 'rb').read()
        off = struct.unpack_from('<I', data, 10)[0]
        self.w, h = struct.unpack_from('<ii', data, 18)
        bpp = struct.unpack_from('<H', data, 28)[0] // 8
        self.h = abs(h)
        stride = (self.w * bpp + 3) // 4 * 4
        self.px = []
        for y in range(self.h):
            row = (self.h - 1 - y) if h > 0 else y
            base = off + row * stride
            self.px.append([(data[base + x * bpp + 2], data[base + x * bpp + 1], data[base + x * bpp]) for x in range(self.w)])

    def at(self, x, y):
        return self.px[y][x]

    def differs(self, other, margin=30):
        if (self.w, self.h) != (other.w, other.h):
            return -1
        return sum(1 for y in range(self.h) for x in range(self.w) if max(abs(a - b) for a, b in zip(self.px[y][x], other.px[y][x])) > margin)

    def box_differs(self, other, x0, y0, x1, y1, margin=30):
        return sum(1 for y in range(y0, y1) for x in range(x0, x1)
                   if max(abs(a - b) for a, b in zip(self.px[y][x], other.px[y][x])) > margin)


def check(name, cond, detail=''):
    global passed
    if cond:
        passed += 1
        print('PASS ' + name)
    else:
        failures.append(name)
        print('FAIL %s %s' % (name, detail))


def shot(name, gid, script, style):
    path = os.path.join(TMP, name + '.bmp')
    code, log = run(gid, script, style, path)
    check('%s: the player ran' % name, code == 0 and os.path.exists(path), log[-400:])
    if SHOTS:
        os.makedirs(SHOTS, exist_ok=True)
        shutil.copy(path, os.path.join(SHOTS, name + '.bmp'))
        subprocess.run(['convert', path, os.path.join(SHOTS, name + '.png')], check=False)
    return Image(path), log


def near(a, b, tol=12):
    return all(abs(x - y) <= tol for x, y in zip(a, b))


# ---- where things are in the 2D view of the 480 x 270 game: tileSize 24 and a 10 x 7 map. ORIGIN is found from the screenshot. ----
TILE_PX = 24
PLAIN = (156, 204, 101)  # the terrain colour of plain (a square with no tile shows it)


def find_origin(img):
    """The top-left pixel of the map: the black 2 x 2 mark of tile 12 in square (0,0) scales to 3 x 3 black pixels at the origin."""
    for y in range(img.h - 3):
        for x in range(img.w - 3):
            if all(img.at(x + i, y + j) == (0, 0, 0) for i in range(3) for j in range(3)):
                return x, y
    return None


def tile_sample(origin, sx, sy, tx, ty):
    """The screen pixel inside square (sx, sy) that shows pixel (tx, ty) of its 16 x 16 tile (the tile is drawn 1.5 times as big)."""
    return origin[0] + sx * TILE_PX + int((tx + 0.5) * 1.5), origin[1] + sy * TILE_PX + int((ty + 0.5) * 1.5)


def tile_at(x, y):
    if y == 6 and x < 2:
        return -1
    if 4 <= x <= 6 and 2 <= y <= 4:
        return 8
    return 12 + (x * 3 + y * 7) % 20


def test_2d():
    make_game('g2d')
    before, blog = shot('top_before', 'g2d', inp(IDLE, 'wait:10'), 'top')
    org = find_origin(before)
    check('2D: the map is on screen (the first tile\'s black corner mark is at its top-left)', org is not None)
    if not org:
        return
    print('  map origin', org)
    # Every square that has a tile shows that tile's colour at a pixel no mark or digit covers (tile pixel 2, 8: left middle); the
    # units' squares and the animated water are left out here.
    bad = []
    for y in range(7):
        for x in range(10):
            if (x, y) in ((1, 1), (8, 5)) or tile_at(x, y) == 8:
                continue
            want = PLAIN if tile_at(x, y) < 0 else TS.tile_colour(tile_at(x, y))
            got = before.at(*tile_sample(org, x, y, 2, 8))
            if not near(got, want):
                bad.append((x, y, got, want))
    check('2D: every square shows its own tile (or the terrain colour where it has none)', not bad, str(bad[:4]))
    # Orientation: the black mark is top-left, the white one top-right of a tile; the digits are not mirrored (tile 12: "12", the
    # "1" is a vertical bar left of the "2": in tile pixels the bar is the column x=4... checked by the whole 16 x 16 tile).
    wrong = 0
    for (x, y) in ((0, 0), (3, 1), (7, 5), (2, 5)):
        for ty in range(16):
            for tx in range(16):
                if (x, y) == (1, 1):
                    continue
                want = TS.tile_pixel(tile_at(x, y), tx, ty)[:3]
                got = before.at(*tile_sample(org, x, y, tx, ty))
                if not near(got, want, 20):
                    wrong += 1
    check('2D: a tile is drawn as its picture: not mirrored, not turned (4 whole tiles compared pixel by pixel)', wrong < 40, str(wrong))
    # Units and the cursor stand on top of the tiles.
    hero = before.at(*tile_sample(org, 1, 1, 8, 8))
    check('2D: the hero is drawn over its tile', not near(hero, TS.tile_colour(tile_at(1, 1)), 6), str(hero))
    # Animated water: two shots at different times show different frames of tile 8 (colours of tiles 8, 9, 10).
    seen = set()
    for i, wait in enumerate((10, 11, 12, 13, 14)):
        img, _ = shot('top_water_%d' % i, 'g2d', inp(IDLE, 'wait:%d' % wait), 'top')
        c = img.at(*tile_sample(org, 5, 3, 2, 8))
        for n in (8, 9, 10):
            if near(c, TS.tile_colour(n), 6):
                seen.add(n)
    check('2D: the water tile cycles through its frames over time (tiles 8, 9 and 10 all seen)', seen == {8, 9, 10}, str(seen))
    # Highlights over tiles: select the hero, its reach tints squares blue over the tile.
    sel, slog = shot('top_selected', 'g2d', inp(SELECT), 'top')
    got = sel.at(*tile_sample(org, 2, 1, 2, 8))
    want = TS.tile_colour(tile_at(2, 1))
    check('2D: the move highlight is tinted over a tile (bluer than the tile alone)', got[2] > want[2] + 15 and got[0] < want[0], '%s vs %s' % (got, want))
    # The map change at round 2.
    after, alog = shot('top_after', 'g2d', inp(END_TURN), 'top')
    check('2D: round 2 began and the change was applied', 'round 2' in alog and 'map changed' in alog, alog[-300:])

    def col(img, x, y, tx=2, ty=8):
        return img.at(*tile_sample(org, x, y, tx, ty))
    check('2D: a change square with its own tile shows that tile (4,3 -> tile 5)', near(col(after, 4, 3), TS.tile_colour(5)), str(col(after, 4, 3)))
    check('2D: a terrain change with no tile given clears the tile (5,3 shows the plain terrain colour)', near(col(after, 5, 3), PLAIN), str(col(after, 5, 3)))
    check('2D: keepTile keeps the tile (6,3 is still the water)', any(near(col(after, 6, 3), TS.tile_colour(n)) for n in (8, 9, 10)), str(col(after, 6, 3)))
    check('2D: setTerrain with a tile (4,2 -> tile 1)', near(col(after, 4, 2), TS.tile_colour(1)), str(col(after, 4, 2)))
    check('2D: squares outside the change are as before', near(col(after, 0, 0), col(before, 0, 0)) and near(col(after, 8, 2), col(before, 8, 2)))
    check('2D: no complaint about the layer in the log', 'tileLayer' not in blog and 'error' not in blog.lower(), blog[-300:])


def test_views():
    make_game('gviews')
    for style, label in (('iso', 'iso2d'), ('iso3', 'iso3d'), ('full', 'full3d')):
        before, blog = shot('%s_before' % label, 'gviews', inp(IDLE, 'wait:10'), style)
        after, alog = shot('%s_after' % label, 'gviews', inp(END_TURN), style)
        check('%s: round 2 began and the map changed' % label, 'round 2' in alog and 'map changed' in alog, alog[-300:])
        check('%s: the picture changed where the change is' % label, before.differs(after) > 150, str(before.differs(after)))
        # The picture is full of tile colours: count pixels near each of the 32 tile colours (every tile on the map is visible somewhere
        # in the 2D iso view; the 3D views are lit and viewed at an angle, so only a few are counted).
        hits = set()
        for row in before.px:
            for p in row:
                for n in range(32):
                    if near(p, TS.tile_colour(n), 4):
                        hits.add(n)
        check('%s: tile colours are on screen (%d of the tiles seen)' % (label, len(hits)), len(hits) >= 6, str(sorted(hits)))
        check('%s: no complaint in the log' % label, 'error' not in (blog + alog).lower(), (blog + alog)[-300:])


def count_near(img, color, tol=5):
    return sum(1 for row in img.px for p in row if near(p, color, tol))


def test_standing_terrain():
    """A square with a tile shows no standing part (docs/GRAPHICS.md "Tile layer"): plain has an upright picture (tile 1's cell, the
    only place that colour appears before round 2) in the 3D isometric style; with the layer only the two squares without a tile
    have it, without the layer every plain square does."""
    stand = {'iso3': {'terrain': {'plain': {'sheet': 'tiles', 'cell': [0, 3], 'stand': {'sheet': 'tiles', 'cell': [1, 0], 'width': 24}}}}}

    def build(gid, with_layer):
        g = make_game(gid)
        gr = read_json(g + '/data/graphics.json')
        gr['styles'] = stand
        write_json(g + '/data/graphics.json', gr)
        if not with_layer:
            path = g + '/maps/tiled.json'
            text = open(path).read().replace('"tileLayer"', '"unusedLayer"').replace(', "tile": 5', '').replace(', "keepTile": true', '').replace(', "tile": 1', '')
            open(path, 'w').write(text)
    build('gstand_layer', True)
    build('gstand_plain', False)
    with_layer, _ = shot('iso3d_stand_with_layer', 'gstand_layer', inp(IDLE, 'wait:10'), 'iso3')
    without, _ = shot('iso3d_stand_without_layer', 'gstand_plain', inp(IDLE, 'wait:10'), 'iso3')
    a, b = count_near(with_layer, TS.tile_colour(1)), count_near(without, TS.tile_colour(1))
    check('3D: squares with a tile have no standing part; those without one do (%d vs %d pixels)' % (a, b), 0 < a * 6 < b, '%d vs %d' % (a, b))


def test_portrait():
    make_game('gport', view=(270, 480))
    before, blog = shot('top_portrait_before', 'gport', inp(IDLE, 'wait:10'), 'top')
    after, alog = shot('top_portrait_after', 'gport', inp(END_TURN), 'top')
    check('portrait: the picture is 270 x 480', (before.w, before.h) == (270, 480), '%dx%d' % (before.w, before.h))
    check('portrait: the map change shows', before.differs(after) > 100, str(before.differs(after)))


def main():
    test_2d()
    test_views()
    test_standing_terrain()
    test_portrait()
    print('%d passed, %d failed' % (passed, len(failures)))
    shutil.rmtree(TMP, ignore_errors=True)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
