#!/usr/bin/env python3
"""Builds the demo's CC0 audio set (docs/SOUND.md "Starter audio", the alternative to the retro set).

usage: python3 tools/audio/make_cc0_set.py --src ORIGINALS [--game games/demo-srpg] [--credits]

ORIGINALS is a folder holding the downloaded originals, anywhere below it (files are found by
name, so the unzipped Kenney packs and single OpenGameArt files can sit side by side). The
originals are not part of the repository: `--credits` lists every source (page, file, licence) and
the files needed. Writes GAME/audio/*.ogg and GAME/data/audio.json (the whole standard set of
cues, music for the title, battles, enemy turn, boss, story scenes and the three jingles).

What it does to the originals (ffmpeg): mixes layers of several files into one effect where a
sound is made of two parts (a slice and a clang), trims leading and trailing silence, makes loops of
the long tracks by finding a stretch that can repeat and cross-fading its end into its start, mixes
down to mono at 32 kHz, scales every file to about the loudness of the retro set (RMS 0.16 effects,
0.13 music, 0.16 jingles, measured on the decoded samples like tests/starter_audio_test.py) with a
peak limiter, and encodes OGG Vorbis (effects 32 kHz mono q3 are about 6 KB per second).
The loop points below were chosen by comparing the sound of the two ends (spectral similarity over a
2 s window, see LOOPS); they were checked by numbers, not by ear. OGG bytes depend on the ffmpeg
version. Needs Python 3 and ffmpeg; nothing from the downloads is ever executed.
"""
import argparse
import array
import json
import math
import os
import shutil
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True

RATE = 32000
SFX_RMS, MUSIC_RMS, JINGLE_RMS = 0.16, 0.13, 0.16
PEAK = 0.85   # limiter ceiling; Vorbis can overshoot it a little, so the decoded peaks stay under 1.0
QUALITY = '3'

KENNEY = 'https://kenney.nl/assets/%s'
OGA = 'https://opengameart.org/content/%s'
# pack key -> (name, author, page, licence as shown on that page)
PACKS = {
    'rpg-audio': ('RPG Audio', 'Kenney Vleugels (Kenney.nl)', KENNEY % 'rpg-audio', 'CC0 1.0'),
    'interface-sounds': ('Interface Sounds', 'Kenney Vleugels (Kenney.nl)', KENNEY % 'interface-sounds', 'CC0 1.0'),
    'impact-sounds': ('Impact Sounds', 'Kenney Vleugels (Kenney.nl)', KENNEY % 'impact-sounds', 'CC0 1.0'),
    'music-jingles': ('Music Jingles', 'Kenney Vleugels (Kenney.nl)', KENNEY % 'music-jingles', 'CC0 1.0'),
    'bards-tale': ('Medieval: The Bard\'s Tale', 'RandomMind', OGA % 'medieval-the-bards-tale', 'CC0 1.0'),
    'old-tower-inn': ('Medieval: The Old Tower Inn', 'RandomMind', OGA % 'medieval-the-old-tower-inn', 'CC0 1.0'),
    'battle': ('Medieval: Battle', 'RandomMind', OGA % 'medieval-battle', 'CC0 1.0'),
    'rejoicing': ('Medieval: Rejoicing', 'RandomMind', OGA % 'medieval-rejoicing', 'CC0 1.0'),
    'defeat': ('Medieval: Defeat Theme', 'RandomMind', OGA % 'medieval-defeat-theme', 'CC0 1.0'),
    'orcs': ('Orcs Victorious', 'Bobjt', OGA % 'orcs-victorious', 'CC0 1.0'),
    'war-theme': ('War Theme', 'Spring Spring', OGA % 'war-theme', 'CC0 1.0'),
}

# Effect id -> layers [(pack, file stem, delay seconds)], longest allowed length. Where a cue has no
# fitting sound in the packs the closest one is used and noted here.
SFX = {
    # menus and the cursor (Interface Sounds)
    'cursor': dict(layers=[('interface-sounds', 'click_001', 0)], max=0.3),
    'confirm': dict(layers=[('interface-sounds', 'confirmation_002', 0)], max=0.8),
    'cancel': dict(layers=[('interface-sounds', 'back_002', 0)], max=0.6),
    'error': dict(layers=[('interface-sounds', 'error_004', 0)], max=0.6),
    'select': dict(layers=[('interface-sounds', 'select_003', 0)], max=0.4),
    'dialogue': dict(layers=[('interface-sounds', 'tick_004', 0)], max=0.12),
    # walking (RPG Audio footsteps; one picked each square)
    'step1': dict(layers=[('rpg-audio', 'footstep02', 0)], max=0.25),
    'step2': dict(layers=[('rpg-audio', 'footstep05', 0)], max=0.25),
    'step3': dict(layers=[('rpg-audio', 'footstep08', 0)], max=0.25),
    'hooves1': dict(layers=[('impact-sounds', 'impactWood_light_001', 0)], max=0.3),
    'hooves2': dict(layers=[('impact-sounds', 'impactTin_medium_000', 0)], max=0.3),
    'hooves3': dict(layers=[('impact-sounds', 'impactWood_light_003', 0)], max=0.3),
    'plate_step1': dict(layers=[('impact-sounds', 'impactPlate_light_000', 0)], max=0.35),
    'plate_step2': dict(layers=[('impact-sounds', 'impactPlate_light_002', 0)], max=0.35),
    'flutter1': dict(layers=[('rpg-audio', 'cloth2', 0)], max=0.35),
    'flutter2': dict(layers=[('rpg-audio', 'cloth3', 0)], max=0.35),
    # fights (Impact Sounds, RPG Audio); the pack has no bow or magic sounds of its own, so the bow is a
    # string pluck (Interface Sounds) and a soft thud, and the potion and magic are glass
    'hit': dict(layers=[('impact-sounds', 'impactPunch_medium_001', 0)], max=0.5),
    'sword_hit': dict(layers=[('rpg-audio', 'knifeSlice', 0), ('impact-sounds', 'impactMetal_medium_001', 0.06)], max=0.7),
    'lance_hit': dict(layers=[('impact-sounds', 'impactPlate_medium_002', 0)], max=0.6),
    'axe_hit': dict(layers=[('rpg-audio', 'chop', 0), ('impact-sounds', 'impactWood_heavy_001', 0.04)], max=0.6),
    'bow_hit': dict(layers=[('interface-sounds', 'pluck_001', 0), ('impact-sounds', 'impactSoft_medium_001', 0.16)], max=0.6),
    'miss': dict(layers=[('rpg-audio', 'knifeSlice2', 0)], max=0.5),
    'critical': dict(layers=[('impact-sounds', 'impactPlate_heavy_001', 0), ('impact-sounds', 'impactBell_heavy_001', 0.02)], max=0.9),
    'defeated': dict(layers=[('impact-sounds', 'impactSoft_heavy_002', 0), ('rpg-audio', 'dropLeather', 0.12)], max=0.9),
    # the rest of the standard set
    'heal': dict(layers=[('interface-sounds', 'maximize_005', 0), ('interface-sounds', 'glass_001', 0.1)], max=1.0),
    'capture': dict(layers=[('rpg-audio', 'doorOpen_2', 0), ('rpg-audio', 'metalLatch', 0.35)], max=1.0),
    'build': dict(layers=[('impact-sounds', 'impactPlank_medium_001', 0), ('impact-sounds', 'impactWood_heavy_001', 0.2)], max=0.9),
    'item_use': dict(layers=[('rpg-audio', 'handleSmallLeather', 0)], max=0.7),
    'potion': dict(layers=[('interface-sounds', 'glass_001', 0), ('interface-sounds', 'maximize_006', 0.08)], max=0.8),
    'magic': dict(layers=[('interface-sounds', 'glass_004', 0), ('interface-sounds', 'maximize_004', 0)], max=0.8),
    'gold': dict(layers=[('rpg-audio', 'handleCoins2', 0)], max=0.8),
    'turn_start': dict(layers=[('impact-sounds', 'impactBell_heavy_000', 0)], max=1.2),
    'upgrade': dict(layers=[('impact-sounds', 'impactMetal_light_002', 0), ('interface-sounds', 'confirmation_003', 0.1)], max=0.9),
    'research': dict(layers=[('rpg-audio', 'bookFlip2', 0)], max=0.9),
    'repair': dict(layers=[('impact-sounds', 'impactMetal_light_001', 0), ('impact-sounds', 'impactMetal_light_004', 0.16)], max=0.7),
    'supply': dict(layers=[('rpg-audio', 'beltHandle2', 0)], max=0.7),
    'ambush': dict(layers=[('rpg-audio', 'drawKnife3', 0)], max=0.8),
    'alarm': dict(layers=[('impact-sounds', 'impactBell_heavy_003', 0)], max=1.2),
}
# Standard cue -> sound id (a cue with variants lists the ids of the variants).
USE_SOUNDS = {
    'cursor': 'cursor', 'confirm': 'confirm', 'cancel': 'cancel', 'error': 'error',
    'step': ['step1', 'step2', 'step3'], 'hit': 'hit', 'miss': 'miss', 'critical': 'critical',
    'heal': 'heal', 'defeated': 'defeated', 'capture': 'capture', 'build': 'build',
    'itemUse': 'item_use', 'gold': 'gold', 'dialogue': 'dialogue', 'turnStart': 'turn_start',
    'upgrade': 'upgrade', 'research': 'research', 'repair': 'repair', 'supply': 'supply',
    'ambush': 'ambush', 'select': 'select',
}
# Sound id (shown in audio.json) -> its files, for the ones with variants or different file names.
VARIANTS = {'step': ['step1', 'step2', 'step3'], 'hooves': ['hooves1', 'hooves2', 'hooves3'],
            'plate_step': ['plate_step1', 'plate_step2'], 'flutter': ['flutter1', 'flutter2']}
HIDDEN = {s for v in VARIANTS.values() for s in v}   # files that are only listed as variants
SOUND_IDS = ['cursor', 'confirm', 'cancel', 'error', 'select', 'dialogue', 'step', 'hooves', 'plate_step',
             'flutter', 'hit', 'sword_hit', 'lance_hit', 'axe_hit', 'bow_hit', 'miss', 'critical', 'defeated',
             'heal', 'capture', 'build', 'item_use', 'potion', 'magic', 'gold', 'turn_start', 'upgrade',
             'research', 'repair', 'supply', 'ambush', 'alarm']

# Music: id -> how the original becomes the track. loop = (start, end, cross-fade seconds): the stretch
# start..end repeats and its end is blended into its start (found with the similarity search above);
# whole = the file already is a seamless loop (its author's "Loop" version); jingle = (start, length, fade out).
MUSIC = {
    'title': dict(pack='bards-tale', file='Loop_The_Bards_Tale', kind='whole'),
    'battle': dict(pack='battle', file='battle_8', kind='loop', loop=(24.05, 62.90, 2.0)),
    'enemy': dict(pack='orcs', file='orcs_victorious_2024', kind='loop', loop=(9.55, 56.00, 2.0)),
    'scene': dict(pack='old-tower-inn', file='Loop_The_Old_Tower_Inn', kind='whole'),
    'boss': dict(pack='war-theme', file='war theme_0', kind='loop', loop=(48.25, 74.25, 2.0)),
    'victory': dict(pack='rejoicing', file='Rejoicing_0', kind='jingle', jingle=(0, 11.0, 1.8)),
    'defeat': dict(pack='defeat', file='defeat_0', kind='jingle', jingle=(0, 10.0, 2.5)),
    'levelup': dict(pack='music-jingles', file='jingles_STEEL07', kind='jingle', jingle=(0, 1.6, 0.25)),
}
FILE_NAMES = {   # pack -> {stem: real file name} where the file's name is not stem + .ogg
    'bards-tale': {'Loop_The_Bards_Tale': 'Loop_The_Bards_Tale.wav'},
    'old-tower-inn': {'Loop_The_Old_Tower_Inn': 'Loop_The_Old_Tower_Inn.wav'},
    'battle': {'battle_8': 'battle_8.mp3'},
    'rejoicing': {'Rejoicing_0': 'Rejoicing_0.mp3'},
    'defeat': {'defeat_0': 'defeat_0.mp3'},
    'orcs': {'orcs_victorious_2024': 'orcs_victorious_2024.mp3'},
    'war-theme': {'war theme_0': 'war theme_0.ogg'},
}


def real_name(pack, stem):
    return FILE_NAMES.get(pack, {}).get(stem, stem + '.ogg')


def find(src, pack, stem):
    name = real_name(pack, stem)
    hits = []
    for dirpath, _, files in os.walk(src, followlinks=True):
        if name in files:
            hits.append(os.path.join(dirpath, name))
    if len(hits) != 1:
        sys.exit('%s: %s in %r (%s); download it from %s' % (
            'not found' if not hits else 'found %d times' % len(hits), name, src, PACKS[pack][0], PACKS[pack][2]))
    return hits[0]


def ffmpeg(inputs, graph, out_args):
    """Runs ffmpeg on input files through a filter graph that ends in [out]; returns its stdout."""
    cmd = ['ffmpeg', '-v', 'error', '-nostdin', '-y']
    for i in inputs:
        cmd += ['-i', i]
    cmd += ['-filter_complex', graph, '-map', '[out]'] + out_args
    p = subprocess.run(cmd, stdout=subprocess.PIPE)
    if p.returncode:
        sys.exit('ffmpeg failed: ' + ' '.join(cmd))
    return p.stdout


MONO = 'aresample=%d,aformat=sample_fmts=fltp:channel_layouts=mono' % RATE


def sfx_graph(spec):
    parts, labels = [], ''
    for i, (_, _, delay) in enumerate(spec['layers']):
        chain = '[%d:a]%s,silenceremove=start_periods=1:start_threshold=-50dB' % (i, MONO)
        if delay:
            chain += ',adelay=%d' % int(round(delay * 1000))
        parts.append(chain + '[l%d]' % i)
        labels += '[l%d]' % i
    n = len(spec['layers'])
    parts.append(labels + ('amix=inputs=%d:normalize=0:duration=longest[m]' % n if n > 1 else 'anull[m]'))
    # cut to the longest allowed, drop a silent end, and fade the end out whatever its length is
    parts.append('[m]atrim=0:%g,areverse,silenceremove=start_periods=1:start_threshold=-55dB,'
                 'afade=t=in:d=0.03,areverse[out]' % spec['max'])
    return ';'.join(parts)


def loop_graph(start, end, x):
    return ('[0:a]%s,asplit=3[a][b][c];'
            '[a]atrim=%g:%g,asetpts=PTS-STARTPTS,afade=t=in:d=%g:curve=qsin[hin];'
            '[b]atrim=%g:%g,asetpts=PTS-STARTPTS,afade=t=out:d=%g:curve=qsin[hout];'
            '[hin][hout]amix=inputs=2:normalize=0:duration=first[h];'
            '[c]atrim=%g:%g,asetpts=PTS-STARTPTS[body];'
            '[h][body]concat=n=2:v=0:a=1[out]'
            % (MONO, start, start + x, x, end, end + x, x, start + x, end))


def jingle_graph(start, length, fade):
    return ('[0:a]%s,atrim=%g:%g,asetpts=PTS-STARTPTS,afade=t=out:st=%g:d=%g[out]'
            % (MONO, start, start + length, max(0.0, length - fade), fade))


def decode_stats(raw):
    x = array.array('h')
    x.frombytes(raw)
    n = len(x)
    rms = math.sqrt(sum(v * v for v in x) / max(1, n)) / 32768
    peak = max((abs(v) for v in x), default=0) / 32768
    return n / RATE, rms, peak


def render(inputs, graph, target_rms, out_path):
    """Graph -> raw samples -> measure -> gain + limiter -> OGG. Returns (seconds, rms before, bytes)."""
    raw = ffmpeg(inputs, graph, ['-f', 's16le', '-ar', str(RATE), '-ac', '1', '-'])
    secs, rms, peak = decode_stats(raw)
    if rms < 1e-4:
        sys.exit('%s: the result is silent' % out_path)
    # the last volume leaves headroom for the codec's overshoot (a decoded peak of 1.0 would clip)
    filt = 'volume=%.4f,alimiter=limit=%g:attack=3:release=40:level=disabled,volume=0.9' % (target_rms / rms / 0.9, PEAK)
    cmd = ['ffmpeg', '-v', 'error', '-nostdin', '-y', '-f', 's16le', '-ar', str(RATE), '-ac', '1', '-i', '-',
           '-af', filt, '-c:a', 'libvorbis', '-q:a', QUALITY, '-ar', str(RATE), '-ac', '1',
           '-map_metadata', '-1', '-fflags', '+bitexact', out_path]
    if subprocess.run(cmd, input=raw).returncode:
        sys.exit('ffmpeg failed: ' + ' '.join(cmd))
    return secs, rms, os.path.getsize(out_path)


def audio_json():
    sounds = {}
    for sid in SOUND_IDS:
        if sid in VARIANTS:
            sounds[sid] = {'files': ['audio/%s.ogg' % v for v in VARIANTS[sid]]}
        else:
            sounds[sid] = {'file': 'audio/%s.ogg' % sid}
    music = {}
    for mid, m in MUSIC.items():
        entry = {'file': 'audio/%s.ogg' % mid}
        if m['kind'] == 'jingle':
            entry['loop'] = False
        else:
            entry['loopStart'] = 0
        music[mid] = entry
    use_sounds = {}
    for cue, sid in USE_SOUNDS.items():
        use_sounds[cue] = 'step' if isinstance(sid, list) else sid
    return {
        'music': music,
        'sounds': sounds,
        'use': {
            'titleMusic': 'title', 'mapMusic': 'battle', 'playerTurnMusic': '', 'enemyTurnMusic': 'enemy',
            'sceneMusic': 'scene', 'victory': 'victory', 'defeat': 'defeat', 'levelUp': 'levelup',
            'sounds': use_sounds,
        },
    }


def credits():
    rows = {}
    for spec in SFX.values():
        for pack, stem, _ in spec['layers']:
            rows.setdefault(pack, set()).add(real_name(pack, stem))
    for m in MUSIC.values():
        rows.setdefault(m['pack'], set()).add(real_name(m['pack'], m['file']))
    for pack, files in rows.items():
        name, author, page, lic = PACKS[pack]
        print('%s | %s | %s | %s | %s' % (name, author, page, lic, ', '.join(sorted(files))))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--src', help='folder with the downloaded originals')
    ap.add_argument('--game', default='games/demo-srpg', help='game folder to write audio/ and data/audio.json into')
    ap.add_argument('--credits', action='store_true', help='print the sources (name | author | page | licence | files)')
    args = ap.parse_args()
    if args.credits:
        credits()
        return
    if not args.src or not shutil.which('ffmpeg'):
        ap.error('--src is needed (and ffmpeg)')
    gdir = os.path.abspath(args.game)
    if not os.path.isfile(os.path.join(gdir, 'game.json')):
        sys.exit('%s is not a game folder' % gdir)
    adir = os.path.join(gdir, 'audio')
    os.makedirs(adir, exist_ok=True)
    total, rows = 0, []
    for sid, spec in SFX.items():
        inputs = [find(args.src, p, s) for p, s, _ in spec['layers']]
        secs, rms, size = render(inputs, sfx_graph(spec), SFX_RMS, os.path.join(adir, sid + '.ogg'))
        rows.append((sid, secs, rms, size))
    for mid, m in MUSIC.items():
        src = find(args.src, m['pack'], m['file'])
        if m['kind'] == 'loop':
            graph = loop_graph(*m['loop'])
        elif m['kind'] == 'jingle':
            graph = jingle_graph(*m['jingle'])
        else:
            graph = '[0:a]%s[out]' % MONO
        secs, rms, size = render([src], graph, JINGLE_RMS if m['kind'] == 'jingle' else MUSIC_RMS,
                                 os.path.join(adir, mid + '.ogg'))
        rows.append((mid, secs, rms, size))
    # effects that are only listed as variants keep their own file; none are left over from older runs
    wanted = {r[0] + '.ogg' for r in rows}
    for fn in os.listdir(adir):
        if fn.endswith('.ogg') and fn not in wanted:
            os.remove(os.path.join(adir, fn))
    with open(os.path.join(gdir, 'data', 'audio.json'), 'w', encoding='utf-8') as f:
        json.dump(audio_json(), f, indent=2)
        f.write('\n')
    print('%-14s %8s %9s %8s' % ('file', 'seconds', 'rms(in)', 'bytes'))
    for sid, secs, rms, size in rows:
        total += size
        print('%-14s %8.2f %9.3f %8d' % (sid, secs, rms, size))
    print('total %d bytes (%.0f KB)' % (total, total / 1024))


if __name__ == '__main__':
    main()
