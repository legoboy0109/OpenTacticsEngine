#!/usr/bin/env python3
"""Branching research in the real player, windowless (docs/ECONOMY.md "Branching research").

  python3 tests/research_branches_player_test.py ./build/engine/player/ote-player . [--shots DIR]

Plays copies of the synthetic game tests/fixtures/research-branches (not a demo): blue has 500 gold and a lab; five technologies in two trees
(Military: tactics, then the exclusive doctrines shield_wall / skirmish; Economy: trade, then smelting which needs a forge). Judged from the
[info] lines the player logs, from the saved game and from the screenshots:

  - The research panel groups the technologies by tree and tier (headings), greys what cannot be started and says why.
  - Choosing a doctrine closes the other one for that side ("closed: you chose ..."); the panel says so.
  - Choosing the technology under way offers "Cancel research" with the refund; confirming pays 50 % back and frees the slot; "Keep" does not.
  - The unit details list a side-wide skill (granted by research) with the origin "Side".
  - The same panel at UI scale 150 on the 480x270 canvas draws without a UI bug.
  - A game whose technologies use none of the new keys keeps its flat panel (no headings).
Pictures: --shots DIR keeps them (BMP).
"""
import json
import os
import re
import shutil
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import check, player_and_repo, run_text, scratch_dir, shots_dir, summary, ticks  # noqa: E402

sys.dont_write_bytecode = True
PLAYER, REPO = player_and_repo()
SHOTS = shots_dir(absolute=True)
TMP = scratch_dir()
GAMES = os.path.join(TMP, 'games')
IDLE = 'wait:30'  # the turn banner plays first; it takes the first inputs otherwise
# The battle menu: End Turn, Unit, Research, ... (the Unit entry exists because the game has skills).
OPEN_PANEL = 'menu,down,down,confirm,wait:3'


def write_json(path, doc):
    with open(path, 'w') as f:
        json.dump(doc, f)


def make_games():
    shutil.rmtree(GAMES, ignore_errors=True)
    src = os.path.join(REPO, 'tests', 'fixtures', 'research-branches')
    shutil.copytree(src, os.path.join(GAMES, 'rb'))

    def variant(name, grants, sub=None):
        """A copy whose map grants technologies to blue at the start of the battle (what a story event does)."""
        shutil.copytree(os.path.join(GAMES, 'rb'), os.path.join(GAMES, name))
        m = json.load(open(os.path.join(GAMES, name, 'maps', 'field.json')))
        m['events'] = [{'id': 'grant', 'when': {'battle': 'start'}, 'do': [{'grantResearch': t, 'side': 'blue'} for t in grants]}]
        write_json(os.path.join(GAMES, name, 'maps', 'field.json'), m)

    variant('rb_tactics', ['tactics'])                 # a doctrine can be chosen
    variant('rb_wall', ['tactics', 'shield_wall'])     # the shield wall doctrine is done: infantry have Bulwark
    # A game with plain technologies: the new keys taken out.
    shutil.copytree(os.path.join(GAMES, 'rb'), os.path.join(GAMES, 'rb_plain'))
    plain = {'technologies': [{'id': 'a', 'name': 'Alpha', 'cost': {'gold': 10}, 'turns': 1}, {'id': 'b', 'name': 'Beta', 'cost': {'gold': 20}, 'turns': 2}]}
    write_json(os.path.join(GAMES, 'rb_plain', 'data', 'research.json'), plain)


def play(game, inp, name, scale=None, saves=False):
    shot = os.path.join(TMP, name + '.bmp')
    cmd = [PLAYER, '--games', GAMES, '--assets', os.path.join(REPO, 'assets'), '--headless', '--no-audio',
           '--frames', str(ticks(inp) * 4 + 80), '--input-interval', '4', '--fixed-dt', '0.016',
           '--game', game, '--map', 'field', '--3d', 'software', '--no-settings', '--input', inp, '--screenshot', shot]
    sdir = os.path.join(TMP, 'saves_' + name)
    if saves:
        shutil.rmtree(sdir, ignore_errors=True)
        cmd += ['--saves', sdir]
    else:
        cmd += ['--no-saving']
    if scale:
        cmd += ['--ui-scale', str(scale)]
    log = run_text(cmd).stdout
    if SHOTS and os.path.exists(shot):
        os.makedirs(SHOTS, exist_ok=True)
        shutil.copy(shot, os.path.join(SHOTS, 'rb_' + name + '.bmp'))
    save = None
    if saves and os.path.exists(os.path.join(sdir, game, 'continue.json')):
        save = json.load(open(os.path.join(sdir, game, 'continue.json')))
    return log, save


def lines(log, prefix):
    out = []
    for line in log.split('\n'):
        m = re.search(r'\[info\]\s*(.*)$', line)
        if m and m.group(1).startswith(prefix):
            out.append(m.group(1))
    return out


def clean(log):
    return 'UI bug' not in log and '[warn]' not in log and '[error]' not in log


def main():
    make_games()

    # ---- the grouped panel, at 100 % and 150 %
    for tag, scale in (('panel', None), ('panel_150', 150)):
        log, _ = play('rb', IDLE + ',' + OPEN_PANEL, tag, scale)
        rows = lines(log, 'research panel:')
        check('%s: grouped by tree and tier with headings, greyed rows marked' % tag,
              rows == ['research panel: Military  tier 1; Tactics (50 Gold, 1 turn); Military  tier 2; Shield Wall (100 Gold, 2 turns) [greyed]; '
                       'Skirmish (100 Gold, 2 turns) [greyed]; Economy  tier 1; Trade (60 Gold, 1 turn); Economy  tier 2; Smelting (80 Gold, 1 turn) [greyed]'], rows)
        why = lines(log, 'research reasons:')
        check('%s: the reasons it cannot be started' % tag,
              why == ['research reasons: Shield Wall: needs Tactics; Skirmish: needs Tactics; Smelting: needs a Forge'], why)
        check('%s: no UI bug or warning' % tag, clean(log), log[-400:])

    # ---- a branch: choosing one closes the other
    pick = IDLE + ',' + OPEN_PANEL + ',down,confirm,wait:90,' + OPEN_PANEL  # (the first selectable row is Tactics, done; down = Shield Wall)
    for tag, scale in (('branch', None), ('branch_150', 150)):
        log, _ = play('rb_tactics', pick, tag, scale)
        check('%s: Shield Wall was started' % tag, lines(log, 'research started:') == ['research started: blue Shield Wall (2 turns)'], lines(log, 'research started:'))
        why = lines(log, 'research reasons:')
        check('%s: the other doctrine is closed, the rest wait for the one under way' % tag,
              bool(why) and 'Skirmish: closed: you chose Shield Wall' in why[-1] and 'Trade: already researching Shield Wall' in why[-1], why)
        check('%s: no UI bug or warning' % tag, clean(log), log[-400:])

    # ---- cancel: ask first, pay back half, free the slot
    cancel = pick + ',down,confirm,wait:3,confirm,wait:90'  # on the row under way: Confirm opens "Cancel research / Keep researching"
    log, save = play('rb_tactics', cancel, 'cancel', saves=True)
    menu = lines(log, 'research cancel menu:')
    check('cancel: the menu offers the refund', menu == ['research cancel menu: Cancel research (+50 Gold)'], menu)
    check('cancel: it was logged with the turns that were left', lines(log, 'research cancelled:') == ['research cancelled: blue Shield Wall (2 turns were left)'],
          lines(log, 'research cancelled:'))
    side = save['battle']['sides'][0] if save else {}
    check('cancel: 500 - 100 + 50 gold left and nothing under way', save is not None and side['resources'][0] == 450 and 'current' not in side.get('research', {}),
          side)
    check('cancel: no UI bug or warning', clean(log), log[-400:])
    shot_log, _ = play('rb_tactics', pick + ',down,confirm,wait:3', 'cancel_menu', 150)
    check('cancel menu at UI scale 150: no UI bug', clean(shot_log) and bool(lines(shot_log, 'research cancel menu:')), shot_log[-300:])
    keep = pick + ',down,confirm,wait:3,down,confirm,wait:5'
    log, save = play('rb_tactics', keep, 'keep', saves=True)
    side = save['battle']['sides'][0] if save else {}
    check('keep: the research goes on and nothing is paid back', save is not None and side['resources'][0] == 400 and side['research'].get('current') is not None
          and not lines(log, 'research cancelled:'), side)

    # ---- the unit details name the side as the origin of a granted skill
    log, _ = play('rb_wall', 'wait:120,menu,down,confirm,wait:4', 'info')  # two research banners play first
    check('details: the infantry unit has Bulwark from its side', 'unit side skills: Soldier: Bulwark (Side)' in lines(log, 'unit side skills:'),
          [lines(log, 'unit side skills:'), lines(log, 'battle menu:'), lines(log, 'unit details:')])
    check('details: no warnings', clean(log), log[-300:])
    log, _ = play('rb', IDLE + ',menu,down,confirm,wait:4', 'info_none')
    check('details: a game without the grant lists no side skills', not lines(log, 'unit side skills:') and bool(lines(log, 'unit details:')), lines(log, 'unit details:'))

    # ---- plain technologies keep the flat panel
    log, _ = play('rb_plain', IDLE + ',' + OPEN_PANEL, 'plain')
    rows = lines(log, 'research panel:')
    check('plain game: a flat list without headings', rows == ['research panel: Alpha (10 Gold, 1 turn); Beta (20 Gold, 2 turns)'], rows)
    check('plain game: nothing about reasons or tiers', not lines(log, 'research reasons:'), lines(log, 'research reasons:'))

    summary()
    if not SHOTS:
        shutil.rmtree(TMP, ignore_errors=True)


if __name__ == '__main__':
    main()
