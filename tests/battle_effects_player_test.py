#!/usr/bin/env python3
"""Battle scene effects through the real player, windowless (docs/BATTLE_SCENE.md "Effects").

  python3 tests/battle_effects_player_test.py ./build/engine/player/ote-player .
  python3 tests/battle_effects_player_test.py ./build/engine/player/ote-player . --shots OUT_DIR   # keep the screenshots (BMP)

Makes a copy of tests/fixtures/srpg-quotes with original generated test art (the stage test's 240 x 160 backdrop, platforms and fighters with
magenta foot pixels, plus a few effect pictures with distinctive colours) and plays Eirik's attack on Gorm with "battleSceneSides":
"playerRight", always headless, with --audio-log (headless opens no sound device; the sound files are tiny generated WAVs).

  - effect sounds: the audio log's times show a sound `hitAtMs` before the weapon's own hit sound (a lead-in), and the log lines
    `battle scene: sound X (effect ID)` and `effect ID start` come in the right order, before the hit; a long lead-in makes the hit wait;
  - the effect sounds are the first thing heard of an effect (the probe sound at 0 ms gives the effect's start time, from which a
    screenshot frame is picked);
  - screenshots (480 x 270 with the fixed stage, and 270 x 480): a screen-layer effect (covers the stage, platforms and fighters on top),
    a behind-layer effect (in front of the platform, behind the fighter), a charge on the striker and a spark on the target from one list,
    a returning projectile on its way back (turned round), a flash and a shake; a screen effect without a fixed stage;
  - an effect by unit type; a bad sound id is refused when the game loads.
"""
import json
import os
import shutil
import struct
import subprocess
import sys
import wave
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import (  # noqa: E402
    run_text, check, edit, failures, player_and_repo, read_jsonc, shots_dir, summary, write_json)
from ote_testkit import scratch_dir  # noqa: E402
read_json = read_jsonc  # this script reads fixtures and games whose JSON has `//` comment lines

sys.dont_write_bytecode = True

PLAYER, REPO = player_and_repo()
SHOTS = shots_dir(absolute=True)
TMP = scratch_dir()
DT = 0.016

sys.path.insert(0, os.path.join(REPO, 'tools', 'art'))
import battle_art  # noqa: E402  (our own generator: original pixel art, no Pillow)

Canvas, write_png = battle_art.Canvas, battle_art.write_png

# Marker colours (exact matches only).
C_TL, C_TR, C_BL, C_BR = (255, 255, 0), (0, 255, 255), (0, 255, 0), (255, 128, 0)  # the backdrop's corners
C_FOOT = (255, 0, 255)                                                              # a fighter's two foot pixels
C_BLUE_BODY, C_RED_BODY = (30, 60, 200), (200, 40, 40)
C_GRID = (180, 190, 210)
C_PLATFORM = (200, 120, 40)                                                         # the left platform picture's body
C_SCREEN_CORNER = (255, 0, 128)                                                     # the screen effect's four corners
C_SCREEN_PATCH = (200, 200, 0)                                                      # ... and a patch behind the fighters
C_BLOB = (0, 200, 120)                                                              # the behind-layer effect
C_CHARGE, C_SPARK = (250, 120, 200), (120, 255, 60)                                 # the striker's charge, the target's spark
C_ARROW_A, C_ARROW_B = (255, 0, 0), (0, 0, 255)                                     # the projectile's left and right ends (as drawn)

STAGE = {'width': 240, 'height': 160,
         'feet': {'left': [88, 104], 'right': [152, 104]}, 'feetFar': {'left': [56, 104], 'right': [184, 104]},
         'platform': {'left': [56, 96], 'right': [120, 96]}, 'platformFar': {'left': [24, 96], 'right': [152, 96]},
         'align': 'bottom'}
PLAT = {'sheet': 'plat', 'cell': [0, 0], 'far': [1, 0]}


# ---- generated test art -------------------------------------------------------------------------------------------------

def backdrop():
    c = Canvas(240, 160)
    for y in range(160):
        for x in range(240):
            c.set(x, y, (24, 30, 60 + y // 4, 255))
    for x in range(0, 240, 20):
        c.rect(x, 0, x, 159, C_GRID + (255,))
    for y in range(0, 160, 20):
        c.rect(0, y, 239, y, C_GRID + (255,))
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


def screen_fx(w, h, corner):
    """A picture the size of a stage: a 2 x 2 mark in each corner and a patch across the middle (the fighters stand on it)."""
    c = Canvas(w, h)
    c.rect(70 * w // 240, 60 * h // 160, 170 * w // 240, 110 * h // 160, C_SCREEN_PATCH + (255,))
    for (x0, y0) in ((0, 0), (w - 2, 0), (0, h - 2), (w - 2, h - 2)):
        c.rect(x0, y0, x0 + 1, y0 + 1, corner + (255,))
    return c


def solid(w, h, col):
    c = Canvas(w, h)
    c.rect(0, 0, w - 1, h - 1, col + (255,))
    return c


def arrow():
    c = Canvas(8, 8)
    c.rect(0, 0, 7, 7, (255, 255, 255, 255))
    c.rect(0, 0, 1, 7, C_ARROW_A + (255,))
    c.rect(6, 0, 7, 7, C_ARROW_B + (255,))
    return c


def wav(path, hz):
    with wave.open(path, 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(22050)
        w.writeframes(b''.join(struct.pack('<h', 6000 if (i * hz * 2 // 22050) % 2 else -6000) for i in range(1100)))


SOUNDS = ('probe', 's_hit', 's_fx1', 's_fx2', 's_anim', 's_cue')


def make_game(gid, effects, stage=True, sides='playerRight', patch=None, phone=False, screen_size=(240, 160), sword_hit=True):
    g = os.path.join(TMP, gid)
    shutil.copytree(os.path.join(REPO, 'tests/fixtures/srpg-quotes'), g)
    os.makedirs(os.path.join(g, 'art'))
    os.makedirs(os.path.join(g, 'audio'))
    art = os.path.join(g, 'art')
    write_png(os.path.join(art, 'stage_bg.png'), backdrop())
    write_png(os.path.join(art, 'plat.png'), platforms())
    write_png(os.path.join(art, 'blue.png'), fighter(C_BLUE_BODY))
    write_png(os.path.join(art, 'red.png'), fighter(C_RED_BODY))
    write_png(os.path.join(art, 'fx_screen.png'), screen_fx(screen_size[0], screen_size[1], C_SCREEN_CORNER))
    write_png(os.path.join(art, 'fx_blob.png'), solid(40, 40, C_BLOB))
    write_png(os.path.join(art, 'fx_charge.png'), solid(16, 16, C_CHARGE))
    write_png(os.path.join(art, 'fx_spark.png'), solid(16, 16, C_SPARK))
    write_png(os.path.join(art, 'fx_arrow.png'), arrow())
    for i, n in enumerate(SOUNDS):
        wav(os.path.join(g, 'audio', n + '.wav'), 300 + 40 * i)
    # Eirik's sword plays s_hit when it lands (the weapon's own hit sound is the hit moment in the audio log).
    audio = {'sounds': {n: {'file': 'audio/%s.wav' % n} for n in SOUNDS}}
    write_json(os.path.join(g, 'data', 'audio.json'), audio)
    if sword_hit:
        edit(g, 'data/weapons.json', lambda w: w['weapons'][0].update({'sounds': {'hit': 's_hit'}}))
    anims = lambda sheet: {k: {'sheet': sheet, 'frames': [[0, 0], [0, 0], [0, 0], [0, 0]], 'ms': [100, 100, 100, 100], 'hit': 2,
                               **({'sounds': {'2': 's_anim'}} if k in ('attack', 'cast') else {})}
                           for k in ('stand', 'attack', 'crit', 'ranged', 'cast', 'dodge', 'hurt', 'defeat', 'miss')}
    cells = lambda sid, f, w, h: {'id': sid, 'file': 'art/%s.png' % f, 'cellWidth': w, 'cellHeight': h}
    gfx = {'sheets': [cells('stage_bg', 'stage_bg', 240, 160), cells('plat', 'plat', 64, 32), cells('blue', 'blue', 48, 48), cells('red', 'red', 48, 48),
                      cells('fx_screen', 'fx_screen', screen_size[0], screen_size[1]), cells('fx_blob', 'fx_blob', 40, 40),
                      cells('fx_charge', 'fx_charge', 16, 16), cells('fx_spark', 'fx_spark', 16, 16), cells('fx_arrow', 'fx_arrow', 8, 8)],
           'battle': {'sets': {'blue': {'anchor': [24, 46], 'animations': anims('blue')},
                               'red': {'anchor': [24, 46], 'animations': anims('red')}},
                      'assign': [{'set': 'blue', 'side': 'blue'}, {'set': 'red', 'side': 'red'}],
                      'backgrounds': {'arena': {'sheet': 'stage_bg', 'cell': [0, 0]}}, 'defaultBackground': 'arena',
                      'platforms': {'slab': PLAT}, 'defaultPlatform': 'slab',
                      'effects': effects}}
    if stage:
        gfx['battle']['stage'] = STAGE
    write_json(os.path.join(g, 'data', 'graphics.json'), gfx)
    edit(g, 'game.json', lambda gj: gj.update({'battleScene': 'scene', 'battleSceneSides': sides}))
    edit(g, 'data/characters.json', lambda c: [ch.pop('quotes', None) for ch in c['characters']])
    if phone:
        edit(g, 'game.json', lambda gj: gj.update({'view': {'width': 270, 'height': 480, 'tileSize': 24}}))
    if patch:
        patch(g)
    return g


IDLE = 'wait:14'
ATTACK = [IDLE, 'confirm', 'wait:3', 'confirm', 'wait:3', 'confirm', 'wait:3', 'confirm', 'wait:400']


def run(gid, frames=420, shot=None):
    """One windowless run; returns (log lines of the player, audio-log lines as (t, name))."""
    work = os.path.join(TMP, 'saves_' + gid)
    shutil.rmtree(work, ignore_errors=True)
    alog = os.path.join(TMP, gid + '.audio.log')
    if os.path.exists(alog):
        os.remove(alog)
    cmd = [PLAYER, '--games', TMP, '--assets', os.path.join(REPO, 'assets'), '--headless', '--no-settings', '--saves', work,
           '--frames', str(frames), '--input-interval', '4', '--fixed-dt', str(DT), '--game', gid, '--map', 'ch1',
           '--audio-log', alog, '--input', ','.join(ATTACK)]
    if shot:
        cmd += ['--screenshot', shot]
    p = run_text(cmd)
    sounds = []
    if os.path.exists(alog):
        for line in open(alog).read().splitlines():
            parts = line.split()
            if len(parts) > 3 and parts[1] == 'sound':
                sounds.append((float(parts[0][2:]), os.path.splitext(os.path.basename(parts[2]))[0]))
    return p.stdout, sounds


def first(sounds, name):
    return next((t for t, n in sounds if n == name), None)


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

    def bbox(self, col, x0=0, x1=None, y0=0, y1=None):
        xs, ys = [], []
        for y in range(y0, self.h if y1 is None else y1):
            for x in range(x0, self.w if x1 is None else x1):
                if self.px(x, y) == col:
                    xs.append(x)
                    ys.append(y)
        return (min(xs), min(ys), max(xs) + 1, max(ys) + 1) if xs else None


def keep(path, name):
    if SHOTS and os.path.exists(path):
        os.makedirs(SHOTS, exist_ok=True)
        shutil.copy(path, os.path.join(SHOTS, name + '.bmp'))


def shot_at(gid, name, delta, W=480, H=270):
    """A screenshot `delta` seconds after the effect's first sound ("probe" at 0 ms) was heard; returns the Shot (None if it was not taken)."""
    log, sounds = run(gid)
    t0 = first(sounds, 'probe')
    check(name + ': the effect started (its probe sound was heard)', t0 is not None, log[-300:])
    if t0 is None:
        return None
    path = os.path.join(TMP, name + '.bmp')
    run(gid, frames=int(round((t0 + delta) / DT)), shot=path)
    keep(path, name)
    if not os.path.exists(path):
        check(name + ': screenshot exists', False)
        return None
    s = Shot(path)
    check(name + ': screenshot is %d x %d' % (W, H), (s.w, s.h) == (W, H), '%d x %d' % (s.w, s.h))
    return s


def place(s, k):
    """The stage's top-left on the screen, read from the backdrop's bottom-left corner mark (the one always in view)."""
    bl = s.bbox(C_BL)
    return (bl[0], bl[3] - 160 * k) if bl else None


def feet(k, ox, oy, side):
    f = STAGE['feet'][side]
    return ox + f[0] * k, oy + f[1] * k


# ---- scenarios ----------------------------------------------------------------------------------------------------------------------------

def sounds_and_lead_in():
    """hitAtMs 150: the effect (and its first sound) starts 150 ms before the weapon's hit sound; the second sound lands with the hit."""
    fx = [{'id': 'zap', 'group': 'blade', 'sheet': 'fx_spark', 'frames': [[0, 0]], 'ms': 300, 'hitAtMs': 150,
           'sounds': {'0': 'probe', '150': 's_fx1', '220': 's_fx2'}}]
    make_game('lead', fx)
    log, snd = run('lead')
    t_probe, t_hit, t_fx1, t_fx2, t_anim = (first(snd, n) for n in ('probe', 's_hit', 's_fx1', 's_fx2', 's_anim'))
    check('lead-in: all sounds were heard', None not in (t_probe, t_hit, t_fx1, t_fx2, t_anim), str(snd))
    if None in (t_probe, t_hit, t_fx1, t_fx2, t_anim):
        return
    check('lead-in: the effect\'s first sound comes 150 ms before the hit (%.3f -> %.3f)' % (t_probe, t_hit), abs((t_hit - t_probe) - 0.150) < 0.034)
    check('lead-in: the effect\'s sound at 150 ms is the hit', abs(t_fx1 - t_hit) < 0.034, '%.3f vs %.3f' % (t_fx1, t_hit))
    check('lead-in: the sound at 220 ms is 70 ms after the hit', abs((t_fx2 - t_hit) - 0.070) < 0.034)
    check('lead-in: the strike\'s hit frame (200 ms into the attack) is the hit moment: no wait', abs(t_anim - t_hit) < 0.034, '%.3f vs %.3f' % (t_anim, t_hit))
    lines = [l for l in log.splitlines() if 'battle scene:' in l]
    start_i = next((i for i, l in enumerate(lines) if 'effect zap start' in l), -1)
    probe_i = next((i for i, l in enumerate(lines) if 'sound probe (effect zap)' in l), -1)
    hit_i = next((i for i, l in enumerate(lines) if 'hit Eirik -> Gorm' in l), -1)
    snd2_i = next((i for i, l in enumerate(lines) if 'sound s_fx2 (effect zap)' in l), -1)
    check('lead-in: log order is effect start, its first sound, the hit, then the late sound', 0 <= start_i < probe_i < hit_i < snd2_i,
          str((start_i, probe_i, hit_i, snd2_i)) + '\n' + '\n'.join(lines))
    # A lead-in longer than the time since the strike began makes the hit wait for it: the effect starts at the strike's start.
    fx2 = [{'id': 'slow', 'group': 'blade', 'sheet': 'fx_spark', 'frames': [[0, 0]], 'ms': 300, 'hitAtMs': 450,
            'sounds': {'0': 'probe', '450': 's_fx1'}}]
    make_game('wait', fx2)
    log, snd = run('wait')
    t_probe, t_hit, t_fx1, t_anim = (first(snd, n) for n in ('probe', 's_hit', 's_fx1', 's_anim'))
    check('long lead-in: the hit waits (450 ms after the effect began, the strike hit frame came at 200 ms)',
          None not in (t_probe, t_hit, t_anim) and abs((t_hit - t_probe) - 0.450) < 0.034 and t_anim < t_hit - 0.15, str(snd))
    check('long lead-in: the effect started with the strike (its probe is the strike\'s start, 200 ms before the hit frame)',
          None not in (t_probe, t_anim) and abs((t_anim - t_probe) - 0.200) < 0.034)


def striker_list_and_by_type():
    """A charge on the striker and a spark on the target from one list; byType picks by the striker's unit type."""
    fx = [{'id': 'charge', 'sheet': 'fx_charge', 'frames': [[0, 0]], 'ms': 5000, 'at': 'striker', 'hitAtMs': 600, 'anchor': [8, 8],
           'sounds': {'0': 'probe'}},
          {'id': 'spark', 'sheet': 'fx_spark', 'frames': [[0, 0]], 'ms': 5000, 'anchor': [8, 8], 'sounds': {'0': 's_fx2'}},
          {'id': 'other', 'sheet': 'fx_spark', 'frames': [[0, 0]], 'ms': 200, 'sounds': {'0': 's_fx1'}},
          {'group': 'blade', 'effects': ['charge', 'spark'], 'byType': {'brute': 'other'}}]
    make_game('list', fx)
    s = shot_at('list', 'effects-striker-and-target', 0.75)
    log, snd = run('list')
    starts = [l.split('battle scene: ')[1] for l in log.splitlines() if 'battle scene: effect ' in l]
    check('list: Eirik (hero) plays the charge then the spark; Gorm (brute) the byType choice "other"',
          starts[:3] == ['effect charge start', 'effect spark start', 'effect other start'] or starts[:2] == ['effect charge start', 'effect spark start'], str(starts))
    check('list: Gorm\'s counter uses its unit type\'s own effect', 'effect other start' in starts and starts.index('effect other start') > starts.index('effect spark start'), str(starts))
    if not s:
        return
    ox, oy = place(s, 2)
    rx, ry = feet(2, ox, oy, 'right')   # Eirik, the striker, stands on the right
    lx, ly = feet(2, ox, oy, 'left')
    bb = s.bbox(C_CHARGE, 0, 480)
    check('striker effect: the white charge (16 x 16, anchor in the middle) sits on the striker\'s foot point', bb == (rx - 16, ry - 16, rx + 16, ry + 16), str((bb, rx, ry)))
    sp = s.bbox(C_SPARK, 0, 480)
    check('target effect of the same list: the green spark sits on the target\'s foot point', sp == (lx - 16, ly - 16, lx + 16, ly + 16), str((sp, lx, ly)))
    check('striker effect: it is not drawn on the target', s.bbox(C_CHARGE, 0, 240) is None)


def screen_layer():
    fx = [{'id': 'wash', 'group': 'blade', 'sheet': 'fx_screen', 'frames': [[0, 0]], 'ms': 5000, 'layer': 'screen', 'sounds': {'0': 'probe'}}]
    for name, W, H, k, kw in (('effects-screen-landscape-contain', 480, 270, 1, {'contain': True}), ('effects-screen-landscape-cover', 480, 270, 2, {}),
                              ('effects-screen-portrait', 270, 480, 1, {'phone': True})):
        stage = dict(STAGE, fit='contain') if kw.get('contain') else STAGE
        gid = name.replace('-', '_')
        make_game(gid, fx, patch=(lambda g, st=stage: edit(g, 'data/graphics.json', lambda gj: gj['battle'].update({'stage': st}))), phone=bool(kw.get('phone')))
        s = shot_at(gid, name, 0.1, W, H)
        if not s:
            continue
        pl = place(s, k)
        if not pl:
            check(name + ': the stage is in view', False)
            continue
        ox, oy = pl
        # The effect picture is drawn at the stage's origin and scale: its corner marks (2 x 2 stage pixels) sit on the stage's corners.
        bl = s.bbox(C_SCREEN_CORNER, 0, W // 2, oy + 150 * k, oy + 160 * k)
        br = s.bbox(C_SCREEN_CORNER, W // 2, W, oy + 150 * k, oy + 160 * k)
        check(name + ': the bottom-left corner mark is on the stage\'s corner (%d, %d), 2 stage pixels' % (ox, oy + 158 * k),
              bl is not None and bl[:2] == (ox, oy + 158 * k) and bl[2] - bl[0] == 2 * k, str(bl))
        check(name + ': the bottom-right corner mark is on the stage\'s right edge, bottom row', br is not None and br[2] == ox + 240 * k and br[3] == oy + 160 * k, str(br))
        if oy >= 0:
            tl = s.bbox(C_SCREEN_CORNER, 0, W // 2, oy, oy + 10 * k)
            check(name + ': the whole stage is in view: the top-left mark is on the stage origin', tl is not None and tl[:2] == (ox, oy), str(tl))
        # The patch lies across the middle: between the fighters it shows, the fighters and the platforms are drawn over it.
        mid = s.px(ox + 120 * k, oy + 70 * k)
        check(name + ': the effect\'s patch shows between the fighters', mid == C_SCREEN_PATCH, str(mid))
        lx, ly = feet(k, ox, oy, 'left')
        check(name + ': the left fighter\'s body is over the effect', s.px(lx, ly - 10 * k) == C_RED_BODY, str(s.px(lx, ly - 10 * k)))
        check(name + ': a fighter\'s foot pixels are over the effect', s.bbox(C_FOOT, 0, W // 2) is not None)
        check(name + ': the platform is drawn over the effect', s.px(ox + 110 * k, oy + 100 * k) == C_PLATFORM, str(s.px(ox + 110 * k, oy + 100 * k)))
    # Without a fixed stage: enlarged by the whole number that covers the screen (160 x 90 -> x3 on 480 x 270).
    gid = 'screen_free'
    make_game(gid, fx, stage=False, screen_size=(160, 90))
    s = shot_at(gid, 'effects-screen-no-stage', 0.1)
    if s:
        # (the opaque band behind the boxes hides the top corners: the bottom ones show)
        bl = s.bbox(C_SCREEN_CORNER, 0, 100, 200, 270)
        br = s.bbox(C_SCREEN_CORNER, 380, 480, 200, 270)
        check('screen effect without a fixed stage: x3 covers the screen, the bottom-left mark (2 x 2 px of the picture = 6 px) is at the corner', bl == (0, 264, 6, 270), str(bl))
        check('screen effect without a fixed stage: the bottom-right mark is at the right edge', br is not None and br[2] == 480 and br[3] == 270, str(br))


def behind_layer():
    fx = [{'id': 'blob', 'group': 'blade', 'sheet': 'fx_blob', 'frames': [[0, 0]], 'ms': 5000, 'layer': 'behind', 'anchor': [20, 20], 'sounds': {'0': 'probe'}}]
    make_game('behind', fx)
    s = shot_at('behind', 'effects-behind-layer', 0.1)
    if not s:
        return
    ox, oy = place(s, 2)
    lx, ly = feet(2, ox, oy, 'left')  # the target: Gorm on the left
    bb = s.bbox(C_BLOB, 0, 240)
    check('behind layer: the effect is on the target\'s foot point (40 x 40, anchor in the middle), cut where the fighter stands in front',
          bb is not None and bb[:2] == (lx - 40, ly - 40) and bb[2:] == (lx + 40, ly + 40), str((bb, lx, ly)))
    check('behind layer: the fighter\'s body is over the effect', s.px(lx, ly - 18) == C_RED_BODY, str(s.px(lx, ly - 18)))
    check('behind layer: next to the body the effect shows', s.px(lx - 24, ly - 18) == C_BLOB, str(s.px(lx - 24, ly - 18)))
    check('behind layer: the effect is in front of the platform (platform pixels at the foot level are covered)', s.px(lx - 24, ly + 8) == C_BLOB, str(s.px(lx - 24, ly + 8)))
    check('behind layer: the platform shows where the effect does not reach', s.px(lx - 60, ly + 8) == C_PLATFORM, str(s.px(lx - 60, ly + 8)))
    # The same picture in the default layer would be over the fighter.
    fx2 = [dict(fx[0], layer='front')]
    del fx2[0]['layer']
    make_game('front', fx2)
    s2 = shot_at('front', 'effects-front-layer-compare', 0.1)
    if s2:
        check('front layer (default): the same effect is over the fighter\'s body', s2.px(lx, ly - 18) == C_BLOB, str(s2.px(lx, ly - 18)))


def returning_projectile():
    fx = [{'id': 'boomerang', 'group': 'blade', 'sheet': 'fx_arrow', 'frames': [[0, 0]], 'ms': 5000, 'mode': 'travel', 'travelMs': 1000, 'returns': True,
           'anchor': [4, 4], 'sounds': {'0': 'probe', '1000': 's_fx1'}}]
    make_game('boom', fx)
    log, snd = run('boom')
    t0, t_hit = first(snd, 'probe'), first(snd, 's_hit')
    check('returning projectile: the hit lands on arrival (1000 ms after it left)', t0 is not None and t_hit is not None and abs((t_hit - t0) - 1.0) < 0.034, str(snd))
    out = shot_at('boom', 'effects-projectile-outbound', 0.25)
    back = shot_at('boom', 'effects-projectile-returning', 1.75)
    if not out or not back:
        return
    ox, oy = place(out, 2)
    rx, ry = feet(2, ox, oy, 'right')   # Eirik, the striker, on the right
    lx, ly = feet(2, ox, oy, 'left')
    for name, s, mirrored in (('outbound', out, True), ('returning', back, False)):
        pa, pb = s.bbox(C_ARROW_A), s.bbox(C_ARROW_B)
        check('projectile %s: both ends are in view' % name, pa is not None and pb is not None, str((pa, pb)))
        if pa is None or pb is None:
            continue
        cx = (min(pa[0], pb[0]) + max(pa[2], pb[2])) / 2
        cy = (pa[1] + pa[3]) / 2
        want = rx + (lx - rx) * 0.25   # a quarter of the way from the striker (out) / a quarter from home (back): the same place
        check('projectile %s: a quarter of the way from the striker\'s foot point (x %.0f, got %.0f)' % (name, want, cx), abs(cx - want) <= 2, str((pa, pb)))
        check('projectile %s: on the line between the feet (y %d)' % (name, ry), abs(cy - ry) <= 2, str(cy))
        # Out: the striker is on the right, so the art (red left, blue right) is turned round: blue on the left. Back: turned round again.
        left_is_blue = pb[0] < pa[0]
        check('projectile %s: drawn %s' % (name, 'turned round (flying left)' if mirrored else 'the right way round again (flying home, to the right)'),
              left_is_blue == mirrored, str((pa, pb)))


def flash_and_shake():
    fx = [{'id': 'lightning', 'group': 'blade', 'sheet': 'fx_spark', 'frames': [[0, 0]], 'ms': 200, 'sounds': {'0': 'probe'},
           'flash': [{'atMs': 0, 'ms': 5000, 'color': '#ff0000'}], 'byType': {}}]
    fx = [dict(fx[0], byType={'brute': []})]
    make_game('flash', fx)
    s = shot_at('flash', 'effects-flash', 0.1)
    fx0 = [dict(fx[0])]
    del fx0[0]['flash']
    make_game('noflash', fx0)
    s0 = shot_at('noflash', 'effects-flash-compare-none', 0.1)
    if s and s0:
        # A backdrop pixel clear of the grid lines and the fighters: stage (10, 70) at scale 2.
        ox, oy = place(s0, 2)
        p, p0 = s.px(ox + 20 + 10, oy + 140 + 10), s0.px(ox + 20 + 10, oy + 140 + 10)
        check('flash: without it the pixel is the backdrop\'s blue (R %s)' % str(p0), p0 is not None and p0[0] < 40 and p0[2] > 60, str(p0))
        check('flash: a red flash turns the same pixel red (%s -> %s)' % (p0, p), p is not None and p[0] > 140 and p[1] < 45 and p[0] > p0[0] + 100, str((p, p0)))
    sh = [dict(fx[0], shake=[{'atMs': 0, 'ms': 400, 'strength': 8}])]
    del sh[0]['flash']
    make_game('shake', sh)
    early = shot_at('shake', 'effects-shake', 0.02)
    late = shot_at('shake', 'effects-shake-over', 0.8)
    if early and late and s0:
        e, l, n = early.bbox(C_PLATFORM), late.bbox(C_PLATFORM), s0.bbox(C_PLATFORM)
        check('shake: at its start the stage is moved (8 stage pixels = 16 px across, 8 up and down) against the quiet picture %s -> %s' % (n, e),
              e is not None and n is not None and abs(e[0] - n[0]) >= 8 and abs(e[1] - n[1]) >= 4, str((e, n)))
        check('shake: once over the stage is back where it was', l is not None and n is not None and l[:2] == n[:2], str((l, n)))


def refusals():
    fx = [{'group': 'blade', 'sheet': 'fx_spark', 'frames': [[0, 0]], 'sounds': {'0': 'no_such_sound'}}]
    g = make_game('badsound', fx)
    work = os.path.join(TMP, 'saves_bad')
    p = subprocess.run([PLAYER, '--games', TMP, '--assets', os.path.join(REPO, 'assets'), '--headless', '--no-settings', '--saves', work, '--frames', '30',
                        '--fixed-dt', str(DT), '--game', 'badsound', '--audio-log', os.path.join(TMP, 'bad.audio.log')],
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True, timeout=120)
    check('a sound id that is not in audio.json is refused when the game loads, naming the effect and the sound',
          'battle effect 1: sound "no_such_sound" is not in data/audio.json "sounds"' in p.stdout, p.stdout[-400:])
    del g


def old_data_unchanged():
    """An effect with none of the new keys logs exactly what it always did (no "effect ... start", no effect sounds)."""
    fx = [{'group': 'blade', 'sheet': 'fx_spark', 'frames': [[0, 0], [0, 0]], 'ms': 80}]
    make_game('oldfx', fx)
    log, snd = run('oldfx')
    lines = [l for l in log.splitlines() if 'battle scene:' in l]
    check('plain effect: the scene played', any('battle scene: start' in l for l in lines))
    check('plain effect: no new log lines (no effect start, no effect sound)', not any('effect ' in l for l in lines), '\n'.join(lines))


def main():
    sounds_and_lead_in()
    striker_list_and_by_type()
    screen_layer()
    behind_layer()
    returning_projectile()
    flash_and_shake()
    refusals()
    old_data_unchanged()


try:
    main()
finally:
    shutil.rmtree(TMP, ignore_errors=True)

summary()
sys.exit(1 if failures else 0)
