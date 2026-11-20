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
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # the shared kit sits beside the scripts (also under `python -I`)
from ote_testkit import run_text, check, failures, player_and_repo, summary, write_json  # noqa: E402
from ote_testkit import scratch_dir  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.dont_write_bytecode = True
import sound_game  # noqa: E402
import srpg_promo_games as promo_games  # noqa: E402

PLAYER, REPO = player_and_repo()
SHOW = '--show' in sys.argv
TMP = scratch_dir()

TRACKS = ['battle', 'boss', 'red_theme', 'green_theme', 'few', 'bossnow', 'promo', 'card', 'prep', 'thunder']
JINGLES = ('promo', 'card')


def make_audio(root):
    """root/audio/*.wav (the tones) and the text of a data/audio.json that names them."""
    out = os.path.join(root, 'audio')
    subprocess.run([sys.executable, '-I', '-B', os.path.join(REPO, 'tools', 'audio', 'make_music_test_audio.py'), out] + TRACKS, check=True)
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
    p = run_text(cmd + args)
    raw = open(log).read().splitlines() if os.path.exists(log) else []
    return p.returncode, p.stdout, [t for t in (token(l) for l in raw) if t]


def shown(name, seq):
    if SHOW:
        print('%-18s %s' % (name, ' '.join(seq)))


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


def read_save(saves, game):
    path = os.path.join(saves, game, 'continue.json')
    return json.load(open(path)) if os.path.exists(path) else None


def run_game_page_difficulty():
    """The game page's Difficulty row (docs/DIFFICULTY.md section 8), with input tokens. The games are the difficulty test's."""
    dg = os.path.join(TMP, 'dgames')
    subprocess.run([sys.executable, '-I', '-B', os.path.join(HERE, 'difficulty_player_test.py'), '--make-games', dg, REPO], check=True,
                   stdout=subprocess.DEVNULL)
    CARD = 'confirm,skip,wait:20'
    END = 'menu,confirm,wait:60'
    ch1 = CARD + ',' + END + ',confirm,wait:5'
    to_page_row = 'down,down,down'  # Continue, New game, Load, Difficulty

    def run(game, args, inp, saves, frames):
        cmd = [PLAYER, '--games', dg, '--game', game, '--headless', '--no-audio', '--no-settings', '--saves', saves,
               '--input-interval', '3', '--fixed-dt', '0.016', '--frames', str(frames)] + args
        if inp:
            cmd += ['--input', inp]
        p = run_text(cmd)
        return p.returncode, p.stdout

    # lowerOnly: Hard -> the list offers Easy and Normal; Easy is picked; only the playthrough's difficulty changes in the save.
    sv = os.path.join(TMP, 'dsv')
    run('dcamp', ['--campaign', '--difficulty', 'hard'], ch1 + ',confirm,skip,wait:30', sv, 900)
    before = read_save(sv, 'dcamp')
    check('game page: the Continue save is chapter 2 on Hard', before and before['chapter'] == 'ch2' and before['playthrough']['difficulty'] == 'hard')
    code, out = run('dcamp', [], 'wait:5', sv, 60)
    check('game page: the Difficulty row shows the current difficulty', 'game page: difficulty row "Hard"' in out and 'greyed' not in out, out[-300:])
    code, out = run('dcamp', [], to_page_row + ',confirm,wait:5,confirm,wait:10', sv, 200)
    after = read_save(sv, 'dcamp')
    check('game page: the list offers what the rule allows (easy, normal)', 'game page: difficulty menu offers easy, normal' in out, out[-400:])
    check('game page: picking Easy writes it into the Continue save\'s playthrough', after and after['playthrough']['difficulty'] == 'easy',
          json.dumps(after)[:200] if after else 'no save')
    if before and after:
        b, a = json.loads(json.dumps(before)), json.loads(json.dumps(after))
        b['playthrough'].pop('difficulty')
        a['playthrough'].pop('difficulty')
        check('game page: nothing else in the save changed (the battle in it keeps Hard)', a == b and after['battle']['difficulty'] == 'hard')
    check('game page: the row is rebuilt: Easy is the lowest, and it applies from the next battle', 'difficulty row "Easy (lowest) (next battle)" greyed' in out, out[-300:])
    # The next battle built uses it: chapter 2 is finished (END wins it), chapter 3 is built on Easy.
    code, out = run('dcamp', ['--continue'], 'wait:30,cancel,' + END + ',confirm,wait:40,confirm,wait:40', sv, 1500)
    s = read_save(sv, 'dcamp')
    check('game page: the next battle built (chapter 3) is on Easy', s and s['chapter'] == 'ch3' and s['battle']['difficulty'] == 'easy', out[-300:])
    # "never" greys the row with the reason; "free" offers every other difficulty.
    sv2 = os.path.join(TMP, 'dsv-never')
    run('dcamp_never', ['--campaign', '--difficulty', 'hard'], 'confirm,skip,wait:30', sv2, 300)
    code, out = run('dcamp_never', [], to_page_row + ',confirm,wait:10', sv2, 200)
    check('game page: rule "never" greys the row (fixed) and changes nothing',
          'difficulty row "Hard (fixed)" greyed' in out and 'difficulty menu' not in out and read_save(sv2, 'dcamp_never')['playthrough']['difficulty'] == 'hard',
          out[-300:])
    sv3 = os.path.join(TMP, 'dsv-free')
    run('dcamp_free', ['--campaign', '--difficulty', 'normal'], 'confirm,skip,wait:30', sv3, 300)
    code, out = run('dcamp_free', [], to_page_row + ',confirm,wait:5,down,confirm,wait:10', sv3, 200)
    check('game page: rule "free" offers the others and a harder one can be picked',
          'difficulty menu offers easy, hard' in out and read_save(sv3, 'dcamp_free')['playthrough']['difficulty'] == 'hard', out[-300:])
    # A game without difficulties has no row.
    sv4 = os.path.join(TMP, 'dsv-none')
    run('dnone', ['--map', 'skirmish'], 'wait:20', sv4, 100)
    code, out = run('dnone', [], 'wait:5', sv4, 60)
    check('game page: a game without difficulties has no Difficulty row', 'difficulty row' not in out, out[-200:])


def sound_token(line):
    m = re.match(r't=[\d.]+ sound (\S+) vol=(\d+)', line)
    return os.path.splitext(os.path.basename(m.group(1)))[0] if m else None


def play_sounds(games, game, args, inp, frames=0, interval=8):
    """Like play(), but the sound effects of the audio log (names), and the player's output."""
    log = os.path.join(TMP, 'audio.log')
    if os.path.exists(log):
        os.remove(log)
    cmd = [PLAYER, '--headless', '--no-settings', '--games', games, '--assets', os.path.join(REPO, 'assets'), '--audio-log', log,
           '--input-interval', str(interval), '--no-saving', '--game', game, '--fixed-dt', '0.016']
    if frames:
        cmd += ['--frames', str(frames)]
    if inp:
        cmd += ['--input', inp]
    p = run_text(cmd + args)
    raw = open(log).read().splitlines() if os.path.exists(log) else []
    return p.returncode, p.stdout, [t for t in (sound_token(l) for l in raw) if t]


def run_step_and_animation_sounds():
    """Step sounds by movement type (docs/SOUND.md) and the sounds a battle animation plays at its frames (docs/BATTLE_SCENE.md)."""
    games = os.path.join(TMP, 'step-games')
    names = ['battle', 'boss', 'hooves', 'tread', 'swing', 'clang', 'thunder']
    tones = os.path.join(TMP, 'step-tones')
    os.makedirs(tones)
    audio = os.path.join(tones, 'audio')
    subprocess.run([sys.executable, '-I', '-B', os.path.join(REPO, 'tools', 'audio', 'make_music_test_audio.py'), audio] + names, check=True)
    aj = json.dumps({'music': {'battle': {'file': 'audio/battle.wav'}, 'boss': {'file': 'audio/boss.wav'}},
                     'sounds': {n: {'file': 'audio/%s.wav' % n} for n in names[2:]}, 'use': {'mapMusic': 'battle'}})
    g = sound_game.make_game(games, 'snd', tones, aj, sound_overrides=False)

    def edit(rel, fn):
        d = json.load(open(os.path.join(g, rel)))
        fn(d)
        write_json(os.path.join(g, rel), d)

    def mounted(interval):
        def fn(gj):
            gj['moveTypes'] = [{'id': 'foot', 'name': 'Foot'}, {'id': 'mounted', 'name': 'Mounted', 'sounds': {'step': 'hooves', **({'stepInterval': interval} if interval else {})}}]
        edit('game.json', fn)
        edit('data/units.json', lambda u: [x.update({'moveType': 'mounted'}) for x in u['units'] if x['id'] == 'rider'])

    RIDE = 'wait:60,confirm,right,right,right,confirm,confirm,wait:60'
    mounted(0)
    code, out, seq = play_sounds(games, 'snd', ['--map', 'ride'], RIDE)
    check('movement type: the mounted rider\'s steps use its movement type\'s sound', code == 0 and seq.count('hooves') >= 1 and 'tread' not in seq, str(seq) + out[-300:])
    base = seq.count('hooves')
    mounted(0.02)
    code, out, seq = play_sounds(games, 'snd', ['--map', 'ride'], RIDE)
    check('movement type: its stepInterval sets the time between steps (a short one = a step on every square)',
          seq.count('hooves') > base and seq.count('hooves') >= 3, '%s vs %d' % (seq, base))
    edit('data/units.json', lambda u: [x.update({'sounds': {'step': 'tread'}}) for x in u['units'] if x['id'] == 'rider'])
    code, out, seq = play_sounds(games, 'snd', ['--map', 'ride'], RIDE)
    check('movement type: the unit type\'s own step sound beats its movement type\'s', seq.count('tread') >= 1 and 'hooves' not in seq, str(seq))
    # a foot unit with no step sound of its own and none on its movement type: silence (the game has no standard step)
    code, out, seq = play_sounds(games, 'snd', ['--map', 'duel'], 'wait:60,confirm,right,confirm,confirm,confirm,wait:100')
    check('movement type: a unit whose movement type names nothing steps silently', 'hooves' not in seq and 'tread' not in seq, str(seq))
    # refusals
    bad = json.load(open(os.path.join(g, 'game.json')))
    bad['moveTypes'][1]['sounds'] = {'step': 'nope'}
    write_json(os.path.join(g, 'game.json'), bad)
    code, out, seq = play_sounds(games, 'snd', ['--map', 'ride'], 'wait:5', frames=20)
    check('movement type: an unknown sound id is refused with the move type named', 'move type "mounted"' in out and 'names unknown sound "nope"' in out, out[-300:])
    bad['moveTypes'][1]['sounds'] = {'step': 'hooves', 'stepInterval': 9}
    write_json(os.path.join(g, 'game.json'), bad)
    code, out, seq = play_sounds(games, 'snd', ['--map', 'ride'], 'wait:5', frames=20)
    check('movement type: a stepInterval out of range is refused', '"stepInterval" is seconds between two steps' in out, out[-300:])

    # Animation sounds: the srpg-quotes fixture with the knight set whose attack plays "swing" on frame 1 and "clang" on frame 3.
    sys.path.insert(0, os.path.join(REPO, 'tools', 'art'))
    import battle_art
    bg = os.path.join(TMP, 'bscene-games', 'bs')
    shutil.copytree(os.path.join(REPO, 'tests', 'fixtures', 'srpg-quotes'), bg)
    os.makedirs(os.path.join(bg, 'art'))
    battle_art.main(os.path.join(bg, 'art'))
    units = battle_art.Canvas(24 * 4, 24)
    for i, col in enumerate(((70, 110, 200, 255), (200, 80, 70, 255), (90, 170, 90, 255), (190, 160, 70, 255))):
        fig = battle_art.Canvas(24, 24)
        fig.rect(8, 6, 15, 21, col)
        fig.rect(9, 2, 14, 6, (232, 192, 150, 255))
        fig.outline()
        units.paste(fig, i * 24, 0)
    battle_art.write_png(os.path.join(bg, 'art', 'units.png'), units)
    sets = json.loads(json.dumps(battle_art.BATTLE_SET['sets']))
    sets['knight']['animations']['attack']['sounds'] = {'1': 'swing', '3': 'clang'}
    sets['knight']['animations']['hurt']['sounds'] = {'0': 'tread'}
    write_json(os.path.join(bg, 'data', 'graphics.json'), {
        'sheets': battle_art.BATTLE_SHEETS + [{'id': 'units', 'file': 'art/units.png', 'cellWidth': 24, 'cellHeight': 24}],
        'units': {'hero': {'sheet': 'units', 'cell': [0, 0]}, 'brute': {'sheet': 'units', 'cell': [1, 0]},
                  'rider': {'sheet': 'units', 'cell': [2, 0]}, 'grunt': {'sheet': 'units', 'cell': [3, 0]}},
        'battle': {'sets': sets, 'assign': [{'set': 'knight', 'type': 'hero'}], 'effects': [],
                   'backgrounds': battle_art.BATTLE_SET['backgrounds'], 'defaultBackground': 'meadow'}})
    shutil.copytree(os.path.join(tones, 'audio'), os.path.join(bg, 'audio'))
    write_json(os.path.join(bg, 'data', 'audio.json'), json.loads(aj))
    hp = json.load(open(os.path.join(bg, 'data', 'units.json')))
    for x in hp['units']:
        if x['id'] == 'brute':
            x['stats']['hp'] = 2
    write_json(os.path.join(bg, 'data', 'units.json'), hp)
    chars = json.load(open(os.path.join(bg, 'data', 'characters.json')))
    for ch in chars['characters']:
        ch.pop('quotes', None)
    write_json(os.path.join(bg, 'data', 'characters.json'), chars)
    attack = 'wait:14,confirm,wait:3,confirm,wait:3,confirm,wait:3,confirm,wait:120'
    code, out, seq = play_sounds(os.path.dirname(bg), 'bs', ['--map', 'ch1', '--battle-scene', 'scene'], attack, interval=4)
    lines = [l for l in out.splitlines() if 'battle scene: sound' in l]
    check('animation sounds: the attack plays swing then clang at its frames (audio log and log lines)',
          [s for s in seq if s in ('swing', 'clang')] == ['swing', 'clang'] and 'battle scene: sound swing (Eirik)' in out, str(seq) + '\n'.join(lines))
    check('animation sounds: the unit that is struck plays its hurt animation\'s sound (Gorm has no set: nothing)', 'tread' not in seq, str(seq))
    # a sound id that audio.json does not have is refused when the game loads
    sets['knight']['animations']['attack']['sounds'] = {'1': 'whoosh'}
    gj = json.load(open(os.path.join(bg, 'data', 'graphics.json')))
    gj['battle']['sets'] = sets
    write_json(os.path.join(bg, 'data', 'graphics.json'), gj)
    code, out, seq = play_sounds(os.path.dirname(bg), 'bs', ['--map', 'ch1', '--battle-scene', 'scene'], 'wait:5', frames=20)
    check('animation sounds: an unknown sound id is refused (set, animation and sound named)',
          'battle set "knight" animation "attack": sound "whoosh" is not in data/audio.json' in out, out[-300:])
    sets['knight']['animations']['attack']['sounds'] = {'9': 'swing'}
    gj['battle']['sets'] = sets
    write_json(os.path.join(bg, 'data', 'graphics.json'), gj)
    code, out, seq = play_sounds(os.path.dirname(bg), 'bs', ['--map', 'ch1'], 'wait:5', frames=20)
    check('animation sounds: a frame the animation does not have is refused', '"sounds" names frame \"9\" but the animation has frames 0 to 3' in out, out[-300:])


run_situations()
run_step_and_animation_sounds()
run_game_page_difficulty()
summary()
shutil.rmtree(TMP, ignore_errors=True)
sys.exit(1 if failures else 0)
