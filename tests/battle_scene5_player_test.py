#!/usr/bin/env python3
"""The fifth batch of opt-in battle scene keys through the real player, windowless (docs/BATTLE_SCENE.md).

  python3 tests/battle_scene5_player_test.py ./build/engine/player/ote-player .
  python3 tests/battle_scene5_player_test.py ./build/engine/player/ote-player . --shots OUT_DIR   # keep the screenshots (BMP)

Reuses the generated games and helpers of tests/battle_scene3_player_test.py (a staff scene: Wen casts the Mend Staff on wounded Tarn, a 240 x 160 stage,
plain-colour pictures, an effect "probe" sound whose time in the --audio-log is the clock of the screenshots) and, for the fighting keys, a copy of
tests/fixtures/srpg-quotes in which every blow misses and every frame of every animation is a different colour. Always headless, no sound device,
a temporary games folder.

  1. holdFrame / startFrame / startDelayMs and the scene's resultHoldMs: which frame each fighter shows when, read from the colour of its body;
     leaving the keys out gives the old timing; the next strike waits for the held poses;
  2. an item's "sounds": { "healScene": ... }: its heal sound in the scene only, "heal" keeps meaning both;
  3. "battlePlatformFrom": "own": each fighter's platform from its own terrain (the background still follows "battleBackgroundFrom");
  4. a travel effect's "endOffset" by distance (atRange) and negative (past the target);
  5. "atRangeFixed": a fade row / a screen layer that is not carried along by the distance's delay;
  6. an effect's "panAtMs": the camera of a panning band leaves relative to that effect's start.
"""
import os
import shutil
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import battle_scene3_player_test as s3  # noqa: E402  (the shared generated games; its main() is not run)

Canvas, write_png = s3.Canvas, s3.write_png
STAGE, USE, C_SPARK, C_BL, C_BACK = s3.STAGE, s3.USE, s3.C_SPARK, s3.C_BL, s3.C_BACK
check = s3.check


def near(a, b, tol=3):
    return a is not None and b is not None and abs(a - b) <= tol


# ---- 1. holding a pose ---------------------------------------------------------------------------------------------------------------------

FIG_ROWS, FIG_COLS = 4, 6          # the fighters' sheet: row 0 attack, row 1 dodge, row 2 stand, row 3 hurt; every cell has a body colour of its own
IDLE = 'wait:14'
ATTACK = [IDLE, 'confirm', 'wait:3', 'confirm', 'wait:3', 'confirm', 'wait:3', 'confirm', 'wait:200']  # Eirik attacks Gorm (srpg-quotes, map ch1)


def body_colour(row, col):
    return (20 + 35 * col, 40 + 55 * row, 230 - 25 * col - 20 * row)


def fig_sheet():
    c = Canvas(48 * FIG_COLS, 48 * FIG_ROWS)
    for row in range(FIG_ROWS):
        for col in range(FIG_COLS):
            c.paste(s3.fighter(body_colour(row, col)), col * 48, row * 48)
    return c


def reading(shot, left):
    """Which cell of the fighters' sheet the left or the right fighter shows, as (row, col), from the colour of its body; None when none shows."""
    best, found = 20, None
    x0, x1 = (0, 240) if left else (240, 480)
    for row in range(FIG_ROWS):
        for col in range(FIG_COLS):
            n = shot.count(body_colour(row, col), x0, x1, 60, 220)
            if n > best:
                best, found = n, (row, col)
    return found


def anim_keys(attack_extra, dodge_extra, hurt_extra=None):
    attack = dict({'sheet': 'fig', 'frames': [[0, 0], [1, 0], [2, 0], [3, 0]], 'ms': [200, 200, 200, 100], 'hit': 2}, **attack_extra)
    dodge = dict({'sheet': 'fig', 'frames': [[i, 1] for i in range(6)], 'ms': 100}, **dodge_extra)
    anims = {'stand': {'sheet': 'fig', 'frames': [[0, 2]]}, 'attack': attack, 'dodge': dodge,
             'hurt': dict({'sheet': 'fig', 'frames': [[0, 3], [1, 3], [2, 3]], 'ms': 100}, **(hurt_extra or {}))}
    return {'anchor': [24, 46], 'animations': anims}


def fight_game(gid, attack_extra=None, dodge_extra=None, scene=None):
    """A copy of the srpg-quotes fixture in which every blow misses (hit chance 0): Eirik (right, "playerRight") misses Gorm (left), who counters and misses."""
    g = os.path.join(s3.TMP, gid)
    shutil.copytree(os.path.join(s3.REPO, 'tests/fixtures/srpg-quotes'), g)
    s3.edit(g, 'game.json', lambda gj: (gj.update({'battleScene': 'scene', 'battleSceneSides': 'playerRight'}), gj['formulas'].update({'hit': '0'})))
    s3.edit(g, 'data/characters.json', lambda c: [ch.pop('quotes', None) for ch in c['characters']])
    art = os.path.join(g, 'art')
    os.makedirs(art)
    write_png(os.path.join(art, 'stage_bg.png'), s3.backdrop())
    write_png(os.path.join(art, 'plat.png'), s3.platforms())
    write_png(os.path.join(art, 'fig.png'), fig_sheet())
    write_png(os.path.join(art, 'fx_spark.png'), s3.solid(16, 16, C_SPARK))
    cells = lambda sid, w, h: {'id': sid, 'file': 'art/%s.png' % sid, 'cellWidth': w, 'cellHeight': h}
    battle = {'sets': {'fig': anim_keys(attack_extra or {}, dodge_extra or {})},
              'assign': [{'set': 'fig', 'type': 'hero'}, {'set': 'fig', 'type': 'brute'}],
              'backgrounds': {'arena': {'sheet': 'stage_bg', 'cell': [0, 0]}}, 'defaultBackground': 'arena',
              'platforms': {'slab': {'sheet': 'plat', 'cell': [0, 0]}}, 'defaultPlatform': 'slab',
              # a short effect on the struck fighter whose sound is the clock: it starts at the miss moment
              'effects': [{'group': 'blade', 'sheet': 'fx_spark', 'frames': [[0, 0]], 'ms': 100, 'sounds': {'0': 'probe'}}],
              'stage': STAGE}
    if scene is not None:
        battle['scene'] = scene
    s3.write_json(os.path.join(g, 'data', 'graphics.json'),
                  {'sheets': [cells('stage_bg', 240, 160), cells('plat', 64, 32), cells('fig', 48, 48), cells('fx_spark', 16, 16)], 'battle': battle})
    s3.probe_audio(g)
    return g


def probes(gid):
    snd = []
    log, _ = s3.run_player(gid, ATTACK, None, frames=1000, sounds=snd, map_id='ch1')
    return [t for t, n in snd if n == 'probe'], log


def frames_at(gid, deltas, tag):
    """{delta: (striker's cell, dodger's cell)} of screenshots `delta` seconds after the first probe (the miss moment). The striker is Eirik on the right,
    the dodger Gorm on the left."""
    names = {d: '%s_%+d' % (tag, int(round(d * 1000))) for d in deltas}
    shots, log = s3.timed_shots(gid, ATTACK, None, {names[d]: d for d in deltas}, map_id='ch1')
    return {d: (reading(shots[names[d]], False), reading(shots[names[d]], True)) for d in deltas if names[d] in shots}


STAND, ATK, DDG = (2, 0), 0, 1


def expect(tag, got, want):
    """want: {delta: (striker, dodger)} with each as ('a', frame) / ('d', frame) / 'stand'."""
    cell = lambda w: STAND if w == 'stand' else ((0 if w[0] == 'a' else 1), w[1])
    for d, (ws, wd) in want.items():
        g = got.get(d)
        check('%s, %+d ms: striker shows %s, dodger shows %s' % (tag, int(round(d * 1000)), ws, wd), g == (cell(ws), cell(wd)), 'got %s' % (g,))


def holds():
    # Held (docs/BATTLE_SCENE.md "Holding a pose", the worked example): the striker's blow is on frame 2 at the miss moment M (frames 200, 200, 200, 100 ms), it waits on
    # frame 3 until M + 533 ms; the dodger's brace (frames 0 and 1) plays from the start of the strike and holds frame 1 until M, frame 2 starts at M, frame 3 is held until
    # M + 533 ms, then frames 4 and 5.
    gid = 'hold'
    fight_game(gid, {'holdFrame': 3}, {'startFrame': 2, 'holdFrame': 3})
    got = frames_at(gid, [-0.34, -0.25, -0.1, 0.05, 0.15, 0.3, 0.45, 0.58, 0.68, 0.8], 'hold')
    expect('held', got, {-0.34: (('a', 0), ('d', 0)), -0.25: (('a', 0), ('d', 1)), -0.1: (('a', 1), ('d', 1)), 0.05: (('a', 2), ('d', 2)),
                         0.15: (('a', 2), ('d', 3)), 0.3: (('a', 3), ('d', 3)), 0.45: (('a', 3), ('d', 3)), 0.58: ('stand', ('d', 4)),
                         0.68: ('stand', ('d', 5)), 0.8: ('stand', 'stand')})
    # The next strike (Gorm's counter, which also misses) starts after the held poses have played out: the probes are 1.433 s apart (the dodge ends M + 733 ms, 300 ms gap, 400 ms to the blow).
    pr, log = probes(gid)
    check('held: both strikes started an effect (two probes heard)', len(pr) >= 2, str(pr))
    if len(pr) >= 2:
        check('held: the counter begins after the dodge has finished its frames (%.3f s between the misses; 1.433 s)' % (pr[1] - pr[0]), near(pr[1] - pr[0], 1.433, 0.06), str(pr))
    sc = s3.scene_lines(log)
    check('held: the scene logs the same lines as any fight (strike, miss, strike (counter), miss, end)',
          [l.split(' ')[0] for l in sc if l.split(' ')[0] in ('strike', 'miss', 'hit', 'end')] == ['strike', 'miss', 'strike', 'miss', 'end'], str(sc))

    # The same animations with the keys left out: today's timing. The dodge starts 120 ms before the miss and plays once; the striker has no hold.
    gid = 'nohold'
    fight_game(gid)
    got = frames_at(gid, [-0.05, 0.05, 0.15, 0.35, 0.45, 0.65], 'nohold')
    expect('no keys', got, {-0.05: (('a', 1), ('d', 0)), 0.05: (('a', 2), ('d', 1)), 0.15: (('a', 2), ('d', 2)), 0.35: ('stand', ('d', 4)),
                            0.45: ('stand', ('d', 5)), 0.65: ('stand', 'stand')})
    pr, _ = probes(gid)
    if len(pr) >= 2:
        check('no keys: the counter comes 1.180 s after the miss as before (%.3f s)' % (pr[1] - pr[0]), near(pr[1] - pr[0], 1.18, 0.06), str(pr))
    # A scene section with the durations but no animation that holds changes nothing either.
    gid = 'nohold_scene'
    fight_game(gid, scene={'resultHoldMs': 4000, 'resultHoldAgainMs': 900})
    pr, _ = probes(gid)
    if len(pr) >= 2:
        check('no keys, a "scene" section: still 1.180 s (the durations are only used by a pose that holds)', near(pr[1] - pr[0], 1.18, 0.06), str(pr))

    # The scene's resultHoldMs: the hold is as long as the key says (900 ms here), and so is the wait for the next strike.
    gid = 'hold_long'
    fight_game(gid, {'holdFrame': 3}, {'startFrame': 2, 'holdFrame': 3}, scene={'resultHoldMs': 900})
    got = frames_at(gid, [0.3, 0.8, 1.0], 'holdlong')
    expect('resultHoldMs 900', got, {0.3: (('a', 3), ('d', 3)), 0.8: (('a', 3), ('d', 3)), 1.0: ('stand', ('d', 4))})
    pr, _ = probes(gid)
    if len(pr) >= 2:
        check('resultHoldMs 900: the counter comes 367 ms later than with 533 (1.800 s; %.3f s)' % (pr[1] - pr[0]), near(pr[1] - pr[0], 1.8, 0.06), str(pr))

    # startDelayMs: the dodge proper starts that long after the miss (150 ms): frame 2 at M + 150 ms, frame 3 at M + 250 ms.
    gid = 'hold_delay'
    fight_game(gid, None, {'startFrame': 2, 'holdFrame': 3, 'startDelayMs': 150})
    got = frames_at(gid, [0.1, 0.18, 0.4], 'holddelay')
    expect('startDelayMs 150', got, {0.1: (('a', 2), ('d', 1)), 0.18: (('a', 2), ('d', 2)), 0.4: ('stand', ('d', 3))})

    # "holdFrame" without "startFrame": there is no brace, the whole dodge starts at the miss (and not 120 ms before it).
    gid = 'hold_only'
    fight_game(gid, None, {'holdFrame': 3})
    got = frames_at(gid, [-0.05, 0.05, 0.35, 0.45, 0.58], 'holdonly')
    expect('holdFrame alone', got, {-0.05: (('a', 1), 'stand'), 0.05: (('a', 2), ('d', 0)), 0.35: ('stand', ('d', 3)), 0.45: ('stand', ('d', 3)), 0.58: ('stand', ('d', 4))})


# ---- 2. a staff's heal sound in the scene only -----------------------------------------------------------------------------------------------

def heal_sounds(gid, sounds, staves=True):
    """The sounds (by file name) heard while Wen heals wounded Tarn with a Mend Staff whose "sounds" are `sounds`; on the map when `staves` is false."""
    saves = s3.fx_game(gid, [], tweak=s3.mend_effects(lambda it: it.update({'sounds': sounds})), staves=staves)
    g = os.path.join(s3.TMP, gid)
    for name in ('heal_map', 'heal_scene'):
        shutil.copy(os.path.join(g, 'audio', 'probe.wav'), os.path.join(g, 'audio', name + '.wav'))
    s3.write_json(os.path.join(g, 'data', 'audio.json'),
                  {'sounds': {n: {'file': 'audio/%s.wav' % n} for n in ('probe', 'heal_map', 'heal_scene')}})
    snd = []
    log, units = s3.run_player(gid, USE + ['wait:160'], saves, frames=800, sounds=snd)
    return [n for t, n in snd], log, units


def heal_scene_sound():
    for name, sounds, staves, want in (
            ('both', {'heal': 'heal_map', 'healScene': 'heal_scene'}, True, ['heal_scene']),                  # the scene plays healScene instead of heal
            ('only_heal', {'heal': 'heal_map'}, True, ['heal_map']),                                         # no healScene: heal plays in the scene as before
            ('silent', {'heal': 'heal_map', 'healScene': ''}, True, []),                                      # "" silences the scene
            ('only_scene', {'healScene': 'heal_scene'}, True, ['heal_scene']),
            ('map_both', {'heal': 'heal_map', 'healScene': 'heal_scene'}, False, ['heal_map']),               # on the map "heal" plays, healScene does not
            ('map_silent', {'heal': 'heal_map', 'healScene': ''}, False, ['heal_map']),
            ('map_only_scene', {'healScene': 'heal_scene'}, False, [])):                                      # nothing on the map from healScene
        names, log, units = heal_sounds('hs_' + name, sounds, staves)
        where = 'in the scene' if staves else 'on the map'
        check('heal sound %s (%s): heard %s' % (name, where, want or 'nothing'), [n for n in names if n != 'probe'] == want, str(names))
        check('heal sound %s: the scene %s' % (name, 'played' if staves else 'did not play'), ('battle scene: start' in log) == staves, log[-300:])
        check('heal sound %s: the rules result is the same (Tarn healed by 8)' % name, units.get('tarn', {}).get('hp') == 13, str(units.get('tarn')))


# ---- 3. a platform per fighter's own terrain ---------------------------------------------------------------------------------------------------

C_PLAIN_PLAT, C_SAND_PLAT = (200, 120, 40), (60, 160, 70)   # cells 0 and 1 of the platform sheet
C_SAND_BG = (90, 40, 120)


def platform_game(gid, plat_from, bg_from):
    """The staff scene with Tarn (the target, on the left) standing on sand and Wen (the user, on the right) on plain, two squares apart; sand has its own platform
    and backdrop. Returns (saves, extra log lines needed)."""
    def tweak(g):
        s3.edit(g, 'data/terrain.json', lambda t: t['terrain'].append({'id': 'sand', 'name': 'Sand'}))

        def level(m):
            m['legend']['s'] = 'sand'
            m['tiles'][2] = m['tiles'][2][:5] + 's' + m['tiles'][2][6:]
            for u in m['units']:
                if u.get('character') == 'wen':
                    u['x'], u['y'] = 3, 2
                if u.get('character') == 'tarn':
                    u['x'], u['y'] = 5, 2
        s3.edit(g, 'maps/clinic.json', level)
        extra = {'battlePlatformFrom': plat_from} if plat_from else {}
        if bg_from:
            extra['battleBackgroundFrom'] = bg_from
        s3.edit(g, 'game.json', lambda gj: gj.update(extra))
    glow = [{'id': 'glow', 'weapon': 'mend_staff', 'sheet': 'fx_spark', 'frames': [[0, 0]], 'ms': 300, 'anchor': [8, 8], 'sounds': {'0': 'probe'}}]
    saves = s3.fx_game(gid, glow, tweak=tweak, extra_sheets=[s3.sheet('bg_sand', 240, 160)], extra_files={'bg_sand': s3.solid(240, 160, C_SAND_BG)})
    s3.edit(os.path.join(s3.TMP, gid), 'data/graphics.json', lambda gj: gj['battle'].update({
        'platforms': {'plain_p': {'sheet': 'plat', 'cell': [0, 0]}, 'sandy': {'sheet': 'plat', 'cell': [1, 0]}},
        'terrainPlatform': {'plain': 'plain_p', 'sand': 'sandy'}, 'defaultPlatform': 'plain_p',
        'backgrounds': {'arena': {'sheet': 'stage_bg', 'cell': [0, 0]}, 'arena_sand': {'sheet': 'bg_sand', 'cell': [0, 0]}},
        'terrainBackground': {'plain': 'arena', 'sand': 'arena_sand'}}))
    return saves


def platforms():
    # (platformFrom, backgroundFrom, left platform, right platform, backdrop). Tarn (left, struck) is on sand, Wen (right, the user) on plain.
    cases = (
        ('default', None, None, C_SAND_PLAT, C_SAND_PLAT, C_SAND_BG),      # absent: one platform from the struck fighter's square for both
        ('same', 'same', None, C_SAND_PLAT, C_SAND_PLAT, C_SAND_BG),
        ('same_att', 'same', 'attacker', C_PLAIN_PLAT, C_PLAIN_PLAT, C_BACK),
        ('own', 'own', None, C_SAND_PLAT, C_PLAIN_PLAT, C_SAND_BG),        # each on its own: the backdrop still follows "defender"
        ('own_att', 'own', 'attacker', C_SAND_PLAT, C_PLAIN_PLAT, C_BACK),  # ... and "attacker" moves only the backdrop
    )
    results = {}
    for tag, pf, bf, left, right, back in cases:
        gid = 'plat_' + tag
        saves = platform_game(gid, None if tag == 'default' else pf, bf)
        shots, log = s3.timed_shots(gid, USE + ['wait:120'], saves, {'plat-' + tag: 0.15})
        s = shots.get('plat-' + tag)
        results[tag] = (s, log)
        if not s:
            continue
        # The ranged layout (two squares): platforms at (24, 96) and (152, 96) in stage pixels, drawn from the stage's top left (0, -50); sample inside each.
        got = (s.px(2 * 28, 2 * 120 - 50), s.px(2 * 156, 2 * 120 - 50), s.px(2 * 120, 2 * 60 - 50))
        check('platform %s: left platform %s, right platform %s, backdrop %s' % (tag, left, right, back), got == (left, right, back), 'got %s' % (got,))
        sl = s3.scene_lines(log)
        plat_lines = [l for l in sl if l.startswith('platform ')]
        want = ['platform "sandy"'] if tag in ('default', 'same') else ['platform "plain_p"'] if tag == 'same_att' else \
            ['platform "sandy" (left)', 'platform "plain_p" (right)']
        check('platform %s: the log names the platform%s' % (tag, 's' if len(want) > 1 else ''), plat_lines == want, str(plat_lines))
    d, o = results.get('default'), results.get('same')
    if d and o and d[0] and o[0]:
        check('platform: "same" is exactly the picture of leaving the key out', d[0].d == o[0].d)


# ---- 4. endOffset by distance ----------------------------------------------------------------------------------------------------------------

def end_offset():
    def bolt(extra):
        return [dict({'id': 'bolt', 'weapon': 'mend_staff', 'sheet': 'fx_spark', 'frames': [[0, 0]], 'ms': 100, 'mode': 'travel', 'travelMs': 300, 'lingerMs': 1500,
                      'anchor': [8, 8], 'sounds': {'0': 'probe'}}, **extra)]
    by_distance = {'atRange': [{'from': 2, 'endOffset': 40}, {'from': 4, 'endOffset': -40}]}
    centre = {}
    for dist, wen in ((2, 3), (4, 1)):
        for name, extra in (('absent', {}), ('plain_past', {'endOffset': -40}), ('by_distance', by_distance),
                            ('with_delay', {'endOffset': 10, 'atRange': [{'from': 2, 'delayMs': 200, 'endOffset': 20}, {'from': 4, 'delayMs': 200}]})):
            gid = 'eo5_%s_%d' % (name, dist)
            saves = s3.far_game(gid, bolt(extra), STAGE, wen, 5)
            shots, log = s3.timed_shots(gid, USE + ['wait:120'], saves, {gid: 0.9})
            s = shots.get(gid)
            bb = s.bbox(C_SPARK) if s else None
            centre[(name, dist)] = None if bb is None else (bb[0] + bb[2]) // 2
    for dist in (2, 4):
        base = centre.get(('absent', dist))
        check('endOffset: the plain shot lands somewhere at %d squares' % dist, base is not None, str(centre))
        if base is None:
            continue
        # Wen (right) shoots left at Tarn: a shot that stops short rests further right, one that goes past further left; k = 2 screen pixels per stage pixel.
        want = {2: 80, 4: -80}[dist]  # (the by-distance steps: 2 squares stop 40 short, 4 squares go 40 past)
        check('endOffset by distance, %d squares: %+d stage pixels = %+d screen pixels from the plain landing (%s -> %s)' % (dist, want // 2, want, base, centre.get(('by_distance', dist))),
              near(centre.get(('by_distance', dist)), base + want), str(centre))
        check('endOffset -40 (plain key, past the target) at %d squares: 80 screen pixels left of the plain landing' % dist, near(centre.get(('plain_past', dist)), base - 80), str(centre))
        step = {2: 20, 4: 10}[dist]  # a step with a delay: at 2 squares its own 20, at 4 squares (a step with no endOffset) the plain key's 10
        check('endOffset: a step that gives none uses the plain key, one that gives it wins (%d at %d squares)' % (step, dist),
              near(centre.get(('with_delay', dist)), base + 2 * step), str(centre))


# ---- 5. atRangeFixed -----------------------------------------------------------------------------------------------------------------------------

def at_range_fixed():
    C_FX = (100, 90, 120)
    delay = {'atRange': [{'from': 2, 'delayMs': 600}]}
    # A fade row of the delayed effect: marked, it starts on time (0.6 s before the effect's picture), unmarked it is carried along with the effect.
    for tag, fixed in (('fixed', True), ('moved', False)):
        row = {'atMs': 0, 'ms': 200, 'to': '#000000', 'alpha': 1, 'holdMs': 400, 'over': 'all'}
        if fixed:
            row['atRangeFixed'] = True
        glow = [dict({'id': 'glow', 'weapon': 'mend_staff', 'sheet': 'fx_spark', 'frames': [[0, 0]], 'ms': 300, 'anchor': [8, 8], 'sounds': {'0': 'probe'}, 'fade': [row]}, **delay)]
        gid = 'fixed_fade_' + tag
        saves = s3.far_game(gid, glow, STAGE, 3, 5)
        shots, log = s3.timed_shots(gid, USE + ['wait:120'], saves, {'fixed-fade-%s-before' % tag: -0.3, 'fixed-fade-%s-after' % tag: 1.2})
        b, a = shots.get('fixed-fade-%s-before' % tag), shots.get('fixed-fade-%s-after' % tag)
        if b and a:
            dark = b.px(240, 70) == (0, 0, 0) and b.bbox(C_BL) is None
            check('atRangeFixed fade (%s): 0.3 s before the delayed effect starts the scene is %s' % (tag, 'already black' if fixed else 'still untouched'),
                  dark == fixed and (fixed or b.px(240, 70) == C_BACK), str((b.px(240, 70), b.bbox(C_BL))))
            check('atRangeFixed fade (%s): afterwards the scene is back to normal' % tag, a.px(240, 70) == C_BACK and a.bbox(C_BL) is not None)
        check('atRangeFixed fade (%s): the effect still logs its start once' % tag, s3.scene_lines(log).count('effect glow start') == 1)
    # A screen layer of its own, listed with the delayed effect: marked, it covers the backdrop on time; unmarked it comes with the delayed effect.
    for tag, fixed in (('fixed', True), ('moved', False)):
        dim = {'id': 'dim', 'sheet': 'fx_wash', 'frames': [[0, 0]], 'ms': 3000, 'layer': 'screen'}
        if fixed:
            dim['atRangeFixed'] = True
        glow = dict({'id': 'glow', 'weapon': 'mend_staff', 'sheet': 'fx_spark', 'frames': [[0, 0]], 'ms': 300, 'anchor': [8, 8], 'sounds': {'0': 'probe'}, 'effects': ['dim']}, **delay)
        gid = 'fixed_layer_' + tag
        saves = s3.far_game(gid, [glow, dim], STAGE, 3, 5, extra_sheets=[s3.sheet('fx_wash', 240, 160)], extra_files={'fx_wash': s3.solid(240, 160, C_FX)})
        shots, log = s3.timed_shots(gid, USE + ['wait:120'], saves, {'fixed-layer-%s-before' % tag: -0.3, 'fixed-layer-%s-with' % tag: 0.1})
        b, w = shots.get('fixed-layer-%s-before' % tag), shots.get('fixed-layer-%s-with' % tag)
        if b and w:
            check('atRangeFixed layer (%s): 0.3 s before the delayed effect the screen layer is %s' % (tag, 'already there' if fixed else 'not there yet'),
                  (b.px(240, 70) == C_FX) == fixed, str(b.px(240, 70)))
            check('atRangeFixed layer (%s): when the delayed effect starts the layer is there' % tag, w.px(240, 70) == C_FX, str(w.px(240, 70)))


# ---- 6. panAtMs -----------------------------------------------------------------------------------------------------------------------------------

def pan_at():
    base_glow = {'id': 'glow', 'weapon': 'mend_staff', 'sheet': 'fx_spark', 'frames': [[0, 0]], 'ms': 800, 'anchor': [8, 8], 'hitAtMs': 300, 'sounds': {'0': 'probe'}}

    def band(**pan):
        return dict(STAGE, ranges=[dict(s3.BAND_EACH, pan=dict(s3.BAND_EACH['pan'], **pan))])

    def run(tag, effect_extra, pan, deltas):
        gid = 'pa_' + tag
        saves = s3.far_game(gid, [dict(base_glow, **effect_extra)], band(**pan), 0, 6)
        shots, log = s3.timed_shots(gid, USE + ['wait:160'], saves, {'pan-%s-%s' % (tag, d): d for d in deltas})
        out = {}
        for d in deltas:
            s = shots.get('pan-%s-%s' % (tag, d))
            if not s:
                continue
            ox, oy = s3.place(s, 2) if s.bbox(C_BL) else (0, -50)
            bb = s.bbox(s3.C_BLUE_BODY)
            c = None if bb is None else (bb[0] + bb[2]) // 2
            # Wen (the caster, on the right) is under the camera at the start: her body is at (400 - 200) * 2 = 400; Tarn's (left) at 40 * 2 = 80 once it has moved.
            out[d] = 'caster' if near(c, ox + 400, 24) else 'target' if near(c, ox + 80, 24) else 'moving (%s)' % c
        return out
    # Without any key the camera arrives with the blow (0.3 s after the effect starts): on the target at 0.7 s.
    a = run('absent', {}, {}, [0.02, 0.7])
    check('panAtMs absent: the camera is on the caster at the start and on the target at 0.7 s', a.get(0.02) == 'caster' and a.get(0.7) == 'target', str(a))
    # panAtMs 800: it leaves 0.8 s after the effect starts (arrives 1.05 s): still on the caster at 0.7 s, on the target at 1.6 s. The band's own startMs (100) loses.
    b = run('own', {'panAtMs': 800}, {'startMs': 100}, [0.02, 0.7, 1.6])
    check('panAtMs 800: the camera stays on the caster until 0.8 s after the effect starts, then moves to the target (the band\'s startMs does not apply)',
          b.get(0.02) == 'caster' and b.get(0.7) == 'caster' and b.get(1.6) == 'target', str(b))
    # With a delay by distance: the effect (and so panAtMs) is counted from the delayed start. A band startMs of 3000 would keep the camera on the caster for ages.
    c = run('delay', {'panAtMs': 0, 'atRange': [{'from': 2, 'delayMs': 600}]}, {'startMs': 3000}, [0.02, 0.5])
    check('panAtMs 0 with a 600 ms delay and a band startMs of 3000: the camera leaves when the delayed effect starts and has arrived 0.5 s later',
          c.get(0.02) == 'caster' and c.get(0.5) == 'target', str(c))
    d = run('band_only', {'atRange': [{'from': 2, 'delayMs': 600}]}, {'startMs': 3000}, [0.5])
    check('the same without panAtMs: the band\'s startMs (3000 ms after the strike) keeps the camera on the caster', d.get(0.5) == 'caster', str(d))


def main():
    try:
        holds()
        heal_scene_sound()
        platforms()
        end_offset()
        at_range_fixed()
        pan_at()
    finally:
        shutil.rmtree(s3.TMP, ignore_errors=True)
    print('%d passed, %d failed' % (s3.passed, len(s3.failures)))
    if s3.failures:
        print('FAILED: ' + ', '.join(s3.failures))
        sys.exit(1)


if __name__ == '__main__':
    main()
