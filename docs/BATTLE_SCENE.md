# Battle scene (side-view fights with fighter animations)

A fight can be shown two ways. **On the map** (the default, exactly what the player always did): strikes lunge on the map,
numbers float over the squares. **Battle scene**: a full-screen side view with both fighters, names, levels, weapon names,
hit-point bars that drain on each hit, damage / MISS / CRIT numbers, skill labels, and the fighters drawn from per-class animation
sets. Presentation only: the rules core's event stream is the single source of truth; saves, hashes and `ote-sim` results do not change.

## Settings

- `game.json` `"battleScene": "map" | "scene"` (default `"map"`): the maker's default. Existing games stay on the map.
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
`defeated X`, `end` or `end (skipped)` (skipped beats are still logged, in order).

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
- **Backgrounds**: picture per terrain of the struck fighter, a map's own (`"battleBackground": "id"` in the map file) wins; then the
  default; then a generated sky and ground in the terrain's colour. A background is scaled up by a whole number to cover the screen.
- Side colours come from per-side sheets (above); there is no tint.
- Refusals (plain English, `data/graphics.json: battle set "knight": needs a "stand" animation ...`): unknown sheet / animation /
  set / unit type / character / weapon / weapon group / terrain / background, a frame outside the sheet, more than 512 frames, `ms` count
  not matching frames, `hit` / `spawn` beyond the last frame, an anchor outside the frame, a side's sheet with another layout or too small
  for the frames, a rule with nobody named, a rule naming a weapon and a group, a duplicate rule, a sheet image above 2048 x 2048.

## Fallback

A fighter with no set (or without an animation) shows the unit's **map picture** enlarged by a whole number (nearest) with simple
motion: lunge on attack, yellow strobe on a crit, shake with red tint on a hit, sidestep on a dodge, fade and sink on defeat. A unit
with no picture at all is the map's coloured block with its letter. A game works before any battle art exists.

## Layout

Same code at every size; positions come from the screen size and the font's line height. Top: a box per fighter (side colour,
name, level, weapon, hit-point bar with numbers). Middle: the fighters standing on a ground line (landscape 80% down the stage, portrait
66%), each as large as fits (whole-number scale, up to 8x), melee close together, ranged apart with a dotted line and "Range N".
Bottom: "Confirm / tap: skip". Floating numbers and skill labels rise above the fighter; a crit flashes the screen; the scene fades in
and out. Checked at 480 x 270 and 270 x 480.

## Preparing art (artist or converter)

Draw each animation as a strip of equal frames in a row of one PNG (any number of rows per sheet), declare the sheet with the frame
size as `cellWidth` / `cellHeight`, list the frames and times, mark `hit`, set `anchor` to where the feet stand. Draw facing right.
`tools/art/battle_art.py` is a complete original example (a knight in blue and in red, an arrow, a meadow, a walking figure for
docs/GRAPHICS.md "Walking") and the data for it (`BATTLE_SET`). To make a side's colours, redraw the same sheet in other colours and name
it in `sides`. **Converters**: a long animation is one strip per sheet (up to 512 frames); the extra slots are optional, so a source that
has no ranged stand or whiff animation simply leaves them out.

## Code

`engine/core/data/battle_art.*`, `loader_battle_art.cpp`; player: `fight_script.*` (events to strikes, pure), `fight_timeline.*`
(clock, pure), `fight_scene*.cpp` (drawing), `battle_view_fight.cpp` (the view holds the run and plays it silently afterwards),
`battle_fight.cpp`. Tests: `test_battle_art.cpp`, `test_fight_script.cpp`, `test_fight_timeline.cpp`, `test_battle_scene_settings.cpp`,
`tests/battle_scene_player_test.py`, `tests/test_battle_followup.cpp` (side sheets, the assign order, limits, extra slots), editor
`battleArt.test.ts`, `battle-anims.spec.ts`.
