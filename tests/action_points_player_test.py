#!/usr/bin/env python3
"""The action-point turn system in the real player, windowless (docs/SRPG.md "Action points", "Player").

  python3 tests/action_points_player_test.py ./build/engine/player/ote-player . [--shots DIR]

Plays a copy of the synthetic game tests/fixtures/action-points (not a demo; map "duel": blue lancer (1,2) and archer (1,3), woods on (3,3),
red dummies on (5,2) and (8,0)). Blue starts with 20 AP; a movement point costs 1 AP (the first 4 of a unit's turn), an attack 2. Judged from
the [info] lines the player logs, the Continue save and the screenshots:

  - the hint line names the AP price of the square under the cursor ("move 2 AP"; the woods cost 2 points);
  - a unit that walked can walk on: its action menu has a Move entry, the second walk is paid on top of the first;
  - once its Move stat is used up the menu has no Move entry;
  - the Attack entry shows its price ("2 AP"), the forecast box has an "AP 2" line, the attack is paid, and the unit may still be picked
    afterwards (its Attack entry is then greyed "used", while it has movement left);
  - an attack the pool cannot pay is not offered (greyed "no AP");
  - the pool and the counters survive in the Continue save; ending the turn refills the pool (the leftover carries over up to 10, plus 20);
  - the pool reads as a resource in the side panel, a spent unit is dimmed, and everything fits at UI scale 100 and 150 on 480x270 (the
    pictures are kept for a look).
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
PICK = 'confirm'  # the first unit (the lancer) is selected: its reach is painted
STYLES = [('classic', 'top-down'), ('isometric', 'isometric'), ('isometric-3d', '3D isometric'), ('full-3d', 'full 3D')]


def make_game(tweak=None):
    shutil.rmtree(GAMES, ignore_errors=True)
    g = os.path.join(GAMES, 'ap')
    shutil.copytree(os.path.join(REPO, 'tests', 'fixtures', 'action-points'), g)
    j = json.load(open(os.path.join(g, 'game.json')))
    j['styles'] = [{'id': 'classic', 'name': 'Classic'}, {'id': 'isometric', 'name': 'Isometric', 'view': 'isometric'},
                   {'id': 'isometric-3d', 'name': '3D Isometric', 'view': 'isometric-3d'}, {'id': 'full-3d', 'name': 'Full 3D', 'view': '3d'}]
    if tweak:
        tweak(j)
    json.dump(j, open(os.path.join(g, 'game.json'), 'w'))


def play(inp, name, style='classic', scale=None, saves=False):
    shot = os.path.join(TMP, name + '.bmp')
    cmd = [PLAYER, '--games', GAMES, '--assets', os.path.join(REPO, 'assets'), '--headless', '--no-audio',
           '--frames', str(ticks(inp) * 4 + 80), '--input-interval', '4', '--fixed-dt', '0.016',
           '--game', 'ap', '--map', 'duel', '--style', style, '--3d', 'software', '--no-settings', '--input', inp, '--screenshot', shot]
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
        shutil.copy(shot, os.path.join(SHOTS, 'ap_' + name + '.bmp'))
    save = None
    if saves and os.path.exists(os.path.join(sdir, 'ap', 'continue.json')):
        save = json.load(open(os.path.join(sdir, 'ap', 'continue.json')))
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


def unit_of(save, story_id):
    for u in save['battle']['units']:
        if u.get('id') == story_id:
            return u
    return None


def pool(save, side=0):
    """The AP of a side in a save (resource 0 of the fixture)."""
    return save['battle']['sides'][side]['resources'][0]


def pool_after(log):
    """The pool the player logged after its last order ("ap pool: 14 (order 0)")."""
    got = lines(log, 'ap pool:')
    return int(got[-1].split()[2]) if got else None


def differs(a, b):
    pa, pb = Bmp(a), Bmp(b)
    return sum(1 for y in range(pa.h) for x in range(pa.w) if pa.px[y][x] != pb.px[y][x])


def main():
    make_game()
    walk = IDLE + ',' + PICK

    # ---- the price of a square, in the hint line
    log, _, _ = play(walk + ',right,right,wait:6', 'hint')
    h = lines(log, 'move hint:')
    check('two squares east: "move 2 AP"', any('move 2 AP' in l for l in h), h)
    log, _, _ = play(walk + ',right,right,down,wait:6', 'hint_woods')
    h = lines(log, 'move hint:')
    check('the woods square (2 movement points) makes it 4 AP', any('move 4 AP' in l for l in h), h)
    check('hint: no UI bug', clean(log), log[-300:])

    # ---- a unit that walked may walk on: the second walk is paid on top
    first = walk + ',right,right,confirm,wait:30'  # to (3,2): 2 AP; the action menu opens
    log, _, _ = play(first, 'menu_after_move')
    menus = lines(log, 'menu entries:')
    check('after the walk the action menu offers Move (movement is left) and Wait', bool(menus) and 'Move' in menus[-1] and 'Wait' in menus[-1], menus)
    second = first + ',confirm,wait:4,down,wait:6'  # Move; the cursor goes to (3,3): woods, 2 points: 2 AP (2 + 2 = 4 points: all its Move)
    log, _, _ = play(second, 'moveon')
    h = lines(log, 'move hint:')
    check('walking on: the hint names the price of the rest ("move 2 AP" for the woods)', any('move 2 AP' in l for l in h[-2:]), h)
    log, _, _ = play(second + ',confirm,wait:40', 'moveon_done')
    check('both walks were paid: 20 - 2 - 2 = 16 AP', pool_after(log) == 16, lines(log, 'ap pool:'))
    menus = lines(log, 'menu entries:')
    check('with its Move stat used up the menu has no Move entry', bool(menus) and 'Move' not in menus[-1] and 'Wait' in menus[-1], menus)
    check('walking on: nothing warned about', clean(log), log[-300:])

    # ---- attacking: the price on the entry, in the forecast, paid; the unit may still be picked
    strike = walk + ',right,right,right,confirm,wait:30'  # to (4,2), next to the dummy on (5,2): 3 AP
    log, _, _ = play(strike + ',wait:6', 'attack_menu')
    menus = lines(log, 'menu entries:')
    check('the Attack entry shows its price ("Attack (2 AP)", first)', bool(menus) and menus[-1].startswith('menu entries: Attack (2 AP)'), menus)
    log, _, _ = play(strike + ',confirm,wait:8', 'forecast')
    f = lines(log, 'forecast:')
    check('the forecast box has the attacker\'s AP line', bool(f) and 'AP 2;' in f[-1].split('|')[0], f)
    log, _, _ = play(strike + ',confirm,confirm,wait:80', 'attacked')
    check('the attack was paid: 20 - 3 - 2 = 15 AP', pool_after(log) == 15, lines(log, 'ap pool:'))
    check('the order names the dummy', any('Dummy' in l or 'attack' in l for l in lines(log, 'order: attack')), lines(log, 'order:'))
    check('attacking: nothing warned about', clean(log), log[-300:])
    again = strike + ',confirm,confirm,wait:80,confirm,confirm,wait:6'  # the lancer is picked again (1 movement point left: its reach first, then its menu)
    log, _, _ = play(again, 'attacked_again')
    menus = lines(log, 'menu entries:')
    check('picked again: Attack is greyed (the one attack of the turn is made), Move is there (1 movement point left)', bool(menus) and 'Attack (used) [grey]' in menus[-1] and 'Move' in menus[-1], menus)

    log, _, _ = play(again + ',down,confirm,wait:40', 'spent')  # one more square: the Move stat and the attack are both used up
    check('a unit with nothing left to do is dimmed (picture kept for a look); nothing warned about', clean(log), log[-300:])

    # ---- an attack the pool cannot pay is not offered
    make_game(lambda j: j['resources'][0]['refill'].update({'perTurn': 4}))  # 4 AP: the walk of 3 leaves 1, the attack costs 2
    log, _, _ = play(strike + ',wait:6', 'no_ap')
    menus = lines(log, 'menu entries:')
    check('with 1 AP left Attack is greyed ("Attack (no AP) [grey]")', bool(menus) and 'Attack (no AP) [grey]' in menus[-1], menus)
    make_game()

    # ---- ending the turn refills the pool: the 15 left carry over only up to 10, plus 20
    log, save, _ = play(strike + ',confirm,confirm,wait:80,menu,confirm,wait:400', 'next_turn', saves=True)
    check('the next blue turn starts with min(15, 10) + 20 = 30 AP', save is not None and pool(save) == 30 and save['battle']['round'] == 2, (pool(save) if save else None, save and save['battle']['round']))
    lancer = unit_of(save, 'lan') if save else None
    check('the counters of a unit start over', lancer is not None and 'apAttacks' not in lancer and 'moveSpent' not in lancer, lancer)
    check('the whole round: nothing warned about (the computer opponent played its turn)', clean(log), log[-400:])

    # ---- the pictures: UI scale 100 and 150, all four views
    check_shots = {}
    for tag, scale in (('s100', None), ('s150', 150)):
        log, _, shot = play(strike + ',confirm,wait:10', 'forecast_' + tag, scale=scale)
        check('forecast box at UI scale %s: drawn, no UI bug' % (scale or 100), clean(log) and bool(lines(log, 'forecast:')), log[-300:])
        log, _, shot = play(walk + ',right,right,wait:6', 'hint_' + tag, scale=scale)
        check('the reach and the hint at UI scale %s: no UI bug' % (scale or 100), clean(log), log[-300:])
        log, _, shot = play(first, 'menu_' + tag, scale=scale)
        check('the action menu with Move at UI scale %s: no UI bug' % (scale or 100), clean(log), log[-300:])
    for style, label in STYLES:
        _, _, plain = play(IDLE + ',wait:3', 'plain_' + style, style=style)
        _, _, picked = play(walk + ',wait:6', 'picked_' + style, style=style)
        check('%s: the reach squares are tinted (the picture differs from the plain map)' % label, differs(plain, picked) > 800, differs(plain, picked))
    summary()


if __name__ == '__main__':
    main()
