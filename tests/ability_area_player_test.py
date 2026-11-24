#!/usr/bin/env python3
"""Abilities with an area and several effects through the real player, windowless (docs/SRPG.md "Areas and several effects for abilities").

  python3 tests/ability_area_player_test.py ./build/engine/player/ote-player .
  python3 tests/ability_area_player_test.py ./build/engine/player/ote-player . --shots OUT_DIR   # also keeps the screenshots (BMP)

Writes a small game into a temp folder (the repository is never changed), runs the player with --headless --no-audio and --input scripts,
and judges the [info] lines, the Continue save and pixels:

  - choosing an area ability opens a square pick; the hint line names who it acts on ("area ability forecast: Barrier: 2 units ...") and the
    squares of the area are tinted red (the other squares are not);
  - confirming carries it out: one "ability:" line on square N, and both units of the user's side in the area have both conditions of the
    "inflict" list in the Continue save (the foe next to them, outside the area, has none);
  - the Abilities page words: the ability's line in the log of the menu shows it is offered.
"""
import json
import math
import os
import shutil
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import Bmp, run_text, check, failures, lines, player_and_repo, player_cmd, shots_dir, summary, write_json  # noqa: E402
from ote_testkit import scratch_dir  # noqa: E402

sys.dont_write_bytecode = True

PLAYER, REPO = player_and_repo()
SHOTS = shots_dir()
TMP = scratch_dir()

W, H, TILE = 480, 270, 24
MAP_W, MAP_H = 9, 7
VIEW_W, VIEW_H = 352, 259  # the map viewport: the window minus the panel and the hint strip
IDLE = 'wait:14'           # the turn banner plays first
SELECT = 'confirm,confirm'  # select the unit under the cursor, stay where it is: its action menu opens
PROTECT, HASTE = 0, 1       # condition indices


def write_game():
    g = os.path.join(TMP, 'areas')
    shutil.rmtree(g, ignore_errors=True)
    write_json(g + '/game.json', {
        'formatVersion': 1, 'name': 'Ability Areas Player Test', 'view': {'width': W, 'height': H, 'tileSize': TILE},
        'stats': [{'id': 'hp', 'name': 'HP'}, {'id': 'atk'}, {'id': 'def'}, {'id': 'mov', 'name': 'Move'}],
        'roles': {'hp': 'hp', 'move': 'mov'}, 'moveTypes': [{'id': 'foot'}],
        'rules': {'turnOrder': 'sides', 'counterAttacks': True, 'defeat': ['noUnits'], 'items': {'weapons': 'fixed'}},
        'formulas': {'damage': 'a.atk - b.def'}, 'maps': [{'id': 'field', 'name': 'Field', 'file': 'maps/field.json'}]})
    write_json(g + '/data/terrain.json', {'terrain': [{'id': 'plain', 'name': 'Plain', 'color': '#9ccc65'}]})
    stats = lambda hp_, mv: {'hp': hp_, 'atk': 5, 'def': 1, 'mov': mv}
    write_json(g + '/data/units.json', {'units': [
        {'id': 'cleric', 'name': 'Cleric', 'moveType': 'foot', 'stats': stats(20, 4), 'weapons': ['sword'], 'abilities': ['barrier']},
        {'id': 'squire', 'name': 'Squire', 'moveType': 'foot', 'stats': stats(20, 4), 'weapons': ['sword']},
        {'id': 'grunt', 'name': 'Grunt', 'moveType': 'foot', 'stats': stats(12, 3), 'weapons': ['sword']}]})
    write_json(g + '/data/weapons.json', {'weapons': [{'id': 'sword', 'name': 'Sword'}]})
    blessing = lambda cid, name: {'id': cid, 'name': name, 'kind': 'helpful', 'turns': 3,
                                  'blocks': [{'when': 'always', 'do': [{'stats': {'def': 1}}]}]}
    write_json(g + '/data/conditions.json', {'conditions': [blessing('protect', 'Protect'), blessing('haste', 'Haste')]})
    write_json(g + '/data/abilities.json', {'abilities': [
        {'id': 'barrier', 'name': 'Barrier', 'target': 'ally', 'range': [0, 0], 'area': {'shape': 'burst'},
         'inflict': [{'condition': 'protect'}, {'condition': 'haste'}]}]})
    # The cleric at 2,2 with the squire next to it (3,2); a grunt two squares away (5,2: outside the burst); another far off.
    units = [('cleric', 'blue', 2, 2), ('squire', 'blue', 3, 2), ('grunt', 'red', 5, 2), ('grunt', 'red', 8, 6)]
    write_json(g + '/maps/field.json', {
        'name': 'Field', 'grid': {'type': 'square', 'width': MAP_W, 'height': MAP_H}, 'legend': {'.': 'plain'},
        'tiles': ['.' * MAP_W] * MAP_H,
        'sides': [{'id': 'blue', 'name': 'Blue', 'color': '#3d85c6', 'controller': 'human'},
                  {'id': 'red', 'name': 'Red', 'color': '#cc0000', 'controller': 'ai'}],
        'units': [{'type': t, 'side': s, 'x': x, 'y': y} for (t, s, x, y) in units]})


def play(name, inp, saves=False, tail=60):
    frames = sum(int(t[5:]) * 4 if t.startswith('wait:') else 4 for t in inp.split(',')) + tail
    shot = os.path.join(TMP, name + '.bmp')
    sdir = os.path.join(TMP, 'saves_' + name)
    shutil.rmtree(sdir, ignore_errors=True)
    cmd = player_cmd(PLAYER, TMP, REPO) + ['--frames', str(frames), '--input-interval', '4', '--fixed-dt', '0.016', '--game', 'areas',
                                            '--map', 'field', '--input', inp, '--screenshot', shot]
    cmd += ['--saves', sdir] if saves else ['--no-saving']
    p = run_text(cmd)
    if SHOTS:
        os.makedirs(SHOTS, exist_ok=True)
        shutil.copy(shot, os.path.join(SHOTS, name + '.bmp'))
    save = None
    path = os.path.join(sdir, 'areas', 'continue.json')
    if saves and os.path.exists(path):
        save = json.load(open(path))
    return Bmp(shot), p.stdout, save


def redness(img, x, y):
    """How red a tile's corner is (the covered squares are tinted red): red minus green and blue."""
    mw, mh = MAP_W * TILE, MAP_H * TILE
    bx = int(math.floor((VIEW_W - mw) / 2.0 + 0.5)) + x * TILE
    by = int(math.floor((VIEW_H - mh) / 2.0 + 0.5)) + y * TILE
    p = img.px[by + 3][bx + 3]
    return p[0] - (p[1] + p[2]) // 2


def conditions(save, index):
    u = save['battle']['units'][index]
    return sorted(c.get('condition', c.get('id', -1)) if isinstance(c, dict) else c for c in u.get('conditions', []))


def main():
    write_game()
    # The pick: the hint line and the red area.
    img, log, _ = play('area_pick', IDLE + ',' + SELECT + ',confirm,wait:3')
    check('the action menu offers Barrier and choosing it opens a square pick',
          'ability chosen: Barrier' in log and 'pick square: 1 candidates' in log, log[-600:])
    hint = lines(log, 'area ability forecast:')
    check('the hint names who the area acts on', bool(hint) and 'Barrier: 2 units (Cleric Protect Haste, Squire Protect Haste)' in hint[-1], hint)
    plain = redness(img, 7, 0)
    empty = [(1, 2), (2, 1), (2, 3)]  # (the two units' own squares are covered by their blocks where this looks)
    check('the squares of the burst are tinted red', all(redness(img, x, y) > plain + 40 for (x, y) in empty),
          [redness(img, x, y) for (x, y) in empty] + [plain])
    check('a square outside the burst is not red', redness(img, 4, 2) < plain + 20, (redness(img, 4, 2), plain))
    check('no warnings or UI bugs in the pick run', 'UI bug' not in log and '[warn]' not in log, log[-400:])
    # Confirming carries it out.
    _, log, save = play('area_use', IDLE + ',' + SELECT + ',confirm,confirm,wait:30', saves=True)
    used = lines(log, 'ability: Cleric uses Barrier')
    check('the use is logged once, on the square it was aimed at', len(used) == 1 and 'on unit -1 square %d' % (2 * MAP_W + 2) in used[0], used)
    check('a Continue save was written', save is not None, log[-400:])
    if save:
        both = sorted([PROTECT, HASTE])
        check('the cleric (the user, in its own burst) has both conditions', conditions(save, 0) == both, save['battle']['units'][0])
        check('the squire next to it has both conditions', conditions(save, 1) == both, save['battle']['units'][1])
        check('the grunt outside the burst has none', conditions(save, 2) == [], save['battle']['units'][2])
    summary()
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
