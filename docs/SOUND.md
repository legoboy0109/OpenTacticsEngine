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

Status: design only; see "Build order".

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

1. Engine: decoders, mixer, `AudioOut` on SDL3 / SDL2 / web, null device,
   audio log, volumes in Options.
2. Player: audio.json loader, standard set and overrides wired to battle
   events and menus, music director, story commands, campaign scenes.
3. Starter audio: generator + retro set in the demos and templates; CC0
   set for the Detailed 3D style.
4. Editor: Sound screen, pickers, uploads.
