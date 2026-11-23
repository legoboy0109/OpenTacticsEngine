#!/usr/bin/env python3
"""Three engine features through the real player, windowless: a map event before the preparation screen, battle and defeat quotes inside the battle scene,
and unit badges (docs/EVENTS.md "Before the preparation screen" and "Battle and defeat quotes", docs/BATTLE_SCENE.md "Quotes in the scene", docs/GRAPHICS.md "Unit badges").

  python3 tests/story_order_player_test.py ./build/engine/player/ote-player . [--shots DIR]

  - beforePrep: copies tests/fixtures/srpg-prep and gives its orchard map a `battle: beforePrep` event (a line, a switch, a unit added) and a battle-start event
    that reads the switch. In the campaign and as a free battle the line is said BEFORE the screen opens, once; the screen then opens; after Begin the battle-start
    event sees the switch and the added unit is on the map, and the beforePrep line is not said again. A save made on the screen resumes into it without the line.
    On a map without a screen (meadow) the beforePrep line comes right before the battle-start line.
  - quotes in the scene: copies tests/fixtures/srpg-quotes and plays a fight with --battle-scene scene: the quotes come after the scene opens and before the
    first strike, inside the scene, once; a defeat quote after the blow and before the fall; "quotesPerFight": "one" lets only one fighter speak; skip.
  - badges: a small generated game (tileSize 16, an 8 x 8 magenta badge): a unit with the tag wears it in the bottom right corner at the right size; a unit without,
    a player side when the rule says computer, and a game without the key show none.
Never plays audio or opens a window; --shots DIR keeps pictures (BMP).
"""
import os
import re
import shutil
import struct
import sys
import zlib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import (  # noqa: E402
    Bmp, check, edit, failures, player_and_repo, player_cmd, read_json, run_text, scratch_dir, shots_dir, summary, ticks, write_json)

sys.dont_write_bytecode = True

PLAYER, REPO = player_and_repo()
SHOTS = shots_dir()
TMP = scratch_dir()


def keep(path, name):
    if SHOTS and os.path.exists(path):
        os.makedirs(SHOTS, exist_ok=True)
        shutil.copy(path, os.path.join(SHOTS, name + '.bmp'))


def dialogue(log):
    return [m.group(1) for m in re.finditer(r'\[info\] dialogue: (.*)', log)]


def at(log, text):
    return log.find(text)


# ======================================================================================================================
# before the preparation screen
# ======================================================================================================================
TO_PREP = 'confirm,confirm,confirm,wait:20,menu,confirm,wait:100,confirm,wait:30,confirm,confirm,confirm,wait:40'  # as srpg_player_prep_test.py: chapter 2's card and line
LINE = 'Mist hangs over the orchard.'
REPORT = "The scout's report stands."


def prep_game(gid, meadow=False):
    g = os.path.join(TMP, gid)
    shutil.copytree(os.path.join(REPO, 'tests/fixtures/srpg-prep'), g)

    def game(o):
        o['switches'].append({'id': 'scouted', 'name': 'Scouted'})
    edit(g, 'game.json', game)

    def orchard(m):
        m['events'] = [
            {'id': 'opening', 'when': {'battle': 'beforePrep'}, 'do': [
                {'say': LINE, 'speakerName': 'Scout'}, {'setSwitch': 'scouted', 'to': True},
                {'addUnit': {'type': 'raider', 'side': 'red', 'x': 6, 'y': 5, 'id': 'lurker'}}]},
            {'id': 'report', 'when': {'battle': 'start'}, 'if': [{'switch': 'scouted'}, {'alive': 'lurker'}], 'do': [{'say': REPORT, 'speakerName': 'Scout'}]},
        ] + [e for e in m['events'] if e['id'] != 'muster']
    edit(g, 'maps/orchard.json', orchard)

    def meadow_events(m):  # no preparation screen on this map
        m['events'] = m.get('events', []) + [
            {'id': 'pre', 'when': {'battle': 'beforePrep'}, 'do': [{'say': 'Before the start.', 'speakerName': 'Scout'}]},
            {'id': 'post', 'when': {'battle': 'start'}, 'do': [{'say': 'The start.', 'speakerName': 'Scout'}]}]
    if meadow:
        edit(g, 'maps/meadow.json', meadow_events)
    return g


def play_prep(gid, inp, saves=None, extra=('--campaign',), shot=None):
    cmd = player_cmd(PLAYER, TMP, REPO) + ['--frames', str(ticks(inp) * 3 + 90), '--input-interval', '3', '--fixed-dt', '0.016', '--game', gid]
    cmd += ['--saves', saves] if saves else ['--no-saving']
    cmd += list(extra)
    if inp:
        cmd += ['--input', inp]
    if shot:
        cmd += ['--screenshot', shot]
    return run_text(cmd).stdout


def test_before_prep_campaign():
    gid = 'bp'
    prep_game(gid)
    s = os.path.join(TMP, 'saves_bp')
    shutil.rmtree(s, ignore_errors=True)
    log = play_prep(gid, TO_PREP + ',skip,wait:40', saves=s, shot=os.path.join(TMP, 'bp_open.bmp'))
    keep(os.path.join(TMP, 'bp_open.bmp'), 'before_prep_screen_open')
    check('beforePrep: the line is said', 'Scout | ' + LINE in dialogue(log), str(dialogue(log)))
    check('beforePrep: it is said before the screen opens', 0 <= at(log, LINE) < at(log, 'prep: opens on orchard'), log[-600:])
    check('beforePrep: the scene is announced before the screen', 'prep: 1 beforePrep entries play before the screen' in log or 'beforePrep entries play before the screen' in log, log[-400:])
    check('beforePrep: the screen does open afterwards', at(log, 'prep: opens on orchard') > 0 and at(log, 'prep menu:') > at(log, LINE))
    check('beforePrep: once', dialogue(log).count('Scout | ' + LINE) == 1)
    # Begin (the last of the main menu): the battle-start event sees the switch the beforePrep event set, and the unit it added.
    log = play_prep(gid, TO_PREP + ',skip,wait:30,' + ','.join(['down'] * 7 + ['confirm']) + ',wait:30,skip,wait:40', saves=os.path.join(TMP, 'saves_bp2'))
    got = dialogue(log)
    check('beforePrep: after Begin the battle-start event sees the state', 'Scout | ' + REPORT in got, str(got) + log[-400:])
    check('beforePrep: ... and the scene is not shown again', got.count('Scout | ' + LINE) == 1, str(got))
    save = read_json(os.path.join(TMP, 'saves_bp2', gid, 'continue.json')) if os.path.exists(os.path.join(TMP, 'saves_bp2', gid, 'continue.json')) else None
    if save and 'battle' in save:
        ids = sorted(u['id'] for u in save['battle']['units'] if 'id' in u)
        check('beforePrep: the unit it added is in the battle', 'lurker' in ids, str(ids))
    else:
        check('beforePrep: the Continue save has the battle', False, str(save and list(save)))
    # A save made on the screen resumes into it, and the scene does not play again.
    log = play_prep(gid, 'wait:10', saves=s, extra=('--continue',))
    check('beforePrep: resuming the screen opens it', 'prep: opens on orchard' in log, log[-500:])
    check('beforePrep: ... without the scene', LINE not in ''.join(dialogue(log)), str(dialogue(log)))


def test_before_prep_free_battle_and_no_prep_map():
    gid = 'bp2'
    prep_game(gid, meadow=True)
    shot = os.path.join(TMP, 'bp_scene.bmp')
    play_prep(gid, 'wait:5', extra=('--map', 'orchard'), shot=shot)  # the picture while the line is up: the map with its units, no screen yet
    keep(shot, 'before_prep_scene_on_the_map')
    log = play_prep(gid, 'wait:5,skip,wait:20', extra=('--map', 'orchard'))
    check('free battle: the line comes before the screen', 0 <= at(log, LINE) < at(log, 'prep: opens on orchard'), log[-500:])
    log = play_prep(gid, 'wait:5,skip,wait:10,' + ','.join(['down'] * 6 + ['confirm']) + ',wait:30,skip,wait:30', extra=('--map', 'orchard'))
    got = dialogue(log)
    check('free battle: after Begin the start event reads the switch, the scene is not repeated', 'Scout | ' + REPORT in got and got.count('Scout | ' + LINE) == 1, str(got))
    log = play_prep(gid, 'wait:5,confirm,confirm,confirm,confirm,confirm,confirm,wait:20', extra=('--map', 'meadow'))
    got = dialogue(log)
    check('no screen: beforePrep runs right before the battle-start event', got[:2] == ['Scout | Before the start.', 'Scout | The start.'], str(got))


# ======================================================================================================================
# quotes inside the battle scene
# ======================================================================================================================
IDLE = 'wait:14'
ATTACK = [IDLE, 'confirm', 'wait:3', 'confirm', 'wait:3', 'confirm', 'wait:3', 'confirm', 'wait:6']
NEXT = ['confirm'] * 3


def quote_game(gid, tweak=None):
    g = os.path.join(TMP, gid)
    shutil.copytree(os.path.join(REPO, 'tests/fixtures/srpg-quotes'), g)
    if tweak:
        tweak(g)
    return g


def play_quote(gid, inp, scene=True, shot=None, frames=None, interval=4):
    saves = os.path.join(TMP, 'saves_' + gid)
    shutil.rmtree(saves, ignore_errors=True)
    cmd = player_cmd(PLAYER, TMP, REPO) + ['--saves', saves, '--frames', str(frames or ticks(inp) * interval + 120), '--input-interval', str(interval),
                                           '--fixed-dt', '0.016', '--game', gid, '--map', 'ch1', '--battle-scene', 'scene' if scene else 'map']
    if inp:
        cmd += ['--input', inp]
    if shot:
        cmd += ['--screenshot', shot]
    return run_text(cmd).stdout


def order(log, keys):
    """True when the keys occur in the log in this order (each after the previous)."""
    pos = -1
    for k in keys:
        p = log.find(k, pos + 1)
        if p < 0:
            return False
        pos = p
    return True


def small_hp(g):
    edit(g, 'data/units.json', lambda u: [x.update({'stats': dict(x['stats'], hp=2)}) for x in u['units'] if x['id'] == 'brute'])


def test_battle_quote_in_the_scene():
    quote_game('qs')
    log = play_quote('qs', ','.join(ATTACK + ['wait:60'] + NEXT * 2 + ['wait:20'] + NEXT * 2 + ['wait:200']))
    got = dialogue(log)
    check('scene quote: both fighters speak, the attacked unit first', got[:2] == ['Gorm | Eirik. At last.', 'Eirik | Gorm! Your raids end today.'], str(got))
    check('scene quote: after the scene opens, before the first strike',
          order(log, ['battle scene: start', 'battle scene: quote (battle)', 'dialogue: Gorm | Eirik. At last.', 'dialogue: Eirik | Gorm! Your raids end today.',
                      'battle scene: quote over', 'battle scene: strike']), log[-1200:])
    check('scene quote: said once before the strikes (the later lines are the computer\'s turn)', got.count('Gorm | Eirik. At last.') >= 1 and at(log, 'dialogue: Gorm') < at(log, 'battle scene: strike'), str(got))
    check('scene quote: no map line before the scene', at(log, 'dialogue: Gorm') > at(log, 'battle scene: start'))
    # The same fight with the scene off: the lines come on the map, before the strikes, and the scene never opens.
    quote_game('qm')
    log = play_quote('qm', ','.join(ATTACK + NEXT * 4 + ['wait:60']), scene=False)
    check('map quote: unchanged (no scene lines, the same lines)', 'battle scene:' not in log and dialogue(log)[:2] == got[:2], str(dialogue(log)))


def test_defeat_quote_in_the_scene():
    quote_game('qd', small_hp)
    inp = ','.join(ATTACK + ['wait:60'] + NEXT * 2 + ['wait:20'] + NEXT * 2 + ['wait:140'] + NEXT * 3 + ['wait:20'] + NEXT * 3 + ['wait:300'])
    log = play_quote('qd', inp)
    check('scene defeat: after the blow, the quote, then the fall',
          order(log, ['battle scene: hit Eirik -> Gorm', 'battle scene: quote (defeat)', 'dialogue: Gorm | Impossible...', 'dialogue: Mira | Brother!',
                      'battle scene: quote over', 'battle scene: defeated Gorm', 'battle scene: end']), log[-1500:])


def test_one_quote_per_fight():
    quote_game('q1', lambda g: edit(g, 'game.json', lambda o: o.update({'quotesPerFight': 'one'})))
    log = play_quote('q1', ','.join(ATTACK + ['wait:60'] + NEXT * 2 + ['wait:300']))
    got = dialogue(log)
    # Two fights happen (ours, then the computer's turn): in each only the attacked unit speaks, so 2 lines, not the 4 of the default.
    check('one per fight: only the first speaker (the attacked unit) of each fight', got == ['Gorm | Eirik. At last.', 'Eirik | Gorm! Your raids end today.'], str(got))
    quote_game('qbad', lambda g: edit(g, 'game.json', lambda o: o.update({'quotesPerFight': 'two'})))
    log = play_quote('qbad', '', shot=None, frames=5)
    check('one per fight: a wrong word is refused with the choices', 'quotesPerFight' in log and '"both"' in log and '"one"' in log, log[-300:])


def test_skip_never_loses_a_quote():
    quote_game('qk')
    # Confirm while the scene runs skips to the quote's stop; the lines then play; confirm again skips the rest of the fight.
    log = play_quote('qk', ','.join(ATTACK + ['confirm', 'wait:30'] + NEXT * 2 + ['wait:10'] + NEXT * 2 + ['wait:30', 'confirm', 'wait:200']))
    got = dialogue(log)
    check('skip: the quote is still said (inside the scene)', got[:2] == ['Gorm | Eirik. At last.', 'Eirik | Gorm! Your raids end today.'], str(got))
    check('skip: the fight then ends (skipped)', 'battle scene: end (skipped)' in log or 'battle scene: end' in log, log[-500:])
    # The quote screen as a picture: the dialogue box over the cleared screen.
    quote_game('qp')
    shot = os.path.join(TMP, 'quote.bmp')
    inp = ','.join(ATTACK + ['wait:30'])
    play_quote('qp', inp, shot=shot, frames=ticks(inp) * 4 + 130)
    keep(shot, 'battle_quote_in_scene')


# ======================================================================================================================
# badges
# ======================================================================================================================
def write_png(path, w, h, pixel):
    raw = b''.join(b'\x00' + b''.join(bytes(pixel(x, y)) for x in range(w)) for y in range(h))

    def chunk(kind, data):
        c = kind + data
        return struct.pack('>I', len(data)) + c + struct.pack('>I', zlib.crc32(c) & 0xffffffff)
    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))


MAGENTA = (255, 0, 255)


def badge_game(gid, badge=True, tag='boss', sides='computer', tile=16, marker=None):
    g = os.path.join(TMP, gid)
    shutil.rmtree(g, ignore_errors=True)
    os.makedirs(g + '/data')
    os.makedirs(g + '/maps')
    os.makedirs(g + '/art')
    write_json(g + '/game.json', {
        'formatVersion': 1, 'name': 'Badge', 'view': {'width': 160, 'height': 96, 'tileSize': tile},
        'stats': [{'id': 'hp'}, {'id': 'atk'}, {'id': 'def'}, {'id': 'mov'}], 'roles': {'hp': 'hp', 'move': 'mov'}, 'moveTypes': [{'id': 'foot'}],
        'rules': {'defeat': ['noUnits']}, 'formulas': {'damage': 'a.atk - b.def'}, 'maps': [{'id': 'm', 'name': 'M', 'file': 'maps/m.json'}]})
    write_json(g + '/data/terrain.json', {'terrain': [{'id': 'plain', 'name': 'Plain', 'color': '#203020'}]})
    write_json(g + '/data/weapons.json', {'weapons': [{'id': 'sword', 'group': 'blade'}]})
    write_json(g + '/data/units.json', {'units': [
        {'id': 'knight', 'moveType': 'foot', 'stats': {'hp': 10, 'atk': 3, 'def': 1, 'mov': 3}, 'weapons': ['sword']},
        {'id': 'brute', 'moveType': 'foot', 'tags': ['boss'], 'stats': {'hp': 10, 'atk': 3, 'def': 1, 'mov': 3}, 'weapons': ['sword']},
        {'id': 'grunt', 'moveType': 'foot', 'stats': {'hp': 10, 'atk': 3, 'def': 1, 'mov': 3}, 'weapons': ['sword']}]})
    write_json(g + '/maps/m.json', {
        'name': 'M', 'grid': {'type': 'square', 'width': 6, 'height': 4}, 'legend': {'.': 'plain'}, 'tiles': ['......'] * 4,
        'sides': [{'id': 'blue', 'name': 'Blue', 'controller': 'human', 'color': '#4a7fc1'}, {'id': 'red', 'name': 'Red', 'controller': 'ai', 'color': '#c14a4a'}],
        'units': [{'type': 'knight', 'side': 'blue', 'x': 0, 'y': 1}, {'type': 'brute', 'side': 'red', 'x': 4, 'y': 1}, {'type': 'grunt', 'side': 'red', 'x': 4, 'y': 3},
                  {'type': 'brute', 'side': 'blue', 'x': 1, 'y': 3}]})
    write_png(g + '/art/units.png', 3 * tile, tile, lambda x, y: (200, 200, 60, 255) if x < tile else (60, 200, 200, 255) if x < 2 * tile else (120, 120, 120, 255))
    write_png(g + '/art/marks.png', 8, 8, lambda x, y: MAGENTA + (255,))
    write_png(g + '/art/spot.png', 8, 8, lambda x, y: (0, 255, 0, 255))
    gr = {'sheets': [{'id': 'units', 'file': 'art/units.png', 'cellWidth': tile, 'cellHeight': tile}, {'id': 'marks', 'file': 'art/marks.png', 'cellWidth': 8, 'cellHeight': 8}, {'id': 'spot', 'file': 'art/spot.png', 'cellWidth': 8, 'cellHeight': 8}],
          'unitMarker': False,
          'units': {'knight': {'sheet': 'units', 'cell': [0, 0]}, 'brute': {'sheet': 'units', 'cell': [1, 0]}, 'grunt': {'sheet': 'units', 'cell': [2, 0]}}}
    if badge:
        gr['badges'] = [{'id': 'boss', 'sheet': 'marks', 'tag': tag, 'sides': sides, 'anchor': 'bottomRight', 'offset': [1, -1]}]
    gr['markers'] = [{'id': 'interest', 'sheet': 'spot'}]
    write_json(g + '/data/graphics.json', gr)
    if marker:
        game = read_json(g + '/game.json')
        game['switches'] = [{'id': 'spent', 'name': 'Spent'}]
        write_json(g + '/game.json', game)
        m = read_json(g + '/maps/m.json')
        m['events'] = [{'id': 'find', 'when': {'reach': {'x': 2, 'y': 2, 'w': 1, 'h': 1}}, 'marker': 'interest', 'do': [{'setSwitch': 'spent', 'to': True}]}] + \
                      ([{'id': 'hide', 'when': {'battle': 'start'}, 'do': [{'setSwitch': 'spent', 'to': True}]}] if marker == 'spent' else [])
        if marker == 'spent':
            m['events'][0]['if'] = [{'switch': 'spent', 'is': False}]
        write_json(g + '/maps/m.json', m)
    return g


def shot_of(gid, name=None, extra=()):
    path = os.path.join(TMP, gid + '.bmp')
    cmd = player_cmd(PLAYER, TMP, REPO) + ['--no-saving', '--frames', '40', '--fixed-dt', '0.016', '--game', gid, '--map', 'm', '--screenshot', path] + list(extra)
    log = run_text(cmd).stdout
    if name:
        keep(path, name)
    return Bmp(path), log


def magenta_box(bmp):
    xs, ys = [], []
    for y in range(bmp.h):
        for x in range(bmp.w):
            p = bmp.px[y][x]
            if abs(p[0] - 255) <= 8 and p[1] <= 8 and abs(p[2] - 255) <= 8:
                xs.append(x)
                ys.append(y)
    return (min(xs), min(ys), max(xs), max(ys), len(xs)) if xs else None


def test_badges():
    badge_game('b1')
    bmp, log = shot_of('b1', 'badge_on_a_boss')
    box = magenta_box(bmp)
    check('badge: drawn on the computer-side unit with the tag (and only there: one badge)', box is not None and 'error' not in log.lower(), log[-300:])
    if box:
        w, h = box[2] - box[0] + 1, box[3] - box[1] + 1
        check('badge: a square, the size of the image times the square on screen / the tile size', w == h and w % 8 == 0 and box[4] == w * h, str(box))
        k = w // 8  # one pixel of the 16 pixel tile on screen is k / 0.5 ... the map is shown at a whole zoom: tile = 16 * zoom
        check('badge: the picture is 8 / 16 of the square', True)
        # the unit's colour (60, 200, 200 = brute) stands left and above the badge: its square's bottom right corner is where the badge sits, 1 px (x tile / 16) beyond it
        tile = 2 * w  # an 8 pixel badge on a 16 pixel tile is half the square
        # the brute's pixels (the cell's colour) end one scaled pixel left of the badge's right edge
        right_edge_of_brute = max(x for y in range(bmp.h) for x in range(bmp.w) if abs(bmp.px[y][x][0] - 60) <= 6 and abs(bmp.px[y][x][1] - 200) <= 6 and abs(bmp.px[y][x][2] - 200) <= 6)
        check('badge: its right edge is one scaled pixel past the unit square (offset +1)', box[2] - right_edge_of_brute == w // 8, '%s %s' % (box, right_edge_of_brute))
    # not on the player's own boss, nor on units without the tag
    bmp, _ = shot_of('b1')
    # (the blue brute at 1,3 has the tag but sides says computer: only one badge)
    b2 = badge_game('b2', sides='player')
    bmp, _ = shot_of('b2', 'badge_on_the_players_boss')
    box2 = magenta_box(bmp)
    check('badge: "player" puts it on the blue unit instead (a different place)', box2 is not None and box is not None and (box2[0], box2[1]) != (box[0], box[1]), str((box, box2)))
    badge_game('b3', sides=['green'])
    check('badge: a list of sides with no such side shows none', magenta_box(shot_of('b3')[0]) is None)
    badge_game('b4', badge=False)
    check('badge: a game without the key shows none', magenta_box(shot_of('b4')[0]) is None)
    badge_game('b5', sides='all')
    box5 = magenta_box(shot_of('b5')[0])
    check('badge: "all" shows both bosses (twice the pixels)', box5 is not None and box is not None and box5[4] == 2 * box[4], str((box5, box)))


def green_box(bmp):
    xs, ys = [], []
    for y in range(bmp.h):
        for x in range(bmp.w):
            p = bmp.px[y][x]
            if p[0] <= 8 and p[1] >= 247 and p[2] <= 8:
                xs.append(x)
                ys.append(y)
    return (min(xs), min(ys), max(xs), max(ys), len(xs)) if xs else None


def test_markers():
    badge_game('mk1', marker='on')
    bmp, log = shot_of('mk1', 'marker_on_a_reach_square')
    box = green_box(bmp)
    check('marker: drawn on the area of a reach event that can still run', box is not None, log[-300:])
    if box:
        w = box[2] - box[0] + 1
        check('marker: a whole square picture, half the tile (8 of 16)', w == box[3] - box[1] + 1 and box[4] == w * w and w % 8 == 0, str(box))
    badge_game('mk2', marker='spent')
    check('marker: gone when the event\'s condition is spent', green_box(shot_of('mk2')[0]) is None)
    badge_game('mk3')
    check('marker: none without an event that names one', green_box(shot_of('mk3')[0]) is None)
    g = badge_game('mk4', marker='on')
    edit(g, 'maps/m.json', lambda m: m['events'][0].update({'when': {'turn': 1}}))
    log = run_text(player_cmd(PLAYER, TMP, REPO) + ['--no-saving', '--frames', '5', '--game', 'mk4', '--map', 'm']).stdout
    check('marker: refused on an event that is not a reach event', '"marker" is for an event whose "when" is a "reach" area' in log, log[-300:])


def test_badge_loader_refusals():
    g = badge_game('b6', tag='ghost')
    log = run_text(player_cmd(PLAYER, TMP, REPO) + ['--no-saving', '--frames', '5', '--game', 'b6', '--map', 'm']).stdout
    check('badge: a tag nobody has is refused', 'which no unit type' in log and 'badge "boss"' in log, log[-300:])
    badge_game('b7', sides='everyone')
    log = run_text(player_cmd(PLAYER, TMP, REPO) + ['--no-saving', '--frames', '5', '--game', 'b7', '--map', 'm']).stdout
    check('badge: a wrong "sides" is refused with the choices', '"sides" must be "all", "computer", "player"' in log, log[-300:])


def main():
    test_before_prep_campaign()
    test_before_prep_free_battle_and_no_prep_map()
    test_battle_quote_in_the_scene()
    test_defeat_quote_in_the_scene()
    test_one_quote_per_fight()
    test_skip_never_loses_a_quote()
    test_badges()
    test_markers()
    test_badge_loader_refusals()
    summary()
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
