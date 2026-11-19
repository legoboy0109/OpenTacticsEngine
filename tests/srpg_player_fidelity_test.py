#!/usr/bin/env python3
"""The fidelity options through the real player, windowless (docs/SRPG.md section 11, docs/EVENTS.md).

  python3 tests/srpg_player_fidelity_test.py ./build/engine/player/ote-player .

Writes a small game into a temp folder (the repository is never changed) and plays it with --autoplay, --headless,
--no-audio, --no-settings and --no-saving. Asserted from the player's log lines:
  * stat caps (data/caps.json with computerCaps) load: the level-up box says "Attack: max" in the control game,
  * the average-of-two hit roll loads and plays (rules.hitRoll "average2") to the end of the battle,
  * level cap per unit type (maxLevel 0) and computerGains false: no "level-up box" in the game that sets them,
    while the same game without those keys does level up (the control),
  * a unit hidden by `removeUnit` + `hide` is neither alive nor dead for the story, still lives for rosterAlive / inRoster,
    and comes back with `showUnit` ("dialogue:" lines in the order the events say them),
  * roster conditions in a free battle read the map.
Items that need a person at the menu (an ability's experience and condition, a staff's xpMiss) and the numbers of the hit roll and
of `side` / `team` / `human` in formulas leave no line in the player's log; the core tests (tests/test_fidelity_*.cpp) assert them.
"""
import json
import os
import re
import shutil
import sys
import tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import run_text, check, failures, summary  # noqa: E402

PLAYER = ''
repo = '.'

STATS = [{"id": "hp", "name": "HP"}, {"id": "atk", "name": "Attack"}, {"id": "def", "name": "Defense"}, {"id": "mov", "name": "Move"}]


def write(root, rel, data):
    path = os.path.join(root, rel)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w') as f:
        json.dump(data, f)


def make_game(root, options):
    """options: with_keys (the fidelity keys on or off)."""
    keys = options['with_keys']
    rules = {"defeat": ["noUnits"], "counterAttacks": True,
             "progression": {"enabled": True, "xpPerLevel": 100, "maxLevel": 20, "xpHit": "60", "xpKill": "100"}}
    if keys:
        rules["hitRoll"] = "average2"
        rules["progression"]["computerGains"] = False
        rules["progression"]["xpNoDamage"] = 3
    write(root, 'game.json', {
        "formatVersion": 1, "name": "Fidelity Player Test", "view": {"width": 480, "height": 270, "tileSize": 24},
        "stats": STATS, "roles": {"hp": "hp", "move": "mov"}, "moveTypes": [{"id": "foot", "name": "Foot"}],
        "rules": rules,
        # the blue (human) side hits for one more: formulas see `a.human`
        "formulas": {"damage": "a.atk - b.def + ((a.human and 1) or 0)", "hit": "100"},
        "maps": [{"id": "field", "name": "Field", "file": "maps/field.json"}]})
    write(root, 'data/terrain.json', {"terrain": [{"id": "plain", "name": "Plain"}]})
    write(root, 'data/weapons.json', {"weapons": [{"id": "sword", "name": "Sword"}]})
    hero = {"id": "hero", "name": "Hero", "glyph": "H", "moveType": "foot", "weapons": ["sword"],
            "stats": {"hp": 24, "atk": 5, "def": 1, "mov": 3}, "growths": {"hp": 100, "atk": 100}}
    if keys:
        hero["maxLevel"] = 0  # a unit that never levels
    brute = {"id": "brute", "name": "Brute", "glyph": "B", "moveType": "foot", "weapons": ["sword"],
             "stats": {"hp": 30, "atk": 4, "def": 1, "mov": 3}, "growths": {"hp": 100}}
    write(root, 'data/units.json', {"units": [hero, brute]})
    # the stat cap shows in the level-up box ("Attack: max") once a unit levels into it; the computer's own cap is looser
    write(root, 'data/caps.json', {"caps": {"atk": 6}, "computerCaps": {"atk": 7}})
    events = [
        {"id": "hide", "when": {"battle": "start"}, "do": [{"removeUnit": {"unit": "lyra"}, "hide": True}]},
        {"id": "probe", "when": {"battle": "start"}, "do": [
            {"if": [{"alive": "lyra"}], "then": [{"say": "story says lyra alive"}]},
            {"if": [{"dead": "lyra"}], "then": [{"say": "story says lyra dead"}]},
            {"if": [{"rosterAlive": "lyra"}], "then": [{"say": "roster says lyra alive"}]},
            {"if": [{"inRoster": "lyra"}], "then": [{"say": "roster has lyra"}]},
            {"if": [{"rosterAlive": "stranger"}], "then": [{"say": "roster says stranger alive"}]},
            {"if": [{"rosterDead": "stranger"}], "then": [{"say": "roster says stranger dead"}]}]},
        {"id": "back", "when": {"turn": 2}, "do": [{"showUnit": "lyra", "x": 1, "y": 0},
                                                    {"if": [{"alive": "lyra"}], "then": [{"say": "lyra is back"}]}]}]
    write(root, 'maps/field.json', {
        "name": "Field", "grid": {"type": "square", "width": 8, "height": 3}, "legend": {".": "plain"},
        "tiles": ["........"] * 3,
        "sides": [{"id": "blue", "controller": "ai"}, {"id": "red", "controller": "ai"}],
        "units": [{"type": "hero", "id": "kay", "name": "Kay", "side": "blue", "x": 0, "y": 1},
                  {"type": "hero", "id": "lyra", "name": "Lyra", "side": "blue", "x": 0, "y": 2},
                  {"type": "brute", "id": "gorm", "name": "Gorm", "side": "red", "x": 7, "y": 1}],
        "events": events})


def play(root):
    cmd = [PLAYER, '--games', os.path.dirname(root), '--game', os.path.basename(root), '--map', 'field', '--autoplay',
           '--headless', '--no-audio', '--no-settings', '--no-saving', '--frames', '6000']
    p = run_text(cmd)
    return p.returncode, p.stdout


def lines(log, tag):
    return [l for l in log.splitlines() if tag in l]


def main():
    tmp = tempfile.mkdtemp(prefix='fidelity_player_')
    try:
        keyed = os.path.join(tmp, 'fid_on')
        control = os.path.join(tmp, 'fid_off')
        make_game(keyed, {'with_keys': True})
        make_game(control, {'with_keys': False})
        code, log = play(keyed)
        check('the game with every key plays to the end', code == 0 and 'ERROR' not in log, log[-400:])
        said = [re.sub(r'^.*dialogue: ', '', l) for l in lines(log, 'dialogue:')]
        check('hidden unit: neither alive nor dead for the story',
              not any('story says lyra' in s for s in said), str(said))
        check('hidden unit: the roster conditions say it lives', any('roster says lyra alive' in s for s in said) and any('roster has lyra' in s for s in said), str(said))
        check('a character that is not on the map is neither alive nor dead (free battle)',
              not any('stranger' in s for s in said), str(said))
        check('showUnit brings it back (alive again on turn 2)', any('lyra is back' in s for s in said), str(said))
        order = [i for i, s in enumerate(said) if 'roster has lyra' in s or 'lyra is back' in s]
        check('and in the right order', len(order) == 2 and order[0] < order[1], str(said))
        check('level cap 0 and computerGains false: nobody levels up (no capped-stat box)', not lines(log, 'level-up box'), str(lines(log, 'level-up box')[:3]))
        code, base = play(control)
        boxes = lines(base, 'level-up box')
        check('the same game without the keys plays and its units level up into the cap (control)', code == 0 and bool(boxes), base[-300:])
        check('the stat cap is read from caps.json (Attack: max in the level-up box)', any('Attack' in l and 'max' in l for l in boxes), str(boxes[:3]))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    summary()
    return 1 if failures else 0


if __name__ == '__main__':
    if len(sys.argv) > 1:
        PLAYER = os.path.abspath(sys.argv[1])
    if len(sys.argv) > 2:
        repo = sys.argv[2]
    sys.exit(main())
