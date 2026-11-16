#!/usr/bin/env python3
"""The player's part of docs/SRPG.md step U3 "abilities and summons", windowless.

  python3 tests/srpg_player_abilities_test.py ./build/engine/player/ote-player .
  python3 tests/srpg_player_abilities_test.py ./build/engine/player/ote-player . --shots OUT_DIR   # also keeps the screenshots (BMP)

Writes a small game in a temp folder (square, pointy and flat hexagon maps; abilities push, pull, swap, moveBehind, heal,
a free summon with a limit and a lifetime, a free heal with a cooldown), runs the player headless with --input scripts and
--saves on a temp folder, and judges the [info] lines it logs, the Continue save (where the units ended up) and pixels:

  - the action menu lists a unit's abilities after Attack / Talk and before Wait; ones that cannot be used stay in the menu
    greyed with the core's plain reason ("ready in 2 turns", "can only be used before moving", ...);
  - the targeting mode marks the squares the units would end on (push, pull, swap, moveBehind) and the new unit's square
    (summon) in red; choosing carries the ability out and the units stand where the preview said (all three grids);
  - a free ability leaves the unit its turn (the menu opens again), an action ability ends it;
  - a push stopped by a hidden unit in fog logs "(blocked)" and moves nothing past the visible squares;
  - the same menu fits the phone-sized screen (--ui-scale 200).
The Abilities page of Unit details is reached through U1's Info entry, so it is covered by tests/test_srpg_player_abilities.cpp
(with a fake renderer) until that entry exists.
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import hex_player_test as hp  # noqa: E402  (its image reader and checker)

check, near, Image = hp.check, hp.near, hp.Image
PLAYER = REPO = GAMES = TMP = SHOTS = ''
W, H, TILE = 480, 270, 24
MAP_W, MAP_H = 9, 7
VIEW_W, VIEW_H = 352, 259          # the map viewport: the window minus the panel and the hint strip
PLAIN, FOREST = (0x9c, 0xcc, 0x65), (0x2e, 0x7d, 0x32)
IDLE = 'wait:14'                   # the turn banner plays first
SELECT = 'confirm,confirm'         # select the unit under the cursor, stay where it is: its action menu opens


def write_json(path, obj):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    json.dump(obj, open(path, 'w'), indent=1)


ABILITIES = {'abilities': [
    {'id': 'shove', 'name': 'Shove', 'target': 'unit', 'range': [1, 1], 'effect': {'push': 2}, 'cost': 'action'},
    {'id': 'call_hound', 'name': 'Call Hound', 'target': 'cell', 'range': [1, 2],
     'effect': {'summon': {'type': 'hound', 'level': 2, 'limit': 1, 'vanish': True, 'lifetime': 0, 'canAct': False}},
     'cost': 'free'},
    {'id': 'call_wisp', 'name': 'Call Wisp', 'target': 'cell', 'range': [1, 1], 'cost': 'free', 'cooldown': 2,
     'effect': {'summon': {'type': 'wisp', 'level': 1, 'limit': 1, 'vanish': True, 'lifetime': 2, 'canAct': True}}},
    {'id': 'flank', 'name': 'Flank', 'target': 'enemy', 'effect': {'moveBehind': True}},
    {'id': 'yank', 'name': 'Yank', 'target': 'enemy', 'effect': {'pull': True}},
    {'id': 'mend', 'name': 'Mend', 'target': 'ally', 'range': [1, 2], 'effect': {'heal': 10}},
    {'id': 'rest', 'name': 'Rest', 'target': 'self', 'effect': {'heal': 'full'}, 'cost': 'free'},
    {'id': 'trade', 'name': 'Trade Places', 'target': 'ally', 'effect': {'swap': True}, 'afterMove': False}]}


def write_game(out, kind):
    """kind: square, pointy or flat. Maps are written by scenario() below."""
    g = os.path.join(out, kind)
    shutil.rmtree(g, ignore_errors=True)
    game = {'formatVersion': 1, 'name': 'Abilities Player Test ' + kind,
            'view': {'width': W, 'height': H, 'tileSize': TILE},
            'stats': [{'id': 'hp', 'name': 'HP'}, {'id': 'atk'}, {'id': 'def'}, {'id': 'mov', 'name': 'Move'}],
            'roles': {'hp': 'hp', 'move': 'mov'}, 'moveTypes': [{'id': 'foot'}],
            'rules': {'turnOrder': 'sides', 'counterAttacks': True, 'defeat': ['noUnits'], 'items': {'weapons': 'fixed'}},
            'formulas': {'damage': 'a.atk - b.def'}, 'maps': [{'id': n, 'name': n, 'file': 'maps/%s.json' % n} for n in MAPS]}
    if kind != 'square':
        game['grid'] = {'type': 'hex', 'layout': kind}
    write_json(g + '/game.json', game)
    write_json(g + '/data/terrain.json', {'terrain': [
        {'id': 'plain', 'name': 'Plain', 'color': '#9ccc65'},
        {'id': 'forest', 'name': 'Forest', 'color': '#2e7d32', 'hides': True}]})
    stats = lambda hp_, mv: {'hp': hp_, 'atk': 5, 'def': 1, 'mov': mv}
    write_json(g + '/data/units.json', {'units': [
        {'id': 'hero', 'name': 'Hero', 'moveType': 'foot', 'stats': stats(20, 4), 'weapons': ['sword'], 'vision': 1,
         'abilities': ['shove', 'call_hound', 'call_wisp', 'flank', 'yank']},
        {'id': 'archer', 'name': 'Archer', 'moveType': 'foot', 'stats': stats(14, 3), 'weapons': ['sword'], 'vision': 1,
         'abilities': ['mend', 'rest', 'trade']},
        {'id': 'grunt', 'name': 'Grunt', 'moveType': 'foot', 'stats': stats(12, 3), 'weapons': ['sword'], 'vision': 1},
        {'id': 'hound', 'name': 'Hound', 'moveType': 'foot', 'stats': stats(6, 3), 'weapons': ['sword'], 'vision': 1},
        {'id': 'wisp', 'name': 'Wisp', 'moveType': 'foot', 'stats': stats(4, 4), 'weapons': ['sword'], 'vision': 1}]})
    write_json(g + '/data/weapons.json', {'weapons': [{'id': 'sword', 'name': 'Sword'}]})
    write_json(g + '/data/abilities.json', ABILITIES)
    for name, spec in MAPS.items():
        write_json(g + '/maps/%s.json' % name, make_map(kind, *spec))
    return g


def line(kind, k, o=(2, 2)):
    """The square k steps from o along the straight line the tests use: east on squares and pointy hexagons, south on flat
    ones (the neighbours that are straight in that layout)."""
    return (o[0], o[1] + k) if kind == 'flat' else (o[0] + k, o[1])


def make_map(kind, units, fog=False, tiles=()):
    rows = [['.'] * MAP_W for _ in range(MAP_H)]
    for (x, y) in tiles:
        rows[y][x] = 'f'
    grid = {'type': 'square' if kind == 'square' else 'hex', 'width': MAP_W, 'height': MAP_H}
    m = {'name': 'Map', 'grid': grid, 'legend': {'.': 'plain', 'f': 'forest'}, 'tiles': [''.join(r) for r in rows],
         'sides': [{'id': 'blue', 'name': 'Blue Army', 'color': '#3d85c6', 'controller': 'human'},
                   {'id': 'red', 'name': 'Red Army', 'color': '#cc0000', 'controller': 'ai'}],
         'units': [{'type': t, 'side': s, 'x': x, 'y': y} for (t, s, x, y) in units]}
    if fog:
        m['fog'] = True
    return m


# map id -> (kind) -> spec; the units are (type, side, x, y); `K` is the straight line of the layout
def scenarios(kind):
    k = lambda n, o=(2, 2): line(kind, n, o)
    far = (7, 6) if kind != 'flat' else (6, 0)
    return {
        'push': ([('hero', 'blue', *k(0)), ('grunt', 'red', *k(1)), ('grunt', 'red', *far)], False, ()),
        'pull': ([('hero', 'blue', *k(0)), ('grunt', 'red', *k(1)), ('grunt', 'red', *far)], False, ()),
        'swap': ([('hero', 'blue', *k(0)), ('archer', 'blue', *k(1)), ('grunt', 'red', *far)], False, ()),
        'flank': ([('hero', 'blue', *k(1)), ('grunt', 'red', *k(2)), ('grunt', 'red', *far)], False, ()),
        'summon': ([('hero', 'blue', *k(0)), ('grunt', 'red', *far)], False, ()),
        'reasons': ([('archer', 'blue', 2, 2), ('hero', 'blue', 0, 6), ('grunt', 'red', 8, 0)], False, ()),
        # the hidden grunt stands in the forest right behind the one the hero shoves
        'fogpush': ([('hero', 'blue', *k(0)), ('grunt', 'red', *k(1)), ('grunt', 'red', *k(2))], True, (k(2),)),
    }


MAPS = {}


def play(game, map_id, inp, shot=None, saves=None, extra=(), tail=60):
    frames = sum(int(t[5:]) * 4 if t.startswith('wait:') else 4 for t in inp.split(',')) + tail
    cmd = [PLAYER, '--games', GAMES, '--assets', os.path.join(REPO, 'assets'), '--headless', '--no-audio', '--no-settings',
           '--frames', str(frames), '--input-interval', '4', '--fixed-dt', '0.016', '--game', game, '--map', map_id,
           '--input', inp]
    cmd += ['--saves', saves] if saves else ['--no-saving']
    if shot:
        cmd += ['--screenshot', shot]
    p = subprocess.run(cmd + list(extra), stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True, timeout=300)
    return p.returncode, p.stdout


def run(name, game, map_id, inp, saves=False, extra=(), tail=60):
    """Returns (image, log, save-or-None) of one script."""
    path = os.path.join(TMP, name + '.bmp')
    sdir = os.path.join(TMP, 'saves_' + name) if saves else None
    if sdir:
        shutil.rmtree(sdir, ignore_errors=True)
    _, log = play(game, map_id, inp, shot=path, saves=sdir, extra=extra, tail=tail)
    if SHOTS:
        shutil.copy(path, os.path.join(SHOTS, name + '.bmp'))
    save = None
    if sdir and os.path.exists(os.path.join(sdir, game, 'continue.json')):
        save = json.load(open(os.path.join(sdir, game, 'continue.json')))
    return Image(path), log, save


def cells_of(save):
    """{unit id: (x, y)} of the living units of a Continue save."""
    return {i: (u['cell'] % MAP_W, u['cell'] // MAP_W) for i, u in enumerate(save['battle']['units']) if u.get('alive', True)}


def screen_box(x, y, kind):
    """Top-left of a square map's tile on the screen (the map is smaller than the viewport and centred)."""
    import math
    mw, mh = MAP_W * TILE, MAP_H * TILE
    return int(math.floor((VIEW_W - mw) / 2.0 + 0.5)) + x * TILE, int(math.floor((VIEW_H - mh) / 2.0 + 0.5)) + y * TILE


def redness(img, x, y):
    """How red the middle of a square map's tile is (the end squares are tinted red): red minus green and blue."""
    bx, by = screen_box(x, y, 'square')
    p = img.px(bx + 3, by + 3)  # a corner: no unit block covers it, the tint does
    return p[0] - (p[1] + p[2]) // 2


def listed(log, *names):
    return all(('ability chosen: ' + n) in log for n in names)


def square_checks():
    kind = 'square'
    write_game(GAMES, kind)
    # ---- push: the menu, the preview, the result
    img, log, _ = run('sq_menu', kind, 'push', IDLE + ',' + SELECT)
    check('square: no warnings or UI bugs in the menu run', 'UI bug' not in log and '[warn]' not in log, log[-300:])
    img, log, save = run('sq_push_pick', kind, 'push', IDLE + ',' + SELECT + ',down,confirm,wait:2')
    check('square: choosing Shove opens a unit pick', 'pick unit: 1 candidates' in log and listed(log, 'Shove'), log[-300:])
    end = line(kind, 3)
    plain = redness(img, 7, 4)
    check('square: the pick marks the end square of the push red', redness(img, *end) > plain + 40,
          (redness(img, *end), plain))
    img, log, save = run('sq_push_done', kind, 'push', IDLE + ',' + SELECT + ',down,confirm,confirm,wait:25', saves=True)
    check('square: the use is logged', 'ability: Hero uses Shove on unit 1' in log and '(blocked)' not in log, log[-300:])
    pos = cells_of(save) if save else {}
    check('square: Shove pushed the grunt two squares away', pos.get(1) == line(kind, 3) and pos.get(0) == line(kind, 0), pos)
    # ---- pull (an adjacent enemy: the hero steps back, the enemy takes its square)
    img, log, _ = run('sq_pull_pick', kind, 'pull', IDLE + ',' + SELECT + ',down,down,down,down,down,confirm,wait:2')
    check('square: Yank is the sixth entry (Attack first)', listed(log, 'Yank'), log[-300:])
    check('square: pull marks the square the hero steps back to', redness(img, 1, 2) > plain + 40,
          (redness(img, 1, 2), plain))
    img, log, save = run('sq_pull_done', kind, 'pull', IDLE + ',' + SELECT + ',down,down,down,down,down,confirm,confirm,wait:25', saves=True)
    pos = cells_of(save) if save else {}
    check('square: Yank pulled the grunt next to the hero, who stepped back', pos.get(1) == (2, 2) and pos.get(0) == (1, 2), pos)
    # ---- swap (the archer's Trade Places: Mend and Rest are greyed, nothing to heal)
    img, log, save = run('sq_swap_done', kind, 'swap', IDLE + ',next,' + SELECT + ',confirm,confirm,wait:25', saves=True)
    pos = cells_of(save) if save else {}
    check('square: Trade Places swapped the two blue units', listed(log, 'Trade Places') and pos.get(0) == (3, 2) and pos.get(1) == (2, 2), (log[-200:], pos))
    # ---- moveBehind
    img, log, save = run('sq_flank_done', kind, 'flank', IDLE + ',' + SELECT + ',down,down,down,down,confirm,confirm,wait:25', saves=True)
    pos = cells_of(save) if save else {}
    check('square: Flank put the grunt behind the hero', listed(log, 'Flank') and pos.get(1) == (2, 2), (log[-200:], pos))
    # ---- summon: Call Wisp is free; the menu opens again with the limit as the reason
    img, log, _ = run('sq_summon_pick', kind, 'summon', IDLE + ',' + SELECT + ',down,confirm,wait:2')
    check('square: Call Wisp opens a square pick', 'pick square:' in log and listed(log, 'Call Wisp'), log[-300:])
    reds = [(x, y) for x in range(MAP_W) for y in range(MAP_H) if redness(img, x, y) > plain + 40]
    check('square: the pick marks exactly one square red (where the new unit would stand)', len(reds) == 1 and reds[0] != (2, 2), reds)
    img, log, save = run('sq_summon_done', kind, 'summon', IDLE + ',' + SELECT + ',down,confirm,confirm,wait:25', saves=True)
    check('square: the summon is logged and the unit exists', 'ability: Hero uses Call Wisp' in log and save is not None and
          len(save['battle']['units']) == 3, log[-300:])
    # ---- reasons, 1: the archer moved, so Trade Places is greyed (Mend and Rest have nobody to heal)
    img, log, _ = run('sq_reasons_moved', kind, 'reasons', IDLE + ',confirm,right,confirm,wait:8')
    check('square: after moving, the menu is open again', 'UI bug' not in log and 'battle menu' not in log, log[-200:])
    # ---- reasons, 2: two free summons: the menu opens again after each; then both are greyed with their reasons
    inp = IDLE + ',confirm,confirm,confirm,confirm,wait:16,confirm,confirm,wait:16'
    img, log, _ = run('sq_reasons', kind, 'summon', inp)
    check('square: both free summons were used in one turn',
          'ability: Hero uses Call Hound' in log and 'ability: Hero uses Call Wisp' in log and 'UI bug' not in log, log[-400:])
    for scale, tag in (('200', 'phone'),):
        img, log, _ = run('sq_%s_reasons' % tag, kind, 'summon', inp, extra=('--ui-scale', scale))
        check('square: the phone-sized menu runs without UI bugs', 'UI bug' not in log and 'ui scale %s%%' % scale in log, log[-200:])
        img, log, _ = run('sq_%s_pick' % tag, kind, 'push', IDLE + ',' + SELECT + ',down,confirm,wait:2', extra=('--ui-scale', scale))
        check('square: the phone-sized pick runs', 'pick unit' in log, log[-200:])
    # ---- fog: a hidden unit behind the target blocks the push
    img, log, save = run('sq_fog_blocked', kind, 'fogpush', IDLE + ',' + SELECT + ',down,confirm,confirm,wait:30', saves=True)
    pos = cells_of(save) if save else {}
    check('square fog: the push is blocked by the hidden unit', 'ability: Hero uses Shove on unit 1' in log and '(blocked)' in log, log[-300:])
    check('square fog: the shoved unit did not move through it', pos.get(1) == line(kind, 1) and pos.get(2) == line(kind, 2), pos)


def hex_checks(kind):
    write_game(GAMES, kind)
    img, log, save = run('%s_push_done' % kind, kind, 'push', IDLE + ',' + SELECT + ',down,confirm,confirm,wait:25', saves=True)
    pos = cells_of(save) if save else {}
    check('%s: Shove pushed the grunt two squares along the line' % kind, 'ability: Hero uses Shove' in log and pos.get(1) == line(kind, 3), (log[-200:], pos))
    run('%s_push_pick' % kind, kind, 'push', IDLE + ',' + SELECT + ',down,confirm,wait:2')
    img, log, save = run('%s_pull_done' % kind, kind, 'pull', IDLE + ',' + SELECT + ',down,down,down,down,down,confirm,confirm,wait:25', saves=True)
    pos = cells_of(save) if save else {}
    check('%s: Yank pulled the grunt next to the hero' % kind, pos.get(1) == line(kind, 0) and pos.get(0) == line(kind, -1), pos)
    img, log, save = run('%s_swap_done' % kind, kind, 'swap', IDLE + ',next,' + SELECT + ',confirm,confirm,wait:25', saves=True)
    pos = cells_of(save) if save else {}
    check('%s: Trade Places swapped the two blue units' % kind, pos.get(0) == line(kind, 1) and pos.get(1) == line(kind, 0), pos)
    img, log, save = run('%s_flank_done' % kind, kind, 'flank', IDLE + ',' + SELECT + ',down,down,down,down,confirm,confirm,wait:25', saves=True)
    pos = cells_of(save) if save else {}
    check('%s: Flank put the grunt behind the hero' % kind, pos.get(1) == line(kind, 0), pos)
    run('%s_summon_pick' % kind, kind, 'summon', IDLE + ',' + SELECT + ',down,confirm,wait:2')
    img, log, save = run('%s_summon_done' % kind, kind, 'summon', IDLE + ',' + SELECT + ',down,confirm,confirm,wait:25', saves=True)
    check('%s: Call Wisp added a unit' % kind, save is not None and len(save['battle']['units']) == 3, log[-200:])
    img, log, save = run('%s_fog_blocked' % kind, kind, 'fogpush', IDLE + ',' + SELECT + ',down,confirm,confirm,wait:30', saves=True)
    check('%s fog: the push is blocked by the hidden unit' % kind, '(blocked)' in log, log[-300:])


def main():
    global PLAYER, REPO, GAMES, SHOTS, TMP, MAPS
    args = sys.argv[1:]
    if len(args) < 2:
        print(__doc__)
        return 2
    PLAYER, REPO = os.path.abspath(args[0]), os.path.abspath(args[1])
    if '--shots' in args:
        SHOTS = os.path.abspath(args[args.index('--shots') + 1])
        os.makedirs(SHOTS, exist_ok=True)
    TMP = tempfile.mkdtemp(prefix='ote_abilities_test_')
    GAMES = os.path.join(TMP, 'games')
    try:
        for kind in ('square', 'pointy', 'flat'):
            MAPS = scenarios(kind)
            (square_checks if kind == 'square' else (lambda: hex_checks(kind)))()
    finally:
        shutil.rmtree(TMP, ignore_errors=True)
    print('\n%d passed, %d failed' % (hp.passed, len(hp.failures)))
    return 1 if hp.failures else 0


if __name__ == '__main__':
    sys.exit(main())
