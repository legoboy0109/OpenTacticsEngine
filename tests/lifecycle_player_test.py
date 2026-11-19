#!/usr/bin/env python3
"""App life cycle through the real player, windowless (docs/PORTING.md "Life cycle").

  python3 tests/lifecycle_player_test.py ./build/engine/player/ote-player .

The --input tokens background / foreground / lowmemory / terminate inject what the OS sends a phone or console app.
Checked: going to the background writes the in-battle Continue save with the very latest position (a unit that has only
moved, which no normal save moment includes) and not before; settings are written; with --no-saving / --no-settings
nothing is written and no file or folder appears; the simulation clock stands still between background and foreground
(an --autoplay battle does not advance) and goes on afterwards; the bad token list is rejected.
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile

failures = []
passed = 0
PLAYER = ''
REPO = ''
WORK = ''


def check(name, cond, detail=''):
    global passed
    if cond:
        passed += 1
        print('PASS ' + name)
    else:
        failures.append(name)
        print('FAIL %s %s' % (name, detail))


def play(args, inp, frames=1500):
    cmd = [PLAYER, '--games', os.path.join(REPO, 'games'), '--assets', os.path.join(REPO, 'assets'), '--headless',
           '--no-audio', '--frames', str(frames), '--input', inp] + args
    p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True, timeout=300)
    return p.returncode, p.stdout


def battle(saves_args, inp):
    return play(['--no-settings', '--game', 'demo-economy', '--map', 'misty_ford'] + saves_args, inp)


def moved_units(path):
    """How many units of the Continue file are marked as having moved."""
    save = json.load(open(path))

    def walk(o):
        if isinstance(o, dict):
            n = 1 if o.get('moved') is True else 0
            return n + sum(walk(v) for v in o.values())
        if isinstance(o, list):
            return sum(walk(v) for v in o)
        return 0
    return walk(save)


def main():
    global PLAYER, REPO, WORK
    PLAYER = os.path.abspath(sys.argv[1])
    REPO = os.path.abspath(sys.argv[2])
    WORK = tempfile.mkdtemp(prefix='ote-life-', dir=os.environ.get('TMPDIR'))
    try:
        # The first unit in the cycle (an APC) steps up; a move alone is half an order and is not saved by itself.
        move = 'wait:30,next,confirm,up,confirm,wait:20'

        s1 = os.path.join(WORK, 's1')
        code, log = battle(['--saves', s1], move)
        f1 = os.path.join(s1, 'demo-economy', 'continue.json')
        check('no suspend: a moved-only unit is not in the Continue save', os.path.exists(f1) and moved_units(f1) == 0, log)

        s2 = os.path.join(WORK, 's2')
        code, log = battle(['--saves', s2], move + ',background')
        f2 = os.path.join(s2, 'demo-economy', 'continue.json')
        check('background logged', 'lifecycle: background' in log, log)
        check('background writes the Continue save', 'lifecycle: Continue save written' in log, log)
        check('...with the latest position (the moved unit)', os.path.exists(f2) and moved_units(f2) == 1, log)

        s3 = os.path.join(WORK, 's3')
        code, log = battle(['--saves', s3], move + ',terminate')
        f3 = os.path.join(s3, 'demo-economy', 'continue.json')
        check('terminating writes it too', 'lifecycle: terminating' in log and os.path.exists(f3) and moved_units(f3) == 1, log)

        # --no-saving: nothing written, nothing created.
        s4 = os.path.join(WORK, 's4')
        code, log = battle(['--no-saving'], move + ',background,wait:3,foreground')
        check('--no-saving: background runs but writes no Continue save', 'lifecycle: background' in log and 'Continue save written' not in log, log)
        code, log = battle(['--no-saving', '--saves', s4], move + ',background')
        check('--no-saving beats --saves: no folder appears', 'Continue save written' not in log and not os.path.exists(s4), log)

        # Settings: change an option on the global options screen (no scene change, so nothing flushed it yet), then background.
        sets = os.path.join(WORK, 'settings.json')
        opt = 'wait:5,down,down,down,confirm,wait:3,right,wait:3,background,wait:5'
        code, log = play(['--settings', sets, '--no-saving'], opt)
        check('background writes changed settings', 'lifecycle: settings written' in log and os.path.exists(sets), log)
        sets2 = os.path.join(WORK, 'settings2.json')
        code, log = play(['--no-settings', '--no-saving'], opt)
        check('--no-settings: no settings are written', 'settings written' not in log, log)
        code, log = play(['--settings', sets2, '--no-saving'], 'wait:5,background,wait:5')
        check('nothing changed: nothing to write', 'settings written' not in log and not os.path.exists(sets2), log)

        # Low memory is accepted and harmless.
        code, log = play(['--no-settings', '--no-saving'], 'wait:3,lowmemory,wait:3')
        check('low memory is logged and the run goes on', 'lifecycle: low memory' in log and code == 0, log)

        # The clock stands still in the background: a computer-only battle makes no turns until foreground.
        def turns(inp):
            code, log = play(['--no-settings', '--no-saving', '--game', 'demo-economy', '--map', 'misty_ford', '--autoplay'], inp, frames=900)
            return log.count('] ai: '), log
        free, _ = turns('wait:10')
        held, log = turns('wait:2,background,wait:200')
        back, _ = turns('wait:2,background,wait:100,foreground,wait:100')
        check('the battle runs on its own without a suspend (%d turns)' % free, free >= 3, str(free))
        check('suspended: no turn is played (%d turns)' % held, held <= 1, log)
        check('foreground: it goes on (%d turns)' % back, back > held, str(back))

        code, log = play(['--no-settings', '--no-saving'], 'wait:3,sleep')
        check('an unknown token is still an error', code != 0 and 'bad --input token' in log, log)
    finally:
        shutil.rmtree(WORK, ignore_errors=True)
    print('%d passed, %d failed' % (passed, len(failures)))
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
