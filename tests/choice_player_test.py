#!/usr/bin/env python3
"""Dialogue choices through the real player, windowless (docs/EVENTS.md "Dialogue choices").

  python3 tests/choice_player_test.py ./build/engine/player/ote-player .

Plays tests/fixtures/choice-story (copied to a temp folder, never changed) with --input scripts: a question in a
campaign scene and in a map event (at the battle's start and in the middle of an action), answered by keyboard and by
pointer (a tap highlights, a second tap picks), skipping that stops at a question, a nested question, a hidden answer,
a branch to another chapter, the choice in the Continue save, a question on screen that is never saved (Continue asks
it again) and --autoplay taking the first answers. The player logs "choice: ..." lines (the rows' positions, the
answers); the script reads them. Pictures are checked by hand (see docs/EVENTS.md).
"""
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

PLAYER = ''
repo = '.'
GAMES = ''
failures = []
passed = 0
INTERVAL = 3  # frames per token: a line has more letters than that is typed in one token (60 letters a second)


def C(n=1):
    """n Confirm presses."""
    return ','.join(['confirm'] * n)


def play(args, saves=None, frames=3000, inp=None, game='choice-story'):
    cmd = [PLAYER, '--games', GAMES, '--game', game, '--headless', '--no-audio', '--no-settings',
           '--frames', str(frames), '--input-interval', str(INTERVAL)]
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


def rows(log, n=-1):
    """The row centres of the n-th (default: last) "choice: ... N answers: 0@x,y ..." log line."""
    found = re.findall(r'answers:((?: \d+@\d+,\d+)+)', log)
    if not found:
        return []
    return [tuple(int(v) for v in re.match(r'\d+@(\d+),(\d+)', r).groups()) for r in found[n].split()]


def tap(pos):
    return 'tap:%d:%d' % pos


def answers(log):
    return [int(a) for a in re.findall(r'choice: (?:scene|battle) answer (\d+)', log)]


def save_of(saves, slot='continue'):
    path = os.path.join(saves, 'choice-story', slot + '.json')
    return json.load(open(path)) if os.path.exists(path) else None


def variables(save):
    """The story variables of a saved battle, by id."""
    ids = ['stranger', 'gate', 'pack', 'road', 'mood']
    v = save['battle']['story']['variables']
    return dict(zip(ids, v))


# ---- the script of the story, token by token --------------------------------------------------------------
# Every line takes two Confirms (show it whole, end it); a question takes two as well (read it, the answers
# come up) and then the pick.
CARD = C(1)
BEFORE_Q = CARD + ',' + C(2) + ',' + C(2)            # chapter 1's card, the narrator, the question (answers up)
AFTER_PICK = C(2) + ',' + C(2)                       # "The rope it is." and "Then we move."
BATTLE_Q = C(2) + ',' + C(2)                         # "The ridge is quiet." and the stranger's question (answers up)
END_TURN = 'wait:20,menu,confirm,wait:60'            # the only turn of the battle; round 2's event wins it
VICTORY = C(1)


def fresh(tmp, name):
    d = os.path.join(tmp, name)
    shutil.rmtree(d, ignore_errors=True)
    return d


def scenarios(tmp):
    # 1. Keyboard: a question in the scene (second answer), then the battle's start question (first answer).
    sv = fresh(tmp, 'sv_keys')
    inp = BEFORE_Q + ',down,confirm,' + AFTER_PICK + ',' + BATTLE_Q + ',confirm,' + C(2) + ',wait:30'
    code, log = play(['--campaign'], saves=sv, inp=inp, frames=900)
    check('scene question: asked with its two answers', 'scene question "Which pack do we carry?"' in log and len(rows(log, 0)) == 2, log[-500:])
    check('keyboard: the scene and the battle questions are answered', answers(log) == [1, 0], str(answers(log)) + log[-300:])
    s = save_of(sv)
    check('the answers are in the Continue save of the battle (a save made after a choice)',
          s and 'battle' in s and variables(s)['stranger'] == 1 and variables(s)['pack'] == 2 and s['battle']['story']['switches'][0] == 1,
          json.dumps(s)[:300] if s else 'no save')

    # 2. Pointer: a first tap highlights, a second picks. Scene question first (rows from the first run).
    pos = rows(log, 0)
    sv = fresh(tmp, 'sv_tap_scene')
    inp = BEFORE_Q + ',' + tap(pos[1]) + ',wait:5'
    code, log2 = play(['--campaign'], saves=sv, inp=inp, frames=400)
    check('pointer: one tap only highlights', answers(log2) == [], log2[-300:])
    inp = BEFORE_Q + ',' + tap(pos[1]) + ',' + tap(pos[1]) + ',wait:5'
    code, log2 = play(['--campaign'], saves=sv, inp=inp, frames=400)
    check('pointer: the second tap on the highlighted answer picks it', answers(log2) == [1], log2[-300:])
    inp = BEFORE_Q + ',' + tap((pos[1][0], 5)) + ',wait:5'  # a tap outside the answers does nothing
    code, log2 = play(['--campaign'], saves=fresh(tmp, 'sv_tap_out'), inp=inp, frames=400)
    check('pointer: a tap away from the answers does not pick', answers(log2) == [], log2[-300:])
    # ... and in a battle question (rows from the first run's second question)
    bpos = rows(log, 1)
    sv = fresh(tmp, 'sv_tap_battle')
    inp = BEFORE_Q + ',confirm,' + AFTER_PICK + ',' + BATTLE_Q + ',' + tap(bpos[1]) + ',' + tap(bpos[1]) + ',' + C(2) + ',wait:30'
    code, log2 = play(['--campaign'], saves=sv, inp=inp, frames=900)
    s = save_of(sv)
    check('pointer: a battle question is answered by two taps', answers(log2) == [0, 1] and s and 'battle' in s and variables(s)['stranger'] == 2,
          str(answers(log2)) + (json.dumps(s)[:200] if s else ' no save'))
    check('pointer: the answer ran its own commands (the second answer sets no switch)', s and s['battle']['story']['switches'][0] == 0)

    # 3. Skipping stops at a question; Cancel / Menu / Skip never answer it.
    sv = fresh(tmp, 'sv_skip')
    inp = CARD + ',skip,wait:5,skip,cancel,menu,wait:5,down,confirm,wait:5'
    code, log2 = play(['--campaign'], saves=sv, inp=inp, frames=400)
    check('skip stops at the scene question (it is asked, and skipping it again does not answer it)',
          'scene question "Which pack do we carry?"' in log2 and answers(log2) == [1], log2[-400:])
    inp = BEFORE_Q + ',confirm,' + AFTER_PICK + ',skip,wait:5,skip,wait:5,down,confirm,wait:5'
    code, log2 = play(['--campaign'], saves=fresh(tmp, 'sv_skip2'), inp=inp, frames=600)
    check('skip stops at the battle question and the question is then answered by hand',
          'battle question "A stranger asks to cross. Let them?"' in log2 and answers(log2) == [0, 1], log2[-400:])

    # 4. A question that waits is never saved: the Continue save is the scene's, and continuing asks it again.
    sv = fresh(tmp, 'sv_wait')
    inp = BEFORE_Q + ',confirm,' + AFTER_PICK + ',' + BATTLE_Q + ',wait:40'  # stops with the battle question up
    code, log2 = play(['--campaign'], saves=sv, inp=inp, frames=600)
    s = save_of(sv)
    check('a question on screen is not saved: the Continue save has no battle yet',
          s and s['playthrough']['phase'] == 'battle' and 'battle' not in s, json.dumps(s)[:300] if s else 'no save')
    check('the scene answer was kept in the playthrough', s and s['playthrough']['variables'][2] == 1)
    code, log3 = play(['--continue'], saves=sv, inp=BATTLE_Q + ',wait:10', frames=300)
    check('continuing from there asks the battle question again',
          'battle question "A stranger asks to cross. Let them?"' in log3 and answers(log3) == [], log3[-300:])

    # 5. A question in the middle of an action, a hidden answer's absence, a nested question, then the save.
    sv = fresh(tmp, 'sv_gate')
    walk = 'confirm,right,right,confirm,wait:10'  # select Mara, move her two squares: she stands on the gate square
    inp = BEFORE_Q + ',confirm,' + AFTER_PICK + ',' + BATTLE_Q + ',confirm,' + C(2) + ',wait:20,' + walk
    code, log2 = play(['--campaign'], saves=sv, inp=inp, frames=900)
    check('a question during the battle comes up when Mara steps on the gate square',
          'battle question "An old gate blocks the path. What now?"' in log2 and len(rows(log2)) == 3, log2[-500:])
    gate = rows(log2)
    inp += ',' + C(2) + ',down,down,confirm,' + C(2) + ',confirm,' + C(2) + ',wait:30'  # the nested question: "Yes"
    # (the question's text is read first: two Confirms; "Ask Tobin first" is the third answer)
    code, log2 = play(['--campaign'], saves=sv, inp=inp, frames=1200)
    check('the nested question comes after the third answer and each answer is logged', answers(log2) == [0, 0, 2, 0],
          str(answers(log2)) + log2[-300:])
    s = save_of(sv)
    check('the saved battle holds both choices', s and 'battle' in s and variables(s)['gate'] == 3 and variables(s)['mood'] == 1,
          json.dumps(variables(s)) if s and 'battle' in s else 'no battle in the save')

    # 7. The whole story by keyboard: the summit road, then ch4's scenes read the earlier answers.
    sv = fresh(tmp, 'sv_whole')
    ch1 = (BEFORE_Q + ',down,confirm,' + AFTER_PICK + ',' + BATTLE_Q + ',confirm,' + C(2) + ',' + END_TURN + ',' + VICTORY +
           ',' + C(2) + ',' + C(2) + ',down,confirm,' + C(2))              # after scene: the road question, "summit path", "Up we go."
    ch3 = CARD + ',' + C(2) + ',' + END_TURN + ',' + VICTORY               # ch3 (summit): card, its scene line, the battle
    ch4 = CARD + ',' + C(2) + ',' + C(2) + ',' + END_TURN + ',' + VICTORY + ',' + C(2) + ',confirm,' + C(2)  # ch4: two lines, battle, Stay
    code, log2 = play(['--campaign'], saves=sv, inp=ch1 + ',' + ch3 + ',' + ch4 + ',wait:60', frames=4000)
    check('the summit road: chapter 3 (The Summit) is played, never chapter 2, then chapter 4 (Home)',
          'chapter 3 battle over (won)' in log2 and 'chapter 2 battle' not in log2 and 'chapter 4 battle over (won)' in log2, log2[-600:])
    check('the story ends after the last answer', log2.count('(won)') == 3 and save_of(sv) is None, log2[-400:])

    # 8. --autoplay takes the first answers by itself and plays the whole story.
    sv = fresh(tmp, 'sv_auto')
    code, log2 = play(['--campaign', '--autoplay'], saves=sv, frames=6000)
    check('autoplay: first answers taken (the valley road: chapters 1, 2 and 4), nobody is asked',
          code == 0 and 'chapter 2 battle over (won)' in log2 and 'chapter 3 battle' not in log2 and 'chapter 4 battle over (won)' in log2
          and 'choice:' not in log2 and 'a question waits' not in log2, log2[-400:])

    # 9. A free battle (--map): the start question is asked there too, and nothing campaign-ish is written.
    sv = fresh(tmp, 'sv_free')
    code, log2 = play(['--map', 'ridge'], saves=sv, inp=BATTLE_Q.replace(C(2) + ',' + C(2), C(2) + ',' + C(2)) + ',down,confirm,wait:30', frames=400)
    check('--map: the map event asks and the answer is taken', answers(log2) == [1], log2[-300:])
    check('--map: a free battle of a campaign game leaves no Continue save (as before)', save_of(sv) is None)


def main():
    global PLAYER, repo, GAMES
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    PLAYER, repo = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
    tmp = tempfile.mkdtemp(prefix='ote_choice_test_')
    GAMES = os.path.join(tmp, 'games')
    try:
        os.makedirs(GAMES)
        shutil.copytree(os.path.join(repo, 'tests/fixtures/choice-story'), os.path.join(GAMES, 'choice-story'))
        scenarios(tmp)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    print('\n%d passed, %d failed' % (passed, len(failures)))
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
