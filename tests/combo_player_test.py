#!/usr/bin/env python3
"""Levels + merit ranks + class jobs + overlay jobs in the real player, windowless (docs/JOBS.md "Class jobs and overlay jobs", docs/SRPG.md "Unit ranks" merit mode).

  python3 tests/combo_player_test.py ./build/engine/player/ote-player . [--shots DIR]

Plays a copy of the synthetic game tests/fixtures/combo (not a demo). Free battle "duel": the unit details of Aldric (a Squire in the class job Squire) say the class, the job,
the level AND the rank with its Merit counter, and the panel shows both, at UI scale 100 and 150 without a UI bug. Campaign: in chapter 1 Aldric (merit 8) strikes the dummy and
earns Veteran (merit 13, then 18 with the counter-strike that also levels him up; the rank-up box opens); the camp's Jobs screen lists the overlay jobs Scout and Warden without a "Plays as" line and with their bonus; Aldric becomes a
Warden and keeps being a Squire by type; the Continue save holds the merit, the job and the base type; the preparation screen and the battle of chapter 2 carry them. Judged from
the [info] lines the player logs, the save files and the screenshots (--shots DIR keeps them, BMP).
"""
import json
import os
import re
import shutil
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import Bmp, check, edit, player_and_repo, run_text, save_of, scratch_dir, shots_dir, summary, ticks  # noqa: E402

sys.dont_write_bytecode = True
PLAYER, REPO = player_and_repo()
SHOTS = shots_dir(absolute=True)
TMP = scratch_dir()
GAMES = os.path.join(TMP, 'games')

IDLE = 'wait:14'
INFO = IDLE + ',confirm,wait:3,confirm,wait:3,down,confirm,wait:10'  # select Aldric, stay, the menu (Attack, Info, Wait): Info
FIGHT = 'confirm,confirm,confirm,confirm,wait:100'  # select Aldric, open the menu, Attack, the dummy beside him, confirm: one strike
END_TURN = 'menu,confirm,wait:60'
# Aldric's counter-strike during the red turn levels him up (a box that waits for Confirm) after the dummy's own rank-up box; then the result banner.
CHAPTER_ONE = 'confirm,wait:12,' + FIGHT + ',' + END_TURN + ',wait:200,confirm,wait:60,confirm,wait:30'
OPEN_JOBS = 'confirm,wait:5'
TO_ALDRIC = OPEN_JOBS + ',confirm,wait:5'
TAKE_WARDEN = 'down,down,down,down,confirm,wait:5'  # the Jobs tab: Squire (current), Knight, Mage, Scout, Warden
LEAVE = 'cancel,wait:3,cancel,wait:5'  # the tab back to the character list, then to the camp
TO_PREP = 'wait:30,down,confirm,wait:80'  # from the camp: Continue the story (the title card, then the preparation screen)
BEGIN = 'down,down,down,down,down,down,down,confirm,wait:5,confirm,wait:60,confirm,wait:60'  # the menu: Units ... Begin is the eighth row; then the promotion box of Aldric (a Warden at level 2) is dismissed


def make_game():
    shutil.rmtree(GAMES, ignore_errors=True)
    g = os.path.join(GAMES, 'cg')
    shutil.copytree(os.path.join(REPO, 'tests', 'fixtures', 'combo'), g)

    def win(j):
        j['sides'][0]['color'] = '#3060ff'
        j['sides'][1]['color'] = '#ff3030'
        j['events'] = [{'id': 'finish', 'when': {'turn': 2, 'side': 'blue'}, 'do': [{'win': 'blue'}]}]
    edit(g, 'maps/first.json', win)

    def second(j):
        win(j)
        j['prep'] = {'slots': [{'x': 1, 'y': 1}, {'x': 1, 'y': 2}, {'x': 1, 'y': 3}, {'x': 0, 'y': 2}], 'limit': 3}
    edit(g, 'maps/second.json', second)


def play(inp, saves=None, args=None, name='run', scale=None, free=False):
    cmd = [PLAYER, '--games', GAMES, '--assets', os.path.join(REPO, 'assets'), '--headless', '--no-audio', '--no-settings',
           '--frames', str(ticks(inp) * 3 + 1500), '--input-interval', '3', '--fixed-dt', '0.016', '--game', 'cg', '--input', inp]
    if free:
        cmd += ['--map', 'duel', '--no-saving']
    else:
        cmd += ['--saves', saves] + (args or ['--campaign'])
    pic = os.path.join(TMP, name + '.bmp')
    cmd += ['--screenshot', pic]
    if scale:
        cmd += ['--ui-scale', str(scale)]
    log = run_text(cmd).stdout
    if SHOTS and os.path.exists(pic):
        os.makedirs(SHOTS, exist_ok=True)
        shutil.copy(pic, os.path.join(SHOTS, 'combo_' + name + '.bmp'))
    return log


def info(log, prefix):
    out = []
    for line in log.split('\n'):
        m = re.search(r'\[info\]\s*(.*)$', line)
        if m and m.group(1).startswith(prefix):
            out.append(m.group(1))
    return out


def clean(log):
    return 'UI bug' not in log and '[warn]' not in log and '[error]' not in log


def copy_saves(src, name):
    dst = os.path.join(TMP, name)
    shutil.rmtree(dst, ignore_errors=True)
    shutil.copytree(src, dst)
    return dst


def roster(save, cid):
    for r in save['playthrough']['roster']:
        if r['id'] == cid:
            return r
    return None


def lit(path):
    p = Bmp(path)
    return p.w * p.h - p.count((18, 20, 32), tol=30)


def main():
    make_game()

    # ---- a free battle: the details and the panel say the level AND the rank with its counter
    for tag, scale in (('s100', None), ('s150', 150)):
        log = play(INFO, name='details_' + tag, scale=scale, free=True)
        ident = info(log, 'unit identity: Aldric:')
        check('details at UI scale %s: class, job, level and the rank with the Merit counter are all there' % (scale or 100),
              ident and ident[0] == 'unit identity: Aldric: Class: Squire | Job: Squire  Lv 2 JP 30 | Level: 1  XP 0/20 | Rank: None Merit 0/10', ident)
        check('... no UI bug or warning, and the page is drawn', clean(log) and lit(os.path.join(TMP, 'details_%s.bmp' % tag)) > 3000, log[-300:])
        log = play(IDLE + ',confirm,wait:3,confirm,wait:3,cancel,wait:3', name='panel_' + tag, scale=scale, free=True)
        check('the panel at UI scale %s: no UI bug or warning' % (scale or 100), clean(log), log[-300:])

    # ---- chapter 1: the hit earns Veteran (merit 8 + 5, then 5 more for the counter-strike); then the camp: Jobs -> Aldric
    sv = os.path.join(TMP, 'saves')
    log = play(CHAPTER_ONE + ',' + TO_ALDRIC + ',wait:5', sv, name='camp')
    box = info(log, 'rank up box:')
    check('the strike earns Aldric the rank Veteran from merit (the struck dummy earns merit too, but stays below)', box and box[0].startswith('rank up box: Aldric - Veteran (+1 Strength)'), (box, log[-400:]))
    expl = info(log, 'jobs explanations aldric:')
    check('the Jobs tab explains the class jobs by their type and the overlay jobs by their bonus (no "Plays as" for an overlay)',
          expl and 'Knight: Heavy armour. Plays as a Knight.' in expl[0] and 'Scout: Keeps its own class; quick and keen. Bonus: +1 Strength, +1 Move.' in expl[0]
          and 'Warden: Keeps its own class; stands firm. Bonus: +2 Defense.' in expl[0] and 'Plays as a Squire' not in expl[0], expl[:1])
    jobs_tab = info(log, 'jobs Jobs aldric:')
    check('the Jobs tab lists the five jobs', jobs_tab and jobs_tab[0] == 'jobs Jobs aldric: Squire[Current Lv 2], Knight[Lv 1], Mage[Lv 1], Scout[Lv 1], Warden[Lv 1]', jobs_tab[:1])
    check('... nothing warned about', clean(log), log[-300:])

    log = play(CHAPTER_ONE + ',' + TO_ALDRIC + ',' + TAKE_WARDEN + ',' + LEAVE, sv, name='warden')
    check('Aldric takes the overlay job Warden', 'jobs: Now a Warden. (aldric)' in log, log[-600:])
    s = save_of(sv, 'cg')
    a = s and roster(s, 'aldric')
    check('the Continue save holds the merit, the job and the base type; the unit type is still squire (nothing was swapped) and Aldric is level 2',
          a and a['type'] == 0 and a['merit'] == 18 and a['jobs']['job'] == 4 and a['jobs']['baseType'] == 0 and a['level'] == 2, a and (a['type'], a.get('merit'), a['jobs'], a.get('level')))
    b = s and roster(s, 'bryn')
    check('Bryn (a Scout, an overlay job from the start) has his own class kept as the base type', b and b['type'] == 0 and b['jobs']['job'] == 3 and b['jobs']['baseType'] == 0, b and (b['type'], b['jobs']))

    # ---- chapter 2: the preparation screen and the battle carry it
    sv2 = copy_saves(sv, 'saves2')
    log = play(TO_PREP + ',' + BEGIN, sv2, args=['--continue'], name='prep')
    check('the preparation screen and the battle begin without a warning, and Aldric (a Warden, level 2) is promoted', 'prep: begin with' in log and 'promotion plays: Aldric, Squire to Captain' in log and clean(log), log[-500:])
    s = save_of(sv2, 'cg')
    units = (s or {}).get('battle', {}).get('units', [])
    ald = [u for u in units if u.get('jobs', {}).get('job') == 4]
    check('the battle holds Aldric as a Warden with merit 18, promoted to Captain (an overlay job promotes) and the base type moved with him', ald and ald[0]['merit'] == 18 and ald[0]['type'] == 1 and ald[0]['jobs']['baseType'] == 1, ald)
    check('every unit of the battle save carries a merit counter (the game has ranks)', units and all('merit' in u for u in units), [u.get('merit') for u in units])

    # ---- the Jobs screen at UI scale 100 and 150 (the overlay explanation is on it)
    for tag, scale in (('s100', None), ('s150', 150)):
        log = play(TO_ALDRIC, copy_saves(sv, 'saves3'), args=['--continue'], name='jobs_' + tag, scale=scale)
        check('the Jobs tab at UI scale %s: no UI bug or warning, a panel is drawn' % (scale or 100), clean(log) and lit(os.path.join(TMP, 'jobs_%s.bmp' % tag)) > 3000, log[-300:])
        log = play(TO_ALDRIC + ',down,down,down,wait:5', copy_saves(sv, 'saves3'), args=['--continue'], name='scout_' + tag, scale=scale)
        check('... with the cursor on the Scout (its bonus is the explanation): no UI bug', clean(log), log[-300:])
    summary()
    return 1 if __import__('ote_testkit').failures else 0


if __name__ == '__main__':
    sys.exit(main())
