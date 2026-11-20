#!/usr/bin/env python3
"""Facing in the real player, windowless (docs/SRPG.md "Facing", "Player").

  python3 tests/facing_player_test.py ./build/engine/player/ote-player . [--shots DIR]

Plays a copy of the synthetic game tests/fixtures/facing (not a demo), map "duel": blue lancer (2,2) looking east and archer (1,4) looking north, red
dummies on (5,2) and (6,0) looking north and south. Sides have colours, so the facing marker (a light yellow arrowhead with a dark rim on the edge of the
unit's square) can be found in the pictures. Judged from the [info] lines the player logs and the screenshots:

  - the marker points the way the unit looks, in every kind of view (top-down, isometric, 3D isometric, full 3D);
  - Wait in a facing game asks which way the unit looks (the pick is skippable: Cancel or Confirm keep the direction, an arrow turns it at once on squares);
  - a move turns the unit toward its last step; an attack turns it toward its target;
  - the forecast names the arc ("Backstab") when the attack is not from the front, and the box fits at UI scale 100 and 150;
  - a game without facing has no pick and no marker.
Pictures: --shots DIR keeps them (BMP).
"""
import json
import os
import re
import shutil
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import Bmp, check, edit, player_and_repo, run_text, scratch_dir, shots_dir, summary, ticks  # noqa: E402

sys.dont_write_bytecode = True
PLAYER, REPO = player_and_repo()
SHOTS = shots_dir(absolute=True)
TMP = scratch_dir()
GAMES = os.path.join(TMP, 'games')
IDLE = 'wait:14'  # the turn banner plays first; it takes the first inputs otherwise
PICK = 'confirm'  # the first unit (the lancer) is selected: its reach is painted
STYLES = [('classic', 'top-down'), ('isometric', 'isometric'), ('isometric-3d', '3D isometric'), ('full-3d', 'full 3D')]
MARK = (255, 236, 140)  # the marker's fill (engine/player/battle/facing_marker.cpp)


def make_game(facing=True, hex_=False, maps=None):
    shutil.rmtree(GAMES, ignore_errors=True)
    g = os.path.join(GAMES, 'fcg')
    shutil.copytree(os.path.join(REPO, 'tests', 'fixtures', 'facing'), g)

    def game(j):
        j['styles'] = [{'id': 'classic', 'name': 'Classic'}, {'id': 'isometric', 'name': 'Isometric', 'view': 'isometric'},
                       {'id': 'isometric-3d', 'name': '3D Isometric', 'view': 'isometric-3d'}, {'id': 'full-3d', 'name': 'Full 3D', 'view': '3d'}]
        if hex_:
            j['grid'] = {'type': 'hex', 'layout': 'pointy'}
        if not facing:
            del j['rules']['facing']
            j['formulas']['damage'] = 'math.max(1, a.str + w.might - b.def)'
    edit(g, 'game.json', game)

    def duel(j):
        j['sides'][0]['color'] = '#3060ff'
        j['sides'][1]['color'] = '#ff3030'
        if hex_:
            j['grid'] = {'type': 'hex', 'width': j['grid']['width'], 'height': j['grid']['height']}
            for u in j['units']:
                u['facing'] = {'north': 'northeast', 'south': 'southwest', 'east': 'east'}.get(u.get('facing'), u.get('facing'))
        if not facing:
            for u in j['units']:
                u.pop('facing', None)
        if maps:
            maps(j)
    edit(g, 'maps/duel.json', duel)
    if not facing:
        edit(g, 'data/skills.json', lambda j: j.update({'skills': []}))
        edit(g, 'data/units.json', lambda j: [u.pop('skills', None) for u in j['units']])


def play(inp, name, style='classic', scale=None):
    shot = os.path.join(TMP, name + '.bmp')
    cmd = [PLAYER, '--games', GAMES, '--assets', os.path.join(REPO, 'assets'), '--headless', '--no-audio',
           '--frames', str(ticks(inp) * 4 + 80), '--input-interval', '4', '--fixed-dt', '0.016',
           '--game', 'fcg', '--map', 'duel', '--style', style, '--3d', 'software', '--no-settings', '--no-saving', '--input', inp, '--screenshot', shot]
    if scale:
        cmd += ['--ui-scale', str(scale)]
    log = run_text(cmd).stdout
    if SHOTS and os.path.exists(shot):
        os.makedirs(SHOTS, exist_ok=True)
        shutil.copy(shot, os.path.join(SHOTS, 'fc_' + name + '.bmp'))
    return log, shot


def lines(log, prefix):
    out = []
    for line in log.split('\n'):
        m = re.search(r'\[info\]\s*(.*)$', line)
        if m and m.group(1).startswith(prefix):
            out.append(m.group(1))
    return out


def clean(log):
    return 'UI bug' not in log and '[warn]' not in log and '[error]' not in log


def marker_pixels(shot, box=None):
    """Positions of the marker's fill colour in the picture (inside the box (x0, y0, x1, y1) when given)."""
    p = Bmp(shot)
    x0, y0, x1, y1 = box or (0, 0, p.w, p.h)
    out = []
    for y in range(y0, min(y1, p.h)):
        for x in range(x0, min(x1, p.w)):
            c = p.px[y][x]
            if abs(c[0] - MARK[0]) <= 6 and abs(c[1] - MARK[1]) <= 6 and abs(c[2] - MARK[2]) <= 6:
                out.append((x, y))
    return out


def centroid(pts):
    return (sum(p[0] for p in pts) / float(len(pts)), sum(p[1] for p in pts) / float(len(pts))) if pts else None


def differs(a, b):
    pa, pb = Bmp(a), Bmp(b)
    return sum(1 for y in range(pa.h) for x in range(pa.w) if pa.px[y][x] != pb.px[y][x])


def only_in(a, b):
    """Marker pixels in picture a that are not in picture b."""
    pb = set(marker_pixels(b))
    return [p for p in marker_pixels(a) if p not in pb]


def main():
    make_game()
    stay = IDLE + ',' + PICK + ',confirm,wait:4'      # the lancer stays where it is: the action menu opens (Info, Wait)
    wait = stay + ',down,confirm,wait:6'              # ... Wait: in a facing game the direction pick opens

    # ---- the pick
    log, _ = play(wait, 'pick')
    p = lines(log, 'facing pick:')
    check('Wait in a facing game opens the direction pick', p == ['facing pick: Lancer looks east'], p)
    log, _ = play(wait + ',left,wait:30', 'west')
    c = lines(log, 'facing chosen:')
    check('an arrow turns the unit at once (squares): "looks west (was east)"', c == ['facing chosen: Lancer looks west (was east)'], c)
    check('... and the view plays it', any('faced: Lancer looks west' in l for l in lines(log, 'faced:')), lines(log, 'faced:'))
    check('the pick: nothing warned about', clean(log), log[-300:])
    log, _ = play(wait + ',confirm,wait:30', 'keep')
    c = lines(log, 'facing chosen:')
    check('Confirm keeps the direction and ends the turn', c == ['facing chosen: Lancer looks east (was east)'] and not lines(log, 'faced:'), (c, lines(log, 'faced:')))
    log, _ = play(wait + ',cancel,wait:30', 'cancel')
    c = lines(log, 'facing chosen:')
    check('Cancel keeps the direction too', c == ['facing chosen: Lancer looks east (was east)'], c)
    log, _ = play(wait + ',down,wait:30', 'south')
    c = lines(log, 'facing chosen:')
    check('Down turns it south', c == ['facing chosen: Lancer looks south (was east)'], c)

    # ---- a move turns the unit toward its last step, an attack toward its target
    log, _ = play(IDLE + ',' + PICK + ',left,confirm,wait:30', 'moved')  # one step west: (1,2)
    f = lines(log, 'faced:')
    check('a step west turns the lancer west', f == ['faced: Lancer looks west'], f)
    to_back = IDLE + ',' + PICK + ',right,right,right,down,confirm,wait:30'  # (5,3): south of the dummy on (5,2), which looks north
    log, _ = play(to_back + ',confirm,wait:8', 'forecast_back')
    fc = lines(log, 'forecast:')
    check('the forecast names the arc of an attack on a back: "Backstab" and the damage with it', bool(fc) and 'Backstab' in fc[-1] and 'Dmg 13' in fc[-1], fc)
    log, _ = play(to_back + ',confirm,confirm,wait:60', 'attacked')
    f = lines(log, 'faced:')
    check('the attacker turns to its target (north) before it strikes', 'faced: Lancer looks north' in f, f)
    to_side = IDLE + ',' + PICK + ',right,right,confirm,wait:30'  # (4,2): west of the dummy: its side
    log, _ = play(to_side + ',confirm,wait:8', 'forecast_side')
    fc = lines(log, 'forecast:')
    check('a side attack says "Side" with the +2', bool(fc) and 'Side' in fc[-1] and 'Dmg 10' in fc[-1], fc)
    to_front = IDLE + ',' + PICK + ',right,right,right,up,confirm,wait:30'  # (5,1): north of the dummy, which looks north: its front
    log, _ = play(to_front + ',confirm,wait:8', 'forecast_front')
    fc = lines(log, 'forecast:')
    check('a front attack names no arc', bool(fc) and 'Dmg 8' in fc[-1] and 'Side' not in fc[-1] and 'Backstab' not in fc[-1], fc)

    # ---- the marker, in every kind of view: east and west differ in where the lancer's arrow is
    for style, label in STYLES:
        _, east = play(wait + ',confirm,wait:30', 'mark_east_' + style, style=style)
        _, west = play(wait + ',left,wait:30', 'mark_west_' + style, style=style)
        e, w = only_in(east, west), only_in(west, east)
        ce, cw = centroid(e), centroid(w)
        check('%s: the arrow follows the facing (east %d px, west %d px)' % (label, len(e), len(w)),
              len(e) >= 6 and len(w) >= 6 and ce[0] - cw[0] >= 3, (len(e), len(w), ce, cw))

    # ---- UI scale 100 and 150 with the arc line in the forecast
    for tag, scale in (('s100', None), ('s150', 150)):
        log, _ = play(to_back + ',confirm,wait:10', 'forecast_' + tag, scale=scale)
        check('the forecast with its arc line at UI scale %s: drawn, no UI bug' % (scale or 100), clean(log) and bool(lines(log, 'forecast:')), log[-300:])
        log, _ = play(wait, 'pick_' + tag, scale=scale)
        check('the pick\'s hint line at UI scale %s: no UI bug' % (scale or 100), clean(log), log[-300:])

    # ---- a game without facing: no pick, no marker
    make_game(facing=False)
    log, shot = play(stay + ',down,confirm,wait:30', 'off')
    check('without rules.facing Wait ends the turn at once: no pick, no facing event', not lines(log, 'facing pick:') and not lines(log, 'faced:'), lines(log, 'facing'))
    check('... and no marker is drawn', len(marker_pixels(shot)) == 0, len(marker_pixels(shot)))
    check('... nothing warned about', clean(log), log[-300:])

    # ---- hexagons: six directions, the highlight turns, Confirm commits
    make_game(hex_=True)
    log, _ = play(wait, 'hex_pick')
    p = lines(log, 'facing pick:')
    check('hexagons: the pick opens on the unit\'s own direction (east)', p == ['facing pick: Lancer looks east'], p)
    log, _ = play(wait + ',right,confirm,wait:30', 'hex_turn')
    c = lines(log, 'facing chosen:')
    check('hexagons: Right turns the highlight clockwise (southeast), Confirm commits it', c == ['facing chosen: Lancer looks southeast (was east)'], c)
    log, _ = play(wait + ',left,left,confirm,wait:30', 'hex_turn2')
    c = lines(log, 'facing chosen:')
    check('hexagons: Left twice goes round the other way (northwest)', c == ['facing chosen: Lancer looks northwest (was east)'], c)
    log, _ = play(wait + ',cancel,wait:30', 'hex_keep')
    check('hexagons: Cancel keeps the direction', lines(log, 'facing chosen:') == ['facing chosen: Lancer looks east (was east)'], lines(log, 'facing chosen:'))
    check('hexagons: nothing warned about', clean(log), log[-300:])
    summary()
    return 1 if __import__('ote_testkit').failures else 0


if __name__ == '__main__':
    sys.exit(main())
