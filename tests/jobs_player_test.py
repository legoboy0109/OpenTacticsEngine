#!/usr/bin/env python3
"""The job system in the real player, windowless (docs/JOBS.md "Player UI").

  python3 tests/jobs_player_test.py ./build/engine/player/ote-player . [--shots DIR]

Plays a copy of the synthetic game tests/fixtures/jobs (not a demo) as a campaign with --saves on a temp folder. Chapter 1: Aldric (Squire, 30 JP) strikes the dummy once
and is struck back at (+10 JP each: 50), the chapter is won. The camp offers "Jobs": the test changes Aldric to Knight, learns Guard (30 JP), equips it and leaves; the
Continue save holds all of it. Chapter 2 has a preparation screen (the game says changeIn "camp+prep"): Jobs on that screen changes Cora from Mage to Squire, Begin starts the
battle, and the battle in the save has Aldric as a Knight with Guard and Cora as a Squire. A locked job says why, UI scale 100 and 150 draw every screen without a UI bug, and
a game without jobs has no Jobs row in the camp. Judged from the [info] lines the player logs, the save files and the screenshots (--shots DIR keeps them, BMP).
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

FIGHT = 'confirm,confirm,confirm,confirm,wait:100'  # select Aldric, open the menu, Attack, the dummy beside him, confirm: one strike
END_TURN = 'menu,confirm,wait:60'                   # blue ends its turn; the map's event wins at the start of round 2
CHAPTER_ONE = 'confirm,wait:12,' + FIGHT + ',' + END_TURN + ',confirm,wait:10'  # the title card, the fight, the end of the turn, the result banner: the camp is up
# In the camp: Jobs, Aldric; on the Jobs tab down to Knight and take it; Learn tab (right) takes Guard; Equip tab (right) equips it; back out to the camp.
OPEN_JOBS = 'confirm,wait:5'
TO_ALDRIC = OPEN_JOBS + ',confirm,wait:5'
TAKE_KNIGHT = 'down,confirm,wait:5'
LEARN_GUARD = 'right,confirm,wait:5'
EQUIP_GUARD = 'right,confirm,wait:5'
LEAVE = 'cancel,wait:3,cancel,wait:3,cancel,wait:5'
# On the preparation screen (main menu: Units, Positions, Items, Jobs, ...): Jobs, Cora (third), the first job (Squire) taken; back out; Begin (last row) and its question.
PREP_JOBS = 'down,down,down,confirm,wait:5,down,down,confirm,wait:5,confirm,wait:5,cancel,wait:3,cancel,wait:3,cancel,wait:5'
BEGIN = 'up,confirm,wait:5,confirm,wait:30'
TO_PREP = 'down,confirm,wait:20,confirm,wait:10'  # from the camp: Continue the story, the title card


def make_game(prep=True, jobs=True):
    shutil.rmtree(GAMES, ignore_errors=True)
    g = os.path.join(GAMES, 'jg')
    shutil.copytree(os.path.join(REPO, 'tests', 'fixtures', 'jobs'), g)

    def win(j):
        j['sides'][0]['color'] = '#3060ff'
        j['sides'][1]['color'] = '#ff3030'
        j['events'] = [{'id': 'finish', 'when': {'turn': 2, 'side': 'blue'}, 'do': [{'win': 'blue'}]}]

    edit(g, 'maps/first.json', win)

    def second(j):
        win(j)
        if prep:
            j['prep'] = {'slots': [{'x': 1, 'y': 1}, {'x': 1, 'y': 2}, {'x': 1, 'y': 3}, {'x': 0, 'y': 2}], 'limit': 3}
    edit(g, 'maps/second.json', second)
    if not jobs:  # the same game with no job anywhere
        os.remove(os.path.join(g, 'data', 'jobs.json'))

        def game(j):
            del j['rules']['jobs']
        edit(g, 'game.json', game)

        def chars(j):
            for c in j['characters']:
                for k in ('job', 'jobs', 'equipped', 'learned'):
                    c.pop(k, None)
        edit(g, 'data/characters.json', chars)

        def plain(j):
            for u in j['units']:
                if 'job' in u:
                    u['type'] = 'mage'
                for k in ('job', 'jobs', 'equipped', 'learned'):
                    u.pop(k, None)
        edit(g, 'maps/second.json', plain)
        edit(g, 'maps/duel.json', plain)


def play(inp, saves, args=None, name='run', scale=None):
    cmd = [PLAYER, '--games', GAMES, '--assets', os.path.join(REPO, 'assets'), '--headless', '--no-audio', '--no-settings',
           '--frames', str(ticks(inp) * 3 + 120), '--input-interval', '3', '--fixed-dt', '0.016', '--game', 'jg', '--saves', saves,
           '--input', inp] + (args or ['--campaign'])
    pic = os.path.join(TMP, name + '.bmp')
    cmd += ['--screenshot', pic]
    if scale:
        cmd += ['--ui-scale', str(scale)]
    log = run_text(cmd).stdout
    if SHOTS and os.path.exists(pic):
        os.makedirs(SHOTS, exist_ok=True)
        shutil.copy(pic, os.path.join(SHOTS, 'jobs_' + name + '.bmp'))
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
    """How many pixels of the picture are not the dark launcher background: a screen is on it."""
    p = Bmp(path)
    return p.w * p.h - p.count((18, 20, 32), tol=30)


def main():
    make_game()
    sv = os.path.join(TMP, 'saves')

    # ---- chapter 1, then the camp: Jobs -> Aldric -> Knight, Guard learned and equipped
    log = play(CHAPTER_ONE + ',' + TO_ALDRIC + ',' + TAKE_KNIGHT + ',' + LEARN_GUARD + ',' + EQUIP_GUARD + ',' + LEAVE, sv, name='camp')
    check('the camp offers Jobs after the won chapter', info(log, 'camp: Jobs (Change, learn, equip)'), log[-600:])
    check('the character list names the two roster characters', 'jobs who: aldric, bryn' in log, log[-800:])
    jobs_tab = info(log, 'jobs Jobs aldric:')
    check('the Jobs tab shows the current job and the level of the others', jobs_tab and jobs_tab[0] == 'jobs Jobs aldric: Squire[Current Lv 2], Knight[Lv 1], Mage[Lv 1]', jobs_tab)
    check('Knight is taken (Squire level 2 unlocked it)', 'jobs: Now a Knight. (aldric)' in log, log[-800:])
    learn = info(log, 'jobs Learn aldric:')
    check('the Learn tab lists each job with its level and the JP left: 50 earned in chapter 1 (30 + 10 for the strike + 10 for the answer)',
          learn and learn[0].startswith('jobs Learn aldric: Squire  Lv 2  JP 50:, Guard[30 JP], Shove (A)[40 JP], Light Step[25 JP], Knight  Lv 1  JP 0:'), learn[:1])
    check('Guard is learned and the JP are spent (20 left)', 'jobs: Learned Guard. (aldric)' in log and any('Squire  Lv 2  JP 20:, Guard[Learned]' in l for l in learn), learn)
    equip = info(log, 'jobs Equip aldric:')
    check('the Equip tab offers Guard under the Support slots only', equip and equip[0] == 'jobs Equip aldric: Support  0/1:, Guard[Squire]', equip[:1])
    check('Guard is equipped', 'jobs: Equipped Guard. (aldric)' in log and equip[-1] == 'jobs Equip aldric: Support  1/1:, Guard[Equipped]', equip[-1:])
    check('leaving the screens goes back to the camp', log.count('camp: Jobs (Change, learn, equip)') == 2, log[-300:])
    check('... nothing warned about', clean(log), log[-300:])
    s = save_of(sv, 'jg')
    a = s and roster(s, 'aldric')
    check('the Continue save holds the new job, Squire progress (50 earned, 30 spent, Guard learned) and Guard equipped',
          a and a['type'] == 1 and a['jobs']['job'] == 1 and a['jobs']['progress'] == [{'job': 0, 'earned': 50, 'spent': 30, 'learned': [0]}] and a['jobs']['equipped'] == [0],
          a and (a['type'], a['jobs']))
    check('... and the stats moved with the unit type (Knight hp 36, str 7, def 5, mov 3)', a and a['stats'] == [36, 7, 5, 3], a and a['stats'])

    # ---- continue: the camp comes back, "Continue the story", the preparation screen with Jobs; Cora becomes a Squire; Begin
    sv2 = copy_saves(sv, 'saves2')
    log = play(TO_PREP + ',' + PREP_JOBS + ',' + BEGIN, sv2, args=['--continue'], name='prep')
    check('the camp is back after loading the save', info(log, 'camp: Jobs'), log[-500:])
    check('the preparation screen offers Jobs between Items and View map', 'prep menu: Units, Positions, Items, Jobs, View map, Save, Leave, Begin' in log, log[-600:])
    check('the three characters are listed (Cora joined from the map)', 'jobs who: aldric, bryn, cora' in log, log[-800:])
    check('Cora is a Mage and can take Squire', 'jobs Jobs cora: Squire[Lv 2], Knight[Lv 1], Mage[Current Lv 1]' in log and 'jobs: Now a Squire. (cora)' in log, log[-800:])
    check('the preparation was rebuilt for the change and the battle began', 'prep: deployed 3/3: aldric@1, bryn@2, cora@3' in log and 'prep: begin with 3' in log, log[-600:])
    check('... nothing warned about', clean(log), log[-300:])
    s = save_of(sv2, 'jg')
    battle = s and s.get('battle')
    check('the save has a battle under way', battle is not None, str(s and s.keys()))
    byjob = [u for u in (battle or {}).get('units', []) if 'jobs' in u]
    check('the battle holds the job state of Aldric (Knight, Guard equipped), Cora and Bryn (Squires) and the red shaman (Mage)',
          sorted(u['jobs']['job'] for u in byjob) == [0, 0, 1, 2] and any(u['jobs'].get('equipped') == [0] for u in byjob), [u['jobs'] for u in byjob])
    cora = roster(s, 'cora')
    check('the roster has Cora as a Squire (her Mage JP kept)', cora and cora['jobs']['job'] == 0 and any(p['job'] == 2 and p['earned'] == 10 for p in cora['jobs']['progress']), cora and cora['jobs'])
    check('... as the unit type squire with the stats of one (hp 30 str 6 def 2 mov 4: mage 22 / 3 / 1 / 4 plus the difference 8 / 3 / 1 / 0)', cora and cora['type'] == 0 and cora['stats'] == [30, 6, 2, 4], cora and (cora['type'], cora['stats']))

    # ---- a locked job says why (Bryn has no JP): the refusal when it is chosen
    sv3 = copy_saves(sv, 'saves3')
    log = play(OPEN_JOBS + ',down,confirm,wait:5,down,confirm,wait:5', sv3, args=['--continue'], name='locked')
    check('choosing a locked job says why and changes nothing', 'jobs refused: Needs Squire level 2.' in log and 'jobs: Now' not in log, log[-600:])

    # ---- every screen at UI scale 100 and 150: no UI bug, a panel is drawn
    for tag, scale in (('s100', None), ('s150', 150)):
        screens = [('who', OPEN_JOBS), ('jobs', TO_ALDRIC), ('learn', TO_ALDRIC + ',right,wait:5'), ('equip', TO_ALDRIC + ',right,right,wait:5')]
        for what, script in screens:
            log = play(script, copy_saves(sv, 'saves4'), args=['--continue'], name='%s_%s' % (what, tag), scale=scale)
            check('%s at UI scale %s: no UI bug or warning, and a panel is drawn' % (what, scale or 100), clean(log) and lit(os.path.join(TMP, '%s_%s.bmp' % (what, tag))) > 3000, log[-300:])
        log = play(TO_PREP + ',down,down,down,confirm,wait:5,confirm,wait:5,wait:5', copy_saves(sv, 'saves4'), args=['--continue'], name='prepjobs_' + tag, scale=scale)
        check('the Jobs screen over the preparation at UI scale %s: no UI bug' % (scale or 100), clean(log) and 'jobs Jobs aldric:' in log, log[-300:])

    # ---- a game without jobs: no Jobs row, no job state in the save
    make_game(jobs=False)
    sv5 = os.path.join(TMP, 'saves5')
    log = play(CHAPTER_ONE, sv5, name='nojobs')
    check('without data/jobs.json there is no camp (nothing waits there) and no job line in the log', 'camp:' not in log and 'jobs ' not in log.replace('Jobs Fixture', ''), log[-500:])
    s = save_of(sv5, 'jg')
    check('... and the save has no job state', s is not None and '"jobs"' not in json.dumps(s), json.dumps(s)[:200])
    summary()
    return 1 if __import__('ote_testkit').failures else 0


if __name__ == '__main__':
    sys.exit(main())
