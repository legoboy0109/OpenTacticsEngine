#!/usr/bin/env python3
"""Unit ranks in the real player, windowless (docs/SRPG.md "Unit ranks", "Player").

  python3 tests/unit_ranks_player_test.py ./build/engine/player/ote-player . [--shots DIR]

Plays a copy of the synthetic game tests/fixtures/unit-ranks (not a demo), map "duel": blue soldier "sol" (2,2) next to the red dummy (3,2), blue "vet" (1,4) that
starts with 10 experience (Veteran, badge I) and blue "elite" (3,0) with 30 (Elite, badge II). Sides have colours, so the rank badge (a gold tab with a dark rim, fill
(120, 86, 20)) can be found in the pictures. Judged from the [info] lines the player logs and the screenshots:

  - the badge is drawn in every kind of view (top-down, isometric, 3D isometric, full 3D) for the units that hold a rank and not for the one that does not;
  - a hit that earns the first rank makes the badge appear, opens the rank-up box (titled "Veteran!", the unit's name and the stat gains) and closes it by itself;
  - the unit details say the rank and the experience instead of a level;
  - UI scale 100 and 150 draw without a UI bug;
  - a game without a rank table draws no badge.
Pictures: --shots DIR keeps them (BMP).
"""
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
STYLES = [('classic', 'top-down'), ('isometric', 'isometric'), ('isometric-3d', '3D isometric'), ('full-3d', 'full 3D')]
TAB = (120, 86, 20)    # the badge's fill (engine/player/battle/unit_rank_view.cpp)
TEXT = (255, 232, 130)  # and its letters


def make_game(table=True):
    shutil.rmtree(GAMES, ignore_errors=True)
    g = os.path.join(GAMES, 'urg')
    shutil.copytree(os.path.join(REPO, 'tests', 'fixtures', 'unit-ranks'), g)

    def game(j):
        j['styles'] = [{'id': 'classic', 'name': 'Classic'}, {'id': 'isometric', 'name': 'Isometric', 'view': 'isometric'},
                       {'id': 'isometric-3d', 'name': '3D Isometric', 'view': 'isometric-3d'}, {'id': 'full-3d', 'name': 'Full 3D', 'view': '3d'}]
        if not table:
            prog = j['rules']['progression']
            del prog['levels']
    edit(g, 'game.json', game)

    def duel(j):
        j['sides'][0]['color'] = '#3060ff'
        j['sides'][1]['color'] = '#ff3030'
        if not table:
            for u in j['units']:
                u.pop('xp', None)
    edit(g, 'maps/duel.json', duel)


def play(inp, name, style='classic', scale=None):
    shot = os.path.join(TMP, name + '.bmp')
    cmd = [PLAYER, '--games', GAMES, '--assets', os.path.join(REPO, 'assets'), '--headless', '--no-audio',
           '--frames', str(ticks(inp) * 4 + 80), '--input-interval', '4', '--fixed-dt', '0.016',
           '--game', 'urg', '--map', 'duel', '--style', style, '--3d', 'software', '--no-settings', '--no-saving', '--input', inp, '--screenshot', shot]
    if scale:
        cmd += ['--ui-scale', str(scale)]
    log = run_text(cmd).stdout
    if SHOTS and os.path.exists(shot):
        os.makedirs(SHOTS, exist_ok=True)
        shutil.copy(shot, os.path.join(SHOTS, 'ur_' + name + '.bmp'))
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


def tab_pixels(shot):
    p = Bmp(shot)
    return p.count(TAB, tol=6) + p.count(TEXT, tol=6)


def main():
    make_game()
    # sol (selected first) stays where it is, opens the menu, and attacks the dummy beside it: 10 experience = the first rank
    attack = IDLE + ',confirm,confirm,confirm,confirm,wait:100'
    midbox = IDLE + ',confirm,confirm,confirm,confirm,wait:22'  # the box is up

    # ---- the badge, in every kind of view: two ranked units are on the map before anything happens
    for style, label in STYLES:
        _, shot = play(IDLE, 'start_' + style, style=style)
        n = tab_pixels(shot)
        check('%s: the badges of the two ranked units are drawn (%d tab pixels)' % (label, n), n >= 30, n)

    # ---- the first rank: badge appears, box opens, closes by itself
    log, shot = play(attack, 'rankup')
    box = lines(log, 'rank up box:')
    check('a hit that earns the first rank opens the rank-up box (the struck dummy answers and earns its own too)',
          len(box) == 2 and box[0].startswith('rank up box: Soldier - Veteran (+1 Strength)') and box[1].startswith('rank up box: Dummy - Veteran'), (box, log[-400:]))
    check('... nothing warned about', clean(log), log[-300:])
    _, before = play(IDLE, 'before')
    check('... and the third badge is on the map afterwards (more tab pixels than before the fight)', tab_pixels(shot) > tab_pixels(before), (tab_pixels(shot), tab_pixels(before)))
    log, _ = play(attack + ',wait:60', 'rankup_later')
    check('... the box closes by itself (the player is still running to the end of the script without a warning)', clean(log), log[-300:])

    # ---- UI scale 100 and 150: the badge, the box
    for tag, scale in (('s100', None), ('s150', 150)):
        log, shot = play(midbox, 'box_' + tag, scale=scale)
        n = tab_pixels(shot)
        check('the box is up a moment after the hit at UI scale %s: the gold frame of the level-up box is on screen' % (scale or 100), Bmp(shot).count((255, 220, 90), tol=6) > 50, n)
        log, shot = play(attack, 'after_' + tag, scale=scale)
        check('the rank-up box at UI scale %s: no UI bug' % (scale or 100), clean(log) and len(lines(log, 'rank up box:')) == 2, log[-300:])
        _, shot = play(IDLE, 'map_' + tag, scale=scale)
        check('the badges at UI scale %s are drawn' % (scale or 100), tab_pixels(shot) >= 30, tab_pixels(shot))

    # ---- a game without a table: no badge
    make_game(table=False)
    log, shot = play(attack, 'off')
    check('without a table nothing is drawn and no rank-up box opens', tab_pixels(shot) == 0 and not lines(log, 'rank up box:'), (tab_pixels(shot), lines(log, 'rank up box:')))
    check('... nothing warned about', clean(log), log[-300:])
    summary()
    return 1 if __import__('ote_testkit').failures else 0


if __name__ == '__main__':
    sys.exit(main())
