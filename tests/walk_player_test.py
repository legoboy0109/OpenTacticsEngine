#!/usr/bin/env python3
"""Walking frames of unit pictures through the real player, windowless (docs/GRAPHICS.md "Walking").

  python3 tests/walk_player_test.py ./build/engine/player/ote-player .
  python3 tests/walk_player_test.py ./build/engine/player/ote-player . --shots OUT_DIR     # keep the mid-move screenshots (BMP)

Copies tests/fixtures/srpg-quotes into a temp folder, gives it a U-shaped map (the hero's only way from the top left to the top
middle is down, right, up) and original art made by tools/art/battle_art.py: a 16 x 16 standing figure and a 32 x 32 walking
figure (a different shirt colour per direction, so a picture shows which row was drawn). Runs the player headless (no audio, no window,
no settings, saves in the temp folder) with OTE_LOG_LEVEL=debug, which makes the drawing log "walk: NAME DIRECTION" once per change
of direction (nothing at the normal log level):

  - 2D top-down: the directions down, right, up (a step down the map is down on the screen), the walking picture is drawn
    from the feet at twice the standing size (its cells are twice as big), and the unit stands again after the move;
  - 2D isometric: a step down the map goes down-left on the screen, a step right goes down-right, a step up goes up-right: the
    nearest of the given directions on the SCREEN (left, right, right);
  - a unit picture without walk frames: no walk line at all, the standing picture all along.
"""
import json
import os
import shutil
import struct
import subprocess
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import (  # noqa: E402
    run_text, check, failures, player_and_repo, player_cmd, read_jsonc, shots_dir, summary, write_json)
from ote_testkit import scratch_dir  # noqa: E402
read_json = read_jsonc  # this script reads fixtures and games whose JSON has `//` comment lines

sys.dont_write_bytecode = True

PLAYER, REPO = player_and_repo()
SHOTS = shots_dir(absolute=True)
TMP = scratch_dir()

sys.path.insert(0, os.path.join(REPO, 'tools', 'art'))
import battle_art  # noqa: E402  (our own generator: original pixel art, no Pillow)

DOWN, UP, RIGHT = battle_art.SHIRTS['down'], battle_art.SHIRTS['up'], battle_art.SHIRTS['right']
STAND = (200, 60, 200, 255)   # the standing figure's shirt (a colour the walking figure does not have)
# One move: select the hero (the cursor starts on it), two squares right, confirm. The path is down, down, right, right, up, up.
MOVE = ['wait:14', 'confirm', 'wait:3', 'right', 'right', 'confirm']


def read_bmp(path):
    data = open(path, 'rb').read()
    offset = struct.unpack_from('<I', data, 10)[0]
    width, height = struct.unpack_from('<ii', data, 18)
    bpp = struct.unpack_from('<H', data, 28)[0] // 8
    stride = (width * bpp + 3) & ~3
    rows = [data[offset + r * stride: offset + r * stride + width * bpp] for r in range(abs(height))]
    if height > 0:
        rows.reverse()
    return width, abs(height), rows, bpp


def pixels_of(path, rgb):
    """(count, min x, min y, max x, max y) of the pixels of exactly this colour in a BMP screenshot."""
    w, h, rows, bpp = read_bmp(path)
    want = bytes((rgb[2], rgb[1], rgb[0]))  # BMP stores blue, green, red
    n, x0, y0, x1, y1 = 0, 10 ** 9, 10 ** 9, -1, -1
    for y, row in enumerate(rows):
        for x in range(w):
            if row[x * bpp:x * bpp + 3] == want:
                n += 1
                x0, y0, x1, y1 = min(x0, x), min(y0, y), max(x1, x), max(y1, y)
    return n, x0, y0, x1, y1


def standing_sheet():
    """units.png: two 16 x 16 cells (the hero and the brute), each a little figure with a shirt of the STAND colour."""
    sheet = battle_art.Canvas(32, 16)
    for i, shirt in enumerate((STAND, (60, 60, 60, 255))):
        fig = battle_art.Canvas(16, 16)
        fig.rect(5, 6, 10, 11, shirt)
        fig.rect(5, 1, 10, 5, battle_art.SKIN)
        fig.rect(5, 12, 10, 14, (60, 60, 90, 255))
        fig.outline()
        sheet.paste(fig, i * 16, 0)
    return sheet


def look(extra=None):
    cell = {'sheet': 'units', 'cell': [0, 0]}
    if extra:
        cell['walk'] = extra
    return cell


WALK = {'sheet': 'walk', 'down': [[c, 0] for c in range(4)], 'up': [[c, 1] for c in range(4)], 'side': [[c, 2] for c in range(4)], 'frameMs': 90}


def make_game(gid, walk=True, view=None):
    g = os.path.join(TMP, gid)
    shutil.copytree(os.path.join(REPO, 'tests/fixtures/srpg-quotes'), g)
    os.remove(os.path.join(g, 'data', 'campaign.json'))  # a single battle
    os.makedirs(os.path.join(g, 'art'))
    battle_art.main(os.path.join(g, 'art'))              # walk.png (and the battle art, not used here)
    battle_art.write_png(os.path.join(g, 'art', 'units.png'), standing_sheet())
    gj = read_json(os.path.join(g, 'game.json'))
    gj['styles'] = [{'id': 'classic', 'name': 'Classic'}, {'id': 'iso', 'name': 'Isometric', 'view': 'isometric'}]
    if view:
        gj['view'] = view
    write_json(os.path.join(g, 'game.json'), gj)
    units = read_json(os.path.join(g, 'data/units.json'))
    for u in units['units']:
        if u['id'] == 'hero':
            u['stats']['mov'] = 8
    write_json(os.path.join(g, 'data/units.json'), units)
    # A U-shaped field: walls at (1,0) and (1,1), so the way from (0,0) to (2,0) is down, down, right, right, up, up.
    write_json(os.path.join(g, 'maps/ch1.json'), {
        'name': 'Field', 'grid': {'type': 'square', 'width': 5, 'height': 3}, 'legend': {'.': 'plain', '#': 'wall'},
        'tiles': ['.#...', '.#...', '.....'],
        'sides': [{'id': 'blue', 'controller': 'human'}, {'id': 'red', 'controller': 'ai'}],
        'units': [{'type': 'hero', 'side': 'blue', 'x': 0, 'y': 0, 'leader': True},
                  {'type': 'brute', 'side': 'red', 'x': 4, 'y': 2, 'leader': True}]})
    hero, brute = look(WALK if walk else None), {'sheet': 'units', 'cell': [1, 0]}
    write_json(os.path.join(g, 'data/graphics.json'), {
        'sheets': [{'id': 'units', 'file': 'art/units.png', 'cellWidth': 16, 'cellHeight': 16},
                   {'id': 'walk', 'file': 'art/walk.png', 'cellWidth': 32, 'cellHeight': 32}],
        'unitMarker': False,
        'units': {'hero': hero, 'brute': brute},
        'styles': {'iso': {'units': {'hero': hero, 'brute': brute}}}})
    return g


def run(gid, inp, style=None, frames=300, shot=None, interval=4):
    work = os.path.join(TMP, 'saves_' + gid)
    shutil.rmtree(work, ignore_errors=True)
    cmd = player_cmd(PLAYER, TMP, REPO) + ['--saves', work, '--frames', str(frames), '--input-interval', str(interval), '--fixed-dt', '0.016',
           '--game', gid, '--map', 'ch1', '--input', ','.join(inp)]
    if style:
        cmd += ['--style', style]
    if shot:
        cmd += ['--screenshot', shot]
    env = dict(os.environ, OTE_LOG_LEVEL='debug')
    p = run_text(cmd, env=env)
    return p.stdout


def walks(log):
    return [l.split('walk: ', 1)[1] for l in log.splitlines() if '[debug] walk: ' in l]


def mid_move(gid, style, want_last):
    """The first frame count at which the log's last walk line is `want_last` (the screenshot is taken at the end of the run)."""
    for frames in range(40, 200, 2):
        if walks(run(gid, MOVE, style, frames))[-1:] == ['Hero ' + want_last]:
            return frames
    return None


def keep(path, name):
    if SHOTS and os.path.exists(path):
        os.makedirs(SHOTS, exist_ok=True)
        shutil.copy(path, os.path.join(SHOTS, name + '.bmp'))


def test_directions_2d():
    make_game('w_flat')
    log = run('w_flat', MOVE)
    check('2D: the directions follow the path, one line per change', walks(log) == ['Hero down', 'Hero right', 'Hero up'], str(walks(log)) + log[-300:])
    check('2D: debug lines are not shown at the normal log level', not walks(subprocess.run(
        player_cmd(PLAYER, TMP, REPO) + ['--saves',
         os.path.join(TMP, 'saves_w_flat'), '--frames', '200', '--input-interval', '4', '--fixed-dt', '0.016', '--game', 'w_flat', '--map', 'ch1',
         '--input', ','.join(MOVE)], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True).stdout))


def test_directions_iso():
    make_game('w_iso')
    log = run('w_iso', MOVE, style='iso')
    check('2D isometric: down the map is left on the screen, right and up the map are right', walks(log) == ['Hero left', 'Hero right'], str(walks(log)) + log[-300:])


def test_no_walk_frames():
    make_game('w_none', walk=False)
    log = run('w_none', MOVE)
    check('no walk frames: no walk line, the standing picture all along', walks(log) == [] and 'error' not in log.lower(), log[-300:])


def test_screenshots_and_scale():
    big = {'width': 960, 'height': 540, 'tileSize': 32}   # a desktop-sized screen: standing 16 px -> 32 px, walking 32 px -> 64 px
    for style, label in ((None, '2d'), ('iso', 'iso')):
        gid = 'w_shot_' + label
        make_game(gid, view=big)
        frames = mid_move(gid, style, 'right')
        check('%s: found a moment in the middle of the move (a "right" step)' % label, frames is not None)
        if frames is None:
            continue
        path = os.path.join(TMP, gid + '.bmp')
        run(gid, MOVE, style, frames, shot=path)
        keep(path, 'walk-%s-mid-move' % label)
        n, x0, y0, x1, y1 = pixels_of(path, RIGHT)
        check('%s: the walking picture (side frames) is on the screen, the standing shirt is not' % label,
              n > 50 and pixels_of(path, STAND)[0] == 0, 'right-shirt pixels %d, standing %d' % (n, pixels_of(path, STAND)[0]))
        # The shirt is 7 x 11 art pixels (13..19, 12..22 in the 32 x 32 cell); drawn at 2 pixels per art pixel.
        check('%s: drawn at twice the standing picture\'s scale (a 7 x 11 shirt is 14 x 22 pixels)' % label,
              (x1 - x0 + 1, y1 - y0 + 1) == (14, 22), 'shirt box %d x %d' % (x1 - x0 + 1, y1 - y0 + 1))
        done = os.path.join(TMP, gid + '_end.bmp')
        run(gid, MOVE, style, 260, shot=done)
        keep(done, 'walk-%s-after-move' % label)
        check('%s: after the move the standing picture is back' % label,
              pixels_of(done, STAND)[0] > 20 and sum(pixels_of(done, c)[0] for c in (DOWN, UP, RIGHT)) == 0)


try:
    test_directions_2d()
    test_directions_iso()
    test_no_walk_frames()
    test_screenshots_and_scale()
finally:
    shutil.rmtree(TMP, ignore_errors=True)

summary()
sys.exit(1 if failures else 0)
