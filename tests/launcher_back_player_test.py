#!/usr/bin/env python3
"""What Cancel (back / B) does on the launcher's first screen, through the real player, windowless.

  python3 tests/launcher_back_player_test.py ./build/engine/player/ote-player .

platform.h `launcherBack()` answers per platform (Android: leave; UWP / Xbox: ask first; elsewhere: nothing). Linux has
no such platform, so OTE_LAUNCHER_BACK=nothing|exit|confirm stands in for each answer:
  nothing  Cancel leaves the launcher exactly as it was.
  exit     Cancel quits at once.
  confirm  Cancel opens "Exit the player?" (Yes / No, No selected): No stays and the launcher still works, Cancel again
           closes the prompt, Yes quits with exit code 0.
The run is capped with --frames, so a run that stops early was ended by the player, not by the end of the script.
A temp games folder with the three demos keeps the result independent of what else a checkout's games/ holds.
"""
import os
import shutil
import subprocess
import sys
import tempfile

failures = []
passed = 0
PLAYER = REPO = WORK = ''
FRAMES = 600
TAIL = 'wait:60'  # 120 frames at interval 2: long enough that quitting before FRAMES is the player's doing


def check(name, cond, detail=''):
    global passed
    if cond:
        passed += 1
        print('PASS ' + name)
    else:
        failures.append(name)
        print('FAIL ' + name + ('\n' + detail if detail else ''))


def run(mode, inp, shot=None, games=None):
    env = dict(os.environ)
    if mode:
        env['OTE_LAUNCHER_BACK'] = mode
    cmd = [PLAYER, '--games', games, '--assets', os.path.join(REPO, 'assets'), '--headless', '--no-audio',
           '--no-settings', '--no-saving', '--frames', str(FRAMES), '--fixed-dt', '0.016', '--input-interval', '2',
           '--input', inp]
    if shot:
        cmd += ['--screenshot', shot]
    p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True, timeout=120, env=env)
    frames = None
    for line in p.stdout.splitlines():
        if 'quit requested after' in line:  # the player's own exit path (PlayerApp::quit), not the frame cap
            frames = int(line.split('quit requested after')[1].split()[0])
    return p.returncode, p.stdout, frames


def same(a, b):
    return open(a, 'rb').read() == open(b, 'rb').read()


def main():
    global PLAYER, REPO, WORK
    PLAYER = os.path.abspath(sys.argv[1])
    REPO = os.path.abspath(sys.argv[2])
    WORK = tempfile.mkdtemp(prefix='ote-lback-', dir=os.environ.get('TMPDIR'))
    try:
        games = os.path.join(WORK, 'games')
        os.makedirs(games)
        for g in ('demo-srpg', 'demo-economy', 'demo-hex'):
            os.symlink(os.path.join(REPO, 'games', g), os.path.join(games, g))
        shot = lambda n: os.path.join(WORK, n + '.bmp')

        # Reference: the launcher with a Down press (second game selected) and nothing else.
        code, log, n = run('nothing', 'wait:5,down,' + TAIL, shot('ref'), games)
        check('reference run runs to the frame cap', code == 0 and n is None, log)

        # Nothing: Cancel changes nothing, the player runs on.
        code, log, n = run('nothing', 'wait:5,down,cancel,' + TAIL, shot('nothing'), games)
        check('nothing: the player keeps running after Cancel', code == 0 and n is None, log)
        check('nothing: the launcher looks as without Cancel', same(shot('ref'), shot('nothing')))

        # Exit: Cancel quits at once.
        code, log, n = run('exit', 'wait:5,cancel,' + TAIL, None, games)
        check('exit: Cancel quits (exit code 0, before the cap)', code == 0 and n is not None and n < 40, log)

        # Confirm: the question appears, nothing else happened yet.
        code, log, n = run('confirm', 'wait:5,down,cancel,' + TAIL, shot('ask'), games)
        check('confirm: Cancel opens the prompt and the player stays', code == 0 and n is None, log)
        check('confirm: the prompt is on screen', not same(shot('ref'), shot('ask')))

        # Cancel on the prompt closes it: same picture as before.
        code, log, n = run('confirm', 'wait:5,down,cancel,wait:3,cancel,' + TAIL, shot('closed'), games)
        check('confirm: Cancel again closes the prompt and stays', code == 0 and n is None and same(shot('ref'), shot('closed')), log)

        # No is selected from the start: Confirm answers No and the launcher still works (Down moved it above, the
        # prompt did not eat the selection; a further Down then moves again, so compare with two Downs).
        code, log, n = run('nothing', 'wait:5,down,down,' + TAIL, shot('ref2'), games)
        code, log, n = run('confirm', 'wait:5,down,cancel,wait:3,confirm,wait:3,down,' + TAIL, shot('declined'), games)
        check('confirm: Confirm on the default (No) stays', code == 0 and n is None, log)
        check('confirm: the launcher still works after declining', same(shot('ref2'), shot('declined')))

        # Yes: move to it (the prompt lists No first, Yes second), Confirm quits.
        code, log, n = run('confirm', 'wait:5,cancel,wait:3,right,wait:3,confirm,' + TAIL, None, games)
        check('confirm: Yes quits with exit code 0 before the cap', code == 0 and n is not None and n < 40, log)

        # Up/Down are not eaten: the prompt moves between its buttons, and a Down never reaches the launcher list.
        code, log, n = run('confirm', 'wait:5,cancel,wait:3,down,wait:3,down,wait:3,confirm,wait:3,down,' + TAIL, shot('wrap'), games)
        check('confirm: the buttons wrap (Down twice = No again, stays)', code == 0 and n is None and same(shot('ref'), shot('wrap')), log)

        # Without the override Linux does nothing on Cancel (the platform default).
        code, log, n = run(None, 'wait:5,cancel,' + TAIL, None, games)
        check('default on this platform: Cancel does nothing', code == 0 and n is None, log)
    finally:
        shutil.rmtree(WORK, ignore_errors=True)
    print('%d passed, %d failed' % (passed, len(failures)))
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
