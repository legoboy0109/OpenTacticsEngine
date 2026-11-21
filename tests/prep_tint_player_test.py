#!/usr/bin/env python3
"""The preparation screen's deployment squares are a GROUND tint under the units, in every kind of view (docs/EVENTS.md "Preparation").

  python3 tests/prep_tint_player_test.py ./build/engine/player/ote-player . [--shots DIR]

Plays the srpg-prep fixture to the Positions screen of chapter 2 (the orchard) in the top-down view on a square map and a
hexagon map, and in the 2D isometric and 3D isometric kinds, takes a picture and reads pixels. --shots DIR keeps the pictures
(all four kinds).
"""
import os
import shutil
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import srpg_player_prep_test as P  # noqa: E402  (its helpers: make_game, go, positions, menu_to; nothing runs on import)
from ote_testkit import Bmp, check, failures, read_json, summary, write_json  # noqa: E402

sys.dont_write_bytecode = True

STYLES = [{'id': 'classic', 'name': 'Classic'}, {'id': 'isometric', 'name': 'Isometric', 'view': 'isometric'},
          {'id': 'isometric-3d', 'name': '3D Isometric', 'view': 'isometric-3d'}, {'id': 'full-3d', 'name': 'Full 3D', 'view': '3d'}]
KINDS = ['classic', 'isometric', 'isometric-3d', 'full-3d']


def tweak(hexes):
    def f(g):
        game = read_json(g + '/game.json')
        game['styles'] = STYLES
        if hexes:
            game['grid'] = {'type': 'hex', 'layout': 'pointy'}
        write_json(g + '/game.json', game)
        if hexes:
            for m in ('meadow', 'orchard', 'hilltop'):
                mp = g + '/maps/%s.json' % m
                j = read_json(mp)
                j['grid'] = {'type': 'hex', 'width': j['grid']['width'], 'height': j['grid']['height']}
                write_json(mp, j)
    return f


GROUND, UNIT = (143, 191, 106), (74, 127, 193)  # the fixture's plain ground and the placeholder unit of the blue side (flat colours)
FREE, TAKEN, CARRY = ((90, 170, 255), 110), ((110, 220, 120), 130), ((255, 220, 90), 170)  # the slot tints and their strength (of 255)


def blend(under, tint):
    rgb, a = tint
    return tuple(round(under[i] * (255 - a) / 255 + rgb[i] * a / 255) for i in range(3))


def box_count(bmp, centre, rgb, tol=3, r=14):
    return bmp.count(rgb, tol, (centre[0] - r, centre[1] - r, centre[0] + r + 1, centre[1] + r + 1))


def run(gid, kind, tag, more=''):
    name = 'prep_%s_%s' % (tag, kind)
    path = os.path.join(P.TMP, name + '.bmp')
    # the Positions screen: slots 1-3 hold units, slot 4 (on the pond) is free; `more` can pick a unit up
    log = P.go(gid, P.menu_to(1) + ',wait:5' + more, name=name, extra=['--campaign', '--style', kind])
    pos = P.positions(log)
    check(tag + ' ' + kind + ': the slots are logged and the picture taken', len(pos) == 5 and os.path.exists(path), str(pos))
    return pos, (Bmp(path) if os.path.exists(path) else None)


def test_view(gid, kind, tag):
    t = tag + ' ' + kind
    pos, bmp = run(gid, kind, tag)
    if not bmp or len(pos) != 5:
        return
    unit_here = box_count(bmp, pos[2], UNIT, 3)
    check(t + ': the unit on a taken slot keeps its own colours', unit_here >= 100, str(unit_here))
    wash = box_count(bmp, pos[2], blend(UNIT, TAKEN), 6)
    check(t + ': ... and is not washed with the slot colour', wash * 4 < unit_here, '%d of %d' % (wash, unit_here))
    ground = box_count(bmp, pos[2], blend(GROUND, TAKEN), 3)
    check(t + ': the ground of the taken square shows the green tint', ground >= 10, str(ground))
    free = box_count(bmp, pos[4], blend(UNIT, FREE), 3)  # (the free slot of the fixture is on the pond, which has the unit colour)
    check(t + ': a free slot shows the light-blue tint on the ground', free >= 100, str(free))
    wrong = box_count(bmp, pos[4], UNIT, 1)
    check(t + ': ... and no flat box of the old kind covers it twice', wrong < free, str(wrong))
    # one unit picked up: its square is the gold "being carried" tint, the unit still in front of it
    pos, bmp = run(gid, kind, tag + 'c', ',next,confirm,wait:3')
    if bmp and len(pos) == 5:
        gold = box_count(bmp, pos[2], blend(GROUND, CARRY), 20)  # (the cursor is on it: a little lighter)
        check(t + ': the carried slot is tinted gold on the ground', gold >= 10, str(gold))
        check(t + ': ... the unit still in front', box_count(bmp, pos[2], UNIT, 3) >= 100, str(box_count(bmp, pos[2], UNIT, 3)))


def main():
    cases = [('sq', False), ('hex', True)]
    for tag, hexes in cases:
        P.make_game('t' + tag, tweak=tweak(hexes))
    for tag, hexes in cases:
        for kind in (KINDS if tag == 'sq' else KINDS[:1]):
            test_view('t' + tag, kind, tag)
    summary(blank_line=True)
    shutil.rmtree(P.TMP, ignore_errors=True)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
