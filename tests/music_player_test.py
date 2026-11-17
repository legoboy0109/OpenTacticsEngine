#!/usr/bin/env python3
"""Music by situation and the game page's difficulty row through the real player, windowless and without a sound device.

  python3 tests/music_player_test.py ./build/engine/player/ote-player .
  python3 tests/music_player_test.py ./build/engine/player/ote-player . --show    # also print every sequence

Every run is --headless (no window, no sound device) with --audio-log, which writes one line per music change (the same way
tests/sound_player_test.py reads them); the tracks are tiny synthesized tones (tools/audio/make_music_test_audio.py) that are never
played. Checked: phase music per side id (a game default and a map's own, with a third side), "fewEnemies" (decided when a
human phase starts, never mid-phase), the setPhaseMusic story command (changes at once in that side's phase, is in the Continue save
and comes back after Continue), the promotion jingle, the chapter title card jingle and the preparation screen's music.
"""
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.dont_write_bytecode = True
import sound_game  # noqa: E402
import srpg_promo_games as promo_games  # noqa: E402

PLAYER = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else ''
REPO = os.path.abspath(sys.argv[2]) if len(sys.argv) > 2 else '.'
SHOW = '--show' in sys.argv
failures = []
passed = 0
TMP = tempfile.mkdtemp(prefix='ote-music-')

TRACKS = ['battle', 'boss', 'red_theme', 'green_theme', 'few', 'bossnow', 'promo', 'card', 'prep', 'thunder']
JINGLES = ('promo', 'card')


def check(name, ok, detail=''):
    global passed
    if ok:
        passed += 1
        print('PASS ' + name)
    else:
        failures.append(name)
        print('FAIL %s %s' % (name, detail))


def make_audio(root):
    """root/audio/*.wav (the tones) and the text of a data/audio.json that names them."""
    out = os.path.join(root, 'audio')
    subprocess.run([sys.executable, '-I', os.path.join(REPO, 'tools', 'audio', 'make_music_test_audio.py'), out] + TRACKS, check=True)
    return out


def audio_json(use, extra_music=None):
    music = {n: {'file': 'audio/%s.wav' % n, **({'loop': False} if n in JINGLES else {})} for n in TRACKS if n != 'thunder'}
    music.update(extra_music or {})
    return json.dumps({'music': music, 'sounds': {'thunder': {'file': 'audio/thunder.wav'}}, 'use': use}, indent=1)


def token(line):
    m = re.match(r't=[\d.]+ music play (\S+) fade=([\d.]+) loop=(\d)', line)
    if m:
        s = 'M:' + os.path.splitext(os.path.basename(m.group(1)))[0]
        if m.group(3) == '0':
            s += ' once'
        f = float(m.group(2))
        return s + ('' if f == 0.5 else ' cut' if f == 0 else ' f=%g' % f)
    m = re.match(r't=[\d.]+ music stop fade=([\d.]+)', line)
    if m:
        return 'M-'
    return None


def play(games, game, args, inp, frames=0, saves=None, interval=8):
    log = os.path.join(TMP, 'audio.log')
    if os.path.exists(log):
        os.remove(log)
    cmd = [PLAYER, '--headless', '--no-settings', '--games', games, '--assets', os.path.join(REPO, 'assets'), '--audio-log', log,
           '--input-interval', str(interval), '--game', game]
    cmd += ['--saves', saves] if saves else ['--no-saving']
    if frames:
        cmd += ['--frames', str(frames)]
    if inp:
        cmd += ['--input', inp]
    p = subprocess.run(cmd + args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True, timeout=300)
    raw = open(log).read().splitlines() if os.path.exists(log) else []
    return p.returncode, p.stdout, [t for t in (token(l) for l in raw) if t]


def shown(name, seq):
    if SHOW:
        print('%-18s %s' % (name, ' '.join(seq)))


def write_json(path, obj):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    json.dump(obj, open(path, 'w'), indent=1)


def add_maps(g, maps):
    """Adds maps (name -> map dict) to the sound_game game in `g`."""
    game = json.load(open(g + '/game.json'))
    for name, m in maps.items():
        write_json(g + '/maps/%s.json' % name, m)
        game['maps'].append({'id': name, 'name': name.capitalize(), 'file': 'maps/%s.json' % name})
    write_json(g + '/game.json', game)


def unit(type_, side, x, y):
    return {'type': type_, 'side': side, 'x': x, 'y': y}


def sides(third=False):
    s = [{'id': 'blue', 'name': 'Blue Army', 'color': '#3d85c6', 'controller': 'human', 'resources': {'gold': 0}},
         {'id': 'red', 'name': 'Red Army', 'color': '#cc0000', 'controller': 'ai', 'resources': {'gold': 0}}]
    if third:
        s.append({'id': 'green', 'name': 'Green Army', 'color': '#00aa44', 'controller': 'ai', 'resources': {'gold': 0}})
    return s


def base_map(name, units, third=False, **extra):
    m = {'name': name, 'grid': {'type': 'square', 'width': 8, 'height': 3}, 'legend': {'.': 'plain'}, 'tiles': ['........'] * 3,
         'sides': sides(third), 'units': units}
    m.update(extra)
    return m


OPEN = 'wait:60,'
END_TURN = 'menu,confirm,wait:200'
KILL = 'confirm,right,confirm,confirm,confirm,wait:100'  # the knight walks next to the grunt and defeats it (as in tests/sound_player_test.py)


def run_situations():
    games = os.path.join(TMP, 'games')
    audio = make_audio(os.path.join(TMP, 'tones'))
    use = {'mapMusic': 'battle', 'promotion': 'promo', 'chapterCard': 'card', 'prepMusic': 'prep', 'phaseMusic': {'red': 'red_theme'}}
    g = sound_game.make_game(games, 'snd', os.path.dirname(audio), audio_json(use), sound_overrides=False)
    tough = unit('tough', 'red', 7, 1)
    add_maps(g, {
        'three': base_map('Three', [unit('knight', 'blue', 1, 1), tough, unit('tough', 'green', 7, 2)], third=True,
                          phaseMusic={'green': 'green_theme'}),
        'few': base_map('Few', [unit('knight', 'blue', 1, 1), unit('grunt', 'red', 3, 1), tough], fewEnemies={'music': 'few', 'count': 1},
                        phaseMusic={'red': ''}),
        'shift': base_map('Shift', [unit('knight', 'blue', 1, 1), tough], events=[
            {'id': 'shift', 'when': {'turn': 2}, 'do': [{'setPhaseMusic': {'side': 'blue', 'music': 'bossnow'}}]},
            {'id': 'red2', 'when': {'turn': 3}, 'do': [{'setPhaseMusic': {'side': 'red', 'music': 'boss'}}, {'setPhaseMusic': {'side': 'blue', 'music': None}}]}]),
    })

    # 1. Per side id: red has the game's phase music, green its map's, blue none (the map's music).
    code, out, seq = play(games, 'snd', ['--map', 'three'], OPEN + END_TURN)
    shown('three', seq)
    check('three sides: the run is clean', code == 0 and '[ERROR]' not in out and '[WARN]' not in out, out[-400:])
    check('three sides: red\'s phase plays the game\'s red music, green\'s the map\'s, blue\'s the map music again',
          seq[:4] == ['M:battle', 'M:red_theme', 'M:green_theme', 'M:battle'], str(seq))

    # 2. Few enemies: decided when a human phase starts. Killing the grunt (2 enemies -> 1) changes nothing during that phase;
    #    the player's next phase starts with the track. The map's "phaseMusic": {"red": ""} = nothing of its own for red.
    code, out, seq = play(games, 'snd', ['--map', 'few'], OPEN + KILL)
    shown('few (mid-phase)', seq)
    check('few enemies: defeating the unit that makes them few does not switch the music mid-phase', seq == ['M:battle'], str(seq))
    code, out, seq = play(games, 'snd', ['--map', 'few'], OPEN + KILL + ',' + END_TURN)
    shown('few (next phase)', seq)
    check('few enemies: the next player phase starts with the few-enemies track', seq == ['M:battle', 'M:few'], str(seq))
    code, out, seq = play(games, 'snd', ['--map', 'few'], OPEN + END_TURN)
    shown('few (not few)', seq)
    check('few enemies: with two enemies left the usual music goes on', seq == ['M:battle'], str(seq))

    # 3. setPhaseMusic: at the start of round 2 (blue's phase) blue's music changes at once; it is in the Continue save and the
    #    resumed battle plays it.
    saves = os.path.join(TMP, 'saves')
    code, out, seq = play(games, 'snd', ['--map', 'shift'], OPEN + END_TURN + ',wait:60', saves=saves)
    shown('shift', seq)
    # (the turn's banner plays before the event: blue's phase starts with the map music for a moment, then the override takes over)
    check('setPhaseMusic: the side\'s music changes when the event runs in its phase', seq == ['M:battle', 'M:red_theme', 'M:battle', 'M:bossnow'], str(seq))
    path = os.path.join(saves, 'snd', 'continue.json')
    save = json.load(open(path)) if os.path.exists(path) else None
    check('setPhaseMusic: the override is saved with the battle', save and save['battle']['story'].get('phaseMusic') == {'blue': 'bossnow'},
          json.dumps(save)[:300] if save else 'no save')
    code, out, seq = play(games, 'snd', ['--continue'], 'wait:20', saves=saves)
    shown('shift (continue)', seq)
    check('setPhaseMusic: a resumed battle plays the saved override', seq[:1] == ['M:bossnow'], str(seq))
    # (round 3: the second event gives red a track and takes blue's change away)
    code, out, seq = play(games, 'snd', ['--map', 'shift'], OPEN + END_TURN + ',' + END_TURN + ',' + END_TURN + ',wait:60')
    shown('shift (round 3, 4)', seq)
    check('setPhaseMusic: null takes the change away (blue is back to the map music), another side\'s change waits for its phase',
          seq[3:9] == ['M:bossnow', 'M:red_theme', 'M:bossnow', 'M:battle', 'M:boss', 'M:battle'], str(seq))

    # 4. Promotion jingle (a story promotion in the promotion test game).
    pgames = os.path.join(TMP, 'promo-games')
    pg = promo_games.write_game(pgames)
    ptones = make_audio(os.path.join(TMP, 'ptones'))
    shutil.copytree(ptones, os.path.join(pg, 'audio'))
    open(os.path.join(pg, 'data', 'audio.json'), 'w').write(audio_json({'mapMusic': 'battle', 'promotion': 'promo'}))
    code, out, seq = play(pgames, 'promo', ['--map', 'story'], 'wait:60', frames=400)
    shown('promotion', seq)
    check('promotion jingle: it plays once, on top of the map music', seq[:2] == ['M:battle', 'M:promo once cut'], str(seq) + out[-300:])

    # 5. The chapter title card's jingle and the preparation screen's music (the srpg-prep fixture, chapter 2).
    cg = os.path.join(TMP, 'campaign-games')
    shutil.copytree(os.path.join(REPO, 'tests', 'fixtures', 'srpg-prep'), os.path.join(cg, 'prep'))
    shutil.copytree(os.path.join(ptones), os.path.join(cg, 'prep', 'audio'))
    open(os.path.join(cg, 'prep', 'data', 'audio.json'), 'w').write(audio_json({'mapMusic': 'battle', 'chapterCard': 'card', 'prepMusic': 'prep'}))
    to_prep = 'confirm,confirm,confirm,wait:20,menu,confirm,wait:100,confirm,wait:30,confirm,confirm,confirm,wait:40'
    code, out, seq = play(cg, 'prep', ['--campaign'], to_prep, interval=3)
    shown('prep', seq)
    check('chapter card: each chapter\'s title card plays the jingle', seq.count('M:card once cut') >= 2 and seq[0] == 'M:card once cut', str(seq))
    check('preparation: the preparation screen plays its music', seq[-1:] == ['M:prep'], str(seq) + out[-300:])
    code, out, seq = play(cg, 'prep', ['--campaign'], to_prep + ',' + ','.join(['down'] * 6 + ['confirm']) + ',wait:40', interval=3)
    shown('prep (begin)', seq)
    check('preparation: beginning the battle goes back to the map music', seq[-2:] == ['M:prep', 'M:battle'], str(seq))


run_situations()
print('%d passed, %d failed' % (passed, len(failures)))
shutil.rmtree(TMP, ignore_errors=True)
sys.exit(1 if failures else 0)
