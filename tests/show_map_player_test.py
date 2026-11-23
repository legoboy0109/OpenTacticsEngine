#!/usr/bin/env python3
"""A story scene on another map through the real player, windowless (docs/EVENTS.md "Showing another map").

  python3 tests/show_map_player_test.py ./build/engine/player/ote-player . [--shots DIR]

  Game: tests/fixtures/scene-map. The keep's battle-start event says a line, fades out, shows the cut-scene map "flashback", fades in, has the
  elder speak, adds a unit there, moves the elder, stages a fight in which the elder kills the bandit, sets a switch, fades out, returns and
  says a last line on the keep.
  - The lines come in order, the shown map is announced between the hero's lines, the staged fight plays (the side-view battle scene and on
    the map), and after the return the battle's own units are on their squares with full hit points (the Continue save written at the
    start of the turn) and the battle goes on.
  - The picture while the elder speaks is the flashback map (7 x 4, its wall and river) in the top-down view and in the isometric view; the
    picture after the scene is the keep (9 x 3).
  - The cut-scene map is not offered in the map list and --map refuses it.
Never plays audio or opens a window; --shots DIR keeps pictures (BMP).
"""
import os
import re
import shutil
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import (  # noqa: E402
    Bmp, check, edit, failures, player_and_repo, player_cmd, run_text, save_of, scratch_dir, shots_dir, summary, ticks)

sys.dont_write_bytecode = True

PLAYER, REPO = player_and_repo()
SHOTS = shots_dir()
TMP = scratch_dir()
GID = 'sm'
TO_ELDER = 'wait:20,confirm,wait:40'  # the hero's line, then the fade and the swap: the elder's line is up
WHOLE = TO_ELDER + ',confirm,wait:200,confirm,wait:60,confirm,wait:60'  # ... the fight, "Who is next?", the return, "Never again."
GRASS, WALL, RIVER = (0x9c, 0xcc, 0x65), (0x4a, 0x4a, 0x4a), (0x3d, 0x85, 0xc6)


def make_game():
    g = os.path.join(TMP, GID)
    shutil.rmtree(g, ignore_errors=True)
    shutil.copytree(os.path.join(REPO, 'tests/fixtures/scene-map'), g)

    def styles(o):
        o['styles'] = [{'id': 'classic', 'name': 'Classic'}, {'id': 'isometric', 'name': 'Isometric', 'view': 'isometric'}]
    edit(g, 'game.json', styles)


def keep(path, name):
    if SHOTS and os.path.exists(path):
        os.makedirs(SHOTS, exist_ok=True)
        shutil.copy(path, os.path.join(SHOTS, name + '.bmp'))


def play(inp, extra=(), saves=None, shot=None, frames=None):
    cmd = player_cmd(PLAYER, TMP, REPO) + ['--frames', str(frames or ticks(inp) * 3 + 120), '--input-interval', '3', '--fixed-dt', '0.016',
                                           '--game', GID, '--input', inp]
    cmd += ['--saves', saves] if saves else ['--no-saving']
    cmd += list(extra)
    if shot:
        cmd += ['--screenshot', shot]
    return run_text(cmd).stdout


def dialogue(log):
    return [m.group(1) for m in re.finditer(r'\[info\] dialogue: (.*)', log)]


def test_the_scene_plays_and_the_battle_is_untouched():
    for fights in ('scene', 'map'):
        saves = os.path.join(TMP, 'saves_' + fights)
        shutil.rmtree(saves, ignore_errors=True)
        log = play(WHOLE, ('--map', 'keep', '--battle-scene', fights), saves=saves)
        got = dialogue(log)
        want = ['Hero | I remember that day.', 'Elder | Leave this village.', 'Elder | Who is next?', 'Hero | Never again.']
        check(fights + ': the four lines in order', got == want, str(got) + log[-600:])
        shown, back = log.find('scene: showing map flashback'), log.find("scene: back to the battle's map")
        check(fights + ': the map is shown after the first line and left before the last',
              log.find('I remember') < shown < log.find('Leave this') and log.find('Who is next') < back < log.find('Never again'), log[-800:])
        if fights == 'scene':
            check('scene: the staged fight plays in the battle scene, the bandit falls',
                  'battle scene: start Elder' in log and 'battle scene: defeated Bandit' in log, log[-800:])
        check(fights + ': no warning about the scene', 'scene: a map entry out of place' not in log and 'have no end' not in log)
        check(fights + ': the battle goes on (no result)', 'battle over' not in log.lower() and '[info] turn: blue, round 1' in log, log[-400:])
        save = save_of(saves, GID)
        units = save and {u['id']: (u.get('cell'), u.get('hp')) for u in save['battle']['units']}
        # The keep is 9 wide: hero (1,1) = 10, wing (1,0) = 1, r1 (7,1) = 16; fighter 24, bird 12, raider 22 hit points.
        check(fights + ': the battle\'s own units on their squares, unhurt, none added',
              units == {'hero': (10, 24), 'wing': (1, 12), 'r1': (16, 22)}, str(units))
        sw = save and save['battle'].get('story', {}).get('switches')
        check(fights + ': the switch set on the shown map stays', sw in ([True], [1]), str(sw))  # the game's one switch, "remembered"


class Pic(Bmp):
    def grid_box(self):
        """The bounding box of the grass, wall and river pixels (the map's squares) in the left part of the picture (not the panel)."""
        xs, ys = [], []
        for y in range(0, self.h, 2):
            for x in range(0, self.w * 7 // 10, 2):
                p = self.px[y][x]
                if any(all(abs(p[i] - c[i]) <= 14 for i in range(3)) for c in (GRASS, WALL, RIVER)):
                    xs.append(x)
                    ys.append(y)
        return (min(xs), min(ys), max(xs), max(ys)) if xs else None


def test_the_pictures():
    for style in ('classic', 'isometric'):
        shot = os.path.join(TMP, 'elder_' + style + '.bmp')
        log = play(TO_ELDER, ('--map', 'keep', '--style', style), shot=shot)
        keep(shot, 'show_map_elder_' + style)
        check(style + ': the elder speaks on the shown map', 'scene: showing map flashback' in log and 'Elder | Leave this village.' in dialogue(log), log[-500:])
        if not os.path.exists(shot):
            check(style + ': a picture', False)
            continue
        p = Pic(shot)
        check(style + ': the flashback\'s wall and river are drawn', p.count(WALL, 14) > 40 and p.count(RIVER, 14) > 40,
              '%d wall, %d river' % (p.count(WALL, 14), p.count(RIVER, 14)))
        if style == 'classic':
            box = p.grid_box()
            w, h = (box[2] - box[0] + 2, box[3] - box[1] + 2) if box else (0, 0)
            check('classic: the map drawn is 7 x 4 squares (the flashback, not the 9 x 3 keep)', box is not None and abs(w * 4 - h * 7) <= 4 * 7, str(box))
    shot = os.path.join(TMP, 'after.bmp')
    play(WHOLE, ('--map', 'keep', '--battle-scene', 'map'), shot=shot)
    keep(shot, 'show_map_after')
    if os.path.exists(shot):
        p = Pic(shot)
        box = p.grid_box()
        w, h = (box[2] - box[0] + 2, box[3] - box[1] + 2) if box else (0, 0)
        check('after: the keep is back (9 x 3, its river, no wall)', box is not None and abs(w * 3 - h * 9) <= 3 * 9 and p.count(WALL, 14) < 10, str(box))


def test_the_cut_scene_map_is_not_played():
    log = play('wait:5', ('--map', 'flashback'))
    check('--map refuses the cut-scene map', 'cut-scene map' in log and 'showing map' not in log, log[-400:])
    log = play('wait:5', ('--map', 'field'))
    check('... another map plays', 'cut-scene map' not in log and 'turn: blue' in log, log[-400:])


make_game()
test_the_scene_plays_and_the_battle_is_untouched()
test_the_pictures()
test_the_cut_scene_map_is_not_played()
summary()
if not failures:
    shutil.rmtree(TMP, ignore_errors=True)
sys.exit(1 if failures else 0)
