# Starter audio generator

Synthesizes the engine's original retro sound set (docs/SOUND.md "Starter audio").
Pure Python standard library, no input files; two runs give byte-identical WAVs.

    python3 tools/audio/make_starter_audio.py --out DIR [--ogg]

Writes `DIR/audio/*.wav` (30 effects, 7 music tracks) and `DIR/audio.json.example`,
a complete `data/audio.json` for a game that uses the set. `--ogg` re-encodes the
music to OGG Vorbis (ffmpeg, quality 3; WAVs are removed and the example points at
the `.ogg` files). No ffmpeg: it says so and keeps WAV. OGG bytes depend on the
ffmpeg/libvorbis version; the WAVs never do. Check it with
`python3 tests/starter_audio_test.py . [--summary]`.

Files: `synth.py` (oscillators, ADSR, slides, vibrato, LFSR noise, mastering),
`sfx.py` (effect recipes), `music.py` (step sequencer), `tracks.py` (the songs as
data), `wavio.py`, `make_starter_audio.py`.

## Add an effect

Write a function in `sfx.py` returning a float buffer (0.05..1.2 s) built from
`tone(kind, dur, f0, f1, ...)` (kind: square/triangle/saw/sine/noise; `f1` slides the
pitch), `burst`, `mix`, `arpeggio`. Add it to `EFFECTS`. Loudness and the fade in/out
are applied for you. Register its id in `make_starter_audio.py` (`STANDARD_KEYS` or
`EXTRA_SOUNDS`) so it lands in `audio.json.example`.

## Add a track

Add a spec to `tracks.py` and to `TRACKS` (`True` = loops). A spec is
`dict(bpm=..., sections=[...], lead={voice overrides}, tail=seconds)`; sections play
in order (4/4, 16 steps per bar). A section:

    dict(chords='Am F C G',        # one chord per bar: C, Am, F#m, Bb, G7 ...
         arp='fast', bass='pump', drums='march',   # pattern names from music.py
         lead="A4 - - - C5 - - - E5 - - - D5 - C5 - / ...")  # 16 tokens per bar, '/' between bars

Tokens: a note name starts a note (`C#5`, `Bb3`), `-` holds it, `.` rests. Arp tokens
are chord-tone indexes 0 root, 1 third, 2 fifth, 3 octave; bass tokens `r` root, `f`
fifth, `o` octave. Drum characters: `k` kick, `s` snare, `h` hat, `o` open hat,
`K`/`S` kick/snare plus hat. A wrong token count in a bar is an error.

Loops are whole bars; note tails that run past the end are folded back onto the
start, so the loop point has no click (`loopStart` is 0, there are no intros).
Jingles (`loops=False`) keep their tail and fade out over 20 ms.
