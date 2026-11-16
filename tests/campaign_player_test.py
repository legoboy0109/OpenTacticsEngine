#!/usr/bin/env python3
"""Plays a campaign through the real player, windowless (docs/EVENTS.md "Campaign", docs/SAVES.md).

  python3 tests/campaign_player_test.py ./build/engine/player/ote-player .
  python3 tests/campaign_player_test.py --make-games OUT_DIR .     # only writes the test games (tools/webtest uses it)

Builds three small campaign games as copies of games/demo-srpg in a temp folder (never in the repo), then runs
the player with --input scripts and --saves on a temp folder and checks the log lines and the save files:
New game through three chapters (carry-over, scenes, saves after scenes and a won battle), Leave battle +
--continue, a slot save + --load-slot, defeat with retry, defeat with the story ending, the end screen,
free battles never touching the Continue file, and the command line. Pictures are checked by hand (see
HANDOFF.md); here only that the player runs and writes what it should.
"""
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

repo = '.'
src = ''
out = ''
def jsonc(path):
    text = open(path).read()
    text = '\n'.join(l for l in text.split('\n') if not l.strip().startswith('//'))
    return json.loads(text)


bridge = None  # loaded in make_games()


def U(type_, side, x, y, name, id_=None, **kw):
    u = {'type': type_, 'side': side, 'x': x, 'y': y, 'name': name}
    if id_:
        u['id'] = id_
    u.update(kw)
    return u


def make_map(name, blue, red, gold, storage, events):
    m = {k: v for k, v in bridge.items()}
    m['name'] = name
    m['sides'] = [
        {'id': 'blue', 'name': 'Blue Company', 'color': '#3d85c6', 'controller': 'human',
         'resources': {'gold': gold}, 'storage': storage},
        {'id': 'red', 'name': 'Red Warband', 'color': '#cc0000', 'controller': 'ai'}]
    m['units'] = blue + red
    m['events'] = events
    m.pop('objectives', None)  # the demo's own story: these games decide battles by their events
    return m


def end_event(lose=False, extra=None):
    do = list(extra or [])
    do.append({'lose': 'blue'} if lose else {'win': 'blue'})
    return {'id': 'finish', 'when': {'turn': 2, 'side': 'blue'}, 'do': do}


def maps(lose_first):
    ch1 = make_map(
        'The Ford',
        [U('lord', 'blue', 2, 4, 'Eirik', 'eirik', level=3, leader=True, stats={'hp': 24, 'str': 7, 'def': 5}),
         U('cavalier', 'blue', 3, 3, 'Kell', 'kell', level=2),
         U('archer', 'blue', 1, 5, 'Lyra', 'lyra', level=2),
         U('fighter', 'blue', 3, 5, 'Bram', 'bram', level=2)],
        [U('knight', 'red', 11, 4, 'Gorm', 'gorm', level=3, leader=True),
         U('soldier', 'red', 9, 4, 'Tane', level=1),
         U('soldier', 'red', 8, 3, 'Wanderer', 'wanderer', level=5)],
        700, ['mending_salve'],
        [{'id': 'recruit', 'when': {'battle': 'start'}, 'do': [{'changeSide': 'wanderer', 'side': 'blue'}]},
         end_event(lose_first, [] if lose_first else [{'removeUnit': 'bram'}])])
    # Chapters 2 and 3 name their heroes at level 1: what the party carries (levels, stats) must override that.
    def hero_set():
        return [U('lord', 'blue', 2, 4, 'Eirik', 'eirik', level=1, leader=True),
                U('cavalier', 'blue', 3, 3, 'Kell', 'kell', level=1),
                U('archer', 'blue', 1, 5, 'Lyra', 'lyra', level=1),
                U('fighter', 'blue', 3, 5, 'Bram', 'bram', level=1),
                U('soldier', 'blue', 2, 6, 'Wanderer', 'wanderer', level=1)]
    ch2 = make_map('Across the River', hero_set(),
                   [U('knight', 'red', 11, 4, 'Brann', 'brann', level=3, leader=True), U('soldier', 'red', 9, 4, 'Tane', level=1)],
                   150, [], [end_event()])
    ch3 = make_map('The Keep', hero_set(),
                   [U('knight', 'red', 11, 4, 'Warden', 'warden', level=4, leader=True), U('archer', 'red', 9, 5, 'Vex', level=2)],
                   0, [], [end_event()])
    free = make_map('Skirmish', [U('lord', 'blue', 2, 4, 'Eirik', level=1, leader=True), U('archer', 'blue', 1, 5, 'Lyra', level=1)],
                    [U('knight', 'red', 11, 4, 'Gorm', level=1, leader=True), U('soldier', 'red', 9, 4, 'Tane', level=1)],
                    100, [], [])
    return {'bridge': ch1, 'bridge2': ch2, 'keep': ch3, 'skirmish': free}


def campaign(on_defeat, fallen, free):
    return {
        'name': 'The Ford War',
        'rules': {'fallen': fallen, 'onDefeat': on_defeat},
        'freeBattles': free,
        'chapters': [
            {'id': 'ch1', 'name': 'The Ford', 'map': 'bridge',
             'before': [{'say': 'We hold the ford. Nobody crosses today.', 'speakerName': 'Eirik', 'portrait': 'eirik'},
                        {'say': 'The river is swollen and the enemy is close.', 'speakerName': 'Narrator'},
                        {'addVariable': 'chapters', 'by': 1}],
             'after': [{'say': 'It is done. The ford is ours.', 'speaker': 'lyra'},
                       {'giveResource': {'gold': 100}, 'side': 'blue'},
                       {'if': [{'alive': 'wanderer'}], 'then': [{'say': 'The soldier from the river joins us.', 'speaker': 'eirik', 'portrait': 'eirik'}],
                        'else': [{'say': 'We march on alone.'}]}]},
            {'id': 'ch2', 'name': 'Across the River', 'map': 'bridge2',
             'before': [{'say': 'Beyond the water lies the road to the keep.', 'speakerName': 'Narrator'}],
             'after': []},
            {'id': 'ch3', 'name': 'The Keep', 'map': 'keep',
             'before': [],
             'after': [{'say': 'The keep falls silent. The war is over.', 'speakerName': 'Narrator'}]}]}


def build(folder, title, on_defeat, fallen, free, lose_first):
    dst = os.path.join(out, folder)
    if os.path.exists(dst):
        shutil.rmtree(dst)
    shutil.copytree(src, dst)
    # D1: the demo's characters would own the ids these test maps use (eirik, kell, ...) and its supports need them;
    # this test is about the campaign flow, so the copy goes without both.
    for f in ('characters', 'supports'):
        os.remove(dst + '/data/%s.json' % f)
    os.remove(dst + '/maps/bridge.json')
    for name, m in maps(lose_first).items():
        json.dump(m, open(dst + '/maps/%s.json' % name, 'w'), indent=1)
    g = open(dst + '/game.json').read()
    g = g.replace('"name": "Story Tactics Demo"', '"name": "%s"' % title)
    g = g.replace('"roles": { "hp": "hp", "move": "mov" },',
                  '"roles": { "hp": "hp", "move": "mov" },\n  "switches": [ { "id": "met_wanderer", "name": "Met" } ],\n'
                  '  "variables": [ { "id": "chapters", "name": "Chapters" } ],')
    g = re.sub(r'"maps": \[.*?\]\s*\}\s*$', '''"maps": [ { "id": "bridge", "name": "The Ford", "file": "maps/bridge.json" },
    { "id": "bridge2", "name": "Across the River", "file": "maps/bridge2.json" },
    { "id": "keep", "name": "The Keep", "file": "maps/keep.json" },
    { "id": "skirmish", "name": "Skirmish", "file": "maps/skirmish.json" } ]
}''', g, flags=re.S)
    open(dst + '/game.json', 'w').write(g)
    gr = jsonc(dst + '/data/graphics.json')
    gr['portraits'] = {'eirik': {'sheet': 'units', 'cell': [0, 0]}}
    json.dump(gr, open(dst + '/data/graphics.json', 'w'), indent=1)
    json.dump(campaign(on_defeat, fallen, free), open(dst + '/data/campaign.json', 'w'), indent=1)




def make_games(root, folder_out):
    global bridge, src, repo, out
    repo = root
    out = folder_out
    src = os.path.join(repo, 'games/demo-srpg')
    bridge = jsonc(src + '/maps/bridge.json')
    os.makedirs(out, exist_ok=True)
    build('camp', 'Campaign Test', 'retry', 'gone', ['skirmish'], False)   # wins; retry; fallen gone; one free battle
    build('camp_end', 'Campaign Ends', 'end', 'return', 'none', True)      # loses chapter 1; the story ends; fallen return
    build('camp_lose', 'Campaign Retry', 'retry', 'gone', 'all', True)     # loses chapter 1; retry; every map free
    json.dump({'games': ['camp', 'camp_end', 'camp_lose']}, open(os.path.join(out, 'index.json'), 'w'))  # the web build has no folder listing


# ---- running the player

PLAYER = ''
GAMES = ''
SAVES = ''
failures = []
passed = 0


def play(args, saves=None, frames=1500, interval=8, inp=None):
    """Runs the player windowless; returns (exit code, log text)."""
    cmd = [PLAYER, '--games', GAMES, '--headless', '--no-settings', '--frames', str(frames), '--input-interval', str(interval)]
    cmd += ['--saves', saves] if saves else ['--no-saving']
    if inp:
        cmd += ['--input', inp]
    p = subprocess.run(cmd + args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True, timeout=300)
    return p.returncode, p.stdout


def check(name, cond, detail=''):
    global passed
    if cond:
        passed += 1
        print('PASS ' + name)
    else:
        failures.append(name)
        print('FAIL %s %s' % (name, detail))


def save_of(saves, game, slot='continue'):
    path = os.path.join(saves, game, slot + '.json')
    if not os.path.exists(path):
        return None
    return json.load(open(path))


def phase(save):
    return save['playthrough']['phase'] if save else None


# Input recipes (one token every `interval` frames): the title card is left with confirm, a scene is skipped with
# skip, a battle's first turn is ended with menu + confirm (every map here is decided at the start of round 2),
# the result banner is left with confirm.
CARD_AND_SKIP = 'confirm,skip,wait:20'
END_TURN = 'menu,confirm,wait:60'
CH1 = CARD_AND_SKIP + ',' + END_TURN + ',confirm,skip,wait:5'      # chapter 1 won, its after scene skipped
CH2 = CARD_AND_SKIP + ',' + END_TURN + ',confirm,wait:5'          # chapter 2 won, no after scene
CH3 = 'confirm,wait:20,' + END_TURN + ',confirm,skip,wait:10'     # chapter 3 (no before scene) won, after scene skipped


def run_all():
    tmp = tempfile.mkdtemp(prefix='ote_campaign_test_')
    global GAMES
    GAMES = os.path.join(tmp, 'games')
    try:
        make_games(repo, GAMES)
        scenarios(tmp)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def scenarios(tmp):
    def fresh(name):
        d = os.path.join(tmp, name)
        shutil.rmtree(d, ignore_errors=True)
        return d

    # 1. Autoplay plays the whole story, scenes dismiss themselves; nothing is saved.
    sv = fresh('s_auto')
    code, log = play(['--game', 'camp', '--campaign', '--autoplay'], saves=sv, frames=3000)
    check('autoplay: all three chapters are won', code == 0 and len(re.findall(r'chapter \d battle over \(won\)', log)) == 3, log[-400:])
    check('autoplay saves nothing', save_of(sv, 'camp') is None)

    # 2. A human New game: chapter 1 won -> the Continue save is the after scene's (phase after), roster carried.
    sv = fresh('s_new')
    code, log = play(['--game', 'camp', '--campaign'], saves=sv, inp=CARD_AND_SKIP + ',' + END_TURN)
    s = save_of(sv, 'camp')
    check('won battle: continue save is phase after of chapter 1', phase(s) == 'after' and s['chapter'] == 'ch1' and 'battle' not in s, log[-300:])
    roster = {r['id']: r for r in s['playthrough']['roster']} if s else {}
    check('won battle: heroes carried, the fallen one is gone', roster.get('eirik', {}).get('level') == 3 and roster.get('bram', {}).get('gone') is True)
    check('won battle: the recruit joined the roster', roster.get('wanderer', {}).get('level') == 5)

    # 3. Continue: the after scene, then chapter 2; a save is written after the scene (phase before), then in the battle.
    code, log = play(['--game', 'camp', '--continue'], saves=sv, inp='skip,wait:20,confirm,skip,wait:20,confirm,wait:30', frames=400)
    s = save_of(sv, 'camp')
    check('continue (after): scenes then chapter 2 battle, saved mid-battle', phase(s) == 'battle' and s['chapter'] == 'ch2' and s.get('round') == 1, log[-300:])
    check('chapter 2 starts with the carried party', s and s['battle']['sides'][0]['resources'] == [950])  # 700 + 100 + 150

    # 4. Save after a scene only: skip the after scene and stop -> phase before of chapter 2.
    sv4 = fresh('s_scene')
    play(['--game', 'camp', '--campaign'], saves=sv4, inp=CARD_AND_SKIP + ',' + END_TURN)
    code, log = play(['--game', 'camp', '--continue'], saves=sv4, inp='skip,wait:10', frames=120)
    s = save_of(sv4, 'camp')
    check('save after a scene: phase before of chapter 2', phase(s) == 'before' and s['chapter'] == 'ch2', log[-300:])
    code, log = play(['--game', 'camp', '--continue'], saves=sv4, inp='wait:10', frames=60)
    check('continue (before) goes back to the chapter\'s title card', "campaign, chapter Across the River, before" in log, log[-300:])

    # 5. Leave battle, then --continue picks the battle up; a slot save + --load-slot.
    sv5 = fresh('s_leave')
    shutil.copytree(sv, sv5)
    # (the battle menu has a "Unit" entry second since the demo has skills (D1), so each index below is one more than before)
    code, log = play(['--game', 'camp', '--continue'], saves=sv5, inp='wait:30,cancel,menu,down,down,confirm,confirm,wait:10,menu,down,down,down,down,confirm,wait:10', frames=400)
    s = save_of(sv5, 'camp')
    check('leave battle writes the continue save with the battle', phase(s) == 'battle' and 'battle' in s)
    s1 = save_of(sv5, 'camp', 'slot1')
    check('slot save mid-battle is a campaign save with the battle', phase(s1) == 'battle' and 'battle' in s1)
    code, log = play(['--game', 'camp', '--load-slot', '1'], saves=sv5, inp='wait:10', frames=60)
    check('--load-slot resumes the campaign battle', 'resumed slot 1' in log and 'campaign, chapter Across the River, battle' in log, log[-300:])

    # 6. Defeat with retry: the Continue save is "ready to fight" (no battle); Try again starts a new battle.
    sv6 = fresh('s_retry')
    code, log = play(['--game', 'camp_lose', '--campaign'], saves=sv6, inp=CARD_AND_SKIP + ',' + END_TURN)
    s = save_of(sv6, 'camp_lose')
    check('defeat (retry): continue save is the chapter ready to fight', phase(s) == 'battle' and 'battle' not in s, log[-300:])
    code, log = play(['--game', 'camp_lose', '--campaign'], saves=sv6, inp=CARD_AND_SKIP + ',' + END_TURN + ',confirm,wait:5,confirm,wait:30')
    check('defeat (retry): Try again starts the battle again, without the scene',
          log.count('battle over: DEFEAT') == 1 and log.count('saves:') == 2, log[-300:])
    code, log = play(['--game', 'camp_lose', '--continue'], saves=sv6, inp='wait:30', frames=120)
    check('defeat (retry): continue starts a new battle', 'chapter The Ford, battle' in log and log.count('saves:') == 1, log[-300:])

    # 7. Defeat with the story ending: no Continue save left.
    sv7 = fresh('s_end')
    code, log = play(['--game', 'camp_end', '--campaign'], saves=sv7, inp=CARD_AND_SKIP + ',' + END_TURN + ',confirm,wait:10')
    check('defeat (end): story ends and the continue save is deleted', 'lost, story ends' in log and save_of(sv7, 'camp_end') is None, log[-300:])

    # 8. The whole story by hand: after the last scene the continue save is gone; the end screen is shown.
    sv8 = fresh('s_whole')
    code, log = play(['--game', 'camp', '--campaign'], saves=sv8, inp=','.join([CH1, CH2, CH3]), frames=3000)
    check('the last chapter ends the story: continue save deleted', log.count('(won)') == 3 and save_of(sv8, 'camp') is None, log[-300:])

    # 9. Free battles never touch a playthrough: --map on a campaign game, even where freeBattles is "none".
    sv9 = fresh('s_free')
    shutil.copytree(sv, sv9)
    before = open(os.path.join(sv9, 'camp', 'continue.json')).read()
    code, log = play(['--game', 'camp', '--map', 'keep'], saves=sv9, inp='wait:30,cancel,menu,confirm,wait:60,confirm,wait:5', frames=400)
    after = open(os.path.join(sv9, 'camp', 'continue.json')).read()
    check('--map plays a free battle and leaves the continue save alone', code == 0 and 'battle over: VICTORY' in log and before == after, log[-300:])
    code, log = play(['--game', 'camp_end', '--map', 'bridge'], saves=fresh('s_free2'), inp='wait:5', frames=40)
    check('--map works where freeBattles is "none"', code == 0 and 'saves:' in log)

    # 10. Command line.
    code, log = play(['--game', 'camp', '--campaign', '--map', 'keep'], frames=10)
    check('--campaign with --map is refused', code != 0 and '--campaign' in log)
    code, log = play(['--campaign'], frames=10)
    check('--campaign needs --game', code != 0)
    code, log = play(['--game', 'demo-srpg', '--campaign', '--games', os.path.join(repo, 'games')], frames=40, inp='wait:5')
    check('--campaign on a game without a campaign shows an error, not a crash', code == 0, log[-200:])


def main():
    global PLAYER, repo
    if len(sys.argv) == 4 and sys.argv[1] == '--make-games':
        make_games(os.path.abspath(sys.argv[3]), os.path.abspath(sys.argv[2]))
        return 0
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    PLAYER, repo = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
    run_all()
    print('\n%d passed, %d failed' % (passed, len(failures)))
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
