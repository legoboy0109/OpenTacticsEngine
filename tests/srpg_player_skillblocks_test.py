#!/usr/bin/env python3
"""The player's part of docs/SRPG.md "Skill and ability blocks, second set": steal, refresh an ally and move past a neighbour,
windowless.

  python3 tests/srpg_player_skillblocks_test.py ./build/engine/player/ote-player .
  python3 tests/srpg_player_skillblocks_test.py ./build/engine/player/ote-player . --shots OUT_DIR   # also keeps the screenshots (BMP)

Writes a small game in a temp folder (carried weapons, abilities steal / steal_arms / refresh / leap), runs the player headless
(--no-audio --no-settings, --saves on a temp folder) with --input scripts, and judges the [info] lines it logs and the Continue
save:

  - Steal: the foe is picked first, then a "Steal which?" list of its items; the ones that cannot be taken (a weapon, an item the
    maker marked unstealable) are greyed with the reason; choosing one moves it to the thief; Cancel steps back (list -> pick ->
    action menu); a full pack greys the ability with the reason; the pointer (taps) does the same as the keys.
  - Refresh: the ally pick, the unit that had acted may act again (its Continue-save flags), the same unit cannot be refreshed twice.
  - Move past: the preview marks the far square, the user ends there.
"""
import json
import os
import shutil
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import run_text, failures, player_cmd, summary, write_json  # noqa: E402
from ote_testkit import scratch_dir  # noqa: E402

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import hex_player_test as hp  # noqa: E402  (its image reader and checker)

check, Image = hp.check, hp.Image
PLAYER = REPO = GAMES = TMP = SHOTS = ''
W, H, TILE = 480, 270, 24
MAP_W, MAP_H = 9, 7
VIEW_W, VIEW_H = 352, 259
IDLE = 'wait:14'                   # the turn banner plays first
SELECT = 'confirm,confirm'         # select the unit under the cursor, stay where it is: its action menu opens
GAME = 'skillblocks_player'


ABILITIES = {'abilities': [
    {'id': 'steal', 'name': 'Steal', 'target': 'enemy', 'effect': {'steal': True}},
    {'id': 'refresh', 'name': 'Encore', 'target': 'ally', 'effect': {'refresh': True}},
    {'id': 'leap', 'name': 'Leap Past', 'target': 'unit', 'effect': {'movePast': True}}]}


def write_game(out, inventory=3):
    g = os.path.join(out, GAME)
    shutil.rmtree(g, ignore_errors=True)
    game = {'formatVersion': 1, 'name': 'Skill Blocks Player Test',
            'view': {'width': W, 'height': H, 'tileSize': TILE},
            'stats': [{'id': 'hp', 'name': 'HP'}, {'id': 'atk'}, {'id': 'def'}, {'id': 'mov', 'name': 'Move'}],
            'roles': {'hp': 'hp', 'move': 'mov'}, 'moveTypes': [{'id': 'foot'}],
            'rules': {'turnOrder': 'sides', 'counterAttacks': True, 'defeat': ['noUnits'],
                      'items': {'weapons': 'carried', 'inventorySize': inventory}},
            'formulas': {'damage': 'a.atk - b.def'},
            'maps': [{'id': n, 'name': n, 'file': 'maps/%s.json' % n} for n in MAPS]}
    write_json(g + '/game.json', game)
    write_json(g + '/data/terrain.json', {'terrain': [{'id': 'plain', 'name': 'Plain', 'color': '#9ccc65'}]})
    stats = lambda hp_, mv: {'hp': hp_, 'atk': 5, 'def': 1, 'mov': mv}
    write_json(g + '/data/units.json', {'units': [
        {'id': 'rogue', 'name': 'Rogue', 'moveType': 'foot', 'stats': stats(20, 4), 'wields': ['sword'], 'items': ['sword'],
         'abilities': ['steal', 'refresh', 'leap']},
        {'id': 'ally', 'name': 'Ally', 'moveType': 'foot', 'stats': stats(14, 3), 'wields': ['sword'], 'items': ['sword'],
         'abilities': ['refresh']},
        {'id': 'grunt', 'name': 'Grunt', 'moveType': 'foot', 'stats': stats(12, 3), 'wields': ['sword'],
         'items': ['sword', 'potion', 'relic']}]}),
    write_json(g + '/data/weapons.json', {'weapons': [{'id': 'sword', 'name': 'Sword', 'group': 'sword'}]})
    write_json(g + '/data/items.json', {'items': [
        {'id': 'sword', 'name': 'Sword', 'kind': 'weapon', 'weapon': 'sword', 'uses': 30},
        {'id': 'potion', 'name': 'Potion', 'kind': 'usable', 'uses': 3, 'heal': 5},
        {'id': 'relic', 'name': 'Relic', 'kind': 'usable', 'uses': 1, 'heal': 1, 'stealable': False},
        {'id': 'amulet', 'name': 'Amulet', 'kind': 'accessory', 'bonus': {'hp': 2}}]})
    write_json(g + '/data/abilities.json', ABILITIES)
    for name, units in MAPS.items():
        write_json(g + '/maps/%s.json' % name, make_map(units))
    return g


def make_map(units):
    rows = ['.' * MAP_W for _ in range(MAP_H)]
    return {'name': 'Map', 'grid': {'type': 'square', 'width': MAP_W, 'height': MAP_H}, 'legend': {'.': 'plain'}, 'tiles': rows,
            'sides': [{'id': 'blue', 'name': 'Blue Army', 'color': '#3d85c6', 'controller': 'human'},
                      {'id': 'red', 'name': 'Red Army', 'color': '#cc0000', 'controller': 'ai'}],
            'units': [dict({'type': t, 'side': s, 'x': x, 'y': y}, **extra) for (t, s, x, y, extra) in units]}


# map id -> units (type, side, x, y, extra keys); the far grunt keeps the battle going
FAR = ('grunt', 'red', 8, 6, {'items': ['sword']})
MAPS = {
    'steal': [('rogue', 'blue', 2, 2, {}), ('grunt', 'red', 3, 2, {}), FAR],
    # a full pack: the rogue carries three items (inventorySize 3)
    'full': [('rogue', 'blue', 2, 2, {'items': ['sword', 'potion', 'amulet']}), ('grunt', 'red', 3, 2, {}), FAR],
    'refresh': [('rogue', 'blue', 2, 2, {}), ('ally', 'blue', 3, 2, {}), FAR],
    'leap': [('rogue', 'blue', 2, 2, {}), ('grunt', 'red', 3, 2, {}), FAR],
}


def play(map_id, inp, shot=None, saves=None, extra=(), tail=60):
    frames = sum(int(t[5:]) * 4 if t.startswith('wait:') else 4 for t in inp.split(',')) + tail
    cmd = player_cmd(PLAYER, GAMES, REPO) + ['--frames', str(frames), '--input-interval', '4', '--fixed-dt', '0.016', '--game', GAME, '--map', map_id,
           '--input', inp]
    cmd += ['--saves', saves] if saves else ['--no-saving']
    if shot:
        cmd += ['--screenshot', shot]
    p = run_text(cmd + list(extra))
    return p.returncode, p.stdout


def run(name, map_id, inp, saves=False, extra=(), tail=60):
    """Returns (log, save-or-None) of one script."""
    path = os.path.join(TMP, name + '.bmp')
    sdir = os.path.join(TMP, 'saves_' + name) if saves else None
    if sdir:
        shutil.rmtree(sdir, ignore_errors=True)
    _, log = play(map_id, inp, shot=path, saves=sdir, extra=extra, tail=tail)
    if SHOTS and os.path.exists(path):
        shutil.copy(path, os.path.join(SHOTS, name + '.bmp'))
    save = None
    if sdir and os.path.exists(os.path.join(sdir, GAME, 'continue.json')):
        save = json.load(open(os.path.join(sdir, GAME, 'continue.json')))
    return log, save


def tap(x, y):
    """A pointer tap on the middle of a square map's tile (the map is centred in the viewport)."""
    import math
    ox = int(math.floor((VIEW_W - MAP_W * TILE) / 2.0 + 0.5))
    oy = int(math.floor((VIEW_H - MAP_H * TILE) / 2.0 + 0.5))
    return 'tap:%d:%d' % (ox + x * TILE + TILE // 2, oy + y * TILE + TILE // 2)


def items_of(save, unit, db_items=('sword', 'potion', 'relic', 'amulet')):
    """The item ids a unit of the Continue save carries, in order."""
    u = save['battle']['units'][unit]
    return [db_items[i['item']] if isinstance(i, dict) else db_items[i] for i in u.get('items', [])]


def cell_of(save, unit):
    c = save['battle']['units'][unit]['cell']
    return c % MAP_W, c // MAP_W


def main():
    global PLAYER, REPO, GAMES, SHOTS, TMP
    args = sys.argv[1:]
    if len(args) < 2:
        print(__doc__)
        return 2
    PLAYER, REPO = os.path.abspath(args[0]), os.path.abspath(args[1])
    if '--shots' in args:
        SHOTS = os.path.abspath(args[args.index('--shots') + 1])
        os.makedirs(SHOTS, exist_ok=True)
    TMP = scratch_dir()
    GAMES = os.path.join(TMP, 'games')
    try:
        write_game(GAMES)
        steal_checks()
        refresh_checks()
        leap_checks()
    finally:
        shutil.rmtree(TMP, ignore_errors=True)
    summary(blank_line=True)
    return 1 if failures else 0


# ---- steal ----

def steal_checks():
    # menu: Attack, Steal, Encore, Leap Past, Wait; Steal is the second entry
    log, _ = run('steal_pick', 'steal', IDLE + ',' + SELECT + ',down,confirm,wait:2')
    check('steal: choosing Steal opens a unit pick', 'ability chosen: Steal' in log and 'pick unit: 1 candidates' in log, log[-400:])
    log, _ = run('steal_list', 'steal', IDLE + ',' + SELECT + ',down,confirm,confirm,wait:3')
    check('steal: the foe is picked, then its items are listed', 'steal list from Grunt:' in log, log[-400:])
    check('steal: the weapon is greyed with its reason', 'Sword [weapons cannot be taken]' in log, log[-400:])
    check('steal: the unstealable item is greyed with its reason', 'Relic [cannot be taken]' in log, log[-400:])
    check('steal: the potion is listed without a reason', 'Potion,' in log or 'Potion [' not in log, log[-400:])
    # Potion is the second row of the list: down, confirm
    log, save = run('steal_done', 'steal', IDLE + ',' + SELECT + ',down,confirm,confirm,down,confirm,wait:25', saves=True)
    check('steal: the event is logged', 'steal: Rogue stole Potion from Grunt' in log, log[-400:])
    if save:
        check('steal: the thief carries the potion', items_of(save, 0) == ['sword', 'potion'], items_of(save, 0))
        check('steal: the foe no longer carries it', items_of(save, 1) == ['sword', 'relic'], items_of(save, 1))
    else:
        check('steal: a Continue save exists', False, log[-300:])
    # Cancel steps back: list -> pick -> action menu
    log, _ = run('steal_back', 'steal', IDLE + ',' + SELECT + ',down,confirm,confirm,cancel,wait:3')
    check('steal: Cancel in the list goes back to the unit pick', log.count('pick unit: 1 candidates') >= 2, log[-400:])
    log, _ = run('steal_back2', 'steal', IDLE + ',' + SELECT + ',down,confirm,confirm,cancel,cancel,wait:3')
    check('steal: Cancel in the pick goes back to the action menu', 'UI bug' not in log, log[-300:])
    # the list opens with the first row that can be taken highlighted; the greyed rows never take anything
    log, save = run('steal_greyed', 'steal', IDLE + ',' + SELECT + ',down,confirm,confirm,up,confirm,up,confirm,wait:25', saves=True)
    if save:
        mine = items_of(save, 0)
        check('steal: a greyed row (weapon, unstealable item) is never taken', 'relic' not in mine and mine.count('sword') == 1, mine)
    # the pointer: tap the foe (pick), then the potion row through the keys is not needed: taps on the menu are tested by the menu itself
    log, save = run('steal_touch', 'steal', IDLE + ',' + SELECT + ',down,confirm,' + tap(3, 2) + ',wait:3', saves=True)
    check('steal: tapping the foe in the pick opens the list', 'steal list from Grunt:' in log, log[-300:])
    # a full pack: Steal is greyed in the menu, with the reason
    write_game(GAMES, inventory=3)
    log, _ = run('steal_full', 'full', IDLE + ',' + SELECT + ',down,confirm,wait:2')
    check('steal: with a full pack the ability cannot be chosen', 'ability chosen: Steal' not in log, log[-300:])
    # the phone-sized screen runs
    log, _ = run('steal_phone', 'steal', IDLE + ',' + SELECT + ',down,confirm,confirm,wait:3', extra=('--ui-scale', '200'))
    check('steal: the phone-sized list runs without UI bugs', 'UI bug' not in log and 'steal list from Grunt' in log, log[-300:])


# ---- refresh ----

def refresh_checks():
    # The ally (second unit) acts first and waits (its menu: Encore greyed, Items, Trade, Info, Wait); then the rogue's Encore picks it.
    ally_waits = 'next,confirm,confirm,down,down,down,confirm,wait:10,prev,confirm,confirm'
    log, save = run('refresh_greyed', 'refresh', IDLE + ',next,confirm,confirm,wait:3')
    check('refresh: with nobody who has acted, Encore is greyed (the menu opens without it chosen)', 'ability chosen: Encore' not in log, log[-300:])
    log, save = run('refresh_done', 'refresh', IDLE + ',' + ally_waits + ',confirm,confirm,wait:25', saves=True)
    check('refresh: choosing Encore picks the ally', 'ability chosen: Encore by Rogue' in log and 'pick unit: 1 candidates' in log, log[-400:])
    check('refresh: the event is logged', 'refresh: Ally may act again' in log, log[-400:])
    if save:
        units = save['battle']['units']
        check('refresh: the ally may act again (not done), the rogue used its action', not units[1].get('done', False) and units[0].get('done', False),
              [(u.get('done'), u.get('moved')) for u in units])
        check('refresh: the guard is saved (the ally was refreshed by the rogue)', units[1].get('refreshedBy') == [0], units[1])
    else:
        check('refresh: a Continue save exists', False, log[-300:])
    # the pointer does the pick
    log, save = run('refresh_touch', 'refresh', IDLE + ',' + ally_waits + ',confirm,' + tap(3, 2) + ',wait:25', saves=True)
    check('refresh: tapping the ally picks it', 'refresh: Ally may act again' in log, log[-300:])


# ---- move past ----

def leap_checks():
    # menu: Attack, Steal, (Encore greyed), Leap Past: two downs from Attack
    log, save = run('leap_pick', 'leap', IDLE + ',' + SELECT + ',down,down,confirm,wait:2')
    check('leap: the pick opens', 'ability chosen: Leap Past' in log and 'pick unit: 1 candidates' in log, log[-300:])
    log, save = run('leap_done', 'leap', IDLE + ',' + SELECT + ',down,down,confirm,confirm,wait:25', saves=True)
    check('leap: the rogue ends on the far side of the grunt, which stays', save is not None and cell_of(save, 0) == (4, 2) and cell_of(save, 1) == (3, 2),
          (log[-300:], save and cell_of(save, 0)))
    log, save = run('leap_touch', 'leap', IDLE + ',' + SELECT + ',down,down,confirm,' + tap(3, 2) + ',wait:25', saves=True)
    check('leap: tapping the unit picks it', save is not None and cell_of(save, 0) == (4, 2), (log[-300:], save and cell_of(save, 0)))
    # the phone-sized screen
    log, _ = run('leap_phone', 'leap', IDLE + ',' + SELECT + ',down,down,confirm,wait:2', extra=('--ui-scale', '200'))
    check('leap: the phone-sized pick runs', 'UI bug' not in log and 'pick unit' in log, log[-300:])


if __name__ == '__main__':
    sys.exit(main())
