#!/usr/bin/env python3
"""The player's part of the four small additions (docs/EVENTS.md "Branching on who acted" and "Sending a unit's items to storage", docs/SRPG.md
"Effects that fade" and "Linking two strike blocks"), windowless.

  python3 tests/srpg_player_extras_test.py ./build/engine/player/ote-player .

Copies tests/fixtures/srpg-skillblocks2 (a rogue "thief" next to a guard "mark", the rogue "pal" behind) into a temp folder, changes a few files, runs
the player headless (--no-audio --no-settings, --saves on a temp folder) with --input scripts and judges the [info] lines it logs, the forecast it
draws and the Continue save it leaves:

  - Actor: a fight event branches on who attacked: the unit it names gets the "then" part (a line of dialogue, a variable, and the items of
    another unit sent to the shared storage), any other the "else" part.
  - Storage: the items leave the unit's pack and are in the side's storage in the save.
  - Fading: a condition "might" (4 turns, strength +1 per turn left, put on by a battle-start event) shows in the forecast as +3, and after a round
    of turns the saved count is 2 (strength +2).
  - Marks: a skill whose first strike (with a chance) leaves a mark and whose second strike asks for it: one roll switches on both (the log says
    the skill was activated once, the guard loses both strikes' damage); a roll that fails switches on neither.
"""
import json
import os
import shutil
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import (  # noqa: E402
    run_text, check, edit, failures, inp, lines, player_and_repo, player_cmd, read_json, summary, ticks, write_json)
from ote_testkit import scratch_dir  # noqa: E402

sys.dont_write_bytecode = True

PLAYER, REPO = player_and_repo()
TMP = scratch_dir()
IDLE = 'wait:14'  # the turn banner plays first; it takes the first inputs otherwise


def make_game(gid, tweak, width=480, height=270):
    """A copy of the fixture with this view size; `tweak(game_dir)` changes its files."""
    g = os.path.join(TMP, gid)
    shutil.copytree(os.path.join(REPO, 'tests/fixtures/srpg-skillblocks2'), g)
    game = read_json(g + '/game.json')
    game['view'] = {'width': width, 'height': height, 'tileSize': 24}
    write_json(g + '/game.json', game)
    if tweak:
        tweak(g)
    return g


def run(gid, inp, map_id='field', interval=4):
    """Runs the player on a fresh battle; returns (log, the Continue save's units by story id, its battle)."""
    work = os.path.join(TMP, 'saves_%s_%s' % (gid, map_id))
    shutil.rmtree(work, ignore_errors=True)
    cmd = player_cmd(PLAYER, TMP, REPO) + ['--saves', work, '--frames', str(ticks(inp) * interval + 90), '--input-interval', str(interval),
           '--fixed-dt', '0.016', '--game', gid, '--map', map_id, '--input', inp]
    p = run_text(cmd)
    battle, units = {}, {}
    try:
        battle = read_json(os.path.join(work, gid, 'continue.json'))['battle']
        for u in battle['units']:
            units[u.get('id') or u['name']] = u
    except (OSError, ValueError, KeyError):
        pass
    return p.stdout, units, battle


# The thief (the first unit) is under the cursor: confirm selects it, confirm again opens its action menu; Attack is the first entry.
OPEN = [IDLE, 'confirm', 'wait:3', 'confirm', 'wait:3']
FIGHT = inp(*OPEN, 'confirm', 'wait:3', 'confirm', 'wait:3')  # Attack, the foe: the forecast shows
GO = 'confirm'


def story_variable(battle, index=0):
    return battle.get('story', {}).get('variables', [None])[index]


def items_of(unit):
    return [i['item'] if isinstance(i, dict) else i for i in unit.get('items', [])]


# ---- 1 and 2: the actor, and storeItems ----

def actor_game(actor):
    def tweak(g):
        def game(d):
            d['rules']['items']['storage'] = 'anywhere'
            d['variables'] = [{'id': 'who', 'name': 'Who'}]
        edit(g, 'game.json', game)
        def field(m):
            m['events'] = [{'id': 'first', 'when': {'fight': {'side': 'blue'}}, 'do': [
                {'if': [{'actor': {'unit': actor}}],
                 'then': [{'say': 'The one we hoped for struck.', 'speakerName': 'Narrator'}, {'setVariable': 'who', 'to': 1},
                          {'storeItems': 'pal'}],
                 'else': [{'say': 'Somebody else struck.', 'speakerName': 'Narrator'}, {'setVariable': 'who', 'to': 2}]}]}]
        edit(g, 'maps/field.json', field)
    return tweak


def test_actor(tag, w, h):
    gid = 'actor_' + tag
    make_game(gid, actor_game('thief'), w, h)
    log, units, battle = run(gid, inp(FIGHT, GO, 'wait:30', 'confirm', 'wait:40'))
    check('%s actor: the named unit started the fight: its line is said' % tag, bool(lines(log, 'dialogue: Narrator | The one we hoped for struck.')), log[-500:])
    check('%s actor: the other branch is not said' % tag, not lines(log, 'Somebody else struck.'))
    check('%s actor: the "then" part ran (the variable is 1)' % tag, story_variable(battle) == 1, str(battle.get('story')))
    check('%s storage: pal\'s items left its pack' % tag, items_of(units.get('pal', {'items': ['?']})) == [], str(units.get('pal')))
    storage = (battle.get('sides') or [{}])[0].get('storage', [])
    check('%s storage: and are in the side\'s storage (the Continue save)' % tag, items_of({'items': storage}) == [0], str(battle.get('sides')))  # item 0 = the iron sword

    gid = 'actor_other_' + tag
    make_game(gid, actor_game('pal'), w, h)
    log, units, battle = run(gid, inp(FIGHT, GO, 'wait:30', 'confirm', 'wait:40'))
    check('%s actor: another unit attacked: the "else" part is said' % tag, bool(lines(log, 'dialogue: Narrator | Somebody else struck.')), log[-500:])
    check('%s actor: the "then" part did not run (variable 2, pal keeps its items)' % tag, story_variable(battle) == 2 and items_of(units.get('pal', {})) == [0], str(units.get('pal')) + str(battle.get('story')))


# ---- 3: a condition that fades ----

def fading_game(g):
    def conditions(d):
        d['conditions'].append({'id': 'might', 'name': 'Might', 'tag': 'Mi', 'turns': 4, 'scale': 'turnsLeft',
                                'blocks': [{'when': 'always', 'do': [{'stats': {'str': 1}}]}]})
    edit(g, 'data/conditions.json', conditions)
    def field(m):
        m['events'] = [{'id': 'bless', 'when': {'battle': 'start'}, 'do': [{'addCondition': 'might', 'to': 'thief'}]}]
    edit(g, 'maps/field.json', field)


def test_fading(tag, w, h):
    gid = 'fade_' + tag
    make_game(gid, fading_game, w, h)
    log, units, battle = run(gid, inp(FIGHT))
    fc = lines(log, 'forecast:')
    # Strength 5 + 3 turns left (the battle's first turn is counted at once) + the sword 3 - the guard's defence 4.
    check('%s fading: the forecast counts the turns left (damage 4 + 3 = 7)' % tag, any('Dmg 7' in f for f in fc), str(fc[-2:]))
    check('%s fading: the condition was put on' % tag, bool(lines(log, 'condition applied: Rogue - Might')), log[-400:])
    # One round later (End turn, the computer plays by itself) one turn less: +2. (The battle-start event's float delays the first input.)
    log, units, battle = run(gid, inp('wait:40', 'menu', 'wait:6', 'confirm', 'wait:300'))
    left = [c.get('left') for c in battle.get('units', [{}])[0].get('conditions', [])] if battle else []
    check('%s fading: a round later the count is saved one lower (2 left, round 2)' % tag, left == [2] and battle.get('round') == 2, str((left, battle.get('round'))))
    check('%s fading: the condition is still on and has not ended' % tag, not lines(log, 'condition ended'), str(lines(log, 'condition ended')))


# ---- 4: a link between two strike blocks ----

def link_game(chance):
    def tweak(g):
        edit(g, 'game.json', lambda d: d['formulas'].update({'strikes': '2'}))
        def skills(d):
            d['skills'].append({'id': 'link', 'name': 'Link', 'blocks': [
                {'when': 'strike', 'if': [{'strike': 'first'}], 'chance': chance, 'do': [{'damage': 3}, {'mark': 'hit'}]},
                {'when': 'strike', 'if': [{'strike': 2}, {'marked': 'hit'}], 'do': [{'damage': 5}]}]})
        edit(g, 'data/skills.json', skills)
        edit(g, 'data/units.json', lambda d: d['units'][0].update({'skills': ['link']}))
    return tweak


def test_marks(tag, w, h):
    gid = 'link_on_' + tag
    make_game(gid, link_game(100), w, h)
    log, units, battle = run(gid, inp(FIGHT))
    fc = lines(log, 'forecast:')
    check('%s marks: the forecast shows the first block\'s chance, not the linked block' % tag, any('Link 100%' in f and 'Dmg 4' in f for f in fc), str(fc[-2:]))
    log, units, battle = run(gid, inp(FIGHT, GO, 'wait:60'))
    check('%s marks: one roll: the skill is activated once' % tag, len(lines(log, 'skill activated: Rogue - Link')) == 1, str(lines(log, 'skill activated')))
    # Two strikes: 4 + 3 (the first, activated and marked) and 4 + 5 (the second, because it was marked): 16 in all.
    check('%s marks: both strikes did their extra damage (the guard lost 7 + 9)' % tag, units.get('mark', {}).get('hp') == 30 - 7 - 9, str(units.get('mark')))
    gid = 'link_off_' + tag
    make_game(gid, link_game(0), w, h)
    log, units, battle = run(gid, inp(FIGHT, GO, 'wait:60'))
    check('%s marks: a roll that fails switches on neither (the guard lost 4 + 4)' % tag, units.get('mark', {}).get('hp') == 30 - 4 - 4 and not lines(log, 'skill activated: Rogue - Link'), str(units.get('mark')) + str(lines(log, 'skill activated')))


def main():
    try:
        for tag, w, h in (('desktop', 480, 270), ('phone', 270, 480)):
            test_actor(tag, w, h)
            test_fading(tag, w, h)
            test_marks(tag, w, h)
    finally:
        shutil.rmtree(TMP, ignore_errors=True)
    summary()
    for f in failures:
        print('  FAILED: ' + f)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
