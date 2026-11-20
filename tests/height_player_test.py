#!/usr/bin/env python3
"""The height rules in the real player, windowless (docs/SRPG.md "Height rules", "Player").

  python3 tests/height_player_test.py ./build/engine/player/ote-player . [--shots DIR]

Plays a copy of the synthetic game tests/fixtures/height-rules (not a demo), map "cliffs": columns 0 to 4 are level 0, columns 5 to 7 a plateau of
level 2 (a cliff of two levels between). Blue: lancer (2,2) (jump 1), slinger (4,1) (a sling cannot fire at anything higher than its shooter), goat
(3,4) (jump up 3). Red: dummies on (5,1) and (7,4), both on the plateau. Judged from the [info] lines the player logs and the screenshots:

  - the terrain panel shows the height of the square under the cursor ("terrain height: 2 at 5,2"), and not in a game without the rules;
  - the lancer's reach stays below the cliff (23 squares it can stop on) and its sword still reaches the plateau's first column (attack-only
    squares), while the goat climbs it (its reach is worked out here independently by a search over the same heights);
  - confirming a square across the cliff gives the reason ("too steep to climb there"); confirming an enemy the sling cannot fire up at gives
    "too high to reach";
  - everything fits at UI scale 100 and 150 on 480x270 in all four kinds of view (the pictures are kept for a look).
Pictures: --shots DIR keeps them (BMP).
"""
import json
import os
import re
import shutil
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import Bmp, check, edit, player_and_repo, run_text, scratch_dir, shots_dir, summary, ticks  # noqa: E402

sys.dont_write_bytecode = True
PLAYER, REPO = player_and_repo()
SHOTS = shots_dir(absolute=True)
TMP = scratch_dir()
GAMES = os.path.join(TMP, 'games')
IDLE = 'wait:14'  # the turn banner plays first; it takes the first inputs otherwise
PICK = 'confirm'  # the first unit (the lancer) is selected: its reach is painted
STYLES = [('classic', 'top-down'), ('isometric', 'isometric'), ('isometric-3d', '3D isometric'), ('full-3d', 'full 3D')]

# The map "cliffs": its heights and the squares units stand on (blue first), for the independent reach search below.
W, H = 8, 5
HEIGHT = [[0, 0, 0, 0, 0, 2, 2, 2] for _ in range(H)]
UNITS = {(2, 2), (4, 1), (3, 4), (5, 1), (7, 4)}


def make_game(rules=True):
    shutil.rmtree(GAMES, ignore_errors=True)
    g = os.path.join(GAMES, 'hr')
    shutil.copytree(os.path.join(REPO, 'tests', 'fixtures', 'height-rules'), g)

    def game(j):
        j['styles'] = [{'id': 'classic', 'name': 'Classic'}, {'id': 'isometric', 'name': 'Isometric', 'view': 'isometric'},
                       {'id': 'isometric-3d', 'name': '3D Isometric', 'view': 'isometric-3d'}, {'id': 'full-3d', 'name': 'Full 3D', 'view': '3d'}]
        if not rules:
            del j['rules']['height']
    edit(g, 'game.json', game)
    if not rules:  # a game that never heard of the keys
        def weapons(j):
            for w in j['weapons']:
                w.pop('heightRange', None)
                w.get('area', {}).pop('vertical', None)
        edit(g, 'data/weapons.json', weapons)
        edit(g, 'data/units.json', lambda j: [u.pop('jump', None) for u in j['units']])


def play(inp, name, style='classic', scale=None):
    shot = os.path.join(TMP, name + '.bmp')
    cmd = [PLAYER, '--games', GAMES, '--assets', os.path.join(REPO, 'assets'), '--headless', '--no-audio',
           '--frames', str(ticks(inp) * 4 + 80), '--input-interval', '4', '--fixed-dt', '0.016',
           '--game', 'hr', '--map', 'cliffs', '--style', style, '--3d', 'software', '--no-settings', '--no-saving', '--input', inp, '--screenshot', shot]
    if scale:
        cmd += ['--ui-scale', str(scale)]
    log = run_text(cmd).stdout
    if SHOTS and os.path.exists(shot):
        os.makedirs(SHOTS, exist_ok=True)
        shutil.copy(shot, os.path.join(SHOTS, 'hr_' + name + '.bmp'))
    return log, shot


def lines(log, prefix):
    out = []
    for line in log.split('\n'):
        m = re.search(r'\[info\]\s*(.*)$', line)
        if m and m.group(1).startswith(prefix):
            out.append(m.group(1))
    return out


def clean(log):
    return 'UI bug' not in log and '[warn]' not in log and '[error]' not in log


def differs(a, b):
    pa, pb = Bmp(a), Bmp(b)
    return sum(1 for y in range(pa.h) for x in range(pa.w) if pa.px[y][x] != pb.px[y][x])


def reach(start, move, up, down):
    """Squares a unit can stop on (own square included, no other unit's square) and the squares its melee weapon (range 1) reaches from them."""
    seen, frontier = {start: 0}, [start]
    while frontier:
        nxt = []
        for (x, y) in frontier:
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if not (0 <= nx < W and 0 <= ny < H) or (nx, ny) in seen or seen[(x, y)] + 1 > move:
                    continue
                step = HEIGHT[ny][nx] - HEIGHT[y][x]
                if step > up or -step > down:
                    continue
                seen[(nx, ny)] = seen[(x, y)] + 1
                nxt.append((nx, ny))
        frontier = nxt
    stops = {c for c in seen if c == start or c not in UNITS}
    hits = {(x + dx, y + dy) for (x, y) in stops for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)) if 0 <= x + dx < W and 0 <= y + dy < H}
    return len(stops), len(hits - stops)


def main():
    make_game()
    # ---- the panel's Height row follows the cursor
    log, _ = play(IDLE + ',' + PICK + ',right,right,right,wait:6', 'panel')
    h = lines(log, 'terrain height:')
    check('the panel shows level 0 where the lancer stands and level 2 on the plateau', any('0 at 2,2' in l for l in h) and any('2 at 5,2' in l for l in h), h)
    check('panel: no UI bug', clean(log), log[-300:])

    # ---- the lancer cannot climb the cliff; its sword still reaches the first column of the plateau
    log, _ = play(IDLE + ',' + PICK + ',wait:6', 'lancer')
    o = lines(log, 'overlay:')
    move, attack = reach((2, 2), 5, 1, 1)
    check('the lancer\'s reach: %d squares to stop on, %d more its sword reaches' % (move, attack), bool(o) and o[-1] == 'overlay: %d move squares, %d attack-only squares' % (move, attack), o)
    # ---- the goat climbs it
    log, _ = play(IDLE + ',down,down,right,confirm,wait:6', 'goat')
    o = lines(log, 'overlay:')
    move, attack = reach((3, 4), 5, 3, 1)
    check('the goat climbs the cliff: %d squares to stop on, %d more its sword reaches' % (move, attack), bool(o) and o[-1] == 'overlay: %d move squares, %d attack-only squares' % (move, attack), o)

    # ---- the reasons
    log, _ = play(IDLE + ',' + PICK + ',right,right,right,confirm,wait:6', 'steep')
    r = lines(log, 'refused:')
    check('a step up a cliff of two levels says why', r == ['refused: too steep to climb there'], r)
    log, _ = play(IDLE + ',right,right,up,confirm,wait:4,right,confirm,wait:6', 'sling')  # the slinger (4,1) is picked, then the dummy on (5,1)
    r = lines(log, 'refused:')
    check('a sling cannot fire up at the plateau and says why', r == ['refused: too high to reach'], r)
    check('the refusals: nothing warned about', clean(log), log[-300:])

    # ---- a game without the rules has no Height row and no new lines
    make_game(rules=False)
    log, _ = play(IDLE + ',' + PICK + ',right,right,right,wait:6', 'plain')
    check('without rules.height the panel has no Height row', not lines(log, 'terrain height:') and not lines(log, 'overlay:'), lines(log, 'terrain height:'))
    check('the same board without the rules: nothing warned about', clean(log), log[-300:])
    make_game()

    # ---- the pictures: UI scale 100 and 150, all four views
    for tag, scale in (('s100', None), ('s150', 150)):
        log, _ = play(IDLE + ',' + PICK + ',right,right,right,wait:8', 'panel_' + tag, scale=scale)
        check('the panel with its Height row at UI scale %s: no UI bug' % (scale or 100), clean(log) and bool(lines(log, 'terrain height:')), log[-300:])
    for style, label in STYLES:
        _, plain = play(IDLE + ',wait:3', 'plain_' + style, style=style)
        _, picked = play(IDLE + ',' + PICK + ',wait:6', 'picked_' + style, style=style)
        check('%s: the reach squares are tinted (the picture differs from the plain map)' % label, differs(plain, picked) > 800, differs(plain, picked))
    summary()
    return 1 if __import__('ote_testkit').failures else 0


if __name__ == '__main__':
    sys.exit(main())
