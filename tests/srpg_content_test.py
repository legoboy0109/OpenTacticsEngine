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
           ('tests/fixtures/srpg-supports', 'ch1'), ('tests/fixtures/srpg-supports', 'ch2'),
           ('tests/fixtures/srpg-staves', 'skirmish'), ('tests/fixtures/srpg-staves', 'clinic'),
           ('tests/fixtures/srpg-staves2', 'skirmish'), ('tests/fixtures/srpg-staves2', 'yard'), ('tests/fixtures/srpg-staves2', 'gate'),
           ('tests/fixtures/srpg-conditions', 'skirmish'), ('tests/fixtures/srpg-conditions', 'field')]
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

# Staves (docs/SRPG.md section 8): the healers of the staves fixture heal, the same seed plays the same twice, and the event
# printer names a use.
staves = os.path.join(root, 'tests/fixtures/srpg-staves')
a = run(staves, 'skirmish', '--seed', '3', '--verbose')[1]
b = run(staves, 'skirmish', '--seed', '3', '--verbose')[1]
check('staves skirmish: same seed twice, same text', a == b and len(a) > 500)
text = ''.join(run(staves, 'skirmish', '--seed', str(seed), '--verbose')[1] for seed in range(1, 11))
check('the event printer names a staff use', ' uses mend_staff on ' in text and ' heals ' in text)

# Staves, stage two (docs/SRPG.md section 8): the computer holds warp, pull, swap, repair and light staves it does not use (it ignores
# every staff that does more than heal) and never stalls on them; the fog map (mist) is not in the stall list above because the
# computer's hunt for units it cannot see stalls there with or without staves (same numbers with the staves taken away).
staves2 = os.path.join(root, 'tests/fixtures/srpg-staves2')
a = run(staves2, 'skirmish', '--seed', '3', '--verbose')[1]
b = run(staves2, 'skirmish', '--seed', '3', '--verbose')[1]
check('staves2 skirmish: same seed twice, same text', a == b and len(a) > 500)
text = ''.join(run(staves2, 'skirmish', '--seed', str(seed), '--verbose')[1] for seed in range(1, 11))
check('staves2: the computer heals with its plain heal staff and never uses the others',
      ' uses mend_staff on ' in text and not any(' uses %s ' % s in text for s in ('warp_staff', 'recall_staff', 'swap_staff', 'shove_staff', 'hop_staff',
                                                                                    'fix_staff', 'patch_staff', 'kindly_pull_staff')))

# Status conditions (docs/SRPG.md section 9): two armies with sleep, poison and cure staves, venom blades and tonics play without a stall,
# the same seed plays the same twice, and the event printer names every kind of condition event. (The mist map is left out of the stall
# list for the reason given above.)
conditions = os.path.join(root, 'tests/fixtures/srpg-conditions')
a = run(conditions, 'skirmish', '--seed', '3', '--verbose')[1]
b = run(conditions, 'skirmish', '--seed', '3', '--verbose')[1]
check('conditions skirmish: same seed twice, same text', a == b and len(a) > 500)
text = ''.join(run(conditions, 'skirmish', '--seed', str(seed), '--verbose')[1] for seed in range(1, 21))
for what in (' uses sleep_staff on ', ' gets Sleep for ', ' cannot act (Sleep)', " was woken by a strike", " ran out", ' gets Poison for ',
             ' is immune to Poison', ' uses mend_cure_staff on ', " was cured"):
    check('the computer and the event printer: ' + what, what in text)

sys.exit(1 if failures else 0)
