#!/usr/bin/env python3
"""Downed units and revive through the real player, windowless (docs/SRPG.md "Downed units and revive").

  python3 tests/downed_player_test.py ./build/engine/player/ote-player .
  python3 tests/downed_player_test.py ./build/engine/player/ote-player . --shots OUT_DIR   # also keeps the screenshots (BMP)

Writes a small game into a temp folder (the repository is never changed), runs the player with --headless --no-audio and --input scripts,
and judges the [info] lines, the Continue save and pixels:

  - blue ends its turn; the red grunt next to the frail squire strikes it: "downed: Squire", and the Continue save keeps the squire on its
    square with "downed": true and 0 HP;
  - the body is still drawn on its square (the tile differs from an empty one), with the "Down" marker;
  - on blue's next turn the cleric walks next to it and uses Raise: "revived: Squire by Cleric (5 HP)", and the save has the squire standing
    with 5 HP and no "downed".
"""
import os
import shutil
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import Bmp, run_text, check, failures, inp, lines, player_and_repo, player_cmd, read_json, shots_dir, summary, ticks, write_json  # noqa: E402
from ote_testkit import scratch_dir  # noqa: E402

sys.dont_write_bytecode = True

PLAYER, REPO = player_and_repo()
SHOTS = shots_dir()
TMP = scratch_dir()

W, H, TILE = 480, 270, 24
MAP_W, MAP_H = 9, 7
VIEW_W, VIEW_H = 352, 259  # the map viewport: the window minus the panel and the hint strip
IDLE = 'wait:14'           # the turn banner plays first
END_TURN = ['menu', 'wait:3', 'confirm']
RED_TURN = 'wait:200'      # the computer's turn and the next banner
# Blue's second turn: the cursor is on the cleric (0,2); select it, walk to 2,2 (next to the body at 3,2), choose Raise (the only thing it can
# do there besides waiting), the body is the one candidate.
RAISE = ['confirm', 'wait:3', 'right', 'wait:2', 'right', 'wait:2', 'confirm', 'wait:20', 'confirm', 'wait:3', 'confirm', 'wait:40']


def write_game():
    g = os.path.join(TMP, 'downed')
    shutil.rmtree(g, ignore_errors=True)
    write_json(g + '/game.json', {
        'formatVersion': 1, 'name': 'Downed Player Test', 'view': {'width': W, 'height': H, 'tileSize': TILE},
        'stats': [{'id': 'hp', 'name': 'HP'}, {'id': 'atk'}, {'id': 'def'}, {'id': 'mov', 'name': 'Move'}],
        'roles': {'hp': 'hp', 'move': 'mov'}, 'moveTypes': [{'id': 'foot'}],
        'rules': {'turnOrder': 'sides', 'counterAttacks': False, 'defeat': ['noUnits'], 'items': {'weapons': 'fixed'}, 'downed': {}},
        'formulas': {'damage': 'a.atk - b.def', 'hit': '100'}, 'maps': [{'id': 'field', 'name': 'Field', 'file': 'maps/field.json'}]})
    write_json(g + '/data/terrain.json', {'terrain': [{'id': 'plain', 'name': 'Plain', 'color': '#9ccc65'}]})
    stats = lambda hp_, atk, mv: {'hp': hp_, 'atk': atk, 'def': 0, 'mov': mv}
    write_json(g + '/data/units.json', {'units': [
        {'id': 'cleric', 'name': 'Cleric', 'moveType': 'foot', 'stats': stats(20, 0, 4), 'weapons': ['sword'], 'abilities': ['raise']},
        {'id': 'squire', 'name': 'Squire', 'moveType': 'foot', 'stats': stats(10, 1, 4), 'weapons': ['sword']},
        {'id': 'grunt', 'name': 'Grunt', 'moveType': 'foot', 'stats': stats(30, 12, 0), 'weapons': ['sword']}]})
    write_json(g + '/data/weapons.json', {'weapons': [{'id': 'sword', 'name': 'Sword'}]})
    write_json(g + '/data/abilities.json', {'abilities': [
        {'id': 'raise', 'name': 'Raise', 'target': 'ally', 'range': [1, 1], 'effect': {'revive': {'percent': 50}}}]})
    # The cleric far from the grunt (it cannot move: mov 0); the squire right next to it; another grunt far off keeps red in the battle.
    units = [('cleric', 'blue', 0, 2), ('squire', 'blue', 3, 2), ('grunt', 'red', 4, 2), ('grunt', 'red', 8, 6)]
    write_json(g + '/maps/field.json', {
        'name': 'Field', 'grid': {'type': 'square', 'width': MAP_W, 'height': MAP_H}, 'legend': {'.': 'plain'},
        'tiles': ['.' * MAP_W] * MAP_H,
        'sides': [{'id': 'blue', 'name': 'Blue', 'color': '#3d85c6', 'controller': 'human'},
                  {'id': 'red', 'name': 'Red', 'color': '#cc0000', 'controller': 'ai'}],
        'units': [{'type': t, 'side': s, 'x': x, 'y': y} for (t, s, x, y) in units]})


def play(name, script, interval=4):
    shot = os.path.join(TMP, name + '.bmp')
    sdir = os.path.join(TMP, 'saves_' + name)
    shutil.rmtree(sdir, ignore_errors=True)
    cmd = player_cmd(PLAYER, TMP, REPO) + ['--frames', str(ticks(script) * interval + 60), '--input-interval', str(interval), '--fixed-dt', '0.016',
                                            '--game', 'downed', '--map', 'field', '--input', script, '--screenshot', shot, '--saves', sdir]
    p = run_text(cmd)
    if SHOTS:
        os.makedirs(SHOTS, exist_ok=True)
        shutil.copy(shot, os.path.join(SHOTS, name + '.bmp'))
    units = []
    try:
        units = read_json(os.path.join(sdir, 'downed', 'continue.json'))['battle']['units']
    except (OSError, ValueError, KeyError):
        pass
    return Bmp(shot), p.stdout, units


def tile_spread(img, x, y):
    """How far the pixels of a tile's middle are from one colour (an empty plain tile is flat)."""
    mw, mh = MAP_W * TILE, MAP_H * TILE
    bx = int((VIEW_W - mw) / 2.0 + 0.5) + x * TILE
    by = int((VIEW_H - mh) / 2.0 + 0.5) + y * TILE
    px = [img.px[by + j][bx + i] for j in range(4, TILE - 4) for i in range(4, TILE - 4)]
    return max(max(p[k] for p in px) - min(p[k] for p in px) for k in range(3))


def main():
    write_game()
    # The fall.
    img, log, units = play('downed_fall', inp(IDLE, *END_TURN, RED_TURN))
    check('the grunt downs the squire', bool(lines(log, 'downed: Squire')), log[-800:])
    sq = units[1] if len(units) > 1 else {}
    check('the save keeps the squire downed on its square with 0 HP',
          sq.get('downed') is True and sq.get('hp') == 0 and sq.get('x', sq.get('cell')) is not None, sq)
    check('the body is still drawn on its square', tile_spread(img, 3, 2) > 30, (tile_spread(img, 3, 2), tile_spread(img, 5, 4)))
    check('an empty square is plain', tile_spread(img, 5, 4) < 30, tile_spread(img, 5, 4))
    check('no warnings or UI bugs in the fall run', 'UI bug' not in log and '[warn]' not in log, log[-400:])
    # The revive.
    _, log, units = play('downed_raise', inp(IDLE, *END_TURN, RED_TURN, *RAISE))
    rev = lines(log, 'revived: Squire by Cleric')
    check('the cleric raises the squire', len(rev) == 1 and '(5 HP)' in rev[0], log[-1000:])
    sq = units[1] if len(units) > 1 else {}
    check('the save has the squire standing with 5 HP', sq.get('hp') == 5 and 'downed' not in sq, sq)
    check('no warnings or UI bugs in the revive run', 'UI bug' not in log and '[warn]' not in log, log[-400:])
    summary()
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
