#!/usr/bin/env python3
"""A free battle's preparation screen: Items, the shop and the stocked supply (docs/EVENTS.md "Preparation",
docs/GAME_FORMAT.md "Free battles"), through the real player, windowless.

  python3 tests/free_battle_prep_player_test.py ./build/engine/player/ote-player .

Copies tests/fixtures/srpg-prep (map "orchard" has a prep block, a shop, funds of 25 and one named unit, Ilsa, with a token) and gives
the game rules.items.freeBattleStorage { each: 3, funds: { gold: 100 } }. Starts the map as a free battle (--map orchard), opens
Items, moves a potion from the storage to Ilsa, buys a tonic at the shop, begins, and reads the player's log: the battle starts with
the potion on Ilsa, the storage reduced and the funds spent. Also: a game without the key has an empty storage, and nothing of a
free battle ever writes a save (the saves folder stays empty). Never plays audio or opens a window.
"""
import os
import shutil
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts
from ote_testkit import (  # noqa: E402
    run_text, check, failures, player_and_repo, player_cmd, read_json, shots_dir, summary, ticks, write_json, scratch_dir)

sys.dont_write_bytecode = True

PLAYER, REPO = player_and_repo()
TMP = scratch_dir()
SHOTS = shots_dir(absolute=True)  # --shots DIR keeps the storage list's screenshots (BMP)



def make_game(gid, stock):
    g = os.path.join(TMP, gid)
    shutil.copytree(os.path.join(REPO, 'tests/fixtures/srpg-prep'), g)
    game = read_json(g + '/game.json')
    if stock is not None:
        game['rules']['items']['freeBattleStorage'] = stock
    write_json(g + '/game.json', game)
    return g


def play(gid, inp, saves, shot=None, scale=None):
    cmd = player_cmd(PLAYER, TMP, REPO) + ['--frames', str(ticks(inp) * 3 + 90), '--input-interval', '3', '--fixed-dt', '0.016',
                                           '--game', gid, '--saves', saves, '--map', 'orchard', '--input', inp]
    if shot and SHOTS:
        os.makedirs(SHOTS, exist_ok=True)
        cmd += ['--screenshot', os.path.join(SHOTS, shot + '.bmp')]
    if scale:
        cmd += ['--ui-scale', str(scale)]
    return run_text(cmd).stdout


def at(log, key):
    found = [l.split('] ', 1)[-1] for l in log.splitlines() if key in l]
    return found[-1] if found else ''


def menu_to(n):
    return ','.join(['down'] * n + ['confirm'])


def stock(log):
    """The rows of the last storage list the player logged, e.g. ['Potion 2/2 x3', 'Potion 1/2', ...]."""
    line = at(log, 'prep storage:')
    return line[line.find('[') + 1:line.rfind(']')].split(', ') if '[' in line else []


def stacking(saves):
    """The storage list stacks identical copies: one row with a count per distinct item and state."""
    to_trade = 'wait:10,' + menu_to(2) + ',confirm,confirm,wait:3'
    rows = stock(play('stocked', to_trade, saves, shot='storage_stacked_before'))
    check('stacking: one row per distinct item with x3', len(rows) == 4 and all(r.endswith(' x3') for r in rows) and rows[0] == 'Potion 2/2 x3', str(rows))
    log = play('stocked', to_trade + ',right,confirm,wait:3', saves, shot='storage_stacked_after_take')
    check('stacking: taking one copy leaves x2 (one copy moved, not the whole row)', stock(log)[0] == 'Potion 2/2 x2' and 'prep trade: Ilsa [Token, Potion, -]' in log, str(stock(log)))
    log = play('stocked', to_trade + ',right,confirm,left,down,confirm,wait:3', saves)
    check('stacking: giving it back merges into the row: x3 again', stock(log) == rows and 'Ilsa [Token, -, -]' in at(log, 'prep trade:'), str(stock(log)))
    # A partly used copy is its own row, after the fuller ones; taking from the x3 row never touches it.
    g = os.path.join(TMP, 'mixed')
    shutil.copytree(os.path.join(TMP, 'stocked'), g)
    m = read_json(g + '/maps/orchard.json')
    m['sides'][0]['storage'] = [{'item': 'potion', 'uses': 1}]
    write_json(g + '/maps/orchard.json', m)
    log = play('mixed', to_trade, saves, shot='storage_stacked_partial')
    rows = stock(log)
    check('stacking: a half-used copy is its own row after the fuller ones', rows[:2] == ['Potion 2/2 x3', 'Potion 1/2'], str(rows))
    log = play('mixed', to_trade + ',right,confirm,wait:3', saves)
    check('stacking: taking from the x3 row leaves the half-used row alone', stock(log)[:2] == ['Potion 2/2 x2', 'Potion 1/2'], str(stock(log)))
    log = play('mixed', to_trade + ',right,down,confirm,wait:3', saves)
    check('stacking: taking the half-used row moves that copy and its row disappears (the cursor stays on the list)', stock(log)[0] == 'Potion 2/2 x3' and 'Potion 1/2' not in stock(log), str(stock(log)))
    # The shop's Sell tab lists the storage the same way: one row per distinct item, one copy per confirm, the price column in line.
    to_sell = 'wait:10,' + menu_to(3) + ',right,down,confirm,wait:3'
    log = play('stocked', to_sell, saves, shot='storage_stacked_sell')
    line = at(log, 'prep sell:')
    check('stacking: the sell list has one row per item with its count and price', line.startswith('prep sell: Storage [Potion x3 [') and line.count(' x3 [') == 4, line)
    log = play('stocked', to_sell + ',confirm,wait:3', saves)
    check('stacking: selling sells ONE copy (x3 -> x2)', 'prep: sold potion from storage' in log and at(log, 'prep sell:').startswith('prep sell: Storage [Potion x2 ['), at(log, 'prep sell:'))
    log = play('stocked', to_trade, saves, shot='storage_stacked_scale150', scale=150)
    check('stacking: the list opens at UI scale 150%', len(stock(log)) == 4, str(stock(log)))


def main():
    stocked = make_game('stocked', {'each': 3, 'funds': {'gold': 100}})
    saves = os.path.join(TMP, 'saves')
    os.makedirs(saves)
    # 1. The menu offers Items and the Shop in a free battle; the storage is stocked (4 items x 3, Ilsa carries a token).
    log = play('stocked', 'wait:10', saves)
    check('free battle: the screen opens', 'prep: opens on orchard, 4 slots, limit 3 (free battle)' in log, log[-300:])
    menu = at(log, 'prep menu:')
    check('free battle: the menu has Items and Shop', all(w in menu for w in ('Items', 'Shop', 'Abandon')) and 'Save' not in menu, menu)
    log = play('stocked', 'wait:10,' + menu_to(2) + ',wait:3', saves)
    check('free battle: whose items lists Ilsa with her pack', at(log, 'prep items:').startswith('prep items: Ilsa [1/3]'), at(log, 'prep items:'))
    # 2. Items -> Ilsa -> Storage; right column: the first storage row (a potion) goes to Ilsa.
    to_trade = 'wait:10,' + menu_to(2) + ',confirm,confirm,wait:3'
    log = play('stocked', to_trade, saves)
    check('free battle: the trade panel shows Ilsa with the storage', 'prep trade: Ilsa [Token, -, -] with Storage' in log, at(log, 'prep trade'))
    log = play('stocked', to_trade + ',right,confirm,wait:3', saves)
    check('free battle: the potion goes from the storage to Ilsa', 'prep trade: Ilsa [Token, Potion, -] with Storage' in log, at(log, 'prep trade'))
    stacking(saves)
    # 3. The shop spends the free battle's funds (25 + 100): a tonic (30) for Ilsa, then Begin.
    log = play('stocked', 'wait:10,' + menu_to(3) + ',wait:3', saves)
    check('free battle: the shop shows the funds', 'Gold 125' in at(log, 'prep shop:'), at(log, 'prep shop:'))
    log = play('stocked', to_trade + ',right,confirm,wait:3,cancel,cancel,cancel,' + menu_to(3) + ',down,confirm,confirm,wait:3,cancel,' + menu_to(6) + ',wait:30', saves)
    check('free battle: a tonic is bought', 'prep: bought tonic for ilsa' in log, log[-400:])
    start = at(log, 'free battle starts:')
    # Ilsa's pack: token, potion, tonic. The storage: 12 stocked - 1 potion taken; funds 125 - 30.
    check('free battle: the battle starts with what the screen arranged', 'ilsa[token,potion,tonic]' in start and 'storage=11' in start and 'funds=95' in start, start)
    # 4. Without the key: an empty storage, the same menu.
    make_game('plain', None)
    log = play('plain', 'wait:10,' + menu_to(2) + ',confirm,confirm,wait:3', saves)
    check('free battle: a game without the key has an empty storage', 'with Storage' in at(log, 'prep trade') and 'prep trade: Ilsa [Token, -, -]' in log, at(log, 'prep trade'))
    log = play('plain', 'wait:10,' + menu_to(6) + ',wait:30', saves)
    check('free battle: and begins with storage=0 funds=25', 'storage=0 funds=25' in at(log, 'free battle starts:'), at(log, 'free battle starts:'))
    # 5. Nothing of a free battle touched the saves.
    check('free battle: no save or Continue file was written', not os.path.exists(os.path.join(saves, 'stocked')) or not os.listdir(os.path.join(saves, 'stocked')), str(os.listdir(saves)))
    shutil.rmtree(TMP, ignore_errors=True)
    summary()
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
