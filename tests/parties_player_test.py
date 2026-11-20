#!/usr/bin/env python3
"""Campaign parties through the real player, windowless (docs/EVENTS.md "Parties", docs/SAVES.md).

  python3 tests/parties_player_test.py ./build/engine/player/ote-player .

Plays tests/fixtures/parties-flow (copied to a temp folder, never changed): four short chapters doing the owner's flow.
Parts 1 and 2 with the `main` party (mara, tobin and the recruit wren), part 3 switching to `north` (a whole new game:
kara, ivo and the recruit fenn, its own storage and gold), part 4 merging `north` into `main`. Every step is a separate
player run that CONTINUES the Continue save of the one before, so each chapter's end is checked in the saved file, and
the run in the middle of chapter 3's battle picks the north party's battle up again. The player logs
"campaign: chapter N is played by party ..." when a chapter's battle starts. Pictures are checked by hand.
"""
import json
import os
import re
import shutil
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import run_text, check, failures, summary  # noqa: E402
from ote_testkit import scratch_dir  # noqa: E402

PLAYER = ''
repo = '.'
GAMES = ''
GAME = 'parties-flow'

# Input recipes (one token every INTERVAL frames): the title card is left with confirm and the scene skipped, the battle's
# only turn is ended with menu + confirm (round 2's event wins it), the result banner is left with confirm.
INTERVAL = 8
CARD_AND_SKIP = 'confirm,skip,wait:20'
END_TURN = 'menu,confirm,wait:60'
WON = 'confirm,wait:10'


def play(saves, args, inp, frames=1500):
    cmd = [PLAYER, '--games', GAMES, '--game', GAME, '--headless', '--no-audio', '--no-settings', '--frames', str(frames),
           '--input-interval', str(INTERVAL), '--saves', saves, '--input', inp]
    p = run_text(cmd + args)
    return p.returncode, p.stdout


def save_of(saves):
    path = os.path.join(saves, GAME, 'continue.json')
    return json.load(open(path)) if os.path.exists(path) else None


def ids(roster):
    return sorted(r['id'] for r in roster)


def gold(sides):
    for s in sides:
        if s['id'] == 'blue':
            return s['resources'][0]
    return None


def potions(sides):
    for s in sides:
        if s['id'] == 'blue':
            return len(s.get('storage', []))
    return 0


def party_lines(log):
    return re.findall(r"campaign: chapter (\d) is played by party '(\w+)' \(([^)]*)\)", log)


def scenarios(tmp):
    sv = os.path.join(tmp, 'saves')

    # 1. Chapter 1, the main party: the recruit joins it, the after scene's gold goes to it. No party keys but the active one.
    code, log = play(sv, ['--campaign'], CARD_AND_SKIP + ',' + END_TURN + ',' + WON)
    s = save_of(sv)
    p = s['playthrough'] if s else {}
    check('chapter 1 is played by main', party_lines(log) == [('1', 'main', 'The Company')], log[-300:])
    check('chapter 1 won: the save is chapter 2, before, party main',
          s and s['chapter'] == 'ch2' and p['phase'] == 'before' and p['party'] == 'main' and 'parties' not in p, json.dumps(s)[:300] if s else 'no save')
    check('main carries mara, tobin and the recruit wren', ids(p.get('roster', [])) == ['mara', 'tobin', 'wren'], str(p.get('roster'))[:200])
    check('main has 60 gold (map 50 + the scene 10) and its potion', gold(p.get('sides', [])) == 60 and potions(p.get('sides', [])) == 1)

    # 2. Continue: chapter 2, still main (25 gold from its map).
    code, log = play(sv, ['--continue'], CARD_AND_SKIP + ',' + END_TURN + ',' + WON)
    s = save_of(sv)
    p = s['playthrough'] if s else {}
    check('chapter 2 is played by main', party_lines(log) == [('2', 'main', 'The Company')], log[-300:])
    check('chapter 2 won: the save is chapter 3, before, main has 85 gold',
          s and s['chapter'] == 'ch3' and p['phase'] == 'before' and p['party'] == 'main' and gold(p['sides']) == 85, json.dumps(s)[:300] if s else 'no save')

    # 3. Continue: chapter 3's before scene switches to north; its battle starts (a save is written at the turn start).
    code, log = play(sv, ['--continue'], CARD_AND_SKIP + ',wait:40')
    s = save_of(sv)
    p = s['playthrough'] if s else {}
    check('chapter 3 is played by north', party_lines(log) == [('3', 'north', 'The Northern Road')], log[-300:])
    check('the switch: north is active and starts empty, main is parked untouched',
          p and p['party'] == 'north' and p['roster'] == [] and p['sides'] == [] and [x['id'] for x in p.get('parties', [])] == ['main']
          and ids(p['parties'][0]['roster']) == ['mara', 'tobin', 'wren'] and gold(p['parties'][0]['sides']) == 85, json.dumps(p)[:400])
    check('the save is in the middle of the north party\'s battle', s and 'battle' in s and s['chapter'] == 'ch3')
    units = sorted(u.get('id', '') for u in s['battle']['units']) if s and 'battle' in s else []
    check('the north battle has the forest\'s own characters (and not main\'s)', 'kara' in units and 'ivo' in units and 'mara' not in units, str(units))

    # 4. Continue mid-battle: the north party's battle resumes and is won; its recruit, gold and potion go to north only.
    code, log = play(sv, ['--continue'], 'wait:20,' + END_TURN + ',' + WON)
    s = save_of(sv)
    p = s['playthrough'] if s else {}
    check('the north battle was picked up and won', 'chapter 3 battle over (won)' in log, log[-300:])
    check('north now has kara, ivo and the recruit fenn, 35 gold (30 + the scene\'s 5) and its own potion',
          p and p['party'] == 'north' and ids(p['roster']) == ['fenn', 'ivo', 'kara'] and gold(p['sides']) == 35 and potions(p['sides']) == 1, json.dumps(p)[:400])
    check('main is still as it was (85 gold, three characters, one potion)',
          p and ids(p['parties'][0]['roster']) == ['mara', 'tobin', 'wren'] and gold(p['parties'][0]['sides']) == 85 and potions(p['parties'][0]['sides']) == 1)
    check('chapter 4 is next', s and s['chapter'] == 'ch4' and p['phase'] == 'before')

    # 5. Continue: chapter 4's before scene merges north into main.
    code, log = play(sv, ['--continue'], CARD_AND_SKIP + ',wait:40')
    s = save_of(sv)
    p = s['playthrough'] if s else {}
    check('chapter 4 is played by main', party_lines(log) == [('4', 'main', 'The Company')], log[-300:])
    check('the merge: one party left, north is gone', p and p['party'] == 'main' and 'parties' not in p and p.get('mergedParties') == ['north'], json.dumps(p)[:300])
    check('merged: six characters, 120 gold, two potions',
          p and ids(p['roster']) == ['fenn', 'ivo', 'kara', 'mara', 'tobin', 'wren'] and gold(p['sides']) == 120 and potions(p['sides']) == 2, json.dumps(p)[:500])
    check('the chapter 4 battle starts with the merged funds', s and 'battle' in s and s['battle']['sides'][0]['resources'] == [120],
          str(s['battle']['sides'][0]['resources']) if s and 'battle' in s else 'no battle')

    # 6. Continue: the last battle is won, the story ends and the Continue save is deleted.
    code, log = play(sv, ['--continue'], 'wait:20,' + END_TURN + ',' + WON + ',confirm,wait:10')
    check('the story ends after chapter 4', 'chapter 4 battle over (won)' in log and save_of(sv) is None, log[-300:])


def main():
    global PLAYER, repo, GAMES
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    PLAYER, repo = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
    tmp = scratch_dir()
    GAMES = os.path.join(tmp, 'games')
    try:
        os.makedirs(GAMES)
        shutil.copytree(os.path.join(repo, 'tests/fixtures/parties-flow'), os.path.join(GAMES, GAME))
        scenarios(tmp)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    summary(blank_line=True)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
