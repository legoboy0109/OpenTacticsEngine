#!/usr/bin/env python3
"""Scrolling a long list with the mouse, through the real player, windowless (docs/PORTING.md "Pointer scrolling", ui/row_list.h).

  python3 tests/list_scroll_player_test.py ./build/engine/player/ote-player .

The preparation shop of tests/fixtures/srpg-prep is given 32 wares, more than its panel shows. The player logs
"list scrolled: first row N ..." whenever a list's first visible row changes, and "prep panel shop: row0@x,y step=h shown=n"
where its rows are. The rules under test: hovering the rows, even creeping over the bottom ones for seconds, never scrolls
(it once raced through the whole list: every hovered row re-centred it); the wheel scrolls one row a notch; resting on the
"v" marker scrolls after a short dwell at a steady pace and stops when the pointer leaves; a click on it scrolls one row;
keys still scroll to keep the selection in view.
"""
import os
import re
import shutil
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ote_testkit import check, player_cmd, player_and_repo, read_json, run_text, scratch_dir, summary, ticks, write_json  # noqa: E402

sys.dont_write_bytecode = True

PLAYER, REPO = player_and_repo()
TMP = scratch_dir()
INTERVAL = 3
DT = 0.016
WARES = 32
# Chapter 1 played (as tests/srpg_player_prep_test.py does), then the preparation's main menu: Shop (the 4th entry).
TO_SHOP = 'confirm,confirm,confirm,wait:20,menu,confirm,wait:100,confirm,wait:30,confirm,confirm,confirm,wait:40,down,down,down,confirm,wait:3'


def make_game():
    g = os.path.join(TMP, 'longshop')
    shutil.copytree(os.path.join(REPO, 'tests/fixtures/srpg-prep'), g)
    items = read_json(g + '/data/items.json')
    for i in range(WARES - 2):
        items['items'].append({'id': 'ware%02d' % i, 'name': 'Ware %02d' % i, 'kind': 'usable', 'uses': 1, 'heal': 1, 'price': {'gold': 5}})
    write_json(g + '/data/items.json', items)
    orchard = read_json(g + '/maps/orchard.json')
    orchard['prep']['shop']['sells'] = ['potion', 'tonic'] + ['ware%02d' % i for i in range(WARES - 2)]
    write_json(g + '/maps/orchard.json', orchard)
    return 'longshop'


GAME = make_game()


def play(more):
    script = TO_SHOP + ',device:mouse,' + more if more else TO_SHOP
    cmd = player_cmd(PLAYER, TMP, REPO) + ['--no-saving', '--campaign', '--game', GAME, '--fixed-dt', str(DT),
                                           '--input-interval', str(INTERVAL), '--frames', str(ticks(script) * INTERVAL + 90), '--input', script]
    return run_text(cmd).stdout


def after_shop(log):
    """The log from the shop's panel line on."""
    i = log.find('prep panel shop:')
    return log[i:] if i >= 0 else ''


def firsts(log):
    return [int(n) for n in re.findall(r'list scrolled: first row (\d+)', after_shop(log))]


def main():
    log = play('')
    m = re.search(r'prep panel shop: row0@(\d+),(\d+) step=(\d+) shown=(\d+)', log)
    check('the shop opens with its long list', bool(m), log[-400:])
    if not m:
        summary()
        return 1
    x, y0, step, shown = (int(v) for v in m.groups())
    check('the list is longer than the panel (%d of %d shown)' % (shown, WARES), 3 <= shown < WARES)
    bottom = y0 - step // 2 + shown * step  # the first pixel under the rows: the "v" strip starts here

    # 1. The pointer creeps over the bottom rows for seconds (every row from the third-last to the last, small moves in each,
    # across row boundaries and onto the last row's bottom pixel): the first visible row never changes.
    moves = []
    for _ in range(6):
        for r in range(shown - 3, shown):
            for dy in (-step // 2 + 1, -1, 0, 2, step // 2 - 1):
                moves.append('move:%d:%d' % (x, y0 + r * step + dy))
    moves.append('move:%d:%d' % (x, bottom - 1))
    log = play(','.join(moves + ['wait:40']))
    check('hovering the bottom rows (%d moves, %.1f s) never scrolls the list' % (len(moves), len(moves) * INTERVAL * DT),
          firsts(log) == [], str(firsts(log)))

    # 2. The wheel: one row a notch, down and back up.
    log = play('move:%d:%d,zoomout,zoomout,zoomout,zoomin,wait:10' % (x, y0 + step))
    check('the wheel scrolls one row a notch (3 down, 1 up)', firsts(log) == [1, 2, 3, 2], str(firsts(log)))

    # 3. Resting on the "v" marker: after the dwell (0.3 s) one row, then 6 a second; leaving it stops at once.
    rest = 40  # wait:40 = 3 + 40 * 3 frames from the move to the next token = 1.97 s
    log = play('move:%d:%d,wait:%d,move:%d:%d,wait:60' % (x, bottom + 3, rest, x, y0 + step))
    f = firsts(log)
    held = (INTERVAL + rest * INTERVAL) * DT
    want = min(WARES - shown, 1 + int((held - 0.3) * 6))
    check('resting on "v" %.2f s scrolls %d rows one at a time (got %s)' % (held, want, f),
          bool(f) and abs(f[-1] - want) <= 1 and f == list(range(1, len(f) + 1)), str(f))
    # a short pass over it (under the dwell) does nothing
    log = play('move:%d:%d,wait:4,move:%d:%d,wait:30' % (x, bottom + 3, x, y0 + step))
    check('passing over "v" (0.2 s) does not scroll', firsts(log) == [], str(firsts(log)))

    # 4. A click on "v" scrolls one row (and the panel stays open: the click is no "tap outside").
    log = play('move:%d:%d,tap:%d:%d,move:%d:%d,wait:20' % (x, bottom + 3, x, bottom + 3, x, y0 + step))
    check('a click on "v" scrolls one row', firsts(log) == [1], str(firsts(log)))
    check('... and leaves the shop open', 'prep shop closed' not in after_shop(log) and 'UI bug' not in log)

    # 5. Keys still bring the selection into view (centred, as before).
    log = play(','.join(['device:keyboard'] + ['down'] * (shown + 2) + ['wait:10']))
    check('keys walking down scroll the list with them', bool(firsts(log)) and firsts(log)[-1] >= 3, str(firsts(log)))

    shutil.rmtree(TMP, ignore_errors=True)
    summary()
    return 1 if __import__('ote_testkit').failures else 0


if __name__ == '__main__':
    sys.exit(main())
