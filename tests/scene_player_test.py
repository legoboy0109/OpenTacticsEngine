#!/usr/bin/env python3
"""Story scene presentation through the real player, windowless (docs/EVENTS.md "Scene presentation").

  python3 tests/scene_player_test.py ./build/engine/player/ote-player .
  python3 tests/scene_player_test.py ./build/engine/player/ote-player . --shots OUT_DIR   # also keep screenshots (PNG)

Plays tests/fixtures/scene-pres (copied to a temp folder, never changed; its own art comes from tools/art/make_scene_test_art.py)
with --input scripts, headless, no audio, no settings. The player logs one "scene: ..." line per new command and the usual
"dialogue: ..." line per line of dialogue; the script reads them:

  - a map event scene using every command (fade, background, two speakers left and right, wait, shake, narrate, auto line,
    leave, background fade, clearStage, focus with ms, weather, walked move) and a campaign before scene using every scene-legal one:
    the log lines come in the order of the commands, and the scene that ends faded out fades in by itself;
  - screenshots at 480x270 and 270x480 (the background with two portraits, mid-fade, narrator line, weather) for a person to look at;
  - skipping (the Skip button during a fade, a skip on a line) leaves exactly the state that playing leaves, in the Continue
    save of the battle and of the campaign;
  - a save made while a background is up (map event) comes back with the same picture and replays nothing; a campaign scene that
    was cut short by closing the window plays again from its card;
  - --autoplay never waits on a scene.
"""
import json
import os
import shutil
import sys
import tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import run_text, check, failures, player_and_repo, shots_dir, summary  # noqa: E402

sys.dont_write_bytecode = True

PLAYER, REPO = player_and_repo()
SHOTS = shots_dir()
TMP = tempfile.mkdtemp(prefix='ote-scene-')
INTERVAL = 3  # frames per input token (wait:N = N tokens)

# The map event scene, key by key: confirm each line twice (show it whole, end it), wait out the timed commands.
# fade 300 + fade 300 take ~40 frames; the auto line goes on by itself after 600 ms.
MAP_SCRIPT = 'wait:20,confirm,confirm,wait:8,confirm,confirm,wait:40,confirm,confirm,wait:100'
# The campaign scene: the card, then the same shape.
CAMP_SCRIPT = 'confirm,wait:25,confirm,confirm,wait:8,confirm,confirm,wait:40,confirm,confirm,confirm,wait:30,confirm,confirm,wait:60,confirm,wait:60'


def make_game(name, view=None, campaign=True):
    """A temp copy of the fixture; `view` = (width, height) to change the logical screen (the phone shape); `campaign` False =
    the same game without data/campaign.json (a single map: its Continue save is written, a campaign game's free battle writes none)."""
    games = os.path.join(TMP, 'games')
    dest = os.path.join(games, name)
    shutil.copytree(os.path.join(REPO, 'tests', 'fixtures', 'scene-pres'), dest)
    if not campaign:
        os.remove(os.path.join(dest, 'data', 'campaign.json'))
    if view:
        p = os.path.join(dest, 'game.json')
        g = json.load(open(p))
        g['view'] = {'width': view[0], 'height': view[1], 'tileSize': 24}
        json.dump(g, open(p, 'w'), indent=1)
    return games


def play(games, game, args, inp, frames, saves=None, shot=''):
    cmd = [PLAYER, '--games', games, '--game', game, '--headless', '--no-audio', '--no-settings', '--frames', str(frames),
           '--input-interval', str(INTERVAL)]
    cmd += ['--saves', saves] if saves else ['--no-saving']
    if inp:
        cmd += ['--input', inp]
    if shot:
        cmd += ['--screenshot', shot + '.bmp']
    p = run_text(cmd + args)
    if shot:
        keep_shot(shot)
    return p.returncode, p.stdout


def keep_shot(base):
    """BMP -> PNG in SHOTS (when asked for), the BMP always removed."""
    bmp = base + '.bmp'
    if not os.path.exists(bmp):
        return
    if SHOTS:
        os.makedirs(SHOTS, exist_ok=True)
        try:
            from PIL import Image
            Image.open(bmp).save(os.path.join(SHOTS, os.path.basename(base) + '.png'))
        except ImportError:
            shutil.copy(bmp, os.path.join(SHOTS, os.path.basename(base) + '.bmp'))
    os.remove(bmp)


def lines(log, *prefixes):
    """The log lines (without the "[info] ") that start with one of the prefixes, in order."""
    out = []
    for l in log.splitlines():
        l = l.split('] ', 1)[1] if l.startswith('[') and '] ' in l else l
        if any(l.startswith(p) for p in prefixes):
            out.append(l)
    return out


def in_order(got, wanted):
    """True when every wanted prefix is found in `got` in this order (other lines may lie between)."""
    i = 0
    for w in wanted:
        while i < len(got) and not got[i].startswith(w):
            i += 1
        if i >= len(got):
            return False
        i += 1
    return True


def read_save(saves, game='scene-solo'):
    path = os.path.join(saves, game, 'continue.json')
    return json.load(open(path)) if os.path.exists(path) else None


MAP_ORDER = ['scene: fade out 300 ms #000000', 'scene: background forest cut', 'scene: fade in 300 ms',
             'dialogue: Eirik | The forest is quiet.', 'scene: portrait eirik left',
             'dialogue: Lyra | Too quiet.', 'scene: portrait lyra right',
             'scene: wait 400 ms', 'scene: shake 300 ms strength 3',
             'dialogue:  | Something stirs in the dark.', 'scene: narrate',
             'dialogue: Gorm | Who goes there?', 'scene: portrait gorm center',
             'scene: leave gorm', 'scene: background (none) fade 300 ms', 'scene: leave (everybody)',
             'scene: focus 6,1 400 ms', 'scene: weather snow', 'scene: walk lyra 3 steps']

CAMP_ORDER = ['scene: fade out 200 ms #102040', 'scene: background forest cut', 'scene: fade in 200 ms',
              'scene: portrait Eirik left', 'scene: portrait Lyra right',
              'scene: wait 300 ms', 'scene: shake 200 ms strength 3', 'scene: narrate',
              'scene: background hall fade 300 ms', 'scene: leave Lyra', 'scene: leave (everybody)',
              'scene: background (none) cut', 'scene: fade out 200 ms #000000',
              'scene: fade in 500 ms (automatic, the scene ended faded out)']


def run():
    games = make_game('scene-pres')
    phone = make_game('scene-pres-phone', (270, 480))
    solo = make_game('scene-solo', campaign=False)  # the map without the campaign (saves)
    solo_phone = make_game('scene-solo-phone', (270, 480), campaign=False)

    # 1. The map event scene: every command, in order.
    code, log = play(solo, 'scene-solo', ['--map', 'field'], MAP_SCRIPT, 700)
    got = lines(log, 'scene:', 'dialogue:')
    check('map scene: every command logs in the order of the commands', in_order(got, MAP_ORDER), '\n'.join(got))
    check('map scene: the player ran without errors', code == 0 and '[ERROR]' not in log, log[-400:])
    check('map scene: the lines say nothing of a skip', '(skipped)' not in log)

    # 2. The campaign scene: card, every scene-legal command, fade in by itself at the end.
    code, log = play(games, 'scene-pres', ['--campaign'], CAMP_SCRIPT, 900)
    got = lines(log, 'scene:', 'dialogue:')
    check('campaign scene: every command logs in the order of the commands', in_order(got, CAMP_ORDER), '\n'.join(got))
    check('campaign scene: a scene that ends faded out fades in by itself', 'fade in 500 ms (automatic' in log)
    check('campaign scene: the battle follows (its own event scene starts)', in_order(got, CAMP_ORDER + ['scene: fade out 300 ms #000000']), '\n'.join(got))

    # 3. Screenshots, desktop and phone (a person looks at these: docs/EVENTS.md "Scene presentation").
    shots = [('map-midfade', MAP_SCRIPT, 20, ['--map', 'field']),
             ('map-two-portraits', MAP_SCRIPT, 85, ['--map', 'field']),
             ('map-narrator', MAP_SCRIPT, 215, ['--map', 'field']),
             ('map-snow', MAP_SCRIPT, 600, ['--map', 'field']),
             ('camp-two-portraits', CAMP_SCRIPT, 100, ['--campaign']),
             ('camp-narrator', CAMP_SCRIPT, 200, ['--campaign']),
             ('camp-hall-fade', CAMP_SCRIPT, 250, ['--campaign'])]
    for gname, gdir, tag in (('scene-pres', games, 'desktop'), ('scene-pres-phone', phone, 'phone')):
        for name, script, frames, args in shots:
            base = os.path.join(TMP, '%s-%s' % (name, tag))
            code, log = play(gdir, gname, args, script, frames, shot=base)
            check('screenshot %s %s: the player ran' % (name, tag), code == 0 and '[ERROR]' not in log, log[-300:])
            if SHOTS and not os.path.exists(os.path.join(SHOTS, '%s-%s.png' % (name, tag))):
                check('screenshot %s %s: kept' % (name, tag), False)

    # 4. Skipping leaves the same state as playing: the Continue save of the battle (map event scene).
    played = os.path.join(TMP, 'sv_played')
    skipped = os.path.join(TMP, 'sv_skipped')
    skipped2 = os.path.join(TMP, 'sv_skipped2')
    play(solo, 'scene-solo', ['--map', 'field'], MAP_SCRIPT + ',wait:40', 1100, saves=played)
    code, log = play(solo, 'scene-solo', ['--map', 'field'], 'wait:20,skip,wait:80', 700, saves=skipped)
    check('skip on a line: the lines after it pass and are marked skipped, timed commands too',
          'dialogue: Lyra | Too quiet. (skipped)' in log and 'scene: wait 400 ms (skipped)' in log and 'scene: shake 300 ms strength 3 (skipped)' in log, log[-600:])
    code, log = play(solo, 'scene-solo', ['--map', 'field'], 'wait:3,skip,wait:80', 700, saves=skipped2)
    check('skip during the first fade: the whole scene is skipped, fades and waits take no time',
          'scene: wait 400 ms (skipped)' in log and 'scene: shake 300 ms strength 3 (skipped)' in log and 'dialogue: Gorm | Who goes there? (skipped)' in log,
          log[-600:])
    a, b, c = read_save(played), read_save(skipped), read_save(skipped2)
    check('the Continue saves exist after played and skipped scenes', a and b and c and 'battle' in a and 'battle' in b and 'battle' in c)
    if a and b and c:
        check('skipped (on a line) and played scenes leave the same battle in the save', a['battle'] == b['battle'], json.dumps(b['battle'])[:300])
        check('skipped (during a fade) and played scenes leave the same battle in the save', a['battle'] == c['battle'], json.dumps(c['battle'])[:300])
        check('the walked unit stands where the scene put it, weather saved', a['battle']['story']['scene'] == {'weather': 'snow'}
              and any(u.get('cell') == 2 * 8 + 3 for u in a['battle']['units'] if u.get('id') == 'lyra'), json.dumps(a['battle'])[:400])

    # ... and the campaign: the playthrough after the scene (Continue written when the scene ends).
    cp, cs = os.path.join(TMP, 'sv_cplayed'), os.path.join(TMP, 'sv_cskipped')
    play(games, 'scene-pres', ['--campaign'], CAMP_SCRIPT, 900, saves=cp)
    code, log = play(games, 'scene-pres', ['--campaign'], 'confirm,wait:5,skip,wait:60', 900, saves=cs)
    check('campaign skip: the rest of the scene took effect at once', 'scene: background hall fade 300 ms (skipped)' in log and 'scene: wait 300 ms (skipped)' in log, log[-500:])
    a, b = read_save(cp, 'scene-pres'), read_save(cs, 'scene-pres')
    check('campaign: the Continue save after played and skipped scenes is the same', a and b and a['playthrough'] == b['playthrough'],
          json.dumps(b)[:300] if b else 'no save')

    # 5. A save made with a background up (turn 2's event) comes back with the same picture; nothing replays.
    sv = os.path.join(TMP, 'sv_hall')
    end_turn = ',wait:20,menu,confirm,wait:90,confirm,confirm,wait:60'
    code, log = play(solo, 'scene-solo', ['--map', 'field'], MAP_SCRIPT + end_turn, 1400, saves=sv)
    s = read_save(sv)
    check('turn 2 event: the hall and the rain are in the Continue save',
          s and 'battle' in s and s['battle']['story'].get('scene') == {'background': 'hall', 'weather': 'rain'}, json.dumps(s)[:300] if s else 'no save')
    base = os.path.join(TMP, 'continue-hall-desktop')
    code, log = play(solo, 'scene-solo', ['--continue'], 'wait:5', 60, saves=sv, shot=base)
    check('continue: the saved scene replays nothing (no scene or dialogue line)', not lines(log, 'scene:', 'dialogue:'), '\n'.join(lines(log, 'scene:', 'dialogue:')))
    check('continue: the player ran', code == 0 and '[ERROR]' not in log, log[-300:])
    if SHOTS:
        code, log = play(solo_phone, 'scene-solo-phone', ['--continue'], 'wait:5', 60, saves=sv, shot=os.path.join(TMP, 'continue-hall-phone'))

    # 6. A scene in progress is never saved: closing the window mid-scene leaves no Continue; one that has ended is not replayed.
    sv = os.path.join(TMP, 'sv_cut')
    play(games, 'scene-pres', ['--campaign'], 'confirm,wait:30', 90, saves=sv)
    check('campaign: closing the window mid-scene leaves no Continue save (it is shown again from its card next time)', read_save(sv, 'scene-pres') is None)
    sv = os.path.join(TMP, 'sv_done')
    play(games, 'scene-pres', ['--campaign'], CAMP_SCRIPT, 900, saves=sv)
    check('campaign: the Continue save written when the scene ended exists', read_save(sv, 'scene-pres') is not None)
    code, log = play(games, 'scene-pres', ['--continue'], 'wait:10', 120, saves=sv)
    check('campaign: Continue after the scene does not play it again', 'scene: fade out 200 ms #102040' not in log and 'scene: background forest cut' in log, log[-300:])

    # 7. --autoplay never waits on a scene.
    code, log = play(games, 'scene-pres', ['--campaign', '--autoplay'], '', 3000)
    check('autoplay: the story plays through (scene, battle, end)', code == 0 and '[ERROR]' not in log and 'scene: fade out 200 ms #102040' in log,
          log[-400:])


run()
summary()
shutil.rmtree(TMP, ignore_errors=True)
sys.exit(1 if failures else 0)
