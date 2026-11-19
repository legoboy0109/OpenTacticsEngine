#!/usr/bin/env python3
"""The player and the computer unit behaviours (docs/AI.md), windowless.

  python3 tests/ai_behaviour_player_test.py ./build/engine/player/ote-player .

Writes a tiny game (both sides played by the computer, so nobody has to press a key) into a temp folder and runs the real player headless
(--headless --no-audio --no-settings, saves in the temp folder) for a few turns. The player logs "ai: <side> acts, round N" for every
computer turn and writes its Continue save when each turn starts, so the test stops the run at the turn it wants (by the frame count) and reads
where every unit stands and whether a guard is awake from that save. It judges:

  - hold never moves while an advancing unit of the same side walks up;
  - a guard sleeps while the enemy is out of reach, wakes on the turn the enemy comes into reach, and its group wakes with it;
  - seek marches a unit to its square, and it stays;
  - fromTurn keeps a unit idle for the first rounds;
  - story events change a behaviour mid-battle (setBehaviour) and wake a group (wakeGroup).
"""
import json
import os
import shutil
import sys
import tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import run_text, check, failures, player_and_repo, player_cmd, summary  # noqa: E402

sys.dont_write_bytecode = True

PLAYER, REPO = player_and_repo()
TMP = tempfile.mkdtemp(prefix='ote-aibehaviour-')
WIDTH = 14


def write(path, obj):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w') as f:
        json.dump(obj, f, indent=1)


def make_game(gid, units, events=None):
    """A 14 x 5 plain map; blue's lord "b1" stands still at 0,2 (a computer unit, so the battle plays itself); `units` are added."""
    g = os.path.join(TMP, gid)
    write(g + '/game.json', {
        'formatVersion': 1, 'name': 'AI behaviour player test', 'view': {'width': 480, 'height': 270, 'tileSize': 24},
        'stats': [{'id': 'hp', 'name': 'HP'}, {'id': 'str', 'name': 'Strength'}, {'id': 'def', 'name': 'Defense'}, {'id': 'mov', 'name': 'Move'}],
        'roles': {'hp': 'hp', 'move': 'mov'}, 'moveTypes': [{'id': 'foot', 'name': 'Foot'}],
        'rules': {'defeat': ['noUnits'], 'counterAttacks': True}, 'formulas': {'damage': 'a.str + w.might - b.def', 'hit': '100'},
        'maps': [{'id': 'field', 'name': 'Field', 'file': 'maps/field.json'}]})
    write(g + '/data/terrain.json', {'terrain': [{'id': 'plain', 'name': 'Plain'}]})
    write(g + '/data/weapons.json', {'weapons': [{'id': 'sword', 'name': 'Sword', 'group': 'sword', 'props': {'might': 5}}]})
    write(g + '/data/units.json', {'units': [{'id': 'soldier', 'name': 'Soldier', 'glyph': 'S', 'moveType': 'foot',
                                              'stats': {'hp': 20, 'str': 6, 'def': 2, 'mov': 4}, 'weapons': ['sword']}]})
    m = {'name': 'Field', 'grid': {'type': 'square', 'width': WIDTH, 'height': 5}, 'legend': {'.': 'plain'}, 'tiles': ['.' * WIDTH] * 5,
         'sides': [{'id': 'blue', 'controller': 'ai'}, {'id': 'red', 'controller': 'ai'}],
         'units': [{'type': 'soldier', 'id': 'b1', 'side': 'blue', 'x': 0, 'y': 2, 'behaviour': 'hold'}] + units}
    if events:
        m['events'] = events
    write(g + '/maps/field.json', m)


def red(uid, x, y, behaviour=None):
    u = {'type': 'soldier', 'id': uid, 'side': 'red', 'x': x, 'y': y}
    if behaviour is not None:
        u['behaviour'] = behaviour
    return u


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
            return battle
    return {}


def spot(battle, uid):
    """(x, y, awake) of the unit in a save."""
    for u in battle.get('units', []):
        if u.get('id') == uid:
            return u['cell'] % WIDTH, u['cell'] // WIDTH, bool(u.get('awake'))
    return None, None, None


def dist(a, b):
    return abs(a[0] - b[0]) + abs(a[1] - b[1])


def test_hold_and_advance():
    make_game('hold', [red('h', 9, 0, 'hold'), red('a', 9, 4)])
    for rnd in (2, 3):
        b = state_at('hold', 0, rnd)
        check('hold: after red turn %d the holding unit has not moved' % (rnd - 1), spot(b, 'h')[:2] == (9, 0), spot(b, 'h'))
        check('hold: after red turn %d its advancing neighbour has' % (rnd - 1), spot(b, 'a')[0] is not None and spot(b, 'a')[0] < 9, spot(b, 'a'))


def test_guard_wakes_when_the_enemy_comes_into_reach():
    # Blue's b2 walks toward red; the guard g (move 4 + range 1 = reach 5) wakes on the red turn that begins with b2 within 5.
    make_game('guard', [red('g', 11, 2, 'guard'), {'type': 'soldier', 'id': 'b2', 'side': 'blue', 'x': 1, 'y': 3}])
    woke = None
    for rnd in (1, 2, 3):
        before = state_at('guard', 1, rnd)
        after = state_at('guard', 0, rnd + 1)
        if not before or not after:
            check('guard: round %d states were found' % rnd, False)
            break
        g0, g1, b2 = spot(before, 'g'), spot(after, 'g'), spot(before, 'b2')
        d = dist(g0[:2], b2[:2])
        expect_awake = g0[2] or d <= 5
        check('guard: round %d, enemy %d squares away: awake=%s' % (rnd, d, expect_awake), g1[2] == expect_awake, (g1, d))
        if not g0[2] and g1[2] and woke is None:
            woke = rnd
        if not expect_awake:
            check('guard: round %d: asleep means it did not move' % rnd, g1[:2] == g0[:2], (g0, g1))
    check('guard: it did wake up during the battle', woke is not None)


def test_group_wakes_together():
    make_game('group', [red('g1', 8, 2, {'mode': 'guard', 'group': 'pack'}), red('g2', 13, 4, {'mode': 'guard', 'group': 'pack'}),
                        {'type': 'soldier', 'id': 'b2', 'side': 'blue', 'x': 3, 'y': 2}])
    after = state_at('group', 0, 2)
    check('group: the one that could reach the enemy woke', spot(after, 'g1')[2] is True, spot(after, 'g1'))
    check('group: its far-away mate woke with it and walked', spot(after, 'g2')[2] is True and spot(after, 'g2')[:2] != (13, 4), spot(after, 'g2'))


def test_seek_a_square_and_stay():
    make_game('seek', [red('s', 12, 4, {'mode': 'seek', 'x': 3, 'y': 4})])
    for rnd, want in ((2, (8, 4)), (3, (4, 4)), (4, (3, 4)), (5, (3, 4))):
        b = state_at('seek', 0, rnd)
        check('seek: after red turn %d it stands on %s' % (rnd - 1, want), spot(b, 's')[:2] == want, spot(b, 's'))


def test_from_turn():
    make_game('from', [red('r', 9, 2, {'fromTurn': 3})])
    for rnd, moved in ((2, False), (3, False), (4, True)):
        b = state_at('from', 0, rnd)
        check('fromTurn 3: after red turn %d it %s' % (rnd - 1, 'has moved' if moved else 'still stands at 9,2'), (spot(b, 'r')[:2] != (9, 2)) == moved, spot(b, 'r'))


def test_story_commands_change_behaviour():
    ev = [{'id': 'release', 'when': {'turn': 3}, 'do': [{'setBehaviour': 'advance', 'units': {'unit': 'h'}}, {'wakeGroup': 'pack'}]}]
    make_game('story', [red('h', 9, 0, 'hold'), red('g', 13, 4, {'mode': 'guard', 'group': 'pack'})], ev)
    b = state_at('story', 0, 2)
    check('story: before the event (it fires as round 3 begins) the hold unit stands still and the guard sleeps',
          spot(b, 'h')[:2] == (9, 0) and spot(b, 'g') == (13, 4, False), (spot(b, 'h'), spot(b, 'g')))
    b = state_at('story', 0, 4)
    check('story: after setBehaviour the unit advances', spot(b, 'h')[:2] != (9, 0), spot(b, 'h'))
    check('story: after wakeGroup the guard walks', spot(b, 'g')[:2] != (13, 4) and spot(b, 'g')[2] is True, spot(b, 'g'))


if not os.path.exists(PLAYER):
    print('FAIL the player %s is not built' % PLAYER)
    sys.exit(1)
for t in (test_hold_and_advance, test_guard_wakes_when_the_enemy_comes_into_reach, test_group_wakes_together, test_seek_a_square_and_stay,
          test_from_turn, test_story_commands_change_behaviour):
    t()
shutil.rmtree(TMP, ignore_errors=True)
summary()
sys.exit(1 if failures else 0)
