#!/usr/bin/env python3
"""Guests and an ability on its user through the real player, windowless (docs/SRPG.md "Guests", "An ability on its user").

  python3 tests/guest_player_test.py ./build/engine/player/ote-player .

Writes a small game into a temp folder (the repository is never changed), runs the player with --headless --no-audio and --input scripts,
and judges the [info] lines and the Continue save:

  - at the start of Blue's turn the computer moves Blue's guest first ("ai: guests of Blue act"), then the person gets the turn; the
    Continue save (written at the turn start) marks only the guest;
  - the person's cleric picks itself for Focus (a condition, range from 0): one "ability:" line; the save written after it has Haste on
    the cleric, and the guest has walked towards the foe and is done.
"""
import json
import os
import shutil
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import run_text, check, failures, lines, player_and_repo, player_cmd, summary, write_json  # noqa: E402
from ote_testkit import scratch_dir  # noqa: E402

sys.dont_write_bytecode = True

PLAYER, REPO = player_and_repo()
TMP = scratch_dir()

W, H, TILE = 480, 270, 24
MAP_W, MAP_H = 9, 5
IDLE = 'wait:20'            # the turn banner plays and the guest acts first
SELECT = 'confirm,confirm'  # select the unit under the cursor, stay where it is: its action menu opens


def write_game():
    g = os.path.join(TMP, 'guests')
    shutil.rmtree(g, ignore_errors=True)
    write_json(g + '/game.json', {
        'formatVersion': 1, 'name': 'Guest Player Test', 'view': {'width': W, 'height': H, 'tileSize': TILE},
        'stats': [{'id': 'hp', 'name': 'HP'}, {'id': 'atk'}, {'id': 'def'}, {'id': 'mov', 'name': 'Move'}],
        'roles': {'hp': 'hp', 'move': 'mov'}, 'moveTypes': [{'id': 'foot'}],
        'rules': {'turnOrder': 'sides', 'counterAttacks': True, 'defeat': ['noUnits'], 'items': {'weapons': 'fixed'}},
        'formulas': {'damage': 'a.atk - b.def'}, 'maps': [{'id': 'field', 'name': 'Field', 'file': 'maps/field.json'}]})
    write_json(g + '/data/terrain.json', {'terrain': [{'id': 'plain', 'name': 'Plain', 'color': '#9ccc65'}]})
    stats = lambda hp_, mv: {'hp': hp_, 'atk': 5, 'def': 1, 'mov': mv}
    write_json(g + '/data/units.json', {'units': [
        {'id': 'cleric', 'name': 'Cleric', 'moveType': 'foot', 'stats': stats(20, 4), 'weapons': ['sword'], 'abilities': ['focus']},
        {'id': 'fencer', 'name': 'Ritz', 'moveType': 'foot', 'stats': stats(20, 3), 'weapons': ['sword']},
        {'id': 'grunt', 'name': 'Grunt', 'moveType': 'foot', 'stats': stats(30, 2), 'weapons': ['sword']}]})
    write_json(g + '/data/weapons.json', {'weapons': [{'id': 'sword', 'name': 'Sword'}]})
    write_json(g + '/data/abilities.json', {'abilities': [
        {'id': 'focus', 'name': 'Focus', 'target': 'unit', 'range': [0, 2], 'inflict': {'condition': 'haste'}}]})
    write_json(g + '/data/conditions.json', {'conditions': [{'id': 'haste', 'name': 'Haste', 'kind': 'helpful', 'turns': 3,
                                                             'blocks': [{'when': 'always', 'do': [{'stats': {'mov': 1}}]}]}]})
    # The cleric at 1,2, the guest Ritz at 1,0, a grunt far off at 8,0.
    write_json(g + '/maps/field.json', {
        'name': 'Field', 'grid': {'type': 'square', 'width': MAP_W, 'height': MAP_H}, 'legend': {'.': 'plain'},
        'tiles': ['.' * MAP_W] * MAP_H,
        'sides': [{'id': 'blue', 'name': 'Blue', 'color': '#3d85c6', 'controller': 'human'},
                  {'id': 'red', 'name': 'Red', 'color': '#cc0000', 'controller': 'ai'}],
        'units': [{'type': 'cleric', 'id': 'cleric', 'side': 'blue', 'x': 1, 'y': 2},
                  {'type': 'fencer', 'id': 'ritz', 'side': 'blue', 'x': 1, 'y': 0, 'guest': True, 'behaviour': 'advance'},
                  {'type': 'grunt', 'side': 'red', 'x': 8, 'y': 0}]})


def play(name, inp, tail=60):
    frames = sum(int(t[5:]) * 4 if t.startswith('wait:') else 4 for t in inp.split(',')) + tail
    sdir = os.path.join(TMP, 'saves_' + name)
    shutil.rmtree(sdir, ignore_errors=True)
    cmd = player_cmd(PLAYER, TMP, REPO) + ['--frames', str(frames), '--input-interval', '4', '--fixed-dt', '0.016', '--game', 'guests',
                                            '--map', 'field', '--input', inp, '--saves', sdir]
    p = run_text(cmd)
    save = None
    path = os.path.join(sdir, 'guests', 'continue.json')
    if os.path.exists(path):
        save = json.load(open(path))
    return p.stdout, save


def main():
    write_game()
    log, save = play('guest_turn', IDLE)
    check('the game loads and the battle starts', '[error]' not in log and 'ai: guests of Blue act' in log, log[-800:])
    check('the guest acts before the person (nothing of Red has acted yet)', not lines(log, 'ai: Red acts'), lines(log, 'ai:'))
    check('no warnings or UI bugs', 'UI bug' not in log and '[warn]' not in log, log[-400:])
    if save:
        units = save['battle']['units']
        check('only the guest is marked in the save', units[1].get('guest') is True and 'guest' not in units[0] and 'guest' not in units[2], units)
    else:
        check('a Continue save was written', False, log[-400:])
    # The cleric picks itself for Focus: the unit pick starts on the first candidate (the cleric, unit 0).
    log, save = play('self_focus', IDLE + ',' + SELECT + ',confirm,confirm,wait:30')
    used = lines(log, 'ability: Cleric uses Focus')
    check('Focus is used once, on the cleric itself', len(used) == 1 and 'on unit 0' in used[0], lines(log, 'ability') + lines(log, 'pick'))
    if save:
        conds = save['battle']['units'][0].get('conditions', [])
        check('the cleric has the condition in the save', len(conds) == 1, save['battle']['units'][0])
        ritz = save['battle']['units'][1]
        check('the guest walked towards the foe and is done', ritz.get('cell') != 1 and ritz.get('done') is True, ritz)
    summary()
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
