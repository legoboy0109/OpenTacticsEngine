#!/usr/bin/env python3
"""The player and character-locked weapons (docs/SRPG.md "Character-locked weapons"), windowless.

  python3 tests/srpg_player_locks_test.py ./build/engine/player/ote-player .
  python3 tests/srpg_player_locks_test.py ./build/engine/player/ote-player . --shots OUT_DIR   # keep the screenshots (BMP)

Copies tests/fixtures/srpg-locks into a temp folder (two window sizes: 480x270 and a 270x480 phone), runs the player
headless with --input scripts and --no-saving, and reads the player's log lines and the pixels of the screenshots:

  - a weapon that is locked to someone else is greyed with the reason ("Only Ser Ovan can wield this") on the Items
    screen, in its choice menu (Equip greyed) and on the unit details' Items page;
  - the named character can equip the weapon and attacks with it (the forecast names it), and the foe whose own
    locked weapon comes first in its pack counters with the weapon it can wield.
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import (  # noqa: E402
    Bmp, run_text, check, failures, inp, lines, player_and_repo, player_cmd, read_json, shots_dir, summary, ticks, write_json)
Image = Bmp  # (the name this script uses)

sys.dont_write_bytecode = True

PLAYER, REPO = player_and_repo()
SHOTS = shots_dir()
TMP = tempfile.mkdtemp(prefix='ote-locks-')

IDLE = 'wait:14'  # the turn banner plays first; it takes the first inputs otherwise
RED = (255, 140, 120)     # a reason
GREYED = (110, 114, 140)  # a weapon the unit may not wield
REASON = 'Only Ser Ovan can wield this'


def make_game(gid, width, height, locked_away):
    """A copy of the fixture. `locked_away`: Kell carries the dawn blade but it is locked to Ser Ovan, not to him."""
    g = os.path.join(TMP, gid)
    shutil.copytree(os.path.join(REPO, 'tests/fixtures/srpg-locks'), g)
    game = read_json(g + '/game.json')
    game['view'] = {'width': width, 'height': height, 'tileSize': 24}
    game['formulas']['hit'] = '100'
    write_json(g + '/game.json', game)
    if locked_away:
        weapons = read_json(g + '/data/weapons.json')
        for w in weapons['weapons']:
            if w['id'] == 'dawn_blade':
                w['wieldedBy'] = ['ser']
        write_json(g + '/data/weapons.json', weapons)
        chars = read_json(g + '/data/characters.json')
        chars['characters'][0]['items'] = ['bronze_sword', 'dawn_blade']
        write_json(g + '/data/characters.json', chars)
    return g


def run(game, inp, shot=None, frames=None, interval=4):
    cmd = player_cmd(PLAYER, TMP, REPO) + ['--no-saving', '--frames', str(frames or ticks(inp) * interval + 60), '--input-interval', str(interval),
           '--fixed-dt', '0.016', '--game', game, '--map', 'duel', '--input', inp]
    if shot:
        cmd += ['--screenshot', shot]
    p = run_text(cmd)
    return p.returncode, p.stdout


def shot(name, game, inp, extra=20):
    path = os.path.join(TMP, name + '.bmp')
    code, log = run(game, inp, shot=path, frames=ticks(inp) * 4 + extra)
    if SHOTS:
        os.makedirs(SHOTS, exist_ok=True)
        shutil.copy(path, os.path.join(SHOTS, name + '.bmp'))
    return Image(path), log, code


ACTION_MENU = [IDLE, 'confirm', 'wait:3', 'confirm', 'wait:3']  # select Kell, stay put: his action menu is open
# Kell's menu: Attack, Promote (the fighter has an offered promotion), Items, Trade, Info, Wait.
ITEMS = ACTION_MENU + ['down'] * 2 + ['confirm', 'wait:3']
INFO = ACTION_MENU + ['down'] * 4 + ['confirm', 'wait:3']
DAWN_MENU = ITEMS + ['down', 'confirm', 'wait:3']  # the dawn blade's choice menu (second in Kell's pack)


def test_locked_away(game, tag):
    code, log = run(game, inp(*ITEMS))
    items = lines(log, 'items:')
    check('%s: the Items screen lists the locked weapon with its reason' % tag,
          bool(items) and 'Dawn Blade [%s]' % REASON in items[-1] and 'Bronze Sword,' in items[-1], str(items[-1:]))
    code, log = run(game, inp(*DAWN_MENU))
    menu = lines(log, 'item menu:')
    check('%s: its Equip is greyed with the reason' % tag, bool(menu) and 'Equip [%s]' % REASON in menu[-1], str(menu[-1:]))
    img, log, code = shot('item_choice_%s' % tag, game, inp(*DAWN_MENU))
    check('%s: the item screen draws the weapon greyed' % tag, code == 0 and img.count(GREYED, 10) > 20)
    script = inp(*INFO, 'right', 'wait:2')
    img, log, code = shot('details_items_%s' % tag, game, script)
    check('%s: the unit details Items page shows it greyed with the reason' % tag,
          code == 0 and img.count(GREYED, 10) > 20 and img.count(RED, 10) > 20,
          'greyed=%d red=%d' % (img.count(GREYED, 10), img.count(RED, 10)))


def test_right_character(game):
    code, log = run(game, inp(*DAWN_MENU))
    menu = lines(log, 'item menu:')
    check('the named character can equip the weapon (Equip is not greyed)',
          bool(menu) and 'Equip' in menu[-1] and '[' not in menu[-1].split(' - ', 1)[1], str(menu[-1:]))
    # Equip it (the menu's first entry), then Attack the grunt next to him and read the forecast.
    script = inp(*DAWN_MENU, 'confirm', 'wait:3', 'confirm', 'wait:3', 'confirm', 'wait:3')
    code, log = run(game, script)
    fc = lines(log, 'forecast:')
    check('he attacks with it: the forecast names the Dawn Blade', bool(fc) and fc[-1].startswith('[info] forecast: Kell; Dawn Blade;'),
          str(fc[-1:]) + log[-300:])
    check('the grunt counters with the weapon it may wield (not the locked blade first in its pack)',
          bool(fc) and '| Grunt; Bronze Sword;' in fc[-1], str(fc[-1:]))
    img, log, code = shot('attack_with_dawn_blade', game, script)
    check('the attack forecast draws', code == 0)


def test_plain_game():
    shutil.copytree(os.path.join(REPO, 'games/demo-srpg'), os.path.join(TMP, 'plain'))
    p = subprocess.run(player_cmd(PLAYER, TMP, REPO) + ['--no-saving', '--frames', '80', '--fixed-dt', '0.016', '--game', 'plain', '--map', 'bridge'],
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True, timeout=300)
    check('a game without locks plays as before (no reason anywhere)', p.returncode == 0 and 'can wield this' not in p.stdout, p.stdout[-300:])


def main():
    try:
        make_game('away', 480, 270, True)
        make_game('mine', 480, 270, False)
        make_game('phone', 270, 480, True)
        test_locked_away('away', 'desktop')
        test_locked_away('phone', 'phone')
        test_right_character('mine')
        test_plain_game()
    finally:
        if not SHOTS:
            shutil.rmtree(TMP, ignore_errors=True)
    summary()
    for f in failures:
        print('  FAILED: ' + f)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
