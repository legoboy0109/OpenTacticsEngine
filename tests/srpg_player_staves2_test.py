#!/usr/bin/env python3
"""The player and the second stage of staves (docs/SRPG.md section 8: warp, pull, swap, repair, light), windowless.

  python3 tests/srpg_player_staves2_test.py ./build/engine/player/ote-player .
  python3 tests/srpg_player_staves2_test.py ./build/engine/player/ote-player . --shots OUT_DIR   # keep the screenshots (BMP)

Copies tests/fixtures/srpg-staves2 into a temp folder (two window sizes: 480x270 and a 270x480 phone), gives the Sage the staff a
flow needs, runs the player headless with --input scripts and reads the player's log lines, the Continue save it writes and the
pixels of the screenshots:

  - warp: the unit pick, then the square pick (with the forecast of each), Cancel steps back one stage at a time, a tap on a
    square that cannot be picked says why; a staff that picks no unit ("self") starts at the square;
  - pull and swap: the unit pick shows where they would end up; the move is made and keeps the unit's turn state;
  - repair: the unit pick, then a menu of the items that can be repaired (name, uses before > after), Cancel steps back;
  - light: the square pick with the area it would light, the light is saved, and the greyed reason on a map without fog;
  - the same flows on hexagon maps and in every kind of view (top-down, isometric, 3D isometric, full 3D);
  - the greyed reasons of the Staff list.
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
TMP = tempfile.mkdtemp(prefix='ote-staves2-')
passed, failures = 0, []

IDLE = 'wait:14'  # the turn banner plays first; it takes the first inputs otherwise
WIDTH = {'yard': 10, 'mist': 12, 'gate': 6}


def read_json(path):
    with open(path) as f:
        return json.load(f)


def write_json(path, obj):
    with open(path, 'w') as f:
        json.dump(obj, f, indent=1)


STYLES = [{'id': 'top', 'name': 'Top'}, {'id': 'iso', 'name': 'Iso', 'view': 'isometric'},
          {'id': 'iso3', 'name': 'Iso 3D', 'view': 'isometric-3d'}, {'id': 'full3', 'name': 'Full 3D', 'view': '3d'}]


def make_game(gid, width, height, tweak=None, styles=False):
    """A copy of the fixture; `tweak(game_dir)` may change its files."""
    g = os.path.join(TMP, gid)
    shutil.copytree(os.path.join(REPO, 'tests/fixtures/srpg-staves2'), g)
    game = read_json(g + '/game.json')
    game['view'] = {'width': width, 'height': height, 'tileSize': 24}
    if styles:
        game['styles'] = STYLES
    write_json(g + '/game.json', game)
    if tweak:
        tweak(g)
    return g


def edit_map(g, name, fn):
    path = '%s/maps/%s.json' % (g, name)
    m = read_json(path)
    fn(m)
    write_json(path, m)


def unit_of(m, uid):
    return [u for u in m['units'] if u.get('id') == uid][0]


def sage_has(*items, **moves):
    """A tweak: the Sage of the yard carries only these staves; `moves` = story id -> (x, y); `ren_items` replaces Ren's items."""
    def tweak(g):
        def fn(m):
            unit_of(m, 'sage')['items'] = list(items)
            for uid, xy in moves.items():
                if uid != 'ren_items':
                    unit_of(m, uid)['x'], unit_of(m, uid)['y'] = xy
            if 'ren_items' in moves:
                unit_of(m, 'ren')['items'] = moves['ren_items']
        edit_map(g, 'yard', fn)
    return tweak


def ticks(script):
    return sum(int(t[5:]) if t.startswith('wait:') else 1 for t in script.split(','))


def run(gid, inp, map_id='yard', style=None, shot=None, interval=4):
    """Runs the player on a fresh battle; returns (code, log, the units of the Continue save written afterwards by story id, the battle)."""
    work = os.path.join(TMP, 'saves_%s_%s' % (gid, map_id))
    shutil.rmtree(work, ignore_errors=True)
    cmd = [PLAYER, '--games', TMP, '--assets', os.path.join(REPO, 'assets'), '--headless', '--no-audio', '--no-settings',
           '--saves', work, '--frames', str(ticks(inp) * interval + 90), '--input-interval', str(interval),
           '--fixed-dt', '0.016', '--game', gid, '--map', map_id, '--input', inp]
    if style:
        cmd += ['--style', style]
    if shot:
        cmd += ['--screenshot', shot]
    p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True, timeout=300)
    units, battle = {}, {}
    try:
        battle = read_json(os.path.join(work, gid, 'continue.json'))['battle']
        for u in battle['units']:
            units[u.get('id') or u['name']] = u
    except (OSError, ValueError, KeyError):
        pass
    return p.returncode, p.stdout, units, battle


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

    def brightness(self):
        return sum(p[0] + p[1] + p[2] for row in self.px for p in row)

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


def shot(name, gid, inp, map_id='yard', style=None):
    path = os.path.join(TMP, name + '.bmp')
    code, log, units, battle = run(gid, inp, map_id, style, shot=path)
    if SHOTS:
        os.makedirs(SHOTS, exist_ok=True)
        shutil.copy(path, os.path.join(SHOTS, name + '.bmp'))
    return Image(path), log, units, battle, code


def lines(log, key):
    return [l for l in log.splitlines() if key in l]


def xy(unit, width):
    return (unit['cell'] % width, unit['cell'] // width)


def inp(*parts):
    return ','.join(parts)


# Sage is the first unit under the cursor: confirm selects her, confirm again opens her action menu (Staff first).
MENU = [IDLE, 'confirm', 'wait:3', 'confirm', 'wait:3']
LIST = MENU + ['confirm', 'wait:3']              # the Staff list
FIRST = LIST + ['confirm', 'wait:3']             # the first staff of the list chosen


def hex_neighbours(x, y, layout):
    if layout == 'pointy':
        return ([(x, y - 1), (x + 1, y), (x, y + 1), (x - 1, y + 1), (x - 1, y), (x - 1, y - 1)] if y % 2 == 0
                else [(x + 1, y - 1), (x + 1, y), (x + 1, y + 1), (x, y + 1), (x - 1, y), (x, y - 1)])
    return ([(x, y - 1), (x + 1, y - 1), (x + 1, y), (x, y + 1), (x - 1, y), (x - 1, y - 1)] if x % 2 == 0
            else [(x, y - 1), (x + 1, y), (x + 1, y + 1), (x, y + 1), (x - 1, y + 1), (x - 1, y)])


# ---- warp ----

def test_warp(gid, tag):
    # Pick Bram (second candidate; Wisp is first), then the first free square in reach: 0,0.
    script = inp(*FIRST, 'next', 'wait:2', 'confirm', 'wait:3', 'confirm', 'wait:30')
    img, log, units, battle, code = shot('warp_%s' % tag, gid, script)
    check('%s warp: choosing the staff says what it does' % tag, any('Warps a unit up to 4 squares away' in l for l in lines(log, 'staff chosen:')), log[-300:])
    fc = lines(log, 'staff forecast:')
    check('%s warp: the unit pick forecast says a square is still to pick' % tag, any(f.endswith('Warp Staff 5; Bram: pick a square; Exp 10; ') for f in fc), str(fc))
    check('%s warp: then the square pick, with its forecast' % tag, bool(lines(log, 'pick square:')) and any(f.endswith('Warp Staff 5; Bram warps there; Exp 10; ') for f in fc), str(fc))
    check('%s warp: the hint line asks for the square' % tag, bool(lines(log, 'staff hint: Pick a square to send it to')))
    check('%s warp: Bram stands on the chosen square, the use is spent, Sage has her turn over and her experience' % tag,
          code == 0 and xy(units['bram'], 10) == (0, 0) and units['sage']['items'][0]['uses'] == 4 and units['sage']['done'] and units['sage']['xp'] == 10,
          str((units.get('bram'), units.get('sage'))))
    check('%s warp: the move keeps its turn state (Bram has not moved or finished)' % tag, not units['bram']['moved'] and not units['bram']['done'])


def test_warp_cancel(gid, tag):
    base = len(lines(run(gid, inp(*MENU))[1], 'staff entry of Sage'))  # (the action menu opened once: how often that is logged)
    script = inp(*FIRST, 'next', 'wait:2', 'confirm', 'wait:3', 'cancel', 'wait:3')
    code, log, units, battle = run(gid, script)
    check('%s warp: Cancel on the square pick goes back to the unit pick (not the menu)' % tag,
          len(lines(log, 'pick unit:')) == 2 and len(lines(log, 'pick square:')) == 1 and len(lines(log, 'staff entry of Sage')) == base, str(lines(log, 'pick ')))
    code, log, units, battle = run(gid, inp(script, 'cancel', 'wait:3', 'wait:5'))
    check('%s warp: Cancel on the unit pick goes back to the action menu, and nothing was used' % tag,
          len(lines(log, 'staff entry of Sage')) == base + 1 and units['sage']['items'][0]['uses'] == 5 and not units['sage']['done'], str(lines(log, 'staff entry')))


def test_warp_taps(gid):
    # Desktop only (the squares are at known places): Bram at 2,4 and the square 1,1; then squares that cannot be picked.
    cell = lambda x, y: 'tap:%d:%d' % (68 + 24 * x, 69 + 24 * y)
    script = inp(*FIRST, cell(2, 4), cell(2, 4), 'wait:3', cell(1, 1), 'wait:2', cell(9, 5), 'wait:2', cell(4, 1), 'wait:2', cell(1, 1), 'wait:25')
    code, log, units, battle = run(gid, script)
    refused = [l.split('pick refused: ')[1] for l in lines(log, 'pick refused:')]
    check('%s: a tap on a square too far says why, and so does one the unit could not stand on' % 'tap', refused == ['Out of range', 'Cannot stand there'], str(refused))
    check('tap: two taps pick the unit and two the square: Bram is on 1,1', xy(units['bram'], 10) == (1, 1), str(units.get('bram')))


def test_hop(gid, tag):
    # A "self" staff: no unit to pick, it starts at the square. Sage goes to the first square in reach: 1,0.
    script = inp(*FIRST, 'confirm', 'wait:30')
    code, log, units, battle = run(gid, script)
    check('%s hop: a staff of "self" asks for no unit, only the square' % tag, not lines(log, 'pick unit:') and len(lines(log, 'pick square:')) == 1, str(lines(log, 'pick ')))
    check('%s hop: the user moved there and its turn is over' % tag, xy(units['sage'], 10) == (1, 0) and units['sage']['done'], str(units.get('sage')))
    code, log, units, battle = run(gid, inp(*FIRST, 'cancel', 'wait:3', 'wait:5'))
    check('%s hop: Cancel at the square goes back to the action menu' % tag, len(lines(log, 'staff entry of Sage')) == 2 and not lines(log, 'pick unit:'), str(lines(log, 'staff entry')))


# ---- pull and swap ----

def test_pull(gid, tag):
    img, log, units, battle, code = shot('pull_%s' % tag, gid, inp(*FIRST))
    fc = lines(log, 'staff forecast:')
    check('%s pull: the forecast says where the unit would end up' % tag, any(f.endswith('Recall Staff 4; Ren is pulled next to you; ') for f in fc), str(fc))
    code, log, units, battle = run(gid, inp(*FIRST, 'confirm', 'wait:30'))
    check('%s pull: Ren lands on the free square next to the user with the lowest number (2,1)' % tag, code == 0 and xy(units['ren'], 10) == (2, 1), str(units.get('ren')))
    check('%s pull: Ren keeps his turn state; Sage used the staff' % tag, not units['ren']['done'] and units['sage']['done'] and units['sage']['items'][0]['uses'] == 3)
    code, log, units, battle = run(gid, inp(*FIRST, 'cancel', 'wait:3', 'wait:5'))
    check('%s pull: Cancel on the unit pick goes back to the action menu' % tag, len(lines(log, 'staff entry of Sage')) == 2, str(lines(log, 'staff entry')))


def test_pull_reason(gid):
    cell = lambda x, y: 'tap:%d:%d' % (68 + 24 * x, 69 + 24 * y)
    code, log, units, battle = run(gid, inp(*FIRST, cell(3, 2), 'wait:3'))  # Wisp is already next to Sage
    refused = [l.split('pick refused: ')[1] for l in lines(log, 'pick refused:')]
    check('pull: a tap on a unit that is not a valid target says why', refused == ['Already next to you'], str(refused))


def test_swap(gid, tag):
    img, log, units, battle, code = shot('swap_%s' % tag, gid, inp(*FIRST))
    fc = lines(log, 'staff forecast:')
    check('%s swap: the forecast says they trade places' % tag, any(f.endswith('Swap Staff 4; Bram and Sage swap places; ') for f in fc), str(fc))
    code, log, units, battle = run(gid, inp(*FIRST, 'confirm', 'wait:30'))
    check('%s swap: Sage and Bram traded squares' % tag, code == 0 and xy(units['sage'], 10) == (2, 4) and xy(units['bram'], 10) == (2, 2), str((units.get('sage'), units.get('bram'))))


# ---- repair ----

WORN = [{'item': 'iron_blade', 'uses': 3}, {'item': 'glass_blade', 'uses': 1}, 'short_sword', {'item': 'herb', 'uses': 1}]


def test_repair(gid, tag):
    script = inp(*FIRST, 'confirm', 'wait:3')
    img, log, units, battle, code = shot('repair_%s' % tag, gid, script)
    check('%s repair: a menu of the items that can be repaired (not the glass blade nor the sword)' % tag,
          any(l.endswith('Iron Blade 3>8, Herb 1>3') for l in lines(log, 'staff repair choices for Ren:')), str(lines(log, 'staff repair')))
    check('%s repair: the hint asks which item' % tag, bool(lines(log, 'staff hint: Repair which?')))
    code, log, units, battle = run(gid, inp(script, 'down', 'wait:2', 'confirm', 'wait:30'))
    herb = [i for i in units['ren']['items'] if i['item'] == 16][0]
    iron = [i for i in units['ren']['items'] if i['item'] == 12][0]
    check('%s repair: the picked item is mended (and only that one)' % tag, code == 0 and herb['uses'] == 3 and iron['uses'] == 3, str(units['ren']['items']))
    check('%s repair: the use is spent and Sage has her experience' % tag, units['sage']['items'][0]['uses'] == 3 and units['sage']['xp'] == 5)
    code, log, units, battle = run(gid, inp(script, 'cancel', 'wait:3', 'wait:5'))
    check('%s repair: Cancel in the item menu goes back to the unit pick' % tag, len(lines(log, 'pick unit:')) == 2 and units['sage']['items'][0]['uses'] == 4, str(lines(log, 'pick ')))
    code, log, units, battle = run(gid, inp(script, 'confirm', 'wait:30'))
    iron = [i for i in units['ren']['items'] if i['item'] == 12][0]
    check('%s repair: the first item, the iron blade, goes back to full' % tag, iron['uses'] == 8, str(units['ren']['items']))


# ---- light ----

def test_light(gid, tag):
    # The last square of the pick (prev from the first) is 5,4: radius 2 around it.
    script = inp(*FIRST, 'prev', 'wait:2')
    img0, log, units, battle, code = shot('light_pick_%s' % tag, gid, script, 'mist')
    fc = lines(log, 'staff forecast:')
    check('%s light: the pick shows how many squares come into sight and for how long' % tag, any('Lantern Staff 3; Lights ' in f and '2 turns' in f for f in fc), str(fc))
    check('%s light: the hint line asks for the square' % tag, bool(lines(log, 'staff hint: Pick a square to light')))
    img1, log, units, battle, code = shot('light_done_%s' % tag, gid, inp(script, 'confirm', 'wait:30'), 'mist')
    lights = battle.get('lights', [])
    check('%s light: the light is saved with the battle' % tag, lights == [{'side': 0, 'cell': 4 * 12 + 5, 'radius': 2, 'turns': 2}], str(lights))
    check('%s light: Sage used the staff and has her experience' % tag, units['sage']['items'][0]['uses'] == 2 and units['sage']['done'] and units['sage']['xp'] == 5)
    check('%s light: more of the map is in sight afterwards' % tag, img1.brightness() > img0.brightness(), '%d %d' % (img1.brightness(), img0.brightness()))
    code, log, units, battle = run(gid, inp(script, 'cancel', 'wait:3', 'wait:5'), 'mist')
    check('%s light: Cancel goes back to the action menu' % tag, len(lines(log, 'staff entry of Sage')) == 2 and 'lights' not in battle, str(lines(log, 'staff entry')))


def test_light_without_fog(gid):
    code, log, units, battle = run(gid, inp(*LIST), 'yard')
    lst = lines(log, 'staff list:')
    check('light: on a map without fog the list says why it cannot be used',
          bool(lst) and lst[-1].endswith('Lantern Staff [No fog on this map], Fix Staff [Nothing in reach to repair], Warp Staff 5/5'), str(lst[-1:]))
    code, log, units, battle = run('nofog_only', inp(*MENU), 'yard')
    entry = lines(log, 'staff entry of Sage')
    check('light: with nothing else to use the Staff entry is greyed with the reason', bool(entry) and entry[-1].endswith('greyed: No fog on this map'), str(entry[-1:]))


# ---- other kinds of view, hexagons ----

def test_kinds(gid):
    for style in ('top', 'iso', 'iso3', 'full3'):
        for name, staff, want in (('warp', 'warp_staff', lambda u: xy(u['bram'], 10) == (0, 0)),
                                  ('pull', 'recall_staff', lambda u: xy(u['ren'], 10) != (8, 2))):
            def tweak(g, staff=staff):
                sage_has(staff)(g)
            g = 'kind_%s_%s' % (name, style)
            make_game(g, 480, 270, tweak, styles=True)
            script = inp(*FIRST, 'next', 'wait:2', 'confirm', 'wait:3', 'confirm', 'wait:30') if name == 'warp' else inp(*FIRST, 'confirm', 'wait:30')
            img, log, units, battle, code = shot(g, g, script, style=style)
            check('%s in the %s view works end to end' % (name, style), code == 0 and want(units) and bool(lines(log, 'staff:')), str((code, units.get('bram'), units.get('ren'))))
    g = 'kind_light_iso3'
    make_game(g, 480, 270, lambda d: edit_map(d, 'mist', lambda m: unit_of(m, 'sage').update({'items': ['lantern_staff']})), styles=True)
    for style in ('top', 'iso', 'iso3', 'full3'):
        img, log, units, battle, code = shot('kind_light_' + style, g, inp(*FIRST, 'prev', 'wait:2', 'confirm', 'wait:30'), 'mist', style=style)
        check('light in the %s view works end to end' % style, code == 0 and len(battle.get('lights', [])) == 1, str((code, battle.get('lights'))))


def hexify(layout):
    def tweak(g):
        edit_map(g, 'yard', lambda m: m['grid'].update({'type': 'hex', 'layout': layout}))
        game = read_json(g + '/game.json')
        game['grid'] = {'type': 'hex', 'layout': layout}
        write_json(g + '/game.json', game)
    return tweak


def test_hex(layout):
    for name, staff, tokens in (('pull', 'recall_staff', ['confirm', 'wait:30']),
                                ('swap', 'swap_staff', ['confirm', 'wait:30']),
                                ('warp', 'warp_staff', ['next', 'wait:2', 'confirm', 'wait:3', 'confirm', 'wait:30'])):
        g = 'hex_%s_%s' % (name, layout)

        def tweak(d, staff=staff):
            hexify(layout)(d)
            sage_has(staff)(d)
        make_game(g, 480, 270, tweak)
        img, log, units, battle, code = shot(g, g, inp(*FIRST, *tokens))
        sage, ren, bram = xy(units['sage'], 10), xy(units['ren'], 10), xy(units['bram'], 10)
        if name == 'pull':
            ok = ren in hex_neighbours(2, 2, layout)  # sage did not move
            check('hexagons (%s): a pulled unit lands on a hexagon next to the user' % layout, code == 0 and ok and sage == (2, 2), str((sage, ren)))
        elif name == 'swap':
            check('hexagons (%s): a swap trades the two hexagons' % layout, code == 0 and sage == (2, 4) and bram == (2, 2), str((sage, bram)))
        else:
            check('hexagons (%s): a warp puts the unit on the picked hexagon' % layout, code == 0 and bram != (2, 4) and bool(lines(log, 'staff:')), str(bram))


def test_plain_game():
    shutil.copytree(os.path.join(REPO, 'games/demo-srpg'), os.path.join(TMP, 'plain'))
    p = subprocess.run([PLAYER, '--games', TMP, '--assets', os.path.join(REPO, 'assets'), '--headless', '--no-audio', '--no-settings',
                        '--no-saving', '--frames', '80', '--fixed-dt', '0.016', '--game', 'plain', '--map', 'bridge'],
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True, timeout=300)
    check('a game without staves plays as before (no staff entry anywhere)', p.returncode == 0 and 'staff' not in p.stdout.lower(), p.stdout[-300:])


def main():
    try:
        for tag, w, h in (('desktop', 480, 270), ('phone', 270, 480)):
            make_game('warp_' + tag, w, h, sage_has('warp_staff'))
            make_game('hop_' + tag, w, h, sage_has('hop_staff'))
            make_game('pull_' + tag, w, h, sage_has('recall_staff'))
            make_game('swap_' + tag, w, h, sage_has('swap_staff'))
            make_game('repair_' + tag, w, h, sage_has('fix_staff', ren_items=WORN, ren=(2, 3)))
            make_game('light_' + tag, w, h, lambda g: edit_map(g, 'mist', lambda m: unit_of(m, 'sage').update({'items': ['lantern_staff']})))
            test_warp('warp_' + tag, tag)
            test_warp_cancel('warp_' + tag, tag)
            test_hop('hop_' + tag, tag)
            test_pull('pull_' + tag, tag)
            test_swap('swap_' + tag, tag)
            test_repair('repair_' + tag, tag)
            test_light('light_' + tag, tag)
        test_warp_taps('warp_desktop')
        make_game('pullreason', 480, 270, sage_has('recall_staff'))
        test_pull_reason('pullreason')
        make_game('nofog', 480, 270, sage_has('lantern_staff', 'fix_staff', 'warp_staff'))
        make_game('nofog_only', 480, 270, sage_has('lantern_staff'))
        test_light_without_fog('nofog')
        test_kinds('kinds')
        test_hex('pointy')
        test_hex('flat')
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
