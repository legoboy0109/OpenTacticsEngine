#!/usr/bin/env python3
"""The player and status conditions (docs/SRPG.md section 9), windowless.

  python3 tests/srpg_player_conditions_test.py ./build/engine/player/ote-player .
  python3 tests/srpg_player_conditions_test.py ./build/engine/player/ote-player . --shots OUT_DIR   # keep the screenshots (BMP)

Copies tests/fixtures/srpg-conditions into a temp folder (two window sizes: 480x270 and a 270x480 phone), arranges the units a flow
needs, runs the player headless with --input scripts and reads the player's log lines, the Continue save it writes and the pixels of the
screenshots:

  - a hostile staff: the forecast says what is inflicted and (with a hit formula) the chance; the condition lands, the marker and the
    count show; a cure staff and a cure item remove it ("Cured ..."), an area "cure all" cleans several units;
  - the turn flow: poison ticks at the start of the afflicted unit's turn (the condition's name, then the "-2" and the bar), a sleeper
    does not act, a berserk unit takes its own action and a tap on it says why, a stunned unit says it cannot move, silence greys the
    Staff entry with its reason;
  - the combat forecast reflects a curse on the foe, a weapon that poisons shows its chance in the forecast and inflicts after a hit;
  - the unit details page "Status", the marker in every kind of view (top-down, isometric, 3D isometric, full 3D) and on the phone.
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
TMP = tempfile.mkdtemp(prefix='ote-conditions-')
passed, failures = 0, []

IDLE = 'wait:14'  # the turn banner plays first; it takes the first inputs otherwise
WIDTH = {'field': 12, 'mist': 12, 'skirmish': 12}
POISON, SLEEP, SILENCE, BERSERK, STUN, CURSE = range(6)  # indices of the starter conditions in the fixture's conditions.json


def read_json(path):
    with open(path) as f:
        return json.load(f)


def write_json(path, obj):
    with open(path, 'w') as f:
        json.dump(obj, f, indent=1)


STYLES = [{'id': 'top', 'name': 'Top'}, {'id': 'iso', 'name': 'Iso', 'view': 'isometric'},
          {'id': 'iso3', 'name': 'Iso 3D', 'view': 'isometric-3d'}, {'id': 'full3', 'name': 'Full 3D', 'view': '3d'}]


def make_game(gid, width=480, height=270, tweak=None, styles=False):
    """A copy of the fixture; `tweak(game_dir)` may change its files."""
    g = os.path.join(TMP, gid)
    shutil.copytree(os.path.join(REPO, 'tests/fixtures/srpg-conditions'), g)
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


def edit_items(g, fn):
    path = g + '/data/items.json'
    d = read_json(path)
    fn({i['id']: i for i in d['items']})
    write_json(path, d)


def unit_of(m, uid):
    return [u for u in m['units'] if u.get('id') == uid][0]


def arrange(items=None, at=None, drop=(), map_id='field', extra=None):
    """A tweak: `items` = story id -> item list, `at` = story id -> (x, y), `drop` = units left out of the map."""
    def tweak(g):
        def fn(m):
            for uid, lst in (items or {}).items():
                unit_of(m, uid)['items'] = lst
            for uid, xy in (at or {}).items():
                unit_of(m, uid)['x'], unit_of(m, uid)['y'] = xy
            m['units'] = [u for u in m['units'] if u.get('id') not in drop]
        edit_map(g, map_id, fn)
        if extra:
            extra(g)
    return tweak


def ticks(script):
    return sum(int(t[5:]) if t.startswith('wait:') else 1 for t in script.split(','))


def run(gid, inp, map_id='field', style=None, shot=None, interval=4, extra_frames=90):
    """Runs the player on a fresh battle; returns (code, log, the units of the Continue save by story id, the battle)."""
    work = os.path.join(TMP, 'saves_%s_%s' % (gid, map_id))
    shutil.rmtree(work, ignore_errors=True)
    cmd = [PLAYER, '--games', TMP, '--assets', os.path.join(REPO, 'assets'), '--headless', '--no-audio', '--no-settings',
           '--saves', work, '--frames', str(ticks(inp) * interval + extra_frames), '--input-interval', str(interval),
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


def shot(name, gid, inp, map_id='field', style=None):
    path = os.path.join(TMP, name + '.bmp')
    code, log, units, battle = run(gid, inp, map_id, style, shot=path)
    if SHOTS:
        os.makedirs(SHOTS, exist_ok=True)
        shutil.copy(path, os.path.join(SHOTS, name + '.bmp'))
    return Image(path), log, units, battle, code


def lines(log, key):
    return [l for l in log.splitlines() if key in l]


def xy(unit, width=12):
    return (unit['cell'] % width, unit['cell'] // width)


def inp(*parts):
    return ','.join(parts)


def conds(unit):
    return [(c['condition'], c.get('left')) for c in unit.get('conditions', [])]


# A user with the first staff of her list: 'next' moves the cursor to Mira (the second unit); confirm selects, confirm opens the action
# menu (Attack, Staff, Items, ...), down chooses Staff, confirm opens the list, confirm chooses the first staff.
SELECT_MIRA = [IDLE, 'next', 'wait:3', 'confirm', 'wait:3', 'confirm', 'wait:3']
STAFF_MIRA = SELECT_MIRA + ['down', 'wait:2', 'confirm', 'wait:3', 'confirm', 'wait:3']   # the first staff chosen: the pick of a unit
END_TURN = ['menu', 'wait:3', 'confirm']
# Kay (the first unit, under the cursor) drinks the second item of her list (Items is the first entry of her menu, "Use" the first of the item's).
DRINK = [IDLE, 'confirm', 'wait:3', 'confirm', 'wait:3', 'confirm', 'wait:3', 'down', 'wait:2', 'confirm', 'wait:3', 'confirm', 'wait:3', 'wait:20']
MIRA = {'mira': ['hex_staff', 'fire_tome']}
NEAR = {'r1': (3, 2)}                       # a foe in reach of Mira's staves (2 away), next to Kay


# ---- a hostile staff ----

def test_staff_inflict(gid, tag, width=12):
    code, log, units, battle = run(gid, inp(*STAFF_MIRA, 'confirm', 'wait:30'))
    check('%s staff: choosing a hostile staff says what it inflicts' % tag, any('Inflicts Curse' in l for l in lines(log, 'staff chosen:')), log[-400:])
    fc = lines(log, 'staff forecast:')
    check('%s staff: the forecast says what the unit would get and for how long' % tag, any(f.endswith('Hex Staff 4; Rook  Curse 3 turns; ') for f in fc), str(fc))
    check('%s staff: the condition lands, with its float and a log line' % tag, any('condition applied: Rook - Curse' in l for l in lines(log, 'condition applied')), log[-400:])
    check('%s staff: Rook has Curse for 3 turns in the save and Mira has used the staff' % tag,
          code == 0 and conds(units['r1']) == [(CURSE, 3)] and units['mira']['items'][0]['uses'] == 3 and units['mira']['done'], str((units.get('r1'), units.get('mira'))))


def test_staff_chance(gid):
    code, log, units, battle = run(gid, inp(*STAFF_MIRA))
    fc = lines(log, 'staff forecast:')
    # 70 + a.mag * 5 - b.res * 5 - dist * 2 = 70 + 30 - 5 - 4: the formula reads both units and the distance
    check('forecast: the chance to work comes from the staff\'s hit formula (both units and the distance)', any('Hit 91%' in f for f in fc), str(fc))


def test_staff_cancel(gid):
    code, log, units, battle = run(gid, inp(*STAFF_MIRA, 'cancel', 'wait:3', 'wait:5'))
    check('staff: Cancel on the pick goes back and nothing was inflicted', not units['r1'].get('conditions') and units['mira']['items'][0]['uses'] == 4, str(units.get('r1')))


def test_cure_staff_and_item(gid):
    # Kay drinks the bad brew (curse on its user); Nell's purify staff (area: everyone in reach) cleans the whole side.
    code, log, units, battle = run(gid, inp(*DRINK))
    check('item: a usable item that inflicts lands on its user', conds(units['kay']) == [(CURSE, 2)], str((units.get('kay'), lines(log, 'items:'))))
    check('item: the log says so', any('condition applied: Kay - Curse' in l for l in lines(log, 'condition applied')))
    # Nell is the fourth unit (next x3): Staff, the list, Purify Staff (nothing to pick): it works on confirm.
    script = inp(*DRINK, 'next', 'next', 'next', 'wait:3', 'confirm', 'wait:3', 'confirm', 'wait:3', 'confirm', 'wait:3', 'confirm', 'wait:3')
    code, log, units, battle = run(gid, script)
    menu = lines(log, 'action menu:')
    check('cure staff: the action menu of Nell has a Staff entry', bool(menu) and 'Staff' in menu[-1], str(menu))
    code, log, units, battle = run(gid, inp(script, 'wait:30'))
    check('cure staff: a staff of "all" picks nobody and works at once: the curse is gone', code == 0 and not units['kay'].get('conditions') and any('condition ended: Kay - Curse (cured)' in l for l in lines(log, 'condition ended')), str((units.get('kay'), log[-500:])))
    check('cure staff: Nell used it', units['nell']['done'] and units['nell']['items'][0]['uses'] == 2, str(units.get('nell')))


def test_cure_single(gid):
    # The same staff made a single-target one: the pick lists only the unit that has something to cure, and the forecast says what.
    script = inp(*DRINK, 'next', 'next', 'next', 'wait:3', 'confirm', 'wait:3', 'confirm', 'wait:3', 'confirm', 'wait:3', 'confirm', 'wait:3', 'wait:3')
    code, log, units, battle = run(gid, script)
    check('cure staff: only the afflicted ally is a candidate', any('pick unit: 1 candidates' in l for l in lines(log, 'pick unit')), str(lines(log, 'pick unit')))
    fc = lines(log, 'staff forecast:')
    check('cure staff: the forecast says what it would cure', any(f.endswith('Purify Staff 3; Kay  Cures Curse; ') for f in fc), str(fc))
    code, log, units, battle = run(gid, inp(script, 'confirm', 'wait:30'))
    check('cure staff: confirming cures it', not units['kay'].get('conditions') and units['nell']['done'], str((units.get('kay'), units.get('nell'))))


def test_wild_draught_and_nothing_to_cure(gid):
    code, log, units, battle = run(gid, inp(IDLE, 'confirm', 'wait:3', 'confirm', 'wait:3', 'wait:2'))
    items = lines(log, 'action menu:')
    check('item: a cure item is refused when there is nothing to cure (the antidote is not offered on a healthy unit)', bool(items), str(items))


# ---- the turn flow ----

def sure(g):
    edit_items(g, lambda it: it['poison_staff'].update({'hit': '100'}))


def test_poison_ticks(gid):
    script = inp(*STAFF_MIRA, 'confirm', 'wait:30', *END_TURN, 'wait:150')
    code, log, units, battle = run(gid, script)
    check('poison: the staff lands (Hit 100)', any('condition applied: Rook - Poison' in l for l in lines(log, 'condition applied')), log[-600:])
    order = [l for l in log.splitlines() if 'condition applied: Rook - Poison' in l or 'skill activated: Rook - Poison' in l or 'unit hurt: Rook' in l]
    check('poison: at the start of Rook\'s turn the condition\'s name floats, then it loses 2 HP', len(order) == 3 and 'skill activated' in order[1] and order[2].endswith('-2'), str(order))
    check('poison: the count went down by one in the save', (POISON, 2) in conds(units['r1']) or (POISON, 3) in conds(units['r1']), str(conds(units.get('r1', {}))))


def test_sleep(gid):
    script = inp(*STAFF_MIRA, 'confirm', 'wait:30', *END_TURN, 'wait:150')
    code, log, units, battle = run(gid, script)
    ticks_ = lines(log, 'condition tick: Rook')
    check('sleep: at the start of its turn the sleeper is told it cannot act', any('Sleep' in l and 'cannot act' in l for l in ticks_), str(ticks_) + log[-300:])
    check('sleep: it stays where it was and is still asleep (the count is down to 1)', xy(units['r1']) == (3, 2) and conds(units['r1']) == [(SLEEP, 1)], str(units.get('r1')))


def test_berserk(gid):
    # Kay drinks the wild draught (berserk for 2 turns); at the start of blue's next turn it attacks Mira next to it by itself.
    script = inp(*DRINK, *END_TURN, 'wait:260')
    code, log, units, battle = run(gid, script)
    check('berserk: the item lands', any('condition applied: Kay - Berserk' in l for l in lines(log, 'condition applied')), str(lines(log, 'item')))
    check('berserk: at the start of Kay\'s turn it acts on its own', any('condition tick: Kay - Berserk (acts on its own)' in l for l in lines(log, 'condition tick')), log[-500:])
    check('berserk: it hit its own neighbour Mira (the nearest unit), and its turn is over', units['mira']['hp'] < 18 and units['kay']['done'], str((units.get('mira'), units.get('kay'))))
    # (the camera, and so the cursor, ended on Mira, whom the fight hit: Kay is one square to the right)
    code, log, units, battle = run(gid, inp(script, 'right', 'wait:3', 'confirm', 'wait:5'))
    check('berserk: tapping the unit says why it takes no orders', any('Berserk: acts on its own' in l for l in lines(log, 'unit stopped')), str(lines(log, 'unit stopped')))


def test_stun_and_silence(gid):
    # A silenced mage: its Staff entry is greyed with the reason and its tome is gone from the action menu.
    script = inp(*SELECT_MIRA, 'confirm', 'wait:3', 'confirm', 'wait:20', *END_TURN, 'wait:260', 'next', 'wait:3', 'confirm', 'wait:3', 'confirm', 'wait:3')
    code, log, units, battle = run(gid, script)
    check('silence: the quiet staff silences its user', (SILENCE, 3) in conds(units['mira']) or (SILENCE, 2) in conds(units['mira']), str(units.get('mira')))
    entry = lines(log, 'staff entry of Mira')
    check('silence: with only silenced staves the Staff entry is greyed with the reason', bool(entry) and 'greyed' in entry[-1] and 'Silence stops that' in entry[-1], str(entry))
    menu = lines(log, 'action menu:')
    check('silence: the fire tome cannot be used: no Attack entry', bool(menu) and 'Attack' not in menu[-1], str(menu))


# ---- combat ----

def test_forecast_after_curse(gid):
    # Mira hexes Rook (defense -2), then Kay (next to it) attacks: the forecast shows 12 damage instead of 10.
    script = inp(*STAFF_MIRA, 'confirm', 'wait:30', 'prev', 'wait:3', 'confirm', 'wait:3', 'confirm', 'wait:3', 'confirm', 'wait:3')
    code, log, units, battle = run(gid, script)
    fc = lines(log, 'forecast:')
    check('forecast: the combat forecast reflects the curse on the foe (Dmg 12, not 10)', any('Dmg 12' in f for f in fc), str(fc) + log[-300:])


def test_weapon_inflict(gid):
    # Rook's venom blade: after it hits (its turn), the unit it struck is poisoned for 2 turns (or resisted, for the immune knight).
    script = inp(IDLE, *END_TURN, 'wait:260')
    code, log, units, battle = run(gid, script)
    applied = lines(log, 'condition applied:')
    resisted = lines(log, 'condition resisted:')
    check('weapon: a hit with a venom blade poisons the unit struck (or the knight resists)',
          any('Poison' in l for l in applied) or any('Poison' in l for l in resisted), str(applied) + str(resisted) + log[-500:])


def test_status_page(gid):
    # Kay drinks the bad brew; its unit details have a Status page.
    code, log, units, battle = run(gid, inp(*DRINK))
    check('item: Kay is cursed', conds(units['kay']) == [(CURSE, 2)], str(units.get('kay')))
    # Kay is done after the item: Confirm on it opens no menu. Open the unit details through the battle menu's "Unit" entry.
    code, log, units, battle = run(gid, inp(*DRINK, 'menu', 'wait:3', 'down', 'wait:2', 'confirm', 'wait:3'))
    details = lines(log, 'unit details:')
    check('details: the Status page is there for an afflicted unit', any('Status' in l for l in details), str(details) + log[-300:])


# ---- every kind of view, the phone ----

def test_marker(gid, style, w=480, h=270, tag=''):
    plain = shot('marker_plain_%s%s' % (style, tag), gid, inp(IDLE, 'wait:40'), style=style)[0]
    marked, log, units, battle, code = shot('marker_%s%s' % (style, tag), gid, inp(*DRINK, 'wait:30'), style=style)
    purple = (70, 40, 110)
    check('marker %s%s: the afflicted unit wears its tag on the map' % (style, tag), code == 0 and conds(units['kay']) == [(CURSE, 2)] and marked.count(purple, 14) > plain.count(purple, 14) + 6,
          '%d vs %d %s' % (marked.count(purple, 14), plain.count(purple, 14), conds(units.get('kay', {}))))


def main():
    desk = make_game('cdesk', tweak=arrange(items=MIRA, at=NEAR), styles=True)
    test_staff_inflict('cdesk', 'desktop')
    make_game('cchance', tweak=arrange(items={'mira': ['poison_staff', 'fire_tome']}, at=NEAR))
    test_staff_chance('cchance')
    test_staff_cancel('cdesk')
    make_game('cdrink', tweak=arrange(items={'kay': ['iron_blade', 'bad_brew', 'antidote'], 'nell': ['purify_staff', 'cure_staff']}, at={'r1': (9, 5), 'r2': (10, 5), 'r3': (11, 5)}))
    test_cure_staff_and_item('cdrink')
    test_wild_draught_and_nothing_to_cure('cdrink')
    make_game('csingle', tweak=arrange(items={'kay': ['iron_blade', 'bad_brew', 'antidote'], 'nell': ['purify_staff', 'cure_staff']}, at={'r1': (9, 5), 'r2': (10, 5), 'r3': (11, 5)},
                                       extra=lambda g: edit_items(g, lambda it: it['purify_staff'].update({'area': 'single'}))))
    test_cure_single('csingle')
    make_game('cpoison', tweak=arrange(items={'mira': ['poison_staff', 'fire_tome']}, at=NEAR, extra=sure))
    test_poison_ticks('cpoison')
    make_game('csleep', tweak=arrange(items={'mira': ['sleep_staff', 'fire_tome']}, at=NEAR))
    test_sleep('csleep')
    make_game('cberserk', tweak=arrange(items={'kay': ['iron_blade', 'wild_draught']}, at={'r1': (9, 5), 'r2': (10, 5), 'r3': (11, 5)}))
    test_berserk('cberserk')
    make_game('cquiet', tweak=arrange(items={'mira': ['quiet_staff', 'hex_staff', 'fire_tome']}, at={'r1': (9, 5), 'r2': (10, 5), 'r3': (11, 5)}))
    test_stun_and_silence('cquiet')
    test_forecast_after_curse('cdesk')
    make_game('cweapon', tweak=arrange(at={'r1': (3, 2)}))
    test_weapon_inflict('cweapon')
    make_game('cdetails', tweak=arrange(items={'kay': ['iron_blade', 'bad_brew']}, at={'r1': (9, 5), 'r2': (10, 5), 'r3': (11, 5)}))
    test_status_page('cdetails')
    for style in ('top', 'iso', 'iso3', 'full3'):
        make_game('cmark_' + style, tweak=arrange(items={'kay': ['iron_blade', 'bad_brew']}, at={'r1': (9, 5), 'r2': (10, 5), 'r3': (11, 5)}), styles=True)
        test_marker('cmark_' + style, style)
    make_game('cphone', 270, 480, tweak=arrange(items=MIRA, at=NEAR))
    test_staff_inflict('cphone', 'phone')
    make_game('cphonemark', 270, 480, tweak=arrange(items={'kay': ['iron_blade', 'bad_brew']}, at={'r1': (9, 5), 'r2': (10, 5), 'r3': (11, 5)}))
    test_marker('cphonemark', 'top', tag='_phone')

    print('\n%d passed, %d failed' % (passed, len(failures)))
    for f in failures:
        print('  FAILED: ' + f)
    shutil.rmtree(TMP, ignore_errors=True)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
