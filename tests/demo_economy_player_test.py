#!/usr/bin/env python3
"""The real games/demo-economy in the real player, windowless: the content of docs/ECONOMY.md "Build order" step 5.

  python3 tests/demo_economy_player_test.py ./build/engine/player/ote-player .
  python3 tests/demo_economy_player_test.py ./build/engine/player/ote-player . --shots OUT_DIR   # keeps the screenshots

economy_player_test.py and fog_player_test.py check the player's features on small games made for the test; this
one only checks that the shipped demo shows them: the structure menus, the research panel (from the battle menu), a locked unit type in
the Build menu, fuel and ammunition in the info panel, and the fog map dimmed where the viewer cannot see (top-down
and one 3D style). The player runs with --headless --no-settings --no-saving, never a window.

Squares of twin_rivers (the cursor starts on the first blue unit, the infantry at 2,4): blue hq 0,4 (2 x 2), factory
2,3, city 1,6, tank 3,4. Misty Ford (11 x 8, fog): the map is drawn from pixel (44,33), 24 pixels a square.
"""
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import check, failures, summary  # noqa: E402

PLAYER = ''
REPO = ''
SHOTS = ''


def play(game_map, inp, frames=400, style=None, extra=None, shot=None):
    cmd = [PLAYER, '--games', os.path.join(REPO, 'games'), '--assets', os.path.join(REPO, 'assets'), '--headless',
           '--no-settings', '--no-saving', '--frames', str(frames), '--input-interval', '8',
           '--game', 'demo-economy', '--map', game_map, '--input', inp]
    if style:
        cmd += ['--style', style]
    if shot:
        cmd += ['--screenshot', shot]
    p = subprocess.run(cmd + (extra or []), stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True,
                       timeout=300)
    return p.stdout


def lines(log, prefix):
    out = []
    for l in log.split('\n'):
        m = re.search(r'\[info\]\s*(.*)$', l)
        if m and m.group(1).startswith(prefix):
            out.append(m.group(1))
    return out


def read_bmp(path):
    """(width, height, function (x, y) -> (r, g, b)) of an uncompressed 24/32-bit BMP."""
    data = open(path, 'rb').read()
    offset = struct.unpack_from('<I', data, 10)[0]
    width, height = struct.unpack_from('<ii', data, 18)
    bpp = struct.unpack_from('<H', data, 28)[0] // 8
    stride = (width * bpp + 3) & ~3
    h = abs(height)

    def px(x, y):
        row = (h - 1 - y) if height > 0 else y
        o = offset + row * stride + x * bpp
        return data[o + 2], data[o + 1], data[o]
    return width, h, px


def brightness(px, x0, y0, x1, y1):
    """Mean of r + g + b over a block of pixels."""
    n, total = 0, 0
    for y in range(y0, y1):
        for x in range(x0, x1):
            total += sum(px(x, y))
            n += 1
    return total / max(1, n)


def main():
    global PLAYER, REPO, SHOTS
    args = sys.argv[1:]
    if len(args) < 2:
        print(__doc__)
        return 2
    PLAYER, REPO = os.path.abspath(args[0]), os.path.abspath(args[1])
    if '--shots' in args:
        SHOTS = os.path.abspath(args[args.index('--shots') + 1])
        os.makedirs(SHOTS, exist_ok=True)
    tmp = tempfile.mkdtemp(prefix='ote_demo_economy_test_')
    try:
        scenarios(tmp)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    summary(blank_line=True)
    return 1 if failures else 0


RESEARCH = 'menu,down,confirm,wait:5'


def scenarios(tmp):
    # 1. Research from the battle menu (End Turn, Research, Abandon battle, Cancel): no building needed.
    log = play('twin_rivers', 'wait:30,' + RESEARCH + ',cancel,wait:3')
    rp = lines(log, 'research panel:')
    check('battle menu: Research panel lists the three technologies, Plating greyed until Armor is done',
          bool(rp) and rp[0] == 'research panel: Armor Doctrine (4000 Funds, 2 turns); '
          'Reinforced Plating (5000 Funds, 2 turns) [greyed]; Urbanism (6000 Funds, 2 turns)', str(rp))

    # 2. The factory: Build, with the tank locked and the APC there.
    log = play('twin_rivers', 'wait:30,up,confirm,wait:5,cancel,wait:3')
    bm = lines(log, 'build menu:')
    check('factory: the tank is greyed with the technology it needs; APC and the rest are buildable',
          bool(bm) and 'Tank (needs Armor Doctrine) [greyed]' in bm[0] and 'APC (5000 Funds)' in bm[0]
          and 'Infantry (1000 Funds)' in bm[0] and 'Artillery (6000 Funds)' in bm[0], str(bm))

    # 3. A city offers one thing: Upgrade opens at once.
    log = play('twin_rivers', 'wait:30,left,down,down,confirm,wait:5,cancel,wait:3')
    um = lines(log, 'upgrade menu:')
    check('city: Upgrade opens directly with the town and its price', um == ['upgrade menu: Town (2000 Funds)'], str(um))

    # 4. Fuel and ammunition in the info panel (cursor on the blue tank).
    log = play('twin_rivers', 'wait:30,right,wait:5')
    check('info panel: the tank shows its fuel and its limited gun', 'info: Tank: Fuel 70/70, Tank Gun 9/9' in lines(log, 'info:'),
          str(lines(log, 'info:')))

    # 5. Armor Doctrine bought and finished on the real map: the tank is no longer greyed (two own turns later; the
    # cursor is sent to the corner first because it does not stay where it was after a turn change).
    end_turn = 'menu,confirm,wait:60'
    log = play('twin_rivers', 'wait:30,' + RESEARCH + ',confirm,wait:30,' + end_turn + ',' + end_turn +
               ',left,left,left,left,left,left,up,up,up,up,up,up,right,right,down,down,down,confirm,wait:5,cancel,wait:3', frames=3000)
    bm = lines(log, 'build menu:')
    check('research: after Armor Doctrine the factory offers the tank without a note',
          lines(log, 'research finished:') == ['research finished: Blue Army Armor Doctrine'] and bool(bm) and
          'Tank (7000 Funds)' in bm[-1], str(lines(log, 'research finished:')) + str(bm))

    # 6. The fog map: viewer logged; unseen squares are dimmed in the top-down view; --show-all draws everything.
    fog = os.path.join(tmp, 'fog.bmp')
    allv = os.path.join(tmp, 'all.bmp')
    log = play('misty_ford', 'wait:30', frames=200, shot=fog)
    play('misty_ford', 'wait:30', frames=200, shot=allv, extra=['--show-all'])
    check('misty_ford: fog is on for the blue viewer', 'fog: viewer Blue Army (team 0)' in lines(log, 'fog:'), str(lines(log, 'fog:')))
    w, h, pf = read_bmp(fog)
    _, _, pa = read_bmp(allv)

    def square(px, sx, sy):  # a block inside square (sx, sy): the map starts at pixel (44, 33)
        return brightness(px, 44 + sx * 24 + 4, 33 + sy * 24 + 4, 44 + sx * 24 + 20, 33 + sy * 24 + 20)
    far = (10, 7)  # the far corner of the red side: nobody of blue is within sight
    own = (1, 3)   # blue's own headquarters
    check('fog, top-down: an unseen square is dimmed to well under its show-all brightness',
          square(pf, *far) < 0.7 * square(pa, *far), '%.0f vs %.0f' % (square(pf, *far), square(pa, *far)))
    check('fog, top-down: the viewer\'s own square is drawn as it is',
          abs(square(pf, *own) - square(pa, *own)) < 0.03 * square(pa, *own), '%.0f vs %.0f' % (square(pf, *own), square(pa, *own)))
    if SHOTS:
        shutil.copy(fog, os.path.join(SHOTS, 'misty_ford_fog_classic.bmp'))
        shutil.copy(allv, os.path.join(SHOTS, 'misty_ford_show_all_classic.bmp'))

    # 7. The same in the 3D isometric style (camera pushed towards the red side): darker overall, not the same picture.
    f3 = os.path.join(tmp, 'fog3.bmp')
    a3 = os.path.join(tmp, 'all3.bmp')
    inp = 'wait:20,right,right,right,right,right,right,wait:60'
    play('misty_ford', inp, frames=300, style='isometric-3d', shot=f3)
    play('misty_ford', inp, frames=300, style='isometric-3d', shot=a3, extra=['--show-all'])
    _, _, p3f = read_bmp(f3)
    _, _, p3a = read_bmp(a3)
    view = (0, 0, 340, 255)  # the battle view, not the info panel
    bf, ba = brightness(p3f, *view), brightness(p3a, *view)
    check('fog, 3D isometric: the picture is darker than the show-all one', bf < 0.96 * ba, '%.1f vs %.1f' % (bf, ba))
    if SHOTS:
        shutil.copy(f3, os.path.join(SHOTS, 'misty_ford_fog_iso3d.bmp'))

    # 8. twin_rivers has no fog: no viewer line.
    log = play('twin_rivers', 'wait:30', frames=100)
    check('twin_rivers: no fog (no viewer line)', not lines(log, 'fog:'), str(lines(log, 'fog:')))


if __name__ == '__main__':
    sys.exit(main())
