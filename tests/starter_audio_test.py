#!/usr/bin/env python3
"""Checks the generated retro starter audio set without listening (tools/audio).

usage: python3 tests/starter_audio_test.py REPO_ROOT [--summary]

Generates the set twice into a temp folder (WAV only: needs no ffmpeg) and checks:
files exist, durations, peak/RMS bands, effect edges near zero, music loop seams,
byte-identical determinism, and that audio.json.example is valid and matches the
contract (every referenced file exists, `use` keys are the standard set).
It also checks the committed copies (games/demo-economy and the editor templates): sizes under
budget, and that `make_starter_audio.py REPO_ROOT` run on a copy of them (a temp folder, never the
repo) changes nothing, twice, even though the OGG bytes would differ between ffmpeg versions; it
re-encodes a music file only when the recorded hash of its source WAV changes.
--summary also prints a per-file sanity table (dominant frequency, centroid, RMS).
"""
import cmath
import json
import math
import os
import re
import shutil
import subprocess
import sys

root = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.getcwd()
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.join(root, 'tools', 'audio'))
from wavio import read_wav  # noqa: E402
from ote_testkit import scratch_dir  # noqa: E402

CONTRACT_KEYS = ['cursor', 'confirm', 'cancel', 'error', 'step', 'hit', 'miss', 'critical',
                 'heal', 'defeated', 'capture', 'build', 'itemUse', 'gold', 'dialogue',
                 'turnStart', 'upgrade', 'research', 'repair', 'supply']
EFFECTS = [{'itemUse': 'item_use', 'turnStart': 'turn_start'}.get(k, k) for k in CONTRACT_KEYS if k != 'step'] + [
    'step1', 'step2', 'sword_hit', 'bow_shot', 'magic_cast', 'cannon_shot', 'hooves',
    'tank_move', 'potion', 'ambush', 'levelup_tick']
# id -> (min s, max s, loops)
MUSIC = {'title': (29.5, 30.5, True), 'battle': (39.5, 40.5, True), 'enemy': (29.5, 30.5, True),
         'scene': (29.5, 30.5, True), 'victory': (4.5, 6.0, False), 'defeat': (4.5, 6.0, False),
         'levelup': (1.5, 2.6, False)}

checks = 0
failures = []


def check(cond, msg):
    global checks
    checks += 1
    if not cond:
        failures.append(msg)


def load(path):
    rate, ch, data = read_wav(path)
    return rate, ch, [s / 32768.0 for s in data]


def rms(x):
    return math.sqrt(sum(v * v for v in x) / len(x))


def fft(a):
    n = len(a)
    if n == 1:
        return a
    ev, od = fft(a[0::2]), fft(a[1::2])
    t = [cmath.exp(-2j * math.pi * k / n) * od[k] for k in range(n // 2)]
    return [ev[k] + t[k] for k in range(n // 2)] + [ev[k] - t[k] for k in range(n // 2)]


def spectrum(x, rate):
    """(dominant Hz, centroid Hz) of the loudest 4096-sample window."""
    n = 4096
    if len(x) < n:
        x = x + [0.0] * (n - len(x))
    best = max(range(0, len(x) - n + 1, n // 2), key=lambda i: sum(v * v for v in x[i:i + n]))
    w = [x[best + i] * (0.5 - 0.5 * math.cos(2 * math.pi * i / n)) for i in range(n)]
    mag = [abs(c) for c in fft(w)[:n // 2]]
    dom = max(range(4, n // 2), key=lambda k: mag[k]) * rate / n
    tot = sum(mag) or 1.0
    cen = sum(k * m for k, m in enumerate(mag)) / tot * rate / n
    return dom, cen


def generate(out):
    subprocess.run([sys.executable, os.path.join(root, 'tools', 'audio', 'make_starter_audio.py'),
                    '--out', out], check=True, stdout=subprocess.DEVNULL)


def seam_stats(x):
    d = sorted(abs(x[i + 1] - x[i]) for i in range(len(x) - 1))
    return abs(x[0] - x[-1]), d[int(len(d) * 0.99)], d[-1]


EFFECT_BUDGET = 400 * 1024   # bytes of effect WAVs in one shipped copy


def snapshot(base, rels):
    """{relative path: (bytes, mtime_ns)} of every file below the given folders."""
    out = {}
    for rel in rels:
        for dp, _, fns in os.walk(os.path.join(base, rel)):
            for fn in fns:
                p = os.path.join(dp, fn)
                with open(p, 'rb') as f:
                    out[os.path.relpath(p, base)] = (f.read(), os.stat(p).st_mtime_ns)
    return out


def shipped_checks():
    sys.path.insert(0, os.path.join(root, 'tools', 'audio'))
    import targets
    rels = sorted(targets.TARGETS)
    for rel in rels:
        gdir = os.path.join(root, rel)
        wavs = [os.path.join(gdir, 'audio', f) for f in os.listdir(os.path.join(gdir, 'audio')) if f.endswith('.wav')]
        total = sum(os.path.getsize(w) for w in wavs)
        check(total < EFFECT_BUDGET, '%s: effects are %d KB, budget %d KB' % (rel, total // 1024, EFFECT_BUDGET // 1024))
        # The editor only lists, plays and removes lower-case names (editor/server/audio.cpp validAudioName).
        for f in os.listdir(os.path.join(gdir, 'audio')):
            check(re.fullmatch(r'[a-z0-9][a-z0-9_-]{0,39}\.(wav|ogg)', f) is not None, '%s: audio/%s is not a name the editor accepts' % (rel, f))
        want = {s + '.wav' for s in targets.profile_sounds(targets.TARGETS[rel])}
        check({os.path.basename(w) for w in wavs} == want, rel + ': effect files differ from targets.py')
        for w in wavs:
            rate, ch, x = load(w)
            check(rate == 22050 and ch == 1 and abs(x[0]) < 0.01 and abs(x[-1]) < 0.01, '%s: %s format or edges' % (rel, os.path.basename(w)))
    if not shutil.which('ffmpeg'):
        print('no ffmpeg: skipping the "rerun is a no-op" checks')
        return
    tmp = scratch_dir('starter_audio_rerun')
    try:
        sub = os.path.join(tmp, 'repo')
        shutil.copytree(os.path.join(root, 'tools', 'audio'), os.path.join(sub, 'tools', 'audio'),
                        ignore=shutil.ignore_patterns('__pycache__'))
        for rel in rels:   # the committed games, without their art: only what the tool reads and writes
            shutil.copytree(os.path.join(root, rel), os.path.join(sub, rel), ignore=shutil.ignore_patterns('art', 'maps'))
        tool = os.path.join(sub, 'tools', 'audio', 'make_starter_audio.py')
        before = snapshot(sub, rels)
        for n in (1, 2):
            res = subprocess.run([sys.executable, tool, sub], stdout=subprocess.PIPE, universal_newlines=True)
            check(res.returncode == 0, 'run %d failed: %s' % (n, res.stdout[-300:]))
            check(res.stdout.count('unchanged') == len(rels) and 'updated' not in res.stdout,
                  'run %d is not a no-op:\n%s' % (n, res.stdout))
            after = snapshot(sub, rels)
            check(after == before, 'run %d changed bytes or modification times: %s' % (n, sorted(k for k in after if after[k] != before.get(k))[:5]))
        # A source that changed (its recorded hash no longer matches) is re-encoded once and shared by every copy.
        mpath = os.path.join(sub, 'tools', 'audio', 'starter_audio.manifest.json')
        with open(mpath, encoding='utf-8') as f:
            good = f.read()
        man = json.loads(good)
        man['title'] = '0' * 64
        with open(mpath, 'w', encoding='utf-8') as f:
            json.dump(man, f)
        res = subprocess.run([sys.executable, tool, sub], stdout=subprocess.PIPE, universal_newlines=True)
        check(res.returncode == 0 and 'title.ogg' in res.stdout and res.stdout.count('title.ogg') == len(rels),
              'a changed hash must re-encode title.ogg in every copy:\n' + res.stdout)
        blobs = {open(os.path.join(sub, rel, 'audio', 'title.ogg'), 'rb').read() for rel in rels}
        check(len(blobs) == 1, 'the copies of a re-encoded track must be byte-identical')
        with open(mpath, encoding='utf-8') as f:
            check(json.load(f) == json.loads(good), 'the manifest must hold the real hash again after a re-encode')
        # A deleted OGG comes back from a sibling copy, bit for bit.
        gone = os.path.join(sub, rels[0], 'audio', 'battle.ogg')
        keep = open(os.path.join(sub, rels[1], 'audio', 'battle.ogg'), 'rb').read()
        os.remove(gone)
        subprocess.run([sys.executable, tool, sub], stdout=subprocess.DEVNULL, check=True)
        check(os.path.isfile(gone) and open(gone, 'rb').read() == keep, 'a missing OGG is restored from a sibling copy')
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def main():
    shipped_checks()
    tmp = scratch_dir('starter_audio')
    try:
        a, b = os.path.join(tmp, 'a'), os.path.join(tmp, 'b')
        generate(a)
        generate(b)
        rows = []
        for name in EFFECTS:
            p = os.path.join(a, 'audio', name + '.wav')
            check(os.path.isfile(p), name + ': file missing')
            if not os.path.isfile(p):
                continue
            rate, ch, x = load(p)
            sec = len(x) / rate
            check(rate == 22050 and ch == 1, name + ': not mono 22050')
            check(0.05 <= sec <= 1.2, '%s: duration %.3f out of range' % (name, sec))
            peak, r = max(abs(v) for v in x), rms(x)
            check(peak < 0.98, '%s: peak %.3f too hot' % (name, peak))
            check(0.08 <= r <= 0.3, '%s: rms %.3f outside 0.08..0.3' % (name, r))
            check(abs(x[0]) < 0.01 and abs(x[-1]) < 0.01, '%s: edges not near zero' % name)
            rows.append((name, sec, r, peak) + spectrum(x, rate))
        for name, (lo, hi, loops) in MUSIC.items():
            p = os.path.join(a, 'audio', name + '.wav')
            check(os.path.isfile(p), name + ': file missing')
            if not os.path.isfile(p):
                continue
            rate, ch, x = load(p)
            sec = len(x) / rate
            check(rate == 22050, name + ': rate')
            check(lo <= sec <= hi, '%s: duration %.2f outside %s..%s' % (name, sec, lo, hi))
            peak, r = max(abs(v) for v in x), rms(x)
            check(peak < 0.98, '%s: peak %.3f too hot' % (name, peak))
            check(0.05 <= r <= 0.3, '%s: rms %.3f outside 0.05..0.3' % (name, r))
            if loops:
                jump, p99, mx = seam_stats(x)
                check(jump <= max(p99, 0.02), '%s: loop jump %.4f vs p99 step %.4f' % (name, jump, p99))
            else:
                check(abs(x[0]) < 0.01 and abs(x[-1]) < 0.01, name + ': jingle edges not near zero')
            rows.append((name, sec, r, peak) + spectrum(x[:len(x) // 2 if sec > 20 else len(x)], rate))

        # determinism: every file byte-identical between the two runs
        for fn in sorted(os.listdir(os.path.join(a, 'audio'))):
            with open(os.path.join(a, 'audio', fn), 'rb') as f1, open(os.path.join(b, 'audio', fn), 'rb') as f2:
                check(f1.read() == f2.read(), fn + ': differs between runs')
        with open(os.path.join(a, 'audio.json.example'), 'rb') as f1, \
                open(os.path.join(b, 'audio.json.example'), 'rb') as f2:
            check(f1.read() == f2.read(), 'audio.json.example differs between runs')

        # audio.json.example against the contract
        with open(os.path.join(a, 'audio.json.example'), encoding='utf-8') as f:
            cfg = json.load(f)
        files = []
        for tid, t in cfg['music'].items():
            files.append(t['file'])
            check('loopStart' in t or t.get('loop') is False, tid + ': neither loopStart nor loop:false')
        for sid, s in cfg['sounds'].items():
            files += s['files'] if 'files' in s else [s['file']]
        for fn in files:
            check(os.path.isfile(os.path.join(a, fn)), 'referenced file missing: ' + fn)
        use = cfg['use']
        check(sorted(use['sounds']) == sorted(CONTRACT_KEYS), 'use.sounds keys differ from the contract list')
        for k, v in use['sounds'].items():
            check(v in cfg['sounds'], 'use.sounds.%s -> unknown sound %s' % (k, v))
        for k in ('titleMusic', 'mapMusic', 'enemyTurnMusic', 'sceneMusic', 'victory', 'defeat', 'levelUp'):
            check(use[k] in cfg['music'], 'use.%s -> unknown music %r' % (k, use[k]))
        for fn in EFFECTS:   # every generated effect is registered under its own id
            check(fn.rstrip('12') in cfg['sounds'] or fn in cfg['sounds'], fn + ': not in audio.json.example')

        if '--summary' in sys.argv:
            print('%-13s %6s %6s %6s %9s %9s' % ('file', 'sec', 'rms', 'peak', 'dom Hz', 'centroid'))
            for n, s, r, p, d, c in rows:
                print('%-13s %6.2f %6.3f %6.3f %9.0f %9.0f' % (n, s, r, p, d, c))
            for name, (_, _, loops) in MUSIC.items():
                if loops:
                    _, _, x = load(os.path.join(a, 'audio', name + '.wav'))
                    j, p99, mx = seam_stats(x)
                    print('seam %-8s jump %.4f  p99 step %.4f  max step %.4f' % (name, j, p99, mx))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    for m in failures:
        print('FAIL:', m)
    print('starter_audio_test: %d checks, %d failed' % (checks, len(failures)))
    sys.exit(1 if failures else 0)


main()
