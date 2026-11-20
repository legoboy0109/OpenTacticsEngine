#!/usr/bin/env python3
"""Area attacks and line fire in the real player, windowless (docs/SRPG.md "Area attacks and line fire").

  python3 tests/area_fire_player_test.py ./build/engine/player/ote-player . [--shots DIR]

Plays a copy of the synthetic game tests/fixtures/area-fire (not a demo; map "field": blue mortar (1,3), rifle (1,1), flak (1,5); red dummies
(5,3), (6,3), (5,1) and a tough one (5,4)). Judged from the [info] lines the player logs, the Continue save and the screenshots:

  - Aiming the mortar (radius 2) at a square with nobody on it shows a box of its own ("Hits 1 foe", the damage); aimed at the dummy on
    (5,3) it shows that dummy's forecast with one more line for the three others the blast reaches ("+3 more: 8, 8, 8 dmg").
  - Confirming fires: the order names the square, all four victims lose the forecast damage in the save, nothing was warned about.
  - The rifle (line fire) offers only the dummy on its row, and only that one square.
  - The same pictures at UI scale 150 on the 480x270 canvas (the box has to fit; the pictures are kept for a look) and in all four views.
  - The highlight of the shot differs from the plain map in every view (the blast squares are tinted).
Pictures: --shots DIR keeps them (BMP).
"""
import json
import os
import re
import shutil
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import Bmp, check, player_and_repo, run_text, scratch_dir, shots_dir, summary, ticks  # noqa: E402

sys.dont_write_bytecode = True
PLAYER, REPO = player_and_repo()
SHOTS = shots_dir(absolute=True)
TMP = scratch_dir()
GAMES = os.path.join(TMP, 'games')
IDLE = 'wait:14'  # the turn banner plays first; it takes the first inputs otherwise
SELECT = 'confirm,confirm'  # select the unit under the cursor: its action menu opens (Attack first)
STYLES = [('classic', 'top-down'), ('isometric', 'isometric'), ('isometric-3d', '3D isometric'), ('full-3d', 'full 3D')]
ENEMIES = [(5, 3), (6, 3), (5, 4), (5, 1)]


def make_game():
    shutil.rmtree(GAMES, ignore_errors=True)
    g = os.path.join(GAMES, 'af')
    shutil.copytree(os.path.join(REPO, 'tests', 'fixtures', 'area-fire'), g)
    j = json.load(open(os.path.join(g, 'game.json')))
    j['styles'] = [{'id': 'classic', 'name': 'Classic'}, {'id': 'isometric', 'name': 'Isometric', 'view': 'isometric'},
                   {'id': 'isometric-3d', 'name': '3D Isometric', 'view': 'isometric-3d'}, {'id': 'full-3d', 'name': 'Full 3D', 'view': '3d'}]
    json.dump(j, open(os.path.join(g, 'game.json'), 'w'))


def aim_order(frm=(1, 3), lo=2, hi=5, radius=2):
    """The squares the mortar can be aimed at, in the order the player cycles them (cell index): in range and with an enemy within `radius`."""
    cells = []
    for y in range(7):
        for x in range(10):
            d = abs(x - frm[0]) + abs(y - frm[1])
            if lo <= d <= hi and any(abs(x - ex) + abs(y - ey) <= radius for ex, ey in ENEMIES):
                cells.append((x, y))
    return cells


def play(inp, name, style='classic', scale=None, saves=False):
    shot = os.path.join(TMP, name + '.bmp')
    cmd = [PLAYER, '--games', GAMES, '--assets', os.path.join(REPO, 'assets'), '--headless', '--no-audio',
           '--frames', str(ticks(inp) * 4 + 80), '--input-interval', '4', '--fixed-dt', '0.016',
           '--game', 'af', '--map', 'field', '--style', style, '--3d', 'software', '--no-settings', '--input', inp, '--screenshot', shot]
    sdir = os.path.join(TMP, 'saves_' + name)
    if saves:
        shutil.rmtree(sdir, ignore_errors=True)
        cmd += ['--saves', sdir]
    else:
        cmd += ['--no-saving']
    if scale:
        cmd += ['--ui-scale', str(scale)]
    log = run_text(cmd).stdout
    if SHOTS and os.path.exists(shot):
        os.makedirs(SHOTS, exist_ok=True)
        shutil.copy(shot, os.path.join(SHOTS, 'af_' + name + '.bmp'))
    save = None
    if saves and os.path.exists(os.path.join(sdir, 'af', 'continue.json')):
        save = json.load(open(os.path.join(sdir, 'af', 'continue.json')))
    return log, save, shot


def lines(log, prefix):
    out = []
    for line in log.split('\n'):
        m = re.search(r'\[info\]\s*(.*)$', line)
        if m and m.group(1).startswith(prefix):
            out.append(m.group(1))
    return out


def clean(log):
    return 'UI bug' not in log and '[warn]' not in log and '[error]' not in log


def hp_of(save, story_id):
    for u in save['battle']['units']:
        if u.get('id') == story_id:
            return u['hp']
    return None


def differs(a, b):
    """Number of pixels that differ between two screenshots."""
    pa, pb = Bmp(a), Bmp(b)
    return sum(1 for y in range(pa.h) for x in range(pa.w) if pa.px[y][x] != pb.px[y][x])


def main():
    make_game()
    order = aim_order()
    first, onA = order[0], order.index((5, 3))
    attack = IDLE + ',' + SELECT + ',confirm'  # the mortar (the first unit) is selected and Attack chosen: aiming at the first square
    to_a = attack + ',right' * onA + ',wait:6'

    # ---- the boxes
    log, _, _ = play(attack + ',wait:6', 'first')
    f = lines(log, 'area forecast:')
    check('aiming at an empty square: a box of its own (%d,%d)' % first, bool(f) and 'Mortar;' in f[-1] and 'Hits 1 foe;' in f[-1] and '8 dmg' in f[-1], f)
    check('the first aim is the first square in the order the player cycles (no UI bug)', clean(log), log[-300:])
    log, _, _ = play(to_a, 'aimed_a')
    f = lines(log, 'forecast:')
    check('aimed at the dummy on (5,3): its forecast, with the three others on one more line',
          bool(f) and 'Dummy;' in f[-1] and '+3 more: 8, 8, 8 dmg' in f[-1] and 'Dmg 8;' in f[-1], f)
    check('the box does not say it is the counter-attack of anyone (the dummy answers nothing at range 4)', bool(f) and 'No counter' in f[-1], f)

    # ---- firing
    log, save, _ = play(to_a + ',confirm,wait:60,confirm,wait:40,confirm,wait:60', 'fire', saves=True)
    check('the order names the square', any('square 5,3' in l for l in lines(log, 'order: attack')), lines(log, 'order: attack'))
    got = [hp_of(save, i) if save else None for i in ('dummyA', 'dummyB', 'toughC', 'dummyD')]
    check('all four victims lost the forecast damage (12-8, 12-8, 60-8, 12-8)', got == [4, 4, 52, 4], got)
    check('firing: nothing warned about', clean(log), log[-300:])

    # ---- line fire: the rifle offers the dummy on its row, only that one
    rifle = IDLE + ',up,up,' + SELECT + ',confirm'
    log, _, _ = play(rifle + ',right,right,wait:6', 'rifle')
    f = lines(log, 'forecast:')
    check('the rifle: one forecast, the dummy on its row', len(set(f)) == 1 and 'Dummy;' in f[-1] and 'Dmg 9;' in f[-1], f)
    check('the rifle: no UI bug', clean(log), log[-300:])

    # ---- UI scale 150 and the four views
    for tag, scale in (('scale150_empty', 150), ('scale150_a', 150)):
        log, _, _ = play((attack + ',wait:6') if tag.endswith('empty') else to_a, tag, scale=scale)
        check('%s: the box is drawn (no UI bug)' % tag, clean(log) and bool(lines(log, 'area forecast:') or lines(log, 'forecast:')), log[-300:])
    plain_shots = {}
    for style, label in STYLES:
        _, _, plain = play(IDLE + ',wait:3', 'plain_' + style, style=style)
        _, _, aimed = play(to_a, 'aim_' + style, style=style)
        _, _, rifleShot = play(rifle + ',wait:6', 'rifle_' + style, style=style)
        check('%s: the blast squares are tinted (the picture differs from the plain map by many pixels)' % label, differs(plain, aimed) > 1200, differs(plain, aimed))
        check('%s: aiming the rifle tints the line squares' % label, differs(plain, rifleShot) > 400, differs(plain, rifleShot))
        plain_shots[style] = plain
    summary()


if __name__ == '__main__':
    main()
