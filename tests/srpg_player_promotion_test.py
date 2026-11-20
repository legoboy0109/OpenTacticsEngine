#!/usr/bin/env python3
"""The promotion part of the strategy-RPG player (docs/SRPG.md "Player UI > Promotion", step U2), windowless.

  python3 tests/srpg_player_promotion_test.py ./build/engine/player/ote-player .
  python3 tests/srpg_player_promotion_test.py ./build/engine/player/ote-player . --shots OUT_DIR   # keep the screenshots (BMP)

Writes the small game of tests/srpg_promo_games.py into a temp folder (every unit type has its own flat colour as sprite
and as 3D model), runs the player headless with --input scripts and checks the log lines (the player logs the Promote
entry, the question or screen it opens and the animation it plays), the Continue save and the pixels:
  - the Promote entry appears only for a unit with a promotion on offer; the question box for one possible promotion
    (No / Cancel go back to the menu, Yes promotes); the promotion screen for several (the highlighted type's picture
    shows, Confirm picks it, Cancel goes back); a promotion item asks the same way from the item list;
  - UnitPromoted plays: the old picture first, then the new type's (sprite, 2D isometric sprite, 3D model, full 3D model),
    the banner and the stat box; automatic and story promotions play it too, with the turn's banner before;
  - the unit ends as the new type in the Continue save (level and stats as the core says);
  - a phone-sized screen (portrait) shows the question and the screen inside the window.
"""
import json
import os
import shutil
import subprocess
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import check_dash, failures, player_cmd, summary  # noqa: E402
from ote_testkit import scratch_dir  # noqa: E402
check = check_dash  # this script prints the "PASS name" / "FAIL name -> detail" layout

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import srpg_promo_games as pg  # noqa: E402
from terrain3d_player_test import Pic  # noqa: E402

PLAYER = REPO = ''
SHOTS = ''
START = 'wait:40'            # the turn banner plays first
PICK = 'confirm,confirm'     # select the unit under the cursor, stay: opens its action menu
INTERVAL = 4


def frames_for(inp):
    return sum(int(t[5:]) * INTERVAL if t.startswith('wait:') else INTERVAL for t in inp.split(',')) + 1


class Run:
    """One run of the player on the test game; `.log` is its output, `.pic` the screenshot, `.save` the Continue save."""
    n = 0

    def __init__(self, tmp, games, name, map_id, inp, style='classic', frames=None, extra=()):
        Run.n += 1
        shot = os.path.join(tmp, '%02d_%s.bmp' % (Run.n, name))
        saves = os.path.join(tmp, 'saves%d' % Run.n)
        cmd = player_cmd(PLAYER, games, REPO) + ['--saves', saves, '--fixed-dt', '0.016', '--input-interval', str(INTERVAL), '--game', 'promo', '--map', map_id,
               '--style', style, '--input', inp, '--frames', str(frames or frames_for(inp)), '--screenshot', shot] + list(extra)
        res = subprocess.run(cmd, capture_output=True, text=True, timeout=240)
        self.log = res.stdout + res.stderr
        self.ok = res.returncode == 0 and os.path.exists(shot)
        self.pic = Pic(shot) if self.ok else None
        if SHOTS and self.ok:
            shutil.copy(shot, os.path.join(SHOTS, name + '.bmp'))
        path = os.path.join(saves, 'promo', 'continue.json')
        self.save = json.load(open(path)) if os.path.exists(path) else None

    def lines(self, text):
        return [l for l in self.log.splitlines() if text in l]

    def unit(self, uid):
        for u in (self.save or {}).get('battle', {}).get('units', []):
            if u.get('id') == uid:
                return u
        return {}


def hue_count(pic, rgb, x0=0, x1=340, y0=0, y1=None, tol=0.16):
    """Pixels of about the colour `rgb` in any brightness (dimmed units, shaded 3D faces): the direction of the colour
    vector matches within `tol` and it is bright enough not to be the dark background."""
    n = 0
    ref = [c / (sum(rgb) or 1) for c in rgb]
    for y in range(y0, min(pic.h, y1 or pic.h)):
        for x in range(x0, min(pic.w, x1)):
            p = pic.px(x, y)
            s = sum(p)
            if s > 90 and all(abs(p[i] / s - ref[i]) <= tol * 0.6 for i in range(3)):
                n += 1
    return n


def types_of(run):
    return {u.get('id'): u.get('type') for u in run.save['battle']['units']} if run.save else {}


def main():
    global PLAYER, REPO, SHOTS
    args = sys.argv[1:]
    if '--shots' in args:
        i = args.index('--shots')
        SHOTS = os.path.abspath(args[i + 1])
        os.makedirs(SHOTS, exist_ok=True)
        del args[i:i + 2]
    if len(args) < 2:
        print(__doc__)
        return 2
    PLAYER, REPO = os.path.abspath(args[0]), os.path.abspath(args[1])
    with scratch_dir() as tmp:
        games = os.path.join(tmp, 'games')
        pg.write_game(games)
        phone = os.path.join(tmp, 'phone')
        pg.write_game(phone, (270, 480))
        R = lambda *a, **k: Run(tmp, games, *a, **k)
        C = pg.COLORS
        # Type indexes in data/units.json order.
        T = {n: i for i, n in enumerate(['recruit', 'captain', 'scout', 'pawn', 'rider', 'squire', 'cadet', 'grunt'])}
        done_pause = ',wait:30,confirm,wait:12'   # let the animation play, confirm its box, let the save be written

        # -- the Promote entry
        r = R('entry_none', 'plain', START + ',' + PICK + ',wait:2')
        check('no Promote entry for a unit with nothing on offer', r.ok and not r.lines('promote entry'), r.log[-400:])
        r = R('entry_two', 'choose', START + ',' + PICK + ',wait:2')
        check('Promote is offered with both types', r.lines('promote entry for Kell: Captain, Scout'), r.log[-400:])
        check('the entry is in the menu (a menu next to the unit, Promote first)',
              r.pic and hue_count(r.pic, (255, 255, 255), 20, 110, 100, 150) > 20)

        # -- two options: the promotion screen
        base = START + ',' + PICK + ',confirm'
        r = R('screen_captain', 'choose', base + ',wait:2')
        check('Promote with two types opens the promotion screen', r.lines('promotion screen: Captain, Scout'), r.log[-400:])
        check('the screen shows the highlighted type\'s picture (captain)', r.pic and hue_count(r.pic, C['captain'], 80, 200, 90, 160) > 400)
        r = R('screen_scout', 'choose', base + ',down,wait:2')
        check('Down highlights the scout and its picture shows', r.pic and hue_count(r.pic, C['scout'], 80, 200, 90, 160) > 400
              and hue_count(r.pic, C['captain'], 80, 200, 90, 160) < 50)
        r = R('screen_cancel', 'choose', base + ',cancel,wait:2')
        check('Cancel on the screen goes back to the action menu', len(r.lines('promote entry for Kell')) == 2, r.log[-400:])
        r = R('tap_select', 'choose', base + ',tap:40:118,wait:2')  # (the Scout row; a first tap only selects)
        check('a first tap on a row only selects it', r.pic and hue_count(r.pic, C['scout'], 80, 200, 90, 160) > 400 and not r.lines('promotion plays'))
        r = R('tap_pick', 'choose', base + ',tap:40:118,tap:40:118,wait:5')
        check('a second tap on the selected row picks it', r.lines('promotion plays: Kell, Recruit to Scout'), r.log[-300:])
        r = R('tap_outside', 'choose', base + ',tap:300:20,wait:2')
        check('a tap outside the screen goes back', len(r.lines('promote entry for Kell')) == 2, r.log[-300:])
        r = R('scout_pick', 'choose', base + ',down,confirm,wait:5')
        check('Confirm on the scout plays the promotion', r.lines('promotion plays: Kell, Recruit to Scout, level 3'), r.log[-400:])
        r = R('scout_done', 'choose', base + ',down,confirm' + done_pause)
        check('the save has Kell as a scout at level 3 (this promotion keeps the level)',
              types_of(r).get('kell') == T['scout'] and r.unit('kell').get('level') == 3, r.unit('kell'))
        check('the scout\'s picture replaced the recruit\'s on the map',
              r.pic and hue_count(r.pic, C['scout'], 0, 352) > 150 and hue_count(r.pic, C['recruit'], 0, 352) < 30)

        # -- one option: the question box
        base = START + ',next,' + PICK
        r = R('question', 'choose', base + ',confirm,wait:2')
        check('Promote with one type asks', r.lines('promotion question: Rider'), r.log[-400:])
        r = R('question_no', 'choose', base + ',confirm,confirm,wait:2')
        check('No (the first button) goes back to the action menu', len(r.lines('promote entry for Bram')) == 2, r.log[-400:])
        r = R('question_cancel', 'choose', base + ',confirm,cancel,wait:2')
        check('Cancel answers No', len(r.lines('promote entry for Bram')) == 2, r.log[-400:])
        r = R('question_yes', 'choose', base + ',confirm,right,confirm' + done_pause)
        check('Yes promotes the pawn to a rider (level reset)', r.lines('promotion plays: Bram, Pawn to Rider, level 1')
              and types_of(r).get('bram') == T['rider'], r.log[-400:])
        check('the rider\'s picture is on the map', r.pic and hue_count(r.pic, C['rider'], 0, 352) > 150
              and hue_count(r.pic, C['pawn'], 0, 352) < 30)

        # -- items
        r = R('item_one', 'choose', START + ',next,next,' + PICK + ',confirm,confirm,confirm,wait:2')
        check('a promotion item asks for one possible type', r.lines('promotion question: Captain'), r.log[-400:])
        r = R('item_one_done', 'choose', START + ',next,next,' + PICK + ',confirm,confirm,confirm,right,confirm' + done_pause)
        check('using the token promotes the squire and spends the token',
              types_of(r).get('ivo') == T['captain'] and not r.unit('ivo').get('items'), r.unit('ivo'))
        r = R('item_two', 'choose', START + ',' + PICK + ',down,confirm,down,confirm,confirm,wait:2')
        check('a promotion item with two possible types opens the screen', r.lines('promotion screen: Captain, Scout'), r.log[-400:])
        r = R('item_cancel', 'choose', START + ',' + PICK + ',down,confirm,down,confirm,confirm,cancel,wait:2')
        check('Cancel on it goes back to the item list', len(r.lines('promote entry for Kell')) == 1, r.log[-400:])

        # -- automatic and story promotions play the same entry
        r = R('auto', 'auto', START + done_pause)
        check('an automatic promotion plays', r.lines('promotion plays: Dara, Cadet to Captain, level 1'), r.log[-400:])
        check('and the unit is a captain in the save', types_of(r).get('dara') == T['captain'], types_of(r))
        r = R('story', 'story', START + done_pause)
        check('a promotion from the story plays', r.lines('promotion plays: Wren, Recruit to Captain'), r.log[-400:])

        # -- a style whose units are models borrows the picture of another style for the screen (not the letter block)
        for style in ('isometric-3d', 'full-3d'):
            r = R('screen_' + style, 'choose', START + ',' + PICK + ',confirm,wait:2', style=style)
            check('%s: the promotion screen shows the captain\'s picture from the sprite style' % style,
                  r.pic and hue_count(r.pic, C['captain'], 80, 200, 90, 160) > 400,
                  hue_count(r.pic, C['captain'], 80, 200, 90, 160) if r.pic else r.log[-300:])

        # -- every kind of picture changes
        for style, tolerance_min in (('classic', 150), ('isometric', 60), ('isometric-3d', 40), ('full-3d', 25)):
            base = START + ',' + PICK + ',confirm,confirm,wait:30,confirm,wait:12'  # first type of the screen: the captain
            r = R('style_' + style, 'choose', base, style=style)
            check('%s: the unit is drawn as the captain after promoting' % style,
                  r.pic and hue_count(r.pic, C['captain'], 0, 352) > tolerance_min and hue_count(r.pic, C['recruit'], 0, 352) < tolerance_min // 3,
                  (hue_count(r.pic, C['captain'], 0, 352), hue_count(r.pic, C['recruit'], 0, 352)) if r.pic else r.log[-300:])
            fl = R('flash_' + style, 'choose', START + ',' + PICK + ',confirm,confirm,wait:1', style=style)
            # The flash washes the picture towards white, so the hue test is wide; the grey ground matches it a little too,
            # which is why it is judged against the same count after the promotion (the recruit's colour gone).
            wide = lambda pic: hue_count(pic, C['recruit'], 0, 352, tol=0.4)
            check('%s: the old picture shows while the flash starts' % style,
                  fl.pic and r.pic and wide(fl.pic) > wide(r.pic) + tolerance_min // 3, (wide(fl.pic), wide(r.pic)) if fl.pic and r.pic else '')

        # -- a phone-sized screen
        for name, inp in (('phone_question', START + ',next,' + PICK + ',confirm,wait:2'), ('phone_screen', START + ',' + PICK + ',confirm,wait:2'),
                          ('phone_banner', START + ',' + PICK + ',confirm,confirm,wait:10')):
            r = Run(tmp, phone, name, 'choose', inp)
            check(name + ' runs and the window is 270 x 480', r.ok and (r.pic.w, r.pic.h) == (270, 480), r.log[-300:])
    summary()
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
