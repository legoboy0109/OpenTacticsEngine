#!/usr/bin/env python3
"""The speed turn order and charged actions in the real player, windowless (docs/TURN_ORDER.md "Player").

  python3 tests/speed_order_player_test.py ./build/engine/player/ote-player . [--shots DIR]

Plays a copy of the synthetic game tests/fixtures/speed-order (not a demo; map "arena": blue mage (3,3) and knight (2,4), two red "stiff" units that
never get a turn on (7,3) and (8,3)). Mage and knight are 10 a tick, so at tick 10 the mage has the turn, then the knight; the mage's "Fire" (range
2-5, radius 1, 25 a tick) goes off 4 ticks after it was declared. Judged from the [info] lines the player logs, the Continue save and the pictures:

  - the turn strip: the active unit's tag has the white frame; a pending charged action is an orange-framed tag; it fits UI scale 100 and 150;
  - the cursor starts on the unit that has the turn; only that unit can be picked (the other one opens the map menu); the hint names End turn;
  - a charged weapon is aimed like any other and DECLARED: "charging: Mage starts Fire" and the mage's turn ends; the markers (teal on the caster,
    magenta on the blast) are painted in all four kinds of view (the picture differs from the one without a pending action);
  - the action goes off by itself ("charge goes off") and hurts what stands there; a unit that is charging has only Cancel Charge, Info and Wait;
  - a Continue save in the middle of a charge holds the clock and the pending action, and resuming it plays the same: the action still goes off;
  - the computer plays one unit's turn at a time (field map), without a warning.
Pictures: --shots DIR keeps them (BMP).
"""
import json
import os
import re
import shutil
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import Bmp, check, player_and_repo, run_text, scratch_dir, shots_dir, summary, ticks  # noqa: E402

sys.dont_write_bytecode = True
PLAYER, REPO = player_and_repo()
SHOTS = shots_dir(absolute=True)
TMP = scratch_dir()
GAMES = os.path.join(TMP, 'games')
IDLE = 'wait:20'  # the first turn banner plays first; it takes the first inputs otherwise
PICK = 'confirm'  # select the unit under the cursor (the mage: it has the turn)
STAY = 'confirm'  # "move here (own cell = stay)": the action menu opens
STYLES = [('classic', 'top-down'), ('isometric', 'isometric'), ('isometric-3d', '3D isometric'), ('full-3d', 'full 3D')]
ORANGE = (255, 170, 40)   # the frame of a pending action's tag
WHITE = (255, 255, 255)   # the frame of the active unit's tag
# Mage: select, stay, Attack, (the first aim square, 7,2: the blast covers the stiff unit on 7,3), declare.
DECLARE = IDLE + ',' + PICK + ',' + STAY + ',wait:10,confirm,wait:10,confirm,wait:40'


def make_game(tweak=None):
    shutil.rmtree(GAMES, ignore_errors=True)
    g = os.path.join(GAMES, 'sp')
    shutil.copytree(os.path.join(REPO, 'tests', 'fixtures', 'speed-order'), g)
    j = json.load(open(os.path.join(g, 'game.json')))
    j['styles'] = [{'id': 'classic', 'name': 'Classic'}, {'id': 'isometric', 'name': 'Isometric', 'view': 'isometric'},
                   {'id': 'isometric-3d', 'name': '3D Isometric', 'view': 'isometric-3d'}, {'id': 'full-3d', 'name': 'Full 3D', 'view': '3d'}]
    if tweak:
        tweak(j)
    json.dump(j, open(os.path.join(g, 'game.json'), 'w'))


def play(inp, name, style='classic', scale=None, saves=None, mapid='arena', cont=False, frames=None):
    shot = os.path.join(TMP, name + '.bmp')
    cmd = [PLAYER, '--games', GAMES, '--assets', os.path.join(REPO, 'assets'), '--headless', '--no-audio',
           '--frames', str(frames or ticks(inp) * 4 + 80), '--input-interval', '4', '--fixed-dt', '0.016',
           '--game', 'sp', '--style', style, '--3d', 'software', '--no-settings', '--input', inp, '--screenshot', shot]
    if cont:
        cmd += ['--continue']
    else:
        cmd += ['--map', mapid]
    if saves:
        cmd += ['--saves', saves]
    else:
        cmd += ['--no-saving']
    if scale:
        cmd += ['--ui-scale', str(scale)]
    log = run_text(cmd).stdout
    if SHOTS and os.path.exists(shot):
        os.makedirs(SHOTS, exist_ok=True)
        shutil.copy(shot, os.path.join(SHOTS, 'speed_' + name + '.bmp'))
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


def strip_box(b):
    """The strip: the top rows of the map part of the screen (the panel is on the right)."""
    return (0, 0, b.w * 3 // 4, 30)


def differs(a, b, y0=0):
    pa, pb = Bmp(a), Bmp(b)
    return sum(1 for y in range(y0, pa.h) for x in range(pa.w) if pa.px[y][x] != pb.px[y][x])


def unit_of(save, story_id):
    for u in save['battle']['units']:
        if u.get('id') == story_id:
            return u
    return None


def main():
    make_game()

    # ---- the turn starts on the unit that has it; the strip
    log, shot = play(IDLE + ',wait:6', 'start')
    turns = lines(log, 'unit turn:')
    check('the first turn is the mage\'s (a tie at tick 10 goes to the lower unit id)', bool(turns) and turns[0].startswith('unit turn: Mage, round 2, charge 100'), turns)
    check('the cursor-ready hint of a speed game names End turn', any('end turn' in l for l in lines(log, 'hint:')), lines(log, 'hint:'))
    b = Bmp(shot)
    check('the active unit\'s tag has the white frame in the strip', b.count(WHITE, 0, strip_box(b)) > 60, b.count(WHITE, 0, strip_box(b)))
    check('no pending action yet: no orange tag', b.count(ORANGE, 8, strip_box(b)) == 0, b.count(ORANGE, 8, strip_box(b)))
    check('start: nothing warned about', clean(log), log[-300:])

    # ---- only the active unit can be picked; the other one is inspected through the map menu
    log, _ = play(IDLE + ',down,left,confirm,wait:6', 'other_unit')  # the cursor goes from the mage (3,3) to the knight (2,4)
    check('picking the knight (it has no turn) selects nothing: the hint stays the map\'s', not any('move here' in l for l in lines(log, 'hint:')), lines(log, 'hint:'))
    check('...and opens the map menu instead (Unit entry: inspect it)', bool(lines(log, 'battle menu:')) and 'Unit' in lines(log, 'battle menu:')[-1], lines(log, 'battle menu:'))
    log, _ = play(IDLE + ',' + PICK + ',wait:6', 'pick_mage')
    check('picking the mage (it has the turn) shows its reach', any('move here' in l for l in lines(log, 'hint:')), lines(log, 'hint:'))

    # ---- a charged weapon is declared
    log, shot = play(DECLARE, 'declared')
    check('the Attack entry is offered', any(l.startswith('menu entries: Attack') for l in lines(log, 'menu entries:')), lines(log, 'menu entries:'))
    check('the aim shows the usual area forecast', bool(lines(log, 'area forecast:')), lines(log, 'area forecast:'))
    check('declaring: "charging: Mage starts Fire at 7,2 (speed 25)"', lines(log, 'charging:') == ['charging: Mage starts Fire at 7,2 (speed 25)'], lines(log, 'charging:'))
    check('the mage\'s turn ended: the knight has the next turn', [l for l in lines(log, 'unit turn:') if l.startswith('unit turn: Knight')], lines(log, 'unit turn:'))
    b = Bmp(shot)
    check('the pending action is an orange-framed tag in the strip', b.count(ORANGE, 8, strip_box(b)) > 30, b.count(ORANGE, 8, strip_box(b)))
    check('declaring: nothing warned about', clean(log), log[-300:])

    # ---- the pending action goes off by itself and hurts what stands in the blast
    log, _ = play(DECLARE + ',menu,confirm,wait:60', 'resolved')
    check('the knight ends its turn: the fire goes off', lines(log, 'charge goes off:') == ["charge goes off: Mage's Fire at 7,2"], lines(log, 'charge goes off:'))
    check('resolving: nothing warned about', clean(log), log[-300:])

    # ---- a charging unit may only move, wait or give the charge up
    wpath = os.path.join(GAMES, 'sp', 'data', 'weapons.json')
    text = open(wpath).read().replace('"chargeSpeed": 25', '"chargeSpeed": 5')
    open(wpath, 'w').write(text)
    # Mage declares (20 ticks); the knight ends its turn twice (it is ready again after 6 ticks, the mage after 8); then the mage has the turn and picks itself.
    again = DECLARE + ',menu,confirm,wait:60,menu,confirm,wait:60,confirm,confirm,wait:10'
    log, _ = play(again, 'charging_menu')
    menus = lines(log, 'menu entries:')
    check('a charging unit\'s menu has Cancel Charge, Info and Wait only', bool(menus) and menus[-1].startswith('menu entries: Info, Cancel Charge, Wait'), menus)
    log, _ = play(again + ',down,confirm,wait:10', 'cancelled')
    check('Cancel Charge gives the charge up ("charge cancelled")', any('Charge given up' in l for l in lines(log, 'charge cancelled:')), lines(log, 'charge cancelled:'))
    menus = lines(log, 'menu entries:')
    check('and the unit may act after all (the Attack entry is back)', bool(menus) and menus[-1].startswith('menu entries: Attack'), menus)
    check('giving it up: nothing warned about', clean(log), log[-300:])
    make_game()

    # ---- a save in the middle of a charge, and resuming it
    sdir = os.path.join(TMP, 'saves_charge')
    shutil.rmtree(sdir, ignore_errors=True)
    log, _ = play(DECLARE + ',wait:20', 'charge_saved', saves=sdir)
    cpath = os.path.join(sdir, 'sp', 'continue.json')
    save = json.load(open(cpath)) if os.path.exists(cpath) else None
    clock = save['battle'].get('clock') if save else None
    check('the Continue save holds the clock', clock is not None and clock.get('activeUnit') == 1 and len(clock.get('pending', [])) == 1, clock)
    p = clock['pending'][0] if clock and clock.get('pending') else {}
    check('the pending action is the mage\'s Fire, at the start of its count', (p.get('kind'), p.get('id'), p.get('counter')) == ('weapon', 'fire', 0), p)
    check('the units keep their charge', unit_of(save, 'mg') is not None and 'ct' in unit_of(save, 'mg'), unit_of(save, 'mg') if save else None)
    log, _ = play('wait:20,menu,confirm,wait:60', 'charge_resumed', saves=sdir, cont=True)
    check('resumed: the knight has the turn again', [l for l in lines(log, 'unit turn:') if l.startswith('unit turn: Knight')], lines(log, 'unit turn:'))
    check('resumed: the fire still goes off', lines(log, 'charge goes off:') == ["charge goes off: Mage's Fire at 7,2"], lines(log, 'charge goes off:'))
    check('resuming: nothing warned about', clean(log), log[-300:])
    log, _ = play('wait:20,menu,confirm,wait:60', 'charge_resumed2', saves=sdir, cont=True)  # (the save was updated by the run above: it has gone off by now)
    save = json.load(open(cpath)) if os.path.exists(cpath) else None
    foe = unit_of(save, 'foe') if save else None
    check('after the fire the stiff unit in the blast is hurt (30 - 9 = 21)', foe is not None and foe['hp'] == 21, foe)
    other = unit_of(save, 'foe2') if save else None
    check('and the one outside it is not', other is not None and other['hp'] == 30, other)

    # ---- the computer plays one unit's turn at a time
    log, _ = play(IDLE + ',menu,confirm,wait:30,menu,confirm,wait:30,menu,confirm,wait:400', 'ai', mapid='field')
    check('the red units took turns of their own', len([l for l in lines(log, 'unit turn:') if 'Dummy' in l]) >= 2, lines(log, 'unit turn:'))
    check('the computer\'s turns: nothing warned about', clean(log), log[-400:])

    # ---- the pictures: UI scale 100 and 150, all four views
    for tag, scale in (('s100', None), ('s150', 150)):
        log, shot = play(DECLARE, 'ui_' + tag, scale=scale)
        b = Bmp(shot)
        check('UI scale %s: the strip is drawn (white and orange frames) and nothing is warned about' % (scale or 100),
              b.count(WHITE, 0, strip_box(b)) > 40 and b.count(ORANGE, 8, strip_box(b)) > 20 and clean(log),
              (b.count(WHITE, 0, strip_box(b)), b.count(ORANGE, 8, strip_box(b))))
        log, shot = play(IDLE + ',' + PICK + ',' + STAY + ',wait:10,confirm,wait:10', 'aim_' + tag, scale=scale)
        b = Bmp(shot)
        check('UI scale %s: while aiming the charged weapon a dashed tag shows where it would land' % (scale or 100),
              b.count(ORANGE, 8, strip_box(b)) > 20 and clean(log), b.count(ORANGE, 8, strip_box(b)))
    for style, label in STYLES:
        _, with_p = play(DECLARE, 'pending_' + style, style=style)
        _, without = play(IDLE + ',' + PICK + ',' + STAY + ',wait:10,down,down,confirm,wait:40', 'none_' + style, style=style)  # Wait instead: no pending action
        check('%s: the charge markers are painted (the picture differs from the one without a pending action)' % label, differs(with_p, without, 24) > 300, differs(with_p, without, 24))
    summary()


if __name__ == '__main__':
    main()
