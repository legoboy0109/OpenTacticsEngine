#!/usr/bin/env python3
"""Synthesizes the engine's original retro starter audio set (docs/SOUND.md "Starter audio").

usage: python3 tools/audio/make_starter_audio.py [REPO_ROOT]          regenerate into the shipped games
       python3 tools/audio/make_starter_audio.py --out DIR [--ogg]    the whole set into one folder

With REPO_ROOT (like tools/art/make_starter_art.py) it writes audio/ and data/audio.json into
games/demo-economy and the three editor templates (tools/audio/targets.py says what each gets):
effects as WAV, music as OGG Vorbis (ffmpeg, quality 3). Because the OGG bytes depend on the
ffmpeg / libvorbis version, an OGG is only re-encoded when the hash of its source WAV differs
from the one recorded in tools/audio/starter_audio.manifest.json (or the file is missing); a
rerun on any machine therefore changes nothing.

With --out DIR it writes DIR/audio/*.wav (every effect and track) and DIR/audio.json.example, a
complete data/audio.json for a game that uses the whole set; --ogg re-encodes the music (no
ffmpeg: it says so and keeps WAV).

Pure Python standard library plus ffmpeg for the OGG files; no input files, fixed LFSR seeds:
two runs give byte-identical WAVs.
"""
import argparse
import hashlib
import io
import json
import os
import shutil
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True  # keep tools/audio free of __pycache__
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import music  # noqa: E402
import sfx  # noqa: E402
import targets  # noqa: E402
import tracks  # noqa: E402
from synth import RATE, master  # noqa: E402
from wavio import write_wav  # noqa: E402

MUSIC_RMS = 0.13    # music sits under the effects
JINGLE_RMS = 0.16
MANIFEST = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'starter_audio.manifest.json')


def build_music(name):
    spec, loops = tracks.TRACKS[name]
    buf = music.render(spec, loop=loops)
    if loops:   # no fades: the loop point must stay as rendered
        return master(buf, MUSIC_RMS, fade_in=0, fade_out=0)
    return master(buf, JINGLE_RMS, fade_in=0.003, fade_out=0.02)


def wav_bytes(buf):
    bio = io.BytesIO()
    write_wav(bio, buf, RATE)
    return bio.getvalue()


def encode_ogg(wav_path, ogg_path):
    subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', wav_path, '-c:a', 'libvorbis',
                    '-q:a', '3', ogg_path], check=True)


def write_if_changed(path, data):
    """Writes bytes unless the file already holds them (keeps mtimes, makes reruns silent)."""
    if os.path.isfile(path):
        with open(path, 'rb') as f:
            if f.read() == data:
                return False
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'wb') as f:
        f.write(data)
    return True


def json_bytes(obj):
    return (json.dumps(obj, indent=2) + '\n').encode('utf-8')


def loops_table():
    return {name: loops for name, (_, loops) in tracks.TRACKS.items()}


# ---------------------------------------------------------------- --out DIR

def run_out(out, ogg):
    adir = os.path.join(out, 'audio')
    os.makedirs(adir, exist_ok=True)
    rows = []   # (file, seconds, kind)
    for name in sfx.EFFECTS:
        buf = sfx.build_effect(name)
        write_wav(os.path.join(adir, name + '.wav'), buf, RATE)
        rows.append((os.path.join(adir, name + '.wav'), len(buf) / RATE, 'sfx'))
    if ogg and not shutil.which('ffmpeg'):
        print('ffmpeg not found: keeping music as WAV (the set works without OGG).')
        ogg = False
    for name in tracks.TRACKS:
        path = os.path.join(adir, name + '.wav')
        buf = build_music(name)
        write_wav(path, buf, RATE)
        secs = len(buf) / RATE
        if ogg:
            encode_ogg(path, path[:-4] + '.ogg')
            os.remove(path)
            path = path[:-4] + '.ogg'
        rows.append((path, secs, 'music'))
    stems = []
    for k in targets.STANDARD_KEYS:
        stems += ['step1', 'step2'] if k == 'step' else [k]
    stems += targets.EXTRA_SOUNDS
    cfg = targets.audio_json(list(tracks.TRACKS), loops_table(), stems, scene=True,
                             ext='.ogg' if ogg else '.wav')
    write_if_changed(os.path.join(out, 'audio.json.example'), json_bytes(cfg))
    totals = {'sfx': 0, 'music': 0}
    print('%-34s %8s %10s' % ('file', 'seconds', 'bytes'))
    for path, secs, kind in rows:
        size = os.path.getsize(path)
        totals[kind] += size
        print('%-34s %8.2f %10d' % (os.path.relpath(path, out), secs, size))
    print('effects total %d bytes (%.0f KB); music total %d bytes (%.0f KB)'
          % (totals['sfx'], totals['sfx'] / 1024, totals['music'], totals['music'] / 1024))
    print('loopStart: 0 for title, battle, enemy, scene (no intros); jingles: loop=false')


# ---------------------------------------------------------------- REPO_ROOT

def run_repo(root):
    if not shutil.which('ffmpeg'):
        sys.exit('ffmpeg not found: the shipped music is OGG, so it is needed to (re)generate it.')
    try:
        with open(MANIFEST, encoding='utf-8') as f:
            manifest = json.load(f)
    except FileNotFoundError:
        manifest = {}
    old_manifest = json.dumps(manifest, sort_keys=True)

    effects = {}   # stem -> wav bytes, built once
    wavs = {}      # track -> (wav bytes, sha256)
    for path, prof in targets.TARGETS.items():
        for stem in targets.profile_sounds(prof):
            if stem not in effects:
                effects[stem] = wav_bytes(sfx.build_effect(stem))
        for name in targets.profile_music(prof):
            if name not in wavs:
                data = wav_bytes(build_music(name))
                wavs[name] = (data, hashlib.sha256(data).hexdigest())

    stale = {n: manifest.get(n) != wavs[n][1] for n in wavs}   # source changed since the OGG was made
    tmp = tempfile.mkdtemp(prefix='ote_starter_audio_')
    encoded = {}   # track -> path of a freshly encoded or copied-from-a-game OGG
    try:
        def source_ogg(name):
            """One OGG per track for this run: an existing shipped copy whose recorded hash still
            matches (so every copy stays byte-identical), else one fresh encode."""
            if name in encoded:
                return encoded[name]
            if not stale[name]:
                for p in targets.TARGETS:
                    cand = os.path.join(root, p, 'audio', name + '.ogg')
                    if os.path.isfile(cand):
                        encoded[name] = cand
                        return cand
            wav = os.path.join(tmp, name + '.wav')
            with open(wav, 'wb') as f:
                f.write(wavs[name][0])
            ogg = os.path.join(tmp, name + '.ogg')
            encode_ogg(wav, ogg)
            encoded[name] = ogg
            return ogg

        generated = set(sfx.EFFECTS) | set(tracks.TRACKS)
        print('%-36s %9s %9s  %s' % ('copy', 'effects', 'music', 'changes'))
        for path, prof in targets.TARGETS.items():
            gdir = os.path.join(root, path)
            if not os.path.isfile(os.path.join(gdir, 'game.json')):
                sys.exit('%s is not a game folder (no game.json)' % gdir)
            adir = os.path.join(gdir, 'audio')
            changed, sfx_bytes, mus_bytes = [], 0, 0
            stems, names = targets.profile_sounds(prof), targets.profile_music(prof)
            for stem in stems:
                if write_if_changed(os.path.join(adir, stem + '.wav'), effects[stem]):
                    changed.append(stem + '.wav')
                sfx_bytes += len(effects[stem])
            for name in names:
                dst = os.path.join(adir, name + '.ogg')
                if stale[name] or not os.path.isfile(dst):
                    src = source_ogg(name)
                    if os.path.abspath(src) != os.path.abspath(dst):
                        os.makedirs(adir, exist_ok=True)
                        shutil.copyfile(src, dst)
                        changed.append(name + '.ogg')
                mus_bytes += os.path.getsize(dst)
            wanted = {s + '.wav' for s in stems} | {n + '.ogg' for n in names}
            if os.path.isdir(adir):   # files this tool owns that the profile no longer wants
                for fn in sorted(os.listdir(adir)):
                    if fn not in wanted and os.path.splitext(fn)[0] in generated:
                        os.remove(os.path.join(adir, fn))
                        changed.append('-' + fn)
            cfg = targets.audio_json(names, loops_table(), stems, prof.get('use_extra'),
                                     scene=bool(prof.get('scene')))
            if write_if_changed(os.path.join(gdir, 'data', 'audio.json'), json_bytes(cfg)):
                changed.append('data/audio.json')
            print('%-36s %7.0f K %7.0f K  %s' % (path, sfx_bytes / 1024, mus_bytes / 1024,
                                                  ', '.join(changed) if changed else 'unchanged'))
        manifest.update({n: wavs[n][1] for n in wavs if stale[n]})
        if json.dumps(manifest, sort_keys=True) != old_manifest:
            write_if_changed(MANIFEST, json_bytes(dict(sorted(manifest.items()))))
            print('updated', os.path.relpath(MANIFEST, root))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('root', nargs='?', help='repository root: regenerate into the shipped games')
    ap.add_argument('--out', help='write the whole set into this folder instead')
    ap.add_argument('--ogg', action='store_true', help='with --out: encode music to OGG Vorbis')
    args = ap.parse_args()
    if args.out and args.root:
        ap.error('give either REPO_ROOT or --out DIR')
    if args.out:
        run_out(args.out, args.ogg)
    else:
        run_repo(os.path.abspath(args.root or '.'))


if __name__ == '__main__':
    main()
