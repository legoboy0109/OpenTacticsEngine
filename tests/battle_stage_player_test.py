#!/usr/bin/env python3
"""The fixed battle stage through the real player, windowless (docs/BATTLE_SCENE.md "Fixed stage").

  python3 tests/battle_stage_player_test.py ./build/engine/player/ote-player .
  python3 tests/battle_stage_player_test.py ./build/engine/player/ote-player . --shots OUT_DIR   # keep the screenshots (BMP)

Makes a copy of tests/fixtures/srpg-quotes with original generated test art: a 240 x 160 backdrop with a grid and a coloured mark in
each corner, platform pictures with a red mark at one end and a blue one at the other (so a mirrored one is told from a plain one),
and a fighter whose two "foot" pixels are magenta. Plays a close fight and a ranged fight with "battleSceneSides": "playerRight" at
480 x 270 and 270 x 480, takes a screenshot of each and checks marker pixels: the backdrop is drawn at one whole-number scale,
each fighter's foot pixels sit exactly on the declared foot point, the platforms are where they were declared (the right one
mirrored, or its own picture), the player is on the right, the range line can be hidden.
"""
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
SHOTS = os.path.abspath(sys.argv[sys.argv.index('--shots') + 1]) if '--shots' in sys.argv else ''
TMP = tempfile.mkdtemp(prefix='ote-bstage-')
passed, failures = 0, []

sys.path.insert(0, os.path.join(REPO, 'tools', 'art'))
import battle_art  # noqa: E402  (our own generator: original pixel art, no Pillow)

Canvas, write_png = battle_art.Canvas, battle_art.write_png

# Marker colours (exact matches only).
C_TL, C_TR, C_BL, C_BR = (255, 255, 0), (0, 255, 255), (0, 255, 0), (255, 128, 0)  # the backdrop's corners
C_END_A, C_END_B = (255, 0, 0), (0, 0, 255)                                         # a platform's two ends
C_OWN = (255, 255, 255)                                                             # the right platform's own picture
C_FOOT = (255, 0, 255)                                                              # a fighter's two foot pixels
C_BLUE_BODY, C_RED_BODY = (30, 60, 200), (200, 40, 40)
C_GRID = (180, 190, 210)

STAGE = {'width': 240, 'height': 160,
         'feet': {'left': [88, 104], 'right': [152, 104]}, 'feetFar': {'left': [56, 104], 'right': [184, 104]},
         'platform': {'left': [56, 96], 'right': [120, 96]}, 'platformFar': {'left': [24, 96], 'right': [152, 96]},
         'align': 'bottom'}
PLAT = {'sheet': 'plat', 'cell': [0, 0], 'far': [1, 0]}


# ---- generated test art -------------------------------------------------------------------------------------------------

def backdrop():
    c = Canvas(240, 160)
    for y in range(160):
        for x in range(240):
            c.set(x, y, (24, 30, 60 + y // 4, 255))
    for x in range(0, 240, 20):  # a grid: one pixel lines every 20 stage pixels
        c.rect(x, 0, x, 159, C_GRID + (255,))
    for y in range(0, 160, 20):
        c.rect(0, y, 239, y, C_GRID + (255,))
    for (x0, y0, col) in ((0, 0, C_TL), (236, 0, C_TR), (0, 156, C_BL), (236, 156, C_BR)):
        c.rect(x0, y0, x0 + 3, y0 + 3, col + (255,))
    return c


def platforms():
    sheet = Canvas(256, 32)
    for i, body in enumerate(((200, 120, 40), (60, 160, 70), (140, 60, 170), (60, 120, 170))):
        sheet.rect(i * 64, 0, i * 64 + 63, 31, body + (255,))
        if i < 2:  # asymmetric: red at the left end, blue at the right end (the mirrored one swaps them)
            sheet.rect(i * 64, 0, i * 64 + 3, 3, C_END_A + (255,))
            sheet.rect(i * 64 + 60, 0, i * 64 + 63, 3, C_END_B + (255,))
        else:
            sheet.rect(i * 64, 0, i * 64 + 3, 3, C_OWN + (255,))
    return sheet


def fighter(body):
    c = Canvas(48, 48)
    c.rect(16, 6, 31, 45, body + (255,))
    c.rect(18, 0, 29, 6, (232, 192, 150, 255))
    c.rect(23, 46, 24, 46, C_FOOT + (255,))  # the foot point (24, 46): the two pixels either side of it
    return c


def make_game(gid, *tweaks, stage=None, plat=None, sides='playerRight', backdrop_size=None):
    g = os.path.join(TMP, gid)
    shutil.copytree(os.path.join(REPO, 'tests/fixtures/srpg-quotes'), g)
    os.makedirs(os.path.join(g, 'art'))
    write_png(os.path.join(g, 'art', 'stage_bg.png'), backdrop())
    write_png(os.path.join(g, 'art', 'plat.png'), platforms())
    write_png(os.path.join(g, 'art', 'blue.png'), fighter(C_BLUE_BODY))
    write_png(os.path.join(g, 'art', 'red.png'), fighter(C_RED_BODY))
    anims = lambda sheet: {k: {'sheet': sheet, 'frames': [[0, 0]], 'ms': 100}  # one picture for everything: a screenshot never depends on timing
                           for k in ('stand', 'attack', 'crit', 'ranged', 'cast', 'dodge', 'hurt', 'defeat', 'miss')}
    gfx = {'sheets': [{'id': 'stage_bg', 'file': 'art/stage_bg.png', 'cellWidth': 240, 'cellHeight': 160},
                      {'id': 'plat', 'file': 'art/plat.png', 'cellWidth': 64, 'cellHeight': 32},
                      {'id': 'blue', 'file': 'art/blue.png', 'cellWidth': 48, 'cellHeight': 48},
                      {'id': 'red', 'file': 'art/red.png', 'cellWidth': 48, 'cellHeight': 48}],
           'battle': {'sets': {'blue': {'anchor': [24, 46], 'animations': anims('blue')},
                               'red': {'anchor': [24, 46], 'animations': anims('red')}},
                      'assign': [{'set': 'blue', 'side': 'blue'}, {'set': 'red', 'side': 'red'}],
                      'backgrounds': {'arena': {'sheet': 'stage_bg', 'cell': [0, 0]}}, 'defaultBackground': 'arena',
                      'platforms': {'slab': plat or PLAT}, 'defaultPlatform': 'slab',
                      'stage': stage or STAGE}}
    write_json(os.path.join(g, 'data', 'graphics.json'), gfx)
    edit(g, 'game.json', lambda gj: gj.update({'battleScene': 'scene', 'battleSceneSides': sides}))
    edit(g, 'data/characters.json', lambda c: [ch.pop('quotes', None) for ch in c['characters']])
    for t in tweaks:
        t(g)
    return g


def read_json(path):
    with open(path) as f:
        return json.loads(''.join(l for l in f if not l.lstrip().startswith('//')))


def write_json(path, obj):
    with open(path, 'w') as f:
        json.dump(obj, f, indent=1)


def edit(g, rel, fn):
    path = os.path.join(g, rel)
    obj = read_json(path)
    fn(obj)
    write_json(path, obj)


def ranged(g):  # the sword reaches two squares; Gorm stands two squares from Eirik
    edit(g, 'data/weapons.json', lambda w: w['weapons'][0].update({'range': [1, 2]}))
    edit(g, 'maps/ch1.json', lambda m: [u.update({'x': 5, 'y': 1}) for u in m['units'] if u.get('character') == 'gorm'])
    edit(g, 'maps/ch1.json', lambda m: [u.update({'x': 6, 'y': 0}) for u in m['units'] if u.get('character') == 'tolly'])


def phone(g):
    edit(g, 'game.json', lambda gj: gj.update({'view': {'width': 270, 'height': 480, 'tileSize': 24}}))


IDLE = 'wait:14'
ATTACK = [IDLE, 'confirm', 'wait:3', 'confirm', 'wait:3', 'confirm', 'wait:3', 'confirm', 'wait:400']


def run(gid, shot, frames=175):
    work = os.path.join(TMP, 'saves_' + gid)
    shutil.rmtree(work, ignore_errors=True)
    cmd = [PLAYER, '--games', TMP, '--assets', os.path.join(REPO, 'assets'), '--headless', '--no-audio', '--no-settings', '--saves', work,
           '--frames', str(frames), '--input-interval', '4', '--fixed-dt', '0.016', '--game', gid, '--map', 'ch1',
           '--input', ','.join(ATTACK), '--screenshot', shot]
    p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True, timeout=300)
    return p.stdout


# ---- reading a screenshot -----------------------------------------------------------------------------------------------

class Shot:
    def __init__(self, path):
        with open(path, 'rb') as f:
            d = f.read()
        off, = struct.unpack_from('<I', d, 10)
        w, h, planes, bpp = struct.unpack_from('<iiHH', d, 18)
        self.bottom_up = h > 0
        self.w, self.h = w, abs(h)
        self.bpp = bpp // 8
        self.stride = (w * self.bpp + 3) // 4 * 4
        self.d, self.off = d, off

    def px(self, x, y):
        if not (0 <= x < self.w and 0 <= y < self.h):
            return None
        row = self.h - 1 - y if self.bottom_up else y
        i = self.off + row * self.stride + x * self.bpp
        return (self.d[i + 2], self.d[i + 1], self.d[i])  # BGR(A) -> RGB

    def bbox(self, col, x0=0, x1=None, y0=0, y1=None):
        """The box of every pixel of exactly this colour inside the window (None when there is none)."""
        xs, ys = [], []
        for y in range(y0, self.h if y1 is None else y1):
            for x in range(x0, self.w if x1 is None else x1):
                if self.px(x, y) == col:
                    xs.append(x)
                    ys.append(y)
        return (min(xs), min(ys), max(xs) + 1, max(ys) + 1) if xs else None


def check(name, cond, detail=''):
    global passed
    if cond:
        passed += 1
        print('PASS ' + name)
    else:
        failures.append(name)
        print('FAIL %s %s' % (name, detail))


def keep(path, name):
    if SHOTS and os.path.exists(path):
        os.makedirs(SHOTS, exist_ok=True)
        shutil.copy(path, os.path.join(SHOTS, name + '.bmp'))


def expect_scene(name, gid, W, H, far, k, own_right=False, range_line=True, plat_far_pics=False):
    """Plays the fight and checks the picture. `k` = the expected scale; the stage's place is read from the backdrop's own corner marks."""
    path = os.path.join(TMP, name + '.bmp')
    log = run(gid, path)
    keep(path, name)
    check(name + ': the scene played (%s)' % ('ranged' if far else 'melee'), 'battle scene: start' in log and (('ranged' in log.split('battle scene: start')[1].split('\n')[0]) == far), log[-300:])
    if not os.path.exists(path):
        check(name + ': screenshot exists', False)
        return
    s = Shot(path)
    check(name + ': screenshot is %d x %d' % (W, H), (s.w, s.h) == (W, H), '%d x %d' % (s.w, s.h))
    # The backdrop: corner marks are 4 x 4 stage pixels = 4k on the screen; read the place from the bottom-left one (it is the one in view).
    bl, br = s.bbox(C_BL), s.bbox(C_BR)
    check(name + ': the backdrop\'s bottom corners are in view', bl is not None and br is not None, str((bl, br)))
    if bl is None or br is None:
        return
    check(name + ': backdrop drawn at scale %d (corner mark %d px wide)' % (k, 4 * k), bl[2] - bl[0] == 4 * k, str(bl))
    ox, oy = bl[0], bl[3] - 160 * k
    check(name + ': the stage is 240 x k wide and centred across (bottom-right mark)', br[2] == ox + 240 * k and ox == (W - 240 * k) // 2, str((ox, br)))
    tl = s.bbox(C_TL)
    if tl:
        check(name + ': the top-left mark sits on the stage origin', tl[:2] == (ox, oy), str((tl, ox, oy)))
    else:
        check(name + ': (the top rows are cropped: no top-left mark, as aligned "bottom")', oy < 0, str((ox, oy)))
    # Grid lines (width k) every 20 stage pixels, nothing between.
    sy = oy + 70 * k  # a stage row between two grid lines, clear of the fighters and the boxes
    lx = ox + 20 * k + k // 2
    check(name + ': a grid line is k pixels wide at the right place', s.px(lx, sy) == C_GRID and s.px(ox + 20 * k - 1, sy) != C_GRID and s.px(ox + 21 * k, sy) != C_GRID,
          str((s.px(lx, sy), s.px(ox + 20 * k - 1, sy))))
    # Feet: each fighter's two foot pixels sit on the foot point (a 2k x k block), left and right.
    feet = STAGE['feetFar' if far else 'feet']
    for side, x0, x1 in (('left', 0, W // 2), ('right', W // 2, W)):
        fx, fy = ox + feet[side][0] * k, oy + feet[side][1] * k
        bb = s.bbox(C_FOOT, x0, x1)
        check(name + ': the %s fighter\'s feet are exactly on its foot point (%d, %d)' % (side, fx, fy), bb == (fx - k, fy, fx + k, fy + k), str(bb))
    # The player (Eirik, blue) is on the right with "playerRight".
    rx, ry = ox + feet['right'][0] * k, oy + feet['right'][1] * k
    lx2, ly2 = ox + feet['left'][0] * k, oy + feet['left'][1] * k
    check(name + ': the player\'s fighter (blue) stands on the right, the computer\'s (red) on the left',
          s.px(rx, ry - 10 * k) == C_BLUE_BODY and s.px(lx2, ly2 - 10 * k) == C_RED_BODY, str((s.px(rx, ry - 10 * k), s.px(lx2, ly2 - 10 * k))))
    # Platforms: where declared; the right one is the left turned round (red / blue ends swap) or its own picture.
    plat = STAGE['platformFar' if far else 'platform']
    px0, py0 = ox + plat['left'][0] * k, oy + plat['left'][1] * k
    qx0, qy0 = ox + plat['right'][0] * k, oy + plat['right'][1] * k
    at = lambda bx, by, dx, dy: s.px(bx + dx * k + k // 2, by + dy * k + k // 2)
    check(name + ': the left platform is at its declared place (red left end, blue right end)',
          at(px0, py0, 1, 1) == C_END_A and at(px0, py0, 62, 1) == C_END_B, str((at(px0, py0, 1, 1), at(px0, py0, 62, 1))))
    if own_right:
        check(name + ': the right platform is its own picture (not mirrored)', at(qx0, qy0, 1, 1) == C_OWN and at(qx0, qy0, 62, 1) not in (C_END_A, C_END_B),
              str((at(qx0, qy0, 1, 1), at(qx0, qy0, 62, 1))))
    else:
        check(name + ': the right platform is the left one mirrored (blue left end, red right end)',
              at(qx0, qy0, 1, 1) == C_END_B and at(qx0, qy0, 62, 1) == C_END_A, str((at(qx0, qy0, 1, 1), at(qx0, qy0, 62, 1))))
    check(name + ': nothing of the platform above its declared top row', at(px0, py0, 1, -2) != C_END_A)
    # The dotted range line sits just under the feet, between them: look for its pixels (light, semi-transparent) in the row below.
    if far:
        y = oy + feet['left'][1] * k + max(3, k * 2)
        # (the backdrop's own grid lines are light too: leave their columns out)
        row = [s.px(x, y) for x in range(lx2 + 6 * k, rx - 6 * k) if (x - ox) % (20 * k) >= k]
        lit = sum(1 for p in row if p and sum(p) > 420)
        check(name + ': the range line is %s' % ('drawn' if range_line else 'hidden (rangeLine false)'), (lit > 5) == range_line, str(lit))
    return s


def main():
    make_game('close')
    make_game('far', ranged)
    make_game('close_p', phone)
    make_game('far_p', ranged, phone)
    make_game('own', plat={'sheet': 'plat', 'cell': [0, 0], 'far': [1, 0], 'right': [2, 0], 'farRight': [3, 0]})
    make_game('norange', ranged, stage=dict(STAGE, rangeLine=False))
    expect_scene('stage-landscape-close', 'close', 480, 270, False, 2)
    expect_scene('stage-landscape-ranged', 'far', 480, 270, True, 2)
    expect_scene('stage-portrait-close', 'close_p', 270, 480, False, 1)
    expect_scene('stage-portrait-ranged', 'far_p', 270, 480, True, 1)
    expect_scene('stage-landscape-own-right-platform', 'own', 480, 270, False, 2, own_right=True)
    expect_scene('stage-landscape-ranged-no-line', 'norange', 480, 270, True, 2, range_line=False)
    # "contain": the whole stage in view, k = 1 on the 480 x 270 screen with bars around it.
    make_game('contain', stage=dict(STAGE, fit='contain'))
    s = expect_scene('stage-landscape-contain', 'contain', 480, 270, False, 1)
    tl = s.bbox(C_TL) if s else None
    check('contain: the whole stage is visible (the top-left mark is in view)', tl is not None)


try:
    main()
finally:
    shutil.rmtree(TMP, ignore_errors=True)

print('%d passed, %d failed' % (passed, len(failures)))
sys.exit(1 if failures else 0)
