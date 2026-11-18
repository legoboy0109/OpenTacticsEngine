#!/usr/bin/env python3
"""A staff item's own "heal" sound through the real player, windowless and without a sound device (docs/SOUND.md "Overrides").

  python3 tests/staff_heal_sound_player_test.py ./build/engine/player/ote-player .

Works on a copy of tests/fixtures/srpg-staves (Wen's Mend Staff heals Tarn, who a Continue save leaves wounded) with three generated beeps
(`heal` = the game's standard heal sound, `own` = what an item may set instead, `probe` = what a battle scene effect plays) and judges the
--audio-log (the player opens no sound device). Both places the "+N" heal cue plays are covered: the map and the battle scene
("battleSceneStaves": the staff's effect plays its own sound, the case the item key was made for).

  - no item key: the standard heal sound plays (also next to the effect's own sound in the scene: today's behaviour);
  - "sounds": { "heal": "" } on the staff: the heal cue is silent, the effect's sound is still heard;
  - "sounds": { "heal": "own" }: that sound plays instead of the standard heal sound;
  - the loader names the key set and the unknown sound id when it is wrong.
"""
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import wave

sys.dont_write_bytecode = True

PLAYER = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else ''
REPO = os.path.abspath(sys.argv[2]) if len(sys.argv) > 2 else '.'
TMP = tempfile.mkdtemp(prefix='ote-staffheal-')
passed, failures = 0, []

sys.path.insert(0, os.path.join(REPO, 'tools', 'art'))
import battle_art  # noqa: E402  (our own generator: original pixel art, no Pillow)

Canvas, write_png = battle_art.Canvas, battle_art.write_png


def check(name, cond, detail=''):
    global passed
    if cond:
        passed += 1
        print('PASS ' + name)
    else:
        failures.append(name)
        print('FAIL %s %s' % (name, detail))


def read_json(path):
    with open(path) as f:
        return json.loads(''.join(l for l in f if not l.lstrip().startswith('//')))


def write_json(path, obj):
    with open(path, 'w') as f:
        json.dump(obj, f, indent=1)


def edit(g, rel, fn):
    path = os.path.join(g, rel)
    obj = read_json(path)
    fn(obj)
    write_json(path, obj)


def beep(path, hz):
    with wave.open(path, 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(22050)
        w.writeframes(b''.join(struct.pack('<h', 6000 if (i * hz * 2 // 22050) % 2 else -6000) for i in range(1100)))


def art(g):
    """Plain generated art for a battle scene (a 240 x 160 stage) and a staff effect that plays the sound `probe` when it starts."""
    os.makedirs(os.path.join(g, 'art'), exist_ok=True)

    def solid(w, h, col):
        c = Canvas(w, h)
        c.rect(0, 0, w - 1, h - 1, col + (255,))
        return c
    for name, w, h, col in (('stage_bg', 240, 160, (24, 30, 96)), ('plat', 128, 32, (200, 120, 40)), ('blue', 48, 48, (30, 60, 200)),
                            ('red', 48, 48, (200, 40, 40)), ('fx_spark', 16, 16, (120, 255, 60))):
        write_png(os.path.join(g, 'art', name + '.png'), solid(w, h, col))
    anims = lambda sheet: {k: {'sheet': sheet, 'frames': [[0, 0]] * 4, 'ms': [100] * 4, 'hit': 2}
                           for k in ('stand', 'attack', 'crit', 'ranged', 'cast', 'dodge', 'hurt', 'defeat', 'miss')}
    cells = lambda sid, w, h: {'id': sid, 'file': 'art/%s.png' % sid, 'cellWidth': w, 'cellHeight': h}
    write_json(os.path.join(g, 'data', 'graphics.json'), {
        'sheets': [cells('stage_bg', 240, 160), cells('plat', 64, 32), cells('blue', 48, 48), cells('red', 48, 48), cells('fx_spark', 16, 16)],
        'battle': {'sets': {'blue': {'anchor': [24, 46], 'animations': anims('blue')}, 'red': {'anchor': [24, 46], 'animations': anims('red')}},
                   'assign': [{'set': 'blue', 'side': 'blue'}, {'set': 'red', 'side': 'red'}],
                   'backgrounds': {'arena': {'sheet': 'stage_bg', 'cell': [0, 0]}}, 'defaultBackground': 'arena',
                   'platforms': {'slab': {'sheet': 'plat', 'cell': [0, 0], 'far': [1, 0]}}, 'defaultPlatform': 'slab',
                   'stage': {'width': 240, 'height': 160, 'feet': {'left': [88, 104], 'right': [152, 104]}, 'feetFar': {'left': [56, 104], 'right': [184, 104]},
                             'platform': {'left': [56, 96], 'right': [120, 96]}, 'platformFar': {'left': [24, 96], 'right': [152, 96]}, 'align': 'bottom'},
                   'effects': [{'id': 'glow', 'weapon': 'mend_staff', 'sheet': 'fx_spark', 'frames': [[0, 0]], 'ms': 800, 'anchor': [8, 8],
                                'hitAtMs': 300, 'sounds': {'0': 'probe'}}]}})


def game(gid, heal=None, scene=False):
    """A game folder TMP/gid. `heal`: the Mend Staff's "sounds": { "heal": ... } (None = the key is left out)."""
    g = os.path.join(TMP, gid)
    shutil.copytree(os.path.join(REPO, 'tests/fixtures/srpg-staves'), g)
    os.makedirs(os.path.join(g, 'audio'), exist_ok=True)
    for name, hz in (('heal', 440), ('own', 660), ('probe', 880)):
        beep(os.path.join(g, 'audio', name + '.wav'), hz)
    write_json(os.path.join(g, 'data', 'audio.json'), {
        'sounds': {n: {'file': 'audio/%s.wav' % n} for n in ('heal', 'own', 'probe')},
        'use': {'sounds': {'heal': 'heal'}}})
    if heal is not None:
        def fn(doc):
            for it in doc['items']:
                if it['id'] == 'mend_staff':
                    it['sounds'] = {'heal': heal}
        edit(g, 'data/items.json', fn)
    if scene:
        art(g)
        edit(g, 'game.json', lambda gj: gj.update({'battleScene': 'scene', 'battleSceneSides': 'playerRight', 'battleSceneStaves': True}))
    return g


def hurt_save(gid):
    """A Continue save with Tarn wounded (the maps cannot place a wounded unit): made by the player, then edited."""
    saves = os.path.join(TMP, 'saves_' + gid)
    subprocess.run([PLAYER, '--games', TMP, '--assets', os.path.join(REPO, 'assets'), '--headless', '--no-audio', '--no-settings', '--saves', saves,
                    '--frames', '120', '--fixed-dt', '0.016', '--game', gid, '--map', 'clinic', '--input', 'wait:20'],
                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=300)
    path = os.path.join(saves, gid, 'continue.json')
    doc = read_json(path)
    for u in doc['battle']['units']:
        if u.get('id') == 'tarn':
            u['hp'] = 5
    write_json(path, doc)
    return saves


# Wen selected, the Staff list, the Mend Staff, Tarn (first in reach): the same keys the battle scene tests press.
USE = ['wait:14', 'confirm', 'wait:3', 'confirm', 'wait:3', 'confirm', 'wait:3', 'confirm', 'wait:3', 'confirm', 'wait:60']


def play(gid):
    """Uses the Mend Staff on the wounded Tarn; returns (sound names in the order heard, Tarn's hit points afterwards, the output)."""
    saves = hurt_save(gid)
    work = os.path.join(TMP, 'run_' + gid)
    shutil.copytree(saves, work)
    alog = os.path.join(TMP, gid + '.audio.log')
    cmd = [PLAYER, '--games', TMP, '--assets', os.path.join(REPO, 'assets'), '--headless', '--no-settings', '--saves', work, '--audio-log', alog,
           '--frames', '600', '--input-interval', '4', '--fixed-dt', '0.016', '--game', gid, '--map', 'clinic', '--continue', '--input', ','.join(USE)]
    p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True, timeout=300)
    heard = []
    if os.path.exists(alog):
        for line in open(alog).read().splitlines():
            parts = line.split()
            if len(parts) > 3 and parts[1] == 'sound':
                heard.append(os.path.splitext(os.path.basename(parts[2]))[0])
    hp = None
    try:
        for u in read_json(os.path.join(work, gid, 'continue.json'))['battle']['units']:
            if u.get('id') == 'tarn':
                hp = u['hp']
    except (OSError, ValueError, KeyError):
        pass
    return heard, hp, p.stdout


def main():
    for place, scene in (('map', False), ('scene', True)):
        for label, heal, wanted in (('no key', None, {'heal'}), ('"" (silent)', '', set()), ('"own"', 'own', {'own'})):
            gid = '%s_%s' % (place, {None: 'plain', '': 'silent', 'own': 'own'}[heal])
            game(gid, heal, scene)
            heard, hp, out = play(gid)
            name = '%s, heal %s' % (place, label)
            check(name + ': the staff was used (Tarn healed)', hp is not None and hp > 5, 'hp %s; %s' % (hp, out[-300:]))
            check(name + ': the heal sounds heard are %s' % sorted(wanted), {n for n in heard if n in ('heal', 'own')} == wanted, str(heard))
            if scene:
                check(name + ': the effect\'s own sound is heard once, whatever the heal cue does', heard.count('probe') == 1, str(heard))
                check(name + ': it played as a scene', 'battle scene: staff heal' in out, out[-300:])
            else:
                check(name + ': it stayed on the map', 'battle scene:' not in out, out[-300:])

    # The loader: an unknown sound id and an unknown key name what is wrong.
    for gid, sounds, part in (('bad_id', {'heal': 'nope'}, 'nope'), ('bad_key', {'cheer': 'own'}, '"heal"')):
        g = game(gid)

        def fn(doc, sounds=sounds):
            for it in doc['items']:
                if it['id'] == 'mend_staff':
                    it['sounds'] = sounds
        edit(g, 'data/items.json', fn)
        p = subprocess.run([PLAYER, '--games', TMP, '--assets', os.path.join(REPO, 'assets'), '--headless', '--audio-log', os.path.join(TMP, 'bad.log'), '--no-settings', '--no-saving',  # (--no-audio would not check the ids)
                            '--frames', '5', '--game', gid, '--map', 'clinic'], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True, timeout=120)
        check('loader: sounds %s is refused and named' % json.dumps(sounds), 'items.json: item "mend_staff"' in p.stdout and part in p.stdout, p.stdout[-300:])


if __name__ == '__main__':
    try:
        main()
    finally:
        shutil.rmtree(TMP, ignore_errors=True)
    print('%d passed, %d failed' % (passed, len(failures)))
    sys.exit(1 if failures else 0)
