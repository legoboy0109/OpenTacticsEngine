#!/usr/bin/env python3
"""The third batch of battle scene features through the real player, windowless (docs/BATTLE_SCENE.md).

  python3 tests/battle_scene3_player_test.py ./build/engine/player/ote-player .
  python3 tests/battle_scene3_player_test.py ./build/engine/player/ote-player . --shots OUT_DIR   # keep the screenshots (BMP)

Works on copies of tests/fixtures/srpg-staves (a staff use) and srpg-quotes (a fight) with original generated test art (a 240 x 160 stage,
platforms, fighters with magenta foot pixels, plain-colour effect pictures), always headless, no sound device.

  - staves in the scene: a heal staff plays as a scene (log lines, the target's bar rises, the staff's effect shows), a staff that misses says
    MISS, a staff with a condition shows its label, a staff that moves a unit stays on the map, the setting off keeps the map;
  - additive blending, scene-wide hide / fade windows, an effect's draw order (`order`), range bands of the fixed stage with a pan, a travel
    effect with `speed`.
"""
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import wave

sys.dont_write_bytecode = True

PLAYER = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else ''
REPO = os.path.abspath(sys.argv[2]) if len(sys.argv) > 2 else '.'
SHOTS = os.path.abspath(sys.argv[sys.argv.index('--shots') + 1]) if '--shots' in sys.argv else ''
TMP = tempfile.mkdtemp(prefix='ote-bs3-')
passed, failures = 0, []
DT = 0.016

sys.path.insert(0, os.path.join(REPO, 'tools', 'art'))
import battle_art  # noqa: E402  (our own generator: original pixel art, no Pillow)

Canvas, write_png = battle_art.Canvas, battle_art.write_png

# Marker colours (exact matches only).
C_TL, C_TR, C_BL, C_BR = (255, 255, 0), (0, 255, 255), (0, 255, 0), (255, 128, 0)  # the backdrop's corners
C_FOOT = (255, 0, 255)                                                              # a fighter's foot pixel
C_BLUE_BODY, C_RED_BODY = (30, 60, 200), (200, 40, 40)
C_PLATFORM = (200, 120, 40)
C_SPARK = (120, 255, 60)                                                            # the staff's effect on the target
C_BACK = (24, 30, 96)                                                               # the backdrop's plain colour (row 0)

STAGE = {'width': 240, 'height': 160,
         'feet': {'left': [88, 104], 'right': [152, 104]}, 'feetFar': {'left': [56, 104], 'right': [184, 104]},
         'platform': {'left': [56, 96], 'right': [120, 96]}, 'platformFar': {'left': [24, 96], 'right': [152, 96]},
         'align': 'bottom'}
PLAT = {'sheet': 'plat', 'cell': [0, 0], 'far': [1, 0]}


# ---- helpers ----------------------------------------------------------------------------------------------------------------------------

def read_json(path):
    with open(path) as f:
        return json.loads(''.join(l for l in f if not l.lstrip().startswith('//')))


def write_json(path, obj):
    with open(path, 'w') as f:
        json.dump(obj, f, indent=1)


def edit(g, rel, fn):
    path = os.path.join(g, rel)
    obj = read_json(path)
    fn(obj)
    write_json(path, obj)


def check(name, cond, detail=''):
    global passed
    if cond:
        passed += 1
        print('PASS ' + name)
    else:
        failures.append(name)
        print('FAIL %s %s' % (name, detail))


def keep(path, name):
    if SHOTS and os.path.exists(path):
        os.makedirs(SHOTS, exist_ok=True)
        shutil.copy(path, os.path.join(SHOTS, name + '.bmp'))


class Shot:
    def __init__(self, path):
        with open(path, 'rb') as f:
            d = f.read()
        off, = struct.unpack_from('<I', d, 10)
        w, h, planes, bpp = struct.unpack_from('<iiHH', d, 18)
        self.bottom_up = h > 0
        self.w, self.h = w, abs(h)
        self.bpp = bpp // 8
        self.stride = (w * self.bpp + 3) // 4 * 4
        self.d, self.off = d, off

    def px(self, x, y):
        if not (0 <= x < self.w and 0 <= y < self.h):
            return None
        row = self.h - 1 - y if self.bottom_up else y
        i = self.off + row * self.stride + x * self.bpp
        return (self.d[i + 2], self.d[i + 1], self.d[i])

    def count(self, col, x0=0, x1=None, y0=0, y1=None):
        n = 0
        for y in range(y0, self.h if y1 is None else y1):
            for x in range(x0, self.w if x1 is None else x1):
                if self.px(x, y) == col:
                    n += 1
        return n

    def bbox(self, col, x0=0, x1=None, y0=0, y1=None):
        xs, ys = [], []
        for y in range(y0, self.h if y1 is None else y1):
            for x in range(x0, self.w if x1 is None else x1):
                if self.px(x, y) == col:
                    xs.append(x)
                    ys.append(y)
        return (min(xs), min(ys), max(xs) + 1, max(ys) + 1) if xs else None


def lines(log, key):
    return [l for l in log.splitlines() if key in l]


def scene_lines(log):
    return [l.split('battle scene: ', 1)[1] for l in log.splitlines() if 'battle scene: ' in l]


# ---- generated test art -----------------------------------------------------------------------------------------------------------------

def backdrop():
    c = Canvas(240, 160)
    for y in range(160):
        for x in range(240):
            c.set(x, y, C_BACK + (255,))
    for (x0, y0, col) in ((0, 0, C_TL), (236, 0, C_TR), (0, 156, C_BL), (236, 156, C_BR)):
        c.rect(x0, y0, x0 + 3, y0 + 3, col + (255,))
    return c


def platforms():
    sheet = Canvas(256, 32)
    for i, body in enumerate(((200, 120, 40), (60, 160, 70), (140, 60, 170), (60, 120, 170))):
        sheet.rect(i * 64, 0, i * 64 + 63, 31, body + (255,))
    return sheet


def fighter(body):
    c = Canvas(48, 48)
    c.rect(16, 6, 31, 45, body + (255,))
    c.rect(18, 0, 29, 6, (232, 192, 150, 255))
    c.rect(23, 46, 24, 46, C_FOOT + (255,))
    return c


def solid(w, h, col):
    c = Canvas(w, h)
    c.rect(0, 0, w - 1, h - 1, col + (255,))
    return c


def probe_audio(g):
    """A tiny sound "probe" a test effect plays at its start (--audio-log tells when): the clock the screenshots are timed by."""
    os.makedirs(os.path.join(g, 'audio'), exist_ok=True)
    with wave.open(os.path.join(g, 'audio', 'probe.wav'), 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(22050)
        w.writeframes(b''.join(struct.pack('<h', 6000 if (i * 600 // 22050) % 2 else -6000) for i in range(1100)))
    write_json(os.path.join(g, 'data', 'audio.json'), {'sounds': {'probe': {'file': 'audio/probe.wav'}}})


def install_art(g, effects, stage=True, extra_sheets=(), extra_files=None):
    """Writes the pictures and data/graphics.json of a game: a 240 x 160 stage, blue and red fighters, platforms, `effects`."""
    art = os.path.join(g, 'art')
    os.makedirs(art, exist_ok=True)
    write_png(os.path.join(art, 'stage_bg.png'), backdrop())
    write_png(os.path.join(art, 'plat.png'), platforms())
    write_png(os.path.join(art, 'blue.png'), fighter(C_BLUE_BODY))
    write_png(os.path.join(art, 'red.png'), fighter(C_RED_BODY))
    write_png(os.path.join(art, 'fx_spark.png'), solid(16, 16, C_SPARK))
    for name, canvas in (extra_files or {}).items():
        write_png(os.path.join(art, name + '.png'), canvas)
    anims = lambda sheet: {k: {'sheet': sheet, 'frames': [[0, 0], [0, 0], [0, 0], [0, 0]], 'ms': [100, 100, 100, 100], 'hit': 2}
                           for k in ('stand', 'attack', 'crit', 'ranged', 'cast', 'dodge', 'hurt', 'defeat', 'miss')}
    cells = lambda sid, f, w, h: {'id': sid, 'file': 'art/%s.png' % f, 'cellWidth': w, 'cellHeight': h}
    sheets = [cells('stage_bg', 'stage_bg', 240, 160), cells('plat', 'plat', 64, 32), cells('blue', 'blue', 48, 48), cells('red', 'red', 48, 48),
              cells('fx_spark', 'fx_spark', 16, 16)] + list(extra_sheets)
    gfx = {'sheets': sheets,
           'battle': {'sets': {'blue': {'anchor': [24, 46], 'animations': anims('blue')},
                               'red': {'anchor': [24, 46], 'animations': anims('red')}},
                      'assign': [{'set': 'blue', 'side': 'blue'}, {'set': 'red', 'side': 'red'}],
                      'backgrounds': {'arena': {'sheet': 'stage_bg', 'cell': [0, 0]}}, 'defaultBackground': 'arena',
                      'platforms': {'slab': PLAT}, 'defaultPlatform': 'slab',
                      'effects': effects}}
    if stage:
        gfx['battle']['stage'] = STAGE
    write_json(os.path.join(g, 'data', 'graphics.json'), gfx)
    probe_audio(g)


def run_player(gid, inp, saves=None, frames=420, shot=None, extra=(), map_id='clinic', interval=4, game_args=(), sounds=None):
    """One windowless run; returns (log of the player, the continue-save units by id). `sounds`: a list that is filled with the audio log's
    sounds as (seconds, name); the sound is then logged (--audio-log; a headless run opens no device) instead of --no-audio."""
    work = os.path.join(TMP, 'run_' + gid)
    shutil.rmtree(work, ignore_errors=True)
    if saves:
        shutil.copytree(saves, work)
    alog = os.path.join(TMP, gid + '.audio.log')
    if os.path.exists(alog):
        os.remove(alog)
    cmd = [PLAYER, '--games', TMP, '--assets', os.path.join(REPO, 'assets'), '--headless', '--no-settings', '--saves', work]
    cmd += ['--audio-log', alog] if sounds is not None else ['--no-audio']
    cmd += ['--frames', str(frames), '--input-interval', str(interval), '--fixed-dt', str(DT), '--game', gid, '--map', map_id, '--input', ','.join(inp)]
    cmd += (['--continue'] if saves else []) + list(extra) + list(game_args)
    if shot:
        cmd += ['--screenshot', shot]
    p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True, timeout=300)
    if sounds is not None and os.path.exists(alog):
        for line in open(alog).read().splitlines():
            parts = line.split()
            if len(parts) > 3 and parts[1] == 'sound':
                sounds.append((float(parts[0][2:]), os.path.splitext(os.path.basename(parts[2]))[0]))
    units = {}
    try:
        for u in read_json(os.path.join(work, gid, 'continue.json'))['battle']['units']:
            units[u.get('id') or u['name']] = u
    except (OSError, ValueError, KeyError):
        pass
    return p.stdout, units


# ---- 1. staves in the scene ---------------------------------------------------------------------------------------------------------------

IDLE = 'wait:14'
MENU = [IDLE, 'confirm', 'wait:3', 'confirm', 'wait:3']   # Wen selected, her action menu open
LIST = MENU + ['confirm', 'wait:3']                         # the Staff list
MEND = LIST + ['confirm', 'wait:3']                         # the Mend Staff chosen: picking a unit (Tarn, two squares away, is first)
USE = MEND + ['confirm']


def staff_game(gid, effects=None, scene=True, staves=True, sides='playerRight', tweak=None):
    g = os.path.join(TMP, gid)
    shutil.copytree(os.path.join(REPO, 'tests/fixtures/srpg-staves'), g)
    install_art(g, effects or [])
    edit(g, 'game.json', lambda gj: gj.update({'battleScene': 'scene' if scene else 'map', 'battleSceneSides': sides,
                                                **({'battleSceneStaves': True} if staves else {})}))
    if tweak:
        tweak(g)
    return g


def hurt_save(gid, hp):
    """A Continue save with some units wounded (the maps cannot place a wounded unit): made by the player, then edited."""
    saves = os.path.join(TMP, 'saves_' + gid)
    shutil.rmtree(saves, ignore_errors=True)
    subprocess.run([PLAYER, '--games', TMP, '--assets', os.path.join(REPO, 'assets'), '--headless', '--no-audio', '--no-settings', '--saves', saves,
                    '--frames', '120', '--fixed-dt', '0.016', '--game', gid, '--map', 'clinic', '--input', 'wait:20'],
                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=300)
    path = os.path.join(saves, gid, 'continue.json')
    doc = read_json(path)
    for u in doc['battle']['units']:
        if u.get('id') in hp:
            u['hp'] = hp[u['id']]
    write_json(path, doc)
    return saves


def first(sounds, name):
    return next((t for t, n in sounds if n == name), None)


def place(s, k):
    """The stage's top-left on the screen, read from the backdrop's bottom-left corner mark (the one always in view); when an effect or a hidden
    backdrop covers the mark, the place the 480 x 270 screen gives a 240 x 160 stage that is aligned to the bottom (x 0, y -50)."""
    bl = s.bbox(C_BL)
    return (bl[0], bl[3] - 160 * k) if bl else (0, 270 - 160 * 2)


def feet(k, ox, oy, side, band='feet'):
    f = STAGE[band][side]
    return ox + f[0] * k, oy + f[1] * k


def timed_shots(gid, inp, saves, shots, cap=900, W=480, H=270, **kw):
    """Screenshots `delta` seconds after the first sound "probe" (an effect's first sound, at its start) was heard. `shots`: {name: delta};
    returns ({name: Shot}, the log of the run that found the probe). A shot that could not be taken is missing."""
    snd = []
    log, units = run_player(gid, inp, saves, frames=cap, sounds=snd, **kw)
    t0 = first(snd, 'probe')
    check(gid + ': the effect started (its probe sound was heard)', t0 is not None, log[-400:])
    out = {}
    if t0 is None:
        return out, log
    for name, delta in shots.items():
        path = os.path.join(TMP, name + '.bmp')
        run_player(gid, inp, saves, frames=int(round((t0 + delta) / DT)), shot=path, **kw)
        keep(path, name)
        if not os.path.exists(path):
            check(name + ': screenshot exists', False)
            continue
        sh = Shot(path)
        check(name + ': screenshot is %d x %d' % (W, H), (sh.w, sh.h) == (W, H), '%d x %d' % (sh.w, sh.h))
        out[name] = sh
    return out, log


C_BAR_EMPTY = (40, 18, 22)  # the empty part of a hit-point bar


def mend_effects(tweak):
    """A tweak of the fixture's Mend Staff (the staff the scripted inputs pick)."""
    def f(g):
        def fn(doc):
            for it in doc['items']:
                if it['id'] == 'mend_staff':
                    tweak(it)
        edit(g, 'data/items.json', fn)
    return f


GLOW = [{'id': 'glow', 'weapon': 'mend_staff', 'sheet': 'fx_spark', 'frames': [[0, 0]], 'ms': 800, 'anchor': [8, 8], 'hitAtMs': 300,
         'sounds': {'0': 'probe'}}]


def staves():
    staff_game('st_heal', GLOW)
    saves = hurt_save('st_heal', {'tarn': 5, 'ola': 20})
    log, units = run_player('st_heal', USE + ['wait:120'], saves, frames=700)
    sl = scene_lines(log)
    start = [l for l in sl if l.startswith('start ')]
    check('staff heal: the scene starts with the staff and the target in the start line',
          len(start) == 1 and 'Wen' in start[0] and 'Tarn' in start[0] and 'staff Mend Staff on Tarn' in start[0] and '1 strike(s)' in start[0], str(start))
    check('staff heal: the target\'s result is logged', 'staff heal Tarn +8' in sl, str(sl))
    check('staff heal: the order is start, the effect, the result, end',
          'effect glow start' in sl and 'staff heal Tarn +8' in sl and sl.index('effect glow start') < sl.index('staff heal Tarn +8') < sl.index('end'), str(sl))
    check('staff heal: the sides: the person on the right with playerRight', 'left Tarn, right Wen' in sl, str(sl))
    check('staff heal: no strike, no counter, no hit line', not any(l.startswith(('strike', 'hit', 'crit', 'miss')) for l in sl), str(sl))
    check('staff heal: the rules did their part (Tarn healed by 8, a use spent)',
          units.get('tarn', {}).get('hp') == 13 and units.get('wen', {}).get('items', [{}])[0].get('uses') == 5, str(units.get('tarn')))
    check('staff heal: the map line of the staff is still written afterwards', bool(lines(log, 'staff: Wen uses Mend Staff')))
    # Pictures: the staff's effect sits on the target (Tarn, left) at once; later the bar of the target's box holds more.
    shots, _ = timed_shots('st_heal', USE + ['wait:120'], saves, {'staff-heal-cast': 0.15, 'staff-heal-after': 1.6})
    cast, after = shots.get('staff-heal-cast'), shots.get('staff-heal-after')
    if cast and after:
        ox, oy = place(cast, 2)
        lx, ly = feet(2, ox, oy, 'left', 'feetFar')  # (Wen and Tarn are two squares apart: the far layout)
        sp = cast.bbox(C_SPARK)
        check('staff heal: the staff\'s effect (anchor in the middle) sits on the target\'s foot point', sp == (lx - 16, ly - 16, lx + 16, ly + 16), str((sp, lx, ly)))
        check('staff heal: the user stands on the right, the target on the left (the spark covers the left foot, the right foot mark shows)',
              cast.bbox(C_FOOT, 240, 480) is not None and cast.px(lx - 14, ly - 20) == C_BLUE_BODY)
        e0, e1 = cast.count(C_BAR_EMPTY, 0, 240, 0, 60), after.count(C_BAR_EMPTY, 0, 240, 0, 60)
        check('staff heal: the target\'s hit-point bar holds more afterwards (5 -> 13 of 30: less empty bar)', e0 > 0 and e1 < e0, str((e0, e1)))
        check('staff heal: the effect is gone afterwards', after.bbox(C_SPARK) is None)
    # The same on the phone-shaped screen.
    staff_game('st_heal_p', GLOW, tweak=lambda g: edit(g, 'game.json', lambda gj: gj.update({'view': {'width': 270, 'height': 480, 'tileSize': 24}})))
    saves_p = hurt_save('st_heal_p', {'tarn': 5})
    shots, _ = timed_shots('st_heal_p', USE + ['wait:120'], saves_p, {'staff-heal-portrait': 0.15}, W=270, H=480)
    ph = shots.get('staff-heal-portrait')
    if ph:
        check('staff heal, portrait: the whole stage is in view and the effect shows', ph.bbox(C_BL) is not None and ph.bbox(C_SPARK) is not None)

    # A staff that misses: MISS, no heal, nothing changes.
    staff_game('st_miss', GLOW, tweak=mend_effects(lambda it: it.update({'hit': '0'})))
    saves = hurt_save('st_miss', {'tarn': 5})
    log, units = run_player('st_miss', USE + ['wait:120'], saves, frames=700)
    sl = scene_lines(log)
    check('staff miss: the scene logs the miss on the target and no heal', 'staff miss Tarn' in sl and not any(l.startswith('staff heal') for l in sl), str(sl))
    check('staff miss: the hit points did not change but the use was spent', units.get('tarn', {}).get('hp') == 5 and units.get('wen', {}).get('items', [{}])[0].get('uses') == 5, str(units.get('tarn')))
    shots, _ = timed_shots('st_miss', USE + ['wait:120'], saves, {'staff-miss': 0.55})
    if 'staff-miss' in shots:
        check('staff miss: the picture draws (the scene still shows)', shots['staff-miss'].bbox(C_BL) is not None)

    # A staff that puts a condition on: the label shows after the number.
    cond = {'conditions': [{'id': 'poison', 'name': 'Poison', 'tag': 'Po', 'turns': 3,
                            'blocks': [{'when': 'turnStart', 'do': [{'hurt': {'percent': 10}, 'to': 'self'}]}]}]}

    def cond_tweak(g):
        write_json(os.path.join(g, 'data', 'conditions.json'), cond)
        mend_effects(lambda it: it.update({'effects': [{'heal': 4}, {'inflict': {'condition': 'poison'}}]}))(g)
    staff_game('st_cond', GLOW, tweak=cond_tweak)
    saves = hurt_save('st_cond', {'tarn': 5})
    log, units = run_player('st_cond', USE + ['wait:120'], saves, frames=700)
    sl = scene_lines(log)
    heal_i = next((i for i, l in enumerate(sl) if l.startswith('staff heal Tarn +')), -1)
    cond_i = next((i for i, l in enumerate(sl) if l.startswith('staff condition Poison')), -1)
    check('staff condition: the number and then the condition label are logged', 0 <= heal_i < cond_i, str(sl))
    shots, _ = timed_shots('st_cond', USE + ['wait:120'], saves, {'staff-condition': 0.75})
    if 'staff-condition' in shots:
        check('staff condition: the picture draws', shots['staff-condition'].bbox(C_BL) is not None)

    # A staff that moves a unit stays on the map; so does a game without the setting and a game that keeps the map.
    staff_game('st_pull', GLOW, tweak=mend_effects(lambda it: it.update({'effects': [{'pull': True}]})))
    saves = hurt_save('st_pull', {'tarn': 5})
    log, units = run_player('st_pull', USE + ['wait:120'], saves, frames=700)
    check('staff that moves a unit: played on the map, no scene', bool(lines(log, 'staff: Wen uses Mend Staff')) and 'battle scene: start' not in log, log[-500:])
    staff_game('st_off', GLOW, staves=False)
    saves = hurt_save('st_off', {'tarn': 5})
    log, units = run_player('st_off', USE + ['wait:120'], saves, frames=700)
    check('without "battleSceneStaves" the staff plays on the map as before', bool(lines(log, 'staff: Wen uses Mend Staff')) and 'battle scene: start' not in log, log[-500:])
    check('... and the rules result is the same (Tarn healed by 8)', units.get('tarn', {}).get('hp') == 13, str(units.get('tarn')))
    staff_game('st_map', GLOW, scene=False)
    saves = hurt_save('st_map', {'tarn': 5})
    log, units = run_player('st_map', USE + ['wait:120'], saves, frames=700)
    check('with "battleScene": "map" a staff stays on the map even with "battleSceneStaves"', 'battle scene: start' not in log and units.get('tarn', {}).get('hp') == 13, log[-300:])
    # Sides: "playerLeft": the person on the left, the target opposite.
    staff_game('st_left', GLOW, sides='playerLeft')
    saves = hurt_save('st_left', {'tarn': 5})
    log, units = run_player('st_left', USE + ['wait:120'], saves, frames=700)
    check('staff sides: "playerLeft" puts the user on the left and the ally opposite', 'left Wen, right Tarn' in scene_lines(log), str(scene_lines(log)))


def fx_game(gid, effects, extra_sheets=(), extra_files=None, stage=True, tweak=None, **kw):
    """A staff-scene game (Wen casts the Mend Staff on the wounded Tarn) with `effects`; returns the Continue save to run it from."""
    staff_game(gid, effects, tweak=tweak, **kw)
    install_art(os.path.join(TMP, gid), effects, stage=stage, extra_sheets=extra_sheets, extra_files=extra_files)
    return hurt_save(gid, {'tarn': 5})


def sheet(sid, w, h):
    return {'id': sid, 'file': 'art/%s.png' % sid, 'cellWidth': w, 'cellHeight': h}


def blending():
    C_FX = (100, 90, 120)
    add = lambda a, b: tuple(min(255, x + y) for x, y in zip(a, b))
    results = {}
    for mode in ('normal', 'add'):
        # A picture the size of the stage on the screen layer (over the backdrop only), and a block on the target at its foot (over the fighter).
        fx = [{'id': 'wash', 'weapon': 'mend_staff', 'sheet': 'fx_wash', 'frames': [[0, 0]], 'ms': 3000, 'layer': 'screen', 'blend': mode,
               'sounds': {'0': 'probe'}},
              {'id': 'blk', 'sheet': 'fx_blk', 'frames': [[0, 0]], 'ms': 3000, 'anchor': [20, 20], 'blend': mode}]
        fx[0]['effects'] = ['blk']
        gid = 'blend_' + mode
        saves = fx_game(gid, fx, extra_sheets=[sheet('fx_wash', 240, 160), sheet('fx_blk', 40, 40)],
                        extra_files={'fx_wash': solid(240, 160, C_FX), 'fx_blk': solid(40, 40, (60, 60, 60))})
        shots, log = timed_shots(gid, USE + ['wait:120'], saves, {'blend-' + mode: 0.3})
        results[mode] = shots.get('blend-' + mode), log
    n, a = results['normal'][0], results['add'][0]
    if not n or not a:
        return
    ox, oy = place(n, 2)
    lx, ly = feet(2, ox, oy, 'left', 'feetFar')
    for name, s in (('normal', n), ('add', a)):
        check('blend %s: the effect\'s log line says it started' % name, any('effect wash start' in l for l in results[name][1].splitlines()))
    # Over the backdrop (away from platforms and fighters): normal = the effect's own colour, add = the sum with the backdrop's.
    px_n, px_a = n.px(ox + 120 * 2, oy + 60 * 2), a.px(ox + 120 * 2, oy + 60 * 2)
    check('blend normal: the screen effect covers the backdrop with its own colour', px_n == C_FX, str(px_n))
    check('blend add: the pixels add to the backdrop\'s (%s + %s)' % (C_FX, C_BACK), px_a == add(C_FX, C_BACK), str(px_a))
    check('blend add: the result is brighter than the effect alone and than the backdrop alone, in every channel',
          px_a is not None and all(c > e and c > b for c, e, b in zip(px_a, C_FX, C_BACK)), str(px_a))
    # Over a fighter: the block on the target adds to the body colour (screen effect first, then the body, then the block).
    body_n, body_a = n.px(lx - 12, ly - 22), a.px(lx - 12, ly - 22)
    check('blend normal: the block replaces the fighter\'s colour', body_n == (60, 60, 60), str(body_n))
    check('blend add: the block brightens the fighter\'s body',
          body_a == add(C_BLUE_BODY, (60, 60, 60)), str(body_a))


# ---- main -------------------------------------------------------------------------------------------------------------------------------

def main():
    try:
        staves()
        blending()
    finally:
        shutil.rmtree(TMP, ignore_errors=True)
    print('%d passed, %d failed' % (passed, len(failures)))
    if failures:
        print('FAILED: ' + ', '.join(failures))
        sys.exit(1)


if __name__ == '__main__':
    main()
