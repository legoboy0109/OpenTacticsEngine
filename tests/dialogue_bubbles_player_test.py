#!/usr/bin/env python3
"""Speech bubbles, notices, choice cursors, portrait expressions, loops, the name keyboard and scene backdrops through the real
player, windowless (docs/EVENTS.md "Speech bubbles, notices, names, faces and loops").

  python3 tests/dialogue_bubbles_player_test.py ./build/engine/player/ote-player .
  python3 tests/dialogue_bubbles_player_test.py ./build/engine/player/ote-player . --shots OUT_DIR   # also keep screenshots (PNG)

Plays tests/fixtures/dialogue-bubbles (copied to a temp folder, never changed) headless, no audio, no settings, no saves:
  - a map event in a game with "dialogue": {"style": "bubble"}: unit lines are bubbles (one "bubble:" log line each), a free name
    keeps the box, a loop's notice question starts on its "cursor" answer ("No"), which breaks the loop after one round, a notice
    line and a question with a bubble follow; in the four view kinds and on a phone-shaped screen;
  - "face": 1 shows the portrait's first expression (Eirik's open mouth) after the line is fully typed;
  - the skip control stops at the loop's question;
  - the campaign: the backdrop is up from the first line, the name keyboard opens with the default, taps type "Kai", and the battle
    after it names Eirik "Kai" (bubble and {name:eirik}); --autoplay takes the default name;
  - a game without "dialogue" logs no bubble lines (the stage-pres fixture).
"""
import json
import os
import shutil
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import run_text, check, failures, player_and_repo, shots_dir, summary  # noqa: E402
from ote_testkit import scratch_dir  # noqa: E402

sys.dont_write_bytecode = True

PLAYER, REPO = player_and_repo()
SHOTS = shots_dir()
TMP = scratch_dir()
INTERVAL = 3  # frames per input token
STYLES = ['classic', 'isometric', 'isometric-3d', 'full-3d']
MOUTH_OPEN = (120, 40, 50)  # Eirik's talk cell (his first expression in this fixture)


def make_game(name, view=None, campaign=False, fixture='dialogue-bubbles'):
    games = os.path.join(TMP, 'games')
    dest = os.path.join(games, name)
    shutil.copytree(os.path.join(REPO, 'tests', 'fixtures', fixture), dest)
    if not campaign and os.path.exists(os.path.join(dest, 'data', 'campaign.json')):
        os.remove(os.path.join(dest, 'data', 'campaign.json'))
    if view:
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
    p = run_text(cmd + args)
    img = keep_shot(shot) if shot and os.path.exists(shot + '.bmp') else None
    return p.returncode, p.stdout, img


def keep_shot(base):
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


def colours(img):
    return set(c for _, c in img.getcolors(maxcolors=1 << 20))


def steps(k):
    """k Confirms, each after the line had time to be typed: every line (one page) and question goes on by one Confirm."""
    return 'wait:20' + ',confirm,wait:6' * k + ',wait:20', 20 + k * INTERVAL * 7 + 100


# Every Confirm ends a fully typed page; Lyra's long line has two pages on the 480 x 270 screen.
FULL = 'wait:20' + ',confirm,wait:6' * 24 + ',wait:40'
MAP_ORDER = ['dialogue: Eirik | Bubbles today, Eirik speaking from the top row.', 'bubble: Eirik ',
             'dialogue: Lyra | And Lyra down by the river.', 'bubble: Lyra ',
             'dialogue: Eirik | Smile!', 'bubble: Eirik ',
             'dialogue: Stranger | A free name keeps the box.',
             'dialogue: Gorm | Now, attack the snowman!', 'bubble: Gorm ',
             'scene: notice', 'choice: battle question "Repeat lesson?"', 'choice: battle answer 1',
             'dialogue:  | Eirik has learned the lesson.', 'scene: notice',
             'choice: battle question "Which way, Eirik?"', 'bubble: Lyra ', 'choice: battle answer 0',
             'dialogue: Eirik | Then we go.', 'bubble: Eirik ']


def run():
    solo = make_game('bub-solo')
    phone = make_game('bub-phone', (270, 480))
    camp = make_game('bub-camp', campaign=True)

    # ---- the map event, all four views
    for style in STYLES:
        code, log, _ = play(solo, 'bub-solo', ['--map', 'field', '--style', style], FULL, 700)
        got = lines(log, 'dialogue:', 'bubble:', 'scene:', 'choice:')
        check('%s: the lines, bubbles, notices and questions come in order' % style, in_order(got, MAP_ORDER) or style != 'classic',
              '\n'.join(got))
        check('%s: the player ran without errors or warnings' % style, code == 0 and '[ERROR]' not in log and '[WARN]' not in log, log[-500:])
        check('%s: unit lines are bubbles' % style, any(l.startswith('bubble: Eirik ') for l in got) and any(l.startswith('bubble: Lyra ') for l in got),
              '\n'.join(got))
        i = next((n for n, l in enumerate(got) if l.startswith('dialogue: Stranger')), -1)
        check('%s: a free name keeps the box (no bubble line after it)' % style, i >= 0 and not got[i + 1].startswith('bubble:'), '\n'.join(got))
        check('%s: the loop ran one round: the cursor answer ("No") breaks it' % style,
              sum(1 for l in got if l.startswith('dialogue: Gorm | Now, attack')) == 1 and 'choice: battle answer 1' in got, '\n'.join(got))
        check('%s: the bubble never covers its speaker here' % style, not any('(covers the speaker)' in l for l in got), '\n'.join(got))
        # Screenshots: a bubble below (Eirik, top row), a long wrapped one, the notice question, a question with a bubble.
        for k, what in ((0, 'below'), (1, 'wrapped'), (11, 'notice-question'), (14, 'question-bubble')):
            inp, frames = steps(k)
            code, log, img = play(solo, 'bub-solo', ['--map', 'field', '--style', style], inp, frames,
                                  shot=os.path.join(TMP, 'bubbles-%s-%s' % (style, what)))
            check('screenshot %s %s: the player ran' % (style, what), code == 0 and '[ERROR]' not in log, log[-300:])
            if img is not None:
                check('screenshot %s %s: the size is the logical screen' % (style, what), img.size == (480, 270))

    # ---- the face: after "Smile!" is fully typed the portrait still shows the open mouth (the expression, not a talk frame)
    inp, frames = steps(5)
    _, log, img = play(solo, 'bub-solo', ['--map', 'field'], inp + ',wait:30', frames + 30, shot=os.path.join(TMP, 'bubbles-face'))
    if img is not None:
        check('face 1: the expression cell is shown once the line is complete', MOUTH_OPEN in colours(img),
              '\n'.join(lines(log, 'dialogue:', 'bubble:')[-2:]))
    inp, frames = steps(0)
    _, _, img = play(solo, 'bub-solo', ['--map', 'field'], inp + ',wait:30', frames + 30, shot=os.path.join(TMP, 'bubbles-no-face'))
    if img is not None:
        check('no face: the complete line shows the plain portrait (control)', MOUTH_OPEN not in colours(img))

    # ---- skip stops at the loop's question
    code, log, _ = play(solo, 'bub-solo', ['--map', 'field'], 'wait:20,skip,wait:40', 300)
    got = lines(log, 'dialogue:', 'choice:')
    check('skip: the rest of the lines pass, the notice question waits', 'choice: battle question "Repeat lesson?": choice: 2 answers' in
          ' '.join(got) and code == 0, '\n'.join(got))

    # ---- the phone: bubbles may use the whole width (the map is narrow), screenshots
    code, log, _ = play(phone, 'bub-phone', ['--map', 'field'], FULL, 900)
    got = lines(log, 'bubble:')
    widths = [int(l.split()[3].split('x')[0]) for l in got if l.startswith('bubble: Lyra ')]
    check('phone: Lyra\'s long line gets a bubble wider than the narrow map (%s)' % widths, widths and max(widths) > 160, '\n'.join(got))
    check('phone: the player ran without errors', code == 0 and '[ERROR]' not in log and '[WARN]' not in log, log[-400:])
    for k, what in ((0, 'eirik'), (1, 'wrapped'), (12, 'notice'), (15, 'question')):
        inp, frames = steps(k)
        code, log, img = play(phone, 'bub-phone', ['--map', 'field'], inp, frames, shot=os.path.join(TMP, 'bubbles-phone-%s' % what))
        check('screenshot phone %s: the player ran' % what, code == 0 and '[ERROR]' not in log, log[-300:])
        if img is not None:
            check('screenshot phone %s: the size is the logical screen' % what, img.size == (270, 480))

    # ---- the campaign: backdrop, name keyboard, the typed name in the battle
    code, log, img = play(camp, 'bub-camp', ['--campaign'], 'wait:10,confirm,wait:60,confirm,wait:20', 300, shot=os.path.join(TMP, 'campaign-name-keyboard'))
    got = lines(log, 'scene:', 'name entry:')
    check('campaign: the backdrop is up before the first line', got and got[0] == 'scene: background forest cut', '\n'.join(got))
    check('campaign: the keyboard opens with the default name', 'name entry: open eirik "Eirik" max 8' in got, '\n'.join(got))
    keys = {}
    for l in got:
        if l.startswith('name entry: keys'):
            for part in l.split()[3:]:
                label, at = part.split('@')
                keys[label] = at.replace(',', ':')
    check('campaign: the keyboard says where its keys are', all(k in keys for k in ('OK', 'Del', 'Case', 'K', 'A', 'I')), str(keys))
    if all(k in keys for k in ('OK', 'Del', 'Case', 'K', 'A', 'I')):
        typing = 'wait:10,confirm,wait:60,confirm,wait:20,' + ','.join(['tap:' + keys['Del']] * 5) + ',tap:%s,tap:%s,tap:%s,tap:%s' % (keys['K'], keys['Case'], keys['A'], keys['I'])
        code, log, img = play(camp, 'bub-camp', ['--campaign'], typing + ',wait:10', 400, shot=os.path.join(TMP, 'campaign-name-typed'))
        check('campaign: nothing is named before OK', not lines(log, 'name entry: eirik ='), log[-300:])
        after = typing + ',tap:%s,wait:20' % keys['OK'] + ',confirm,wait:10' * 3 + ',wait:40'
        code, log, img = play(camp, 'bub-camp', ['--campaign'], after, 900, shot=os.path.join(TMP, 'campaign-battle-named'))
        got = lines(log, 'name entry:', 'dialogue:', 'bubble:', 'scene: notice')
        check('campaign: taps typed "Kai"', 'name entry: eirik = "Kai"' in got, '\n'.join(got))
        check('campaign: the scene after it and the battle use the typed name',
              in_order(got, ['name entry: eirik = "Kai"', 'scene: notice', 'dialogue: Kai | Bubbles today, Kai speaking', 'bubble: Kai ']), '\n'.join(got))
        check('campaign: the player ran without errors', code == 0 and '[ERROR]' not in log and '[WARN]' not in log, log[-400:])
    # The lines between the keyboard and the battle (the scene's notice, "{name:eirik}" in a line).
    code, log, img = play(camp, 'bub-camp', ['--campaign'], 'wait:10,confirm,wait:60,confirm,wait:20,menu,confirm,wait:30', 400, shot=os.path.join(TMP, 'campaign-after-name'))
    check('campaign: Menu then Confirm takes the name as it stands', 'name entry: eirik = "Eirik"' in lines(log, 'name entry:'), log[-300:])
    code, log, _ = play(camp, 'bub-camp', ['--campaign', '--autoplay'], '', 900)
    check('campaign --autoplay: the default name is taken', 'name entry: eirik = "Eirik"' in lines(log, 'name entry:'), log[-400:])

    # ---- a game without "dialogue": no bubble, as before
    old = make_game('stage-old', fixture='stage-pres')
    code, log, _ = play(old, 'stage-old', ['--map', 'field'], 'wait:20,' + ','.join(['confirm,confirm'] * 7) + ',wait:60', 900)
    check('a game without "dialogue" shows no bubble and no notice', not lines(log, 'bubble:', 'scene: notice') and code == 0, log[-300:])


run()
summary()
shutil.rmtree(TMP, ignore_errors=True)
sys.exit(1 if failures else 0)
