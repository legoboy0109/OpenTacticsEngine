#!/usr/bin/env python3
"""Content roots through the real player, windowless (docs/PORTING.md "Roots").

  python3 tests/roots_player_test.py ./build/engine/player/ote-player .

The player must not depend on its working directory: it is started from an EMPTY temp folder and has to find games/
and assets/ through --content-root (or OTE_CONTENT_ROOT), --games / --assets still beat the content root, a second
(user) games folder is merged into the list with its game winning on the same id, and nothing is ever written to the
content root. Without any root the old behaviour stays: relative "games" in the working directory (so an empty folder
finds nothing).
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile

failures = []
passed = 0


def check(name, cond, detail=''):
    global passed
    if cond:
        passed += 1
        print('PASS ' + name)
    else:
        failures.append(name)
        print('FAIL %s %s' % (name, detail))


def main():
    player = os.path.abspath(sys.argv[1])
    repo = os.path.abspath(sys.argv[2])
    work = tempfile.mkdtemp(prefix='ote-roots-', dir=os.environ.get('TMPDIR'))
    try:
        cwd = os.path.join(work, 'elsewhere')  # a working directory with no games/ in it
        os.mkdir(cwd)

        def play(args, env=None, game='demo-economy', frames=20):
            cmd = [player, '--game', game, '--headless', '--no-audio', '--no-settings', '--no-saving',
                   '--frames', str(frames)] + args
            e = dict(os.environ)
            e.pop('OTE_CONTENT_ROOT', None)
            e.pop('OTE_EXTRA_GAMES', None)
            e.update(env or {})
            p = subprocess.run(cmd, cwd=cwd, env=e, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               universal_newlines=True, timeout=120)
            return p.returncode, p.stdout

        # No root anywhere: the working directory is the root, as it always was.
        rc, log = play([])
        check('no root: nothing found in an empty folder', "Cannot load game 'demo-economy'" in log, log)

        rc, log = play(['--content-root', repo, '--font', 'arcade'])
        check('--content-root finds games/ from another working directory', "loaded game 'demo-economy'" in log, log)
        check('--content-root finds assets/ (a built-in font by id)', 'PressStart2P-Regular.ttf' in log, log)

        rc, log = play([], env={'OTE_CONTENT_ROOT': repo})
        check('OTE_CONTENT_ROOT works too', "loaded game 'demo-economy'" in log, log)

        rc, log = play(['--content-root', os.path.join(work, 'nonexistent'), '--games', os.path.join(repo, 'games')])
        check('--games beats --content-root', "loaded game 'demo-economy'" in log, log)

        # A user folder of games: one game with the id of a built-in one (wins), one new game.
        extra = os.path.join(work, 'user-games')
        os.mkdir(extra)
        for gid, name in (('demo-economy', 'User Copy Of Economy'), ('mine', 'My Own Game')):
            dst = os.path.join(extra, gid)
            shutil.copytree(os.path.join(repo, 'games', 'demo-economy'), dst)
            path = os.path.join(dst, 'game.json')
            text = open(path).read()
            text = re.sub(r'"name"\s*:\s*"[^"]*"', '"name": "%s"' % name, text, count=1)
            open(path, 'w').write(text)

        rc, log = play(['--content-root', repo, '--extra-games', extra])
        check('same id: the user game wins', "loaded game 'demo-economy' (User Copy Of Economy)" in log, log)
        rc, log = play(['--content-root', repo, '--extra-games', extra], game='mine')
        check('a game only in the user folder loads', "loaded game 'mine' (My Own Game)" in log, log)
        rc, log = play([], env={'OTE_CONTENT_ROOT': repo, 'OTE_EXTRA_GAMES': extra}, game='mine')
        check('OTE_EXTRA_GAMES works too', "loaded game 'mine' (My Own Game)" in log, log)
        rc, log = play(['--content-root', repo], game='mine')
        check('without the user folder that game does not exist', "Cannot load game 'mine'" in log, log)

        # Nothing is written into the working directory or the content root by a no-settings, no-saving run.
        check('working directory stays empty', os.listdir(cwd) == [], str(os.listdir(cwd)))
    finally:
        shutil.rmtree(work, ignore_errors=True)
    print('%d passed, %d failed' % (passed, len(failures)))
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
