#!/usr/bin/env python3
"""The player's enemy-range display ("danger zone") through the real player, windowless (docs/SRPG.md "Enemy ranges").

  python3 tests/enemy_ranges_player_test.py ./build/engine/player/ote-player . [--shots DIR]

Writes a small game in a temp folder (open 10 x 6 map, an infantry with a rifle, a scout, an archer with a 2-square bow) and runs the
player with --input scripts, judging the [info] lines it logs and the screenshots:

  - Confirm on an enemy marks it; the log line lists how many squares it can move to and, around them, hit. The numbers are worked out
    here, square by square, from the unit's move and range (open ground: a diamond), not copied from the player.
  - Confirm again unmarks; two marks combine; the Ranges button shows every enemy and hides them again; the map menu has the entry.
  - The picture differs from the unmarked one in every view (top-down, isometric, 3D isometric, full 3D, hexagon).
  - Options: "Off" and a game that switched the display off leave Confirm opening the map menu as before; "Attack only" draws one tint.
  - Fog: an enemy that is not in sight cannot be marked and is not in "all".
  - Marks survive the player's next turn within the battle and are not in the Continue save.
Pictures: --shots DIR keeps them (BMP).
"""
import json
import os
import re
import shutil
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import Bmp, check, failures, player_and_repo, run_text, scratch_dir, shots_dir, summary, ticks, write_json  # noqa: E402

sys.dont_write_bytecode = True
PLAYER, REPO = player_and_repo()
SHOTS = shots_dir(absolute=True)
TMP = scratch_dir()
GAMES = os.path.join(TMP, 'games')
W, H, TILE, MAP_W, MAP_H = 480, 270, 24, 10, 6
IDLE = 'wait:14'  # the turn banner plays first; it takes the first inputs otherwise

STYLES = [('classic', 'top-down'), ('isometric', 'isometric'), ('isometric-3d', '3D isometric'), ('full-3d', 'full 3D')]


def unit(type_, side, x, y, **kw):
    u = {'type': type_, 'side': side, 'x': x, 'y': y}
    u.update(kw)
    return u


def make_game(gid, hex_grid=False, rule=None):
    g = os.path.join(GAMES, gid)
    shutil.rmtree(g, ignore_errors=True)
    grid = {'type': 'hex', 'layout': 'pointy'} if hex_grid else {'type': 'square'}
    rules = {'turnOrder': 'sides', 'counterAttacks': True, 'defeat': ['noUnits']}
    if rule is not None:
        rules['display'] = {'enemyRanges': rule}
    game = {'formatVersion': 1, 'name': 'Enemy Ranges ' + gid, 'view': {'width': W, 'height': H, 'tileSize': TILE},
            'styles': [{'id': 'classic', 'name': 'Classic'}, {'id': 'isometric', 'name': 'Isometric', 'view': 'isometric'},
                       {'id': 'isometric-3d', 'name': '3D Isometric', 'view': 'isometric-3d'},
                       {'id': 'full-3d', 'name': 'Full 3D', 'view': '3d'}],
            'stats': [{'id': 'hp', 'name': 'HP'}, {'id': 'mov', 'name': 'Move'}], 'roles': {'hp': 'hp', 'move': 'mov'},
            'moveTypes': [{'id': 'foot', 'name': 'Foot'}], 'rules': rules, 'formulas': {'damage': '5'},
            'maps': [{'id': m, 'name': m, 'file': 'maps/%s.json' % m} for m in ('open', 'fog')]}
    if hex_grid:
        game['grid'] = grid
    write_json(g + '/game.json', game)
    write_json(g + '/data/terrain.json', {'terrain': [
        {'id': 'plain', 'name': 'Plain', 'color': '#9ccc65', 'glyph': '.'},
        {'id': 'forest', 'name': 'Forest', 'color': '#2e7d32', 'glyph': 'f', 'hides': True, 'moveCost': {'foot': 1}}]})
    write_json(g + '/data/units.json', {'units': [
        {'id': 'infantry', 'name': 'Infantry', 'moveType': 'foot', 'stats': {'hp': 40, 'mov': 3}, 'weapons': ['rifle'], 'vision': 2},
        {'id': 'scout', 'name': 'Scout', 'moveType': 'foot', 'stats': {'hp': 40, 'mov': 4}, 'weapons': ['rifle'], 'vision': 2},
        {'id': 'archer', 'name': 'Archer', 'moveType': 'foot', 'stats': {'hp': 40, 'mov': 3}, 'weapons': ['bow'], 'vision': 2}]})
    write_json(g + '/data/weapons.json', {'weapons': [
        {'id': 'rifle', 'name': 'Rifle', 'versus': {'infantry': 1, 'scout': 1, 'archer': 1}},
        {'id': 'bow', 'name': 'Bow', 'range': [2, 2], 'versus': {'infantry': 1, 'scout': 1, 'archer': 1}}]})
    sides = [{'id': 'blue', 'name': 'Blue Army', 'color': '#3d85c6', 'controller': 'human'},
             {'id': 'red', 'name': 'Red Army', 'color': '#cc0000', 'controller': 'ai'}]
    rows = ['.' * MAP_W] * MAP_H
    base = {'grid': dict(grid, width=MAP_W, height=MAP_H), 'legend': {'.': 'plain', 'f': 'forest'}, 'tiles': rows, 'sides': sides}
    # open: blue infantry (1,3) is the first unit; red infantry (6,3), scout (8,0), archer (8,5), nobody within reach of blue.
    write_json(g + '/maps/open.json', dict(base, name='Open', units=[
        unit('infantry', 'blue', 1, 3), unit('infantry', 'red', 6, 3), unit('scout', 'red', 8, 0), unit('archer', 'red', 8, 5)]))
    # fog: blue infantry (1,3) sees 2 squares; red infantry (3,3) is in sight, red scout (6,3) is not.
    write_json(g + '/maps/fog.json', dict(base, name='Fog', fog=True, units=[
        unit('infantry', 'blue', 1, 3), unit('infantry', 'red', 3, 3), unit('scout', 'red', 6, 3)]))
    return g


def play(game, inp, map_id='open', shot=None, saves=None, settings=None, style='classic', extra=(), interval=4, tail=80):
    cmd = [PLAYER, '--games', GAMES, '--assets', os.path.join(REPO, 'assets'), '--headless', '--no-audio',
           '--frames', str(ticks(inp) * interval + tail), '--input-interval', str(interval), '--fixed-dt', '0.016',
           '--game', game, '--map', map_id, '--style', style, '--3d', 'software']
    cmd += ['--input', inp] if inp else []
    cmd += ['--settings', settings] if settings else ['--no-settings']
    cmd += ['--saves', saves] if saves else ['--no-saving']
    if shot:
        cmd += ['--screenshot', shot]
    return run_text(cmd + list(extra)).stdout


def info(log, prefix):
    out = []
    for line in log.split('\n'):
        m = re.search(r'\[info\]\s*(.*)$', line)
        if m and m.group(1).startswith(prefix):
            out.append(m.group(1))
    return out


def counts(log):
    """The (units, move squares, attack squares) of every "enemy ranges:" line, in order."""
    return [tuple(int(v) for v in re.findall(r'\d+', l)) for l in info(log, 'enemy ranges: ')
            if re.match(r'enemy ranges: \d+ units', l)]


def keep(name, path):
    if SHOTS and os.path.exists(path):
        os.makedirs(SHOTS, exist_ok=True)
        shutil.copy(path, os.path.join(SHOTS, name + '.bmp'))


# ---- the expected numbers, square by square (open ground, one unit blocks nothing it cannot pass through)

def expected(units, blue):
    """Union over `units` ((x, y, move, minRange, maxRange) each) of (move squares, attack-only squares) on the open map. A unit
    stops on any square within `move` steps that no unit stands on (its own square included); the blue units block the way."""
    occupied = set((u[0], u[1]) for u in units) | set(blue)
    move, attack = set(), set()
    for (x, y, mov, lo, hi) in units:
        stops = set()
        for sx in range(MAP_W):
            for sy in range(MAP_H):
                if abs(sx - x) + abs(sy - y) <= mov and ((sx, sy) not in occupied or (sx, sy) == (x, y)):
                    stops.add((sx, sy))
        move |= stops
        for (sx, sy) in stops:
            for cx in range(MAP_W):
                for cy in range(MAP_H):
                    if lo <= abs(cx - sx) + abs(cy - sy) <= hi:
                        attack.add((cx, cy))
    return len(move), len(attack - move)


INF, SCOUT, ARCHER = (6, 3, 3, 1, 1), (8, 0, 4, 1, 1), (8, 5, 3, 2, 2)


def differs(a, b):
    """How many pixels of two screenshots differ."""
    return sum(1 for ra, rb in zip(Bmp(a).px, Bmp(b).px) for pa, pb in zip(ra, rb) if pa != pb)


def main():
    make_game('er')
    make_game('er_hex', hex_grid=True)
    make_game('er_off', rule=False)
    to_inf = ','.join(['right'] * 5)  # the cursor starts on blue's infantry (1,3); the red infantry stands at (6,3)

    # ---- marking
    log = play('er', IDLE + ',' + to_inf + ',confirm,wait:3', shot=os.path.join(TMP, 'one.bmp'))
    keep('er_marked_classic', os.path.join(TMP, 'one.bmp'))
    c = counts(log)
    m, a = expected([INF], [(1, 3)])
    check('marking the red infantry lists its reach: %d move, %d attack squares' % (m, a), c and c[-1] == (1, m, a), c)
    check('the mark is logged', 'enemy mark: Infantry on' in ' '.join(info(log, 'enemy mark')), info(log, 'enemy mark'))
    check('no UI bug or warning', 'UI bug' not in log and '[warn]' not in log and '[error]' not in log, log[-400:])

    log = play('er', IDLE + ',' + to_inf + ',confirm,wait:3,confirm,wait:3')
    c = counts(log)
    check('Confirm again unmarks it', c[-1] == (0, 0, 0) and 'enemy mark: Infantry off' in info(log, 'enemy mark')[-1], (c, info(log, 'enemy mark')))
    check('unmarking brings the map menu nowhere (no menu opened by either Confirm)', not info(log, 'battle menu:'), info(log, 'battle menu:'))

    # two marks combine: the infantry, then (cursor on the scout at 8,0) the scout
    to_scout = ',right,right,up,up,up'
    log = play('er', IDLE + ',' + to_inf + ',confirm,wait:3' + to_scout + ',confirm,wait:3')
    c = counts(log)
    m, a = expected([INF, SCOUT], [(1, 3)])
    check('two marked enemies combine (%d move, %d attack)' % (m, a), c and c[-1] == (2, m, a), c)

    # ---- the Ranges button: all enemies, then hidden again
    log = play('er', IDLE + ',ranges,wait:3')
    c = counts(log)
    m, a = expected([INF, SCOUT, ARCHER], [(1, 3)])
    check('the Ranges button shows all three enemies (%d move, %d attack)' % (m, a), c and c[-1] == (3, m, a), c)
    log = play('er', IDLE + ',ranges,wait:3,ranges,wait:3')
    check('pressed again it hides them', counts(log)[-1] == (0, 0, 0), counts(log))

    # ---- the pictures: marked / all differ from the plain one, in every view
    for style, label in STYLES + [('classic', 'hexagon')]:
        game = 'er_hex' if label == 'hexagon' else 'er'
        plain = os.path.join(TMP, 'plain_%s.bmp' % label.replace(' ', ''))
        marked = os.path.join(TMP, 'marked_%s.bmp' % label.replace(' ', ''))
        both = os.path.join(TMP, 'all_%s.bmp' % label.replace(' ', ''))
        play(game, IDLE + ',wait:3', shot=plain, style=style)
        log = play(game, IDLE + ',' + to_inf + ',confirm,wait:3', shot=marked, style=style)
        play(game, IDLE + ',ranges,wait:3', shot=both, style=style)
        for n, p in (('plain', plain), ('marked', marked), ('all', both)):
            keep('er_%s_%s' % (n, label.replace(' ', '')), p)
        d1, d2 = differs(plain, marked), differs(plain, both)
        check('%s: the picture with a marked enemy differs from the plain one (%d pixels)' % (label, d1), d1 > 300, d1)
        check('%s: the picture with all ranges differs from the plain one (%d pixels)' % (label, d2), d2 > 300, (d1, d2))
        check('%s: the mark was made' % label, any(x[0] == 1 for x in counts(log)), counts(log))

    # ---- the map menu has the entry for every device (a tap on an empty square opens it too)
    log = play('er', IDLE + ',menu,wait:3')
    menu = ' '.join(info(log, 'battle menu:'))
    check('the map menu lists "Enemy ranges: show"', 'Enemy ranges: show' in menu, menu)
    # End Turn, [Objective], Unit..., Enemy ranges, Leave/Abandon, Cancel: find the row by walking down until it logs
    entries = [e.strip() for e in menu.split('battle menu:')[-1].split(',')]
    row = entries.index('Enemy ranges: show') if 'Enemy ranges: show' in entries else 0
    log = play('er', IDLE + ',menu,wait:3,' + ','.join(['down'] * row) + ',confirm,wait:3')
    c = counts(log)
    check('choosing it shows every enemy', c and c[-1][0] == 3, (c, entries, row))
    log = play('er', IDLE + ',menu,wait:3,' + ','.join(['down'] * row) + ',confirm,wait:3,menu,wait:3')
    check('and the entry then says "hide"', 'Enemy ranges: hide' in ' '.join(info(log, 'battle menu:')[-1:]), info(log, 'battle menu:'))

    # ---- what Confirm on an enemy did before still works with the feature off (and the menu is reachable otherwise)
    sf = os.path.join(TMP, 'settings.json')
    write_json(sf, {'version': 1, 'global': {'enemyRanges': 'off'}})
    log = play('er', IDLE + ',' + to_inf + ',confirm,wait:3', settings=sf)
    check('option Off: Confirm on an enemy opens the map menu as before', info(log, 'battle menu:') and not counts(log) and not info(log, 'enemy mark'), (info(log, 'battle menu:'), counts(log)))
    check('option Off: the menu has no "Enemy ranges" entry', 'Enemy ranges' not in ' '.join(info(log, 'battle menu:')), info(log, 'battle menu:'))
    log = play('er', IDLE + ',ranges,wait:3', settings=sf)
    check('option Off: the Ranges button does nothing', not counts(log), counts(log))
    write_json(sf, {'version': 1, 'global': {'enemyRanges': 'both'}})
    log = play('er_off', IDLE + ',' + to_inf + ',confirm,wait:3', settings=sf)
    check('a game with rules.display.enemyRanges false: no setting turns it on', info(log, 'battle menu:') and not counts(log), (info(log, 'battle menu:'), counts(log)))
    write_json(sf, {'version': 1, 'global': {'enemyRanges': 'attack'}})
    log = play('er', IDLE + ',' + to_inf + ',confirm,wait:3', settings=sf)
    c = counts(log)
    m, a = expected([INF], [(1, 3)])
    check('option Attack only: one tint over the whole danger area (%d squares)' % (m + a), c and c[-1] == (1, 0, m + a), c)

    # ---- the player's own selection still works (the unit's blue / red win on a shared square) and nothing else breaks
    log = play('er', IDLE + ',' + to_inf + ',confirm,wait:3,left,left,left,left,left,confirm,wait:3')
    check('selecting an own unit with a mark up still opens its move range (no menu, no warning)', 'UI bug' not in log and '[warn]' not in log, log[-300:])

    # ---- fog: only what the player sees
    log = play('er', IDLE + ',ranges,wait:3', map_id='fog')
    c = counts(log)
    check('fog: "all" counts only the enemy in sight', c and c[-1][0] == 1, c)
    log = play('er', IDLE + ',' + ','.join(['right'] * 5) + ',confirm,wait:3', map_id='fog')  # the hidden scout's square
    check('fog: Confirm on a square holding an unseen enemy marks nothing (it opens the menu like an empty square)',
          not info(log, 'enemy mark') and info(log, 'battle menu:'), (info(log, 'enemy mark'), info(log, 'battle menu:')))
    log = play('er', IDLE + ',right,right,confirm,wait:3', map_id='fog')  # the visible infantry (3,3)
    check('fog: the enemy in sight can be marked', 'enemy mark: Infantry on' in info(log, 'enemy mark'), info(log, 'enemy mark'))

    # ---- marks live through the player's next turn and are not saved
    saves = os.path.join(TMP, 'saves')
    shutil.rmtree(saves, ignore_errors=True)
    # mark the infantry, open the menu and end the turn (End Turn is the first row), wait for red, back to blue's turn
    log = play('er', IDLE + ',' + to_inf + ',confirm,wait:3,menu,wait:3,confirm,wait:150', saves=saves, tail=300)
    c = counts(log)
    check('the mark is still there on the next turn (recomputed for the new board)', len(c) >= 2 and c[-1][0] == 1 and c[-1][1] > 0, c)
    check('the turn really changed', len(info(log, 'ai: Red Army acts')) >= 1, info(log, 'ai:'))
    cont = os.path.join(saves, 'er', 'continue.json')
    text = open(cont).read() if os.path.exists(cont) else ''
    check('the Continue save exists and says nothing about marks or ranges', bool(text) and not any(w in text for w in ('enemyRanges', 'marks', 'foe', 'danger')), text[:200])

    shutil.rmtree(TMP, ignore_errors=True) if not failures else None
    summary()


main()
