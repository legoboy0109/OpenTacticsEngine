#!/usr/bin/env python3
"""Sound in the player (docs/SOUND.md, build order step 2), windowless and without a sound device.

  python3 tests/sound_player_test.py ./build/engine/player/ote-player .
  python3 tests/sound_player_test.py ./build/engine/player/ote-player . --show   # also print every sequence

Writes a small game into a temp folder (tests/sound_game.py) together with the generated starter audio
(tools/audio/make_starter_audio.py; nothing generated is ever committed), then runs the real ote-player with
--headless, --fixed-dt (the default there) and --audio-log, and asserts the exact sounds and music changes:
launcher navigation, a move, a hit, a miss, a critical, a defeat, a capture, a build, an item, turn changes with the
enemy's turn music, the story commands, the victory jingle, the overrides, missing files, unknown ids, a save made
after a story playMusic, fog, a campaign scene, and that the two demos (no audio.json) stay silent.

Never opens a window or a sound device (every run is --headless) and never touches the real games / settings / saves.
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
import sound_game  # noqa: E402

PLAYER = ''
REPO = ''
SHOW = False
failures = []
passed = 0


def check(name, ok, detail=''):
    global passed
    if ok:
        passed += 1
    else:
        failures.append(name)
        print('FAIL %s %s' % (name, detail))


# ---- the test game and its audio

def starter_audio(tmp):
    out = os.path.join(tmp, 'starter')
    subprocess.run([sys.executable, os.path.join(REPO, 'tools', 'audio', 'make_starter_audio.py'), '--out', out, '--ogg'],
                   check=True, stdout=subprocess.DEVNULL)
    a = os.path.join(out, 'audio')
    # Tracks and sounds the game uses that the starter set does not have: copies under other names, so the log tells them apart.
    for n in ('boss', 'calm'):
        shutil.copy(os.path.join(a, 'battle.ogg'), os.path.join(a, n + '.ogg'))
    shutil.copy(os.path.join(a, 'hit.wav'), os.path.join(a, 'thunder.wav'))
    shutil.copy(os.path.join(a, 'cursor.wav'), os.path.join(a, 'neigh.wav'))
    return out


def audio_json(starter):
    ex = json.load(open(os.path.join(starter, 'audio.json.example')))
    for n in ('boss', 'calm'):
        ex['music'][n] = {'file': 'audio/%s.ogg' % n}
    ex['sounds']['thunder'] = {'file': 'audio/thunder.wav'}
    ex['sounds']['neigh'] = {'file': 'audio/neigh.wav'}
    return ex


def games_folder(tmp, name='games'):
    d = os.path.join(tmp, name)
    shutil.rmtree(d, ignore_errors=True)
    os.makedirs(d)
    return d


# ---- running the player

def play(tmp, games, args, inp=None, frames=0, saves=None, game='snd', interval=8):
    """One windowless run; returns (exit code, output, audio-log lines as short tokens, raw log lines)."""
    log = os.path.join(tmp, 'audio.log')
    if os.path.exists(log):
        os.remove(log)
    cmd = [PLAYER, '--headless', '--no-settings', '--games', games, '--assets', os.path.join(REPO, 'assets'),
           '--audio-log', log, '--input-interval', str(interval)]
    cmd += ['--saves', saves] if saves else ['--no-saving']
    if game:
        cmd += ['--game', game]
    if frames:  # (with an --input script and no --frames the run ends when the script has run out)
        cmd += ['--frames', str(frames)]
    if inp:
        cmd += ['--input', inp]
    p = subprocess.run(cmd + args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True, timeout=300)
    raw = open(log).read().splitlines() if os.path.exists(log) else []
    return p.returncode, p.stdout, [token(l) for l in raw if token(l)], raw


def token(line):
    """'t=1.350 sound /x/audio/hit.wav vol=100 level=7' -> 'hit'; music: 'M:battle', 'M:battle f=0', 'M-'; volumes: None."""
    m = re.match(r't=[\d.]+ sound (\S+) vol=(\d+)', line)
    if m:
        name = os.path.splitext(os.path.basename(m.group(1)))[0]
        return name if m.group(2) == '100' else '%s@%s' % (name, m.group(2))
    m = re.match(r't=[\d.]+ music play (\S+) fade=([\d.]+) loop=(\d)', line)
    if m:
        s = 'M:' + os.path.splitext(os.path.basename(m.group(1)))[0]
        if m.group(3) == '0':
            s += ' once'
        f = float(m.group(2))
        return s + ('' if f == 0.5 else ' cut' if f == 0 else ' f=%g' % f)
    m = re.match(r't=[\d.]+ music stop fade=([\d.]+)', line)
    if m:
        return 'M-' if m.group(1) == '0.5' else 'M- f=' + m.group(1)
    return None


def shown(name, seq):
    if SHOW:
        print('%-22s %s' % (name, ' '.join(seq)))


def lines_of(out, prefix):
    return [l for l in out.splitlines() if prefix in l]


# ---- scenarios

def edit_json(path, fn):
    data = json.load(open(path))
    fn(data)
    json.dump(data, open(path, 'w'), indent=1)


def variant(tmp, starter, aj, name, **kw):
    """A fresh games folder holding a copy of the test game called `snd`; returns (games folder, game folder)."""
    games = games_folder(tmp, name)
    g = sound_game.make_game(games, 'snd', starter, aj, **kw)
    return games, g


def run_all(tmp):
    starter = starter_audio(tmp)
    games = games_folder(tmp)
    aj = audio_json(starter)
    sound_game.make_game(games, 'snd', starter, aj)

    def go(args, inp, frames=0, g=None, **kw):
        return play(tmp, g or games, args, inp, frames, **kw)

    OPEN = 'wait:60,'  # the first banner
    END_TURN = 'menu,confirm,wait:200'
    DUEL = OPEN + 'confirm,right,confirm,confirm,confirm,wait:200'

    # --- 1. The launcher: the home list is silent; a game's page has its title music and its menu sounds.
    code, out, seq, _ = go([], 'wait:10,confirm,wait:5,down,wait:5,up,wait:5,confirm,wait:5,down,wait:5,cancel,wait:5,cancel,wait:5',
                           frames=700, game='')
    shown('launcher', seq)
    check('launcher: runs', code == 0, out[-300:])
    check('launcher: the game page starts the title music, its menus give cursor / confirm / cancel, going home stops it',
          seq == ['M:title', 'cursor', 'cursor', 'confirm', 'cursor', 'cancel', 'cancel', 'M-'], str(seq))
    code, out, seq, _ = go([], 'wait:10,confirm,wait:5,down,wait:5,confirm,wait:5,down,wait:5,cancel,wait:5,cancel,wait:5',
                           frames=700, game='')
    shown('game options', seq)
    check('launcher: a game\'s options screen keeps the title music and has the menu sounds',
          seq == ['M:title', 'cursor', 'confirm', 'cursor', 'cancel', 'cancel', 'M-'], str(seq))
    code, out, seq, _ = go([], 'wait:10,down,wait:5,confirm,wait:5,down,wait:5,cancel,wait:5', frames=400, game='')
    shown('home options', seq)
    check('launcher: the home list and the global options are silent', seq == [], str(seq))

    # --- 2. A move, a hit, a defeat, the victory jingle, then back out to the map list (title music).
    code, out, seq, _ = go(['--map', 'duel'], DUEL + ',confirm,wait:20')
    shown('duel', seq)
    check('duel: exact sequence (music, turn chime, select, cursor, move + step, attack menu, hit, defeat, victory jingle, title)',
          seq == ['M:battle', 'turnStart', 'confirm', 'cursor', 'confirm', 'step1', 'confirm', 'confirm', 'sword_hit', 'defeated',
                  'M:victory once cut', 'M:title'], str(seq))

    # --- 3. A miss, a critical hit, a refused move.
    code, out, seq, _ = go(['--map', 'dull'], OPEN + 'confirm,right,confirm,confirm,confirm,wait:40')
    shown('dull', seq)
    check('miss: the standard miss sound, no hit sound', seq[-1:] == ['miss'] and 'hit' not in seq and 'sword_hit' not in seq, str(seq))
    code, out, seq, _ = go(['--map', 'crusher'], OPEN + 'confirm,right,confirm,confirm,confirm,wait:40')
    shown('crusher', seq)
    check('critical: the critical sound replaces the hit sound', seq[-1:] == ['critical'] and 'hit' not in seq, str(seq))
    code, out, seq, _ = go(['--map', 'duel'], OPEN + 'confirm,right,right,right,right,right,right,confirm,wait:30')
    shown('refused', seq)
    check('a move to a square out of reach gives the error sound', seq == ['M:battle', 'turnStart', 'confirm'] + ['cursor'] * 6 + ['error'], str(seq))

    # --- 4. A capture, a build, an item.
    code, out, seq, _ = go(['--map', 'cap'], OPEN + 'confirm,confirm,confirm,wait:60')
    shown('capture', seq)
    check('capture: the capture sound', seq == ['M:battle', 'turnStart', 'confirm', 'confirm', 'confirm', 'capture'], str(seq))
    code, out, seq, _ = go(['--map', 'build'], OPEN + 'left,left,left,left,confirm,confirm,wait:60')
    shown('build', seq)
    check('build: the build sound', seq == ['M:battle', 'turnStart'] + ['cursor'] * 4 + ['confirm', 'confirm', 'build'], str(seq))
    code, out, seq, _ = go(['--map', 'potion'], OPEN + 'confirm,confirm,confirm,confirm,confirm,wait:60')
    shown('potion', seq)
    check('item: the item\'s own use sound (override on the item)', seq == ['M:battle', 'turnStart'] + ['confirm'] * 5 + ['potion'], str(seq))

    # --- 5. Turn phases: the enemy's turn music and back (also with a map's own player-turn music).
    code, out, seq, _ = go(['--map', 'turn'], OPEN + END_TURN)
    shown('turn', seq)
    check('turn: enemy music on the enemy\'s turn, the map\'s music again on ours; the AI\'s steps take turns (step1, step2)',
          seq == ['M:battle', 'turnStart', 'confirm', 'M:enemy', 'turnStart', 'step1', 'step2', 'M:battle', 'turnStart'], str(seq))
    code, out, seq, _ = go(['--map', 'quiet'], OPEN + END_TURN)
    shown('quiet', seq)
    check('quiet: the map\'s player-turn music, the game\'s enemy-turn music, back',
          seq == ['M:calm', 'turnStart', 'confirm', 'M:enemy', 'turnStart', 'step1', 'step2', 'M:calm', 'turnStart'], str(seq))

    # --- 6. The story: playMusic (with a fade) overrides, playSound plays, stopMusic releases.
    code, out, seq, _ = go(['--map', 'story'], OPEN + END_TURN)
    shown('story', seq)
    check('story: playMusic overrides the map music (1 s fade), playSound plays, no enemy music while the story holds the music, '
          'stopMusic (0.25 s) releases it to silence',
          seq == ['M:battle', 'M:boss f=1', 'thunder', 'turnStart', 'confirm', 'turnStart', 'step1', 'step2', 'turnStart', 'M- f=0.25'],
          str(seq))

    # --- 7. Level up: a jingle over the music (it pauses it).
    lv, _ = variant(tmp, starter, aj, 'games_lv', levels=True)
    code, out, seq, _ = go(['--map', 'levels'], DUEL, g=lv)
    shown('levels', seq)
    check('level up: the level-up jingle plays once, after the hit',
          seq[-1:] == ['M:levelup once cut'] and 'sword_hit' in seq and 'M:victory once cut' not in seq, str(seq))

    # --- 8. Overrides.
    code, out, seq, _ = go(['--map', 'ride'], OPEN + 'confirm,right,right,right,confirm,confirm,wait:60')
    shown('ride', seq)
    check('overrides: the rider\'s own select and step sounds (the sword\'s hit sound is in the duel above)',
          seq == ['M:battle', 'turnStart', 'neigh', 'cursor', 'cursor', 'cursor', 'confirm', 'hooves', 'hooves', 'confirm'], str(seq))
    sv, sg = variant(tmp, starter, aj, 'games_silent_hit')
    edit_json(os.path.join(sg, 'data', 'weapons.json'), lambda d: d['weapons'][0]['sounds'].update({'hit': ''}))
    code, out, seq, _ = go(['--map', 'duel'], DUEL, g=sv)
    shown('silent hit', seq)
    check('overrides: "" on a weapon is silence (no hit sound, not even the standard one)',
          'sword_hit' not in seq and 'hit' not in seq and 'defeated' in seq, str(seq))

    # --- 9. Dialogue: a blip while the text types, none when skipped.
    code, out, seq, _ = go(['--map', 'talk'], 'wait:30,confirm,confirm,confirm,confirm,wait:60', interval=30)
    shown('talk', seq)
    typed = seq.count('dialogue')
    check('dialogue: blips while a line types out', typed >= 4, str(seq))
    code, out, seq, _ = go(['--map', 'talk'], 'skip,wait:60', interval=1)
    shown('talk skipped', seq)
    check('dialogue: skipping the line does not blip', seq.count('dialogue') < typed and seq.count('dialogue') <= 2, str(seq))

    # --- 10. Fog: what the viewer does not see makes no sound; skipping does not stack sounds.
    code, out, seq, _ = go(['--map', 'fogwalk'], OPEN + END_TURN)
    shown('fog', seq)
    check('fog: the enemy rider walks out of sight: no hooves, the (public) turn start and music still change',
          'hooves' not in seq and 'M:enemy' in seq and seq.count('turnStart') == 3, str(seq))
    code, out, seq, _ = go(['--map', 'fogwalk', '--show-all'], OPEN + END_TURN)
    shown('fog off', seq)
    walked = seq.count('hooves')
    check('fog: the same walk with --show-all is heard', walked >= 2, str(seq))
    code, out, seq, _ = go(['--map', 'fogwalk', '--show-all'], OPEN + 'menu,confirm,confirm,confirm,confirm,confirm,wait:200', interval=1)
    shown('skipped walk', seq)
    check('a skipped animation does not stack sounds', seq.count('hooves') < walked and seq.count('hooves') <= 2, str(seq))

    # --- 11. A save made after a story playMusic resumes with that track.
    sav = os.path.join(tmp, 'saves')
    shutil.rmtree(sav, ignore_errors=True)
    go(['--map', 'story'], 'wait:200', saves=sav)
    code, out, seq, _ = go(['--continue'], 'wait:60', saves=sav)
    shown('resume', seq)
    check('resume: the Continue save of a battle that had played "boss" plays it again',
          seq[:1] == ['M:boss'] and 'M:battle' not in seq, str(seq) + out[-300:])
    sav2 = os.path.join(tmp, 'saves2')
    shutil.rmtree(sav2, ignore_errors=True)
    go(['--map', 'story'], OPEN + END_TURN, saves=sav2)
    code, out, seq, _ = go(['--continue'], 'wait:60', saves=sav2)
    shown('resume stopped', seq)
    check('resume: a battle whose story had stopped the music resumes in silence',
          'M:boss' not in seq and 'M:battle' not in seq and not [t for t in seq if t.startswith('M:')], str(seq))

    # --- 12. A campaign scene with music, sounds and the same music asked again.
    camp = {'name': 'Sound Campaign', 'rules': {'fallen': 'gone', 'onDefeat': 'retry'},
            'chapters': [{'id': 'ch1', 'name': 'First', 'map': 'duel',
                          'before': {'music': 'scene', 'commands': [{'say': 'Hello.'}, {'playSound': 'thunder'},
                                                                    {'playMusic': 'scene'}, {'say': 'Again.'},
                                                                    {'playMusic': 'calm', 'fade': 2}, {'say': 'Over.'}]},
                          'after': [{'say': 'Done.'}]}]}
    cg, cgd = variant(tmp, starter, aj, 'games_camp', campaign=camp)
    edit_json(os.path.join(cgd, 'maps', 'duel.json'), lambda d: d.update({'music': 'boss'}))  # the chapter's map music
    code, out, seq, _ = go(['--campaign', '--autoplay'], None, 2400, g=cg)
    music = [t for t in seq if t.startswith('M')]
    shown('campaign', seq[:12])
    check('campaign scene: scene music with the card, the same track asked again keeps playing, a story track with a 2 s fade, '
          'the chapter map\'s music in the battle, scene music again after it',
          music[:4] == ['M:scene', 'M:calm f=2', 'M:boss', 'M:scene'], str(music))
    check('campaign scene: playSound plays between the lines, lines blip', 'thunder' in seq and 'dialogue' in seq, str(seq[:30]))

    # --- 13. A missing file is a warning and silence; --no-audio reads nothing.
    mg, mgd = variant(tmp, starter, aj, 'games_missing')
    os.remove(os.path.join(mgd, 'audio', 'defeated.wav'))
    os.remove(os.path.join(mgd, 'audio', 'battle.ogg'))
    code, out, seq, _ = go(['--map', 'duel'], DUEL, g=mg)
    shown('missing', seq)
    check('missing file: the game runs', code == 0 and 'Cannot load' not in out, out[-300:])
    check('missing file: a warning for the sound and the track', 'defeated.wav does not exist' in out and 'battle.ogg does not exist' in out, out[-500:])
    check('missing file: that sound and music are silent, the rest plays',
          'defeated' not in seq and 'M:battle' not in seq and 'sword_hit' in seq, str(seq))
    off_g, _ = variant(tmp, starter, aj, 'games_off')
    code, out, seq, raw = go(['--map', 'duel', '--no-audio'], DUEL, g=off_g)
    check('--no-audio: no sound or music is requested', seq == [] and code == 0, str(seq))

    # --- 14. Mistakes are load errors that say where.
    def broken(name, mutate, expect):
        g_, gd = variant(tmp, starter, aj, 'games_bad', campaign=camp)
        mutate(gd)
        code, out, seq, raw = play(tmp, g_, [], None, 20)
        check('error: ' + name, 'Cannot load game' in out and expect in out, out[-500:])

    def audio_edit(fn):
        return lambda gd: edit_json(os.path.join(gd, 'data', 'audio.json'), fn)

    broken('unknown key in audio.json', audio_edit(lambda d: d.update({'muzic': {}})), 'audio.json: unknown key "muzic" (expected "music", "sounds", "use")')
    broken('unknown key in a sound', audio_edit(lambda d: d['sounds']['hit'].update({'flie': 'x'})), 'sound "hit": unknown key "flie"')
    broken('unknown event in use.sounds', audio_edit(lambda d: d['use']['sounds'].update({'hitt': 'hit'})), 'use.sounds: unknown key "hitt"')
    broken('unknown sound id in use.sounds', audio_edit(lambda d: d['use']['sounds'].update({'hit': 'ghost'})), '"hit" names unknown sound "ghost"')
    broken('unknown music id in use', audio_edit(lambda d: d['use'].update({'mapMusic': 'ghost'})), 'use.mapMusic: names unknown music "ghost"')
    broken('a looping jingle', audio_edit(lambda d: d['music']['victory'].update({'loop': True})), 'must have "loop": false')
    broken('a bad path', audio_edit(lambda d: d['sounds']['hit'].update({'file': '../x.wav'})), '"file" must be a path inside the game folder')
    broken('unknown weapon override id', lambda gd: edit_json(os.path.join(gd, 'data', 'weapons.json'),
           lambda d: d['weapons'][0]['sounds'].update({'hit': 'ghost'})), 'weapon "sword": "sounds": "hit" names unknown sound "ghost"')
    broken('unit override key', lambda gd: edit_json(os.path.join(gd, 'data', 'units.json'),
           lambda d: d['units'][3]['sounds'].update({'use': 'neigh'})), 'unit "rider": "sounds": unknown key "use"')
    broken('unknown map music id', lambda gd: edit_json(os.path.join(gd, 'maps', 'quiet.json'),
           lambda d: d.update({'music': 'ghost'})), 'map "quiet": "music" names unknown music "ghost"')
    broken('unknown music id in a story command', lambda gd: edit_json(os.path.join(gd, 'maps', 'story.json'),
           lambda d: d['events'][0]['do'][0].update({'playMusic': 'ghost'})), 'event "open": "playMusic" names unknown music "ghost"')
    broken('unknown sound id in a scene', lambda gd: edit_json(os.path.join(gd, 'data', 'campaign.json'),
           lambda d: d['chapters'][0]['before']['commands'].__setitem__(1, {'playSound': 'ghost'})),
           'chapter "ch1" "before": sound "ghost" is not in data/audio.json')
    ng, ngd = variant(tmp, starter, None, 'games_noaudio_story')
    code, out, seq, raw = play(tmp, ng, [], None, 20)
    check('error: a story command in a game without audio.json', 'Cannot load game' in out and 'this game has no data/audio.json' in out, out[-400:])
    off_bad, offd = variant(tmp, starter, aj, 'games_off_bad')
    edit_json(os.path.join(offd, 'data', 'audio.json'), lambda d: d['use'].update({'mapMusic': 'ghost'}))
    code, out, seq, raw = play(tmp, off_bad, ['--no-audio'], None, 20)
    check('--no-audio: audio.json is not even read (a wrong one does not stop the game)', 'Cannot load' not in out and code == 0, out[-300:])

    # --- 15. Sound never changes a save: the same play with and without sound writes the same Continue save.
    pg, pgd = variant(tmp, starter, aj, 'games_plain', sound_overrides=False)
    os.remove(os.path.join(pgd, 'data', 'audio.json'))
    shutil.rmtree(os.path.join(pgd, 'audio'))
    edit_json(os.path.join(pgd, 'maps', 'story.json'), lambda d: d.pop('events'))  # (story sound commands need an audio.json)
    saved = []
    for label, folder in (('with', games), ('without', pg)):
        sdir = os.path.join(tmp, 'saves_' + label)
        shutil.rmtree(sdir, ignore_errors=True)
        go(['--map', 'ride'], OPEN + 'confirm,right,right,confirm,confirm,wait:100', g=folder, saves=sdir)
        path = os.path.join(sdir, 'snd', 'continue.json')
        d = json.load(open(path)) if os.path.exists(path) else None
        if d:
            d.pop('savedAt', None)
        saved.append(d)
    diff = [k for k in (saved[0] or {}) if (saved[0] or {}).get(k) != (saved[1] or {}).get(k)]
    check('save: a game with sound writes the same save as one without (same hash, same state)',
          saved[0] is not None and saved[0] == saved[1], 'differs in %s' % diff)

    # --- 16. The demos have no audio.json: nothing is requested but the volumes.
    demos = games_folder(tmp, 'demos')
    for d in ('demo-srpg', 'demo-economy'):
        os.symlink(os.path.join(REPO, 'games', d), os.path.join(demos, d))
    for game, extra in (('demo-srpg', ['--map', 'bridge']), ('demo-economy', ['--map', 'twin_rivers'])):
        code, out, seq, raw = play(tmp, demos, extra + ['--autoplay'], 'wait:20', 500, game=game)
        check('demo %s: a battle makes no sound or music' % game, code == 0 and seq == [] and all('volumes' in l for l in raw), str(raw[:5]))
    code, out, seq, raw = play(tmp, demos, [], 'wait:10,down,confirm,wait:5,down,cancel,wait:5,confirm,wait:5,down,confirm,wait:5,cancel,wait:5', 500, game='')
    check('demos: the launcher makes no sound or music', code == 0 and seq == [], str(raw[:5]))


def main():
    global PLAYER, REPO, SHOW
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    SHOW = '--show' in sys.argv
    if len(args) < 2:
        print(__doc__)
        return 2
    PLAYER, REPO = os.path.abspath(args[0]), os.path.abspath(args[1])
    tmp = tempfile.mkdtemp(prefix='ote_sound_')
    try:
        run_all(tmp)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    print('%d passed, %d failed' % (passed, len(failures)))
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
