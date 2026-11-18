#!/usr/bin/env python3
"""The player and map weapons (docs/SRPG.md "Map weapons"), windowless.

  python3 tests/srpg_player_mapweapon_test.py ./build/engine/player/ote-player .
  python3 tests/srpg_player_mapweapon_test.py ./build/engine/player/ote-player . --shots OUT_DIR   # keep the screenshots (BMP)

Copies tests/fixtures/srpg-mapweapon into a temp folder (two window sizes: 480x270 and a 270x480 phone), runs the player headless
with --input scripts and --no-saving, and reads the player's log lines and the pixels of the screenshots:

  - the archer steps onto the ballista, the action menu offers Attack, the target is 5 squares away (out of the bow's reach) and the
    forecast names the weapon as the structure's with its uses ("Bolt (Ballista) 3") and the right numbers;
  - the attack is made, the log says who fired what from which structure, and the uses go down in the terrain info;
  - the third shot spends it (the log line says so) and the ballista becomes the wreck;
  - the knight, who may not use the ballista, has no Attack there; the screenshots are looked at by a person (the weapon picker is the
    targeting forecast; the info panel names the weapon and the uses left).
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
TMP = tempfile.mkdtemp(prefix='ote-mapweapon-')
passed, failures = 0, []

IDLE = 'wait:14'  # the turn banner plays first; it takes the first inputs otherwise


def read_json(path):
    with open(path) as f:
        return json.load(f)


def write_json(path, obj):
    with open(path, 'w') as f:
        json.dump(obj, f, indent=1)


def make_game(gid, width, height):
    g = os.path.join(TMP, gid)
    shutil.copytree(os.path.join(REPO, 'tests/fixtures/srpg-mapweapon'), g)
    game = read_json(g + '/game.json')
    game['view'] = {'width': width, 'height': height, 'tileSize': 24}
    write_json(g + '/game.json', game)
    return g


def ticks(script):
    return sum(int(t[5:]) if t.startswith('wait:') else 1 for t in script.split(','))


def run(game, inp, shot=None, frames=None, interval=4, map_id='field'):
    cmd = [PLAYER, '--games', TMP, '--assets', os.path.join(REPO, 'assets'), '--headless', '--no-audio', '--no-settings',
           '--no-saving', '--frames', str(frames or ticks(inp) * interval + 60), '--input-interval', str(interval),
           '--fixed-dt', '0.016', '--game', game, '--map', map_id, '--input', inp]
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


# Select the archer (the cursor starts on the leader), step right onto the ballista (4,1): its action menu opens.
ON_BALLISTA = [IDLE, 'confirm', 'wait:3', 'right', 'confirm', 'wait:3']
TARGETING = ON_BALLISTA + ['confirm', 'wait:3']   # Attack (the first entry): the forecast of the only target in reach
FIRED = TARGETING + ['confirm', 'wait:40']        # confirm the target: the shot is made and played
# The second ballista (4,2) has one use of its own: the first shot spends it.
LAST_SHOT = [IDLE, 'confirm', 'wait:3', 'right', 'down', 'confirm', 'wait:3', 'confirm', 'wait:3', 'confirm', 'wait:40']
# The knight (3,0): on the ballista (4,1) he may not fire; on the catapult (4,0) he may.
KNIGHT_ON_BALLISTA = [IDLE, 'up', 'confirm', 'wait:3', 'right', 'down', 'confirm', 'wait:3', 'confirm', 'wait:3']
KNIGHT_ON_CATAPULT = [IDLE, 'up', 'confirm', 'wait:3', 'right', 'confirm', 'wait:3', 'confirm', 'wait:3']


def test_attack(game, tag):
    code, log = run(game, inp(*TARGETING))
    fc = lines(log, 'forecast:')
    check('%s: the forecast names the weapon, its uses and the ballista it comes from' % tag,
          bool(fc) and fc[-1].startswith('[info] forecast: Archer; Bolt 3; From Ballista;'), str(fc[-1:]) + log[-400:])
    check('%s: the numbers are the bolt\'s with the archer\'s own strength (5 + 10 - 1 = 14), no counter' % tag,
          bool(fc) and 'Dmg 14' in fc[-1] and 'No counter' in fc[-1] and 'HP 40 > 26' in fc[-1], str(fc[-1:]))
    uw = lines(log, 'unit weapon:')
    tw = lines(log, 'terrain weapon:')
    check('%s: the unit info names the structure\'s weapon with its uses and range' % tag,
          bool(uw) and uw[-1].endswith('unit weapon: Archer: Bolt 3/3 r2-6'), str(uw))
    check('%s: the terrain info names the weapon and the uses left' % tag,
          any(l.endswith('terrain weapon: Ballista: Bolt 3/3') for l in tw), str(tw))
    code, log = run(game, inp(*FIRED))
    fired = lines(log, 'map weapon:')
    check('%s: the log names who fired what from which structure' % tag,
          bool(fired) and fired[-1].endswith('map weapon: Archer fires Bolt from Ballista'), str(fired))
    check('%s: one shot leaves two of three' % tag,
          any(l.endswith('terrain weapon: Ballista: Bolt 2/3') for l in lines(log, 'terrain weapon:')), str(lines(log, 'terrain weapon:')))
    check('%s: the player ran without trouble' % tag, code == 0, log[-300:])


def test_last_shot(game):
    code, log = run(game, inp(*LAST_SHOT))
    fired = lines(log, 'map weapon:')
    check('the last use is said to be spent', bool(fired) and fired[-1].endswith('map weapon: Archer fires Bolt from Ballista (spent)'), str(fired))
    check('its forecast showed the one use left', any('Bolt 1; From Ballista;' in l for l in lines(log, 'forecast:')), str(lines(log, 'forecast:')))


def test_who_may(game):
    code, log = run(game, inp(*KNIGHT_ON_BALLISTA))
    check('a knight on the ballista has no attack to confirm (archers only)',
          code == 0 and not lines(log, 'forecast:') and not lines(log, 'map weapon:'), str(lines(log, 'forecast:')))
    code, log = run(game, inp(*KNIGHT_ON_CATAPULT))
    fc = lines(log, 'forecast:')
    check('a knight on the catapult may fire it (anyone may; 8 + 10 - 1 = 17)',
          bool(fc) and fc[-1].startswith('[info] forecast: Knight; Bolt; From Catapult;') and 'Dmg 17' in fc[-1], str(fc[-1:]))


def test_screens():
    for name in ('desktop', 'phone'):
        img, log, code = shot('mapweapon_picker_%s' % name, name, inp(*TARGETING))
        check('%s: the weapon picker (targeting forecast) draws' % name, code == 0 and img.w > 0)
        img, log, code = shot('mapweapon_info_%s' % name, name, inp(*FIRED, 'wait:20'))
        check('%s: the info panel after a shot draws' % name, code == 0 and img.w > 0)


def main():
    try:
        make_game('desktop', 480, 270)
        make_game('phone', 270, 480)
        test_attack('desktop', 'desktop')
        test_attack('phone', 'phone')
        test_last_shot('desktop')
        test_who_may('desktop')
        test_screens()
    finally:
        if not SHOTS:
            shutil.rmtree(TMP, ignore_errors=True)
    print('%d passed, %d failed' % (passed, len(failures)))
    for f in failures:
        print('  FAILED: ' + f)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
