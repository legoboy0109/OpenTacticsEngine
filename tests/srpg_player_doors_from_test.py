#!/usr/bin/env python3
"""The unlock ability and a chest's "from" (docs/SRPG.md section 10 "Reach"), windowless.

  python3 tests/srpg_player_doors_from_test.py ./build/engine/player/ote-player .

A chest that says "from": "on" is opened by the trait from the chest's own square only: standing on it the ability entry is offered and
its square pick holds that one square; standing next to it the entry is greyed with the plain reason and nothing opens.
"""
import os
import shutil
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
import srpg_player_doors_test as D  # noqa: E402  (the same fixture helpers; importing runs nothing: main() is guarded)
from ote_testkit import check, edit, failures, inp, lines, summary  # noqa: E402

sys.dont_write_bytecode = True


def make(gid, x, y, kind_from='on'):
    def tw_map(m):
        D.front(m, 'tink')
        D.unit_of(m, 'tink')['x'], D.unit_of(m, 'tink')['y'] = x, y
    g = D.make_game(gid, D.tweak_map('hall', tw_map))
    def tw_kinds(s):
        for k in s['structures']:
            if k['id'] == 'chest':
                k['chest']['from'] = kind_from
    edit(g, 'data/structures.json', tw_kinds)
    return D.Game(g)


def test_on_the_chest():
    G = make('from_on', 2, 0)
    code, log, battle = D.run('from_on', inp(*D.MENU, 'confirm', 'wait:3', 'confirm', 'wait:40'))
    check('from on: standing on the chest the ability is offered', bool(lines(log, 'ability entries of Tink: Deft Hands')) and not lines(log, 'Deft Hands ['), str(lines(log, 'ability entries')))
    check('from on: its square pick holds the chest square only', bool(lines(log, 'pick square: 1 candidates')), log[-300:])
    check('from on: it opens the chest, the contents are given', bool(lines(log, 'opened: Chest (c1) by Tink')) and G.structure(battle, 'hall', 'c1')[0] == 'chest_open', log[-300:])


def test_next_to_the_chest():
    G = make('from_next', 2, 1)
    code, log, battle = D.run('from_next', inp(*D.MENU, 'wait:6'))
    check('from on: next to the chest the ability is greyed with the reason', bool(lines(log, 'Deft Hands [stand on the chest to open it]')), str(lines(log, 'ability entries')))
    check('from on: and the chest stays shut', G.structure(battle, 'hall', 'c1')[0] == 'chest' if battle else True, str(battle)[:200])


def main():
    if not D.PLAYER or not os.path.exists(D.PLAYER):
        print('usage: srpg_player_doors_from_test.py PLAYER REPO')
        return 2
    try:
        test_on_the_chest()
        test_next_to_the_chest()
    finally:
        shutil.rmtree(D.TMP, ignore_errors=True)
    summary()
    for f in failures:
        print('  FAILED: ' + f)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
