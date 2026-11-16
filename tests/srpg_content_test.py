#!/usr/bin/env python3
"""Step D1 of docs/SRPG.md: the sims of the strategy-RPG content.

usage: python3 tests/srpg_content_test.py REPO_ROOT [OTE_SIM]

demo-srpg (both maps and its campaign), the strategy-rpg template and the new fixtures play AI against AI on 20 seeds
without a stall or an error line; the same seed printed twice gives the same text (determinism); and the event printer
names every strategy-RPG entry the demo produces (promotion, skills, summons, supports).
"""
import os
import subprocess
import sys

root = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.getcwd()
sim = os.path.abspath(sys.argv[2]) if len(sys.argv) > 2 else os.path.join(root, 'build', 'ote-sim')
failures = []


def check(name, ok, detail=''):
    print(('PASS ' if ok else 'FAIL ') + name + ('' if ok else ' ' + detail))
    if not ok:
        failures.append(name)


def run(*args):
    p = subprocess.run([sim] + list(args), stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True, timeout=300)
    return p.returncode, p.stdout


BATTLES = [('games/demo-srpg', 'bridge'), ('games/demo-srpg', 'north_road'), ('editor/templates/strategy-rpg', 'ford'),
           ('tests/fixtures/srpg-carry', 'field'), ('tests/fixtures/srpg-pair', 'field'), ('tests/fixtures/srpg-hex', 'field'),
           ('tests/fixtures/srpg-supports', 'ch1'), ('tests/fixtures/srpg-supports', 'ch2')]
for game, map_id in BATTLES:
    code, out = run(os.path.join(root, game), map_id, '--runs', '20')
    last = out.strip().split('\n')[-1]
    check('%s %s: 20 seeds finish' % (game, map_id), code == 0 and 'summary: 20 runs' in last and 'stalls 0' in last, last)
    check('%s %s: no error lines' % (game, map_id), 'ERROR' not in out and 'error' not in out.lower().replace('stalls', ''), out[-300:])

for game in ('games/demo-srpg', 'tests/fixtures/srpg-supports'):
    code, out = run(os.path.join(root, game), '--campaign', '--runs', '20')
    last = out.strip().split('\n')[-1]
    check('%s --campaign: 20 runs and none stalls' % game, code in (0, 2) and 'summary: 20 runs' in last and 'stalled' not in out, last)

demo = os.path.join(root, 'games/demo-srpg')
a = run(demo, 'north_road', '--seed', '5', '--verbose')[1]
b = run(demo, 'north_road', '--seed', '5', '--verbose')[1]
check('same seed twice: same text', a == b and len(a) > 500)
text = ''
for seed in range(1, 21):
    text += run(demo, 'north_road', '--seed', str(seed), '--verbose')[1]
    text += run(demo, 'bridge', '--seed', str(seed), '--verbose')[1]
for what in ('is promoted from lord to captain', 'uses call_hound', 'appears at', 'second_charge activates', 'learns ',
             'support eirik and kell', 'a conversation is ready'):
    check('the event printer names: ' + what, what in text)

sys.exit(1 if failures else 0)
