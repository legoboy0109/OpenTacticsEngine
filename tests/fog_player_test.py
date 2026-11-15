#!/usr/bin/env python3
"""The player's part of docs/ECONOMY.md "Fog of war", windowless.

  python3 tests/fog_player_test.py ./build/engine/player/ote-player .
  python3 tests/fog_player_test.py --make-games OUT_DIR .             # only writes the test game
  python3 tests/fog_player_test.py ./build/engine/player/ote-player . --shots OUT_DIR   # also keeps the screenshots

Writes a small game in a temp folder (never in the repo; games/ has no fog content yet): a forest that hides, a
mountain with a vision bonus, a town, an AI side, and maps for each thing checked (hiding, an ambush, an unseen
capture, an AI turn nobody sees, two human sides for the hot-seat cover). Runs the player with --input scripts and
checks the [info] lines it logs and the pixels of the screenshots, in the top-down view and the 3D isometric view
(software backend; also --3d hardware when the hardware backend exists here).

How pixels are judged: a picture with fog is compared with the SAME state drawn with --show-all (the maker's
toggle: no fog drawn, same rules), so "dimmed" means "darker than without fog" and nothing depends on exact colours
or on where the camera put the map. Units are found by their side colour (the placeholder blocks are flat colours).
"""
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile

PLAYER = ''
REPO = ''
GAMES = ''
SHOTS = ''
failures = []
passed = 0
W, H = 480, 270  # the game's view; the picture is read back at this size
MAP_W, MAP_H, TILE = 10, 6, 24  # small, so the whole map is in the picture in the 3D isometric view too

STYLES = [('classic', 'top-down'), ('isometric-3d', '3D isometric')]


# ---- the test game

def write_json(path, obj):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    json.dump(obj, open(path, 'w'), indent=1)


def unit(type_, side, x, y, **kw):
    u = {'type': type_, 'side': side, 'x': x, 'y': y}
    u.update(kw)
    return u


def make_map(name, tiles, units, structures=(), sides=None, fog=True):
    rows = [list('.' * MAP_W) for _ in range(MAP_H)]
    for (x, y), ch in tiles.items():
        rows[y][x] = ch
    m = {'name': name, 'grid': {'type': 'square', 'width': MAP_W, 'height': MAP_H}, 'fog': fog,
         'legend': {'.': 'plain', 'f': 'forest', 'm': 'mountain'}, 'tiles': [''.join(r) for r in rows],
         'sides': sides or [
             {'id': 'blue', 'name': 'Blue Army', 'color': '#3d85c6', 'controller': 'human'},
             {'id': 'red', 'name': 'Red Army', 'color': '#cc0000', 'controller': 'ai'}],
         'structures': list(structures), 'units': list(units)}
    return m


def make_games(out):
    g = write_game(out, 'fog', ('hide', 'ambush', 'capture', 'wander', 'approach', 'seat'))
    write_upgrade_game(out)
    write_json(os.path.join(out, 'index.json'), {'games': ['fog', 'fogup']})
    make_maps(g)


def write_game(out, gid, map_ids, economy=False):
    """The game folder `gid` with the shared rules and data; `economy` adds funds and structure upgrades."""
    g = os.path.join(out, gid)
    shutil.rmtree(g, ignore_errors=True)
    write_json(g + '/game.json', {
        'formatVersion': 1, 'name': 'Fog Player Test',
        'styles': [{'id': 'classic', 'name': 'Classic'},
                   {'id': 'isometric', 'name': 'Isometric', 'view': 'isometric'},
                   {'id': 'isometric-3d', 'name': '3D Isometric', 'view': 'isometric-3d'},
                   {'id': 'full-3d', 'name': 'Full 3D', 'view': '3d'}],
        'view': {'width': W, 'height': H, 'tileSize': TILE},
        'stats': [{'id': 'hp', 'name': 'HP'}, {'id': 'mov', 'name': 'Move'}],
        'roles': {'hp': 'hp', 'move': 'mov'},
        'moveTypes': [{'id': 'foot', 'name': 'Foot'}],
        'rules': dict({'turnOrder': 'sides', 'counterAttacks': True, 'defeat': ['noUnits'],
                       'capture': {'enabled': True, 'amount': 'a.hp / 10'}},
                      **({'economy': {'enabled': True}} if economy else {})),
        'formulas': {'damage': 'vs * (a.hp / a.maxhp)'},
        'maps': [{'id': m, 'name': m, 'file': 'maps/%s.json' % m} for m in map_ids]})
    if economy:
        game = json.load(open(g + '/game.json'))
        game['resources'] = [{'id': 'funds', 'name': 'Funds'}]
        write_json(g + '/game.json', game)
    write_json(g + '/data/terrain.json', {'terrain': [
        {'id': 'plain', 'name': 'Plain', 'color': '#9ccc65', 'glyph': '.', 'props': {'defense': 1}},
        {'id': 'forest', 'name': 'Forest', 'color': '#2e7d32', 'glyph': 'f', 'hides': True, 'moveCost': {'foot': 1},
         'props': {'defense': 2}},
        {'id': 'mountain', 'name': 'Mountain', 'color': '#8d6e63', 'glyph': 'm', 'visionBonus': 2,
         'moveCost': {'foot': 2}, 'props': {'defense': 3}}]})
    write_json(g + '/data/units.json', {'units': [
        {'id': 'infantry', 'name': 'Infantry', 'glyph': 'i', 'moveType': 'foot', 'stats': {'hp': 100, 'mov': 3},
         'weapons': ['rifle'], 'canCapture': True, 'vision': 2},
        {'id': 'scout', 'name': 'Scout', 'glyph': 's', 'moveType': 'foot', 'stats': {'hp': 100, 'mov': 6},
         'weapons': ['rifle'], 'vision': 2},
        {'id': 'blind', 'name': 'Blind', 'glyph': 'b', 'moveType': 'foot', 'stats': {'hp': 100, 'mov': 4},
         'weapons': ['rifle'], 'vision': 1}]})
    write_json(g + '/data/weapons.json', {'weapons': [
        {'id': 'rifle', 'name': 'Rifle', 'versus': {'infantry': 55, 'scout': 55, 'blind': 55}}]})
    structures = [{'id': 'town', 'name': 'Town', 'color': '#b7a58a', 'glyph': 't', 'capturePoints': 10}]
    if economy:  # a camp that red upgrades into a fort: different colours, same 1 x 1 footprint
        structures += [{'id': 'camp', 'name': 'Camp', 'color': '#808080', 'glyph': 'c', 'capturePoints': 10,
                        'upgrades': [{'to': 'fort', 'cost': {'funds': 100}}]},
                       {'id': 'fort', 'name': 'Fort', 'color': '#ffd600', 'glyph': 'F', 'capturePoints': 20}]
    write_json(g + '/data/structures.json', {'structures': structures})
    return g


def write_upgrade_game(out, funds=1000):
    """'fogup': red owns a camp four squares from blue's scout and upgrades it on its first turn (when it has `funds`), out of
    blue's sight; blue's scout can reach a square from which the camp is in sight in one move."""
    g = write_game(out, 'fogup', ('upgrade', 'noupgrade'), economy=True)
    for name, red_funds in (('upgrade', funds), ('noupgrade', 0)):
        m = make_map(name, {}, [unit('scout', 'blue', 0, 5), unit('infantry', 'red', 9, 0)],
                     [{'type': 'camp', 'x': 4, 'y': 5, 'side': 'red'}],
                     sides=[{'id': 'blue', 'name': 'Blue Army', 'color': '#3d85c6', 'controller': 'human',
                             'resources': {'funds': 0}},
                            {'id': 'red', 'name': 'Red Army', 'color': '#cc0000', 'controller': 'ai',
                             'resources': {'funds': red_funds}}])
        write_json(g + '/maps/%s.json' % name, m)


def make_maps(g):
    def maps(name, obj):
        write_json(g + '/maps/%s.json' % name, obj)

    # hide: forest at (6,3) hides a red infantry; mountain at (2,1) adds 2 to a unit's vision. Blue units in id order:
    # scout (1,3), infantry (1,1) -> the cursor starts on the scout.
    maps('hide', make_map('Hide', {(6, 3): 'f', (2, 1): 'm'},
                          [unit('scout', 'blue', 1, 3), unit('infantry', 'blue', 1, 1), unit('infantry', 'red', 6, 3)]))
    # ambush: the blind soldier (vision 1) is ordered four squares east; a red infantry stands at (4,3) in plain sight
    # of nobody (three squares away).
    maps('ambush', make_map('Ambush', {}, [unit('blind', 'blue', 1, 3), unit('infantry', 'red', 4, 3)]))
    # capture: a neutral town far from blue; red stands near it and takes it on its first turn, out of sight.
    maps('capture', make_map('Capture', {}, [unit('infantry', 'blue', 0, 0), unit('infantry', 'red', 7, 4)],
                             [{'type': 'town', 'x': 8, 'y': 5}]))
    # wander: five red soldiers far from blue with a lot to do; nothing of it is in sight.
    maps('wander', make_map('Wander', {}, [unit('infantry', 'blue', 0, 0)] +
                            [unit('infantry', 'red', 8 + (i % 2), i) for i in range(5)]))
    # approach: blue owns a town in the middle; red walks towards it and comes into sight only at the end of its walk.
    maps('approach', make_map('Approach', {}, [unit('infantry', 'blue', 3, 3), unit('infantry', 'red', 8, 3)],
                              [{'type': 'town', 'x': 3, 'y': 3, 'side': 'blue'}]))
    # seat: two human sides of different teams and an AI side. Turn order blue, green, red.
    maps('seat', make_map('Seat', {(6, 3): 'f'},
                          [unit('infantry', 'blue', 1, 3), unit('infantry', 'green', 8, 3), unit('infantry', 'red', 5, 0)],
                          sides=[{'id': 'blue', 'name': 'Blue Army', 'color': '#3d85c6', 'controller': 'human', 'team': 'a'},
                                 {'id': 'green', 'name': 'Green Army', 'color': '#2e9e44', 'controller': 'human', 'team': 'b'},
                                 {'id': 'red', 'name': 'Red Army', 'color': '#cc0000', 'controller': 'ai', 'team': 'c'}]))


# ---- running the player

def play(args, saves=None, frames=300, interval=4, inp=None, shot=None, style='classic', dt=0.016, game='fog'):
    """Runs the player windowless; returns (exit code, log text)."""
    cmd = [PLAYER, '--games', GAMES, '--assets', os.path.join(REPO, 'assets'), '--headless', '--no-settings',
           '--frames', str(frames), '--input-interval', str(interval), '--fixed-dt', str(dt), '--game', game,
           '--style', style, '--3d', 'software']
    cmd += ['--saves', saves] if saves else ['--no-saving']
    if inp:
        cmd += ['--input', inp]
    if shot:
        cmd += ['--screenshot', shot]
    p = subprocess.run(cmd + args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True, timeout=300)
    return p.returncode, p.stdout


def check(name, cond, detail=''):
    global passed
    if cond:
        passed += 1
        print('PASS ' + name)
    else:
        failures.append(name)
        print('FAIL %s %s' % (name, detail))


def lines(log, prefix):
    """The [info] lines that start with `prefix` (the text after the level tag)."""
    out = []
    for l in log.split('\n'):
        m = re.search(r'\[info\]\s*(.*)$', l)
        if m and m.group(1).startswith(prefix):
            out.append(m.group(1))
    return out


# ---- pictures

class Image:
    def __init__(self, path):
        data = open(path, 'rb').read()
        offset = struct.unpack_from('<I', data, 10)[0]
        self.w, h = struct.unpack_from('<ii', data, 18)
        bpp = struct.unpack_from('<H', data, 28)[0] // 8
        self.h = abs(h)
        stride = (self.w * bpp + 3) & ~3
        rows = [data[offset + r * stride: offset + r * stride + self.w * bpp] for r in range(self.h)]
        if h > 0:
            rows.reverse()
        self.rows, self.bpp = rows, bpp

    def px(self, x, y):
        r = self.rows[y]
        i = x * self.bpp  # BMP stores B, G, R (and A)
        return (r[i + 2], r[i + 1], r[i])

    def map_width(self):
        return self.w * 11 // 15  # the map viewport: the window minus the info panel (about 128 of 480)


def dist(a, b):
    return max(abs(a[0] - b[0]), abs(a[1] - b[1]), abs(a[2] - b[2]))


def count_color(img, rgb, tol=24, box=None):
    """Pixels of the map area within `tol` of a colour; also their mean position."""
    x0, y0, x1, y1 = box or (0, 0, img.map_width(), img.h - 12)
    n = sx = sy = 0
    for y in range(y0, y1):
        for x in range(x0, x1):
            if dist(img.px(x, y), rgb) <= tol:
                n += 1
                sx += x
                sy += y
    return n, ((sx // n, sy // n) if n else None)


def darker_count(fog, full, box=None, margin=30):
    """Pixels of the map area where the fog picture is clearly darker than the same state drawn without fog."""
    x0, y0, x1, y1 = box or (0, 0, fog.map_width(), fog.h - 12)
    n = 0
    for y in range(y0, y1):
        for x in range(x0, x1):
            a, b = fog.px(x, y), full.px(x, y)
            if sum(b) - sum(a) > margin:
                n += 1
    return n


def differ_count(a, b, box, margin=12):
    x0, y0, x1, y1 = box
    return sum(1 for y in range(y0, y1) for x in range(x0, x1) if dist(a.px(x, y), b.px(x, y)) > margin)


def box_around(c, rad):
    return (max(0, c[0] - rad), max(0, c[1] - rad), c[0] + rad, c[1] + rad)


BLUE, RED, GREEN = (0x3d, 0x85, 0xc6), (0xcc, 0, 0), (0x2e, 0x9e, 0x44)


def lum_box(img, box):
    x0, y0, x1, y1 = box
    t = n = 0
    for y in range(y0, y1):
        for x in range(x0, x1):
            t += sum(img.px(x, y))
            n += 1
    return t / max(1, n)


# ---- scenarios. Input recipes: a token is one key press every `interval` frames; waits let animations finish.

END_TURN = 'menu,confirm,wait:40'


def shots(tmp, name, style, args, inp, frames, saves=None, dt=0.016, game='fog'):
    """Two pictures of the same moment: with fog and with --show-all. Returns (fog image, all image, fog log)."""
    a, b = os.path.join(tmp, '%s_%s_fog.bmp' % (name, style)), os.path.join(tmp, '%s_%s_all.bmp' % (name, style))
    _, log = play(args, saves=saves, frames=frames, inp=inp, shot=a, style=style, dt=dt, game=game)
    _, log2 = play(args + ['--show-all'], frames=frames, inp=inp, shot=b, style=style, dt=dt, game=game)
    if SHOTS:
        shutil.copy(a, os.path.join(SHOTS, os.path.basename(a)))
        shutil.copy(b, os.path.join(SHOTS, os.path.basename(b)))
    return Image(a), Image(b), log


def scenarios(tmp):
    for style, label in STYLES:
        hide(tmp, style, label)
        ambush(tmp, style, label)
        capture(tmp, style, label)
        upgrade(tmp, style, label)
    wander(tmp)
    approach(tmp)
    hardware(tmp)
    seat(tmp)
    resume(tmp)
    cli()


def hide(tmp, style, label):
    # 1. The start: far squares are dimmed, near ones are not, the hidden enemy is not drawn.
    fog, full, log = shots(tmp, 'start', style, ['--map', 'hide'], 'wait:30', 140)
    check('%s: the log says fog is on and who the viewer is' % label, 'fog: viewer Blue Army (team 0)' in ''.join(lines(log, 'fog:')), log[-300:])
    check('%s: without fog the enemy is drawn' % label, count_color(full, RED)[0] > 30)
    check('%s: with fog the enemy in the forest is not drawn' % label, count_color(fog, RED)[0] == 0, str(count_color(fog, RED)))
    n_blue, centre = count_color(full, BLUE)
    check('%s: the player\'s own units are drawn' % label, count_color(fog, BLUE)[0] > 30)
    near = box_around(centre, 14) if centre else (0, 0, 10, 10)
    check('%s: squares around the player\'s units are not dimmed' % label, darker_count(fog, full, near) < 12, str(darker_count(fog, full, near)))
    check('%s: far squares are dimmed' % label, darker_count(fog, full) > 5000, str(darker_count(fog, full)))

    # 2. Vision grows by the mountain's bonus: the infantry climbs it and square (7,2) comes into sight.
    climb = 'wait:30,next,confirm,right,confirm,wait:20,confirm,wait:30'  # select the infantry (Tab), step onto (3,2), Wait
    fog2, full2, log2 = shots(tmp, 'mountain', style, ['--map', 'hide'], climb, 260)
    # Compare the two states with fog: the dimmed area got smaller.
    check('%s: standing on the mountain lights up more of the map' % label, darker_count(fog2, full2) < darker_count(fog, full) - 600,
          '%d -> %d' % (darker_count(fog, full), darker_count(fog2, full2)))

    # 3. The scout walks to (4,3): the forest square (6,3) is in sight now, but the soldier in it still is not drawn.
    near_forest = 'wait:30,confirm,' + 'right,' * 3 + 'confirm,wait:20,down,confirm,wait:30'
    fog3, full3, log3 = shots(tmp, 'forestnear', style, ['--map', 'hide'], near_forest, 260)
    check('%s: two squares from a hideout the enemy in it is still not drawn' % label, count_color(fog3, RED)[0] == 0, str(count_color(fog3, RED)))
    # 4. Next to it: noticed.
    beside = 'wait:30,confirm,' + 'right,' * 4 + 'confirm,wait:20,down,confirm,wait:30'
    fog4, full4, log4 = shots(tmp, 'forestbeside', style, ['--map', 'hide'], beside, 260)
    check('%s: next to the hideout the enemy is drawn' % label, count_color(fog4, RED)[0] > 30, str(count_color(fog4, RED)))


def ambush(tmp, style, label):
    # Four squares east of the blind soldier stands an enemy nobody sees: it stops short and the cue shows.
    go = 'wait:30,confirm,right,right,right,right,confirm'
    fog, full, log = shots(tmp, 'ambush', style, ['--map', 'ambush'], go, 165)
    a = lines(log, 'ambush:')
    check('%s: the ambush is logged with where it stopped and what was in the way' % label,
          a == ['ambush: Blind stopped at 3,3 by Infantry at 4,3'], str(a) + log[-300:])
    check('%s: the revealed enemy is drawn' % label, count_color(fog, RED)[0] > 30, str(count_color(fog, RED)))
    check('%s: the ambush cue shows on the unit' % label, count_color(fog, (255, 120, 100), tol=40)[0] > 20, str(count_color(fog, (255, 120, 100), 40)))
    # The turn of that unit is over: no action menu, and nothing to select again.
    _, log2 = play(['--map', 'ambush'], frames=400, inp=go + ',wait:40,confirm', style=style)
    check('%s: after an ambush no menu opens and the unit is done' % label, not lines(log2, 'UI bug'), log2[-300:])


def capture(tmp, style, label):
    # Red takes the town in the corner during its turn, out of sight: blue keeps drawing it as nobody's.
    fog, full, log = shots(tmp, 'capture', style, ['--map', 'capture'], 'wait:20,' + END_TURN + ',wait:80', 500)
    check('%s: the AI turn is over and blue plays again' % label, len(lines(log, 'turn:')) >= 2, str(lines(log, 'turn:')))
    check('%s: without fog the town shows red\'s frame' % label, count_color(full, RED)[0] > 30, str(count_color(full, RED)))
    check('%s: with fog the unseen capture changes nothing (no red anywhere)' % label, count_color(fog, RED)[0] == 0, str(count_color(fog, RED)))


def upgrade(tmp, style, label):
    # Red upgrades its camp into a fort during its turn, out of blue's sight. Judged by comparing with the same
    # battle where red could not afford the upgrade (so nothing depends on colours or the camera): out of sight the
    # two pictures are the same, in sight they differ at the structure, and the later sighting shows no animation.
    wait = 'wait:20,' + END_TURN + ',wait:80'
    inp = wait + ',confirm,' + 'right,' * 2 + 'confirm,wait:20,confirm,wait:40'  # the scout walks to (2,5), 2 squares from the camp
    up, up_all, log = shots(tmp, 'up_hidden', style, ['--map', 'upgrade'], wait, 520, game='fogup')
    no, no_all, _ = shots(tmp, 'noup_hidden', style, ['--map', 'noupgrade'], wait, 520, game='fogup')
    box = (0, 0, up.map_width(), up.h - 12)
    check('%s: red upgraded its camp (the maker sees the change)' % label, differ_count(up_all, no_all, box) > 60, str(differ_count(up_all, no_all, box)))
    check('%s: an upgrade out of sight is not drawn (the picture is the one without it)' % label, differ_count(up, no, box) < 8, str(differ_count(up, no, box)))
    check('%s: the upgrade out of sight is not announced' % label, not lines(log, 'upgrade:'), str(lines(log, 'upgrade:')))
    up2, _, log2 = shots(tmp, 'up_seen', style, ['--map', 'upgrade'], inp, 800, game='fogup')
    no2, _, _ = shots(tmp, 'noup_seen', style, ['--map', 'noupgrade'], inp, 800, game='fogup')
    check('%s: a unit that comes into sight of the structure shows the new kind' % label, differ_count(up2, no2, box) > 60, str(differ_count(up2, no2, box)))
    check('%s: and learns it without an animation or a message' % label, not lines(log2, 'upgrade:'), str(lines(log2, 'upgrade:')))


def ai_turn_seconds(log):
    """Seconds from the AI side starting to act (after the banner of its turn) to the next human turn beginning."""
    ai = [float(m.group(1)) for m in re.finditer(r'ai: .*? t=([0-9.]+)', log)]
    human = [float(m.group(1)) for m in re.finditer(r'turn: .*? t=([0-9.]+)', log)]
    if not ai:
        return None
    later = [h for h in human if h > ai[0]]
    return later[0] - ai[0] if later else None


def wander(tmp):
    # An AI turn in which nothing is seen takes no longer than the banners (every unit's walk is skipped).
    inp = 'wait:20,' + END_TURN + ',wait:150'
    _, log = play(['--map', 'wander'], frames=700, inp=inp)
    _, logs = play(['--map', 'wander', '--show-all'], frames=700, inp=inp)
    fog_len, all_len = ai_turn_seconds(log), ai_turn_seconds(logs)
    check('wander: both runs reach the next player turn', fog_len is not None and all_len is not None, '%s %s' % (fog_len, all_len))
    if fog_len is not None and all_len is not None:
        # What is left with fog is the banner of the next turn (0.6 s, the same however much the AI did).
        check('an AI turn nobody sees takes no time of its own (%.2fs against %.2fs with everything shown)' % (fog_len, all_len),
              fog_len < 0.8 and all_len > fog_len + 0.25, '%.2f %.2f' % (fog_len, all_len))


def approach(tmp):
    # An enemy walking into view appears where it is first seen (here: its last square) and not before.
    _, log = play(['--map', 'approach'], frames=500, inp='wait:20,' + END_TURN + ',wait:80')
    w = lines(log, 'fog: Infantry seen on squares')
    check('an enemy that walks into view is shown from the first square seen', bool(w) and w[0].startswith('fog: Infantry seen on squares 3..3 of its 3-square walk'), str(w))
    _, log = play(['--map', 'approach', '--show-all'], frames=500, inp='wait:20,' + END_TURN + ',wait:80')
    check('with --show-all every walk is shown whole', not lines(log, 'fog: Infantry seen'), str(lines(log, 'fog:')))


def hardware(tmp):
    # The OpenGL ES 3 backend draws the same dimmed picture as the software one (both 3D kinds of view).
    script = 'wait:30,next,confirm,right,confirm,wait:20,confirm,wait:30'
    for style, label in (('isometric-3d', '3D isometric'), ('full-3d', 'full 3D')):
        soft, hard, full = (os.path.join(tmp, 'hw_%s_%s.bmp' % (style, k)) for k in ('s', 'h', 'a'))
        play(['--map', 'hide'], frames=260, inp=script, shot=soft, style=style)
        _, log = play(['--map', 'hide', '--3d', 'hardware'], frames=260, inp=script, shot=hard, style=style)
        if 'no hardware backend' in log:
            print('SKIP %s: no hardware backend here' % label)
            continue
        play(['--map', 'hide', '--3d', 'hardware', '--show-all'], frames=260, inp=script, shot=full, style=style)
        s_img, h_img, f_img = Image(soft), Image(hard), Image(full)
        box = (0, 0, s_img.map_width(), s_img.h - 12)
        check('%s: the hardware backend draws fog like the software one' % label, differ_count(s_img, h_img, box, margin=40) < 1200,
              str(differ_count(s_img, h_img, box, margin=40)))
        check('%s: with the hardware backend the unseen squares are dimmed' % label, darker_count(h_img, f_img) > 4000, str(darker_count(h_img, f_img)))


def seat(tmp):
    # Two human sides of different teams: the screen is covered between their turns.
    inp = 'wait:10,confirm,wait:10,' + END_TURN
    for style, label in STYLES:
        a = os.path.join(tmp, 'seat_%s.bmp' % style)
        _, log = play(['--map', 'seat'], frames=40, inp='wait:10', shot=a, style=style)
        cu = lines(log, 'fog: cover up')
        check('%s: hot-seat starts with the cover for the first side' % label, cu[:1] == ['fog: cover up: pass the device to Blue Army'], str(cu))
        img = Image(a)
        lit = sum(1 for y in range(img.h) for x in range(0, img.w, 2) if sum(img.px(x, y)) > 60)
        check('%s: the cover hides the whole map (almost no lit pixel)' % label, lit < img.h * img.w // 2 * 0.06, 'lit %d' % lit)
        check('%s: the cover names the side in its colour' % label, count_color(img, BLUE, tol=10, box=(0, 0, img.w, img.h))[0] > 100)
    _, log = play(['--map', 'seat'], frames=500, inp=inp + ',confirm,wait:20')
    ev = lines(log, 'fog: cover') + lines(log, 'fog: viewer')
    check('hot-seat: Confirm lowers the cover, the next human turn raises it for the other side',
          [l for l in ev if l.startswith('fog: cover')][:3] == ['fog: cover up: pass the device to Blue Army', 'fog: cover down: Blue Army plays',
                                                                 'fog: cover up: pass the device to Green Army'], str(ev))
    check('hot-seat: the viewer follows the side whose turn it is', any('viewer Green Army (team 1)' in l for l in ev), str(ev))
    # The map is shown again after the cover and is drawn for the new side.
    a = os.path.join(tmp, 'seat2.bmp')
    play(['--map', 'seat'], frames=400, inp=inp + ',confirm,wait:30', shot=a)
    img = Image(a)
    check('hot-seat: after Confirm the map is drawn for the next side', count_color(img, GREEN)[0] > 30)
    # Only one human team: no cover.
    _, log = play(['--map', 'hide'], frames=60, inp='wait:10')
    check('one human team: no cover', not lines(log, 'fog: cover'), str(lines(log, 'fog: cover')))
    # --show-all with the same map: no fog, no cover.
    _, log = play(['--map', 'seat', '--show-all'], frames=60, inp='wait:10')
    check('--show-all: no cover and no viewer', not lines(log, 'fog: cover') and not lines(log, 'fog: viewer'), str(lines(log, 'fog:')))


def resume(tmp):
    # The Continue save made at a turn start resumes into the same picture (top-down: per square), and into the
    # cover when two human teams share the device.
    sv = os.path.join(tmp, 'sv_hide')
    shutil.rmtree(sv, ignore_errors=True)
    inp = 'wait:20,confirm,' + 'right,' * 4 + 'confirm,wait:20,down,confirm,wait:20,' + END_TURN + ',wait:150'
    a, b = os.path.join(tmp, 'r1.bmp'), os.path.join(tmp, 'r2.bmp')
    play(['--map', 'hide'], saves=sv, frames=900, inp=inp, shot=a)
    _, log = play(['--continue'], saves=sv, frames=200, inp='wait:10', shot=b)
    i1, i2 = Image(a), Image(b)
    box = (0, 0, i1.map_width(), i1.h - 12)
    check('resume: the picture of a fog battle is the same as before the save', differ_count(i1, i2, box) < 250, str(differ_count(i1, i2, box)))
    check('resume: the log says who the viewer is again', any('viewer Blue Army' in l for l in lines(log, 'fog:')), str(lines(log, 'fog:')))
    sv = os.path.join(tmp, 'sv_seat')
    shutil.rmtree(sv, ignore_errors=True)
    play(['--map', 'seat'], saves=sv, frames=500, inp='wait:10,confirm,wait:10,' + END_TURN)
    _, log = play(['--continue'], saves=sv, frames=60, inp='wait:10', shot=b)
    cu = lines(log, 'fog: cover up')
    check('resume (hot-seat): the Continue save made at the pass resumes into the cover for that side',
          cu[:1] == ['fog: cover up: pass the device to Green Army'], str(cu) + log[-300:])
    img = Image(b)
    lit = sum(1 for y in range(img.h) for x in range(0, img.w, 2) if sum(img.px(x, y)) > 60)
    check('resume (hot-seat): nothing of the map is drawn under it', lit < img.h * img.w // 2 * 0.06, 'lit %d' % lit)


def cli():
    _, log = play(['--map', 'hide', '--show-all'], frames=30, inp='wait:5')
    check('--show-all is announced in the log and no viewer is chosen', 'fog: on, drawn without fog (--show-all)' in lines(log, 'fog:'), str(lines(log, 'fog:')))
    _, log = play(['--map', 'hide', '--autoplay'], frames=30, inp='wait:5')
    check('autoplay (no human side to look for): everyone sees everything', 'fog: on, no human side: everyone sees everything' in lines(log, 'fog:'), str(lines(log, 'fog:')))


def main():
    global PLAYER, REPO, GAMES, SHOTS
    args = sys.argv[1:]
    if len(args) == 3 and args[0] == '--make-games':
        make_games(os.path.abspath(args[1]))
        return 0
    if len(args) < 2:
        print(__doc__)
        return 2
    PLAYER, REPO = os.path.abspath(args[0]), os.path.abspath(args[1])
    if '--shots' in args:
        SHOTS = os.path.abspath(args[args.index('--shots') + 1])
        os.makedirs(SHOTS, exist_ok=True)
    tmp = tempfile.mkdtemp(prefix='ote_fog_test_')
    GAMES = os.path.join(tmp, 'games')
    try:
        make_games(GAMES)
        scenarios(tmp)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    print('\n%d passed, %d failed' % (passed, len(failures)))
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
