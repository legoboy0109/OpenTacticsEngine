#!/usr/bin/env python3
"""Test levels and test playthroughs through the real player, windowless (docs/DIFFICULTY.md "Test level", docs/SAVES.md "Test playthroughs").

  python3 tests/testplay_player_test.py ./build/engine/player/ote-player .

Builds the campaign test game of tests/campaign_player_test.py in a temp folder (never in the repo) and changes it into a four-chapter
campaign whose third chapter has strong enemies (levels 8 and 6, so `auto` is 7 + 2 = 9) and whose second chapter has a recruit (Mira):
  --map with --test-level 7 and auto (the log line, the units raised), the option off changes nothing,
  --campaign --chapter ch3 --test-level auto (the before scene of ch3 plays, the roster holds the earlier heroes and the recruit
  at the level, the battle is won, chapter 4 starts; saves only in the temp --saves folder), autoplay from a chapter, the command line.
"""
import json
import os
import re
import shutil
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import run_text, check, failures, save_of, summary  # noqa: E402
from ote_testkit import scratch_dir  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import campaign_player_test as cpt  # noqa: E402  (its game builder: three chapters of the demo's ford map)

PLAYER = ''
repo = '.'
GAMES = ''


def play(args, saves=None, frames=1500, interval=8, inp=None):
    cmd = [PLAYER, '--games', GAMES, '--headless', '--no-audio', '--no-settings', '--frames', str(frames), '--input-interval', str(interval)]
    cmd += ['--saves', saves] if saves else ['--no-saving']
    if inp:
        cmd += ['--input', inp]
    p = run_text(cmd + args)
    return p.returncode, p.stdout


def make_game():
    """'tp' = the campaign test game 'camp' with a fourth chapter, a recruit in chapter 2 and strong enemies in chapter 3."""
    cpt.make_games(repo, GAMES)
    dst = os.path.join(GAMES, 'tp')
    shutil.copytree(os.path.join(GAMES, 'camp'), dst)
    shutil.rmtree(os.path.join(GAMES, 'camp_end'))
    shutil.rmtree(os.path.join(GAMES, 'camp_lose'))
    shutil.rmtree(os.path.join(GAMES, 'camp'))
    json.dump({'games': ['tp']}, open(os.path.join(GAMES, 'index.json'), 'w'))

    def load(name):
        return json.load(open(os.path.join(dst, 'maps', name + '.json')))

    def save(name, m):
        json.dump(m, open(os.path.join(dst, 'maps', name + '.json'), 'w'), indent=1)

    ch2 = load('bridge2')
    ch2['units'].append(cpt.U('archer', 'blue', 1, 4, 'Mira', 'mira', level=1))  # the recruit of chapter 2
    save('bridge2', ch2)
    ch3 = load('keep')  # strong enemies: levels 8 and 6, so `auto` = 7 + 2 = 9
    for u in ch3['units']:
        if u.get('id') == 'warden':
            u['level'] = 8
    for u in ch3['units']:
        if u['name'] == 'Vex':
            u['level'] = 6
    ch3['units'].append(cpt.U('soldier', 'blue', 1, 4, 'Mira', 'mira', level=1))  # Mira joins the keep as well (same id: the roster's copy is used)
    save('keep', ch3)
    # No promotions in this copy: a raised unit that meets a class's promotion level is promoted (or offered it) at its side's first turn,
    # as for any unit placed at that level, and the offer screen would take the scripted input below.
    units_path = os.path.join(dst, 'data', 'units.json')
    units = cpt.jsonc(units_path)
    for t in units['units']:
        t.pop('promotions', None)
    json.dump(units, open(units_path, 'w'), indent=1)
    camp = json.load(open(os.path.join(dst, 'data', 'campaign.json')))
    camp['chapters'][2]['before'] = [{'say': 'The keep looms. (test before scene)', 'speakerName': 'Narrator'}]
    camp['chapters'][2]['after'] = [{'say': 'The keep falls.', 'speakerName': 'Narrator'}]
    camp['chapters'].append({'id': 'ch4', 'name': 'The Road Home', 'map': 'bridge2',
                             'before': [{'say': 'The fourth chapter begins.', 'speakerName': 'Narrator'}],
                             'after': [{'say': 'Home at last.', 'speakerName': 'Narrator'}]})
    json.dump(camp, open(os.path.join(dst, 'data', 'campaign.json'), 'w'), indent=1)


CARD_AND_SKIP = 'confirm,skip,wait:20'
END_TURN = 'menu,confirm,wait:60'


def levels_in(save):
    return {r['id']: r['level'] for r in save['playthrough']['roster']} if save else {}


def run_all():
    tmp = scratch_dir()
    global GAMES
    GAMES = os.path.join(tmp, 'games')
    try:
        make_game()
        scenarios(tmp)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def scenarios(tmp):
    def fresh(name):
        d = os.path.join(tmp, name)
        shutil.rmtree(d, ignore_errors=True)
        return d

    # 1. A free battle with a number: the log line, every human unit raised, the computer's untouched; hit points full.
    code, log = play(['--game', 'tp', '--map', 'keep', '--test-level', '7', '--autoplay'], frames=600)
    check('--map --test-level 7: the log says the level', code == 0 and re.search(r'test level: 7 \(\d+ units? raised\)', log) is not None, log[-400:])
    check('--map --test-level 7: Kell is raised from 1 to 7', re.search(r'test level: Kell Lv 1 -> 7', log) is not None, log[-400:])
    check('--map --test-level 7: the enemy is not touched', 'test level: Warden' not in log)

    # 2. auto = the rounded average level of the hostile computer units (8 and 6 -> 7), plus 2.
    code, log = play(['--game', 'tp', '--map', 'keep', '--test-level', 'auto', '--autoplay'], frames=600)
    check('--map --test-level auto is 9 here', re.search(r'test level: 9 \(auto\) \(\d+ units? raised\)', log) is not None, log[-400:])
    check('--map --test-level auto says how: enemy average 7 + 2', 'test level: auto aims at enemy average 7 + 2 = 9' in log, log[-400:])
    check('--map --test-level auto: Kell is raised from 1 to 9 (no weapon ranks in this game: nothing about ranks)', re.search(r'test level: Kell Lv 1 -> 9\r?\n', log) is not None, log[-400:])

    # 3. Without the option nothing is said and nothing is raised.
    code, log = play(['--game', 'tp', '--map', 'keep', '--autoplay'], frames=600)
    check('no option: no test level line', 'test level' not in log, log[-300:])

    # 4. A test playthrough at chapter 3 with `auto`: its scene, the roster, the won battle, then chapter 4.
    sv = fresh('s_ch3')
    code, log = play(['--game', 'tp', '--campaign', '--chapter', 'ch3', '--test-level', 'auto'], saves=sv,
                     inp=CARD_AND_SKIP + ',' + END_TURN + ',confirm,skip,wait:20,confirm,wait:30', frames=900)
    check('chapter mode: starts at chapter 3', re.search(r"test playthrough starts at chapter 'ch3' with \d+ roster unit", log) is not None, log[-400:])
    check('chapter mode: chapter 3 is the only battle won', len(re.findall(r'chapter \d battle over \(won\)', log)) == 1 and 'chapter 3 battle over (won)' in log, log[-500:])
    check('chapter mode: the Test level row is in the battle menu', 'battle menu: End Turn, Unit, Test level,' in log, log[-500:])
    check('chapter mode: auto is 9', re.search(r'test level: 9 \(auto\)', log) is not None, log[-400:])
    check('chapter mode: the roster lines say how far they went', re.search(r'test level: Kell \(roster\) Lv \d+ -> 9', log) is not None, log[-600:])
    s = save_of(sv, 'tp')  # the Continue save of the TEST run, in the temp folder only
    lv = levels_in(s)
    check('the roster holds the heroes of chapter 1 and the recruit of chapter 2 at the level',
          s is not None and all(lv.get(i) == 9 for i in ('eirik', 'kell', 'lyra', 'bram', 'mira')), str(lv))
    check('after the won battle the story goes on: the save (temp saves folder) is chapter 4', s is not None and s.get('chapter') == 'ch4', str(s)[:200] if s else 'no save')
    check('earlier levels above the number stay: nobody was lowered', all(v >= 9 for v in lv.values()), str(lv))

    # 4b. The before scene of the chapter plays first: skipping it writes the Continue save "before scene done" of chapter 3.
    sv4 = fresh('s_scene')
    code, log = play(['--game', 'tp', '--campaign', '--chapter', 'ch3', '--test-level', '7'], saves=sv4, inp=CARD_AND_SKIP, frames=150)
    s4 = save_of(sv4, 'tp')
    check('the chapter 3 before scene played and was saved as done', s4 is not None and s4['chapter'] == 'ch3' and s4['playthrough']['phase'] == 'battle', str(s4)[:200])

    # 5. Without the level the roster stands at the map's levels (Eirik 3, the others 2, Mira 1).
    sv5 = fresh('s_ch3_plain')
    code, log = play(['--game', 'tp', '--campaign', '--chapter', 'ch3'], saves=sv5,
                     inp=CARD_AND_SKIP + ',' + END_TURN + ',confirm,skip,wait:20', frames=900)
    lv = levels_in(save_of(sv5, 'tp'))
    check('--chapter alone: the base levels', lv.get('eirik') == 3 and lv.get('kell') == 2 and lv.get('mira') == 1 and 'test level' not in log, str(lv))

    # 6. Autoplay from a chapter plays the rest of the story (chapters 3 and 4) and saves nothing.
    sv6 = fresh('s_auto')
    code, log = play(['--game', 'tp', '--campaign', '--chapter', 'ch3', '--test-level', '7', '--autoplay'], saves=sv6, frames=3000)
    check('autoplay from chapter 3: two chapters won', code == 0 and len(re.findall(r'chapter \d battle over \(won\)', log)) == 2, log[-400:])
    check('autoplay from chapter 3 saves nothing', save_of(sv6, 'tp') is None)

    # 7. The command line.
    code, log = play(['--game', 'tp', '--chapter', 'ch3'], frames=10)
    check('--chapter needs --campaign', code != 0 and '--chapter' in log)
    code, log = play(['--game', 'tp', '--map', 'keep', '--test-level', 'abc'], frames=10)
    check('--test-level refuses a word that is not auto', code != 0 and '--test-level' in log)
    code, log = play(['--game', 'tp', '--map', 'keep', '--test-level', '0'], frames=10)
    check('--test-level refuses 0', code != 0)
    code, log = play(['--game', 'tp', '--campaign', '--chapter', 'nope'], frames=40, inp='wait:5')
    check('an unknown chapter shows an error and names the chapters', code == 0 and 'no chapter "nope"' in log and 'ch3' in log, log[-300:])
    code, log = play(['--help'], frames=1)
    check('--help describes both options', '--test-level' in log and '--chapter' in log, log[-200:])


def main():
    global PLAYER, repo
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    PLAYER, repo = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
    run_all()
    summary(blank_line=True)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
