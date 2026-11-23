#!/usr/bin/env python3
"""Swapping items on the battle's trade screen, through the real player, windowless (docs/ITEMS.md "The trade screen").

  python3 tests/trade_swap_player_test.py ./build/engine/player/ote-player . [--shots DIR]

Plays a copy of tests/fixtures/icon-lists with an inventory of 3: Hero carries Iron Sword, Hunting Bow, Potion and Friend Salve, Cloak,
Charm (two FULL packs: a plain hand over is impossible). The scripts press keys / tap rows: pick up, swap across, a second swap in the
same visit, a reorder, a free slot (a game where Friend carries two), putting an item back with Cancel, and the pointer. The player
logs the packs ("trade: Hero [..] | Friend [..]") and where each row is on screen ("trade rows: 0.0@x,y ..."); the Continue-less
run stays windowless and silent (--no-audio).
"""
import os
import re
import shutil
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import check, edit, failures, inp, lines, player_and_repo, run_text, scratch_dir, shots_dir, summary, ticks  # noqa: E402

sys.dont_write_bytecode = True

PLAYER, REPO = player_and_repo()
SHOTS = shots_dir(absolute=True)
TMP = scratch_dir()

OPEN = ['wait:30', 'confirm', 'confirm', 'wait:5']  # select the hero, "move" nowhere: the action menu (Attack, Items, Trade, ...)
TRADE = OPEN + ['down', 'down', 'confirm', 'wait:6']  # the trade screen with Friend (the only neighbour), cursor on Hero's first item


def make_game(gid, friend_items):
    g = os.path.join(TMP, gid)
    shutil.copytree(os.path.join(REPO, 'tests/fixtures/icon-lists'), g)
    edit(g, 'game.json', lambda gj: gj['rules']['items'].update({'inventorySize': 3}))

    def units(u):
        for d in u['units']:
            if d['id'] == 'hero':
                d['items'] = ['iron_sword', 'hunting_bow', 'potion']
            if d['id'] == 'friend':
                d['items'] = friend_items
    edit(g, 'data/units.json', units)
    return g


def play(gid, script, shot=None):
    cmd = [PLAYER, '--games', TMP, '--assets', os.path.join(REPO, 'assets'), '--headless', '--no-audio', '--no-settings', '--no-saving',
           '--game', gid, '--map', 'field', '--battle-scene', 'map', '--fixed-dt', '0.016', '--input-interval', '4',
           '--frames', str(ticks(script) * 4 + 100), '--input', script]
    if shot:
        cmd += ['--screenshot', os.path.join(TMP, shot + '.bmp')]
    log = run_text(cmd, timeout=120).stdout
    if shot and SHOTS and os.path.exists(os.path.join(TMP, shot + '.bmp')):
        os.makedirs(SHOTS, exist_ok=True)
        shutil.copy(os.path.join(TMP, shot + '.bmp'), os.path.join(SHOTS, shot + '.bmp'))
    return log


def last(log, key):
    found = lines(log, key)
    return found[-1].split('] ', 1)[-1] if found else ''


def packs(log):
    return last(log, 'trade: Hero')


def rows(log):
    return {tuple(map(int, k.split('.'))): (int(x), int(y)) for k, x, y in re.findall(r' (\d\.\d)@(\d+),(\d+)', last(log, 'trade rows:'))}


def tap(p):
    return 'tap:%d:%d' % p


def main():
    make_game('full', ['salve', 'cloak', 'charm'])
    make_game('free', ['salve', 'cloak'])
    first = 'trade: Hero [Iron Sword, Hunting Bow, Potion] | Friend [Salve, Cloak, Charm]'

    log = play('full', inp(TRADE), 'trade_start')
    check('the trade screen opens with two full packs', packs(log) == first and 'UI bug' not in log, packs(log))
    check('the hint line says Confirm picks up', 'hint: ' in log and 'pick up' in last(log, 'hint:'), last(log, 'hint:'))

    # pick up the sword, go right, Confirm on Salve: a swap (the packs are full, so a move could not do this)
    log = play('full', inp(TRADE, ['confirm', 'right', 'wait:2']), 'trade_holding')
    check('picking up changes nothing yet', packs(log) == first and 'UI bug' not in log, packs(log))
    check('holding over an item: the hint says swap', 'swap' in last(log, 'hint:'), last(log, 'hint:'))
    log = play('full', inp(TRADE, ['confirm', 'right', 'confirm', 'wait:4']), 'trade_swapped')
    check('Iron Sword and Salve swapped places', packs(log) == 'trade: Hero [Salve, Hunting Bow, Potion] | Friend [Iron Sword, Cloak, Charm]' and 'UI bug' not in log, packs(log))
    check('... and the screen stays open for the next one (no action menu yet)', len(lines(log, 'trade: Hero')) == 2, str(len(lines(log, 'trade: Hero'))))

    # a second swap in the same visit (cursor stayed on Friend's first row): Cloak <-> Bow, then a reorder in Hero's own pack
    two = TRADE + ['confirm', 'right', 'confirm', 'down', 'confirm', 'left', 'confirm', 'wait:4']
    log = play('full', inp(two))
    check('a second swap in the same visit: Cloak and Bow', packs(log) == 'trade: Hero [Salve, Cloak, Potion] | Friend [Iron Sword, Hunting Bow, Charm]', packs(log))
    three = two[:-1] + ['confirm', 'down', 'confirm', 'wait:4']  # Hero's Cloak picked up, put on Potion's slot: they trade places
    log = play('full', inp(three))
    check('a reorder inside Hero\'s own pack', packs(log) == 'trade: Hero [Salve, Potion, Cloak] | Friend [Iron Sword, Hunting Bow, Charm]', packs(log))

    # Cancel with an item picked up only puts it back; Cancel again leaves the trade, keeping what was done
    log = play('full', inp(TRADE, ['confirm', 'right', 'confirm', 'confirm', 'cancel', 'wait:4']))
    check('Cancel with an item in hand puts it back and stays on the screen',
          len(lines(log, 'trade: Hero')) == 2 and 'order:' not in log and 'pick up' in last(log, 'hint:'), last(log, 'hint:'))
    log = play('full', inp(TRADE, ['confirm', 'right', 'confirm', 'cancel', 'wait:6']), 'trade_left')
    check('Cancel leaves the trade with the swap kept (the action menu is back; Hero has not acted)',
          packs(log) == 'trade: Hero [Salve, Hunting Bow, Potion] | Friend [Iron Sword, Cloak, Charm]' and 'UI bug' not in log and 'order:' not in log, packs(log))
    # (picking up an item and Confirm on the same row puts it back, too)
    log = play('full', inp(TRADE, ['confirm', 'confirm', 'down', 'confirm', 'right', 'confirm', 'wait:4']))
    check('Confirm on the item in hand puts it back; the next pick-up is a new one (Bow <-> Cloak)',
          packs(log) == 'trade: Hero [Iron Sword, Cloak, Potion] | Friend [Salve, Hunting Bow, Charm]', packs(log))

    # a free slot: a move, as before (Friend carries two)
    log = play('free', inp(TRADE))
    check('a game with a free slot: Friend carries two', packs(log) == 'trade: Hero [Iron Sword, Hunting Bow, Potion] | Friend [Salve, Cloak]', packs(log))
    log = play('free', inp(TRADE, ['confirm', 'right', 'down', 'down', 'confirm', 'wait:4']), 'trade_moved')
    check('Hero\'s sword moves into Friend\'s free slot', packs(log) == 'trade: Hero [Hunting Bow, Potion] | Friend [Salve, Cloak, Iron Sword]' and 'UI bug' not in log, packs(log))
    log = play('free', inp(TRADE, ['right', 'down', 'down', 'confirm', 'wait:4']))
    check('a free slot cannot be picked up', packs(log) == 'trade: Hero [Iron Sword, Hunting Bow, Potion] | Friend [Salve, Cloak]' and 'UI bug' not in log, packs(log))
    log = play('full', inp(TRADE, ['down', 'down', 'confirm', 'right', 'up', 'confirm', 'wait:4']))
    check('a full pack cannot take a plain move, but a swap works (Potion <-> Cloak)',
          packs(log) == 'trade: Hero [Iron Sword, Hunting Bow, Cloak] | Friend [Salve, Potion, Charm]', packs(log))

    # the pointer: tap selects, a second tap on the same row picks up / puts down; two taps on Friend's Charm then Hero's Bow
    log = play('full', inp(TRADE))
    spot = rows(log)
    check('the screen logs where its rows are', len(spot) == 6, str(spot))
    if len(spot) == 6:
        script = inp(TRADE, [tap(spot[(0, 1)]), 'wait:2', tap(spot[(0, 1)]), 'wait:2', tap(spot[(1, 2)]), 'wait:2', tap(spot[(1, 2)]), 'wait:4'])
        log = play('full', script, 'trade_tapped')
        check('touch / mouse: tap, tap (pick up the Bow), tap, tap (put it on Charm): they swap',
              packs(log) == 'trade: Hero [Iron Sword, Charm, Potion] | Friend [Salve, Cloak, Hunting Bow]' and 'UI bug' not in log, packs(log))

    # nothing is lost and nothing is made over several swaps
    log = play('full', inp(TRADE, ['confirm', 'right', 'confirm', 'down', 'confirm', 'left', 'confirm', 'wait:4']))
    names = sorted(n.strip() for part in re.findall(r'\[(.*?)\]', packs(log)) for n in part.split(','))
    check('no item appears or disappears over several swaps', names == sorted(['Iron Sword', 'Hunting Bow', 'Potion', 'Salve', 'Cloak', 'Charm']), packs(log))

    if not failures:
        shutil.rmtree(TMP, ignore_errors=True)
    summary()
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
