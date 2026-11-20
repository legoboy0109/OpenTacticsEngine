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
    run_text, check, failures, player_and_repo, player_cmd, read_json, summary, ticks, write_json, scratch_dir)

sys.dont_write_bytecode = True

PLAYER, REPO = player_and_repo()
TMP = scratch_dir()



def make_game(gid, stock):
    g = os.path.join(TMP, gid)
    shutil.copytree(os.path.join(REPO, 'tests/fixtures/srpg-prep'), g)
    game = read_json(g + '/game.json')
    if stock is not None:
        game['rules']['items']['freeBattleStorage'] = stock
    write_json(g + '/game.json', game)
    return g


def play(gid, inp, saves):
    cmd = player_cmd(PLAYER, TMP, REPO) + ['--frames', str(ticks(inp) * 3 + 90), '--input-interval', '3', '--fixed-dt', '0.016',
                                           '--game', gid, '--saves', saves, '--map', 'orchard', '--input', inp]
    return run_text(cmd).stdout


def at(log, key):
    found = [l.split('] ', 1)[-1] for l in log.splitlines() if key in l]
    return found[-1] if found else ''


def menu_to(n):
    return ','.join(['down'] * n + ['confirm'])


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
