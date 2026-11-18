#!/usr/bin/env python3
"""The fourth batch of opt-in battle scene keys through the real player, windowless (docs/BATTLE_SCENE.md).

  python3 tests/battle_scene4_player_test.py ./build/engine/player/ote-player .
  python3 tests/battle_scene4_player_test.py ./build/engine/player/ote-player . --shots OUT_DIR   # keep the screenshots (BMP)

Reuses the generated games and helpers of tests/battle_scene3_player_test.py (the staff scene: Wen casts the Mend Staff on wounded Tarn, a 240 x 160
stage, plain-colour pictures, an effect "probe" sound whose time in the --audio-log is the clock of the screenshots); always headless, no sound device.

  - pan "backdrop": "scroll": the backdrop moves with the camera by the same pixels and wraps (no empty strip); absent / "fixed" is the old picture;
  - pan "startMs": the camera leaves that long after the strike starts; absent it arrives as the blow lands;
  - an effect's "atRange": the effect starts later by the delay of the fight's distance (none below the band, none when absent);
  - a travel effect's "endOffset": the shot stops short of the target.
"""
import os
import shutil
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import battle_scene3_player_test as s3  # noqa: E402  (the shared generated games; its main() is not run)

STAGE, USE, C_SPARK, C_BL, C_BR, C_FOOT = s3.STAGE, s3.USE, s3.C_SPARK, s3.C_BL, s3.C_BR, s3.C_FOOT
check, foot_x = s3.check, s3.foot_x


def near(a, b, tol=3):
    return a is not None and b is not None and abs(a - b) <= tol

GLOW = [{'id': 'glow', 'weapon': 'mend_staff', 'sheet': 'fx_spark', 'frames': [[0, 0]], 'ms': 800, 'anchor': [8, 8], 'hitAtMs': 300, 'sounds': {'0': 'probe'}}]
BAND_EACH = s3.BAND_EACH
C_BARS = (10, 13, 28)  # the dark colour of the bars / an unfilled stage: nothing of the stage's own pictures uses it


def with_effect(extra):
    return [dict(GLOW[0], **extra)]


def band(**pan):
    return dict(STAGE, ranges=[dict(BAND_EACH, pan=dict(BAND_EACH['pan'], **pan))])


# ---- 1. pan "backdrop" ---------------------------------------------------------------------------------------------------------------------

def scroll_backdrop():
    shots_by = {}
    for name, pan in (('absent', {}), ('fixed', {'backdrop': 'fixed'}), ('scroll', {'backdrop': 'scroll'})):
        gid = 'bd_' + name
        saves = s3.far_game(gid, GLOW, band(**pan), 0, 6)
        # The caster's view (the right fighter: rightView 200), half way through the pan, and the target's view.
        shots, log = s3.timed_shots(gid, USE + ['wait:120'], saves, {'backdrop-%s-caster' % name: 0.02, 'backdrop-%s-moving' % name: 0.175, 'backdrop-%s-target' % name: 1.0})
        shots_by[name] = shots
    for name in ('absent', 'fixed'):
        for part in ('caster', 'moving', 'target'):
            s = shots_by[name].get('backdrop-%s-%s' % (name, part))
            if s:
                check('backdrop %s, %s: the corner marks stay at the stage corners (the old picture)' % (name, part),
                      s.bbox(C_BL) is not None and s.bbox(C_BL)[0] == shots_by['absent']['backdrop-absent-caster'].bbox(C_BL)[0], str(s.bbox(C_BL)))
    a, f = shots_by['absent'].get('backdrop-absent-moving'), shots_by['fixed'].get('backdrop-fixed-moving')
    check('backdrop: "fixed" is exactly the picture of leaving the key out', a is not None and f is not None and a.d == f.d)
    c, m, t = (shots_by['scroll'].get('backdrop-scroll-' + p) for p in ('caster', 'moving', 'target'))
    if c and m and t:
        ox, oy = s3.place(t, 2) if t.bbox(C_BL) else (0, -50)
        # Caster view = world x 200: the backdrop's column 200 is at the picture's left edge, so its right end (columns 200..239, with the right corner marks
        # at 236) comes first and its left edge (the left corner marks, column 0) 40 stage pixels in; the stage is 240 wide and the picture repeats.
        bl, br = c.bbox(C_BL), c.bbox(C_BR)
        check('backdrop scroll, caster view: the picture is moved by the camera (its left corner mark is 40 stage pixels in, its right mark 36)',
              bl is not None and br is not None and near(bl[0], ox + 40 * 2, 2) and near(br[0], ox + 36 * 2, 2), str((bl, br, ox)))
        # The target's view = world x 0: the picture is where it always was.
        bl = t.bbox(C_BL)
        check('backdrop scroll, target view: the picture is back at the stage corner', bl is not None and near(bl[0], ox, 2), str(bl))
        # Half way (view about 100): between the two; and no strip of the stage is empty at any of the three.
        check('backdrop scroll, half way: the picture is between the two places', m.bbox(C_BL) is None or ox + 2 < m.bbox(C_BL)[0] < ox + 40 * 2 + 400, str(m.bbox(C_BL)))
        for name, s in (('caster', c), ('moving', m), ('target', t)):
            # The row 20 stage pixels down is above every fighter and platform: all of it is backdrop, wrapped, never the bar colour.
            row = [s.px(x, oy + 20 * 2) for x in range(max(0, ox), min(s.w, ox + 480))]
            check('backdrop scroll, %s: the picture repeats, so no empty strip shows' % name, len(row) > 0 and C_BARS not in row, str(row[:3]))
        check('backdrop scroll: the pictures differ from the fixed backdrop (the backdrop really moved)', c.d != shots_by['fixed']['backdrop-fixed-caster'].d)


# ---- 2. pan "startMs" ----------------------------------------------------------------------------------------------------------------------

def pan_start():
    cam = {}
    for name, pan in (('absent', {}), ('late', {'startMs': 1000})):
        gid = 'ps_' + name
        saves = s3.far_game(gid, GLOW, band(**pan), 0, 6)
        shots, log = s3.timed_shots(gid, USE + ['wait:160'], saves, {'start-%s-early' % name: 0.02, 'start-%s-mid' % name: 0.7, 'start-%s-end' % name: 1.6})
        cam[name] = shots
        s3.check('start %s: the fight is a "ranged, 6 squares" scene' % name, any('ranged, 6 squares' in l for l in s3.scene_lines(log)))
    for name in ('absent', 'late'):
        e, m, t = (cam[name].get('start-%s-%s' % (name, p)) for p in ('early', 'mid', 'end'))
        if not (e and m and t):
            continue
        ox, oy = s3.place(t, 2) if t.bbox(C_BL) else (0, -50)
        # Wen (caster) is the right fighter: the camera is on it (foot mark at (400 - 200) * 2) until it moves to Tarn (foot mark at 40 * 2).
        # (the foot mark can be under the effect's picture: the fighter's blue body is the mark that is always in view)
        centre = lambda s: None if s.bbox(s3.C_BLUE_BODY) is None else (s.bbox(s3.C_BLUE_BODY)[0] + s.bbox(s3.C_BLUE_BODY)[2]) // 2
        on_caster = lambda s: centre(s) is not None and near(centre(s), ox + 400, 24)
        on_target = lambda s: centre(s) is not None and near(centre(s), ox + 80, 24)
        check('start %s: at the strike the camera is on the caster' % name, on_caster(e))
        check('start %s: 0.7 s on the camera is %s' % (name, 'still on the caster (it leaves at 1.0 s)' if name == 'late' else 'on the target (it arrived with the blow)'),
              on_caster(m) if name == 'late' else on_target(m), str(centre(m)))
        check('start %s: 1.6 s on the camera is on the target' % name, on_target(t), str(centre(t)))


# ---- 3. an effect's "atRange" --------------------------------------------------------------------------------------------------------------

def probe_time(gid, effects, wen, tarn):
    saves = s3.far_game(gid, effects, STAGE, wen, tarn)
    snd = []
    log, units = s3.run_player(gid, USE + ['wait:120'], saves, frames=900, sounds=snd)
    return s3.first(snd, 'probe'), log


def at_range():
    # Wen at column 1, Tarn at column 5: four squares apart (the far layout, no band).
    base, base_log = probe_time('ar_base', GLOW, 1, 5)
    check('atRange: the plain effect starts (its probe sound is heard)', base is not None, base_log[-300:])
    if base is None:
        return
    for name, steps, want in (
        ('applies', [{'from': 2, 'delayMs': 400}], 0.4),
        ('last band wins', [{'from': 2, 'delayMs': 100}, {'from': 4, 'delayMs': 700}, {'from': 6, 'delayMs': 2000}], 0.7),
        ('band below', [{'from': 2, 'delayMs': 250}, {'from': 5, 'delayMs': 900}], 0.25),
        ('not reached', [{'from': 5, 'delayMs': 900}], 0.0),
        ('zero', [{'from': 2, 'delayMs': 0}], 0.0),
    ):
        t, log = probe_time('ar_' + name.replace(' ', '_'), with_effect({'atRange': steps}), 1, 5)
        check('atRange %s: the effect starts %.0f ms later than without the key (heard at %s s, plain at %.3f s)' % (name, want * 1000, t, base),
              t is not None and abs((t - base) - want) <= 0.04, str((t, base)))
    # The effect, the hit it leads to and the log all come later together: the staff's heal line is still logged once, after the effect started.
    t, log = probe_time('ar_log', with_effect({'atRange': [{'from': 2, 'delayMs': 400}]}), 1, 5)
    sl = s3.scene_lines(log)
    check('atRange: the effect and the heal are both logged, in order', 'effect glow start' in sl and any(l.startswith('staff heal Tarn') for l in sl)
          and sl.index('effect glow start') < [i for i, l in enumerate(sl) if l.startswith('staff heal Tarn')][0], str(sl))
    # Close fight (Wen at 2, Tarn at 3: one square away is not allowed for a staff of range 2? use two squares): a band at 4 does not touch it.
    near_base, _ = probe_time('ar_near_base', GLOW, 3, 5)
    near_t, _ = probe_time('ar_near', with_effect({'atRange': [{'from': 4, 'delayMs': 600}]}), 3, 5)
    check('atRange: a fight at two squares is not delayed by a band that starts at four', near_base is not None and near_t is not None and abs(near_t - near_base) <= 0.02, str((near_base, near_t)))
    # Pixels: a shot taken at the plain effect's start + 0.1 s shows the spark; with the delay it does not yet, and a little later it does.
    gid = 'ar_pix'
    saves = s3.far_game(gid, with_effect({'atRange': [{'from': 2, 'delayMs': 600}]}), STAGE, 1, 5)
    shots, log = s3.timed_shots(gid, USE + ['wait:120'], saves, {'atrange-delayed-start': 0.1})
    s = shots.get('atrange-delayed-start')
    if s:
        check('atRange: with the delay the effect is on screen once its time has come', s.bbox(C_SPARK) is not None)
    path = os.path.join(s3.TMP, 'ar_plain_early.bmp')
    saves = s3.far_game('ar_pix_plain', GLOW, STAGE, 1, 5)
    s3.run_player('ar_pix_plain', USE + ['wait:120'], saves, frames=int(round((base + 0.1) / s3.DT)), shot=path)
    s3.keep(path, 'atrange-plain-start')
    if os.path.exists(path):
        check('atRange: the plain effect is on screen at the same moment', s3.Shot(path).bbox(C_SPARK) is not None)
    path2 = os.path.join(s3.TMP, 'ar_delayed_early.bmp')
    saves = s3.far_game('ar_pix_late', with_effect({'atRange': [{'from': 2, 'delayMs': 600}]}), STAGE, 1, 5)
    s3.run_player('ar_pix_late', USE + ['wait:120'], saves, frames=int(round((base + 0.1) / s3.DT)), shot=path2)
    s3.keep(path2, 'atrange-delayed-early')
    if os.path.exists(path2):
        check('atRange: ... but not yet with the delay (0.6 s later)', s3.Shot(path2).bbox(C_SPARK) is None)


# ---- 4. a travel effect's "endOffset" ------------------------------------------------------------------------------------------------------

def end_offset():
    def bolt(extra):
        return [dict({'id': 'bolt', 'weapon': 'mend_staff', 'sheet': 'fx_spark', 'frames': [[0, 0]], 'ms': 100, 'mode': 'travel', 'travelMs': 300, 'lingerMs': 1500,
                      'anchor': [8, 8], 'sounds': {'0': 'probe'}}, **extra)]
    centres, logs = {}, {}
    for name, extra in (('absent', {}), ('zero', {'endOffset': 0}), ('short', {'endOffset': 40})):
        gid = 'eo_' + name
        saves = s3.far_game(gid, bolt(extra), STAGE, 3, 5)  # two squares apart
        shots, log = s3.timed_shots(gid, USE + ['wait:120'], saves, {'offset-%s' % name: 0.7})
        s = shots.get('offset-' + name)
        bb = s.bbox(C_SPARK) if s else None
        centres[name] = None if bb is None else (bb[0] + bb[2]) // 2
        logs[name] = s3.scene_lines(log)
        if s:
            # The target's foot mark (the left fighter's) is where the plain shot lands.
            check('endOffset %s: the shot is on screen at the target end of its flight' % name, bb is not None, str(bb))
    check('endOffset: 0 and absent land in the same place', centres['absent'] is not None and centres['absent'] == centres['zero'], str(centres))
    if centres['absent'] is not None and centres['short'] is not None:
        # Wen (the right fighter) shoots left at Tarn: stopping 40 stage pixels (80 screen pixels at scale 2) short leaves the shot 80 pixels to the right.
        check('endOffset 40: the shot rests 40 stage pixels (80 screen pixels) short of where it landed before (%s -> %s)' % (centres['absent'], centres['short']),
              near(centres['short'] - centres['absent'], 80, 3), str(centres))
    check('endOffset: the plain shot logs the same lines as without the key', logs['absent'] == logs['zero'], str((logs['absent'], logs['zero'])))


def main():
    try:
        scroll_backdrop()
        pan_start()
        at_range()
        end_offset()
    finally:
        shutil.rmtree(s3.TMP, ignore_errors=True)
    print('%d passed, %d failed' % (s3.passed, len(s3.failures)))
    if s3.failures:
        print('FAILED: ' + ', '.join(s3.failures))
        sys.exit(1)


if __name__ == '__main__':
    main()
