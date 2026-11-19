# Sound and music

Contract for PLAN item 3h. Owner decisions (2026-11-14):

- **Music** can be set for the title, per map (with a game default), per
  turn phase (player's turn / enemy's turn, and per side id), for story scenes and by event
  commands (play music, stop music, play sound), and as victory / defeat /
  level-up jingles.
- **Sound effects**: one standard set per game, with overrides per weapon,
  unit type and item.
- Players get separate **music and sound volumes**.
- **Starter audio**: a generated retro set (default for new projects) and a
  CC0 set (alternative; used by the demo's Detailed 3D style).

Everything below is engine design and may change; wording shown to players
and makers is placeholder until the owner approves it.

Status: all four build order steps are done (the engine, the player, starter audio in the demos and templates, the editor); see
"Build order" and "Step 1 as built" to "Step 4 as built". Nothing has been heard by a person yet.

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
| `data/items.json`, per item (a staff too) | `"sounds"` | `use` (the cue `itemUse`), `heal` (the cue `heal`, see "A staff's own heal sound"), `healScene` (the same cue, in the battle scene only) |
| `maps/*.json` | `"music"`, `"playerTurnMusic"`, `"enemyTurnMusic"` (music ids) | |
| `data/campaign.json`, a chapter's `before` / `after` | the scene as `{ "music": "id", "commands": [ ... ] }` | |

A sound id of `""` in an override means silence on purpose (the lookup stops there; it does not fall through to the standard
set). `""` as a map's `music` means no music on that map; `""` for a turn-phase key means no phase music on that map.
The unit type's blocks apply to the unit doing the thing: the attacker for `hit` / `miss` / `critical`, the walker for `step`, the
unit that fell for `defeated`, the unit picked for `select`, the unit healed for `heal`.
**A staff's own heal sound.** The `heal` cue plays when a "+N" appears over a healed unit, so a staff whose battle scene effect already plays its own sound ("Effect sounds" in
docs/BATTLE_SCENE.md) is heard twice. An item (a staff or a healing item) may carry `"sounds": { "heal": "<sound id>" }`: that sound plays **instead of** the game's `heal` sound for the heal
that item causes, and `"heal": ""` (the same "silence on purpose" as above) plays nothing. The item's entry is looked up first, then the healed unit type's `heal`, then the standard set,
so an item without the key behaves exactly as before. It applies on the map (the `UnitHealed` entries that follow the item's `StaffUsed` / `ItemUsed`) and in the battle scene (the "+N" beat), **unless the item also gives `"healScene"`** (next paragraph);
a heal from anything else (drain, a condition, an ability, a structure) never takes an item's sound. It changes only the heal cue: the staff's use cue (`use`, else `itemUse`) and the miss
cue are decided as before (a staff with a heal effect plays no `itemUse` of its own when the game defines `heal`, even if the item silences it). An unknown sound id is a load error
(`item "mend_staff": "sounds": "heal" names unknown sound "x"`). Test: `tests/staff_heal_sound_player_test.py`.
**Scene only: `"healScene"`.** `"sounds": { "heal": "<id>", "healScene": "<id>" | "" }` on an item: `healScene` is the heal sound **in the battle scene only** (the "+N" beat of a staff used in the scene,
`battleSceneStaves`); it replaces the item's `heal` there, and `""` silences the scene's heal while `heal` keeps playing on the map. Use it for a staff whose scene effect plays its own sound
(`"sounds"` of the effect, docs/BATTLE_SCENE.md "Effect sounds") so the engine's cue is not heard twice in the scene but still is on the map. Without `healScene` the item's `heal` (or, without that, the unit
type's / the standard `heal`) is used in the scene exactly as before, and `heal` keeps its meaning on the map and in the scene; `healScene` alone leaves the map's heal sound to the unit type / the standard
set. It changes only the heal cue of the staff's own heal: the use cue, the miss cue and a heal from anything else (drain, a condition, an ability, a structure) are unaffected. Item blocks accept the keys
`use`, `heal`, `healScene` (an unknown key is refused, listing them); an unknown sound id is `item "x": "sounds": "healScene" names unknown sound "y"`. Code: `CueCtx::scene`, `CueSet::healScene`
(`sound_bank.*`, `audio_data.*`). Tests: `tests/test_audio_data.cpp`, `tests/battle_scene5_player_test.py` (which sounds are heard on the map and in the scene).

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
| `StaffUsed` | the staff's `sounds.use`, else `heal`, else `itemUse` | The staff item's own `use` sound if it has one; else the game's `heal` for a staff with a heal effect when the game defines `heal`; else `itemUse`. `StaffMissed` plays `miss` when the game defines it. (`tests/sound_player_test.py` observes the first three; the miss is not observed.) |
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

Battle scene effects can play sounds too: `"sounds": { "<ms>": "<sound id>" }` on an effect plays a sound of this file that many milliseconds after the effect starts (docs/BATTLE_SCENE.md "Effects"); the id is checked when the game loads, like an animation's frame sounds, and the audio log shows the time.

**Variants** of a sound with `files` play in turn (a counter per sound, never the rules' RNG). Sound loading is cached per path.

**Music.** `MusicDirector` implements the priority above and only ever says "this is what I want now": the same track asked again keeps
playing. Contexts, set by each scene (`Scene::audioContext`): the launcher's home list has no music; a game's page, map list, save list and
end card play `titleMusic` (the options of a game keep what plays); a campaign scene plays `sceneMusic`, or leaves the music alone when
the game has none; a battle plays the map's turn-phase music, else the game's, else the map's `music`, else `mapMusic`. A story command
overrides all of them until it is changed or the scene ends (a new scene forgets it). Victory and defeat cut the battle's music and play the
jingle once, then nothing starts until the scene changes (leaving to the map list starts the title music). Leaving the game for the home
list fades the music out. A battle loaded from a save plays the story's track the save recorded and starts in the right turn phase; a
new battle learns both from its own start events, in order, with their fades.
**The launcher's own sounds** (owner, 2026-11-16: built-in menu sounds, no music). While no game is open (the home list, the
global options) `uiCue()` plays the player's own set: `<assets>/launcher/data/audio.json` + `audio/{cursor,confirm,cancel,error}.wav`,
laid out like a game's and loaded by a second `GameAudio` (`PlayerApp::launcherAudio_`, `GameAudioCues` picks by the app's open game).
The files are generated by `tools/audio/make_starter_audio.py` (the same effects as the templates' standard set). The home list plays
no music; a game that is open but has no sounds stays silent. Without the folder the player runs silent there.

**Story commands.** `{ "playMusic": id, "fade": seconds }` (fade optional: not given = the 0.5 s cross-fade, 0 = a cut, up to 60),
`{ "stopMusic": true, "fade": s }`, `{ "playSound": id }` in map events and campaign scenes (docs/EVENTS.md).

## Step 3 as built

**Who has what.** One command, `python3 tools/audio/make_starter_audio.py .`, writes `audio/` and `data/audio.json` into
`games/demo-economy` and the three editor templates (`blank`, `economy-tactics`, `strategy-rpg`); `tools/audio/targets.py` lists what
each copy gets. Every copy is self-contained (a project made from a template is a plain copy of the folder, so templates cannot share
files), so each ships only the files its `audio.json` names. `blank` gets the set too (a new project must not be silent) but only the
standard cues, no extra overrides; it has one unit and one sword.

| Copy | Effects (WAV) | Music (OGG) | Overrides written into its data files |
|---|---|---|---|
| `games/demo-economy` | 24 files, 337 KB | title, battle, enemy, victory, defeat, levelup (623 KB) | tank / artillery / APC `step` = `tank_move`; Tank Gun / Howitzer `hit` = `cannon_shot`; standard `ambush` (fog maps) |
| `editor/templates/blank` | 21 files, 284 KB | the same six | none |
| `editor/templates/economy-tactics` | 23 files, 319 KB | the same six | tank / artillery `step` = `tank_move`; both cannons `hit` = `cannon_shot` |
| `editor/templates/strategy-rpg` | 24 files, 315 KB | the same six | Iron Sword `sword_hit`, Iron Bow `bow_shot`, Cavalier `step` = `hooves` |
| `games/demo-srpg` (CC0 set) | none (all OGG, 38 effects, 32 kHz mono) | title, battle, enemy, scene, boss, victory, defeat, levelup (see below) | below |

No game here has a `scene` track in the retro copies (none has a campaign; `sceneMusic` is `""`); a game with a campaign gets it by
adding `scene=True` to its profile in `targets.py`. There is no magic or potion in the templates, so `magic_cast` / `potion` are
not shipped there; `levelup_tick` is not used by anything and is not shipped.

**The files.** Effects are trimmed of silent and inaudible (below -48 dB) tails and fades, which with only the files each copy
uses took the effects from 460 KB (all 30 in each copy) to 284..337 KB; they stay 16-bit 22050 Hz mono WAV (8-bit or a lower rate
were not needed to be under 400 KB). The tread step and the hoof step are one square long (the walk plays one per square, a 0.75 s rumble
would pile up). Music is OGG q3, 22050 Hz mono. The OGG bytes depend on the ffmpeg version, so the tool stores the SHA-256 of each
track's source WAV in `tools/audio/starter_audio.manifest.json` and only re-encodes (once, shared by every copy) when it changes or a file
is missing: a rerun on any machine changes nothing (`tests/starter_audio_test.py` checks it on a copy). Repository growth: the retro copies
are 4 x ~0.95 MB, the CC0 set 2.1 MB.

**The CC0 set** (`games/demo-srpg/audio`, 2.1 MB, mono 32 kHz, music and jingles scaled to the retro set's levels: RMS 0.13 music, 0.16
effects and jingles, peaks under 0.97). Sources, all CC0 1.0 as stated on each source page (the Kenney zips also carry a License.txt):
Kenney's RPG Audio, Interface Sounds, Impact Sounds and Music Jingles (kenney.nl); OpenGameArt entries by RandomMind (Medieval: The Bard's
Tale, Battle, The Old Tower Inn, Rejoicing, Defeat Theme), Bobjt (Orcs Victorious) and Spring Spring (War Theme). The exact file behind each
id is in `tools/audio/make_cc0_set.py` (`--credits` prints it) and in `games/demo-srpg/CREDITS.md`. Where a pack has nothing that fits, the
closest was used: the bow is a string pluck plus a soft thud, the potion and magic are glass. Every music slot found a CC0 track, so no slot of the
CC0 set is filled from the retro generator. The loops of the long tracks (battle, enemy, boss) were found by comparing
the sound of the two ends and cross-fading 2 s (`LOOPS` in the script); **that was checked by numbers, never by ear**.
`demo-srpg` overrides: sword / lance / axe / bow `hit` sounds, `step` for the cavalier (`hooves`), the knight (`plate_step`) and the
sky rider (`flutter`), `potion` for the salve and mending salve, `magic` for the energy drop, `scene` under all campaign scenes
(the `sceneMusic` default), and in the bridge map's events: `playSound alarm` when the reinforcements come, `playMusic boss` (1 s fade)
when Gorm is fought, `stopMusic` (1 s) when the battle is won. The demo's campaign file is unchanged (the editor's campaign screen
does not yet read the object form of a scene).

**Rules hash and saves.** The `sounds` keys and `audio.json` do not change the rules hash; the three story commands in the bridge map do, so
the hash of `demo-srpg` is different from before and existing `demo-srpg` saves of the bridge map are refused ("made with other
rules"), which is acceptable for a demo. The sims did not move: `bridge` Blue 17 / Red 13, `north_road` Blue 19 / Red 11, `--campaign
--runs 20` finished 5; `demo-economy` is unchanged.

**Tests.** `tests/sound_player_test.py` section 16 plays both demos and a project made from each template through `--audio-log`:
title music on the game page, battle / enemy music and back, the tank's `tank_move`, scene music then battle music in the campaign,
every weapon's sound, hooves, the story commands, clean loads (no warning), every file named by every shipped `audio.json` exists and
none is unused, and the size budgets. `tests/starter_audio_test.py` checks the shipped copies and the no-op rerun.

## Step 4 as built

Where it lives: server `editor/server/audio*.cpp` (files, header facts, the player's loader run by `/problems`), docs/EDITOR_API.md
"Sound data and audio files"; web `editor/web/src/screens/sound/` (the Sound screen, `SoundsGroup` for weapons / units / items,
`MapMusic`, `AudioChoice`), `src/model/audio*.ts` and `campaignScene.ts`, `src/audio/preview.ts` (the play buttons: one shared
`<audio>` element). Tests: `tests/editor_api_sound_checks.py`, `editor/web/tests/unit/audio.test.ts`, `tests/e2e/sound*.spec.ts`.
Decisions: "remove all sound" is `"audio": false` (the file goes, `audio/` stays); the editor writes a scene as the object form only
while it has music, a sound with one file as `file`, several as `files`, defaults (volume 100, loop on, loopStart 0) as no key; a track or
sound is named by its id; deleting or renaming one rewrites every place that names it in the same undo step (`model/audioRefs.ts`, run
by the document store after every edit); a sound FILE is not part of Undo. A missing file is a hint in the editor and not a problem in
`/problems`. The Play panel has no sound toggle: the embed page would need `if (q.get('noaudio')) a.push('--no-audio');` in
`engine/platform/web/embed.html`.

## Music by situation, steps by movement type, sounds on animation frames

All optional and all presentation: a game that uses none of these keys sounds, saves and hashes exactly as before (the keys are cosmetic for the data
hash: `phaseMusic`, `fewEnemies`, `prepMusic` on a map and `stepInterval` inside a `sounds` block are not counted). Code: `engine/player/sound/music_director.*`
(the choice), `engine/player/battle/battle_music.*` (what a turn start tells it), `engine/core/battle/story_music.cpp` (setPhaseMusic).
Tests: `tests/test_music_situations.cpp`, `tests/test_step_and_anim_sounds.cpp`, `tests/music_player_test.py` (every one through `--audio-log`).

**What already existed and what was added.** Title / game page music (`titleMusic`), the map's music with a game default, a player-turn and an enemy-turn
music (game and map), scene music, the `victory`, `defeat` and `levelUp` jingles and the story `playMusic` / `stopMusic` were there. The enemy's turn was one
thing for every computer side. Added: music **per side id**, **few enemies**, `setPhaseMusic`, **preparation** music, the **promotion** and **chapter title card** jingles.
There is no separate "game over" key: the `defeat` jingle is the game-over sound and the end card keeps the title music.

```jsonc
// data/audio.json "use"
"phaseMusic": { "red": "enemy_theme", "green": "ally_theme" },   // a side id -> track ("" = none for that side); beats playerTurnMusic / enemyTurnMusic for that side
"fewEnemies": { "music": "last_stand", "count": 2 },              // while the hostile units left are at most `count` at the start of a human side's phase
"prepMusic": "camp",                                               // the preparation screen (docs/EVENTS.md "Preparation"); none = the battle's own goes on
"promotion": "fanfare", "chapterCard": "chapter_jingle"            // jingles: they must have "loop": false; they pause the music under them like the level-up jingle
// maps/<id>.json (every key optional; the map's wins over the game's)
"phaseMusic": { "green": "boss_green" },                           // side ids of THIS map only ("names unknown side" otherwise)
"fewEnemies": { "music": "mapfew", "count": 1 },                   // "music": "" turns it off on this map
"prepMusic": "camp"                                                // "" = no preparation music on this map
```

Order for a battle (highest first): a jingle while it plays; the story's `playMusic` / `stopMusic`; the preparation music while the preparation screen is open;
what `setPhaseMusic` set for the side to move; the **few-enemies** track; the map's `phaseMusic` for that side, else the game's; the map's `playerTurnMusic` /
`enemyTurnMusic`, else the game's; the map's `music`; the game's `mapMusic`. `""` for a side (or in a `setPhaseMusic`) means "no phase music" and the lookup
falls to the map's music.

- **When it switches.** The music is chosen at the **start of a phase** (the `TurnStarted` entry) and **never in the middle of one**: the few-enemies count is
  taken once, when a human side's phase starts (units still alive on the board that belong to a side of another team); killing the last-but-two enemy changes the
  music at the next player phase. The enemy's phases never use it. A resumed battle counts from the saved state. The one exception is `setPhaseMusic`.
- **`{ "setPhaseMusic": { "side": "red", "music": "calm" } }`** (map events): from now on that side's phase plays `calm`; if it is that side's phase right now
  the music changes at once, otherwise when its phase starts. `"music": ""` = no phase music for that side, `"music": null` = take the change away (back to what the
  data says). It beats the few-enemies track and the data, and loses to `playMusic`. It is **saved with the battle** (`story.phaseMusic`, docs/SAVES.md) and a
  resumed battle plays it. The side must be a side of the map; the track must be one of `audio.json` (checked like `playMusic`). An event that fires at the start of a
  turn changes the music right after that turn's banner (the map's music is heard for the length of the banner).
- **Events.** The core emits a `MusicChanged` entry with the flag `kMusicPhase` (a = text index of the track, -1 = cleared, c = side index).

**Step sounds by movement type.** `game.json` `moveTypes`: `{ "id": "mounted", "sounds": { "step": "hooves", "stepInterval": 0.25 } }`. Lookup for a step: the
unit type's own `step`, then its movement type's, then the standard set's, then silence (`""` is silence at any level). `stepInterval` (0.02 to 2 seconds, default 0.1) is
the least time between two steps heard; a unit type may set its own in its `sounds` block (`"stepInterval": 0.2`), which wins. A shorter interval than the walk's pace
gives a step on every square. Refusals: `game.json: move type "mounted": "sounds": "step" names unknown sound "x"`, `unknown key "hit" (expected "step", "stepInterval")`,
`"stepInterval" is seconds between two steps, from 0.02 to 2`.

**Sounds on the frames of a battle scene animation** (docs/BATTLE_SCENE.md "Sounds"): per-weapon / per-type `hit`, `miss`, `critical` sounds at the moment of impact
already existed (the lookup above); what was missing is a sound that belongs to the *animation*, so a set can have a swing sound on frame 1 and a clang on frame 3.

## Shop music, boot jingle, level-up ticks, save sound, promotion sounds (fidelity 3)

All optional and presentation only (nothing here is saved or hashed): a game that uses none of these keys sounds exactly as before. Code:
`engine/player/sound/` (`audio_data.cpp` the keys, `music_director.cpp` the music, `cues.*` the two new cues), `engine/player/battle/battle_shop_music.cpp`,
`battle_view_ticks.cpp`. Tests: `tests/test_fidelity3_sound.cpp`, `tests/srpg_player_fidelity3_test.py` (through `--audio-log`).

```jsonc
// data/audio.json "use"
"shopMusic": "market",                  // music id: any shop screen whose shop names no music of its own (preparation shops and the shop a unit opens on the map)
"bootJingle": "boot",                   // music id, "loop": false: played once when a game's page opens, before titleMusic
"promotionSounds": [ { "atMs": 0, "sound": "whoosh" }, { "atMs": 400, "sound": "boom" } ],   // sounds at times after a promotion starts, next to the promotion jingle
"sounds": { "levelUpTick": "tick",      // once for each stat line that rises on the level-up box
            "save": "chime" }           // a save was written from a menu (the slot list; not the continue file)
```

- **`shopMusic`.** While a shop screen is open its music plays: a preparation shop's own `music` (docs/EVENTS.md "Preparation": `shop.music` / `shops[].music`),
  else `shopMusic`, else nothing changes. It sits above the preparation music and the map's music and below a story `playMusic`; closing the shop brings back what
  was playing (a cross-fade each way, the same track asked again keeps playing). Applies to the preparation shop's three screens (buy, who gets it, sell) and to
  the shop a unit opens on the map (which has no music of its own, so it plays `shopMusic`).
- **`bootJingle`.** Played alone (no title music under it) the first time a game's page opens after the game is opened from the launcher; `titleMusic` starts when
  it ends (and at once when there is no jingle, no file, or the music level is 0). The map list or a battle coming back to the page does not play it again;
  leaving the game for the home list and opening it again does. It must have `"loop": false` like the other jingles. In a headless run nothing is mixed, so the
  jingle counts as over at once: the log shows `music play ...boot...` and, one frame later, `music play ...title...`.
- **`levelUpTick`.** Requested once for each stat that rose in a level-up (a stat held at its cap does not tick), the first as the box opens and the others
  0.12 s apart so that each is heard (the same sound twice in one frame would be one). The box staying up for Confirm does not repeat them; confirming before
  they are all out drops the rest. Not played for the promotion's "Stat changes" box. It is an ordinary cue of `use.sounds` (22 -> 24 keys; no weapon, unit or
  item override).
- **`save`.** A cue played when a manual save to a slot succeeded (battle menu and preparation "Save"). A failed save plays nothing new; nor do the
  continue file and autosaves.
- **`promotionSounds`.** 1 to 16 entries `{ "atMs": 0..60000, "sound": id }`, kept in time order. When a promotion begins (the entry the viewer sees), each sound is
  requested `atMs` after it, whatever the box on screen is doing; the promotion jingle (`use.promotion`) is requested as before. Refused: an empty list, more
  than 16, `atMs` outside 0..60000 or not whole, an unknown sound id, any other key.
- **Editor.** Sound, "Events": the music slots gain "Shop screens" and "When the game's page opens" (the boot jingle), the sound slots gain "Level-up: each stat"
  and "A save was written", and a "Promotion sounds" list (a time in milliseconds and a sound per row) sits under the promotion jingle; renaming or deleting a
  track or sound follows everywhere, as for the other slots.

## Starter audio

- `tools/audio/make_starter_audio.py` synthesizes the **retro set**
  (square / triangle / noise voices): every standard effect and four loops (title,
  battle, enemy turn, story scene) plus the victory, defeat and level-up jingles, and
  the extra effects the overrides use. Output is committed; effects as WAV, music as OGG
  (encoded by ffmpeg at generation time only, never needed to build or play).
  `python3 tools/audio/make_starter_audio.py .` writes it into every place that ships
  it, see "Step 3 as built".
- The **CC0 set**: chosen from public CC0 libraries, cut and level-matched by
  `tools/audio/make_cc0_set.py`, credited in the game's `CREDITS.md` like the 3D art.
  `games/demo-srpg` (the demo with the "Detailed 3D" style) uses it.
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
3. **Done (2026-11-14).** Starter audio: the generator, the retro set generated into `demo-economy`
   and the three templates, the CC0 set in `demo-srpg`. See "Step 3 as built".
4. **Done (2026-11-14).** Editor: Sound screen, pickers, uploads. See "Step 4 as built".
