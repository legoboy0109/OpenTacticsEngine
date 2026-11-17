#!/usr/bin/env python3
"""Synthesizes the tiny tracks of tests/music_player_test.py (docs/SOUND.md "Music by situation").

  python3 tools/audio/make_music_test_audio.py OUT_DIR NAME [NAME ...]

Writes OUT_DIR/NAME.wav for each name: one second of a plain sine tone (8000 Hz, mono, 16 bit) whose pitch depends on the name,
so the files differ but are made from nothing. The tests never play them (the player runs --headless with --audio-log, which
opens no sound device); they only have to exist and decode. Run it again to get the same files byte for byte.
"""
import math
import os
import struct
import sys
import wave

sys.dont_write_bytecode = True


def tone(path, hz, seconds=1.0, rate=8000):
    n = int(rate * seconds)
    frames = b''.join(struct.pack('<h', int(9000 * math.sin(2 * math.pi * hz * i / rate) * min(1.0, i / 200.0, (n - i) / 200.0))) for i in range(n))
    with wave.open(path, 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(frames)


def main(out, names):
    os.makedirs(out, exist_ok=True)
    for i, name in enumerate(names):
        tone(os.path.join(out, name + '.wav'), 220 + 37 * i)


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2:])
