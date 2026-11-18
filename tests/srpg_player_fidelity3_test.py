#!/usr/bin/env python3
"""Fidelity 3 through the real player, windowless (docs/SRPG.md "An ability that spends its item", docs/EVENTS.md "Several shops",
docs/ECONOMY.md "Weather that closes a way", docs/SOUND.md "Shop music ... promotion sounds").

  python3 tests/srpg_player_fidelity3_test.py ./build/engine/player/ote-player .
  python3 tests/srpg_player_fidelity3_test.py ./build/engine/player/ote-player . --shots OUT_DIR     # keep screenshots (BMP)

Writes small games into a temp folder (the repository is never changed) and plays them with --headless, --no-settings and no saving (or saves in
the temp folder); every run is silent: the audio runs only write the --audio-log (nothing is mixed, no device is opened). Asserted:
  * an ability that spends its item: a unit with a three-use wand heals three times (the player's log), the item is gone after the third and
    the ability is not used again;
  * two named preparation shops entered in turn: the menu lists them by name, each shop's music plays while it is open (the audio log), the
    game's shopMusic plays for a shop with none, and the preparation music comes back each time;
  * a level-up asks for one `levelUpTick` sound for each stat that rose (three stats, then one);
  * the boot jingle plays before the title music, once; the `save` sound plays when a slot is saved from a menu; the promotion sounds play at
    their times next to the promotion jingle;
  * weather that closes a way: a computer unit cannot cross a swamp that the rain closes, crosses it when the story clears the rain, and the
    reach shown on the preparation screen's map view shrinks;
  * screenshots of the preparation menu with two shops at 480 x 270 and 270 x 480 (with --shots).
"""
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import wave

sys.dont_write_bytecode = True

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import sound_game  # noqa: E402

PLAYER = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else ''
REPO = os.path.abspath(sys.argv[2]) if len(sys.argv) > 2 else '.'
SHOTS = os.path.abspath(sys.argv[sys.argv.index('--shots') + 1]) if '--shots' in sys.argv else ''
TMP = tempfile.mkdtemp(prefix='ote-fid3-')
passed, failures = 0, []


def read_json(path):
    with open(path) as f:
        return json.loads(''.join(l for l in f if not l.lstrip().startswith('//')))


def write_json(path, obj):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w') as f:
        json.dump(obj, f, indent=1)


def edit(g, rel, fn):
    path = os.path.join(g, rel)
    obj = read_json(path)
    fn(obj)
    write_json(path, obj)


def check(name, cond, detail=''):
    global passed
    if cond:
        passed += 1
        print('PASS ' + name)
    else:
        failures.append(name)
        print('FAIL %s %s' % (name, detail))


def wav(path, ms=120, freq=440):
    """A tiny valid WAV (22050 Hz, mono, 16 bit): the player opens and checks it in a headless run, and logs it by name."""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    n = 22050 * ms // 1000
    with wave.open(path, 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(22050)
        w.writeframes(b''.join(struct.pack('<h', int(3000 * ((i * freq // 22050) % 2 * 2 - 1))) for i in range(n)))


def add_audio(g, music, sounds, use):
    """data/audio.json with these music ids (loop true unless listed in `once`) and sound ids; files are made."""
    mus, snd = {}, {}
    for name in music:
        once = name.startswith('j_')
        mus[name] = {'file': 'audio/%s.wav' % name}
        if once:
            mus[name]['loop'] = False
        wav(os.path.join(g, 'audio', name + '.wav'), 200)
    for name in sounds:
        snd[name] = {'file': 'audio/%s.wav' % name}
        wav(os.path.join(g, 'audio', name + '.wav'), 60)
    write_json(os.path.join(g, 'data', 'audio.json'), {'music': mus, 'sounds': snd, 'use': use})


def keep(path, name):
    if SHOTS and os.path.exists(path):
        os.makedirs(SHOTS, exist_ok=True)
        shutil.copy(path, os.path.join(SHOTS, name + '.bmp'))


def player(args, games=None, audio_log=None, saves=None, frames=0, inp=None, interval=3, shot=None, env=None):
    cmd = [PLAYER, '--games', games or TMP, '--assets', os.path.join(REPO, 'assets'), '--headless', '--no-settings', '--fixed-dt', '0.016']
    cmd += ['--saves', saves] if saves else ['--no-saving']
    if audio_log:
        if os.path.exists(audio_log):
            os.remove(audio_log)
        cmd += ['--audio-log', audio_log]
    else:
        cmd += ['--no-audio']
    if frames:
        cmd += ['--frames', str(frames)]
    if inp:
        cmd += ['--input', inp, '--input-interval', str(interval)]
    if shot:
        cmd += ['--screenshot', shot]
    e = dict(os.environ)
    e.update(env or {})
    p = subprocess.run(cmd + args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True, timeout=300, env=e)
    return p.stdout


def audio_tokens(path):
    """The audio log as short tokens: 'M:camp' for a music start, 'M-' for a stop, 'S:tick' for a sound, in time order (with the time)."""
    out = []
    if not os.path.exists(path):
        return out
    for l in open(path).read().splitlines():
        m = re.match(r't=([\d.]+) music play (\S+)', l)
        if m:
            out.append((float(m.group(1)), 'M:' + os.path.splitext(os.path.basename(m.group(2)))[0]))
            continue
        m = re.match(r't=([\d.]+) music stop', l)
        if m:
            out.append((float(m.group(1)), 'M-'))
            continue
        m = re.match(r't=([\d.]+) sound (\S+)', l)
        if m:
            out.append((float(m.group(1)), 'S:' + os.path.splitext(os.path.basename(m.group(2)))[0]))
    return out


def names(tokens):
    return [t for _, t in tokens]


def infos(log, key):
    return [l.split('] ', 1)[-1] for l in log.splitlines() if key in l]


# ---- 1. an ability that spends its item -------------------------------------------------------------------------------

def make_wand_game(gid):
    g = os.path.join(TMP, gid)
    write_json(g + '/game.json', {
        'formatVersion': 1, 'name': 'Wand', 'view': {'width': 480, 'height': 270, 'tileSize': 24},
        'stats': [{'id': 'hp', 'name': 'HP'}, {'id': 'atk', 'name': 'Attack'}, {'id': 'def', 'name': 'Defense'}, {'id': 'mov', 'name': 'Move'}],
        'roles': {'hp': 'hp', 'move': 'mov'}, 'moveTypes': [{'id': 'foot', 'name': 'Foot'}],
        'rules': {'defeat': ['noUnits'], 'counterAttacks': True, 'items': {'weapons': 'carried', 'inventorySize': 4}},
        'formulas': {'damage': 'a.atk - b.def', 'hit': '100'}, 'maps': [{'id': 'ward', 'name': 'Ward', 'file': 'maps/ward.json'}]})
    write_json(g + '/data/terrain.json', {'terrain': [{'id': 'plain', 'name': 'Plain'}]})
    write_json(g + '/data/weapons.json', {'weapons': [{'id': 'stick', 'name': 'Stick', 'group': 'blunt'}]})
    write_json(g + '/data/skills.json', {'skills': [{'id': 'zapper', 'name': 'Zapper', 'blocks': [{'when': 'always', 'do': [{'ability': 'zap'}]}]}]})
    write_json(g + '/data/abilities.json', {'abilities': [
        {'id': 'zap', 'name': 'Zap', 'target': 'ally', 'range': [1, 1], 'effect': {'heal': 5}, 'cost': 'action', 'spendsItem': True}]})
    write_json(g + '/data/items.json', {'items': [
        {'id': 'stick_item', 'name': 'Stick', 'kind': 'weapon', 'weapon': 'stick'},
        {'id': 'wand', 'name': 'Wand', 'kind': 'accessory', 'uses': 3, 'skills': ['zapper']}]})
    write_json(g + '/data/units.json', {'units': [
        {'id': 'cleric', 'name': 'Cleric', 'moveType': 'foot', 'wields': ['blunt'], 'items': ['stick_item', 'wand'], 'stats': {'hp': 20, 'atk': 2, 'def': 0, 'mov': 0}},
        {'id': 'pal', 'name': 'Pal', 'moveType': 'foot', 'wields': ['blunt'], 'items': ['stick_item'], 'stats': {'hp': 30, 'atk': 2, 'def': 0, 'mov': 0}},
        {'id': 'bandit', 'name': 'Bandit', 'moveType': 'foot', 'wields': ['blunt'], 'items': ['stick_item'], 'stats': {'hp': 30, 'atk': 30, 'def': 0, 'mov': 0}}]})
    write_json(g + '/maps/ward.json', {
        'name': 'Ward', 'grid': {'type': 'square', 'width': 9, 'height': 3}, 'legend': {'.': 'plain'}, 'tiles': ['.' * 9] * 3,
        'sides': [{'id': 'blue', 'controller': 'ai'}, {'id': 'red', 'controller': 'ai'}],
        'units': [{'type': 'cleric', 'id': 'cleric', 'side': 'blue', 'x': 1, 'y': 1, 'leader': True},
                  {'type': 'pal', 'id': 'pal', 'side': 'blue', 'x': 0, 'y': 1},
                  {'type': 'bandit', 'id': 'bandit', 'side': 'red', 'x': 8, 'y': 1}],
        'events': [{'id': 'hurt', 'when': {'battle': 'start'},
                    'do': [{'fight': {'attacker': 'bandit', 'defender': 'pal', 'strikes': [{'by': 'attacker', 'result': 'hit', 'damage': 29}], 'lethal': False}}]}]})
    return g


def test_spends_item():
    make_wand_game('wand')
    log = player(['--game', 'wand', '--map', 'ward', '--autoplay'], frames=3000)
    uses = infos(log, 'ability: Cleric uses Zap')
    gone = infos(log, "item: Cleric's Wand is gone")
    check('item ability: the three-use wand heals exactly three times and then the ability is not used again', len(uses) == 3, '%s\n%s' % (uses, log[-600:]))
    check('item ability: the item is gone after the third use (log line)', len(gone) == 1, str(gone))
    last_use = [i for i, l in enumerate(log.splitlines()) if 'ability: Cleric uses Zap' in l]
    gone_at = [i for i, l in enumerate(log.splitlines()) if "item: Cleric's Wand is gone" in l]
    check('item ability: the "gone" line comes with the third use, not before', bool(last_use) and bool(gone_at) and gone_at[0] > last_use[2] - 1 and gone_at[0] < last_use[2] + 6,
          '%s %s' % (last_use, gone_at))


# ---- 2. several preparation shops -----------------------------------------------------------------------------------

TO_PREP = 'confirm,confirm,confirm,wait:20,menu,confirm,wait:100,confirm,wait:30,confirm,confirm,confirm,wait:40'


def shops_game(gid, width=480, height=270):
    g = os.path.join(TMP, gid)
    shutil.copytree(os.path.join(REPO, 'tests/fixtures/srpg-prep'), g)
    edit(g, 'game.json', lambda gj: gj.update({'view': {'width': width, 'height': height, 'tileSize': 24}}))

    def orchard(m):
        m['prep'].pop('shop')
        m['prep']['shops'] = [
            {'id': 'armory', 'name': 'Armory', 'sells': ['tonic'], 'music': 'armory'},
            {'id': 'market', 'name': 'Market', 'sells': ['potion', 'ring'], 'buys': False}]
    edit(g, 'maps/orchard.json', orchard)
    add_audio(g, ['camp', 'armory', 'market_theme'], [], {'prepMusic': 'camp', 'shopMusic': 'market_theme'})
    return g


def test_shops():
    g = shops_game('shops')
    # main menu: Units, Positions, Items, Armory, Market, View map, Leave, Begin
    log_a = os.path.join(TMP, 'shops.log')
    inp = TO_PREP + ',down,down,down,confirm,wait:6,cancel,wait:6,down,down,down,down,confirm,wait:6,cancel,wait:6'
    out = player(['--game', 'shops', '--campaign'], audio_log=log_a, frames=1600, inp=inp)
    menu = infos(out, 'prep menu:')
    check('shops: the preparation menu lists each shop by its name', bool(menu) and menu[0].endswith('Units, Positions, Items, Armory, Market, View map, Leave, Begin'), str(menu[:1]))
    shop_lines = infos(out, 'prep shop:')
    check('shops: each shop opens under its own name', len(shop_lines) >= 2 and '"Armory"' in shop_lines[0] and '"Market"' in shop_lines[1], str(shop_lines))
    check('shops: the armory sells one item, the market two', shop_lines[0].split('(')[1].startswith('1)') and shop_lines[1].split('(')[1].startswith('2)'), str(shop_lines))
    seq = names(audio_tokens(log_a))
    tail = [t for t in seq if t.startswith('M:')]
    check('shops: the preparation music, then the armory\'s own, then the preparation music again, then the game\'s shop music for the market, '
          'then the preparation music', tail[-5:] == ['M:camp', 'M:armory', 'M:camp', 'M:market_theme', 'M:camp'], str(tail))
    # a shop with no music and a game with no shopMusic changes nothing
    g2 = shops_game('shops_nodef')
    edit(g2, 'data/audio.json', lambda a: a['use'].pop('shopMusic'))
    log_b = os.path.join(TMP, 'shops2.log')
    player(['--game', 'shops_nodef', '--campaign'], audio_log=log_b, frames=1600, inp=inp)
    tail = [t for t in names(audio_tokens(log_b)) if t.startswith('M:')]
    check('shops: a shop without music, in a game without shopMusic, leaves the preparation music playing', tail[-3:] == ['M:camp', 'M:armory', 'M:camp'], str(tail))


def ticks(script):
    return sum(int(t[5:]) if t.startswith('wait:') else 1 for t in script.split(','))


def test_shop_screenshots():
    for tag, (w, h) in (('land', (480, 270)), ('phone', (270, 480))):
        gid = 'shopshot_' + tag
        shops_game(gid, w, h)
        path = os.path.join(TMP, gid + '.bmp')
        script = TO_PREP + ',wait:20'
        player(['--game', gid, '--campaign'], frames=ticks(script) * 3 + 90, inp=script, shot=path)
        keep(path, 'prep_two_shops_' + tag)
        check('screenshot prep two shops %s exists' % tag, os.path.exists(path))
        path2 = os.path.join(TMP, gid + '_shop.bmp')
        script2 = TO_PREP + ',down,down,down,down,confirm,wait:20'
        player(['--game', gid, '--campaign'], frames=ticks(script2) * 3 + 90, inp=script2, shot=path2)
        keep(path2, 'prep_market_' + tag)


# ---- 3. level-up ticks, boot jingle, save sound, promotion sounds -------------------------------------------------

def levels_game(gid, growths):
    games = os.path.join(TMP, gid + '_games')
    shutil.rmtree(games, ignore_errors=True)
    os.makedirs(games)
    g = sound_game.make_game(games, 'snd', None, None, levels=True, sound_overrides=False)
    edit(g, 'data/units.json', lambda u: [x.update({'growths': growths}) for x in u['units'] if x['id'] == 'knight'])
    add_audio(g, ['title', 'j_boot', 'j_up', 'boss', 'calm'], ['tick', 'save_chime', 'whoosh', 'boom', 'hit_s', 'thunder'],
              {'titleMusic': 'title', 'bootJingle': 'j_boot', 'levelUp': 'j_up',
               'sounds': {'levelUpTick': 'tick', 'save': 'save_chime', 'hit': 'hit_s'}})
    return games, g


def test_level_up_ticks():
    for growths, want in (({'hp': 100, 'atk': 100, 'def': 100}, 3), ({'hp': 100}, 1)):
        games, g = levels_game('lv%d' % want, growths)
        log_a = os.path.join(TMP, 'lv%d.log' % want)
        out = player(['--game', 'snd', '--map', 'levels'], games=games, audio_log=log_a, frames=600, inp='wait:60,confirm,right,confirm,confirm,confirm,wait:200', interval=8)
        ticks = [t for t in audio_tokens(log_a) if t[1] == 'S:tick']
        check('level-up ticks: %d stats rose, %d ticks requested (one each)' % (want, want), len(ticks) == want and len(infos(out, 'level-up tick')) == want, '%s\n%s' % (ticks, out[-400:]))
        if want == 3:
            gaps = [round(b[0] - a[0], 2) for a, b in zip(ticks, ticks[1:])]
            check('level-up ticks: they are spaced out so that each is heard (about 0.12 s apart)', all(0.08 <= x <= 0.2 for x in gaps), str(gaps))
            order = names(audio_tokens(log_a))
            check('level-up ticks: the level-up jingle is requested too', any(t.startswith('M:j_up') for t in order), str(order))
    # a game that does not name the tick sound plays none
    games, g = levels_game('lv_none', {'hp': 100, 'atk': 100})
    edit(g, 'data/audio.json', lambda a: a['use']['sounds'].pop('levelUpTick'))
    log_a = os.path.join(TMP, 'lv_none.log')
    player(['--game', 'snd', '--map', 'levels'], games=games, audio_log=log_a, frames=600, inp='wait:60,confirm,right,confirm,confirm,confirm,wait:200', interval=8)
    check('level-up ticks: no levelUpTick slot, no tick', not [t for t in audio_tokens(log_a) if t[1] == 'S:tick'])


def test_boot_jingle():
    games, g = levels_game('boot', {'hp': 100})
    log_a = os.path.join(TMP, 'boot.log')
    player(['--game', 'snd'], games=games, audio_log=log_a, frames=200)
    seq = [t for t in names(audio_tokens(log_a)) if t.startswith('M:')]
    check('boot jingle: it plays first when the game\'s page opens, then the title music', seq[:2] == ['M:j_boot', 'M:title'], str(seq))
    check('boot jingle: and only once', seq.count('M:j_boot') == 1, str(seq))
    # a game without it starts the title music at once
    edit(g, 'data/audio.json', lambda a: a['use'].pop('bootJingle'))
    player(['--game', 'snd'], games=games, audio_log=log_a, frames=200)
    seq = [t for t in names(audio_tokens(log_a)) if t.startswith('M:')]
    check('boot jingle: no slot, no jingle', seq[:1] == ['M:title'] and 'M:j_boot' not in seq, str(seq))


def test_save_sound():
    # the preparation screen's menu has "Save" (a campaign game with slots): Units, Positions, Items, Shop, View map, Save, ...
    g = shops_game('savesound')
    add_audio(g, ['camp', 'armory'], ['save_chime'], {'prepMusic': 'camp', 'sounds': {'save': 'save_chime'}})
    saves = os.path.join(TMP, 'saves_snd')
    shutil.rmtree(saves, ignore_errors=True)
    log_a = os.path.join(TMP, 'save.log')
    # Units, Positions, Items, Armory, Market, View map, Save: six downs, confirm; the slot list: confirm picks the first empty slot
    out = player(['--game', 'savesound', '--campaign'], audio_log=log_a, saves=saves, frames=1800, inp=TO_PREP + ',down,down,down,down,down,down,confirm,wait:6,confirm,wait:10')
    sounds = [t for t in names(audio_tokens(log_a)) if t == 'S:save_chime']
    check('save sound: the game\'s save sound plays once when a slot is written from the menu', len(sounds) == 1, '%s\n%s' % (sounds, out[-300:]))


def test_promotion_sounds():
    games = os.path.join(TMP, 'promo_games')
    shutil.rmtree(games, ignore_errors=True)
    os.makedirs(games)
    g = sound_game.make_game(games, 'snd', None, None, sound_overrides=False)
    edit(g, 'data/units.json', lambda u: [x.update({'promotions': [{'to': 'rider'}]}) for x in u['units'] if x['id'] == 'knight'])
    edit(g, 'maps/duel.json', lambda m: m.update({'events': [{'id': 'up', 'when': {'battle': 'start'}, 'do': [{'promote': 'hero', 'to': 'rider'}]}],
                                                   'units': [dict(u, id='hero') if u['type'] == 'knight' else u for u in m['units']]}))
    add_audio(g, ['title', 'j_fanfare', 'boss', 'calm'], ['whoosh', 'boom', 'late', 'thunder'],
              {'titleMusic': 'title', 'promotion': 'j_fanfare',
               'promotionSounds': [{'atMs': 900, 'sound': 'late'}, {'atMs': 0, 'sound': 'whoosh'}, {'atMs': 400, 'sound': 'boom'}]})
    log_a = os.path.join(TMP, 'promo.log')
    out = player(['--game', 'snd', '--map', 'duel'], games=games, audio_log=log_a, frames=500, inp='wait:150', interval=8)
    toks = audio_tokens(log_a)
    mine = [(t, n) for t, n in toks if n in ('S:whoosh', 'S:boom', 'S:late') or n.startswith('M:j_fanfare')]
    check('promotion sounds: the jingle and the three sounds, the sounds in time order', [n for _, n in mine if n.startswith('S:')] == ['S:whoosh', 'S:boom', 'S:late'] and any(n.startswith('M:j_fanfare') for _, n in mine), '%s\n%s' % (mine, out[-300:]))
    ws = [t for t, n in mine if n == 'S:whoosh']
    bo = [t for t, n in mine if n == 'S:boom']
    la = [t for t, n in mine if n == 'S:late']
    check('promotion sounds: 0, 400 and 900 ms after the first one', bool(ws and bo and la) and abs((bo[0] - ws[0]) - 0.4) < 0.05 and abs((la[0] - ws[0]) - 0.9) < 0.05,
          '%s %s %s' % (ws, bo, la))


# ---- 4. weather that closes a way -------------------------------------------------------------------------------------

def make_weather_game(gid, map_weather, clears, cost):
    g = os.path.join(TMP, gid)
    write_json(g + '/game.json', {
        'formatVersion': 1, 'name': 'Weather', 'view': {'width': 480, 'height': 270, 'tileSize': 24},
        'stats': [{'id': 'hp', 'name': 'HP'}, {'id': 'atk', 'name': 'Attack'}, {'id': 'def', 'name': 'Defense'}, {'id': 'mov', 'name': 'Move'}],
        'roles': {'hp': 'hp', 'move': 'mov'}, 'moveTypes': [{'id': 'foot', 'name': 'Foot'}],
        'rules': {'defeat': ['noUnits'], 'counterAttacks': True}, 'formulas': {'damage': 'a.atk - b.def', 'hit': '100'},
        'maps': [{'id': 'march', 'name': 'March', 'file': 'maps/march.json'}]})
    write_json(g + '/data/terrain.json', {'terrain': [
        {'id': 'plain', 'name': 'Plain'}, {'id': 'swamp', 'name': 'Swamp', 'moveCost': {'foot': 2}, 'weatherCost': {'rain': {'foot': cost}}}]})
    write_json(g + '/data/weapons.json', {'weapons': [{'id': 'sword', 'name': 'Sword'}]})
    write_json(g + '/data/units.json', {'units': [
        {'id': 'post', 'name': 'Post', 'glyph': 'P', 'moveType': 'foot', 'weapons': ['sword'], 'stats': {'hp': 60, 'atk': 1, 'def': 1, 'mov': 0}},
        {'id': 'raider', 'name': 'Raider', 'glyph': 'R', 'moveType': 'foot', 'weapons': ['sword'], 'stats': {'hp': 10, 'atk': 2, 'def': 1, 'mov': 4}}]})
    events = [
        {'id': 'crossed', 'when': {'reach': {'x': 4, 'y': 0, 'w': 1, 'h': 1}, 'by': {'unit': 'raider'}}, 'do': [{'say': 'raider crossed'}]},
        {'id': 'round2', 'when': {'turn': 2}, 'do': [{'say': 'round two'}] + ([{'weather': 'none'}] if clears else [])}]
    m = {'name': 'March', 'grid': {'type': 'square', 'width': 7, 'height': 1}, 'legend': {'.': 'plain', 's': 'swamp'},
         'tiles': ['....ss.'], 'sides': [{'id': 'blue', 'controller': 'ai'}, {'id': 'red', 'controller': 'ai'}],
         'units': [{'type': 'post', 'id': 'post', 'side': 'blue', 'x': 0, 'y': 0}, {'type': 'raider', 'id': 'raider', 'side': 'red', 'x': 6, 'y': 0}],
         'events': events}
    if map_weather:
        m['weather'] = map_weather
    write_json(g + '/maps/march.json', m)
    return g


def test_weather_closes_a_way():
    def lines_of(gid, weather, clears, cost='blocked'):
        make_weather_game(gid, weather, clears, cost)
        log = player(['--game', gid, '--map', 'march', '--autoplay'], frames=4000)
        return [re.sub(r'^.*dialogue: ', '', l).strip(' |') for l in log.splitlines() if 'dialogue:' in l], log
    clear, log0 = lines_of('wb_clear', None, False)
    check('weather closes a way: in clear weather the raider crosses the swamp on its first turn', 'raider crossed' in clear and clear.index('raider crossed') < clear.index('round two'), '%s %s' % (clear, log0[-300:]))
    rain, log1 = lines_of('wb_rain', 'rain', True)
    check('weather closes a way: in the map\'s rain the swamp is closed, the raider cannot enter it; it crosses after the story clears the rain',
          'round two' in rain and 'raider crossed' in rain and rain.index('round two') < rain.index('raider crossed'), '%s %s' % (rain, log1[-300:]))
    stuck, log2 = lines_of('wb_stuck', 'rain', False)
    check('weather closes a way: while the rain lasts it never crosses', 'round two' in stuck and 'raider crossed' not in stuck, '%s %s' % (stuck, log2[-300:]))


def test_weather_reach_display():
    """The preparation screen's map view shows an enemy's reach: the squares a closed swamp takes away are not in it."""
    def reach(gid, weather):
        g = os.path.join(TMP, gid)
        shutil.copytree(os.path.join(REPO, 'tests/fixtures/srpg-prep'), g)
        edit(g, 'data/terrain.json', lambda t: t['terrain'].append({'id': 'bog', 'name': 'Bog', 'weatherCost': {'rain': {'foot': 'blocked'}}}))

        def orchard(m):
            m['legend']['b'] = 'bog'
            rows = [list(r) for r in m['tiles']]
            for y in range(0, 3):
                rows[y][5] = 'b'  # a wall of bog west of the raider at (7,1)
                rows[y][6] = 'b'
            m['tiles'] = [''.join(r) for r in rows]
            if weather:
                m['weather'] = weather
        edit(g, 'maps/orchard.json', orchard)
        out = player(['--game', gid, '--campaign'], frames=1500, inp=TO_PREP + ',down,down,down,down,confirm,' + ','.join(['right'] * 7) + ',wait:6')
        found = infos(out, 'prep view: Raider can move to')
        return int(re.search(r'move to (\d+) squares', found[-1]).group(1)) if found else None
    dry, rain = reach('wr_dry', None), reach('wr_rain', 'rain')
    check('weather closes a way: the reach shown on the map view is smaller in the rain (%s -> %s squares)' % (dry, rain), dry is not None and rain is not None and rain < dry, '%s %s' % (dry, rain))


TESTS = [test_spends_item, test_shops, test_shop_screenshots, test_level_up_ticks, test_boot_jingle, test_save_sound, test_promotion_sounds,
         test_weather_closes_a_way, test_weather_reach_display]

if __name__ == '__main__':
    only = [a for a in sys.argv[3:] if a.startswith('test_')]
    for t in TESTS:
        if not only or t.__name__ in only:
            t()
    shutil.rmtree(TMP, ignore_errors=True)
    print('%d passed, %d failed' % (passed, len(failures)))
    for f in failures:
        print('FAILED: ' + f)
    sys.exit(1 if failures else 0)
