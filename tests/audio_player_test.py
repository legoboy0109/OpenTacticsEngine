#!/usr/bin/env python3
"""The player's sound foundation (docs/SOUND.md, build order step 1), windowless and without a sound device.

  python3 tests/audio_player_test.py ./build/engine/player/ote-player .

Checks, through the real ote-player: the command-line flags, the Music / Sound volume options (driven with
--input, read back from the settings file and from --audio-log, with keyboard and touch), the null device
(--headless, --no-audio) running the self-test, and --audio-render, which writes what the mixer would play
to a WAV so the mixing can be measured: the rendered sine's pitch after 22050 -> 44100 Hz resampling,
levels per volume setting, a seamless loop of a synthetic file, the two committed Ogg fixtures, and
that two runs write identical samples.

Never opens a window or a sound device: every run is --headless or --audio-render.
"""
import array
import hashlib
import json
import math
import os
import struct
import subprocess
import sys
import tempfile
import wave

PLAYER = ''
REPO = ''
failures = []
passed = 0


def check(name, ok, detail=''):
    global passed
    if ok:
        passed += 1
    else:
        failures.append(name)
        print('FAIL %s %s' % (name, detail))


def run(args, timeout=60):
    """Runs the player; returns (exit code, stdout + stderr)."""
    cmd = [PLAYER] + args
    res = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    return res.returncode, res.stdout + res.stderr


def headless(tmp, settings, extra, log=None):
    """A windowless run on its own settings file; returns (code, output, audio log lines)."""
    log = log or os.path.join(tmp, 'audio.log')
    if os.path.exists(log):
        os.remove(log)
    args = ['--headless', '--no-saving', '--settings', settings, '--audio-log', log,
            '--games', os.path.join(REPO, 'games'), '--assets', os.path.join(REPO, 'assets')] + extra
    code, out = run(args)
    lines = open(log).read().splitlines() if os.path.exists(log) else []
    return code, out, lines


def read_wav(path):
    """(rate, channels, samples as a list of lists per channel)."""
    w = wave.open(path, 'rb')
    ch, rate, n, bits = w.getnchannels(), w.getframerate(), w.getnframes(), w.getsampwidth()
    raw = w.readframes(n)
    w.close()
    data = array.array('h')
    data.frombytes(raw)
    return rate, ch, [list(data[c::ch]) for c in range(ch)], bits


def rms(x, a, b):
    seg = x[a:b]
    return math.sqrt(sum(v * v for v in seg) / max(1, len(seg)))


def up_crossings(x, a, b):
    return sum(1 for i in range(max(a, 1), b) if x[i - 1] < 0 <= x[i])


def render(tmp, name, extra, seconds=4):
    path = os.path.join(tmp, name)
    code, out = run(['--audio-render', path, '--seconds', str(seconds)] + extra)
    return code, out, path


def write_tone_wav(path, rate, freq_hz, frames, amp=0.5):
    w = wave.open(path, 'wb')
    w.setnchannels(1)
    w.setsampwidth(2)
    w.setframerate(rate)
    w.writeframes(b''.join(struct.pack('<h', int(round(math.sin(2 * math.pi * freq_hz * i / rate) * amp * 32767)))
                           for i in range(frames)))
    w.close()


# ---- command line

def test_flags(tmp):
    for bad, why in ((['--music-volume', '11'], '--music-volume'), (['--sound-volume', '-1'], '--sound-volume'),
                     (['--music-volume', 'loud'], '--music-volume'), (['--seconds', '2'], '--seconds'),
                     (['--audio-test', '--audio-render', 'x.wav'], '--audio-test'),
                     (['--audio-file', 'x.ogg'], '--audio-file')):
        code, out = run(['--headless'] + bad)
        check('flags: %s is refused' % ' '.join(bad), code == 1 and why in out, out[:200])
    code, out = run(['--help'])
    check('flags: --help lists the sound flags', code == 0 and all(f in out for f in
          ('--music-volume', '--sound-volume', '--no-audio', '--audio-log', '--audio-test', '--audio-render')))


# ---- the null device

def test_null_device(tmp):
    settings = os.path.join(tmp, 's.json')
    expect = ['music play melody fade=0 loop=1 level=7', 'sound chime vol=100 level=7', 'sound chime vol=100 level=7',
              'music stop fade=0.5']
    for flags in (['--audio-test'], ['--audio-test', '--no-audio']):
        code, out, lines = headless(tmp, settings, flags + ['--no-settings'])
        check('null device: %s exits 0' % ' '.join(flags), code == 0, out[-300:])
        body = [l.split(' ', 1)[1] for l in lines]
        check('null device: %s logs the requests' % ' '.join(flags), body[1:] == expect, '\n'.join(lines))
        times = [float(l.split()[0][2:]) for l in lines]
        check('null device: times follow the script (0, 1, 2, 3 s)',
              abs(times[2] - 1.0) < 0.05 and abs(times[3] - 2.0) < 0.05 and abs(times[4] - 3.0) < 0.05, str(times))
        check('null device: a headless run opens no sound device', 'audio: SDL' not in out and 'no sound device' not in out)
    code, out, lines = headless(tmp, settings, ['--audio-test', '--no-settings', '--no-audio'])
    check('null device: --no-audio says so', 'audio: off' in out, out[-300:])
    # Deterministic: the same lines every time.
    code, out, again = headless(tmp, settings, ['--audio-test', '--no-settings'])
    code, out, first = headless(tmp, settings, ['--audio-test', '--no-settings'])
    check('null device: the log is the same on every run', first == again and len(first) == 5)
    # A game plays exactly as before: no sound requests at all.
    code, out, lines = headless(tmp, settings, ['--no-settings', '--game', 'demo-srpg', '--map', 'bridge', '--frames', '30'])
    check('null device: a game without audio makes no sound requests', code == 0 and len(lines) == 1 and 'volumes' in lines[0], str(lines))
    # The log file cannot be written: a warning, not an error.
    code, out, lines = headless(tmp, settings, ['--no-settings', '--frames', '3'], log=os.path.join(tmp, 'nodir', 'a.log'))
    check('null device: an unwritable --audio-log is only a warning', code == 0 and 'cannot write the audio log' in out, out[-300:])


# ---- the options

# Options screen from the home screen (Options is the third entry), then one row per option: Font, Text size,
# UI scale, Visual style, 3D graphics, 3D resolution, Music volume, Sound volume.
TO_OPTIONS = 'down,down,confirm'
DOWN_TO_MUSIC = ',down' * 6


def test_options(tmp):
    settings = os.path.join(tmp, 'opt.json')
    # Keyboard: Right from "Default" goes to Off (0), once more to 1.
    code, out, lines = headless(tmp, settings, ['--input', TO_OPTIONS + DOWN_TO_MUSIC + ',right,right,wait:2'])
    s = json.load(open(settings))
    check('options: music volume is saved in the settings file', code == 0 and s['global'].get('musicVolume') == 1, str(s))
    body = [l.split(' ', 1)[1] for l in lines]
    check('options: each step plays a tick at the new level',
          body == ['volumes music=7 sound=7', 'volumes music=0 sound=7', 'sound tick vol=100 bus=music level=0',
                   'volumes music=1 sound=7', 'sound tick vol=100 bus=music level=1'], '\n'.join(lines))
    # Sound volume next row; Left from "Default" wraps to 10.
    code, out, lines = headless(tmp, settings, ['--input', TO_OPTIONS + DOWN_TO_MUSIC + ',down,left,wait:2'])
    s = json.load(open(settings))
    check('options: sound volume is saved (Left from the default wraps to 10)',
          s['global'].get('soundVolume') == 10 and s['global'].get('musicVolume') == 1, str(s))
    check('options: the sound slider ticks on the sound level',
          any(l.endswith('sound tick vol=100 bus=sound level=10') for l in lines), '\n'.join(lines))
    # The next run starts with the saved volumes.
    code, out, lines = headless(tmp, settings, ['--frames', '3'])
    check('options: saved volumes are used at the next start', lines and lines[0].endswith('volumes music=1 sound=10'), str(lines))
    # A command-line volume beats the saved option for that run only.
    code, out, lines = headless(tmp, settings, ['--frames', '3', '--music-volume', '9', '--sound-volume', '0'])
    check('options: --music-volume / --sound-volume beat the options', lines and lines[0].endswith('volumes music=9 sound=0'), str(lines))
    s = json.load(open(settings))
    check('options: ...and are not saved', s['global'].get('musicVolume') == 1)
    # Touch: tap the row's value (steps forward), tap its left arrow (steps back). Rows are 14 px apart from y=45.
    touch = os.path.join(tmp, 'touch.json')
    code, out, lines = headless(tmp, touch, ['--input', TO_OPTIONS + ',tap:300:130,tap:300:130,tap:146:130,wait:2'])
    s = json.load(open(touch))
    body = [l.split(' ', 1)[1] for l in lines if 'tick' not in l]
    check('options: touch steps the volume forward and back', s['global'].get('musicVolume') == 0 and
          body[-3:] == ['volumes music=0 sound=7', 'volumes music=1 sound=7', 'volumes music=0 sound=7'], str(lines))
    # Reset to defaults: back to 7 with no tick.
    code, out, lines = headless(tmp, settings, ['--input', TO_OPTIONS + ',down' * 8 + ',confirm,wait:2'])
    s = json.load(open(settings))
    check('options: Reset to defaults clears the volumes', 'musicVolume' not in s['global'] and 'soundVolume' not in s['global'], str(s))
    check('options: ...without a tick', not any('tick' in l for l in lines), '\n'.join(lines))
    # The same script twice writes the same log (deterministic with --fixed-dt).
    a = headless(tmp, os.path.join(tmp, 'd1.json'), ['--input', TO_OPTIONS + DOWN_TO_MUSIC + ',right,wait:2'])[2]
    b = headless(tmp, os.path.join(tmp, 'd2.json'), ['--input', TO_OPTIONS + DOWN_TO_MUSIC + ',right,wait:2'])[2]
    check('options: the log of a scripted run is reproducible', a == b and len(a) == 3, str(a))


# ---- --audio-render

def test_render(tmp):
    code, out, path = render(tmp, 'a.wav', [])
    check('render: exits 0 and writes the file', code == 0 and os.path.exists(path), out[-300:])
    rate, ch, x, bits = read_wav(path)
    check('render: 16-bit stereo at 44100 Hz', (rate, ch, bits) == (44100, 2, 2), str((rate, ch, bits)))
    check('render: exactly --seconds long', len(x[0]) == 4 * 44100, str(len(x[0])))
    check('render: both channels carry the same (mono) mix', x[0] == x[1])
    # The melody: C4 (261.6 Hz square wave) for the first 0.225 s.
    n = up_crossings(x[0], 2000, int(0.22 * 44100))
    expect = 261.63 * (0.22 - 2000 / 44100.0)
    check('render: the first note has the pitch of C4', abs(n - expect) <= 2, '%d crossings, expected %.1f' % (n, expect))
    check('render: the music is audible', rms(x[0], 0, 22050) > 2000)
    # The chime at t = 1.0 s (22050 Hz source -> 44100 Hz): an 880 Hz note then 1318.5 Hz, on top of the melody.
    check('render: a chime joins at 1 s', rms(x[0], 44100, 44100 + 4000) > rms(x[0], 44100 - 4000, 44100) * 1.0 - 1)
    # The music fades out from t = 3.0 s over 0.5 s and then it is silent. (What is asked at time t is
    # heard about 80 ms later: the player keeps 80 ms queued in the device.)
    check('render: the music fades out after 3 s', rms(x[0], int(3.62 * 44100), 4 * 44100) == 0 and
          rms(x[0], int(3.0 * 44100), int(3.1 * 44100)) > 2 * rms(x[0], int(3.45 * 44100), int(3.55 * 44100)))
    # Two runs write the same samples.
    code, out, path2 = render(tmp, 'b.wav', [])
    check('render: two runs are byte-identical', hashlib.sha256(open(path, 'rb').read()).hexdigest() ==
          hashlib.sha256(open(path2, 'rb').read()).hexdigest())

    # Volumes.
    def levels(extra):
        c, o, p = render(tmp, 'v.wav', extra)
        return read_wav(p)[2][0]

    off = levels(['--music-volume', '0', '--sound-volume', '10'])
    check('render: music level 0 is silent', rms(off, 0, 40000) == 0 and rms(off, 44100, 44100 + 5000) > 1000)
    loud, quiet = levels(['--music-volume', '0', '--sound-volume', '10']), levels(['--music-volume', '0', '--sound-volume', '5'])
    ratio = rms(loud, 44100, 44100 + 8000) / max(1.0, rms(quiet, 44100, 44100 + 8000))
    check('render: sound level 5 is a quarter of level 10 (perceptual curve)', abs(ratio - 4.0) < 0.4, str(ratio))
    silent = levels(['--music-volume', '10', '--sound-volume', '0'])
    check('render: sound level 0 plays no chime (the music alone is the same before the chime)',
          silent[:40000] == levels(['--music-volume', '10', '--sound-volume', '10'])[:40000] and rms(silent, 44100, 50000) > 500)
    # The audio log of a render.
    log = os.path.join(tmp, 'r.log')
    run(['--audio-render', os.path.join(tmp, 'l.wav'), '--seconds', '1', '--audio-log', log, '--music-volume', '4'])
    lines = open(log).read().splitlines()
    check('render: writes the audio log too', lines[:2] == ['t=0.000 volumes music=4 sound=7',
                                                           't=0.000 music play melody fade=0 loop=1 level=4'], str(lines))
    code, out = run(['--audio-render', os.path.join(tmp, 'nodir', 'x.wav'), '--seconds', '1'])
    check('render: an unwritable file is an error (exit 1)', code == 1 and 'cannot write' in out)


def test_render_files(tmp):
    # 440 Hz at 22050 Hz as a WAV file: the pitch survives the resampling to 44100 Hz.
    wav = os.path.join(tmp, 'tone22.wav')
    write_tone_wav(wav, 22050, 441.0, 2200)  # 50 frames per period, 44 whole periods
    code, out, path = render(tmp, 'f1.wav', ['--audio-file', wav, '--music-volume', '10'], seconds=0.9)
    x = read_wav(path)[2][0]
    n = up_crossings(x, 100, 38000)
    expect = 441.0 * (38000 - 100) / 44100.0
    check('files: 22050 -> 44100 Hz keeps the pitch (zero crossings)', code == 0 and abs(n - expect) <= 1.5, '%d vs %.1f' % (n, expect))
    # Loop point: the 0.0998 s file loops many times in 0.9 s with no step bigger than the sine's own slope.
    amp = 0.5 * 32767
    slope = 2 * math.pi * 441.0 / 44100.0 * amp
    worst = max(abs(x[i] - x[i - 1]) for i in range(1, len(x)))
    check('files: the loop point is seamless', worst <= slope * 1.02 + 1, '%d vs %.1f' % (worst, slope))
    check('files: level is the file level at music volume 10', abs(rms(x, 1000, 30000) - amp / math.sqrt(2)) < amp * 0.01)
    fixtures = os.path.join(REPO, 'tests', 'fixtures', 'audio')
    # Ogg Vorbis through the whole pipeline: mono 22.05 kHz 440 Hz and stereo 44.1 kHz 440 / 660 Hz, looping.
    code, out, path = render(tmp, 'f2.wav', ['--audio-file', os.path.join(fixtures, 'tone_mono_22k.ogg'), '--music-volume', '10'], seconds=0.9)
    x = read_wav(path)[2][0]
    n = up_crossings(x, 2000, 38000)
    check('files: Ogg mono 22.05 kHz plays at 440 Hz', code == 0 and abs(n - 440.0 * 36000 / 44100.0) <= 6, str(n))
    code, out, path = render(tmp, 'f3.wav', ['--audio-file', os.path.join(fixtures, 'tone_stereo_44k.ogg'), '--music-volume', '10'], seconds=0.9)
    left, right = read_wav(path)[2]
    check('files: Ogg stereo keeps the channels apart (440 Hz left, 660 Hz right)',
          abs(up_crossings(left, 2000, 38000) - 440.0 * 36000 / 44100.0) <= 6 and
          abs(up_crossings(right, 2000, 38000) - 660.0 * 36000 / 44100.0) <= 8)
    # A damaged music file: a warning and the built-in melody instead; never a crash.
    bad = os.path.join(tmp, 'bad.ogg')
    open(bad, 'wb').write(b'OggS' + bytes(range(256)) * 4)
    code, out, path = render(tmp, 'f4.wav', ['--audio-file', bad], seconds=1)
    check('files: a damaged --audio-file is a warning, the melody plays', code == 0 and 'playing the built-in melody' in out and
          rms(read_wav(path)[2][0], 0, 20000) > 1000, out[-300:])
    code, out, path = render(tmp, 'f5.wav', ['--audio-file', os.path.join(tmp, 'missing.ogg')], seconds=1)
    check('files: a missing --audio-file is a warning too', code == 0 and 'cannot read' in out, out[-300:])


def main():
    global PLAYER, REPO
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    PLAYER, REPO = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
    with tempfile.TemporaryDirectory(prefix='ote-audio-') as tmp:
        for t in (test_flags, test_null_device, test_options, test_render, test_render_files):
            t(tmp)
    print('%d passed, %d failed' % (passed, len(failures)))
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
