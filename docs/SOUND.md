# Sound and music

Contract for PLAN item 3h. Owner decisions (2026-11-14):

- **Music** can be set for the title, per map (with a game default), per
  turn phase (player's turn / enemy's turn), for story scenes and by event
  commands (play music, stop music, play sound), and as victory / defeat /
  level-up jingles.
- **Sound effects**: one standard set per game, with overrides per weapon,
  unit type and item.
- Players get separate **music and sound volumes**.
- **Starter audio**: a generated retro set (default for new projects) and a
  CC0 set (alternative; used by the demo's Detailed 3D style).

Everything below is engine design and may change; wording shown to players
and makers is placeholder until the owner approves it.

Status: build order step 1 (the engine) is done; the rest is design, see "Build order" and "Step 1 as built".

A game without `data/audio.json` is silent and behaves exactly as today.
Audio never changes rules, saves or the save hash: it is presentation, like
graphics.json.

## Files and formats

- `games/<id>/audio/` holds the files. **OGG Vorbis** (`.ogg`) and **WAV**
  (`.wav`, 8/16-bit PCM). Why these two: both decode with small
  public-domain code (stb_vorbis; WAV by hand) on every target including
  old consoles, with no patent or licence strings. No MP3, no tracker
  formats in this version.
- Sound effects are decoded whole at load (short). Music is **streamed**
  (decoded a little at a time) so a long track costs little memory.
- Limits (loader refusals): effect at most 10 s / 2 MB decoded; mono or
  stereo; sample rate 8000..48000.

## data/audio.json

```jsonc
{
  "music": {                       // tracks by id
    "title":  { "file": "audio/title.ogg", "loopStart": 2.5 },   // loopStart seconds, optional (default 0)
    "battle": { "file": "audio/battle.ogg", "volume": 80 },      // percent, optional
    "win":    { "file": "audio/win.ogg", "loop": false }         // a jingle
  },
  "sounds": {                      // effects by id
    "hit":    { "file": "audio/hit.wav" },
    "step":   { "files": ["audio/step1.wav", "audio/step2.wav"] }  // one picked each time
  },
  "use": {                         // the game's standard set: what plays when
    "titleMusic": "title",
    "mapMusic": "battle",          // default for maps
    "playerTurnMusic": "",         // optional; empty/missing = keep the map's music
    "enemyTurnMusic": "",
    "sceneMusic": "",              // campaign scenes (default: keep / silence)
    "victory": "win", "defeat": "lose", "levelUp": "levelup",   // jingles (music ids)
    "sounds": {                    // event -> sound id; every key optional
      "cursor": "tick", "confirm": "ok", "cancel": "back", "error": "buzz",
      "step": "step", "hit": "hit", "miss": "whiff", "critical": "crit",
      "heal": "heal", "defeated": "fall", "capture": "flag", "build": "build",
      "itemUse": "use", "gold": "coin", "dialogue": "blip", "turnStart": "chime",
      "upgrade": "build", "research": "chime", "repair": "heal", "supply": "use"
    }
  }
}
```

Overrides (each optional, by sound id):
- weapons.json: `"sounds": { "hit": "sword_hit", "miss": "sword_miss", "critical": "sword_crit" }`
- units.json: `"sounds": { "step": "hooves", "defeated": "horse_fall", "select": "neigh" }`
- items.json: `"sounds": { "use": "potion" }`
- map file: `"music": "boss"`, optional `"playerTurnMusic"` / `"enemyTurnMusic"`.
- campaign chapter: `"music"` on a scene (`before` / `after` object form).

Lookup order for a battle sound: weapon / item, then unit type, then the
game's standard set, then silence. Unknown ids are loader errors; a missing
file is a warning and silence (a game must still start without its audio).

## Story commands

```jsonc
{ "playMusic": "boss", "fade": 1.0 }     // fade seconds, optional; stays until changed or the battle ends
{ "stopMusic": true, "fade": 1.0 }
{ "playSound": "thunder" }
```
Presentation only: they emit events (`MusicChanged`, `SoundRequested`) the
player acts on; the rules core holds no audio state except "the music the
story asked for", which is saved with the battle so a resumed battle plays
the right track.

## Music rules in the player

Priority, highest first: a jingle while it plays (the track under it
pauses and resumes); music set by a story command; turn-phase music; the
map's music; the game's default. Changing track cross-fades 0.5 s; the
same track asked again keeps playing (no restart). "Enemy's turn" means
any side not controlled by a human on this device.

## Options

Options screen: **Music volume** and **Sound volume** (0..10 each, saved in
the player's settings; default 7). 0 does not decode music at all. Flags
`--music-volume N`, `--sound-volume N`, `--no-audio`.

## Engine design

- `engine/core` knows only ids (audio.json is loaded by the player's
  loader beside graphics.json, with the same strictness).
- `engine/audio/` (portable, no backend headers): decoders, a **software
  mixer** (16 effect voices + 2 music voices for cross-fades, 16-bit
  stereo, 44100 Hz by default, lower on weak targets), and the music
  director implementing the rules above.
- Platform interface gets one small device: `AudioOut { rate, channels,
  queuedFrames(), push(samples, frames) }`. The player mixes on the main
  loop and keeps ~80 ms queued, so **no threads** are needed anywhere
  (project rule) and every port only has to implement "play this buffer":
  SDL3 audio stream, SDL2 queue, WebAudio through SDL on the web (starts
  at the first tap or key press, as browsers demand), console SDKs later.
- Headless runs and `--no-audio` use a null device; tests read what the
  mixer was asked to play (`--audio-log FILE` writes one line per music
  change and effect) instead of listening.

## Step 1 as built

Where it lives: `engine/audio/` (portable: `wav`, `vorbis` + `third_party/stb/stb_vorbis.c`, `decode`, `mixer`,
`synth`), `engine/player/sound/` (the player's `AudioService`, the audio log, the self-test and offline render),
`engine/platform/{sdl3,sdl2}/*audio.cpp` and `platform/common/null_audio.cpp` (the device). The API step 2 uses is in
HANDOFF.md "Sound, step 1". Refinements of the contract above:

- **Limits.** Effects: at most 10 s (checked from the header before anything is allocated for WAV); the 2 MB decoded
  limit is kept but cannot bind first for the allowed formats (10 s of 48 kHz stereo is 1.92 MB). Music has no limit.
  Both refuse with a plain message ("the sound is longer than 10 seconds ...").
- **WAV.** 8-bit unsigned and 16-bit, mono / stereo, 8000..48000 Hz, also WAVE_FORMAT_EXTENSIBLE PCM. A truncated
  file (or a recorder that left the data size at 0 / 0xFFFFFFFF) keeps the whole frames that are really there; every
  other damage is a message, never an over-read (tests cut a file at every byte and mutate headers; the audio tests
  also pass under AddressSanitizer + UBSan; the service tests were not run under them).
- **Formats are told by the first bytes** (RIFF / OggS), not by the file name. Ogg with more than two channels is refused.
- **Volume curve.** Level 0..10 gives gain (level / 10)^2: 10 = 1, 7 = 0.49, 5 = 0.25, 0 = silent. A track's own
  `volume` (percent) multiplies on top.
- **Level 0.** Sound level 0 starts no effects. Music level 0 freezes the music voices without decoding anything, and the
  service opens no music file at all; if the level is raised later the asked-for music starts then (fading in 0.3 s),
  unless it was stopped on purpose.
- **Cross-fade** is equal power (sin / cos), counted in output frames, so two different tracks stay at the same loudness
  through the fade. A change with no fade cuts the old music at once. `stopMusic(fade)` fades out.
- **Jingles.** `MusicParams::pauseCurrent` pauses the playing music (a 20 ms ramp) on its own voice and plays the jingle on the
  other; when the jingle ends by itself the music resumes. Any other `playMusic` drops a held track. A jingle must not loop.
- **Looping.** At the end of a looping source the mixer seeks to `loopStart` and keeps appending frames behind the ones
  still being interpolated, so a loop of whole periods has no click (tested on a synthetic file, also across 22.05 -> 44.1 kHz).
- **Latency.** What is asked at time t is heard about 80 ms later (the player keeps 80 ms queued in the device) plus the device's own
  buffer (SDL is asked for 1024 frames, about 23 ms). A frame that took long refills the queue to 80 ms, never more than 250 ms at once.
- **Three modes**, so a machine without sound behaves like one with: *Off* (`--no-audio`, or the device failed: one warning,
  never an error; nothing is read or decoded), *Silent* (`--headless`: effects are decoded and music files opened so damaged
  files show up in tests, nothing is mixed), *Live*. All three write the audio log.
- **Web.** The device is opened on the first key press or tap (not at start), so nothing touches the browser's audio before
  then; SDL's own code resumes the AudioContext. The browser prints one deprecation warning for SDL's ScriptProcessorNode.
- **Options.** "Music volume" and "Sound volume" are ordinary options (global, per game as "Use global", settings keys
  `musicVolume` / `soundVolume`, numbers 0..10; "Default (7)" is the unset choice). Moving one plays the built-in tick: on
  the music slider it follows the music level, on the sound slider the sound level.
- **Audio log.** `--audio-log FILE`, one line per request and per change, times from the service's clock (exact with `--fixed-dt`,
  which headless runs have by default):
  ```
  t=0.000 volumes music=7 sound=7                    first line, and again whenever a level changes
  t=1.000 sound chime vol=100 level=7                an effect: name (the path as given), requested volume in percent, sound level
  t=0.917 sound tick vol=100 bus=music level=0       the options tick (bus=music|sound, the level of that bus)
  t=0.000 music play melody fade=0 loop=1 level=7    name, fade seconds, loop, music level
  t=3.000 music stop fade=0.5
  ```
- **Self-test.** `ote-player --audio-test` plays a synthesized melody (looping) with two chimes at 1 s and 2 s, fades the music
  out at 3 s and exits at 3.8 s; `--audio-file FILE` plays that .ogg / .wav as the music instead. `--audio-render FILE.wav --seconds N`
  writes the same script through the real service and mixer into a 16-bit stereo 44.1 kHz WAV with no window and no device
  (identical bytes on every run); tests/audio_player_test.py measures those files.

## Starter audio

- `tools/audio/make_starter_audio.py` synthesizes the **retro set**
  (square / triangle / noise voices): every standard effect and five short
  loops (title, battle, enemy turn, victory, defeat) plus a level-up
  jingle. Output is committed; effects as WAV, music as OGG (encoded by
  ffmpeg at generation time only, never needed to build or play).
- The **CC0 set**: chosen from public CC0 libraries, trimmed, credited in
  the game's `CREDITS.md` like the 3D art.
- New projects from a template get the retro set and a filled `use` block.

## Editor

- New **Sound** screen: lists of music and sounds (add from a file, play
  button, rename, delete with "used by"), the standard set as a form of
  pickers each with a play button, volumes and loop start per track.
- Pickers on Weapons, Units, Items (their sounds), map settings (music),
  campaign scenes, and the three story commands.
- The server accepts `.ogg` / `.wav` uploads into `audio/` with the same
  size and name rules as art.

## Not in this version

Voice acting per line; positional / stereo panning by map position;
ambient loops per terrain; music layers; MP3 / tracker formats; per-player
audio in hot-seat.

## Build order

1. **Done (2026-11-14).** Engine: decoders, mixer, `AudioOut` on SDL3 / SDL2 / web, null device,
   audio log, volumes in Options. See "Step 1 as built".
2. Player: audio.json loader, standard set and overrides wired to battle
   events and menus, music director, story commands, campaign scenes.
3. Starter audio: generator + retro set in the demos and templates; CC0
   set for the Detailed 3D style.
4. Editor: Sound screen, pickers, uploads.
