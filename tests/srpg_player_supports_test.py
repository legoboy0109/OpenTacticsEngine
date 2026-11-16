#!/usr/bin/env python3
"""Supports and the camp through the real player, windowless (docs/SRPG.md step U5).

  python3 tests/srpg_player_supports_test.py ./build/engine/player/ote-player .
  python3 tests/srpg_player_supports_test.py --make-games OUT_DIR .     # only writes the test games

Builds campaign games as copies of games/demo-srpg in a temp folder (never in the repo) with four characters and
three support pairs, one game per `conversations` mode (battle, camp, both), and plays them with --input scripts and
--saves on a temp folder. Checks the log lines (the Support entry, the conversation, the banner, the camp) and the
save files. Pictures are checked by hand.
"""
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

repo = '.'
out = ''
PLAYER = ''
GAMES = ''
failures = []
passed = 0


def jsonc(path):
    text = open(path).read()
    text = '\n'.join(l for l in text.split('\n') if not l.strip().startswith('//'))
    return json.loads(text)


# ---- the games

CHARACTERS = {'characters': [
    {'id': 'eirik', 'name': 'Eirik', 'type': 'lord', 'level': 3},
    {'id': 'lyra', 'name': 'Lyra', 'type': 'archer', 'level': 2},
    {'id': 'kell', 'name': 'Kell', 'type': 'cavalier', 'level': 2},
    {'id': 'bram', 'name': 'Bram', 'type': 'fighter', 'level': 2}]}


def supports(mode):
    return {
        'rules': {'levelNames': ['C', 'B', 'A'], 'points': {'endTurnAdjacent': 5}, 'range': 3, 'conversations': mode},
        'pairs': [
            # ready from the start; its second level has no conversation (gained at once when the points allow)
            {'a': 'eirik', 'b': 'lyra', 'start': 20, 'levels': [
                {'points': 20, 'bonus': {'hit': 5, 'avoid': 5},
                 'talk': [{'say': 'You never miss.', 'speaker': 'eirik'},
                          {'say': 'I miss plenty. You just look away.', 'speaker': 'lyra'},
                          {'say': 'Then we are even.', 'speakerName': 'Narrator'}]},
                {'points': 60, 'bonus': {'hit': 10, 'avoid': 10}}]},
            # ready after blue ends its turn next to each other once (5 points)
            {'a': 'eirik', 'b': 'kell', 'start': 0, 'levels': [
                {'points': 5, 'bonus': {'avoid': 5},
                 'talk': [{'say': 'Ride ahead, I will hold the line.', 'speaker': 'kell'}]}]},
            # never ready: shows a bar on the Supports page
            {'a': 'lyra', 'b': 'bram', 'start': 7, 'levels': [
                {'points': 40, 'talk': [{'say': 'Mind the arrows.', 'speaker': 'bram'}]}]}]}


def U(char, side, x, y, **kw):
    u = {'character': char, 'side': side, 'x': x, 'y': y}
    u.update(kw)
    return u


def make_map(bridge, name, blue, red, events):
    m = {k: v for k, v in bridge.items()}
    m['name'] = name
    m['sides'] = [
        {'id': 'blue', 'name': 'Blue Company', 'color': '#3d85c6', 'controller': 'human', 'resources': {'gold': 0}, 'storage': []},
        {'id': 'red', 'name': 'Red Warband', 'color': '#cc0000', 'controller': 'ai'}]
    m['units'] = blue + red
    m['events'] = events
    m.pop('objectives', None)
    return m


def build(src, dst, title, mode, with_supports=True):
    if os.path.exists(dst):
        shutil.rmtree(dst)
    shutil.copytree(src, dst)
    bridge = jsonc(src + '/maps/bridge.json')
    os.remove(dst + '/maps/bridge.json')
    finish = [{'id': 'finish', 'when': {'turn': 2, 'side': 'blue'}, 'do': [{'win': 'blue'}]}]

    def squad(level):
        # eirik, lyra and kell stand side by side (Eirik in the middle); bram is a step away
        return [U('eirik', 'blue', 2, 4, leader=True, level=level), U('lyra', 'blue', 1, 4, level=level),
                U('kell', 'blue', 3, 4, level=level), U('bram', 'blue', 1, 6, level=level)]

    foes = [{'type': 'knight', 'side': 'red', 'x': 11, 'y': 4, 'name': 'Gorm', 'level': 2, 'leader': True}]
    for i, n in enumerate(['ch1', 'ch2', 'ch3']):
        m = make_map(bridge, 'Chapter %d' % (i + 1), squad(3), foes, finish)
        json.dump(m, open(dst + '/maps/%s.json' % n, 'w'), indent=1)
    g = open(dst + '/game.json').read()
    g = g.replace('"name": "Story Tactics Demo"', '"name": "%s"' % title)
    g = re.sub(r'"maps": \[.*?\]\s*\}\s*$', '''"maps": [ { "id": "ch1", "name": "Chapter 1", "file": "maps/ch1.json" },
    { "id": "ch2", "name": "Chapter 2", "file": "maps/ch2.json" },
    { "id": "ch3", "name": "Chapter 3", "file": "maps/ch3.json" } ]
}''', g, flags=re.S)
    open(dst + '/game.json', 'w').write(g)
    json.dump(CHARACTERS, open(dst + '/data/characters.json', 'w'), indent=1)
    if with_supports:
        json.dump(supports(mode), open(dst + '/data/supports.json', 'w'), indent=1)
    camp = {'name': 'Camp Test', 'rules': {'fallen': 'gone', 'onDefeat': 'retry'}, 'freeBattles': 'none',
            'chapters': [
                {'id': 'ch1', 'name': 'The First Road', 'map': 'ch1', 'before': [{'say': 'We march at dawn.', 'speakerName': 'Narrator'}], 'after': []},
                {'id': 'ch2', 'name': 'The Second Road', 'map': 'ch2', 'before': [{'say': 'A new day.', 'speakerName': 'Narrator'}], 'after': []},
                {'id': 'ch3', 'name': 'The Last Road', 'map': 'ch3', 'before': [], 'after': []}]}
    json.dump(camp, open(dst + '/data/campaign.json', 'w'), indent=1)


def make_games(root, folder_out):
    global repo, out
    repo, out = root, folder_out
    src = os.path.join(repo, 'games/demo-srpg')
    os.makedirs(out, exist_ok=True)
    build(src, out + '/sup_battle', 'Supports In Battle', 'battle')
    build(src, out + '/sup_camp', 'Supports At Camp', 'camp')
    build(src, out + '/sup_both', 'Supports Both', 'both')
    build(src, out + '/sup_none', 'Supports None', 'both', with_supports=False)


# ---- running the player

def play(args, saves=None, frames=1500, interval=8, inp=None, extra=None):
    """Runs the player windowless; returns (exit code, log text)."""
    cmd = [PLAYER, '--games', GAMES, '--headless', '--no-audio', '--no-settings', '--frames', str(frames),
           '--input-interval', str(interval), '--fixed-dt', '0.0167']
    cmd += ['--saves', saves] if saves else ['--no-saving']
    if inp:
        cmd += ['--input', inp]
    p = subprocess.run(cmd + args + (extra or []), stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                       universal_newlines=True, timeout=300)
    return p.returncode, p.stdout


def check(name, cond, detail=''):
    global passed
    if cond:
        passed += 1
        print('PASS ' + name)
    else:
        failures.append(name)
        print('FAIL %s %s' % (name, detail))


def save_of(saves, game):
    path = os.path.join(saves, game, 'continue.json')
    return json.load(open(path)) if os.path.exists(path) else None


def supports_of(save):
    return save['playthrough'].get('supports', {}) if save else {}


# Input recipes. The title card is left with confirm; the cursor starts on the first of blue's units (Eirik).
START = 'confirm,confirm,confirm,wait:12'          # the chapter-1 card, its one before line (two presses), then the map
TALK = 'confirm,wait:3,confirm,wait:3,confirm,wait:3,skip,wait:15,up,confirm,wait:6'  # select Eirik, stay, first entry (Support), skip the lines, then Wait (the last entry)
END_TURN = 'menu,confirm,wait:60'                  # blue ends its turn; every map is decided at the start of round 2
PEEK = 'confirm,wait:3,confirm,wait:3,cancel,wait:3,cancel,wait:3'  # open Eirik's action menu and close it again
WON = ',confirm,wait:10'                           # leave the result banner (the after scene is empty)


def run_all():
    tmp = tempfile.mkdtemp(prefix='ote_supports_test_')
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


    # 1. conversations "battle": the Support entry is in the action menu, the conversation is played on the map, the
    #    level is gained (banner), the camp never appears, and the next chapter's save carries the level.
    sv = fresh('s_battle')
    code, log = play(['--game', 'sup_battle', '--campaign'], saves=sv, inp=START + ',' + TALK + ',' + END_TURN + WON, frames=1500)
    check('battle: the action menu offers "Support: Lyra" (and only her)', log.count('support entries: Support: Lyra\n') == 1, log[-600:])
    check('battle: choosing it plays the conversation and gains level C',
          'support: Eirik talks with Lyra' in log and 'support level: Eirik and Lyra: support level C' in log, log[-600:])
    check('battle: a pair that became ready is announced', 'support ready: Eirik and Kell' in log, log[-600:])
    check('battle: no camp between the chapters', 'camp:' not in log and 'chapter 1 battle over (won)' in log, log[-600:])
    s = save_of(sv, 'sup_battle')
    sp = supports_of(s)
    check('battle: the saved playthrough holds the new level', s and s['playthrough']['phase'] == 'before' and sp.get('levels') == [1, 0, 0], str(s and s['playthrough'])[:300])
    check('battle: the points were earned too (20 start +5 for ending the turn side by side)', sp.get('points', [0])[0] == 25, str(sp))

    # 2. conversations "camp": no entry in battle, the camp after the won chapter, a conversation played in it is saved,
    #    the list comes back without it, "Continue the story" goes on, and an empty camp is skipped.
    sv = fresh('s_camp')
    code, log = play(['--game', 'sup_camp', '--campaign'], saves=sv, inp=START + ',' + PEEK + ',' + END_TURN + WON, frames=1500)
    check('camp: no Support entry in battle (conversations are for the camp)', 'support entries: none' in log and 'support: Eirik' not in log, log[-500:])
    check('camp: the camp lists both waiting conversations',
          'camp: Eirik and Lyra (Level C), Eirik and Kell (Level C)' in log, log[-500:])
    s = save_of(sv, 'sup_camp')
    check('camp: nothing is gained before it is played', supports_of(s).get('levels', [0, 0, 0]) == [0, 0, 0] and s['playthrough']['phase'] == 'before')
    # The first row (Eirik and Lyra): confirm, skip the lines; the camp is saved and comes back with Kell only.
    sv2 = fresh('s_camp2')
    shutil.copytree(sv, sv2)
    code, log = play(['--game', 'sup_camp', '--continue'], saves=sv2, inp='confirm,wait:5,skip,wait:20', frames=900)
    check('camp (continue): the camp comes back after loading the save', 'camp: Eirik and Lyra (Level C), Eirik and Kell (Level C)' in log, log[-500:])
    check('camp: choosing a pair plays it, and the list returns without it',
          'camp: playing Eirik and Lyra' in log and 'camp: Eirik and Kell (Level C)\n' in log, log[-500:])
    s = save_of(sv2, 'sup_camp')
    check('camp: the playthrough is saved after the conversation', supports_of(s).get('levels') == [1, 0, 0] and s['playthrough']['phase'] == 'before', str(s and s['playthrough'])[:300])
    code, log = play(['--game', 'sup_camp', '--continue'], saves=sv2, inp='confirm,wait:5,skip,wait:20', frames=900)
    check('camp: with nothing more pending the camp is skipped (the title card follows)',
          'camp: playing Eirik and Kell' in log and log.count('camp: ') == 2 and 'campaign, chapter The Second Road, before' in log, log[-500:])
    s = save_of(sv2, 'sup_camp')
    check('camp: both conversations are in the save', supports_of(s).get('levels') == [1, 1, 0], str(supports_of(s)))
    # "Continue the story" leaves the rest for the next camp.
    sv3 = fresh('s_camp3')
    shutil.copytree(sv, sv3)
    code, log = play(['--game', 'sup_camp', '--continue'], saves=sv3, inp='down,down,confirm,wait:30', frames=600)
    check('camp: "Continue the story" goes on without playing anything', log.count('camp: ') == 1 and 'playing' not in log, log[-500:])
    s = save_of(sv3, 'sup_camp')
    check('camp: what was not played is still waiting', supports_of(s).get('levels', [0, 0, 0]) == [0, 0, 0])

    # 3. conversations "both": played on the map for Lyra, the other waits in the camp.
    sv = fresh('s_both')
    code, log = play(['--game', 'sup_both', '--campaign'], saves=sv, inp=START + ',' + TALK + ',' + END_TURN + WON, frames=1500)
    check('both: the entry in battle and then a camp with the pair that is left',
          'support entries: Support: Lyra' in log and 'camp: Eirik and Kell (Level C)\n' in log and 'Lyra (Level' not in log.split('camp:')[-1], log[-500:])

    # 4. --autoplay never stops at the camp; a game without supports shows none.
    sv = fresh('s_auto')
    code, log = play(['--game', 'sup_camp', '--campaign', '--autoplay'], saves=sv, frames=3000)
    check('autoplay: passes through the camp and plays every chapter', code == 0 and 'camp:' not in log and log.count('(won)') == 3, log[-400:])
    code, log = play(['--game', 'sup_none', '--campaign'], saves=fresh('s_none'), inp=START + ',' + END_TURN + WON, frames=900)
    check('no supports file: no camp', 'camp:' not in log and 'chapter 1 battle over (won)' in log, log[-400:])


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
