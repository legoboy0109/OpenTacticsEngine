#!/usr/bin/env python3
"""The side-view battle scene through the real player, windowless (docs/BATTLE_SCENE.md).

  python3 tests/battle_scene_player_test.py ./build/engine/player/ote-player .
  python3 tests/battle_scene_player_test.py ./build/engine/player/ote-player . --shots OUT_DIR     # keep screenshots (BMP)
  python3 tests/battle_scene_player_test.py ./build/engine/player/ote-player . --baseline OTHER_PLAYER   # on-map log vs another build

Copies tests/fixtures/srpg-quotes into a temp folder, gives it battle art made by tools/art/battle_art.py (a "knight" animation
set for the hero type, none for the brute: that one falls back to its map picture) and runs the player headless (no audio, no
window, no settings, saves in the temp folder). The scene writes "[info] battle scene: ..." lines for its beats:

  - scene mode: the animation set and the fallback, strikes / hits / counters in order, the quote before and the defeat quote
    after, the level-up after, skipping, the computer's fight, a ranged fight with an effect, the phone-sized screen;
  - the setting: command line, settings file, per-game override and the game's own default;
  - on-map mode: no scene lines at all, and (with --baseline) the log identical to another build's.
"""
import json
import os
import re
import shutil
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import (  # noqa: E402
    run_text, check, edit, failures, player_and_repo, read_jsonc, shots_dir, summary, ticks, write_json)
from ote_testkit import scratch_dir  # noqa: E402
read_json = read_jsonc  # this script reads fixtures and games whose JSON has `//` comment lines

sys.dont_write_bytecode = True

PLAYER, REPO = player_and_repo()
SHOTS = shots_dir(absolute=True)
BASELINE = os.path.abspath(sys.argv[sys.argv.index('--baseline') + 1]) if '--baseline' in sys.argv else ''
TMP = scratch_dir()

sys.path.insert(0, os.path.join(REPO, 'tools', 'art'))
import battle_art  # noqa: E402  (our own generator: original pixel art, no Pillow)

IDLE = 'wait:14'
ATTACK = [IDLE, 'confirm', 'wait:3', 'confirm', 'wait:3', 'confirm', 'wait:3', 'confirm']
NEXT = ['confirm'] * 3


def small_hp(g):  # the brutes have two hit points: Eirik's first hit ends Gorm
    edit(g, 'data/units.json', lambda u: [x.update({'stats': dict(x['stats'], hp=2)}) for x in u['units'] if x['id'] == 'brute'])


def xp(g):  # a kill is worth a whole level
    edit(g, 'game.json', lambda gj: gj['rules'].update({'progression': {'enabled': True, 'xpPerLevel': 100, 'xpHit': '10', 'xpKill': '100'}}))


def no_quotes(g):
    def strip(c):
        for ch in c['characters']:
            ch.pop('quotes', None)
    edit(g, 'data/characters.json', strip)


def ranged(g):  # the sword reaches two squares; Gorm stands two squares from Eirik
    edit(g, 'data/weapons.json', lambda w: w['weapons'][0].update({'range': [1, 2]}))
    edit(g, 'maps/ch1.json', lambda m: [u.update({'x': 5, 'y': 1}) for u in m['units'] if u.get('character') == 'gorm'])
    edit(g, 'maps/ch1.json', lambda m: [u.update({'x': 6, 'y': 0}) for u in m['units'] if u.get('character') == 'tolly'])


def phone(g):
    edit(g, 'game.json', lambda gj: gj.update({'view': {'width': 270, 'height': 480, 'tileSize': 24}}))


ARROW = [{'group': 'blade', 'sheet': 'battle_fx', 'frames': [[0, 0], [1, 0]], 'ms': 80, 'mode': 'travel', 'travelMs': 220}]


RED_SHEET = {'id': 'battle_knight_red', 'file': 'art/battle_knight_red.png', 'cellWidth': 48, 'cellHeight': 48}


def knight_set(**more):
    """A copy of the demo knight set, with extra keys (a set's "sides", for one)."""
    s = json.loads(json.dumps(battle_art.BATTLE_SET['sets']['knight']))
    s.update(more)
    return s


def make_game(gid, *tweaks, scene=None, art=True, battle=None, effects=None, sheets=()):
    g = os.path.join(TMP, gid)
    shutil.copytree(os.path.join(REPO, 'tests/fixtures/srpg-quotes'), g)
    if art:
        os.makedirs(os.path.join(g, 'art'))
        battle_art.main(os.path.join(g, 'art'))
        units = battle_art.Canvas(24 * 4, 24)  # the map pictures: a coloured little figure per unit type
        for i, col in enumerate(((70, 110, 200, 255), (200, 80, 70, 255), (90, 170, 90, 255), (190, 160, 70, 255))):
            fig = battle_art.Canvas(24, 24)
            fig.rect(8, 6, 15, 21, col)
            fig.rect(9, 2, 14, 6, (232, 192, 150, 255))
            fig.outline()
            units.paste(fig, i * 24, 0)
        battle_art.write_png(os.path.join(g, 'art', 'units.png'), units)
        gfx = {
            'sheets': battle_art.BATTLE_SHEETS + [{'id': 'units', 'file': 'art/units.png', 'cellWidth': 24, 'cellHeight': 24}] + list(sheets),
            'units': {'hero': {'sheet': 'units', 'cell': [0, 0]}, 'brute': {'sheet': 'units', 'cell': [1, 0]},
                      'rider': {'sheet': 'units', 'cell': [2, 0]}, 'grunt': {'sheet': 'units', 'cell': [3, 0]}},
            'battle': battle or {
                'sets': battle_art.BATTLE_SET['sets'],
                'assign': [{'set': 'knight', 'type': 'hero'}],
                'effects': effects or [],
                'backgrounds': battle_art.BATTLE_SET['backgrounds'],
                'defaultBackground': 'meadow'}}
        write_json(os.path.join(g, 'data', 'graphics.json'), gfx)
    if scene:
        edit(g, 'game.json', lambda gj: gj.update({'battleScene': scene}))
    for t in tweaks:
        t(g)
    return g


def run(gid, inp, extra=('--battle-scene', 'scene'), shot=None, interval=4, frames=None, player=None, settings=None):
    work = os.path.join(TMP, 'saves_' + gid)
    shutil.rmtree(work, ignore_errors=True)
    cmd = [player or PLAYER, '--games', TMP, '--assets', os.path.join(REPO, 'assets'), '--headless', '--no-audio',
           '--saves', work, '--frames', str(frames or ticks(inp) * interval + 90), '--input-interval', str(interval),
           '--fixed-dt', '0.016', '--game', gid, '--map', 'ch1']
    cmd += ['--settings', settings] if settings else ['--no-settings']
    if inp:
        cmd += ['--input', inp]
    if shot:
        cmd += ['--screenshot', shot]
    p = run_text(cmd + list(extra))
    return p.stdout


def scene_lines(log):
    return [m.group(1) for m in re.finditer(r'\[info\] battle scene: (.*)', log)]


def said(log):
    return [m.group(1) for m in re.finditer(r'\[info\] dialogue: (.*)', log)]


def keep(shot_path, name):
    if SHOTS and os.path.exists(shot_path):
        os.makedirs(SHOTS, exist_ok=True)
        shutil.copy(shot_path, os.path.join(SHOTS, name + '.bmp'))


def saved_units(gid):
    try:
        return read_json(os.path.join(TMP, 'saves_' + gid, gid, 'continue.json'))['battle']['units']
    except (OSError, ValueError, KeyError):
        return []


def index_of(log, needle, start=0):
    return log.find(needle, start)


def test_scene_order_quotes_levelup():
    make_game('s_kill', small_hp, xp, lambda g: os.remove(g + '/data/campaign.json'))  # no campaign: a single battle writes its Continue save
    # The scene opens, the two battle quote lines are said inside it (before the first strike), the strike and the hit play, the defeat quote (two lines) is said
    # inside it after the bar has emptied, then the fall; the level-up box comes after the scene (docs/BATTLE_SCENE.md "Quotes in the scene").
    LINE = ['wait:20', 'confirm', 'wait:4']
    inp = ','.join(ATTACK + LINE * 2 + ['wait:90'] + LINE * 2 + ['wait:30'] + ['confirm'] * 3 + ['wait:30'])
    log = run('s_kill', inp)
    sc = scene_lines(log)
    check('scene: starts with both fighters and their art', sc[:3] == ['start Eirik (Lv 1) vs Gorm (Lv 1), melee, 1 strike(s)', 'Eirik art: set "knight"', 'Gorm art: map picture'], str(sc[:4]))
    want = ['quote (battle)', 'quote over', 'strike Eirik -> Gorm', 'hit Eirik -> Gorm 2', 'quote (defeat)', 'quote over', 'defeated Gorm', 'end']
    check('scene: the quote, the strike, the hit, the defeat quote, the fall, the end, in order', sc[3:] == want or sc[3:][:len(want)] == want, str(sc))
    q1 = index_of(log, 'dialogue: Gorm | Eirik. At last.')
    q2 = index_of(log, 'dialogue: Eirik | Gorm! Your raids end today.')
    s0 = index_of(log, 'battle scene: start')
    s1 = index_of(log, 'battle scene: end')
    d1 = index_of(log, 'dialogue: Gorm | Impossible...')
    d2 = index_of(log, 'dialogue: Mira | Brother!')
    levelled = [u for u in saved_units('s_kill') if u.get('name') == 'Eirik']
    check('order: the scene opens, then the battle quotes, then the first strike', 0 <= s0 < q1 < q2 < index_of(log, 'battle scene: strike'), '%d %d %d' % (q1, q2, s0))
    check('order: the hit, then the defeat quote and the friend\'s answer, then the fall', 0 <= index_of(log, 'battle scene: hit') < d1 < d2 < index_of(log, 'battle scene: defeated') < s1, '%d %d %d' % (s1, d1, d2))
    check('after the fight: the experience and the level-up still happen (Eirik is Lv 2 in the Continue save)',
          levelled and levelled[0].get('level') == 2, str(levelled) + log[-400:])
    check('the map does not play the strike a second time (no map floats logged as strikes)', log.count('battle scene: start') == 1)


def formulas(g):  # hit and crit chances exist in this game: the boxes show them
    edit(g, 'game.json', lambda gj: gj['formulas'].update({'hit': '80', 'crit': '10'}))


def test_chances_from_the_forecast():
    make_game('s_chance', no_quotes, formulas)
    log = run('s_chance', ','.join(ATTACK + ['wait:120']))
    sc = scene_lines(log)
    check('chances: the player\'s attack shows its hit and crit percentages (taken before the fight was applied)',
          'Eirik chances: hit 80% crit 10%' in sc and 'Gorm chances: hit 80% crit 10%' in sc, str(sc))
    make_game('s_chance_ai', no_quotes, formulas)
    log = run('s_chance_ai', ','.join([IDLE, 'menu', 'confirm', 'wait:400']))
    check('chances: so does a fight the computer starts', any(s.endswith('chances: hit 80% crit 10%') for s in scene_lines(log)), str(scene_lines(log)))
    make_game('s_nochance', no_quotes)
    check('chances: a game without hit / crit formulas shows none', not any('chances' in s for s in scene_lines(run('s_nochance', ','.join(ATTACK + ['wait:120'])))))


def test_counter_crit_and_log():
    make_game('s_counter', no_quotes)
    log = run('s_counter', ','.join(ATTACK + ['wait:120']))
    sc = scene_lines(log)
    strikes = [s for s in sc if s.startswith('strike ')]
    check('counter: the attack, then the foe\'s counter', strikes[:2] == ['strike Eirik -> Gorm', 'strike Gorm -> Eirik (counter)'], str(sc))
    check('counter: every strike ends in a hit or a miss', sum(1 for s in sc if s.split(' ')[0] in ('hit', 'miss', 'crit')) == len(strikes), str(sc))
    check('counter: the scene ends once', sc.count('end') == 1 and sc[-1] == 'end', str(sc))


def test_skip():
    make_game('s_skip', small_hp, no_quotes)
    log = run('s_skip', ','.join(ATTACK + ['wait:6', 'confirm', 'wait:60']))
    sc = scene_lines(log)
    check('skip: confirm ends the scene at once but still reaches the result', sc[-1] == 'end (skipped)' and 'defeated Gorm' in sc, str(sc))
    log = run('s_skip', ','.join(ATTACK + ['wait:6', 'tap:100:100', 'wait:60']))
    check('skip: a tap does the same', scene_lines(log)[-1] == 'end (skipped)', str(scene_lines(log)))


def test_computer_turn():
    make_game('s_ai', no_quotes)
    inp = ','.join([IDLE, 'menu', 'confirm', 'wait:400'])
    log = run('s_ai', inp)
    sc = scene_lines(log)
    turn = index_of(log, 'ai: red acts')
    start = index_of(log, 'battle scene: start')
    check('computer: its fight uses the same scene', 0 <= turn < start and any(s.startswith('strike ') for s in sc), str(sc) + log[-300:])
    check('computer: it is its unit that strikes first, and the scene ends', sc and [s for s in sc if s.startswith('strike ')][0].split(' -> ')[0].split(' ')[1] in ('Gorm', 'Tolly', 'Grunt', 'grunt') and 'end' in sc, str(sc))


def test_ranged_and_effect():
    make_game('s_range', no_quotes, ranged, effects=ARROW)
    log = run('s_range', ','.join(ATTACK + ['wait:120']))
    sc = scene_lines(log)
    check('ranged: the scene says it is a ranged fight', sc and 'ranged, 2 squares' in sc[0], str(sc))


def with_battle(**parts):
    """A battle section made of the demo's backgrounds plus `parts` (sets, assign, ...)."""
    b = {'backgrounds': battle_art.BATTLE_SET['backgrounds'], 'defaultBackground': 'meadow', 'effects': []}
    b.update(parts)
    return b


def test_side_sheets():
    # The red side's fighter (Gorm) draws the set from the red sheet; the blue one (Eirik) from the set's own. The log names the
    # sheet only when a side's own is used, so a game without side sheets logs as before.
    battle = with_battle(sets={'knight': knight_set(sides={'red': {'sheet': 'battle_knight_red'}})},
                         assign=[{'set': 'knight', 'type': 'hero'}, {'set': 'knight', 'type': 'brute'}])
    make_game('b_sides', no_quotes, battle=battle, sheets=[RED_SHEET])
    sc = scene_lines(run('b_sides', ','.join(ATTACK + ['wait:120'])))
    check('side sheets: both fighters use the set', 'Eirik art: set "knight"' in sc and 'Gorm art: set "knight"' in sc, str(sc))
    check('side sheets: the red fighter draws from the red sheet, named in the log', 'Gorm sheet: "battle_knight_red" (side red)' in sc, str(sc))
    check('side sheets: the blue fighter keeps the set\'s own sheet (no line)', not any(s.startswith('Eirik sheet') for s in sc), str(sc))
    # One animation of its own for a side, the rest from the set-level entry or the base.
    battle = with_battle(sets={'knight': knight_set()}, assign=[{'set': 'knight', 'type': 'hero'}, {'set': 'knight', 'type': 'brute'}])
    battle['sets']['knight']['animations']['stand']['sides'] = {'red': {'sheet': 'battle_knight_red'}}
    make_game('b_sides_anim', no_quotes, battle=battle, sheets=[RED_SHEET])
    sc = scene_lines(run('b_sides_anim', ','.join(ATTACK + ['wait:120'])))
    check('side sheets: an animation\'s own "sides" entry works the same', 'Gorm sheet: "battle_knight_red" (side red)' in sc, str(sc))
    make_game('b_sides_none', no_quotes, battle=with_battle(sets={'knight': knight_set()}, assign=[{'set': 'knight', 'type': 'brute'}]), sheets=[RED_SHEET])
    check('side sheets: with none given the log has no sheet line',
          not any('sheet' in s for s in scene_lines(run('b_sides_none', ','.join(ATTACK + ['wait:120'])))))
    # Assigning by side: the red side's brute gets its set through a "side" rule (a type rule would lose to nothing here, so use side alone).
    battle = with_battle(sets={'knight': knight_set(), 'lancer': knight_set()}, assign=[{'set': 'knight', 'type': 'hero'}, {'set': 'lancer', 'side': 'red'}])
    make_game('b_side_rule', no_quotes, battle=battle)
    sc = scene_lines(run('b_side_rule', ','.join(ATTACK + ['wait:120'])))
    check('assign by side: a rule naming only the side gives the red fighter its set', 'Gorm art: set "lancer"' in sc and 'Eirik art: set "knight"' in sc, str(sc))


def test_weapon_rule():
    # Eirik strikes with a sword: the rule for that very weapon beats the plain rule for his unit type.
    battle = with_battle(sets={'knight': knight_set(), 'swordsman': knight_set()},
                         assign=[{'set': 'knight', 'type': 'hero'}, {'set': 'swordsman', 'weapon': 'sword'}])
    make_game('b_weapon', no_quotes, battle=battle)
    sc = scene_lines(run('b_weapon', ','.join(ATTACK + ['wait:120'])))
    check('weapon rule: the sword rule wins over the unit type rule (both fighters swing a sword)',
          'Eirik art: set "swordsman"' in sc and 'Gorm art: set "swordsman"' in sc, str(sc))
    # A character rule is still stronger than a weapon rule.
    battle['assign'].append({'set': 'knight', 'character': 'eirik'})
    make_game('b_weapon2', no_quotes, battle=battle)
    sc = scene_lines(run('b_weapon2', ','.join(ATTACK + ['wait:120'])))
    check('weapon rule: a character rule beats it', 'Eirik art: set "knight"' in sc and 'Gorm art: set "swordsman"' in sc, str(sc))


def test_long_animation():
    # An attack of 100 frames at 30 ms is 3 seconds long: its hit frame is the last one, so the hit lands after about 3.4 s of
    # the scene (the player's clock is fixed at 16 ms per frame, so a run of a given number of frames is exactly that long).
    battle = with_battle(sets={'knight': knight_set()}, assign=[{'set': 'knight', 'type': 'hero'}])
    battle['sets']['knight']['animations']['attack'] = {'sheet': 'battle_knight', 'frames': [[i % 4, 1] for i in range(100)], 'ms': 30}
    make_game('b_long', no_quotes, battle=battle)
    base = ticks(','.join(ATTACK)) * 4
    early = scene_lines(run('b_long', ','.join(ATTACK), frames=base + 120))
    late = scene_lines(run('b_long', ','.join(ATTACK + ['wait:400']), frames=base + 700))
    check('long animation: 1.9 s into the scene the blow is under way but has not landed', 'strike Eirik -> Gorm' in early and not any(s.startswith('hit ') for s in early), str(early))
    check('long animation: later it lands and the scene ends', any(s.startswith('hit Eirik -> Gorm') for s in late) and late[-1] == 'end', str(late))


def test_phone_and_background():
    make_game('s_phone', no_quotes, phone)
    log = run('s_phone', ','.join(ATTACK + ['wait:120']))
    sc = scene_lines(log)
    check('phone: the scene runs at 270 x 480', sc and sc[0].startswith('start Eirik') and sc[-1] == 'end', str(sc) + log[-200:])


def test_setting():
    # the game asks for the scene: every fight is one
    make_game('t_game', no_quotes, scene='scene')
    log = run('t_game', ','.join(ATTACK + ['wait:100']), extra=())
    check('setting: a game that asks for the scene shows it', bool(scene_lines(log)), log[-200:])
    # an existing game (no key): on the map
    make_game('t_none', no_quotes)
    log = run('t_none', ','.join(ATTACK + ['wait:100']), extra=())
    check('setting: no key means on the map, no scene at all', not scene_lines(log) and 'forecast:' in log, log[-200:])
    # the player's global setting beats the game
    sf = os.path.join(TMP, 'settings_global.json')
    write_json(sf, {'version': 1, 'global': {'battleScene': 'scene'}})
    log = run('t_none', ','.join(ATTACK + ['wait:100']), extra=(), settings=sf)
    check('setting: the global player setting turns the scene on in a map game', bool(scene_lines(log)), log[-200:])
    write_json(sf, {'version': 1, 'global': {'battleScene': 'map'}})
    log = run('t_game', ','.join(ATTACK + ['wait:100']), extra=(), settings=sf)
    check('setting: ... and "On the map" turns it off in a scene game', not scene_lines(log), log[-200:])
    # this game's own override beats the global one
    write_json(sf, {'version': 1, 'global': {'battleScene': 'map'}, 'games': {'t_none': {'battleScene': 'scene'}}})
    log = run('t_none', ','.join(ATTACK + ['wait:100']), extra=(), settings=sf)
    check('setting: a per-game override beats the global setting', bool(scene_lines(log)), log[-200:])
    # the command line beats everything
    log = run('t_none', ','.join(ATTACK + ['wait:100']), extra=('--battle-scene', 'map'), settings=sf)
    check('setting: the command line beats the settings file', not scene_lines(log), log[-200:])


def normal(log, dirs):
    for d in dirs:
        log = log.replace(d, '<DIR>')
    return log


def test_on_map_unchanged():
    # No battle art at all in this copy: the plain fixture, on the map, must log exactly what the other build logs.
    g = os.path.join(TMP, 'm_map')
    shutil.copytree(os.path.join(REPO, 'tests/fixtures/srpg-quotes'), g)
    inp = ','.join(ATTACK + NEXT * 3 + ['wait:60', 'menu', 'confirm', 'wait:200'])
    log = run('m_map', inp, extra=('--battle-scene', 'map'))
    check('on the map: no scene line', not scene_lines(log) and 'forecast:' in log)
    log_default = run('m_map', inp, extra=())
    check('on the map: the default is the same as asking for it', normal(log, [TMP]) == normal(log_default, [TMP]))
    if BASELINE:
        base = run('m_map', inp, extra=(), player=BASELINE)
        a, b = normal(log_default, [TMP]), normal(base, [TMP])
        check('on the map: the log is identical to the build without the battle scene', a == b,
              '\n'.join(l for l in a.splitlines() if l not in set(b.splitlines()))[:600])
    else:
        print('SKIP on the map: identical to another build (no --baseline given)')


def test_screenshots():
    def shot(name, gid, frames, extra=('--battle-scene', 'scene')):
        path = os.path.join(TMP, name + '.bmp')
        inp = ','.join(ATTACK + ['wait:400'])
        run(gid, inp, extra=extra, shot=path, frames=frames)
        keep(path, name)
        check('screenshot %s exists' % name, os.path.exists(path))

    make_game('p_land', no_quotes, formulas)
    make_game('p_fall', no_quotes, formulas, art=False)
    make_game('p_ranged', no_quotes, formulas, ranged, effects=ARROW)
    make_game('p_port', no_quotes, formulas, phone)
    make_game('p_portr', no_quotes, formulas, ranged, phone, effects=ARROW)
    make_game('p_portf', no_quotes, formulas, phone, art=False)
    for frames in (int(x) for x in (os.environ.get('SHOT_FRAMES') or '150,175,200').split(',')):
        shot('scene-landscape-%d' % frames, 'p_land', frames)
        shot('scene-landscape-fallback-%d' % frames, 'p_fall', frames)
        shot('scene-landscape-ranged-%d' % frames, 'p_ranged', frames)
        shot('scene-phone-%d' % frames, 'p_port', frames)
        shot('scene-phone-ranged-%d' % frames, 'p_portr', frames)
        shot('scene-phone-fallback-%d' % frames, 'p_portf', frames)


try:
    test_scene_order_quotes_levelup()
    test_chances_from_the_forecast()
    test_counter_crit_and_log()
    test_skip()
    test_computer_turn()
    test_ranged_and_effect()
    test_side_sheets()
    test_weapon_rule()
    test_long_animation()
    test_phone_and_background()
    test_setting()
    test_on_map_unchanged()
    if SHOTS:
        test_screenshots()
finally:
    shutil.rmtree(TMP, ignore_errors=True)

summary()
sys.exit(1 if failures else 0)
