#!/usr/bin/env python3
"""Difficulties through the real player, windowless (docs/DIFFICULTY.md).

  python3 tests/difficulty_player_test.py ./build/engine/player/ote-player .
  python3 tests/difficulty_player_test.py ./build/engine/player/ote-player . --shots OUT_DIR   # keep the choice screens (PNG)
  python3 tests/difficulty_player_test.py --make-games OUT_DIR .                                # only write the test games

Builds small games as copies of games/demo-srpg in a temp folder (never in the repo): a campaign with Easy / Normal / Hard
(and the three "change" rules), a plain game with free battles, a tall (270 x 480) copy for the phone-shaped picture.
Checks: the choice screen at New Game and before a free battle (input tokens), --difficulty (and its errors), enemy
stats and a hard-only unit on easy and hard (read from the Continue save), the map's own numbers, changing the
difficulty from the battle menu under each rule, Continue keeping it, and screenshots of the choice screen.
"""
import json
import os
import re
import shutil
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import run_text, check, failures, read_jsonc, save_of, summary, ticks  # noqa: E402
from ote_testkit import scratch_dir  # noqa: E402
jsonc = read_jsonc  # (the name this script uses)

repo = ''
GAMES = ''
PLAYER = ''
SHOTS = ''


bridge = None
UNITS = None  # unit type id -> {'stats': {...}, 'growths': {...}}
STATS = ['hp', 'str', 'skl', 'spd', 'def', 'mov']

DIFFICULTIES = {
    'default': 'normal', 'change': 'lowerOnly',
    'list': [
        {'id': 'easy', 'name': 'Easy', 'description': 'Gentler foes for a relaxed story.', 'enemy': {'levels': -1}, 'xpPercent': 150},
        {'id': 'normal', 'name': 'Normal', 'description': 'The game as its maker meant it.'},
        {'id': 'hard', 'name': 'Hard', 'description': 'Foes two levels stronger, and a little tougher.',
         'enemy': {'levels': 2, 'stats': {'def': 1}}, 'xpPercent': 80}]}


def U(type_, side, x, y, name, id_=None, **kw):
    u = {'type': type_, 'side': side, 'x': x, 'y': y, 'name': name}
    if id_:
        u['id'] = id_
    u.update(kw)
    return u


def make_map(name, blue, red, events, extra=None):
    m = {k: v for k, v in bridge.items()}
    m['name'] = name
    m['sides'] = [
        {'id': 'blue', 'name': 'Blue Company', 'color': '#3d85c6', 'controller': 'human', 'resources': {'gold': 100}},
        {'id': 'red', 'name': 'Red Warband', 'color': '#cc0000', 'controller': 'ai'}]
    m['units'] = blue + red
    m['events'] = events
    m.pop('objectives', None)
    m.update(extra or {})
    return m


def end_event():
    return {'id': 'finish', 'when': {'turn': 2, 'side': 'blue'}, 'do': [{'win': 'blue'}]}


def heroes(level=1):
    return [U('lord', 'blue', 2, 4, 'Eirik', 'eirik', level=level, leader=True),
            U('archer', 'blue', 1, 5, 'Lyra', 'lyra', level=level)]


def enemies(prefix):
    return [U('knight', 'red', 11, 4, 'Gorm', prefix + 'boss', level=3, leader=True, difficultyShift=False),
            U('soldier', 'red', 9, 4, 'Tane', prefix + 'tane', level=2),
            U('soldier', 'red', 9, 6, 'Extra', prefix + 'extra', level=2, difficulty=['hard'])]


def maps():
    return {
        'bridge': make_map('The Ford', heroes(), enemies('a_'), [end_event()]),
        # chapter 2: on hard the enemy is three levels higher here ("hard adds 3 enemy levels in chapter 2")
        'bridge2': make_map('Across the River', heroes(), enemies('b_'), [end_event()],
                            {'difficulty': {'hard': {'enemy': {'levels': 3}}}}),
        'keep': make_map('The Keep', heroes(), enemies('c_'), [end_event()]),
        'skirmish': make_map('Skirmish', heroes(), enemies('f_'), []),
    }


CAMPAIGN = {
    'name': 'Difficulty Test', 'rules': {'fallen': 'gone', 'onDefeat': 'retry'}, 'freeBattles': ['skirmish'],
    'chapters': [
        {'id': 'ch1', 'name': 'The Ford', 'map': 'bridge', 'before': [{'say': 'The ford.', 'speakerName': 'Narrator'}], 'after': []},
        {'id': 'ch2', 'name': 'Across the River', 'map': 'bridge2', 'before': [{'say': 'The river.', 'speakerName': 'Narrator'}], 'after': []},
        {'id': 'ch3', 'name': 'The Keep', 'map': 'keep', 'before': [], 'after': []}]}


def build(folder, change='lowerOnly', difficulties=True, campaign=True, view=None, title='Difficulty Test'):
    dst = os.path.join(GAMES, folder)
    shutil.rmtree(dst, ignore_errors=True)
    shutil.copytree(os.path.join(repo, 'games/demo-srpg'), dst)
    for f in ('characters', 'supports'):
        os.remove(dst + '/data/%s.json' % f)
    os.remove(dst + '/maps/bridge.json')
    for name, m in maps().items():
        if not difficulties:  # a game without difficulties may not name one anywhere
            m = json.loads(json.dumps(m))
            m.pop('difficulty', None)
            for u in m['units']:
                u.pop('difficulty', None)
                u.pop('difficultyShift', None)
        json.dump(m, open(dst + '/maps/%s.json' % name, 'w'), indent=1)
    g = open(dst + '/game.json').read()
    g = g.replace('"name": "Story Tactics Demo"', '"name": "%s"' % title)
    if view:
        g = g.replace('"view": { "width": 480, "height": 270, "tileSize": 24 }', '"view": { "width": %d, "height": %d, "tileSize": 24 }' % view)
    if difficulties:
        d = dict(DIFFICULTIES)
        d['change'] = change
        g = g.replace('"roles": { "hp": "hp", "move": "mov" },', '"roles": { "hp": "hp", "move": "mov" },\n  "difficulties": %s,' % json.dumps(d))
    g = re.sub(r'"maps": \[.*?\]\s*\}\s*$', '''"maps": [ { "id": "bridge", "name": "The Ford", "file": "maps/bridge.json" },
    { "id": "bridge2", "name": "Across the River", "file": "maps/bridge2.json" },
    { "id": "keep", "name": "The Keep", "file": "maps/keep.json" },
    { "id": "skirmish", "name": "Skirmish", "file": "maps/skirmish.json" } ]
}''', g, flags=re.S)
    open(dst + '/game.json', 'w').write(g)
    camp = dict(CAMPAIGN)
    if campaign:
        json.dump(camp, open(dst + '/data/campaign.json', 'w'), indent=1)
    elif os.path.exists(dst + '/data/campaign.json'):
        os.remove(dst + '/data/campaign.json')


def make_games():
    os.makedirs(GAMES, exist_ok=True)
    build('dcamp')
    build('dcamp_never', change='never')
    build('dcamp_free', change='free')
    build('dplain', campaign=False)
    build('dnone', difficulties=False, campaign=False)
    build('dtall', view=(270, 480))
    build('dwide', view=(480, 270))


# ---- running the player


def play(args, saves=None, frames=1500, interval=8, inp=None, shot=None, game_dir=None):
    if frames is None:  # just long enough for the script (and a moment after it)
        frames = ticks(inp) * interval + 40
    cmd = [PLAYER, '--games', game_dir or GAMES, '--headless', '--no-audio', '--no-settings', '--frames', str(frames),
           '--input-interval', str(interval), '--fixed-dt', '0.016']
    cmd += ['--saves', saves] if saves else ['--no-saving']
    if inp:
        cmd += ['--input', inp]
    if shot:
        cmd += ['--screenshot', shot]
    p = run_text(cmd + args)
    return p.returncode, p.stdout


def unit(save, uid):
    for u in save['battle']['units']:
        if u.get('id') == uid:
            return u
    return None


def grown(base, growth, levels):
    """The expected gain: growth% x levels, rounded to the nearest (halves away from zero)."""
    total = growth * levels
    return (total + 50) // 100 if total >= 0 else -((-total + 50) // 100)


def expected(type_, level_shift, flat_def=0):
    u = UNITS[type_]
    out = []
    for s in STATS:
        v = u['stats'][s] + grown(u['stats'][s], u['growths'].get(s, 0), level_shift)
        if s == 'def':
            v += flat_def
        out.append(v)
    return out


CARD = 'confirm,skip,wait:20'      # the title card and the before scene
END_TURN = 'menu,confirm,wait:60'


def scenarios(tmp):
    def fresh(name):
        d = os.path.join(tmp, name)
        shutil.rmtree(d, ignore_errors=True)
        return d

    # 1. --difficulty hard: no choice screen; the enemy is built two levels higher, the hard-only unit is there.
    sv = fresh('s_hard')
    code, log = play(['--game', 'dcamp', '--campaign', '--difficulty', 'hard'], saves=sv, inp=CARD + ',wait:30', frames=400)
    s = save_of(sv, 'dcamp')
    check('--difficulty hard: the playthrough and the battle are on hard',
          s and s['playthrough']['difficulty'] == 'hard' and s['battle']['difficulty'] == 'hard', log[-400:])
    check('--difficulty hard: no choice screen was shown', 'difficulty choice' not in log)
    check('the battle start logs its difficulty', 'battle: difficulty "hard"' in log, log[-300:])
    if s:
        tane = unit(s, 'a_tane')
        check('hard: the enemy soldier is two levels higher', tane and tane['level'] == 4, str(tane))
        check('hard: its stats are the growths of two levels plus 1 defense',
              tane and tane['stats'] == expected('soldier', 2, 1), '%s vs %s' % (tane and tane['stats'], expected('soldier', 2, 1)))
        boss = unit(s, 'a_boss')
        check('hard: the boss with difficultyShift false is as placed', boss and boss['level'] == 3 and boss['stats'] == expected('knight', 0), str(boss))
        check('hard: the hard-only unit is on the map', unit(s, 'a_extra') is not None)
        check('hard: the human hero is untouched', unit(s, 'eirik')['level'] == 1)

    # 2. --difficulty easy: lower level, no hard-only unit.
    sv = fresh('s_easy')
    code, log = play(['--game', 'dcamp', '--campaign', '--difficulty', 'easy'], saves=sv, inp=CARD + ',wait:30', frames=400)
    s = save_of(sv, 'dcamp')
    check('--difficulty easy: the battle is on easy', s and s['battle']['difficulty'] == 'easy', log[-300:])
    if s:
        tane = unit(s, 'a_tane')
        check('easy: the enemy soldier is one level lower with the matching stats',
              tane and tane['level'] == 1 and tane['stats'] == expected('soldier', -1), '%s vs %s' % (tane, expected('soldier', -1)))
        check('easy: the hard-only unit is not there', unit(s, 'a_extra') is None)

    # 3. Errors.
    code, log = play(['--game', 'dcamp', '--campaign', '--difficulty', 'extreme'], saves=fresh('s_err'), frames=40, inp='wait:5')
    check('--difficulty with an unknown id is a clear error', 'no such difficulty (this game has: easy, normal, hard)' in log, log[-300:])
    code, log = play(['--game', 'dnone', '--map', 'skirmish', '--difficulty', 'hard'], saves=fresh('s_err2'), frames=40, inp='wait:5')
    check('--difficulty on a game without difficulties is an error too', 'this game has no difficulties' in log, log[-300:])

    # 4. The choice screen at New Game: the default (normal) is preselected; up = easy, down = hard.
    sv = fresh('s_choose_default')
    code, log = play(['--game', 'dcamp'], saves=sv, inp='confirm,confirm,' + CARD + ',wait:30', frames=500)
    s = save_of(sv, 'dcamp')
    check('New game shows the choice screen with the default preselected', 'difficulty choice: 3 difficulties, default "normal"' in log, log[-300:])
    check('confirming the preselected default plays on normal', s and s['playthrough']['difficulty'] == 'normal' and s['battle']['difficulty'] == 'normal', log[-300:])
    check('normal builds the enemy as placed', s and unit(s, 'a_tane')['level'] == 2 and unit(s, 'a_extra') is None)
    sv = fresh('s_choose_up')
    code, log = play(['--game', 'dcamp'], saves=sv, inp='confirm,up,confirm,' + CARD + ',wait:30', frames=500)
    s = save_of(sv, 'dcamp')
    check('Up then Confirm chooses Easy', s and s['playthrough']['difficulty'] == 'easy' and 'campaign: difficulty "easy"' in log, log[-300:])
    sv = fresh('s_choose_down')
    code, log = play(['--game', 'dcamp'], saves=sv, inp='confirm,down,confirm,' + CARD + ',wait:30', frames=500)
    s = save_of(sv, 'dcamp')
    check('Down then Confirm chooses Hard', s and s['playthrough']['difficulty'] == 'hard' and unit(s, 'a_extra') is not None, log[-300:])
    sv = fresh('s_choose_cancel')
    code, log = play(['--game', 'dcamp'], saves=sv, inp='confirm,cancel,wait:10', frames=200)
    check('Cancel on the choice goes back to the game page and starts nothing', save_of(sv, 'dcamp') is None and 'campaign: new game' in log and 'campaign: difficulty' not in log, log[-300:])

    # 5. A free battle asks too (plain game: Play -> map list -> choice); --difficulty skips the question.
    sv = fresh('s_free')
    code, log = play(['--game', 'dplain'], saves=sv, inp='confirm,down,confirm,wait:30', frames=300)
    s = save_of(sv, 'dplain')
    check('a free battle asks for the difficulty and builds the battle with it',
          'difficulty choice: 3 difficulties' in log and s and s['battle']['difficulty'] == 'hard' and unit(s, 'a_extra') is not None, log[-400:])
    sv = fresh('s_free2')
    code, log = play(['--game', 'dplain', '--map', 'skirmish', '--difficulty', 'easy'], saves=sv, inp='wait:30', frames=200)
    s = save_of(sv, 'dplain')
    check('--map with --difficulty plays at that difficulty without asking', 'difficulty choice' not in log and s and s['battle']['difficulty'] == 'easy', log[-300:])
    code, log = play(['--game', 'dplain', '--map', 'skirmish'], saves=fresh('s_free3'), inp='wait:30', frames=200, )
    check('--map alone asks for the difficulty first', 'difficulty choice: 3 difficulties' in log, log[-300:])
    code, log = play(['--game', 'dcamp', '--autoplay', '--map', 'skirmish'], saves=fresh('s_free4'), frames=300)
    check('--autoplay never asks and plays the default', 'difficulty choice' not in log and 'battle: difficulty "normal"' in log, log[-300:])
    code, log = play(['--game', 'dnone', '--map', 'skirmish'], saves=fresh('s_free5'), inp='wait:30', frames=200)
    check('a game without difficulties shows no choice and logs none', 'difficulty choice' not in log and 'battle: difficulty' not in log, log[-300:])

    # 6. The map's own numbers (chapter 2: hard = +3 levels), and a change from the battle menu (lowerOnly: down only).
    sv = fresh('s_change')
    ch1 = CARD + ',' + END_TURN + ',confirm,wait:5'                  # chapter 1 won (it has no after scene)
    code, log = play(['--game', 'dcamp', '--campaign', '--difficulty', 'hard'], saves=sv, inp=ch1 + ',confirm,skip,wait:30', frames=900)
    s = save_of(sv, 'dcamp')
    check('chapter 2 reaches its battle on hard', s and s['chapter'] == 'ch2' and s['battle']['difficulty'] == 'hard', log[-400:])
    if s:
        tane = unit(s, 'b_tane')
        check('the map says hard = +3 levels: the soldier is at level 5 (replacing the game\'s +2)', tane and tane['level'] == 5, str(tane))
        check('...and its stats are three levels of growth plus 1 defense',
              tane and tane['stats'] == expected('soldier', 3, 1), '%s vs %s' % (tane and tane['stats'], expected('soldier', 3, 1)))
    # menu: End Turn, Unit, Difficulty, ...; the row opens a list of what the rule allows (easy, normal), the first is picked
    code, log = play(['--game', 'dcamp', '--continue'], saves=sv, inp='wait:30,cancel,menu,down,down,confirm,confirm,wait:30', frames=400)
    s = save_of(sv, 'dcamp')
    check('lowerOnly: the menu offers only easier ones (easy, normal)', 'difficulty menu: now hard, can change to easy, normal' in log, log[-500:])
    check('lowerOnly: the change is saved in the playthrough and the battle keeps its own',
          s and s['playthrough']['difficulty'] == 'easy' and s['battle']['difficulty'] == 'hard', log[-300:])
    # the next battle (chapter 3) is built on easy
    code, log = play(['--game', 'dcamp', '--continue'], saves=sv, inp='wait:30,cancel,' + END_TURN + ',confirm,wait:40,confirm,wait:40', frames=None)
    s = save_of(sv, 'dcamp')
    check('the change applies from the next battle built (chapter 3 on easy)',
          s and s['chapter'] == 'ch3' and s['battle']['difficulty'] == 'easy' and unit(s, 'c_tane')['level'] == 1, log[-500:])

    # 7. "never": the row is greyed and nothing changes.
    sv = fresh('s_never')
    code, log = play(['--game', 'dcamp_never', '--campaign', '--difficulty', 'hard'], saves=sv, inp='confirm,skip,wait:30,cancel,menu,down,down,confirm,wait:20', frames=300)
    check('never: the Difficulty row is greyed with the reason', 'difficulty row "Hard (fixed)" greyed' in log and 'difficulty menu' not in log, log[-500:])
    check('never: the difficulty stays', save_of(sv, 'dcamp_never')['playthrough']['difficulty'] == 'hard')

    # 8. "free": any difficulty, up as well as down.
    sv = fresh('s_free_rule')
    code, log = play(['--game', 'dcamp_free', '--campaign', '--difficulty', 'normal'], saves=sv, inp='confirm,skip,wait:30,cancel,menu,down,down,confirm,down,confirm,wait:30', frames=400)
    s = save_of(sv, 'dcamp_free')
    check('free: the menu offers the others (easy, hard) and a harder one can be chosen',
          'difficulty menu: now normal, can change to easy, hard' in log and s and s['playthrough']['difficulty'] == 'hard' and s['battle']['difficulty'] == 'normal', log[-500:])

    # 9. A free battle shows the row greyed ("this battle"); Continue keeps the playthrough's difficulty.
    sv = fresh('s_free_row')
    code, log = play(['--game', 'dplain', '--map', 'skirmish', '--difficulty', 'hard'], saves=sv, inp='wait:30,cancel,menu,wait:10', frames=None)
    check('a free battle: the row is greyed (this battle)', 'difficulty row "Hard (this battle)" greyed' in log, log[-300:])
    sv = fresh('s_continue')
    play(['--game', 'dcamp', '--campaign', '--difficulty', 'hard'], saves=sv, inp=CARD + ',wait:20', frames=300)
    code, log = play(['--game', 'dcamp', '--continue'], saves=sv, inp='wait:20', frames=100)
    s = save_of(sv, 'dcamp')
    check('Continue resumes the battle on the difficulty it was built with', s and s['battle']['difficulty'] == 'hard' and 'chapter The Ford, battle' in log, log[-300:])

    # 10. Screenshots of the choice screen (wide and tall), kept when --shots is given.
    shots = SHOTS or os.path.join(tmp, 'shots')
    os.makedirs(shots, exist_ok=True)
    for gid, name in (('dwide', 'choice-480x270'), ('dtall', 'choice-270x480')):
        bmp = os.path.join(tmp, name + '.bmp')
        code, log = play(['--game', gid, '--map', 'skirmish'], saves=fresh('s_shot_' + gid), inp='down,wait:5', frames=60, shot=bmp)
        ok = os.path.exists(bmp)
        check('screenshot %s exists' % name, ok, log[-200:])
        if ok:
            try:
                from PIL import Image
                Image.open(bmp).save(os.path.join(shots, name + '.png'))
            except Exception as e:  # PIL missing: keep the BMP
                shutil.copy(bmp, os.path.join(shots, name + '.bmp'))
                print('note: no PNG conversion (%s)' % e)


def main():
    global PLAYER, repo, GAMES, bridge, UNITS, SHOTS
    args = sys.argv[1:]
    if len(args) == 3 and args[0] == '--make-games':
        repo = os.path.abspath(args[2])
        GAMES = os.path.abspath(args[1])
        bridge = jsonc(os.path.join(repo, 'games/demo-srpg/maps/bridge.json'))
        make_games()
        return 0
    if '--shots' in args:
        i = args.index('--shots')
        SHOTS = os.path.abspath(args[i + 1])
        del args[i:i + 2]
    if len(args) < 2:
        print(__doc__)
        return 2
    PLAYER, repo = os.path.abspath(args[0]), os.path.abspath(args[1])
    bridge = jsonc(os.path.join(repo, 'games/demo-srpg/maps/bridge.json'))
    UNITS = {u['id']: u for u in jsonc(os.path.join(repo, 'games/demo-srpg/data/units.json'))['units']}
    tmp = scratch_dir()
    GAMES = os.path.join(tmp, 'games')
    try:
        make_games()
        scenarios(tmp)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    summary(blank_line=True)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
