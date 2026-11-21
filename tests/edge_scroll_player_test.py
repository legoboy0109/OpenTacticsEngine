#!/usr/bin/env python3
"""Mouse scrolling of the battle map through the real player, windowless (docs/PORTING.md "Pointer scrolling").

  python3 tests/edge_scroll_player_test.py ./build/engine/player/ote-player .

A 40 x 30 square map is bigger than the 352 x 259 map view. The scripted pointer (`move:X:Y`, `leave`) stands where the test says
and the player logs "camera: X,Y" whenever the camera has been still for 20 frames after moving. The rules under test
(battle/edge_scroll.h): the map scrolls from the pointer only when the pointer is in the narrow band along the picture's edge,
after 0.25 s, at 8 tiles (192 pixels) a second, and stops at once when the pointer leaves the band or the window; a square that
is merely in the outer rows scrolls nothing; keys scroll as before; a finger and an open menu never scroll the map.
"""
import os
import re
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ote_testkit import check, edit, inp, player_and_repo, run_text, scratch_dir, summary  # noqa: E402
import shutil  # noqa: E402

PLAYER, REPO = player_and_repo()
TMP = scratch_dir()
DT = 0.016
INTERVAL = 3
TILE = 24
SPEED = 8 * TILE  # pixels a second
DWELL = 0.25


def make_game():
    g = os.path.join(TMP, 'wide')
    shutil.copytree(os.path.join(REPO, 'tests/fixtures/srpg-quotes'), g)

    def widen(m):
        m['grid'] = {'type': 'square', 'width': 40, 'height': 30}
        m['tiles'] = ['.' * 40 for _ in range(30)]
        for u in m['units']:  # everybody in the top-left corner, off the way
            u['x'] = min(u['x'], 6) if u['side'] == 'blue' else u['x']
    edit(g, 'maps/ch1.json', widen)
    return g


GAME = make_game()
INTRO = ['device:mouse', 'wait:60']


def gap(count):
    """Frames from a token to the one after the wait:count token that follows it (see script_input.cpp)."""
    return INTERVAL + INTERVAL * count


def run(script):
    cmd = [PLAYER, '--games', TMP, '--assets', os.path.join(REPO, 'assets'), '--headless', '--no-audio', '--no-settings', '--no-saving',
           '--game', 'wide', '--map', 'ch1', '--battle-scene', 'map', '--fixed-dt', str(DT), '--input-interval', str(INTERVAL),
           '--frames', str(sum(int(t[5:]) * INTERVAL if t.startswith('wait:') else INTERVAL for t in script.split(',')) + 120),
           '--input', script]
    return run_text(cmd, timeout=180).stdout


def cameras(log):
    return [(int(a), int(b)) for a, b in re.findall(r'\[info\] camera: (-?\d+),(-?\d+)', log)]


def main():
    # 1. In the band at the right edge for 0.2 s only (under the dwell), then away: nothing moves.
    log = run(inp(INTRO, 'move:350:100', 'wait:2', 'move:200:100', 'wait:20'))
    check('right band for less than the dwell: no scroll', not cameras(log), str(cameras(log)))

    # 2. In the band long enough: it moves at 192 px/s after the dwell, and stops the moment the pointer leaves the band.
    for count in (30, 60):
        frames = gap(count)
        want = (frames * DT - DWELL) * SPEED
        log = run(inp(INTRO, 'move:350:100', 'wait:%d' % count, 'move:200:100', 'wait:20'))
        cam = cameras(log)
        got = cam[-1][0] if cam else -1
        check('right band %d frames: x = (time - 0.25 s) * 192 px/s (%.0f, got %d)' % (frames, want, got),
              len(cam) == 1 and abs(got - want) <= 8 and cam[0][1] == 0, str(cam))
    # (the two runs differ by 30 intervals = 90 frames = 1.44 s: the speed is steady, whatever came before)
    # 3. The same by leaving the window (the mouse-leave event), and by the left band coming back.
    log = run(inp(INTRO, 'move:350:100', 'wait:40', 'leave', 'wait:20'))
    cam = cameras(log)
    want = (gap(40) * DT - DWELL) * SPEED
    check('leaving the window stops it at once', len(cam) == 1 and abs(cam[0][0] - want) <= 8, '%s want %.0f' % (cam, want))
    log = run(inp(INTRO, 'move:350:100', 'wait:40', 'move:200:100', 'wait:10', 'move:1:100', 'wait:60', 'move:200:100', 'wait:20'))
    cam = cameras(log)
    check('the left band takes it back (and stops at the map edge)', len(cam) == 2 and cam[0][0] > 200 and cam[1][0] == 0, str(cam))

    # 4. Down and up (the top and bottom bands), and a corner moves both ways.
    log = run(inp(INTRO, 'move:100:258', 'wait:40', 'move:200:100', 'wait:20'))
    cam = cameras(log)
    check('bottom band scrolls down', len(cam) == 1 and cam[0][0] == 0 and cam[0][1] > 150, str(cam))
    log = run(inp(INTRO, 'move:351:258', 'wait:30', 'move:200:100', 'wait:20'))
    cam = cameras(log)
    check('a corner scrolls both ways', len(cam) == 1 and cam[0][0] > 150 and cam[0][1] > 150, str(cam))

    # 5. A square in the outer rows (inside the old two-tile margin, outside the band) scrolls nothing, however long the pointer rests.
    for x, y in ((340, 100), (320, 100), (100, 240), (330, 240)):
        log = run(inp(INTRO, 'move:%d:%d' % (x, y), 'wait:60', 'wait:60'))
        check('resting on the outer rows (%d,%d) does not scroll' % (x, y), not cameras(log), str(cameras(log)))
    # One tile inside the edge is no edge either.
    log = run(inp(INTRO, 'move:%d:100' % (352 - TILE), 'wait:60', 'wait:60'))
    check('one tile inside the right edge does not scroll', not cameras(log), str(cameras(log)))

    # 6. The panel side: the band at the very edge of the picture (over the info panel) counts too.
    log = run(inp(INTRO, 'move:479:100', 'wait:30', 'move:200:100', 'wait:20'))
    check('the picture\'s own right edge scrolls', len(cameras(log)) == 1 and cameras(log)[0][0] > 150, str(cameras(log)))

    # 7. Keys move the cursor and the camera follows, as ever.
    log = run(inp(INTRO, ['right'] * 20, 'wait:30'))
    cam = cameras(log)
    check('keys: the cursor walking right brings the camera along', bool(cam) and cam[-1][0] > 100, str(cam))
    # ... and a mouse left at the edge does not scroll once a key was pressed (the cursor's own two-tile margin still moves the
    # picture a little, as it always did: 44 pixels; the pointer would have gone on for hundreds).
    log = run(inp(INTRO, 'move:350:100', 'wait:2', 'down', 'wait:60'))
    check('a key press ends the pointer scrolling', not cameras(log) or cameras(log)[-1][0] <= 60, str(cameras(log)))

    # 8. A finger never scrolls, and no menu open does.
    log = run(inp('device:touch', 'wait:60', 'move:350:100', 'wait:60'))
    check('touch at the edge does not scroll', not cameras(log), str(cameras(log)))
    log = run(inp(INTRO, 'menu', 'wait:6', 'move:350:100', 'wait:60'))
    check('an open menu: no pointer scrolling', not cameras(log), str(cameras(log)))

    shutil.rmtree(TMP, ignore_errors=True)
    summary()
    return 1 if __import__('ote_testkit').failures else 0


if __name__ == '__main__':
    sys.exit(main())
