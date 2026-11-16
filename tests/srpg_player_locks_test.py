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
import struct
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True

PLAYER = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else ''
REPO = os.path.abspath(sys.argv[2]) if len(sys.argv) > 2 else '.'
SHOTS = sys.argv[sys.argv.index('--shots') + 1] if '--shots' in sys.argv else ''
TMP = tempfile.mkdtemp(prefix='ote-locks-')
passed, failures = 0, []

IDLE = 'wait:14'  # the turn banner plays first; it takes the first inputs otherwise
RED = (255, 140, 120)     # a reason
GREYED = (110, 114, 140)  # a weapon the unit may not wield
REASON = 'Only Ser Ovan can wield this'


def read_json(path):
    with open(path) as f:
        return json.load(f)


def write_json(path, obj):
    with open(path, 'w') as f:
        json.dump(obj, f, indent=1)


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


def ticks(script):
    return sum(int(t[5:]) if t.startswith('wait:') else 1 for t in script.split(','))


def run(game, inp, shot=None, frames=None, interval=4):
    cmd = [PLAYER, '--games', TMP, '--assets', os.path.join(REPO, 'assets'), '--headless', '--no-audio', '--no-settings',
           '--no-saving', '--frames', str(frames or ticks(inp) * interval + 60), '--input-interval', str(interval),
           '--fixed-dt', '0.016', '--game', game, '--map', 'duel', '--input', inp]
    if shot:
        cmd += ['--screenshot', shot]
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

    def count(self, rgb, tol=12):
        n = 0
        for row in self.px:
            for p in row:
                if abs(p[0] - rgb[0]) <= tol and abs(p[1] - rgb[1]) <= tol and abs(p[2] - rgb[2]) <= tol:
                    n += 1
        return n


def check(name, cond, detail=''):
    global passed
    if cond:
        passed += 1
        print('PASS ' + name)
    else:
        failures.append(name)
        print('FAIL %s %s' % (name, detail))


def shot(name, game, inp, extra=20):
    path = os.path.join(TMP, name + '.bmp')
    code, log = run(game, inp, shot=path, frames=ticks(inp) * 4 + extra)
    if SHOTS:
        os.makedirs(SHOTS, exist_ok=True)
        shutil.copy(path, os.path.join(SHOTS, name + '.bmp'))
    return Image(path), log, code


def lines(log, key):
    return [l for l in log.splitlines() if key in l]


def inp(*parts):
    return ','.join(parts)


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
    p = subprocess.run([PLAYER, '--games', TMP, '--assets', os.path.join(REPO, 'assets'), '--headless', '--no-audio', '--no-settings',
                        '--no-saving', '--frames', '80', '--fixed-dt', '0.016', '--game', 'plain', '--map', 'bridge'],
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
    print('%d passed, %d failed' % (passed, len(failures)))
    for f in failures:
        print('  FAILED: ' + f)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
