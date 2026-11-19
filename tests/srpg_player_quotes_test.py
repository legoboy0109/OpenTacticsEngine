#!/usr/bin/env python3
"""Battle and defeat quotes through the real player, windowless (docs/EVENTS.md "Battle and defeat quotes").

  python3 tests/srpg_player_quotes_test.py ./build/engine/player/ote-player .
  python3 tests/srpg_player_quotes_test.py ./build/engine/player/ote-player . --shots OUT_DIR   # keep screenshots (BMP)

Copies tests/fixtures/srpg-quotes into a temp folder, runs the player headless (no audio, no window, no settings, saves in the
temp folder) with --input scripts and reads the player's "[info] dialogue: <speaker> | <text>" lines (one per line that reaches
the dialogue box, " (skipped)" when a skip passed it) and the Continue save:

  - the player's attack: the attacked unit's quote, then the attacker's, both after the forecast and before the strikes;
  - the computer's attack on its own turn (the blue unit attacked speaks first, once-per-map flags used up);
  - skipping a run of lines; the flags saved in the Continue save ("quotes": { "said": [...] });
  - a defeat quote after the fall (and a friend's line), the boss's before "battle over";
  - the same defeat in a game whose campaign brings fallen units back ("retreats");
  - --autoplay: all-computer play shows the lines and never waits.
"""
import json
import os
import re
import shutil
import sys
import tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import (  # noqa: E402
    run_text, check, edit, failures, player_and_repo, player_cmd, shots_dir, summary, ticks, write_json)

sys.dont_write_bytecode = True

PLAYER, REPO = player_and_repo()
SHOTS = shots_dir()
TMP = tempfile.mkdtemp(prefix='ote-quotes-')

IDLE = 'wait:14'  # the turn banner plays first; it takes the first inputs otherwise
# Select the first unit (Eirik), stay where he is, choose Attack, pick the only target (the forecast shows while picking).
ATTACK = [IDLE, 'confirm', 'wait:3', 'confirm', 'wait:3', 'confirm', 'wait:3', 'confirm', 'wait:30']
NEXT = ['confirm'] * 3  # one line: show it whole, end it (a third press for a line that needs a page more)


def read_json(path):
    with open(path) as f:  # the fixture's files may start with // comment lines
        return json.loads(''.join(l for l in f if not l.lstrip().startswith('//')))


def make_game(gid, tweak=None):
    g = os.path.join(TMP, gid)
    shutil.copytree(os.path.join(REPO, 'tests/fixtures/srpg-quotes'), g)
    if tweak:
        tweak(g)
    return g


def run(gid, inp, extra=(), shot=None, interval=4, frames=None):
    """Runs the player on a fresh battle of ch1; returns (log, the Continue save's battle)."""
    work = os.path.join(TMP, 'saves_' + gid)
    shutil.rmtree(work, ignore_errors=True)
    cmd = player_cmd(PLAYER, TMP, REPO) + ['--saves', work, '--frames', str(frames or ticks(inp) * interval + 90), '--input-interval', str(interval),
           '--fixed-dt', '0.016', '--game', gid, '--map', 'ch1']
    if inp:
        cmd += ['--input', inp]
    if shot:
        cmd += ['--screenshot', shot]
    p = run_text(cmd + list(extra))
    battle = {}
    try:
        battle = read_json(os.path.join(work, gid, 'continue.json'))['battle']
    except (OSError, ValueError, KeyError):
        pass
    return p.stdout, battle


def said(log):
    """The dialogue lines that reached the box, in order, as 'Speaker | text' (a skipped one has ' (skipped)')."""
    return [m.group(1) for m in re.finditer(r'\[info\] dialogue: (.*)', log)]


def keep(shot_path, name):
    if SHOTS and os.path.exists(shot_path):
        os.makedirs(SHOTS, exist_ok=True)
        shutil.copy(shot_path, os.path.join(SHOTS, name + '.bmp'))


def test_player_attack():
    make_game('q_attack')
    inp = ','.join(ATTACK + NEXT * 3 + ['wait:30'])
    log, battle = run('q_attack', inp)
    got = said(log)
    check('attack: the attacked unit speaks first, then the attacker', got[:2] == ['Gorm | Eirik. At last.', 'Eirik | Gorm! Your raids end today.'], str(got) + log[-500:])
    fc = log.find('forecast:')
    first = log.find('dialogue:')
    check('attack: the lines come after the forecast', 0 <= fc < first, log[-500:])


def test_enemy_turn_and_skip():
    make_game('q_enemy', lambda g: os.remove(g + '/data/campaign.json'))  # no campaign: a single battle writes its Continue save
    # Blue's fight, then End Turn (battle menu: first entry); the computer's Gorm attacks Eirik.
    inp = ','.join(ATTACK + NEXT * 3 + ['wait:10', 'menu', 'confirm', 'wait:60'] + ['confirm'] * 8 + ['wait:90'])
    log, battle = run('q_enemy', inp)
    got = said(log)
    # Eirik's two once-per-map quotes (against Gorm, general) are used up; Gorm's "always" ones are never kept.
    check('enemy turn: the flags are in the Continue save', battle.get('quotes') == {'said': [0, 1]}, str(battle.get('quotes')) + log[-300:])
    check('enemy turn: the unit attacked speaks first (its general quote, the pair one is used up), then Gorm',
          got[2:4] == ['Eirik | For the valley!', 'Gorm | Eirik. At last.'], str(got))
    # Skipping: the first line shows, the rest of the run passes at once.
    make_game('q_skip')
    log, _ = run('q_skip', ','.join(ATTACK + ['skip', 'wait:30']))
    got = said(log)
    check('skip: the run of lines is skipped, nothing is lost', len(got) == 2 and got[1].endswith('(skipped)'), str(got))


def small_hp(g):
    # Gorm and Bram share the brute type: two hit points, so Eirik's first hit is the end of Gorm.
    edit(g, 'data/units.json', lambda u: [x.update({'stats': dict(x['stats'], hp=2)}) for x in u['units'] if x['id'] == 'brute'])


def test_defeat_and_boss():
    make_game('q_defeat', small_hp)
    inp = ','.join(ATTACK + NEXT * 4 + ['wait:30'])
    log, battle = run('q_defeat', inp)
    got = said(log)
    want = ['Gorm | Eirik. At last.', 'Eirik | Gorm! Your raids end today.', 'Gorm | Impossible...', 'Mira | Brother!']
    check('defeat: last words after the fall, a friend answers', got[:4] == want, str(got) + log[-400:])
    check('defeat: the battle goes on (Gorm was not the last red unit)', 'battle over' not in log, log[-300:])

    def alone(g):
        small_hp(g)
        edit(g, 'maps/ch1.json', lambda m: m.update({'units': [u for u in m['units'] if u.get('character') != 'tolly' and u.get('type') != 'grunt']}))
    make_game('q_boss', alone)
    log, battle = run('q_boss', ','.join(ATTACK + NEXT * 4 + ['wait:60']))
    got = said(log)
    over = log.find('battle over')
    last = log.rfind('dialogue: Mira | Brother!')
    check('boss: the defeat quote plays before the victory', got[2:4] == ['Gorm | Impossible...', 'Mira | Brother!'] and 0 <= last < over, str(got) + log[-400:])


def test_retreat_rule():
    # Eirik (one of ours) has one hit point: Gorm's counter strikes him down in a game that brings fallen units back.
    def eirik_weak(g):
        edit(g, 'data/units.json', lambda u: [x.update({'stats': dict(x['stats'], hp=1)}) for x in u['units'] if x['id'] == 'hero'])
        edit(g, 'data/campaign.json', lambda c: c['rules'].update({'fallen': 'return'}))
    make_game('q_return2', eirik_weak)
    log, _ = run('q_return2', ','.join(ATTACK + NEXT * 4 + ['wait:30']))
    got = said(log)
    check('retreat: a unit of ours that falls in a game with returning units says its "retreats" line', 'Eirik | I must pull back!' in got, str(got))
    make_game('q_gone', lambda g: edit(g, 'data/units.json', lambda u: [x.update({'stats': dict(x['stats'], hp=1)}) for x in u['units'] if x['id'] == 'hero']))
    log, _ = run('q_gone', ','.join(ATTACK + NEXT * 4 + ['wait:30']))
    got = said(log)
    check('falls: the same fight when fallen units are gone says its "falls" line', 'Eirik | Tell them I tried.' in got, str(got))


def test_autoplay():
    make_game('q_auto')
    log, battle = run('q_auto', '', extra=['--autoplay', '--seed', '3'], frames=2500)
    got = said(log)
    check('autoplay: quotes show and the computer play never waits for them', len(got) >= 2 and 'battle over' in log, log[-500:])


def test_screenshot():
    make_game('q_shot')
    path = os.path.join(TMP, 'quote_box.bmp')
    log, _ = run('q_shot', ','.join(ATTACK), shot=path)
    keep(path, 'player-battle-quote')
    check('screenshot: the first line is up when the picture is taken', os.path.exists(path) and 'dialogue: Gorm' in log, log[-300:])


try:
    test_player_attack()
    test_enemy_turn_and_skip()
    test_defeat_and_boss()
    test_retreat_rule()
    test_autoplay()
    test_screenshot()
finally:
    shutil.rmtree(TMP, ignore_errors=True)

summary()
sys.exit(1 if failures else 0)
