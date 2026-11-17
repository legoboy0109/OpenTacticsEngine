#!/usr/bin/env python3
"""The scene stage through the real player, windowless (docs/EVENTS.md "Standing portraits").

  python3 tests/stage_player_test.py ./build/engine/player/ote-player .
  python3 tests/stage_player_test.py ./build/engine/player/ote-player . --shots OUT_DIR   # also keep screenshots (PNG)

Plays tests/fixtures/stage-pres (copied to a temp folder, never changed; its art is drawn by tools/art/make_stage_test_art.py) with
--input scripts, headless, no audio, no settings, no saves:
  - a map event with five portraits that enter (one by fade), talk, move (one slides, one jumps), leave (one by fade) and a
    clearStage: the player logs one "scene: ..." line per command, in order;
  - the same in a campaign scene (enter / move / leave with the scene's own names);
  - screenshots at 480x270 and 270x480 for a person to look at; the Skip tab must sit inside the box, not on a portrait;
  - skipping the scene leaves the same end state as playing it (the logs show the commands marked skipped);
  - a game that uses none of the new keys logs exactly what it logged before (the scene-pres fixture's lines are unchanged).
"""
import os
import shutil
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True

PLAYER = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else ''
REPO = os.path.abspath(sys.argv[2]) if len(sys.argv) > 2 else '.'
SHOTS = sys.argv[sys.argv.index('--shots') + 1] if '--shots' in sys.argv else ''
TMP = tempfile.mkdtemp(prefix='ote-stage-')
passed, failures = 0, []
INTERVAL = 3  # frames per input token


def check(name, cond, detail=''):
    global passed
    if cond:
        passed += 1
        print('PASS ' + name)
    else:
        failures.append(name)
        print('FAIL %s %s' % (name, detail))


def make_game(name, view=None, campaign=False, fixture='stage-pres'):
    """A temp copy of the fixture; `view` = (w, h) changes the logical screen; the campaign file is kept only when asked."""
    games = os.path.join(TMP, 'games')
    dest = os.path.join(games, name)
    shutil.copytree(os.path.join(REPO, 'tests', 'fixtures', fixture), dest)
    if not campaign and os.path.exists(os.path.join(dest, 'data', 'campaign.json')):
        os.remove(os.path.join(dest, 'data', 'campaign.json'))
    if view:
        import json
        p = os.path.join(dest, 'game.json')
        g = json.load(open(p))
        g['view'] = {'width': view[0], 'height': view[1], 'tileSize': 24}
        json.dump(g, open(p, 'w'), indent=1)
    return games


def play(games, game, args, inp, frames, shot=''):
    cmd = [PLAYER, '--games', games, '--game', game, '--headless', '--no-audio', '--no-settings', '--no-saving',
           '--frames', str(frames), '--input-interval', str(INTERVAL)]
    if inp:
        cmd += ['--input', inp]
    if shot:
        cmd += ['--screenshot', shot + '.bmp']
    p = subprocess.run(cmd + args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True, timeout=300)
    png = None
    if shot and os.path.exists(shot + '.bmp'):
        png = keep_shot(shot)
    return p.returncode, p.stdout, png


def keep_shot(base):
    """BMP -> PNG (kept in SHOTS when asked for); returns the image (PIL) or None without PIL."""
    bmp = base + '.bmp'
    img = None
    try:
        from PIL import Image
        img = Image.open(bmp).convert('RGB')
        if SHOTS:
            os.makedirs(SHOTS, exist_ok=True)
            img.save(os.path.join(SHOTS, os.path.basename(base) + '.png'))
    except ImportError:
        if SHOTS:
            os.makedirs(SHOTS, exist_ok=True)
            shutil.copy(bmp, os.path.join(SHOTS, os.path.basename(base) + '.bmp'))
    os.remove(bmp)
    return img


def lines(log, *prefixes):
    out = []
    for l in log.splitlines():
        l = l.split('] ', 1)[1] if l.startswith('[') and '] ' in l else l
        if any(l.startswith(p) for p in prefixes):
            out.append(l)
    return out


def in_order(got, wanted):
    i = 0
    for w in wanted:
        while i < len(got) and not got[i].startswith(w):
            i += 1
        if i >= len(got):
            return False
        i += 1
    return True


# One confirm shows a line whole, the next ends it.
MAP_SCRIPT = 'wait:20,' + ','.join(['confirm,confirm'] * 7) + ',wait:60'
MAP_ORDER = ['scene: background forest cut', 'scene: enter eirik farLeft', 'scene: enter lyra midLeft fade',
             'scene: enter Tobin center', 'scene: enter gorm midRight', 'scene: enter Mira farRight fade',
             'dialogue: Eirik | All five of us at last.', 'dialogue: Lyra | Stand closer, Tobin.', 'scene: move Tobin right',
             'dialogue: Tobin | Here?', 'dialogue: Gorm | Hmph.', 'scene: leave Mira fade',
             'dialogue: Eirik | Mira has gone ahead.', 'scene: portrait eirik farLeft', 'scene: move gorm left cut',
             'dialogue: Gorm | Then we follow.', 'scene: leave (everybody)', 'scene: background (none) cut']
CAMP_SCRIPT = 'confirm,wait:30,' + ',wait:30,'.join(['confirm,confirm'] * 4) + ',wait:80'
CAMP_ORDER = ['scene: background forest cut', 'scene: enter Eirik farLeft', 'scene: enter Lyra left fade', 'scene: enter Gorm farRight',
              'scene: move Lyra midRight', 'scene: leave Gorm fade', 'scene: move Eirik center cut',
              'scene: leave (everybody)', 'scene: background (none) cut']


def run():
    solo = make_game('stage-solo')
    phone = make_game('stage-phone', (270, 480))
    camp = make_game('stage-camp', campaign=True)
    camp_phone = make_game('stage-camp-phone', (270, 480), campaign=True)

    code, log, _ = play(solo, 'stage-solo', ['--map', 'field'], MAP_SCRIPT, 900)
    got = lines(log, 'scene:', 'dialogue:')
    check('map scene: enter, say, move and leave log in the order of the commands', in_order(got, MAP_ORDER), '\n'.join(got))
    check('map scene: the player ran without errors', code == 0 and '[ERROR]' not in log and '[WARN]' not in log, log[-400:])
    check('map scene: nothing is marked skipped', '(skipped)' not in log)

    code, log, _ = play(camp, 'stage-camp', ['--campaign'], CAMP_SCRIPT, 900)
    got = lines(log, 'scene:', 'dialogue:')
    check('campaign scene: enter, move and leave log in the order of the commands', in_order(got, CAMP_ORDER), '\n'.join(got))
    check('campaign scene: the player ran without errors', code == 0 and '[ERROR]' not in log, log[-400:])

    # Skipping: every command still acts, nothing takes time.
    code, log, _ = play(solo, 'stage-solo', ['--map', 'field'], 'wait:20,skip,wait:60', 500)
    check('skip: the scene commands run marked skipped, in order',
          in_order(lines(log, 'scene:', 'dialogue:'), ['scene: move Tobin right cut (skipped)', 'scene: leave Mira (skipped)', 'scene: move gorm left cut (skipped)']),
          '\n'.join(lines(log, 'scene:', 'dialogue:')))

    # Screenshots (a person looks at these).
    shots = [('stage-enter-line', 'wait:20', 45), ('stage-talk', 'wait:20,confirm,confirm,wait:8,confirm,confirm,wait:30', 90),
             ('stage-moved', 'wait:20,' + ','.join(['confirm,confirm'] * 3) + ',wait:30', 125),
             ('stage-left-one', 'wait:20,' + ','.join(['confirm,confirm'] * 5) + ',wait:30', 160)]
    for gname, gdir, tag in (('stage-solo', solo, '480x270'), ('stage-phone', phone, '270x480')):
        for name, script, frames in shots:
            base = os.path.join(TMP, '%s-%s' % (name, tag))
            code, log, img = play(gdir, gname, ['--map', 'field'], script, frames, shot=base)
            check('screenshot %s %s: the player ran' % (name, tag), code == 0 and '[ERROR]' not in log, log[-300:])
            if img is not None:
                w, h = img.size
                check('screenshot %s %s: the size is the logical screen' % (name, tag), (w, h) == ((480, 270) if tag == '480x270' else (270, 480)))
                if name == 'stage-enter-line':  # the speaker (Eirik, bright) is on the stage: his skin colour, unchanged, is there
                    check('screenshot %s %s: the lit speaker is on the stage' % (name, tag), (232, 190, 150) in [c for _, c in img.getcolors(maxcolors=1 << 20)])
    for gname, gdir, tag in (('stage-camp', camp, '480x270'), ('stage-camp-phone', camp_phone, '270x480')):
        code, log, img = play(gdir, gname, ['--campaign'], 'confirm,wait:25,confirm,confirm,wait:10', 110, shot=os.path.join(TMP, 'stage-camp-%s' % tag))
        check('screenshot campaign %s: the player ran' % tag, code == 0 and '[ERROR]' not in log, log[-300:])

    # A game that uses none of the new keys: the scene-pres fixture logs exactly what it always did (the old "left" / "right"
    # lines name the same positions).
    old = make_game('scene-solo', campaign=False, fixture='scene-pres')
    code, log, _ = play(old, 'scene-solo', ['--map', 'field'], 'wait:20,confirm,confirm,wait:8,confirm,confirm,wait:40,confirm,confirm,wait:100', 700)
    got = lines(log, 'scene:')
    check('a game without the new keys logs the old scene lines', 'scene: portrait eirik left' in got and 'scene: portrait lyra right' in got and 'scene: portrait gorm center' in got
          and not any(g.startswith(('scene: enter', 'scene: move')) for g in got), '\n'.join(got))


    # The camera's "speed": the scroll takes as long as its distance needs (from the last focus; the first from the map's middle).
    code, log, _ = play(solo, 'stage-solo', ['--map', 'camera'], '', 200)
    got = lines(log, 'scene: focus')
    check('focus speed: 637 ms for 2.55 squares at 4 squares per second, then 791 ms at 8 (no wait), the plain ms one unchanged, 51 ms at 100',
          got == ['scene: focus 6,1 637 ms (speed 4)', 'scene: focus 0,3 791 ms (speed 8) (no wait)', 'scene: focus 7,1 300 ms',
                  'scene: focus 2,2 51 ms (speed 100)'], '\n'.join(got))


run()
print('%d passed, %d failed' % (passed, len(failures)))
shutil.rmtree(TMP, ignore_errors=True)
sys.exit(1 if failures else 0)
