# Audio test fixtures

Two tiny Ogg Vorbis files for tests/test_audio_ogg.cpp and tests/audio_player_test.py. Both are a
pure sine tone of 0.5 s at 12000/32768 amplitude, made from synthesized WAVs with ffmpeg
(libvorbis, quality 1):

| file | format | content |
| --- | --- | --- |
| `tone_mono_22k.ogg` | mono, 22050 Hz, 11025 frames | 440 Hz |
| `tone_stereo_44k.ogg` | stereo, 44100 Hz, 22050 frames | 440 Hz left, 660 Hz right |

How they were made (the WAVs came from Python's `wave` module: `int(12000 * sin(2 pi f i / rate))`
per sample, 16-bit):

```
ffmpeg -i tone_mono_22k.wav   -c:a libvorbis -q:a 1 tone_mono_22k.ogg
ffmpeg -i tone_stereo_44k.wav -c:a libvorbis -q:a 1 tone_stereo_44k.ogg
```

Vorbis is lossy: tests check lengths, rates, channels, the tone's pitch (zero crossings) and
roughly its level, never exact samples.
