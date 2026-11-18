#!/usr/bin/env python3
"""Free battles that unlock when the story is complete, through the real player, windowless (docs/EVENTS.md "Campaign",
docs/SAVES.md "Progress record").

  python3 tests/freebattles_player_test.py ./build/engine/player/ote-player . [--shots OUT_DIR]

Takes the campaign test's small games, gives one of them  "freeBattles": { "maps": ["skirmish"], "when": "storyComplete" }
and checks on the game page (its log lines and, with --shots, 480x270 pictures): no list at first, the note when the game
has one, the story autoplayed to its end writes progress.json and the list appears, deleting the Continue save keeps it,
a test playthrough (--chapter) or a lost story does not unlock, a finished save made before the record counts, --map still
starts any map, and the old forms are listed as before. Everything runs with --saves on a temp folder.
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import campaign_player_test as ct  # noqa: E402  (its games and input recipes)

PLAYER = ''
GAMES = ''
SHOTS = ''
failures = []
passed = 0
NOTE = 'Finish the story to unlock free battles.'


def play(args, saves=None, frames=1500, interval=8, inp=None, shot=None):
    cmd = [PLAYER, '--games', GAMES, '--headless', '--no-audio', '--no-settings', '--frames', str(frames),
           '--input-interval', str(interval), '--fixed-dt', '0.016']
    cmd += ['--saves', saves] if saves else ['--no-saving']
    if inp:
        cmd += ['--input', inp]
    if shot:
        cmd += ['--screenshot', shot]
    p = subprocess.run(cmd + args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True, timeout=300)
    return p.returncode, p.stdout


def check(name, cond, detail=''):
    global passed
    if cond:
        passed += 1
        print('PASS ' + name)
    else:
        failures.append(name)
        print('FAIL %s %s' % (name, detail))


def progress(saves, game):
    path = os.path.join(saves, game, 'progress.json')
    return json.load(open(path)) if os.path.exists(path) else None


def set_free(game, value):
    path = os.path.join(GAMES, game, 'data', 'campaign.json')
    c = json.load(open(path))
    c['freeBattles'] = value
    json.dump(c, open(path, 'w'), indent=1)


def keep_shot(bmp, name):
    if not SHOTS or not os.path.exists(bmp):
        return
    os.makedirs(SHOTS, exist_ok=True)
    try:
        from PIL import Image
        Image.open(bmp).save(os.path.join(SHOTS, name + '.png'))
    except Exception:  # PIL missing: keep the BMP
        shutil.copy(bmp, os.path.join(SHOTS, name + '.bmp'))


def page(saves, game='camp', shot=None, name=None):
    """Opens the game page and returns its log."""
    code, log = play(['--game', game], saves=saves, inp='wait:5', frames=60, shot=shot)
    if shot and name:
        keep_shot(shot, name)
    return log


def scenarios(tmp):
    def fresh(name):
        d = os.path.join(tmp, name)
        shutil.rmtree(d, ignore_errors=True)
        return d

    # camp: one waiting free battle with a note; camp_lose: the same, no note, and the story can end by defeat only.
    set_free('camp', {'maps': ['skirmish'], 'when': 'storyComplete', 'lockedNote': NOTE})
    set_free('camp_end', {'maps': 'all', 'when': 'storyComplete'})
    # (camp_lose keeps its plain "all": the old forms mean "always")

    # 1. Locked at first: no list, the note in its place.
    sv = fresh('s_main')
    bmp = os.path.join(tmp, 'locked.bmp')
    log = page(sv, shot=bmp, name='locked-480x270')
    check('locked: the game page lists no free battles and says why in its log',
          'game page: free battles locked (1 map(s)' in log and 'note shown' in log and 'free battles listed' not in log, log[-300:])
    check('locked: nothing is written just by looking', progress(sv, 'camp') is None)

    # 2. Without a note: just absent.
    log = page(fresh('s_nonote'), game='camp_end')
    check('locked, no note: the list is simply absent', 'free battles locked' in log and 'note shown' not in log, log[-300:])

    # 3. The story autoplayed to its end writes the record; the page now lists the battle.
    code, log = play(['--game', 'camp', '--campaign', '--autoplay'], saves=sv, frames=3000)
    rec = progress(sv, 'camp')
    check('autoplay to the end: the record is written', code == 0 and 'story complete recorded' in log and rec and rec.get('storyComplete') is True
          and isinstance(rec.get('at'), str) and rec['at'].endswith('Z'), log[-300:])
    bmp = os.path.join(tmp, 'unlocked.bmp')
    log = page(sv, shot=bmp, name='unlocked-480x270')
    check('unlocked: the page lists the free battle', 'game page: free battles listed (1)' in log and 'locked' not in log, log[-300:])

    # 4. A free battle played from the list unlocked by the record leaves the record alone.
    before = open(os.path.join(sv, 'camp', 'progress.json')).read()
    code, log = play(['--game', 'camp', '--map', 'skirmish'], saves=sv, inp='wait:30,cancel,menu,confirm,wait:60,confirm,wait:5', frames=400)
    check('a free battle does not touch the record', open(os.path.join(sv, 'camp', 'progress.json')).read() == before, log[-200:])

    # 5. Deleting every save keeps the list (the record is separate).
    sv5 = fresh('s_continue')
    code, log = play(['--game', 'camp', '--campaign'], saves=sv5, inp=ct.CARD_AND_SKIP + ',' + ct.END_TURN)
    check('(setup) a human New game wrote a Continue save, no record', ct.save_of(sv5, 'camp') is not None and progress(sv5, 'camp') is None)
    code, log = play(['--game', 'camp', '--campaign'], saves=sv5, inp=','.join([ct.CH1, ct.CH2, ct.CH3]), frames=3000)
    check('a human story played to the end writes the record', progress(sv5, 'camp') is not None and 'story complete recorded' in log, log[-300:])
    for f in os.listdir(os.path.join(sv5, 'camp')):
        if f != 'progress.json':
            os.remove(os.path.join(sv5, 'camp', f))
    log = page(sv5)
    check('after deleting the saves the free battle is still listed', 'free battles listed (1)' in log, log[-300:])

    # 6. A test playthrough to the end does not unlock (nor write anything), even with a saves folder.
    sv6 = fresh('s_chapter')
    code, log = play(['--game', 'camp', '--campaign', '--chapter', 'ch3', '--autoplay'], saves=sv6, frames=3000)
    check('--chapter: the test run reaches the end', code == 0 and '(won)' in log and 'story complete recorded' not in log, log[-400:])
    check('--chapter: no record', progress(sv6, 'camp') is None)
    code, log = play(['--game', 'camp', '--campaign', '--test-level', '5', '--autoplay'], saves=sv6, frames=3000)
    check('--test-level: no record either', progress(sv6, 'camp') is None and 'story complete recorded' not in log, log[-300:])
    check('after the test runs the list is still locked', 'free battles locked' in page(sv6))
    code, log = play(['--game', 'camp', '--campaign', '--autoplay'], saves=None, frames=3000)
    check('--no-saving: the story ends and nothing is recorded', code == 0 and 'story complete recorded' not in log, log[-300:])

    # 7. A story that ends by defeat ("Story ended") does not unlock.
    sv7 = fresh('s_defeat')
    code, log = play(['--game', 'camp_end', '--campaign', '--autoplay'], saves=sv7, frames=1500)
    check('defeat ending: the story ends and no record is written', code == 0 and progress(sv7, 'camp_end') is None
          and 'story complete recorded' not in log, log[-300:])
    check('after a defeat ending the list is still locked', 'free battles locked' in page(sv7, game='camp_end'))

    # 8. A save of a finished story made before the record existed counts ("Story complete"); an ended one does not.
    sv8 = fresh('s_old')
    code, log = play(['--game', 'camp', '--campaign'], saves=sv8, inp=ct.CARD_AND_SKIP + ',' + ct.END_TURN)
    path = os.path.join(sv8, 'camp', 'continue.json')
    s = json.load(open(path))
    s['playthrough']['phase'] = 'over'
    json.dump(s, open(path, 'w'))
    check('a "Story ended" save does not unlock', 'free battles locked' in page(sv8))
    s['playthrough']['phase'] = 'finished'
    json.dump(s, open(path, 'w'))
    check('a "Story complete" Continue save unlocks', 'free battles listed (1)' in page(sv8))
    shutil.move(path, os.path.join(sv8, 'camp', 'slot2.json'))
    check('a "Story complete" slot unlocks too', 'free battles listed (1)' in page(sv8))

    # 9. --map starts any map of a locked game, listed or not, and leaves the saves alone.
    sv9 = fresh('s_map')
    code, log = play(['--game', 'camp', '--map', 'keep'], saves=sv9, inp='wait:30,cancel,menu,confirm,wait:60,confirm,wait:5', frames=400)
    check('--map starts a map of a locked game (not in its list)', code == 0 and 'battle over: VICTORY' in log and progress(sv9, 'camp') is None, log[-300:])

    # 10. The old forms are listed as before, with no record.
    log = page(fresh('s_old_all'), game='camp_lose')
    check('the plain form "all" lists its battles with no record', 'free battles listed (4)' in log, log[-300:])


def run_all():
    tmp = tempfile.mkdtemp(prefix='ote_freebattles_test_')
    global GAMES
    GAMES = os.path.join(tmp, 'games')
    try:
        ct.make_games(REPO, GAMES)
        ct.PLAYER, ct.GAMES = PLAYER, GAMES
        scenarios(tmp)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


REPO = '.'


def main():
    global PLAYER, REPO, SHOTS
    args = sys.argv[1:]
    if '--shots' in args:
        i = args.index('--shots')
        SHOTS = os.path.abspath(args[i + 1])
        del args[i:i + 2]
    if len(args) < 2:
        print(__doc__)
        return 2
    PLAYER, REPO = os.path.abspath(args[0]), os.path.abspath(args[1])
    run_all()
    print('\n%d passed, %d failed' % (passed, len(failures)))
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
