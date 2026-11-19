#!/usr/bin/env python3
"""ote-sim and the scripted-style computer players (docs/AI.md), windowless.

  python3 tests/ai_scripted_sim_test.py <repo> ./build/ote-sim

Runs the fixture game tests/fixtures/ai-scripted (both sides played by the computer, every behaviour key in use: acting order, weight sets,
caution none, retreat, idle turns, patrol, ignores, guardArea by movement cost) with ote-sim and judges:

  - two runs with the same seeds print the very same results (apart from the milliseconds);
  - every battle ends (no stall) and the run says no errors;
  - the same map without any behaviour keys and without rules.ai plays out differently (the keys do something);
  - different seeds do not all give the same result (the hit chance is 85 %, so the sim does roll).
"""
import json
import os
import re
import shutil
import sys
import tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import run_text, check, failures, summary  # noqa: E402

sys.dont_write_bytecode = True

REPO = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else '.'
SIM = os.path.abspath(sys.argv[2]) if len(sys.argv) > 2 else ''
FIXTURE = os.path.join(REPO, 'tests', 'fixtures', 'ai-scripted')
TMP = tempfile.mkdtemp(prefix='ote-aiscripted-')


def sim(game, *args):
    p = run_text([SIM, game, 'field'] + list(args))
    return p.returncode, re.sub(r'avg [0-9.]+ ms/run', '', p.stdout)


if not os.path.exists(SIM):
    print('FAIL ote-sim %s is not built' % SIM)
    sys.exit(1)

code1, out1 = sim(FIXTURE, '--runs', '20', '--seed', '3')
code2, out2 = sim(FIXTURE, '--runs', '20', '--seed', '3')
check('the fixture game plays', code1 == 0 and 'summary: 20 runs' in out1, out1[-300:])
check('two runs with the same seeds are identical', out1 == out2)
check('no battle stalls', 'stalls 0' in out1, out1[-200:])
check('the run says no errors', 'error' not in out1.lower() and 'warn' not in out1.lower(), out1[-300:])
winners = set(re.findall(r'winner (\w+)', out1))
rounds = set(re.findall(r'rounds (\d+)', out1))
check('different seeds give different battles', len(rounds) > 1 or len(winners) > 1, out1[:400])

# The same game with every key taken out.
plain = os.path.join(TMP, 'plain')
shutil.copytree(FIXTURE, plain)
with open(os.path.join(plain, 'game.json')) as f:
    game = json.load(f)
del game['rules']['ai']
with open(os.path.join(plain, 'game.json'), 'w') as f:
    json.dump(game, f)
with open(os.path.join(plain, 'maps', 'field.json')) as f:
    m = json.load(f)
for u in m['units']:
    u.pop('behaviour', None)
with open(os.path.join(plain, 'maps', 'field.json'), 'w') as f:
    json.dump(m, f)
code3, out3 = sim(plain, '--runs', '20', '--seed', '3')
check('the same game without the keys plays', code3 == 0 and 'summary: 20 runs' in out3, out3[-300:])
check('the keys change how the battles go', out3 != out1)

shutil.rmtree(TMP, ignore_errors=True)
summary()
sys.exit(1 if failures else 0)
