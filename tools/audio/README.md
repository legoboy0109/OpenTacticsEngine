# Starter audio generator

Synthesizes the engine's original retro sound set (docs/SOUND.md "Starter audio").
Pure Python standard library, no input files; two runs give byte-identical WAVs.

    python3 tools/audio/make_starter_audio.py .                       # regenerate the shipped games (like tools/art)
    python3 tools/audio/make_starter_audio.py --out DIR [--ogg]       # the whole set into one folder

With the repository root it writes `audio/` and `data/audio.json` into `games/demo-economy` and the three
editor templates (`targets.py` says what each copy gets: the standard set, plus the extra sounds that game's
weapon and unit overrides use, plus the scene track for a game with a campaign; none of these has one).
Effects are 16-bit WAV (22050 Hz mono, silent tails trimmed), music is OGG Vorbis (ffmpeg, quality 3). The OGG
bytes depend on the ffmpeg / libvorbis version, so a music file is only re-encoded when the SHA-256 of its
source WAV differs from the one in `starter_audio.manifest.json` (or the file is missing; then it is copied
from a sibling copy when one has it, so every copy of a track stays byte-identical). A rerun on any machine is
a no-op, which `tests/starter_audio_test.py` checks on a copy of the repo's folders. Change a recipe, run the
tool, commit the files and the manifest. The sound overrides in the games' `weapons.json` / `units.json` are
written by hand; `data/audio.json` is regenerated, so edit `targets.py` rather than the file.

`--out DIR` writes `DIR/audio/*.wav` (every effect and track) and `DIR/audio.json.example`, a complete
`data/audio.json` for a game that uses the whole set (`--ogg` re-encodes the music; no ffmpeg: it says so and
keeps WAV). Check it with `python3 tests/starter_audio_test.py . [--summary]`.

`make_cc0_set.py` builds the demo-srpg set from downloaded CC0 originals (Kenney packs, OpenGameArt): see its header.
The originals are not in the repository; `--credits` lists the exact files.

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
