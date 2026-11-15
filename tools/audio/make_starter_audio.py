#!/usr/bin/env python3
"""Synthesizes the engine's original retro starter audio set (docs/SOUND.md "Starter audio").

usage: python3 tools/audio/make_starter_audio.py --out DIR [--ogg]

Writes DIR/audio/*.wav (sound effects and music) and DIR/audio.json.example, a complete
data/audio.json for a game that uses the set. With --ogg the music is re-encoded to
OGG Vorbis with ffmpeg (quality 3); without ffmpeg it says so and keeps the WAVs.
Pure Python standard library, no input files, fixed LFSR seeds: two runs give
byte-identical WAVs. (OGG bytes depend on the ffmpeg/libvorbis version.)
"""
import argparse
import json
import os
import shutil
import subprocess
import sys

sys.dont_write_bytecode = True  # keep tools/audio free of __pycache__
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import music  # noqa: E402
import sfx  # noqa: E402
import tracks  # noqa: E402
from synth import RATE, master  # noqa: E402
from wavio import write_wav  # noqa: E402

MUSIC_RMS = 0.13    # music sits under the effects
JINGLE_RMS = 0.16

# The contract's standard set (docs/SOUND.md data/audio.json "use.sounds").
STANDARD_KEYS = ['cursor', 'confirm', 'cancel', 'error', 'step', 'hit', 'miss', 'critical',
                 'heal', 'defeated', 'capture', 'build', 'itemUse', 'gold', 'dialogue',
                 'turnStart', 'upgrade', 'research', 'repair', 'supply']
EXTRA_SOUNDS = ['sword_hit', 'bow_shot', 'magic_cast', 'cannon_shot', 'hooves', 'tank_move',
                'potion', 'ambush', 'levelup_tick']


def build_music(name):
    spec, loops = tracks.TRACKS[name]
    buf = music.render(spec, loop=loops)
    if loops:   # no fades: the loop point must stay as rendered
        return master(buf, MUSIC_RMS, fade_in=0, fade_out=0)
    return master(buf, JINGLE_RMS, fade_in=0.003, fade_out=0.02)


def encode_ogg(wav_path):
    ogg = wav_path[:-4] + '.ogg'
    subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', wav_path, '-c:a', 'libvorbis',
                    '-q:a', '3', ogg], check=True)
    os.remove(wav_path)
    return ogg


def audio_json(ext):
    music_ids = {}
    for name, (_, loops) in tracks.TRACKS.items():
        entry = {'file': 'audio/%s%s' % (name, ext)}
        if loops:
            entry['loopStart'] = 0
        else:
            entry['loop'] = False
        music_ids[name] = entry
    music_ids['battle']['volume'] = 80
    sounds = {}
    for key in STANDARD_KEYS + EXTRA_SOUNDS:
        if key == 'step':
            sounds['step'] = {'files': ['audio/step1.wav', 'audio/step2.wav']}
        else:
            sounds[key] = {'file': 'audio/%s.wav' % key}
    return {
        'music': music_ids,
        'sounds': sounds,
        'use': {
            'titleMusic': 'title', 'mapMusic': 'battle',
            'playerTurnMusic': '', 'enemyTurnMusic': 'enemy', 'sceneMusic': 'scene',
            'victory': 'victory', 'defeat': 'defeat', 'levelUp': 'levelup',
            'sounds': {k: k for k in STANDARD_KEYS},
        },
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--out', required=True, help='output folder (gets audio/ and audio.json.example)')
    ap.add_argument('--ogg', action='store_true', help='encode music to OGG Vorbis with ffmpeg')
    args = ap.parse_args()

    adir = os.path.join(args.out, 'audio')
    os.makedirs(adir, exist_ok=True)
    rows = []   # (file, seconds, bytes)

    for name in sfx.EFFECTS:
        path = os.path.join(adir, name + '.wav')
        buf = sfx.build_effect(name)
        write_wav(path, buf, RATE)
        rows.append((path, len(buf) / RATE, 'sfx'))

    ogg = args.ogg
    if ogg and not shutil.which('ffmpeg'):
        print('ffmpeg not found: keeping music as WAV (the set works without OGG).')
        ogg = False
    for name in tracks.TRACKS:
        path = os.path.join(adir, name + '.wav')
        buf = build_music(name)
        write_wav(path, buf, RATE)
        secs = len(buf) / RATE
        if ogg:
            path = encode_ogg(path)
        rows.append((path, secs, 'music'))

    with open(os.path.join(args.out, 'audio.json.example'), 'w', encoding='utf-8') as f:
        json.dump(audio_json('.ogg' if ogg else '.wav'), f, indent=2)
        f.write('\n')

    totals = {'sfx': 0, 'music': 0}
    print('%-34s %8s %10s' % ('file', 'seconds', 'bytes'))
    for path, secs, kind in rows:
        size = os.path.getsize(path)
        totals[kind] += size
        print('%-34s %8.2f %10d' % (os.path.relpath(path, args.out), secs, size))
    print('effects total %d bytes (%.0f KB); music total %d bytes (%.0f KB)'
          % (totals['sfx'], totals['sfx'] / 1024, totals['music'], totals['music'] / 1024))
    print('loopStart: 0 for title, battle, enemy, scene (no intros); jingles: loop=false')


if __name__ == '__main__':
    main()
