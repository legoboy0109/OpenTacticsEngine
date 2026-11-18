# Battle scene (side-view fights with fighter animations)

A fight can be shown two ways. **On the map** (the default, exactly what the player always did): strikes lunge on the map,
numbers float over the squares. **Battle scene**: a full-screen side view with both fighters, names, levels, weapon names,
hit-point bars that drain on each hit, damage / MISS / CRIT numbers, skill labels, and the fighters drawn from per-class animation
sets. Presentation only: the rules core's event stream is the single source of truth; saves, hashes and `ote-sim` results do not change.

## Settings

- `game.json` `"battleScene": "map" | "scene"` (default `"map"`): the maker's default. Existing games stay on the map.
- `game.json` `"battleBackgroundFrom": "defender" | "attacker"` (default `"defender"`): whose square decides the background and the platform ("Which background and platform").
- `game.json` `"battleSceneSides": "map" | "playerRight" | "playerLeft"` (default `"map"`): which side each fighter stands on ("Which side each fighter stands on").
- Player option **Battle animations**: *Game default* / *Scene* / *On the map* (options screen, global or per game, saved in the
  settings file as `battleScene`, effective at once). Command line `--battle-scene scene|map` beats it for one run.
- Order: command line > per-game option > global option > the game's default.

## What plays in the scene, and what stays on the map

In the scene: every fight between two units (a player's attack, the computer's attack, counters, follow-up strikes, skills that
fire, drain, recoil, conditions put on in the fight, a weapon that breaks, the fall). **Staves, abilities, attacks on doors, and
everything else stay on the map in this version.** Battle quotes (before) and defeat quotes (after) are dialogue on the map and
keep their place in the stream: quotes, scene, defeat quote. Experience, level-up box and rank-up come after the scene as before
(the scene shows the fighters' level and maximum hit points as they were when the fight began). Confirm / Cancel / tap skips the
rest of the fight to its result. A fight out of the viewer's sight (fog) is not shown.

Log lines (`[info] battle scene: ...`) for tests: `start A (Lv n) vs B (Lv n), melee|ranged, N strike(s)`, `X art: set "id"|map
picture`, `X sheet: "id" (side S)` (only when the fighter's side has a sheet of its own for the set), `skill NAME (unit)`, `strike A -> B [(counter)] [(crit)]`, `hit|crit A -> B DMG`, `miss A -> B`, `drain X +n`, `recoil X -n`,
`defeated X`, `end` or `end (skipped)` (skipped beats are still logged, in order). Also, only in a game that uses the newer settings: `left A, right B` (who stands where, with `battleSceneSides`), `background "id" (terrain t)` (when the game has platforms, a map table or `battleBackgroundFrom: attacker`) and `platform "id"` (when one is used); a game with none of them logs exactly what it always did.

## Which side each fighter stands on

By default (`"battleSceneSides": "map"`, today's rule) the unit standing nearer the left edge of the map stands on the left; in the same column the one
that started the fight does. **`"playerRight"`**: a fighter controlled by a person always stands on the **right** and the computer's fighter on the left
(the classic handheld layout); **`"playerLeft"`** is the opposite. When **both or neither** fighter is a person's (a hot-seat game, or two computers),
the map rule decides. "A person's" = the side's `controller` is not `ai`. Everything that depends on the side index follows, because the scene only
knows left and right: the mirroring (`faces`, `mirror`), the per-side sheets, which way an effect travels, the hit-point boxes and names, the floating
numbers. Presentation only: no save, hash or `ote-sim` result changes. Code: `initiatorStandsLeft` in `fight_script.*` (pure, unit-tested),
`FightInfo::sidesMode`. Log: the line `battle scene: left A, right B` after `start`, only in a game that sets `battleSceneSides` (in the default the `start` line names the left fighter first, as it always did).

## Which background and platform

The background comes from the terrain under **one** fighter's square: the fighter that is struck (`"battleBackgroundFrom": "defender"`, today's rule and
the default) or the one that attacks (`"attacker"`); the platform from the same square. The picture is chosen, first match wins:

1. the **map's table** entry for that terrain: the map file's `"battleBackgrounds": { "swamp": "bog", "default": "meadow" }`;
2. the map's single `"battleBackground": "id"` (as before: one picture for the whole map);
3. the map table's `"default"`;
4. the game-wide table: `terrainBackground` of `data/graphics.json` `"battle"` for that terrain;
5. the game's `defaultBackground`; then the generated sky and ground in the terrain's colour, as before.

So a map that writes a table wins over the game's whole table (its `"default"` too, which beats the game's entry for a terrain the map did not name); a map with
no table behaves exactly as before. The table's keys are terrain ids and `"default"`; its values are background ids of `"battle"` > `"backgrounds"`. Refusals
(in the map file): `"battleBackgrounds" must be { "<terrain id>": "<background id>", "default": "<id>" }` (not an object, or empty), `there is no terrain with this id`
(use a terrain id, or "default"), `there is no background "x"`. `"battleBackgroundFrom"` other than the two is refused in `game.json`.
Log: `battle scene: background "bog" (terrain swamp)`. Pure function: `pickBattleBackground` (`core/data/battle_pick.*`).

## Platforms

The engine had no platforms (the fighters stood on a ground line with a shadow), so a small one was built: a **platform** is a picture drawn **under
each fighter**, centred on the fighter's feet, enlarged by the same whole number as the fighter, behind it (the shadow is drawn over it). One picture per
terrain, an optional `far` picture used instead when the fight is at range 2 or more.

```jsonc
"battle": {
  "platforms": { "grass": { "sheet": "plat", "cell": [0, 0], "far": [1, 0] }, "boards": { "sheet": "plat", "cell": [2, 0] } },
  "terrainPlatform": { "plain": "grass", "swamp": "boards" },     // the game-wide table
  "defaultPlatform": "grass"
}
```

A map may override per terrain with `"battlePlatforms": { "swamp": "boards", "default": "grass" }` in the map file (same rules and order as the background,
without a "single picture" step). Which square decides: `battleBackgroundFrom` (the same square as the background; both fighters stand on the same kind of
platform). A platform's image is a normal sheet; `cell` / `far` are `[column, row]` of it, the piece size is the sheet's cell size. A platform may also give `right` / `farRight`
cells (the right-hand fighter's own pictures, used with a fixed stage, "Fixed stage"). Refusals:
`unknown sheet`, a cell outside the sheet, an id called `default`, unknown keys, `there is no platform "x"` (in the tables), `there is no terrain with this id`.
Not built (on purpose, small): a platform per fighter's own terrain, platform animation. (Without a fixed stage the right-hand platform is not flipped.) Log: `battle scene: platform "grass" (far)`.
Pure function: `pickBattlePlatform`. Editor: Art > Platforms; Terrain > "Weather and platform"; map settings.

## Data: `data/graphics.json` key `"battle"`

```jsonc
"battle": {
  "sets": {
    "knight": {
      "anchor": [24, 46],          // foot point in a frame, pixels from its top left (default: bottom middle)
      "sides": { "red": { "sheet": "battle_knight_red" } },   // optional: the red side's fighter draws this set from another sheet (see "By side")
      "faces": "right",            // which way the art looks: "right" (default) or "left"
      "mirror": true,              // default true: the right-hand fighter is the art turned round; false = never turned
      "animations": {              // "stand" is required
        "stand":  { "sheet": "battle_knight", "frames": [[0,0],[1,0]], "ms": 400 },   // others: attack crit ranged cast dodge hurt defeat + critRanged dodgeRanged standRanged miss
        "attack": { "sheet": "battle_knight", "frames": [[0,1],[1,1],[2,1],[3,1]], "ms": [140,90,110,160],
                    "hit": 2, "spawn": 1 },
        "hurt":   { "sheet": "battle_knight", "frames": [[0,2],[1,2]], "ms": 160 }
      } } },
  "effects": [ { "group": "bow", "sheet": "fx", "frames": [[0,0],[1,0]], "ms": 80, "mode": "travel", "travelMs": 220 } ],
  "assign": [ { "set": "knight", "type": "knight" },
              { "set": "lance_knight", "type": "knight", "group": "lance" },
              { "set": "silver_knight", "type": "knight", "weapon": "silver_lance" },
              { "set": "enemy_knight", "type": "knight", "side": "red" },
              { "set": "hero", "character": "eirik" } ],
  "backgrounds": { "meadow": { "sheet": "battle_meadow", "cell": [0,0] } },
  "terrainBackground": { "plain": "meadow" },
  "defaultBackground": "meadow"
}
```

- Frames are cells `[column, row]` of a normal sheet; **the sheet's cell size is the frame size**. `ms`: one number for all frames or one
  per frame (default 120, 10 to 10000). `loop` (default true for `stand` and `standRanged`, false otherwise).
- **Limits.** An animation lists 1 to **512** frames (the same cell may repeat), all from **one sheet**; an animation cannot span several
  sheets (a 2048 x 2048 sheet of 16 x 16 cells holds 16384 cells, so a long strip fits; split a longer one into two animations or sheets).
  A sheet image is at most **2048 x 2048** pixels (docs/GRAPHICS.md): a bigger image is refused when the game loads, with a message that
  names the size and the limit (`image "art/x.png" is 4096 x 64 pixels; the largest allowed is 2048 x 2048`). The timeline keeps no
  per-frame state beyond the `ms` list, so 512 frames cost nothing extra.
- **Beats**: `hit` = the frame whose start is the moment the blow lands (damage number, hit flash, sound, hit-point drain; default
  the last frame). `spawn` = the frame a projectile / effect appears on (default: the hit frame).
- **Which animation plays** (every slot after `stand` is optional; the extras are cheap fall-backs):
  - the attacker's strike: `miss` when the blow does not land (falls back to `attack`); else `critRanged` for a critical hit in a ranged fight
    (falls back to `crit`); else `crit` for a critical hit; else `ranged` when the fighters are more than one square apart; else `cast`
    when the weapon has an effect; else `attack`. A missing one falls back to the next (crit, ranged, cast become `attack`); with no
    `attack` either, the stand picture's simple motion plays.
  - the struck fighter: `hurt` on a hit, `dodge` on a miss (`dodgeRanged` in a ranged fight, falls back to `dodge`), `defeat` on a fall.
  - standing: `standRanged` while the fight is ranged (falls back to `stand`), else `stand`.
- **Effects**: per weapon (`"weapon": id`) or weapon group (`"group": id`); `mode` `"target"` (appears on the foe) or `"travel"`
  (flies from the striker; the hit lands when it arrives, `travelMs`). Drawn mirrored when the striker is on the right. Optional `anchor`.
  Everything newer (sounds, a lead-in before the hit, layers, flash and shake, a charge on the striker, lists, by unit type, a returning
  shot) is in "Effects" below; an effect with none of those keys plays exactly as before.
- **Which set a fighter uses** (`assign`): every rule that fits the fighter is a candidate and the most specific wins. A rule may name
  `type` (unit type), `character`, `group` (the group of the weapon the fighter struck with), `weapon` (that very weapon's id; a rule
  names a weapon **or** a group, not both) and `side` (the fighter's map side id). From most to least specific:
  1. character + weapon, 2. character + group, 3. character alone (a rule with a character and a type ranks just above one with
  the character alone), 4. type + weapon, 5. type + group, 6. weapon alone, 7. group alone, 8. type alone, 9. side alone; none fits: the
  fallback (the map picture). `side` never lifts a rule above a more specific one: it only breaks a tie between rules of the same level
  (type + side beats type alone, which beats nothing; a type + group rule still beats type + side). The first rule listed wins among
  exact equals; naming the same combination twice is refused. Each rule needs at least one of `type`, `character`, `group`, `weapon`, `side`.
- **By side** (`sides`). Fighters of different sides can draw the same set from different sheets (an enemy in other colours). A set may
  have `"sides": { "<side id>": { "sheet": "<sheet id>" } }`: the fighter of that map side draws **every** animation of the set from
  that sheet; an animation may have its own `sides` of the same shape, which wins over the set's for that animation (use it when a set's
  animations come from different sheets). The side's sheet must have the **same layout** as the sheet it replaces: the same cell size and
  every frame of the animation inside it (the loader refuses otherwise: `... must have the same layout`). A side with no entry (and a side id
  the map does not have) draws from the animation's own sheet. Resolved from the fighter's own side when the scene starts; the log names
  the sheet only when a side's sheet is used (`X sheet: "battle_knight_red" (side red)`).
- **Sounds** (`"sounds": { "<frame>": "<sound id>" }` on an animation): a sound of `data/audio.json` that plays when that frame **begins**, for any animation
  of a set (`attack`, `crit`, `hurt`, `defeat`...), e.g. `"attack": { ..., "hit": 2, "sounds": { "1": "swing", "3": "clang" } }`. Frames count from 0; a
  frame the animation does not have, or an id that is not a sound of the game, is refused (`"sounds" names frame "9" but the animation has frames 0 to 3`,
  `battle set "knight" animation "attack": sound "x" is not in data/audio.json "sounds"`). They come on top of the impact sounds by weapon / unit type
  (`hit`, `miss`, `critical`), which still play at the hit frame. A fighter on the fallback motion (no animation) has none; a skipped scene is silent. The log
  says `battle scene: sound swing (Eirik)`. Time order is kept: a sound is a beat of the timeline at the frame's start.
- **Backgrounds**: picture per terrain of the struck fighter (or the attacker's, "Which background and platform"), a map's own table or
  `"battleBackground": "id"` in the map file wins; then the default; then a generated sky and ground in the terrain's colour. A background is scaled up by a whole number to cover the screen.
- Side colours come from per-side sheets (above); there is no tint.
- Refusals (plain English, `data/graphics.json: battle set "knight": needs a "stand" animation ...`): unknown sheet / animation /
  set / unit type / character / weapon / weapon group / terrain / background, a frame outside the sheet, more than 512 frames, `ms` count
  not matching frames, `hit` / `spawn` beyond the last frame, an anchor outside the frame, a side's sheet with another layout or too small
  for the frames, a rule with nobody named, a rule naming a weapon and a group, a duplicate rule, a sheet image above 2048 x 2048.

## Effects

An effect is a picture sequence drawn over a fight for the weapon (or weapon group) the striker uses. The first keys are in "Data" above
(`weapon` / `group`, `sheet`, `frames`, `ms`, `mode`, `travelMs`, `anchor`). Everything below is optional; **data without the newer keys loads and
plays exactly as it did, and logs no new lines.** Presentation only: no save, hash or `ote-sim` result changes.

```jsonc
"effects": [
  { "id": "charge", "sheet": "fx", "frames": [[0,0],[1,0,"fx2"],[2,0]], "ms": 80, "at": "striker", "hitAtMs": 300,
    "sounds": { "0": "hum", "300": "zap" }, "flash": [{ "atMs": 300, "ms": 80, "color": "#ffffff" }], "shake": [{ "atMs": 300, "ms": 200, "strength": 2 }] },
  { "id": "wash", "sheet": "sky", "frames": [[0,0]], "layer": "screen" },                      // a library effect: an id and pictures, no weapon
  { "id": "bolt", "sheet": "fx", "frames": [[3,0]], "mode": "travel", "travelMs": 300, "returns": true, "lingerMs": 120 },
  { "weapon": "fire_tome", "effects": ["charge", "bolt"], "byType": { "knight": "wash", "mage": [] } }   // an assignment: no pictures of its own
]
```

**Effect sounds.** `"sounds": { "<ms>": "<sound id>" }`: a sound of `data/audio.json` that plays that many milliseconds after the effect **starts**
(for a `target` / `striker` effect with `hitAtMs`, the start is the lead-in, not the hit). Ids are checked when the game loads in the player
(`data/graphics.json: battle effect 3: sound "x" is not in data/audio.json "sounds"`); a key that is not a whole number of milliseconds (0 to 60000) or
an id with other characters than letters, digits, `_` and `-` is refused by the loader. Log: `battle scene: sound X (effect ID)`, where ID is the
effect's `id`, else its weapon id, else its group. The existing animation sound lines (`sound X (Eirik)`) are unchanged. A skipped scene is silent
(the log lines are still written).

**Hit time (a lead-in).** `"hitAtMs": 233` (0 to 15000, default 0) on a `target` or `striker` effect: the effect starts that long **before the hit
lands**, so its peak meets the damage. 0 is today: the effect starts at the hit. The strike's hit moment (the striker animation's `hit` frame) is the reference;
the effect is started early. If the lead-in is longer than the time since the strike began (the striker's animation reaches its hit frame sooner
than `hitAtMs`), the hit **waits**: the effect starts when the strike starts and the hit lands `hitAtMs` later. A strike that misses plays its effects
the same way (the `miss` beat takes the place of the hit). A `travel` effect refuses `hitAtMs` (it hits when it arrives).

**Layers.** `"layer"`: `"front"` (default: over the fighters, as always), `"behind"` (behind **both** fighters, in front of the backdrop, the range line
and the platforms), `"screen"` (frames that cover the whole stage; drawn right after the backdrop, so the range line, platforms, fighters and
`front` effects are over it). A screen effect with a fixed stage is drawn at the stage's origin at the stage's scale `k` (a frame the stage's size covers
it exactly); without a fixed stage it is enlarged by the whole number that covers the screen and centred, like a backdrop. It is never turned round unless
`"mirror": true` (then it is, when the striker stands on the right). A screen effect cannot `travel` and has no `at`; `mirror` is refused on other layers.
Draw order: backdrop, screen effects, range line, platforms, behind effects, fighters (the striker in front), front effects, floating numbers, boxes, flashes, fade.

**Frames from several sheets.** A sheet image is at most 2048 x 2048 (docs/GRAPHICS.md), so a long effect can come from several. In an effect (not in a
fighter's animation) a `frames` entry may be `[column, row, "sheet id"]`: that frame is cell (column, row) of that sheet; an entry with two numbers is from the
effect's own `"sheet"` (which is still required). All the sheets of one effect must have **one cell size** (refused otherwise, naming both). The timeline
and the 512-frame limit are unchanged.

**Flash and shake.** `"flash": [{ "atMs": 100, "ms": 80, "color": "#ffffff" }]`: at `atMs` after the effect starts the whole scene is covered by `color` (default
`#ffffff`), starting at an alpha of about 70% and fading out over `ms` (the critical-hit flash is the same drawing, white, 110 / 255 over 120 ms).
`"shake": [{ "atMs": 100, "ms": 200, "strength": 2 }]`: the stage (backdrop, platforms, fighters, effects, floating numbers; **not** the boxes and the hint) moves back and
forth, dying away over `ms`, like a fighter's hit shake; `strength` (1 to 64) is in stage pixels with a fixed stage (multiplied by the scale `k`) and screen
pixels without one. Both lists have up to 32 rows; `atMs` 0 to 60000, `ms` 10 to 5000; `atMs`, `ms` (and `strength` for a shake) are required in a row.
Several at once add up. A skipped scene shows none.

**`at`: an effect on the striker.** `"at": "target"` (default) or `"striker"` for a non-travel effect: it is put on the striker's foot point (the middle of the
fighter, or with a fixed stage the foot point lifted by the effect's `anchor`), turned round like the striker's art. A charge-up on the caster. `hitAtMs` works
the same as for a target effect.

**Several effects per weapon.** An entry with a `weapon` or `group` may carry `"effects": "<effect id>"` or a list of up to 8 ids: effects named by `"id"` somewhere
in the list (an entry with its own pictures: a library effect with no weapon, or another weapon's entry). The entry's own pictures (if it has any) come first, then the
list in order. An entry without `sheet` / `frames` but with `effects` and / or `byType` only assigns. Each listed effect starts **by its own rules**:

- a `target` or `striker` effect starts `hitAtMs` before the hit (0: at the hit);
- a `travel` effect leaves at the striker animation's `spawn` frame (else its hit frame) and arrives `travelMs` later, as always;
- the hit lands at the **latest** of: the striker animation's hit frame; every travelling effect's arrival; and the start of the strike plus the longest `hitAtMs` (so a charge
  of `hitAtMs: 300` on a 200 ms strike makes the hit land at 300 ms, and a bolt that arrives at 350 ms makes it 350, with the charge then starting at 50 ms).
- the list order is the order of the start of the effects that start at the same moment and of the layers inside a layer (later is drawn over earlier). A weapon that has any effect
  makes the striker play `cast` when the set has it, as before.

An entry's `"effects"` lists name only effects with pictures of their own; the listed entries' own `effects` / `byType` are not followed (no chains). Refusals: an
unknown id (`there is no effect with the id "x"`), an id of an entry without pictures, more than 8 ids, a second entry with one `id`, an `effects` / `byType` on an entry with no weapon
or group. The one-entry-per-weapon rule is unchanged (`a second effect for "longbow"`).

**By unit type.** `"byType": { "<unit type id>": "<effect id>" | ["<effect id>", ...] }` on an entry with a weapon or group: when the **striker's** unit type is listed, those
effects play **instead of** the entry's own pictures and its `effects` list (`[]` = no effect for that type; the striker then also plays no `cast`). Other types play the entry
as written. The weapon's own entry wins over its group's entry as a whole (the by-type table of the group's entry is not consulted for a weapon that has its own).
Unknown unit types and effect ids are refused.

**Projectile extras (`travel` only).** `"returns": true`: after arriving, the projectile flies back to the striker in the same `travelMs`, the frames continuing, turned
round (it flies the other way); the hit still lands on arrival. `"lingerMs": N` (0 to 5000): it stays at the target that long after arriving (before flying back, if both).
The effect is drawn for `travelMs + lingerMs (+ travelMs)` and the strike does not end before the projectile is home (the next strike starts after it). `returns` and
`lingerMs` are refused on an effect that does not travel.

**Timing, as built.** Pure function `buildTimelineList` (`fight_timeline.*`): per strike, `t0` = the strike's start; the hit time is as above; effect `i`'s
`FightFx` has `t0` (a travel effect: the spawn time; another: hit - `hitAtMs`), `dur`, the side it is drawn on; its sounds are beats at `t0 + ms`, its flashes and shakes pulses at
`t0 + atMs`. An effect that uses any newer key also logs `battle scene: effect ID start` at its start (before the hit when it leads in); an effect without them logs nothing new.
Tests: `test_battle_effects.cpp` (the loader, refusals for every key, which effects a strike gets, the audio check, the timeline for lead-ins, striker effects, lists, misses, returns, lingers, sounds, flash,
shake), `tests/battle_effects_player_test.py` (the real player, headless: log order, `--audio-log` times, screenshots of the layers, a striker and a target effect, a returning shot, a
flash and a shake).

## Fallback

A fighter with no set (or without an animation) shows the unit's **map picture** enlarged by a whole number (nearest) with simple
motion: lunge on attack, yellow strobe on a crit, shake with red tint on a hit, sidestep on a dodge, fade and sink on defeat. A unit
with no picture at all is the map's coloured block with its letter. A game works before any battle art exists.

## Layout

Same code at every size; positions come from the screen size and the font's line height. Top: a box per fighter (side colour,
name, level, weapon (the one it struck with, else the one it holds: a fighter felled before it could counter still names it), hit-point bar with numbers). Middle: the fighters standing on a ground line (landscape 80% down the stage, portrait
66%), each as large as fits (whole-number scale, up to 8x), melee close together, ranged apart with a dotted line and "Range N".
Bottom: "Confirm / tap: skip". Floating numbers and skill labels rise above the fighter; a crit flashes the screen; the scene fades in
and out. Checked at 480 x 270 and 270 x 480. Art made for a fixed small stage uses the optional **fixed stage** instead ("Fixed stage").

## Fixed stage

Art made for one small fixed picture (a classic handheld tactics game: a 240 x 160 stage with the fighters at fixed places, a backdrop exactly the
stage's size, platforms at fixed places) does not fit the generic layout, which enlarges each fighter on its own and puts them around the middle of the
screen. An optional **`"stage"`** in `data/graphics.json` `"battle"` lays everything out in the stage's own pixels instead. Without it the scene is
laid out exactly as described under "Layout" (nothing changes). Presentation only: no save, hash or `ote-sim` result changes.

```jsonc
"stage": {
  "width": 240, "height": 160,                                // the stage's own size, 16 to 1024; everything below is in stage pixels
  "feet":    { "left": [88, 104], "right": [152, 104] },       // where each fighter's anchor (foot point) stands in a close fight
  "feetFar": { "left": [56, 104], "right": [184, 104] },       // optional: at range 2 or more (default: feet)
  "platform":    { "left": [56, 96], "right": [120, 96] },     // optional: top left of each fighter's platform picture, close (default: centred on the foot point)
  "platformFar": { "left": [24, 96], "right": [152, 96] },     // optional, needs "platform": at range 2+ (default: platform)
  "align": "bottom",   // "top" | "center" (default) | "bottom": the part that stays visible when the stage is taller than the screen allows
  "fit": "cover",      // "cover" (default) | "contain"
  "rangeLine": true    // default true: the dotted line and "Range N" label of a ranged fight
}
```

Points are `[x, y]` whole numbers counted from the stage's top left and must lie inside the stage (`0 <= x < width`, `0 <= y < height`). `left` / `right`
are the two sides of the scene (after `battleSceneSides` and `faces` / `mirror`, so "right" is where the right-hand fighter stands). Refusals name
`battle stage`: size outside 16 to 1024, a point outside the stage or not a pair of whole numbers, `platformFar` without `platform`, an unknown key, an `align`
or `fit` that is not one of the words, a `left` or `right` missing.

**What is drawn where.** The stage is one picture space with **one whole-number scale `k`**:

- The **backdrop** is drawn at the stage's origin at scale `k` (never scaled on its own). A backdrop of the stage's size covers the stage exactly. A backdrop
  of another size is centred across the stage and placed up or down by `align` (top edge on the stage's top, middle, or bottom edge on the stage's bottom);
  what falls outside the stage is not drawn. With no backdrop picture the generated sky and ground fill the whole screen.
- Each **fighter** is placed so that its set's `anchor` (the point of the frame that sits on the foot point; frame pixel `(ax, ay)` has its top left corner on the
  foot point, and for the right-hand fighter the picture is mirrored around it) stands on the foot point, drawn at `k`. A fighter with no set (the map-picture
  fallback) stands on the foot point too and is enlarged by a further whole number to about the height of an animated fighter (the other side's
  `anchor` height, else 30% of the stage's height), as in the generic layout. Per-fighter enlargement is otherwise off.
- A **platform** picture is drawn at its declared top-left corner at scale `k`, behind the fighters. The right-hand platform is the left picture mirrored,
  unless the platform entry gives its own cells: **`"right"`** (the close picture) and **`"farRight"`** (the far picture, needs `"far"`) are optional
  `[column, row]` cells of the platform's sheet, for art whose two halves differ. Without `platform` points the picture is centred on the foot point.
- **Shadows**, **floating numbers and labels** and the **range line** are placed from the foot points at `k`. **Effects**: a `"travel"` effect flies on the line
  between the two foot points; a `"target"` effect is put on the struck fighter's foot point; in both the effect's `anchor` is the point of its frame that sits on
  that path or point (so give an effect an `anchor` where it should touch: the default is the middle of the frame). The lunge, dodge and hit-shake offsets of
  the fallback motion, the crit flash and the fades keep working (offsets are in stage pixels times `k`).

**Scale and crop, as built.** Screen `W x H`, the boxes end at row `hudBottom` and the hint line starts at row `hintTop` (today's boxes and hint, unchanged);
the **area** is rows `hudBottom` to `hintTop`.

1. `k` = the largest whole number with `width * k <= W` (at least 1; a stage wider than the screen is centred and cut at both sides).
2. `"contain"`: while `k > 1` and `height * k` is taller than the area, `k` is reduced. (At `k = 1` a stage taller than the area is cropped like `cover`.)
3. Across: centred. Up and down: a stage that fits the area (`height * k <= area`) is centred in the area, so nothing is hidden and nothing is under the boxes
   (bars of plain dark colour above and below). A taller stage **runs under the top boxes and the hint line**: it is placed against the whole screen height by `align`
   (`top`: its top at row 0; `center`; `bottom`: its bottom at the screen's bottom; the rows that fall off the screen are cropped) and then moved the least
   that keeps **every foot point and platform picture inside the area** (below the boxes, above the hint line; the bottom wins a conflict). So fighters'
   feet and platforms are always in view and never hidden by the HUD; the boxes are drawn over the stage, as translucent panels.
4. The 480 x 270 screen with a 240 x 160 stage (boxes end at about row 46, hint at about row 257): `cover` gives `k = 2` (320 rows), cropped to 270 rows: with `align: "bottom"`
   the lowest 135 stage rows show (the top 25 are cut: stage row 25 is at screen row 0, the stage's feet at row 104 are at screen row 158); `top` shows rows 0 to 134; `center`
   about rows 12 to 147. `contain` gives `k = 1`, the whole stage centred (120 pixels bar at each side). The 270 x 480 portrait screen: `k = 1`, the whole stage fits
   between the boxes and the hint and is centred there (15 pixels at each side). 960 x 540: `k = 4` under `cover`, `3` under `contain`.

The geometry is the pure function `stageGeometry` (`core/data/battle_stage.*`); the loader is `loader_battle_stage.cpp`; the drawing `fight_scene_stage.cpp`.
Editor: Art > **Stage** card (docs/EDITOR_UI.md). Tests: `test_battle_stage.cpp`, `test_battle_art.cpp`, `tests/battle_stage_player_test.py`.

## Preparing art (artist or converter)

Draw each animation as a strip of equal frames in a row of one PNG (any number of rows per sheet), declare the sheet with the frame
size as `cellWidth` / `cellHeight`, list the frames and times, mark `hit`, set `anchor` to where the feet stand. Draw facing right.
`tools/art/battle_art.py` is a complete original example (a knight in blue and in red, an arrow, a meadow, a walking figure for
docs/GRAPHICS.md "Walking") and the data for it (`BATTLE_SET`). To make a side's colours, redraw the same sheet in other colours and name
it in `sides`. **Converters**: a long animation is one strip per sheet (up to 512 frames); the extra slots are optional, so a source that
has no ranged stand or whiff animation simply leaves them out.

## Code

`engine/core/data/battle_art.*`, `loader_battle_art.cpp`; player: `fight_script.*` (events to strikes, pure), `fight_timeline.*`
(clock, pure), `fight_scene*.cpp` (drawing; `fight_scene_stage.cpp` the fixed stage), `core/data/battle_stage.*` (its geometry, pure), `loader_battle_stage.cpp`, `battle_view_fight.cpp` (the view holds the run and plays it silently afterwards),
`battle_fight.cpp`. Effects: `BattleEffect` (`battle_art.h`), `readEffects` (`loader_battle_art.cpp`), `BattleArt::resolveEffects`, `buildTimelineList`, `FightScene::drawEffects` / `applyShake` / `drawFlashes`. Tests: `test_battle_art.cpp`, `test_fight_script.cpp`, `test_fight_timeline.cpp`, `test_battle_scene_settings.cpp`,
`tests/battle_scene_player_test.py`, `test_battle_stage.cpp`, `tests/battle_stage_player_test.py`, `test_battle_effects.cpp`, `tests/battle_effects_player_test.py`, `tests/test_battle_followup.cpp` (side sheets, the assign order, limits, extra slots), editor
`battleArt.test.ts`, `battleStage.test.ts`, `battleEffects.test.ts`, `battle-anims.spec.ts`, `battle-stage.spec.ts`, `battle-effects.spec.ts`.
