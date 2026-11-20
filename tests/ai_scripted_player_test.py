#!/usr/bin/env python3
"""The player and the scripted-style computer players (docs/AI.md "Scripted-style computer players"), windowless.

  python3 tests/ai_scripted_player_test.py ./build/engine/player/ote-player .

Writes tiny games (both sides played by the computer, so nobody has to press a key) into a temp folder and runs the real player headless
(--headless --no-audio --no-settings, saves in the temp folder). The player logs "ai: <side> acts, round N" for every computer turn and writes
its Continue save when each turn starts, so the test stops the run at the turn it wants and reads the save. It judges:

  - patrol: a unit walks its route and loops (12,2 - 8,2 - 12,2 ...);
  - idleTurns: a unit stands still for its first turns, then walks;
  - retreat: a unit hit below its limit is saved as retreating, walks to the fort and does not strike the unit next to it;
  - weights: a holding unit with a weight set hits the archer its priority table prefers, not the soldier;
  - ignores: a unit walks toward the farther enemy it does not ignore, a control unit toward the nearer one;
  - the turn order "order": the unit with the lower number acts first (and the default turn order does not);
  - the run's log says no warning or error, and every turn it ran is in it.
"""
import json
import os
import shutil
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import run_text, check, failures, player_and_repo, player_cmd, summary  # noqa: E402
from ote_testkit import scratch_dir  # noqa: E402

sys.dont_write_bytecode = True

PLAYER, REPO = player_and_repo()
TMP = scratch_dir()
WIDTH = 14
logs = []


def write(path, obj):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w') as f:
        json.dump(obj, f, indent=1)


AI = {'turnOrder': 'order', 'weights': {
    'pick': {'dmg': 2, 'left': 3, 'friends': 4, 'class': 1, 'turn': 1, 'counter': 2, 'wounds': 1, 'priority': {'archer': 10, 'soldier': 0}}}}


def make_game(gid, units, rows=None):
    """A 14 x 5 map; `units` are all of them. Blue units that must stay out of it carry fromTurn 99 (passive: they only answer blows)."""
    g = os.path.join(TMP, gid)
    write(g + '/game.json', {
        'formatVersion': 1, 'name': 'AI scripted player test', 'view': {'width': 480, 'height': 270, 'tileSize': 24},
        'stats': [{'id': 'hp', 'name': 'HP'}, {'id': 'str', 'name': 'Strength'}, {'id': 'def', 'name': 'Defense'}, {'id': 'mov', 'name': 'Move'}],
        'roles': {'hp': 'hp', 'move': 'mov'}, 'moveTypes': [{'id': 'foot', 'name': 'Foot'}],
        'rules': {'defeat': ['noUnits'], 'counterAttacks': True, 'ai': AI}, 'formulas': {'damage': 'a.str + w.might - b.def', 'hit': '100'},
        'maps': [{'id': 'field', 'name': 'Field', 'file': 'maps/field.json'}]})
    write(g + '/data/terrain.json', {'terrain': [{'id': 'plain', 'name': 'Plain', 'props': {'defense': 0}},
                                                 {'id': 'fort', 'name': 'Fort', 'props': {'defense': 2}}]})
    write(g + '/data/weapons.json', {'weapons': [{'id': 'sword', 'name': 'Sword', 'group': 'sword', 'props': {'might': 5}},
                                                 {'id': 'bow', 'name': 'Bow', 'group': 'bow', 'range': [2, 2], 'props': {'might': 4}}]})
    write(g + '/data/units.json', {'units': [
        {'id': 'soldier', 'name': 'Soldier', 'glyph': 'S', 'moveType': 'foot', 'stats': {'hp': 20, 'str': 6, 'def': 2, 'mov': 4}, 'weapons': ['sword']},
        {'id': 'archer', 'name': 'Archer', 'glyph': 'A', 'moveType': 'foot', 'stats': {'hp': 16, 'str': 5, 'def': 1, 'mov': 4}, 'weapons': ['bow']}]})
    tiles = rows or ['.' * WIDTH] * 5
    write(g + '/maps/field.json', {'name': 'Field', 'grid': {'type': 'square', 'width': WIDTH, 'height': 5}, 'legend': {'.': 'plain', 'F': 'fort'},
                                   'tiles': tiles, 'sides': [{'id': 'blue', 'controller': 'ai'}, {'id': 'red', 'controller': 'ai'}], 'units': units})


def unit(uid, side, x, y, behaviour=None, utype='soldier'):
    u = {'type': utype, 'id': uid, 'side': side, 'x': x, 'y': y}
    if behaviour is not None:
        u['behaviour'] = behaviour
    return u


PASSIVE = {'fromTurn': 99}


def run(gid, frames):
    """Runs the player headless for `frames` frames; returns (log, the Continue save's battle or {})."""
    work = os.path.join(TMP, 'saves_' + gid)
    shutil.rmtree(work, ignore_errors=True)
    cmd = player_cmd(PLAYER, TMP, REPO) + ['--saves', work,
           '--frames', str(frames), '--fixed-dt', '0.016', '--game', gid, '--map', 'field']
    p = run_text(cmd)
    battle = {}
    try:
        with open(os.path.join(work, gid, 'continue.json')) as f:
            battle = json.load(f)['battle']
    except (OSError, ValueError, KeyError):
        pass
    return p.stdout, battle


def state_at(gid, side, rnd):
    """The battle save written when `side` (0 blue, 1 red) starts its turn in round `rnd`: the state that turn begins from."""
    want = 'ai: %s acts, round %d' % ('blue' if side == 0 else 'red', rnd)
    for frames in range(60, 4000, 30):
        log, battle = run(gid, frames)
        acts = [line for line in log.splitlines() if line.startswith('[info] ai:')]
        if acts and want in acts[-1] and battle.get('round') == rnd and battle.get('activeSide') == side:
            logs.append(log)
            return battle
    return {}


def find(battle, uid):
    for u in battle.get('units', []):
        if u.get('id') == uid:
            return u
    return {}


def spot(battle, uid):
    u = find(battle, uid)
    return (u['cell'] % WIDTH, u['cell'] // WIDTH) if 'cell' in u else None


def hp(battle, uid):
    return find(battle, uid).get('hp')


def test_patrol():
    route = [{'x': 12, 'y': 2}, {'x': 8, 'y': 2}]
    make_game('patrol', [unit('lord', 'blue', 0, 2, PASSIVE), unit('p', 'red', 10, 2, {'mode': 'patrol', 'route': route})])
    for rnd, want in ((2, (12, 2)), (3, (8, 2)), (4, (12, 2)), (5, (8, 2))):
        b = state_at('patrol', 0, rnd)
        check('patrol: after red turn %d it stands on %s' % (rnd - 1, want), spot(b, 'p') == want, spot(b, 'p'))
    b = state_at('patrol', 0, 3)
    check('patrol: the next route square is in the save', find(b, 'p').get('behaviour', {}).get('route') == route and find(b, 'p').get('patrolAt') == 1, find(b, 'p'))


def test_idle_turns():
    make_game('idle', [unit('lord', 'blue', 0, 2, PASSIVE), unit('i', 'red', 9, 0, {'idleTurns': 2})])
    for rnd, moved in ((2, False), (3, False), (4, True)):
        b = state_at('idle', 0, rnd)
        check('idleTurns 2: after red turn %d it %s' % (rnd - 1, 'has walked' if moved else 'still stands at 9,0'), (spot(b, 'i') != (9, 0)) == moved, spot(b, 'i'))
    check('idleTurns 2: the count is saved', find(b, 'i').get('idleSeen') == 3, find(b, 'i'))


def test_retreat():
    rows = ['.' * WIDTH, '.' * WIDTH, '...........F..', '.' * WIDTH, '.' * WIDTH]
    behaviour = {'retreat': {'below': 60, 'until': 90, 'to': {'terrain': ['fort']}}}
    # Blue's soldier "b" holds next to the red soldier "h": in round 1 it hits h (20 -> 11) and takes the answer (20 -> 11).
    make_game('retreat', [unit('b', 'blue', 7, 2, 'hold'), unit('h', 'red', 8, 2, behaviour)], rows)
    mid = state_at('retreat', 1, 1)
    check('retreat: hit below 60 % at the start of its turn it is saved as retreating', find(mid, 'h').get('retreating') is True and hp(mid, 'h') == 11, find(mid, 'h'))
    after = state_at('retreat', 0, 2)
    check('retreat: it walked to the fort', spot(after, 'h') == (11, 2), spot(after, 'h'))
    check('retreat: it did not strike the unit next to it while retreating', hp(after, 'b') == 11, hp(after, 'b'))
    check('retreat: it is still retreating', find(after, 'h').get('retreating') is True)


def test_weights():
    # w (red, holds, set "pick") has a soldier next to it on one side and an archer on the other; both are passive. The table of the docs: archer 83, soldier 28.
    make_game('weights', [unit('w', 'red', 6, 2, {'mode': 'hold', 'weights': 'pick'}), unit('s', 'blue', 7, 2, PASSIVE),
                          unit('a', 'blue', 5, 2, PASSIVE, 'archer')])
    b = state_at('weights', 0, 2)
    check('weights: it hit the archer its priority table prefers', hp(b, 'a') is not None and hp(b, 'a') < 16, hp(b, 'a'))
    check('weights: and not the soldier', hp(b, 's') == 20, hp(b, 's'))
    check('weights: a holding unit did not move', spot(b, 'w') == (6, 2), spot(b, 'w'))


def test_ignores():
    # The decoy (13,2) is nearer to r (7,2) by path than the lord (0,2). r ignores it and walks west; the control walks east.
    make_game('ignores', [unit('lord', 'blue', 0, 2, PASSIVE), unit('decoy', 'blue', 13, 2, PASSIVE),
                          unit('r', 'red', 7, 2, {'ignores': ['decoy']}), unit('c', 'red', 7, 4)])
    b = state_at('ignores', 0, 2)
    check('ignores: the unit that ignores the decoy walks toward the lord', spot(b, 'r') is not None and spot(b, 'r')[0] < 7, spot(b, 'r'))
    check('ignores: the control unit walks toward the nearer one, the decoy', spot(b, 'c') is not None and spot(b, 'c')[0] > 7, spot(b, 'c'))


def test_order():
    # A weak blue unit (5 hit points) that either red soldier kills: whoever acts first stands next to it afterwards, the other finds no target.
    # "late" is placed before "early" but has the higher number, so with turnOrder "order" early goes first; with the default turn order the lower id does.
    def units():
        return [unit('lord', 'blue', 0, 4, PASSIVE), dict(unit('weak', 'blue', 5, 2, PASSIVE), stats={'hp': 5}),
                unit('late', 'red', 5, 0, {'order': 2}), unit('early', 'red', 5, 4, {'order': 1})]
    make_game('order', units())
    b = state_at('order', 0, 2)
    check('order: the weak unit fell', hp(b, 'weak') is None or not find(b, 'weak').get('alive', True), find(b, 'weak'))
    check('order: the unit with the lower number acted first and struck it from 5,3', spot(b, 'early') == (5, 3), spot(b, 'early'))
    check('order: the other one did not strike it (it is not on 5,1)', spot(b, 'late') != (5, 1), spot(b, 'late'))
    # The same map with the default turn order: the unit placed first (late) strikes.
    make_game('orderbest', units())
    g = os.path.join(TMP, 'orderbest', 'game.json')
    with open(g) as f:
        game = json.load(f)
    del game['rules']['ai']['turnOrder']
    with open(g, 'w') as f:
        json.dump(game, f)
    b = state_at('orderbest', 0, 2)
    check('order: with the default turn order the unit placed first strikes (5,1)', spot(b, 'late') == (5, 1), spot(b, 'late'))


if not os.path.exists(PLAYER):
    print('FAIL the player %s is not built' % PLAYER)
    sys.exit(1)
for t in (test_patrol, test_idle_turns, test_retreat, test_weights, test_ignores, test_order):
    t()
bad = [line for log in logs for line in log.splitlines() if line.startswith('[warn]') or line.startswith('[error]')]
check('the player logged no warning or error', not bad, bad[:3])
check('the log shows the computer turns it ran', any('ai: red acts, round 1' in log for log in logs))
shutil.rmtree(TMP, ignore_errors=True)
summary()
sys.exit(1 if failures else 0)
