#!/usr/bin/env python3
"""Fidelity 2 through the real player, windowless (docs/EVENTS.md "A staged fight", docs/BATTLE_SCENE.md, docs/ECONOMY.md "Weather and movement cost").

  python3 tests/srpg_player_fidelity2_test.py ./build/engine/player/ote-player .
  python3 tests/srpg_player_fidelity2_test.py ./build/engine/player/ote-player . --shots OUT_DIR     # keep screenshots (BMP)

Writes small games into a temp folder (the repository is never changed; the srpg-quotes fixture is copied) and plays them with --headless,
--no-audio, --no-settings and saves in the temp folder. Asserted from the player's "[info] ..." log lines:
  * a staged fight (`fight` with `strikes`) plays in the battle scene with exactly the written hits, misses and crits, the defeat, and a crit
    with no damage written is three times the normal hit (no dice);
  * the scene's backdrop comes from the map's own table by the attacker's terrain with "battleBackgroundFrom": "attacker", and from the
    defender's by default; a platform is used (the far one at range 2+);
  * weather changes what a terrain costs: a computer unit crosses a swamp in one turn in clear weather and only after the story clears the
    rain when it rains (the map's own weather, then the story's `weather` command);
  * "battleSceneSides": a player's attack and the computer's attack under "playerRight" / "playerLeft", and the map rule by default;
  * screenshots of a staged fight's scene with platforms at 480 x 270 and 270 x 480 (with --shots).
"""
import json
import os
import re
import shutil
import sys
import tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import (  # noqa: E402
    run_text, check, edit, failures, player_and_repo, read_jsonc, shots_dir, summary, write_json)
read_json = read_jsonc  # this script reads fixtures and games whose JSON has `//` comment lines

sys.dont_write_bytecode = True

PLAYER, REPO = player_and_repo()
SHOTS = shots_dir(absolute=True)
TMP = tempfile.mkdtemp(prefix='ote-fid2-')

sys.path.insert(0, os.path.join(REPO, 'tools', 'art'))
import battle_art  # noqa: E402  (our own generator: original pixel art, no Pillow)

IDLE = 'wait:14'
ATTACK = [IDLE, 'confirm', 'wait:3', 'confirm', 'wait:3', 'confirm', 'wait:3', 'confirm']


def platform_sheet():
    """48 x 16 cells: a brown disc (cell 0) and a flatter, paler one for fights at range (cell 1)."""
    c = battle_art.Canvas(96, 16)
    for i, (col, rows) in enumerate((((120, 84, 52, 255), 7), ((170, 150, 110, 255), 4))):
        for y in range(8 - rows // 2 - 1, 8 + rows // 2 + 1):
            half = 22 - abs(y - 8) * 2
            c.rect(i * 48 + 24 - half, y, i * 48 + 24 + half, y, col)
    c.outline()
    return c


def make_game(gid, *tweaks, scene='scene'):
    g = os.path.join(TMP, gid)
    shutil.copytree(os.path.join(REPO, 'tests/fixtures/srpg-quotes'), g)
    os.remove(os.path.join(g, 'data', 'campaign.json')) if os.path.exists(os.path.join(g, 'data', 'campaign.json')) else None
    os.makedirs(os.path.join(g, 'art'))
    battle_art.main(os.path.join(g, 'art'))
    battle_art.write_png(os.path.join(g, 'art', 'plat.png'), platform_sheet())
    write_json(os.path.join(g, 'data', 'graphics.json'), {
        'sheets': battle_art.BATTLE_SHEETS + [{'id': 'plat', 'file': 'art/plat.png', 'cellWidth': 48, 'cellHeight': 16}],
        'battle': {
            'sets': battle_art.BATTLE_SET['sets'],
            'assign': [{'set': 'knight', 'type': 'hero'}],
            'backgrounds': {'meadow': {'sheet': 'battle_meadow', 'cell': [0, 0]}, 'night': {'sheet': 'battle_meadow', 'cell': [0, 0]}},
            'defaultBackground': 'meadow',
            'platforms': {'grass': {'sheet': 'plat', 'cell': [0, 0], 'far': [1, 0]}, 'boards': {'sheet': 'plat', 'cell': [1, 0]}},
            'defaultPlatform': 'grass'}})
    edit(g, 'game.json', lambda gj: gj.update({'battleScene': scene}))
    # the fixture's characters speak before and after a fight: a staged fight has no quotes, a real one is kept out of the way
    def no_quotes(c):
        for ch in c['characters']:
            ch.pop('quotes', None)
    edit(g, 'data/characters.json', no_quotes)
    for t in tweaks:
        t(g)
    return g


def staged(*strikes, attacker='eirik', defender='gorm', extra=None):
    def tweak(g):
        fight = {'attacker': attacker, 'defender': defender}
        if strikes:
            fight['strikes'] = list(strikes)
        fight.update(extra or {})
        edit(g, 'maps/ch1.json', lambda m: m.update({'events': [{'id': 'duel', 'when': {'battle': 'start'}, 'do': [{'fight': fight}]}]}))
    return tweak


def fights(*pairs):
    """Several staged fights in a row at the start (attacker, defender, strikes)."""
    def tweak(g):
        do = [{'fight': {'attacker': a, 'defender': d, 'strikes': s}} for a, d, s in pairs]
        edit(g, 'maps/ch1.json', lambda m: m.update({'events': [{'id': 'duels', 'when': {'battle': 'start'}, 'do': do}]}))
    return tweak


def swamp_under_gorm(far=False):
    """Gorm stands on a swamp (a new terrain); `far`: two squares from Eirik (a ranged scene)."""
    def tweak(g):
        edit(g, 'data/terrain.json', lambda t: t['terrain'].append({'id': 'swamp', 'name': 'Swamp'}))
        def m(mp):
            mp['legend']['s'] = 'swamp'
            gx = 5 if far else 4
            row = list(mp['tiles'][1])
            row[gx] = 's'
            mp['tiles'][1] = ''.join(row)
            for u in mp['units']:
                if u.get('character') == 'gorm':
                    u['x'] = gx
        edit(g, 'maps/ch1.json', m)
    return tweak


def map_tables(bg=None, plat=None):
    def tweak(g):
        def m(mp):
            if bg:
                mp['battleBackgrounds'] = bg
            if plat:
                mp['battlePlatforms'] = plat
        edit(g, 'maps/ch1.json', m)
    return tweak


def game_key(**kv):
    return lambda g: edit(g, 'game.json', lambda gj: gj.update(kv))


def phone(g):
    edit(g, 'game.json', lambda gj: gj.update({'view': {'width': 270, 'height': 480, 'tileSize': 24}}))


def run(gid, inp=None, frames=900, shot=None, extra=('--battle-scene', 'scene'), game_root=None, autoplay=False, map_id='ch1', interval=4):
    work = os.path.join(TMP, 'saves_' + gid)
    shutil.rmtree(work, ignore_errors=True)
    cmd = [PLAYER, '--games', game_root or TMP, '--assets', os.path.join(REPO, 'assets'), '--headless', '--no-audio', '--no-settings',
           '--saves', work, '--frames', str(frames), '--fixed-dt', '0.016', '--game', gid, '--map', map_id]
    if inp:
        cmd += ['--input', inp, '--input-interval', str(interval)]
    if autoplay:
        cmd += ['--autoplay']
    if shot:
        cmd += ['--screenshot', shot]
    p = run_text(cmd + list(extra))
    return p.stdout


def scene_lines(log):
    return [m.group(1) for m in re.finditer(r'\[info\] battle scene: (.*)', log)]


def keep(path, name):
    if SHOTS and os.path.exists(path):
        os.makedirs(SHOTS, exist_ok=True)
        shutil.copy(path, os.path.join(SHOTS, name + '.bmp'))


# ---- the staged fight ----

def test_staged_fight():
    make_game('f_exact', staged({'by': 'attacker', 'result': 'hit', 'damage': 5}, {'by': 'defender', 'result': 'miss'},
                                {'by': 'attacker', 'result': 'hit', 'damage': 0}, {'by': 'defender', 'result': 'hit', 'damage': 2}))
    sc = scene_lines(run('f_exact'))
    body = [s for s in sc if s.split(' ')[0] in ('strike', 'hit', 'miss', 'crit', 'defeated', 'end')]
    check('staged fight: the scene shows exactly the written strikes, in order',
          body == ['strike Eirik -> Gorm', 'hit Eirik -> Gorm 5', 'strike Gorm -> Eirik (counter)', 'miss Gorm -> Eirik',
                   'strike Eirik -> Gorm', 'hit Eirik -> Gorm 0', 'strike Gorm -> Eirik (counter)', 'hit Gorm -> Eirik 2', 'end'], str(sc))
    check('staged fight: the start line names both fighters and the strikes', any(s.startswith('start Eirik (Lv 1) vs Gorm (Lv 1), melee, 4 strike(s)') for s in sc), str(sc))
    # No dice: a crit with no damage written is three times the hit with none written (the same numbers, nothing rolled).
    one = scene_lines(run(make_game('f_hit', staged({'by': 'attacker', 'result': 'hit'})) and 'f_hit'))
    two = scene_lines(run(make_game('f_crit', staged({'by': 'attacker', 'result': 'crit'})) and 'f_crit'))
    hit = [int(s.split()[-1]) for s in one if s.startswith('hit Eirik')]
    crit = [int(s.split()[-1]) for s in two if s.startswith('crit Eirik')]
    check('staged fight: a hit with no damage written is the normal damage, a crit three times that',
          len(hit) == 1 and len(crit) == 1 and hit[0] > 0 and crit[0] == 3 * hit[0], '%s %s' % (one, two))
    # (Tolly, not Gorm: Gorm is red's leader and his fall would end the whole battle at once)
    make_game('f_fall', staged({'by': 'attacker', 'result': 'crit', 'damage': 99}, {'by': 'defender', 'result': 'hit'}, defender='tolly'))
    sc = scene_lines(run('f_fall'))
    check('staged fight: a fighter that falls ends it (the answer is not played)', 'defeated Tolly' in sc and not any(s.startswith('strike Tolly') for s in sc), str(sc))
    make_game('f_spare', staged({'by': 'attacker', 'result': 'crit', 'damage': 99}, extra={'lethal': False}))
    sc = scene_lines(run('f_spare'))
    check('staged fight: "lethal": false keeps the fighter standing', 'end' in sc and 'defeated Gorm' not in sc, str(sc))
    make_game('f_real', staged())
    sc = scene_lines(run('f_real'))
    check('a fight without strikes plays real rolls: strikes by Eirik and a counter or a miss, no loader error',
          any(s.startswith('strike Eirik -> Gorm') for s in sc) and sc[-1] == 'end', str(sc))


# ---- the backdrop ----

def test_backdrop():
    tables = map_tables(bg={'plain': 'night', 'default': 'meadow'})
    duel = staged({'by': 'attacker', 'result': 'hit', 'damage': 1})
    make_game('b_def', swamp_under_gorm(), tables, duel)
    sc = scene_lines(run('b_def'))
    check('backdrop: by default the struck fighter\'s terrain decides (Gorm on a swamp: the map\'s default)', 'background "meadow" (terrain swamp)' in sc, str(sc))
    make_game('b_att', swamp_under_gorm(), tables, duel, game_key(battleBackgroundFrom='attacker'))
    sc = scene_lines(run('b_att'))
    check('backdrop: "battleBackgroundFrom": "attacker" uses Eirik\'s plain: the map table\'s entry for it', 'background "night" (terrain plain)' in sc, str(sc))
    make_game('b_game', swamp_under_gorm(), duel)
    sc = scene_lines(run('b_game'))
    check('backdrop: with no map table the game\'s default is used, as before', 'background "meadow" (terrain swamp)' in sc, str(sc))


def test_platforms():
    duel = staged({'by': 'attacker', 'result': 'hit', 'damage': 1})
    make_game('p_near', duel)
    check('platform: the default platform is used', 'platform "grass"' in scene_lines(run('p_near')))
    make_game('p_far', swamp_under_gorm(far=True), duel)
    sc = scene_lines(run('p_far'))
    check('platform: a fight at range 2+ uses the far picture', 'platform "grass" (far)' in sc and any('ranged, 2 squares' in s for s in sc), str(sc))
    make_game('p_map', swamp_under_gorm(), duel, map_tables(plat={'swamp': 'boards'}))
    check('platform: the map\'s own table wins (Gorm\'s swamp: boards)', 'platform "boards"' in scene_lines(run('p_map')))


# ---- weather ----

def make_weather_game(gid, map_weather, clears):
    g = os.path.join(TMP, gid)
    root = g
    write_json(root + '/game.json', {
        'formatVersion': 1, 'name': 'Weather', 'view': {'width': 480, 'height': 270, 'tileSize': 24},
        'stats': [{'id': 'hp', 'name': 'HP'}, {'id': 'atk', 'name': 'Attack'}, {'id': 'def', 'name': 'Defense'}, {'id': 'mov', 'name': 'Move'}],
        'roles': {'hp': 'hp', 'move': 'mov'}, 'moveTypes': [{'id': 'foot', 'name': 'Foot'}],
        'rules': {'defeat': ['noUnits'], 'counterAttacks': True}, 'formulas': {'damage': 'a.atk - b.def', 'hit': '100'},
        'maps': [{'id': 'march', 'name': 'March', 'file': 'maps/march.json'}]})
    write_json(root + '/data/terrain.json', {'terrain': [
        {'id': 'plain', 'name': 'Plain'}, {'id': 'swamp', 'name': 'Swamp', 'moveCost': {'foot': 2}, 'weatherCost': {'rain': {'foot': 5}}}]})
    write_json(root + '/data/weapons.json', {'weapons': [{'id': 'sword', 'name': 'Sword'}]})
    write_json(root + '/data/units.json', {'units': [
        {'id': 'post', 'name': 'Post', 'glyph': 'P', 'moveType': 'foot', 'weapons': ['sword'], 'stats': {'hp': 60, 'atk': 1, 'def': 1, 'mov': 0}},
        {'id': 'raider', 'name': 'Raider', 'glyph': 'R', 'moveType': 'foot', 'weapons': ['sword'], 'stats': {'hp': 10, 'atk': 2, 'def': 1, 'mov': 4}}]})
    events = [
        {'id': 'crossed', 'when': {'reach': {'x': 4, 'y': 0, 'w': 1, 'h': 1}, 'by': {'unit': 'raider'}}, 'do': [{'say': 'raider crossed'}]},
        {'id': 'round2', 'when': {'turn': 2}, 'do': [{'say': 'round two'}] + ([{'weather': 'none'}] if clears else [])}]
    m = {'name': 'March', 'grid': {'type': 'square', 'width': 7, 'height': 1}, 'legend': {'.': 'plain', 's': 'swamp'},
         'tiles': ['....ss.'], 'sides': [{'id': 'blue', 'controller': 'ai'}, {'id': 'red', 'controller': 'ai'}],
         'units': [{'type': 'post', 'id': 'post', 'side': 'blue', 'x': 0, 'y': 0}, {'type': 'raider', 'id': 'raider', 'side': 'red', 'x': 6, 'y': 0}],
         'events': events}
    if map_weather:
        m['weather'] = map_weather
    write_json(root + '/maps/march.json', m)
    return g


def test_weather():
    def lines_of(gid, weather, clears):
        make_weather_game(gid, weather, clears)
        log = run(gid, frames=4000, autoplay=True, extra=(), map_id='march')
        return [re.sub(r'^.*dialogue: ', '', l).strip(' |') for l in log.splitlines() if 'dialogue:' in l], log
    clear, log0 = lines_of('w_clear', None, False)
    check('weather: in clear weather the raider crosses the swamp on its first turn (before round two)',
          'raider crossed' in clear and 'round two' in clear and clear.index('raider crossed') < clear.index('round two'), '%s %s' % (clear, log0[-300:]))
    rain, log1 = lines_of('w_rain', 'rain', True)
    check('weather: in the map\'s rain it cannot (a swamp costs 5 against a move of 4), only after the story clears it',
          'raider crossed' in rain and 'round two' in rain and rain.index('round two') < rain.index('raider crossed'), '%s %s' % (rain, log1[-300:]))
    stuck, log2 = lines_of('w_stuck', 'rain', False)
    check('weather: while the rain lasts it never crosses', 'round two' in stuck and 'raider crossed' not in stuck, '%s %s' % (stuck, log2[-300:]))


# ---- which side each fighter stands on ----

def test_sides():
    def left_right(gid, mode, script):
        make_game(gid, game_key(battleSceneSides=mode) if mode else (lambda g: None))
        sc = scene_lines(run(gid, ','.join(script), frames=1600))
        return [s for s in sc if s.startswith('left ')], sc
    own, sc = left_right('s_map', None, ATTACK + ['wait:200'])
    # (the default says nothing new: the start line already names the left fighter first)
    check('sides: by default the map decides (Eirik at column 3 stands left of Gorm at 4), and no extra line is logged',
          own == [] and any(s.startswith('start Eirik (Lv 1) vs Gorm') for s in sc), str(sc))
    mine, sc = left_right('s_right', 'playerRight', ATTACK + ['wait:200'])
    check('sides: "playerRight": a player\'s attack puts its fighter on the right', mine[:1] == ['left Gorm, right Eirik'], str(sc))
    mine, sc = left_right('s_right_ai', 'playerRight', [IDLE, 'menu', 'confirm', 'wait:500'])
    check('sides: "playerRight": the computer\'s attack puts the computer\'s fighter on the left, the player\'s on the right',
          len(mine) >= 1 and mine[0].startswith('left ') and mine[0].split(', right ')[1] in ('Eirik', 'Bram', 'Mira') and mine[0].split(', right ')[0][5:] in ('Gorm', 'Tolly', 'Grunt', 'grunt'), str(sc))
    mine, sc = left_right('s_left', 'playerLeft', ATTACK + ['wait:200'])
    check('sides: "playerLeft": a player\'s attack puts its fighter on the left', mine[:1] == ['left Eirik, right Gorm'], str(sc))
    mine, sc = left_right('s_left_ai', 'playerLeft', [IDLE, 'menu', 'confirm', 'wait:500'])
    check('sides: "playerLeft": the computer\'s attack puts the computer\'s fighter on the right',
          len(mine) >= 1 and mine[0].split(', right ')[1] in ('Gorm', 'Tolly', 'Grunt', 'grunt'), str(sc))
    # both fighters the player's: the map rule decides, whatever the setting
    make_game('s_same', game_key(battleSceneSides='playerRight'), fights(('eirik', 'bram', [{'by': 'attacker', 'result': 'hit', 'damage': 1}])))
    sc = scene_lines(run('s_same'))
    check('sides: both fighters the player\'s: the map rule (Bram at column 2 is left of Eirik at 3)', 'left Bram, right Eirik' in sc, str(sc))


def test_screenshots():
    duel = staged({'by': 'attacker', 'result': 'hit', 'damage': 3}, {'by': 'defender', 'result': 'hit', 'damage': 2},
                  {'by': 'attacker', 'result': 'crit', 'damage': 7})
    grid = [('shot_land', ()), ('shot_phone', (phone,)), ('shot_land_far', (swamp_under_gorm(far=True),)), ('shot_phone_far', (swamp_under_gorm(far=True), phone)),
            ('shot_land_right', (game_key(battleSceneSides='playerRight'), swamp_under_gorm(), map_tables(plat={'swamp': 'boards'})))]
    for name, tweaks in grid:
        make_game(name, duel, *tweaks)
        path = os.path.join(TMP, name + '.bmp')
        run(name, frames=int(os.environ.get('SHOT_FRAMES', '110')), shot=path)
        keep(path, name)
        check('screenshot %s exists' % name, os.path.exists(path))
    # a scripted player attack under "playerRight" for the picture of the sides
    make_game('shot_sides', game_key(battleSceneSides='playerRight'), swamp_under_gorm(), map_tables(plat={'swamp': 'boards'}))
    path = os.path.join(TMP, 'shot_sides.bmp')
    run('shot_sides', ','.join(ATTACK + ['wait:400']), frames=int(os.environ.get('SIDE_FRAMES', '150')), shot=path)
    keep(path, 'shot_playerRight_real_attack')


try:
    test_staged_fight()
    test_backdrop()
    test_platforms()
    test_weather()
    test_sides()
    if SHOTS:
        test_screenshots()
finally:
    shutil.rmtree(TMP, ignore_errors=True)

summary()
sys.exit(1 if failures else 0)
