#!/usr/bin/env python3
"""The player and doors, keys, chests and visiting (docs/SRPG.md section 10), windowless.

  python3 tests/srpg_player_doors_test.py ./build/engine/player/ote-player .
  python3 tests/srpg_player_doors_test.py ./build/engine/player/ote-player . --shots OUT_DIR   # keep the screenshots (BMP)

Copies tests/fixtures/srpg-doors into a temp folder, moves the unit a flow needs to the front of its map (the first unit is
the one under the cursor), runs the player headless with --input scripts and reads the player's [info] log lines and the
Continue save it writes:

  - Open with a key from the menu (the default key; a "Which key?" menu when two fit; the key is used up), with two doors next
    to the unit (which one), the greyed reason without a key, a chest (items to the inventory, gold to the side, the "Found" lines);
  - Visit and Destroy; Break door with the forecast (damage, hit points, "Breaks"), the door's hit points saved;
  - an unlock ability (a square pick), an unlock staff (square pick, forecast line, a miss);
  - "rules.places": "auto": no Open entry, the move opens the door;
  - a door in fog opens and the view shows what was behind it (the picture gets brighter).
"""
import json
import os
import shutil
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import (  # noqa: E402
    Bmp, run_text, check, edit, failures, inp, lines, player_and_repo, player_cmd, read_json, shots_dir, summary, ticks, write_json)
from ote_testkit import scratch_dir  # noqa: E402

sys.dont_write_bytecode = True

PLAYER, REPO = player_and_repo()
SHOTS = shots_dir()
TMP = scratch_dir()

IDLE = 'wait:14'  # the turn banner plays first; it takes the first inputs otherwise
MENU = [IDLE, 'confirm', 'wait:3', 'confirm', 'wait:3']  # select the first unit, open its action menu


STYLES = [{'id': 'top', 'name': 'Top'}, {'id': 'iso', 'name': 'Iso', 'view': 'isometric'},
          {'id': 'iso3', 'name': 'Iso 3D', 'view': 'isometric-3d'}, {'id': 'full3', 'name': 'Full 3D', 'view': '3d'}]


def make_game(gid, tweak=None, styles=False):
    g = os.path.join(TMP, gid)
    shutil.copytree(os.path.join(REPO, 'tests/fixtures/srpg-doors'), g)
    game = read_json(g + '/game.json')
    if styles:
        game['styles'] = STYLES
    write_json(g + '/game.json', game)
    if tweak:
        tweak(g)
    return g


def unit_of(m, uid):
    return [u for u in m['units'] if u.get('id') == uid][0]


def structure_of(m, sid):
    return [s for s in m['structures'] if s.get('id') == sid][0]


def front(m, uid):
    """The unit moves to the front of the map's units: the cursor starts on it."""
    u = unit_of(m, uid)
    m['units'].remove(u)
    m['units'].insert(0, u)


def tweak_map(map_id, fn):
    return lambda g: edit(g, 'maps/%s.json' % map_id, fn)


def run(gid, inp, map_id='hall', style=None, shot=None, interval=4):
    """Runs the player on a fresh battle; returns (code, log, the Continue save's battle, a lookup of its structures by kind id)."""
    work = os.path.join(TMP, 'saves_%s_%s' % (gid, map_id))
    shutil.rmtree(work, ignore_errors=True)
    cmd = player_cmd(PLAYER, TMP, REPO) + ['--saves', work, '--frames', str(ticks(inp) * interval + 90), '--input-interval', str(interval),
           '--fixed-dt', '0.016', '--game', gid, '--map', map_id, '--input', inp]
    if style:
        cmd += ['--style', style]
    if shot:
        cmd += ['--screenshot', shot]
    p = run_text(cmd)
    battle = {}
    try:
        battle = read_json(os.path.join(work, gid, 'continue.json'))['battle']
    except (OSError, ValueError, KeyError):
        pass
    return p.returncode, p.stdout, battle


class Image(Bmp):
    def brightness(self):
        return sum(p[0] + p[1] + p[2] for row in self.px for p in row)


def shot(name, gid, inp, map_id='hall', style=None):
    path = os.path.join(TMP, name + '.bmp')
    code, log, battle = run(gid, inp, map_id, style, shot=path)
    if SHOTS:
        os.makedirs(SHOTS, exist_ok=True)
        shutil.copy(path, os.path.join(SHOTS, name + '.bmp'))
    return Image(path), log, battle


class Game:
    """Reads the game's kind numbers (structures.json order) and the units / structures of a save."""
    def __init__(self, g):
        self.kinds = [s['id'] for s in read_json(g + '/data/structures.json')['structures']]
        self.items = [i['id'] for i in read_json(g + '/data/items.json')['items']]
        self.g = g

    def structure(self, battle, map_id, sid):
        m = read_json('%s/maps/%s.json' % (self.g, map_id))
        idx = [s.get('id') for s in m['structures']].index(sid)
        st = battle['structures'][idx]
        return self.kinds[st.get('type', self.kinds.index(m['structures'][idx]['type']))], st

    def unit(self, battle, uid):
        for u in battle['units']:
            if u.get('id') == uid:
                return u
        return None

    def carried(self, unit):
        return [self.items[i['item']] for i in unit.get('items', [])]


def test_open_with_a_key():
    g = make_game('open_key', tweak_map('hall', lambda m: front(m, 'hero')))
    G = Game(g)
    code, log, battle = run('open_key', inp(*MENU, 'confirm', 'wait:30'))
    check('open: the menu has Open for the cell door', any(l.endswith('place entries of Hero: Open') for l in lines(log, 'place entries')), log[-400:])
    check('open: the door opens and the log says by whom', bool(lines(log, 'opened: Cell Door (d1) by Hero')), log[-300:])
    kind, st = G.structure(battle, 'hall', 'd1')
    hero = G.unit(battle, 'hero')
    check('open: the door is now the open kind, unlocked', kind == 'cell_door_open' and not st.get('locked', False), str(st))
    check('open: the brass key (one use) is gone and the turn is over', G.carried(hero) == ['iron_sword'] and hero['done'], str(hero))


def test_two_keys():
    def tw(m):
        front(m, 'hero')
        unit_of(m, 'hero')['items'] = ['iron_sword', 'brass_key', 'master_key']
    g = make_game('two_keys', tweak_map('hall', tw))
    G = Game(g)
    code, log, battle = run('two_keys', inp(*MENU, 'confirm', 'wait:3', 'confirm', 'wait:30'))
    check('keys: two keys fit, so the player asks which', bool(lines(log, 'key choices for Cell Door: Brass Key, Master Key')), log[-300:])
    hero = G.unit(battle, 'hero')
    check('keys: the first (default) key was used, the master key is untouched', G.carried(hero) == ['iron_sword', 'master_key']
          and hero['items'][1]['uses'] == 3, str(hero))
    code, log, battle = run('two_keys', inp(*MENU, 'confirm', 'wait:3', 'next', 'wait:2', 'confirm', 'wait:30'))
    hero = G.unit(battle, 'hero')
    check('keys: choosing the second uses the master key (3 > 2)', G.carried(hero) == ['iron_sword', 'brass_key', 'master_key']
          and hero['items'][2]['uses'] == 2, str(hero))
    code, log, battle = run('two_keys', inp(*MENU, 'confirm', 'wait:3', 'cancel', 'wait:3', 'wait:3'))
    check('keys: Cancel on the key menu goes back to the action menu, nothing opens',
          len(lines(log, 'place entries of Hero')) == 2 and G.structure(battle, 'hall', 'd1')[0] == 'cell_door', str(lines(log, 'place entries')))


def test_two_doors_and_greyed():
    def tw(m):
        front(m, 'hero')
        structure_of(m, 'd2')['x'], structure_of(m, 'd2')['y'] = 4, 2  # the latched door south of the hero, d1 east of him
    g = make_game('two_doors', tweak_map('hall', tw))
    G = Game(g)
    code, log, battle = run('two_doors', inp(*MENU, 'next', 'wait:2', 'confirm', 'wait:30'))
    check('doors: with two doors in reach the entries say which', bool(lines(log, 'Open Cell Door, Open Latched Door')), str(lines(log, 'place entries')))
    check('doors: the second entry opens the latched door and leaves the cell door shut',
          G.structure(battle, 'hall', 'd2')[0] == 'cell_door_open' and G.structure(battle, 'hall', 'd1')[0] == 'cell_door', '')
    # No key: greyed with the short reason; the menu cannot select it (Wait is the next entry that works).
    def tw2(m):
        front(m, 'hero')
        unit_of(m, 'hero')['items'] = ['iron_sword']
    g2 = make_game('no_key', tweak_map('hall', tw2))
    code, log, battle = run('no_key', inp(*MENU, 'wait:3'))
    check('doors: no key - the Open entry is greyed with "Needs a key"', bool(lines(log, 'place entries of Hero: Open [Needs a key]')), str(lines(log, 'place entries')))


def test_chest():
    def tw(m):
        front(m, 'tink')
        unit_of(m, 'tink')['items'] = ['iron_sword', 'master_key']
    g = make_game('chest', tweak_map('hall', tw))
    G = Game(g)
    code, log, battle = run('chest', inp(*MENU, 'confirm', 'wait:40'))
    tink = G.unit(battle, 'tink')
    check('chest: the contents are announced', bool(lines(log, 'chest gave: Found Potion')) and bool(lines(log, 'chest gave: Found 50 Gold')), log[-400:])
    check('chest: the potion is in the opener\'s inventory and the key lost a use', 'potion' in G.carried(tink)
          and [i for i in tink['items'] if G.items[i['item']] == 'master_key'][0]['uses'] == 2, str(tink))
    check('chest: the gold went to the side and the chest is the open kind', battle['sides'][0]['resources'][0] == 50
          and G.structure(battle, 'hall', 'c1')[0] == 'chest_open', str(battle['sides'][0]))
    # A full pack cannot open a chest (no storage in the game): greyed with a reason.
    def tw2(m):
        front(m, 'tink')
        unit_of(m, 'tink')['items'] = ['iron_sword', 'master_key', 'potion', 'lockpick']
    make_game('chest_full', tweak_map('hall', tw2))
    code, log, battle = run('chest_full', inp(*MENU, 'wait:3'))
    check('chest: a full pack greys Open with "Pack full"', bool(lines(log, 'Open [Pack full]')), str(lines(log, 'place entries')))


def test_visit():
    def tw(m):
        front(m, 'hero')
        unit_of(m, 'hero')['x'], unit_of(m, 'hero')['y'] = 3, 3
    g = make_game('visit', tweak_map('hall', tw))
    G = Game(g)
    code, log, battle = run('visit', inp(*MENU, 'confirm', 'wait:40'))
    check('visit: the menu has Visit and the visit is announced', bool(lines(log, 'place entries of Hero: Visit')) and bool(lines(log, 'visited: Hamlet (v1)')), log[-300:])
    check('visit: the hamlet shuts, the story gave 30 gold', G.structure(battle, 'hall', 'v1')[0] == 'hamlet_shut' and battle['sides'][0]['resources'][0] == 30, str(battle['sides'][0]))
    # The red hamlet is destroyed by a blue unit.
    def tw2(m):
        front(m, 'hero')
        unit_of(m, 'hero')['x'], unit_of(m, 'hero')['y'] = 8, 0
    make_game('destroy', tweak_map('hall', tw2))
    code, log, battle = run('destroy', inp(*MENU, 'confirm', 'wait:40'))
    check('destroy: the menu has Destroy, the hamlet is ruined', bool(lines(log, 'place entries of Hero: Destroy')) and bool(lines(log, 'destroyed: Hamlet (v3)'))
          and G.structure(battle, 'hall', 'v3')[0] == 'hamlet_ruin', log[-300:])


def test_break():
    g = make_game('break', tweak_map('gatehouse', lambda m: front(m, 'brute')))
    G = Game(g)
    code, log, battle = run('break', inp(*MENU, 'confirm', 'wait:3', 'confirm', 'wait:40'), 'gatehouse')
    check('break: the menu has Break door', bool(lines(log, 'place entries of Brute: Open [')) and bool(lines(log, ', Break door')), log[-300:])
    check('break: the forecast says damage and hit points', bool(lines(log, 'door forecast: Iron Gate  Heavy Axe; Damage 13; HP 20 > 7;')), str(lines(log, 'forecast')))
    kind, st = G.structure(battle, 'gatehouse', 'g1')
    check('break: the door took 13 and stays shut with 7 hit points', bool(lines(log, 'door hit: Iron Gate takes 13 from Brute (7 left)')) and kind == 'gate' and st.get('hp') == 7, str(st))
    # A weaker door: the same hit breaks it.
    def tw(g_):
        edit(g_, 'data/structures.json', lambda s: s['structures'][0]['door'].update(hp=10))
        edit(g_, 'maps/gatehouse.json', lambda m: front(m, 'brute'))
    make_game('break2', tw)
    code, log, battle = run('break2', inp(*MENU, 'confirm', 'wait:3', 'confirm', 'wait:40'), 'gatehouse')
    check('break: with 10 hit points the forecast says Breaks and the gate opens', bool(lines(log, 'HP 10 > 0; Breaks;')) and bool(lines(log, 'opened: Iron Gate (g1) by Brute'))
          and G.structure(battle, 'gatehouse', 'g1')[0] == 'gate_open', log[-400:])


def test_unlock_ability():
    def tw(m):
        front(m, 'sneak')
        unit_of(m, 'sneak')['x'], unit_of(m, 'sneak')['y'] = 4, 1
        unit_of(m, 'hero')['x'], unit_of(m, 'hero')['y'] = 3, 1
    g = make_game('pick', tweak_map('hall', tw))
    G = Game(g)
    code, log, battle = run('pick', inp(*MENU, 'confirm', 'wait:3', 'confirm', 'wait:40'))
    check('pick lock: the ability asks for a square', bool(lines(log, 'pick square: 1 candidates')), log[-400:])
    check('pick lock: it opens the locked door without a key', bool(lines(log, 'opened: Cell Door (d1) by Sneak')) and G.structure(battle, 'hall', 'd1')[0] == 'cell_door_open', log[-300:])
    check('pick lock: Open stays greyed in the same menu (no key)', bool(lines(log, 'Open [Needs a key]')), str(lines(log, 'place entries')))


def test_unlock_staff():
    def tw(m):
        front(m, 'sage')
        unit_of(m, 'sage')['x'], unit_of(m, 'sage')['y'] = 3, 3
    g = make_game('staff', tweak_map('gatehouse', tw))
    G = Game(g)
    code, log, battle = run('staff', inp(*MENU, 'confirm', 'wait:3', 'confirm', 'wait:3', 'confirm', 'wait:40'), 'gatehouse')
    check('unlock staff: the pick forecast says what opens and the experience', bool(lines(log, 'staff forecast: Unlock Staff 3; Opens Iron Gate; Exp 10;')), str(lines(log, 'forecast')))
    check('unlock staff: the gate opens, a use is spent', bool(lines(log, 'opened: Iron Gate (g1) by Sage')) and G.structure(battle, 'gatehouse', 'g1')[0] == 'gate_open', log[-300:])
    code, log, battle = run('staff', inp(*MENU, 'confirm', 'wait:3', 'next', 'wait:2', 'confirm', 'wait:3', 'confirm', 'wait:40'), 'gatehouse')
    check('unlock staff: a wild staff says its chance, misses, and the gate stays shut', bool(lines(log, 'Hit 0%')) and bool(lines(log, 'staff missed: the door'))
          and G.structure(battle, 'gatehouse', 'g1')[0] == 'gate', str(lines(log, 'staff')))


def test_automatic():
    def tw(g_):
        edit(g_, 'game.json', lambda gm: gm['rules'].update(places={'actions': 'auto'}))
        def fn(m):
            front(m, 'hero')
            unit_of(m, 'hero')['x'], unit_of(m, 'hero')['y'] = 3, 1
        edit(g_, 'maps/hall.json', fn)
    g = make_game('auto', tw)
    G = Game(g)
    # Select the hero, walk one square east (next to the cell door), then Wait.
    code, log, battle = run('auto', inp(IDLE, 'confirm', 'wait:3', 'right', 'wait:2', 'confirm', 'wait:30', 'up', 'wait:2', 'confirm', 'wait:10'))
    check('auto: the move opens the door by itself', bool(lines(log, 'opened: Cell Door (d1) by Hero')) and G.structure(battle, 'hall', 'd1')[0] == 'cell_door_open', log[-400:])
    check('auto: and the menu that follows has no Open entry', not lines(log, 'place entries'), str(lines(log, 'place entries')))
    check('auto: the move did not end the turn: the menu after it still offers Items, Info and Wait', bool(lines(log, 'action menu: Items, Info, Wait')), log[-300:])


def test_fog():
    g = make_game('fog', tweak_map('gatehouse_fog', lambda m: front(m, 'hero')))
    G = Game(g)
    before, log0, b0 = shot('fog_closed', 'fog', inp(IDLE, 'wait:10'), 'gatehouse_fog')
    after, log, battle = shot('fog_opened', 'fog', inp(*MENU, 'confirm', 'wait:60'), 'gatehouse_fog')
    check('fog: the gate opens', bool(lines(log, 'opened: Iron Gate (g1) by Hero')) and G.structure(battle, 'gatehouse_fog', 'g1')[0] == 'gate_open', log[-300:])
    check('fog: the squares behind the gate come into sight (the picture gets brighter)', after.brightness() > before.brightness() * 1.01,
          '%d vs %d' % (after.brightness(), before.brightness()))


def test_views():
    # The same door opening in all four kinds of view: it plays and the gate shows its open look (the picture changes).
    make_game('views', tweak_map('gatehouse', lambda m: front(m, 'hero')), styles=True)
    for style in ('top', 'iso', 'iso3', 'full3'):
        closed, l0, b0 = shot('views_closed_%s' % style, 'views', inp(IDLE, 'wait:10'), 'gatehouse', style)
        opened, log, battle = shot('views_opened_%s' % style, 'views', inp(*MENU, 'confirm', 'wait:60'), 'gatehouse', style)
        check('views %s: the gate opens and the picture shows it' % style, bool(lines(log, 'opened: Iron Gate (g1) by Hero')) and opened.brightness() != closed.brightness(),
              '%d vs %d' % (opened.brightness(), closed.brightness()))


def main():
    if not PLAYER or not os.path.exists(PLAYER):
        print('usage: srpg_player_doors_test.py PLAYER REPO [--shots DIR]')
        return 2
    try:
        test_open_with_a_key()
        test_two_keys()
        test_two_doors_and_greyed()
        test_chest()
        test_visit()
        test_break()
        test_unlock_ability()
        test_unlock_staff()
        test_automatic()
        test_fog()
        test_views()
    finally:
        shutil.rmtree(TMP, ignore_errors=True)
    summary()
    for f in failures:
        print('  FAILED: ' + f)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
