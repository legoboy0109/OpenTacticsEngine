#!/usr/bin/env python3
"""The player and changing the map (docs/EVENTS.md "Changing the map", docs/SRPG.md section 10 "Walls that break", docs/ECONOMY.md
"Fog of war"), windowless.

  python3 tests/mapchange_player_test.py ./build/engine/player/ote-player .
  python3 tests/mapchange_player_test.py ./build/engine/player/ote-player . --shots OUT_DIR   # keep the screenshots (BMP)

Copies tests/fixtures/mapchange into a temp folder, runs the player headless (no audio, temp saves) with --input scripts and reads
the player's [info] log lines, the Continue save it writes and screenshots:

  - a bridge that appears by an event (the log line, the changed square's colour in the picture, the saved state), in the 2D view and
    in the 3D isometric view;
  - a wall broken by an attack: the menu says "Break wall", the forecast, the opening, the map change the opening starts, and the room
    behind it comes into sight (the picture gets brighter);
  - fog lifting by an event (and coming down again), a torch item lighting squares in the fog.
"""
import json
import os
import shutil
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import (  # noqa: E402
    Bmp, run_text, check, edit, failures, inp, lines, player_and_repo, player_cmd, read_json, shots_dir, summary, ticks, write_json)
from ote_testkit import scratch_dir  # noqa: E402

sys.dont_write_bytecode = True

PLAYER, REPO = player_and_repo()
SHOTS = shots_dir()
TMP = scratch_dir()

IDLE = 'wait:14'  # the turn banner plays first; it takes the first inputs otherwise
MENU = [IDLE, 'confirm', 'wait:3', 'confirm', 'wait:3']  # select the first unit, open its action menu
# Move the cursor off the units (the first unit is under it) and end the turn from the empty square's menu; red plays, round 2 begins.
END_TURN = [IDLE, 'right', 'wait:2', 'right', 'wait:2', 'confirm', 'wait:3', 'confirm', 'wait:200']

STYLES = [{'id': 'top', 'name': 'Top'}, {'id': 'iso3', 'name': 'Iso 3D', 'view': 'isometric-3d'}]


def make_game(gid, tweak=None, styles=False):
    g = os.path.join(TMP, gid)
    shutil.copytree(os.path.join(REPO, 'tests/fixtures/mapchange'), g)
    game = read_json(g + '/game.json')
    if styles:
        game['styles'] = STYLES
    write_json(g + '/game.json', game)
    if tweak:
        tweak(g)
    return g


def unit_of(m, uid):
    return [u for u in m['units'] if u.get('id') == uid][0]


def front(m, uid):
    """The unit moves to the front of the map's units: the cursor starts on it."""
    u = unit_of(m, uid)
    m['units'].remove(u)
    m['units'].insert(0, u)


def tweak_map(map_id, fn):
    return lambda g: edit(g, 'maps/%s.json' % map_id, fn)


def run(gid, inp, map_id, style=None, shot=None, interval=4):
    """Runs the player on a fresh battle; returns (code, log, the Continue save's battle)."""
    work = os.path.join(TMP, 'saves_%s_%s' % (gid, map_id))
    shutil.rmtree(work, ignore_errors=True)
    cmd = player_cmd(PLAYER, TMP, REPO) + ['--saves', work, '--frames', str(ticks(inp) * interval + 90), '--input-interval', str(interval),
           '--fixed-dt', '0.016', '--game', gid, '--map', map_id, '--input', inp]
    if style:
        cmd += ['--style', style, '--3d', 'software']
    if shot:
        cmd += ['--screenshot', shot]
    p = run_text(cmd)
    battle = {}
    try:
        battle = read_json(os.path.join(work, gid, 'continue.json'))['battle']
    except (OSError, ValueError, KeyError):
        pass
    return p.returncode, p.stdout, battle


class Image(Bmp):
    def brightness(self):
        return sum(p[0] + p[1] + p[2] for row in self.px for p in row)

    def near(self, color, tol=40):
        """How many pixels are within `tol` (per channel) of the colour."""
        return sum(1 for row in self.px for p in row
                   if abs(p[0] - color[0]) <= tol and abs(p[1] - color[1]) <= tol and abs(p[2] - color[2]) <= tol)

    def differs(self, other, margin=30):
        """How many pixels differ by more than `margin` in some channel."""
        if (self.w, self.h) != (other.w, other.h):
            return -1
        return sum(1 for y in range(self.h) for x in range(self.w)
                   if max(abs(a - b) for a, b in zip(self.px[y][x], other.px[y][x])) > margin)


def shot(name, gid, inp, map_id, style=None):
    path = os.path.join(TMP, name + '.bmp')
    code, log, battle = run(gid, inp, map_id, style, shot=path)
    if SHOTS:
        os.makedirs(SHOTS, exist_ok=True)
        shutil.copy(path, os.path.join(SHOTS, name + '.bmp'))
    return Image(path), log, battle


RIVER, BRIDGE = (61, 133, 198), (160, 82, 45)
LIT_PLAIN = (156, 204, 101)  # plain ground that is in sight (unseen squares are drawn darker)


def test_bridge(style, label):
    """Round 2's event applies the change "bridge": the river square at 4,1 becomes a bridge."""
    def no_events(m):
        m['events'] = []
    make_game('bridge_before_' + label, tweak_map('bridge', no_events), styles=bool(style))
    make_game('bridge_after_' + label, None, styles=bool(style))
    before, blog, bbat = shot('bridge_%s_before' % label, 'bridge_before_' + label, inp(END_TURN), 'bridge', style)
    after, alog, abat = shot('bridge_%s_after' % label, 'bridge_after_' + label, inp(END_TURN), 'bridge', style)
    check('bridge %s: round 2 began in both runs' % label, bool(lines(blog, 'turn: blue, round 2')) and bool(lines(alog, 'turn: blue, round 2')), alog[-400:])
    check('bridge %s: without the event nothing changed' % label, not lines(blog, 'map changed'), str(lines(blog, 'map changed')))
    check('bridge %s: the log says one square became a bridge' % label, bool(lines(alog, 'map changed: 1 square(s) -> Bridge')), str(lines(alog, 'map')))
    state = abat.get('mapState', {})
    check('bridge %s: the saved battle records the square and the change' % label,
          state.get('terrain') == [[13, 3]] and [a['change'] for a in state.get('applied', [])] == ['bridge'], str(state))
    check('bridge %s: a battle that did not change the map has no mapState' % label, 'mapState' not in bbat, str(list(bbat)))
    diff = before.differs(after)
    check('bridge %s: the picture changed where the square is' % label, diff > 60, str(diff))
    if not style:  # the 2D placeholder colours are exact
        check('bridge %s: less river and more bridge after the change' % label,
              after.near(RIVER) < before.near(RIVER) and after.near(BRIDGE) > before.near(BRIDGE),
              '%d/%d %d/%d' % (before.near(RIVER), after.near(RIVER), before.near(BRIDGE), after.near(BRIDGE)))


def test_wall():
    """The wall (hp 11) breaks with the hero's one hit (11 damage); the opening applies the change "rubble"; the red raider behind it
    (moved to 4,1) comes into the hero's sight."""
    def tw(g):
        edit(g, 'data/structures.json', lambda s: s['structures'][0]['door'].update(hp=11))
        def fn(m):
            front(m, 'hero')
            unit_of(m, 'r1')['y'] = 1
        edit(g, 'maps/cracked.json', fn)
    make_game('wall', tw)
    before, blog, bbat = shot('wall_before', 'wall', inp(IDLE, 'wait:10'), 'cracked')
    after, alog, abat = shot('wall_after', 'wall', inp(MENU, 'confirm', 'wait:3', 'confirm', 'wait:60'), 'cracked')
    check('wall: the menu says Break wall and has no greyed Open', bool(lines(alog, 'place entries of Hero: Break wall')) and not lines(alog, 'Open ['), str(lines(alog, 'place entries')))
    check('wall: the pick asks for a wall', bool(lines(alog, 'place hint: Pick a wall to break')), str(lines(alog, 'hint')))
    check('wall: the forecast says damage, hit points and Breaks', bool(lines(alog, 'door forecast: Cracked Wall  Iron Sword; Damage 11; HP 11 > 0; Breaks;')), str(lines(alog, 'forecast')))
    check('wall: it breaks as one piece and its event changes the map', bool(lines(alog, 'opened: Cracked Wall (w1) by Hero')) and bool(lines(alog, 'map changed: 3 square(s) -> Rubble')), alog[-500:])
    kinds = [s['id'] for s in read_json(os.path.join(TMP, 'wall', 'data/structures.json'))['structures']]
    st = abat.get('structures', [{}])[0]
    check('wall: the saved wall is the broken kind and three squares are rubble', kinds[st.get('type', 0)] == 'crack_open'
          and len(abat.get('mapState', {}).get('terrain', [])) == 3, str(st) + str(abat.get('mapState')))
    check('wall: the room behind it is in sight now (more lit ground in the picture)', after.near(LIT_PLAIN, 16) > before.near(LIT_PLAIN, 16) + 400,
          '%d vs %d' % (before.near(LIT_PLAIN, 16), after.near(LIT_PLAIN, 16)))
    check('wall: the log of the flash names the squares', bool(lines(alog, 'map changed: 3 square(s)')), '')


def test_fog_lifts():
    """Round 2 lifts the fog (the map gets brighter), then a second map brings it down in round 2 on a map that had none."""
    make_game('fog_lift', tweak_map('dark', lambda m: m.update(events=[{'id': 'e_lift', 'when': {'turn': 2}, 'do': [{'setFog': False}]}])))
    plain, plog, pbat = shot('fog_start', 'fog_lift', inp(IDLE, 'wait:10'), 'dark')
    lifted, llog, lbat = shot('fog_lifted', 'fog_lift', inp(END_TURN), 'dark')
    check('fog: the log says the fog lifts', bool(lines(llog, 'fog: lifted')), str(lines(llog, 'fog')))
    check('fog: the whole map shows at once (the picture is much brighter)', lifted.brightness() > plain.brightness() * 1.15, '%d vs %d' % (plain.brightness(), lifted.brightness()))
    check('fog: the saved battle says so', lbat.get('mapState', {}).get('fog') is False and 'seenOwners' not in lbat, str(lbat.get('mapState')))

    def fall(m):
        m['fog'] = False
        m['events'] = [{'id': 'e_dark', 'when': {'turn': 2}, 'do': [{'setFog': True}]}]
    make_game('fog_fall', tweak_map('dark', fall))
    bright, blog, bbat = shot('nofog_start', 'fog_fall', inp(IDLE, 'wait:10'), 'dark')
    dark, dlog, dbat = shot('fog_fell', 'fog_fall', inp(END_TURN), 'dark')
    check('fog: a map without fog gets it by an event', bool(lines(dlog, 'fog: came down')) and bool(lines(dlog, 'fog: viewer blue')), str(lines(dlog, 'fog')))
    check('fog: the picture is darker once it is down', dark.brightness() < bright.brightness() * 0.9, '%d vs %d' % (bright.brightness(), dark.brightness()))
    check('fog: the saved battle has the fog and the structures\' owners', dbat.get('mapState', {}).get('fog') is True and 'seenOwners' in dbat, str(dbat.get('mapState')))


def test_torch():
    """The hero uses his torch: the squares around him stay lit (log, picture)."""
    def tw(m):
        m['events'] = []
        front(m, 'hero')
    make_game('torch', tweak_map('dark', tw))
    before, blog, bbat = shot('torch_before', 'torch', inp(IDLE, 'wait:10'), 'dark')
    after, alog, abat = shot('torch_after', 'torch', inp(MENU, 'confirm', 'wait:3', 'next', 'wait:3', 'confirm', 'wait:3', 'confirm', 'wait:40'), 'dark')
    check('torch: the item menu offers it and the use lights the square', bool(lines(alog, 'item menu: Torch')) and bool(lines(alog, 'staff lit: square 29 radius 3 for 2 turns')), str(lines(alog, 'item')) + str(lines(alog, 'lit')))
    check('torch: the light is in the saved battle', [l['radius'] for l in abat.get('lights', [])] == [3], str(abat.get('lights')))
    check('torch: more ground is lit than the hero alone sees (the ring the torch adds)', after.near(LIT_PLAIN, 16) > before.near(LIT_PLAIN, 16) + 400,
          '%d vs %d' % (before.near(LIT_PLAIN, 16), after.near(LIT_PLAIN, 16)))


def main():
    test_bridge(None, '2d')
    test_bridge('iso3', 'iso3d')
    test_wall()
    test_fog_lifts()
    test_torch()
    summary()
    shutil.rmtree(TMP, ignore_errors=True)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
