#!/usr/bin/env python3
"""The player's part of docs/SRPG.md "Rescue and pair up" (step U4), windowless.

  python3 tests/srpg_player_rescue_test.py ./build/engine/player/ote-player .
  python3 tests/srpg_player_rescue_test.py ./build/engine/player/ote-player . --shots OUT_DIR   # also keeps the screenshots (BMP)

Writes two small games (tests/srpg_rescue_games.py: carry mode and pair up mode) and drives the real player with --input
scripts, reading the menu / pick lines it logs, the Continue save (--saves on a temp folder) and pixels of the screenshot:

  - the entries of the action menu, in the contract's order, with Drop greyed when there is no room ("no room");
  - Rescue, Drop, Give, Take, Pair up and Switch through the menus and the two pick modes, judged by the save;
  - the carrier marker: in every kind of view (top-down, 2D isometric, 3D isometric, full 3D, and hexagons in two of
    them) the picture of a unit with a partner differs from the same picture without the partner only by a small tab at
    the unit's top right corner; on the top-down map it is exactly the contract's rectangle;
  - the panel row "Carrying: Bram" / "With: Ann" appears when the cursor is on such a unit;
  - a phone-shaped window plays the same menus.
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import srpg_rescue_games as games  # noqa: E402
from hex_player_test import Image  # noqa: E402  (the BMP reader)

PLAYER = REPO = TMP = GAMES = SHOTS = ''
failures = []
passed = 0
IDLE = 'wait:14'                 # the turn banner takes the first inputs otherwise
SELECT = 'confirm,confirm'       # select the first blue unit (Ann, or Bram once Ann is carried) and open its menu
END_TURN = 'menu,confirm,wait:60'  # the battle menu's End Turn, then red's turn (its one unit cannot reach anybody)
FIRST = 'confirm,confirm,wait:20'  # the first entry of the menu, then the first candidate; the animation plays
FAR = 'up,up,right,right,right,right'  # the cursor to the top right corner: no highlight on the units
PHONE = (270, 480)
TILE = 24
VIEW_W, VIEW_H = 352, 259        # the map area of the 480 x 270 window (the panel is 128 wide, the hint strip 11 high)
BLUE = (0x3d, 0x85, 0xc6)


def check(name, cond, detail=''):
    global passed
    if cond:
        passed += 1
        print('PASS ' + name)
    else:
        failures.append(name)
        print('FAIL %s %s' % (name, detail))


def frames_for(inp, interval=4, slack=40):
    n = 0
    for tok in inp.split(','):
        n += int(tok[5:]) * interval if tok.startswith('wait:') else interval
    return n + slack


def run(game, inp, style='classic', map_id='open', saves=None, shot=None):
    """Runs the player headless; returns the log text."""
    cmd = [PLAYER, '--games', GAMES, '--assets', os.path.join(REPO, 'assets'), '--headless', '--no-audio', '--no-settings',
           '--frames', str(frames_for(inp)), '--input-interval', '4', '--fixed-dt', '0.016', '--game', game, '--map', map_id,
           '--style', style, '--input', inp]
    cmd += ['--saves', saves] if saves else ['--no-saving']
    if style in ('iso3d', 'full3d'):
        cmd += ['--3d', 'software']
    if shot:
        cmd += ['--screenshot', shot]
    p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True, timeout=300)
    return p.stdout


def picture(name, game, inp, style='classic', map_id='open'):
    path = os.path.join(TMP, name + '.bmp')
    log = run(game, inp, style, map_id, shot=path)
    if SHOTS:
        shutil.copy(path, os.path.join(SHOTS, name + '.bmp'))
    return Image(path), log


def played(name, game, inp, map_id='open'):
    """Runs with a Continue save; returns (log, the units of the save, the screenshot)."""
    saves = os.path.join(TMP, 'saves_' + name)
    shutil.rmtree(saves, ignore_errors=True)
    path = os.path.join(TMP, name + '.bmp')
    log = run(game, inp, 'classic', map_id, saves=saves, shot=path)
    if SHOTS:
        shutil.copy(path, os.path.join(SHOTS, name + '.bmp'))
    sf = os.path.join(saves, game, 'continue.json')
    units = json.load(open(sf))['battle']['units'] if os.path.exists(sf) else []
    return log, units, Image(path)


def entries(log, who):
    """The rescue entries logged for the menu opened for `who`, in order (last one wins)."""
    out = None
    for line in log.splitlines():
        if line.startswith('[info] rescue entries of %s: ' % who):
            out = line.split(': ', 1)[1]
    return out


def lines(img, x0, x1, y0, y1, bright=150):
    """The bands of rows that hold light grey text pixels in a box: how many text lines the panel shows."""
    bands, inside = 0, False
    for y in range(y0, y1):
        on = any(min(img.px(x, y)) > bright for x in range(x0, x1))
        if on and not inside:
            bands += 1
        inside = on
    return bands


def diff_box(a, b, w=VIEW_W, h=VIEW_H - 4, tol=40):
    """(count, x0, y0, x1, y1) of the pixels of the map area that differ between two pictures."""
    n, x0, y0, x1, y1 = 0, 10 ** 6, 10 ** 6, -1, -1
    for y in range(h):
        for x in range(w):
            p, q = a.px(x, y), b.px(x, y)
            if max(abs(p[i] - q[i]) for i in range(3)) > tol:
                n += 1
                x0, y0, x1, y1 = min(x0, x), min(y0, y), max(x1, x), max(y1, y)
    return n, x0, y0, x1, y1


def cam():
    """The camera of the small 7 x 5 map in the top-down view: centred (the player rounds half up)."""
    import math
    mw, mh = games.MAP_W * TILE, games.MAP_H * TILE
    return math.floor(-(VIEW_W - mw) / 2.0 + 0.5), math.floor(-(VIEW_H - mh) / 2.0 + 0.5)


# ---- scenarios

def carry_flow():
    log, units, img = played('carry_rescue', 'carry', IDLE + ',' + SELECT + ',confirm,confirm,wait:30')
    check('carry: the first menu offers Rescue', entries(log, 'Ann') == 'Rescue', entries(log, 'Ann'))
    check('carry: the pick lists the two neighbours', 'pick unit: 2 candidates' in log)
    check('carry: Rescue puts Bram on Ann (save)', units and units[0].get('carrying') == 1 and not units[1]['alive']
          and units[1]['cell'] == -1 and units[1]['hp'] == 100 and units[0]['done'], str(units[:2]))
    log, units, _ = played('carry_drop', 'carry', ','.join([IDLE, SELECT, FIRST, END_TURN, SELECT, 'confirm,confirm,wait:30']))
    check('carry: with a unit the menu offers Drop then Give', entries(log, 'Ann') == 'Drop, Give', entries(log, 'Ann'))
    check('carry: Drop lists the free squares around', 'pick square: 3 candidates' in log)
    ann, bram = units[0], units[1]
    # the first of the squares around (2,2) in cell order: (2,1) = 9, (1,2) = 15, (3,2) = 17 (the carried one's own is free too)
    check('carry: Drop stands the unit on a free square with its turn done', 'carrying' not in ann and bram['alive']
          and bram['cell'] in (9, 15, 17) and bram['done'], str((ann, bram)))
    log, units, _ = played('carry_give', 'carry', ','.join([IDLE, SELECT, FIRST, END_TURN, SELECT, 'down,confirm,confirm,wait:30']))
    check('carry: Give hands it on and the menu comes back with Take', entries(log, 'Ann') == 'Take', entries(log, 'Ann'))
    check('carry: Give moved it to Cleo (save)', 'carrying' not in units[0] and units[2].get('carrying') == 1, str(units[:3]))
    log, units, _ = played('carry_take', 'carry', ','.join([IDLE, SELECT, FIRST, END_TURN, SELECT, 'down,confirm,confirm,wait:20',
                                                             'confirm,confirm,wait:30']))
    check('carry: Take brings it back to Ann', units[0].get('carrying') == 1 and 'carrying' not in units[2], str(units[:3]))
    log, units, _ = played('carry_tight', 'carry', ','.join([
        IDLE, SELECT, FIRST, END_TURN,                    # Ann rescues Bram, the enemy turn passes
        'next,next,confirm', 'up,confirm',                     # Dan (the third unit left) steps onto the freed square
        'down,confirm,next,confirm,confirm,wait:20']), map_id='tight')  # Wait; then Ann's menu
    check('carry: a Drop with no room is greyed with the reason', entries(log, 'Ann') == 'Drop (no room), Give', entries(log, 'Ann'))


def pair_flow():
    log, units, img = played('pair_up', 'pair', IDLE + ',' + SELECT + ',confirm,confirm,wait:30')
    check('pair: the first menu offers Pair up', entries(log, 'Ann') == 'Pair up', entries(log, 'Ann'))
    check('pair: Ann goes behind Bram (save)', units and units[1].get('carrying') == 0 and not units[0]['alive']
          and units[0]['cell'] == -1 and not units[1]['done'], str(units[:2]))
    log, units, _ = played('pair_switch', 'pair', ','.join([IDLE, SELECT, FIRST, END_TURN, SELECT, 'confirm,wait:30']))
    check('pair: the pair offers Switch then Drop', 'rescue entries of Bram: Switch, Drop' in log)
    check('pair: Switch brings Ann to the front and goes on with her menu (save)',
          entries(log, 'Ann') == 'Switch, Drop' and units[0].get('carrying') == 1 and not units[1]['alive'], str(units[:2]))
    log, units, _ = played('pair_drop', 'pair', ','.join([IDLE, SELECT, FIRST, END_TURN, SELECT, 'down,confirm,confirm,wait:30']))
    check('pair: Drop puts the unit behind out on a square, done', 'carrying' not in units[1] and units[0]['alive']
          and units[0]['done'] and units[0]['cell'] >= 0, str(units[:2]))


def panel_row():
    before, _ = picture('panel_before', 'carry', IDLE, map_id='open')                       # the cursor on Ann
    after, _ = picture('panel_after', 'carry', IDLE + ',' + SELECT + ',confirm,confirm,wait:30')  # Ann carries Bram
    box = (VIEW_W + 4, 476, 14, 140)
    check('panel: the unit that carries has one more text line (Carrying: Bram)',
          lines(after, *box) == lines(before, *box) + 1, '%d vs %d' % (lines(after, *box), lines(before, *box)))



def marker_pictures():
    """The marker is the only difference between a unit with a partner and the same unit alone (pair up: the partner
    is gone from the map; the baseline map simply has no Ann)."""
    go = IDLE + ',' + SELECT + ',confirm,confirm,wait:30,' + FAR
    base = IDLE + ',' + FAR
    cx, cy = cam()
    img, _ = picture('marker_classic', 'pair', go)
    ref, _ = picture('marker_classic_base', 'pair', base, map_id='noann')
    n, x0, y0, x1, y1 = diff_box(img, ref)
    bx, by = 3 * TILE - cx, 2 * TILE - cy  # Bram's tile
    # inset 3, block 18 wide: the tab is 6 x 7 at the block's top right corner, with a 1 pixel rim
    check('marker: top-down, exactly the tab at the top right corner', n >= 8 and bx + 14 <= x0 and x1 <= bx + 21 and by + 2 <= y0 and y1 <= by + 10,
          str((n, x0, y0, x1, y1, bx, by)))
    for style, game, label in (('iso', 'pair', '2D isometric'), ('iso3d', 'pair', '3D isometric'), ('full3d', 'pair', 'full 3D'),
                               ('classic', 'pair_h', 'hexagons top-down'), ('iso', 'pair_h', 'hexagons isometric')):
        img, _ = picture('marker_%s_%s' % (game, style), game, go, style)
        ref, _ = picture('marker_%s_%s_base' % (game, style), game, base, style, map_id='noann')
        n, x0, y0, x1, y1 = diff_box(img, ref)
        check('marker: %s, a small tab and nothing else' % label, n >= 8 and x1 - x0 <= 14 and y1 - y0 <= 16, str((n, x0, y0, x1, y1)))


def phone():
    for mode in ('carry', 'pair'):
        game = mode + '_s'
        pic, log = picture('phone_%s_menu' % mode, game, IDLE + ',' + SELECT)
        check('phone %s: the menu opens' % mode, entries(log, 'Ann') == ('Rescue' if mode == 'carry' else 'Pair up'), entries(log, 'Ann'))
        pic, log = picture('phone_%s_done' % mode, game, IDLE + ',' + SELECT + ',confirm,confirm,wait:30')
        check('phone %s: the pick and the action play through' % mode, 'pick unit: 2 candidates' in log and '[ERROR]' not in log)


def main():
    global PLAYER, REPO, TMP, GAMES, SHOTS
    argv = sys.argv[1:]
    if '--shots' in argv:  # keep the screenshots (BMP) in this folder
        i = argv.index('--shots')
        SHOTS = os.path.abspath(argv[i + 1])
        os.makedirs(SHOTS, exist_ok=True)
        del argv[i:i + 2]
    if len(argv) < 2:
        sys.exit(__doc__)
    PLAYER, REPO = os.path.abspath(argv[0]), os.path.abspath(argv[1])
    TMP = tempfile.mkdtemp(prefix='srpg_rescue_')
    GAMES = os.path.join(TMP, 'games')
    try:
        games.write_game(GAMES, 'carry', 'carry')
        games.write_game(GAMES, 'pair', 'pairUp')
        games.write_game(GAMES, 'pair_h', 'pairUp', hexes=True)
        games.write_game(GAMES, 'carry_s', 'carry', view=PHONE)
        games.write_game(GAMES, 'pair_s', 'pairUp', view=PHONE)
        for scenario in (carry_flow, pair_flow, panel_row, marker_pictures, phone):
            scenario()
    finally:
        shutil.rmtree(TMP, ignore_errors=True)
    print('%d passed, %d failed' % (passed, len(failures)))
    sys.exit(1 if failures else 0)


if __name__ == '__main__':
    main()
