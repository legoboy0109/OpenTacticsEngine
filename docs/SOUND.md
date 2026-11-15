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

Status: build order steps 1 (the engine) and 2 (the player: data, events, music) are done; the rest is design, see "Build order",
"Step 1 as built" and "Step 2 as built".

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

Options screen: **Music volume** and **Sound volume** as sliders (Left lowers, Right raises, they stop at Off
and 10; 0..10 each, saved in
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

## Step 2 as built

Where it lives: `engine/player/sound/` (`audio_data` the loader, `cues` the event names, `sound_bank` the lookup,
`music_director`, `game_audio` the facade the scenes use, `cue_sink.h` the one-line `uiCue()` hook), `engine/player/battle/battle_view_sound.cpp`
(the sounds of the battle's events) and `battle_sound.cpp`; the core only gained the story commands and passes the sound keys
through (`engine/core/...`, docs/EVENTS.md, docs/SAVES.md). Tests: `ote_tests` `audio_data_*`, `sound_*`, `music_director_*`,
`story_music_*`; `tests/sound_player_test.py` (48 checks through `--audio-log`, ctest `sound_player`).

**The exact `data/audio.json`** is the contract's shape. Unknown keys anywhere are errors (`data/audio.json: sound "hit": unknown key
"flie" (expected "file", "files", "volume")`), so are unknown ids (`use.mapMusic: names unknown music "x"`), a path that is not a
`.ogg` / `.wav` inside the game folder, a `loop: true` jingle (`victory`, `defeat`, `levelUp` must have `"loop": false`), a
volume outside 0..100, a `files` list that is empty or longer than 16. Ids (tracks, sounds) are letters, digits, `_`, `-`, at most 64 (the
same as unit and event ids). Added over the contract: an optional `"volume"` (percent) on a sound, as on a track.
`use.sounds` accepts these keys (22): `cursor confirm cancel error step hit miss critical heal defeated capture build itemUse gold
dialogue turnStart upgrade research repair supply ambush select`. `ambush` (a move stopped by an unseen enemy under fog) and
`select` (a unit picked on the map) are the two optional extras; a game without `select` hears its `confirm`.

**Overrides** (each optional; an unknown key, a non-string value or an unknown sound id is an error naming the file, the entry and the key):

| Where | Key | Accepted event keys |
|---|---|---|
| `data/weapons.json`, per weapon | `"sounds"` | `hit`, `miss`, `critical` |
| `data/units.json`, per unit type | `"sounds"` | `step`, `hit`, `miss`, `critical`, `defeated`, `select`, `heal` |
| `data/items.json`, per item | `"sounds"` | `use` (the cue `itemUse`) |
| `maps/*.json` | `"music"`, `"playerTurnMusic"`, `"enemyTurnMusic"` (music ids) | |
| `data/campaign.json`, a chapter's `before` / `after` | the scene as `{ "music": "id", "commands": [ ... ] }` | |

A sound id of `""` in an override means silence on purpose (the lookup stops there; it does not fall through to the standard
set). `""` as a map's `music` means no music on that map; `""` for a turn-phase key means no phase music on that map.
The unit type's blocks apply to the unit doing the thing: the attacker for `hit` / `miss` / `critical`, the walker for `step`, the
unit that fell for `defeated`, the unit picked for `select`, the unit healed for `heal`.
A scene object with `"music"` is the plain scene with `{ "playMusic": "id" }` in front, nothing else changes (`"commands"`
may be left out); scenes in `then` / `else` branches stay plain lists.

**Where ids are checked.** `audio.json` is loaded and checked in `GameAudio::load`: for the launcher's game page without a
Database (title music and the standard set only), and again with the Database when the game loads (`PlayerApp::loadSession`),
where the overrides and every story id are checked too: the maps whose file mentions `playMusic` / `playSound` are loaded and walked
(also inside `if` branches), and every chapter scene. A mistake is a load error ("Cannot load game ..."), a missing audio FILE is a warning
(`audio: sound 'x': the file ... does not exist; that sound stays silent`) and silence, a damaged file or one over the limits is
a load error. A game with a story command but no `audio.json` is refused (it would never be heard). `--no-audio` (mode Off) reads
nothing and checks nothing; a headless run (Silent) loads everything, so tests find mistakes.

**Events to sounds.** One sound per cue, picked weapon / item, then unit type, then the standard set, then silence:

| Event (BattleEvent) | Cue | Notes |
|---|---|---|
| `Attack`, at the moment it lands | `hit`, `miss`, `critical` | weapon, then the attacker's unit type; a critical with no `critical` sound plays `hit` |
| `UnitMoved` | `step` | on entering each new square of the walk, at most one per 0.1 s |
| `UnitDied` | `defeated` | |
| `UnitHealed` | `heal` | |
| `StructureCaptured` | `capture` | |
| `UnitBuilt` | `build` | |
| `ItemUsed` | `itemUse` | the item's override first |
| `ResourceChanged` (gain, of the viewer's side) | `gold` | the same entries that float "+N" over the panel |
| `TurnStarted` | `turnStart` | also moves the turn-phase music |
| `StructureUpgraded` | `upgrade` | |
| `ResearchStarted` (not instant), `ResearchFinished` | `research` | |
| `UnitRepaired`, `UnitResupplied` | `repair`, `supply` | |
| `MoveBlocked` | `ambush` | |
| `LevelUp` | the `levelUp` jingle | pauses the music |
| `BattleEnded` | the `victory` / `defeat` jingle | when a human side won / all humans lost; none for a draw or `--autoplay` |
| `MusicChanged`, `SoundRequested` | the story's commands | |
| text typing in the dialogue box | `dialogue` | per letter, at most one per 0.045 s; none when the line is skipped |
| menus and lists | `cursor`, `confirm`, `cancel`, `error` | every list, menu, panel, confirm box and screen of the launcher, options, saves and battle |
| a unit picked on the map | `select` | |

Fog: the view only starts entries the viewer saw (`Event::seen`), so unseen enemy actions are silent (tested against `--show-all`).
Skipped or fast-forwarded animations finish entries without their mid-animation sounds, and `GameAudio` plays the same sound once
per frame at most, so a skipped turn never stacks sounds.

**Variants** of a sound with `files` play in turn (a counter per sound, never the rules' RNG). Sound loading is cached per path.

**Music.** `MusicDirector` implements the priority above and only ever says "this is what I want now": the same track asked again keeps
playing. Contexts, set by each scene (`Scene::audioContext`): the launcher's home list is silent; a game's page, map list, save list and
end card play `titleMusic` (the options of a game keep what plays); a campaign scene plays `sceneMusic`, or leaves the music alone when
the game has none; a battle plays the map's turn-phase music, else the game's, else the map's `music`, else `mapMusic`. A story command
overrides all of them until it is changed or the scene ends (a new scene forgets it). Victory and defeat cut the battle's music and play the
jingle once, then nothing starts until the scene changes (leaving to the map list starts the title music). Leaving the game for the home
list fades the music out. A battle loaded from a save plays the story's track the save recorded and starts in the right turn phase; a
new battle learns both from its own start events, in order, with their fades.
The home list and the global options use no sounds (no game is open); whether the launcher should have a sound set of its own is a
design decision for the owner.

**Story commands.** `{ "playMusic": id, "fade": seconds }` (fade optional: not given = the 0.5 s cross-fade, 0 = a cut, up to 60),
`{ "stopMusic": true, "fade": s }`, `{ "playSound": id }` in map events and campaign scenes (docs/EVENTS.md).

## Starter audio

- `tools/audio/make_starter_audio.py` synthesizes the **retro set**
  (square / triangle / noise voices): every standard effect and five short
  loops (title, battle, enemy turn, victory, defeat) plus a level-up
  jingle. Output is committed; effects as WAV, music as OGG (encoded by
  ffmpeg at generation time only, never needed to build or play).
  **Generator built** (`tools/audio/`, checked by `tests/starter_audio_test.py`):
  30 effects, four loops (title, battle, enemy, scene) and three jingles
  (victory, defeat, levelup), plus a ready `audio.json.example`. Not yet
  generated into the games.
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
2. **Done (2026-11-14).** Player: audio.json loader, standard set and overrides wired to battle
   events and menus, music director, story commands, campaign scenes. See "Step 2 as built".
3. Starter audio: generator (built, `tools/audio/`); retro set generated
   into the demos and templates; CC0 set for the Detailed 3D style.
4. Editor: Sound screen, pickers, uploads.
