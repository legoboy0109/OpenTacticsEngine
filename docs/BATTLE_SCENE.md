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
picture`, `skill NAME (unit)`, `strike A -> B [(counter)] [(crit)]`, `hit|crit A -> B DMG`, `miss A -> B`, `drain X +n`, `recoil X -n`,
`defeated X`, `end` or `end (skipped)` (skipped beats are still logged, in order).

## Data: `data/graphics.json` key `"battle"`

```jsonc
"battle": {
  "sets": {
    "knight": {
      "anchor": [24, 46],          // foot point in a frame, pixels from its top left (default: bottom middle)
      "faces": "right",            // which way the art looks: "right" (default) or "left"
      "mirror": true,              // default true: the right-hand fighter is the art turned round; false = never turned
      "animations": {              // "stand" is required; others: attack crit ranged cast dodge hurt defeat
        "stand":  { "sheet": "battle_knight", "frames": [[0,0],[1,0]], "ms": 400 },
        "attack": { "sheet": "battle_knight", "frames": [[0,1],[1,1],[2,1],[3,1]], "ms": [140,90,110,160],
                    "hit": 2, "spawn": 1 },
        "hurt":   { "sheet": "battle_knight", "frames": [[0,2],[1,2]], "ms": 160 }
      } } },
  "effects": [ { "group": "bow", "sheet": "fx", "frames": [[0,0],[1,0]], "ms": 80, "mode": "travel", "travelMs": 220 } ],
  "assign": [ { "set": "knight", "type": "knight" },
              { "set": "lance_knight", "type": "knight", "group": "lance" },
              { "set": "hero", "character": "eirik" } ],
  "backgrounds": { "meadow": { "sheet": "battle_meadow", "cell": [0,0] } },
  "terrainBackground": { "plain": "meadow" },
  "defaultBackground": "meadow"
}
```

- Frames are cells `[column, row]` of a normal sheet; **the sheet's cell size is the frame size**. `ms`: one number for all frames or one
  per frame (default 120, 10 to 10000). `loop` (default true for `stand`, false otherwise).
- **Beats**: `hit` = the frame whose start is the moment the blow lands (damage number, hit flash, sound, hit-point drain; default
  the last frame). `spawn` = the frame a projectile / effect appears on (default: the hit frame).
- Which animation a strike plays: `crit` for a critical hit, else `ranged` when the fighters are more than one square apart, else
  `cast` when the weapon has an effect, else `attack`; a missing one falls back to the next (crit, ranged, cast become `attack`).
  Reactions: `hurt` on a hit, `dodge` on a miss, `defeat` on a fall.
- **Effects**: per weapon (`"weapon": id`) or weapon group (`"group": id`); `mode` `"target"` (appears on the foe) or `"travel"`
  (flies from the striker; the hit lands when it arrives, `travelMs`). Drawn mirrored when the striker is on the right. Optional `anchor`.
- **Which set a fighter uses** (`assign`): the most specific matching rule wins: character with weapon group > character >
  unit type with weapon group > weapon group alone > unit type alone. None: the fallback. Each rule needs one of `type`,
  `character`, `group` (the group of the weapon the fighter struck with).
- **Backgrounds**: picture per terrain of the struck fighter, a map's own (`"battleBackground": "id"` in the map file) wins; then the
  default; then a generated sky and ground in the terrain's colour. A background is scaled up by a whole number to cover the screen.
- Side colour tint: not available (the sprite pipeline has none).
- Refusals (plain English, `data/graphics.json: battle set "knight": needs a "stand" animation ...`): unknown sheet / animation /
  set / unit type / character / weapon group / terrain / background, a frame outside the sheet, `ms` count not matching frames,
  `hit` / `spawn` beyond the last frame, an anchor outside the frame, a rule with nobody named, a duplicate rule.

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
`tools/art/battle_art.py` is a complete original example (a knight, an arrow, a meadow) and the data for it (`BATTLE_SET`).

## Code

`engine/core/data/battle_art.*`, `loader_battle_art.cpp`; player: `fight_script.*` (events to strikes, pure), `fight_timeline.*`
(clock, pure), `fight_scene*.cpp` (drawing), `battle_view_fight.cpp` (the view holds the run and plays it silently afterwards),
`battle_fight.cpp`. Tests: `test_battle_art.cpp`, `test_fight_script.cpp`, `test_fight_timeline.cpp`, `test_battle_scene_settings.cpp`,
`tests/battle_scene_player_test.py`, editor `battleArt.test.ts`, `battle-anims.spec.ts`.
