# Plan

Living roadmap. Update the status column as work lands; keep finished
detail short. Architecture and rules: docs/ARCHITECTURE.md. Data format:
docs/GAME_FORMAT.md.

## Decisions made (by the project owner)

- Own engine, not built on Godot or any other engine. C++17 + SDL3
  (platform plumbing only) + Lua 5.4.
- Targets: PC, web, phones, consoles, retro handhelds; later homebrew ports
  to old consoles (Xbox, 360, PS3, Wii U).
- Genres: economy tactics up to "Advance Wars + light 4X" (several
  resources, tech tree, upgradable cities; not full Civilization) and
  strategy RPGs (Fire Emblem, FFT).
- Grids: square top-down first; hex, isometric, 3D later.
- Audience: mixed. Editor for common things, Lua for unusual ones.
- Open source under GPL-3.0 (owner, 2026-11-14), on GitHub. Known limits of
  that choice: not compatible with Apple's App Store or official (non-homebrew)
  console releases.
- OVERCLOCK (~/Projects/UntitledTBT) is separate: ideas only, never edit it.

## Milestone 1: both genres playable from data, desktop + web

| # | Work package | Status |
|---|---|---|
| 1 | Scaffold, contracts, format spec | done |
| 2 | JSON + game/map loader + two demo games | done |
| 3 | Battle core: movement, combat, turns, capture, production, progression, Lua formulas | done (36 tests) |
| 4 | AI opponent (`core/ai`) + `ote-sim` headless runner | done |
| 5 | SDL3 platform backend + player (menus, battle UI, automation flags) | done |
| 6 | Web build (Emscripten): standalone page + embeddable player, Chromium and Firefox tested | done |
| 6a | Real fonts (stb_truetype, shared by all backends), `--font` option, SDL2 backend | done |
| 6b | Launcher front end inside the player: pick/load a game, edit options (font, size, display...) before starting; settings persist across restarts | done |
| 7 | README, how-to-run, cleanup | todo |

Done means: `ote_tests` green, `ote-sim` finishes both demo maps, the
player plays both demos on Linux and in a browser, verified by screenshots.

## After milestone 1 (owner priorities, 2026-11-13)

Focus on engine functionality and the editor. Demo game balance is NOT a
goal (demos only exercise features).

1. DONE: launcher front end + settings (work package 6b).
2. DONE (2026-11-13): editor first slice: browser editor for novices (see
   docs/EDITOR.md, docs/EDITOR_UI.md): project picker, Game, Rules, Units,
   Weapons, Terrain, Maps (full map editor) and Play (embedded web player).
   Awaiting the owner's verdict on wording/layout. Left out on purpose:
   heights editing, visual-style preview on the map canvas, a scripts
   screen, inline field validation (Problems panel only).
3. DONE (2026-11-13): sprites and tilesets, engine + editor. Decisions and
   contract: docs/GRAPHICS.md (sheets + single images, 16-piece joining
   tiles, colour marker + optional per-side art, original generated starter
   art, simple looping animation). Editor: Art section, Picture cards,
   map canvas draws with art (docs/EDITOR_UI.md "Art"). Awaiting the
   owner's verdict on wording/layout and the starter art. Not in this
   step: 47-piece blending, walking/attack animation, sheet margins/gaps,
   animated joining tiles, unit art in the player's side panel/forecast.
   From here on, each engine feature lands with its editor screen.
   Owner's order (2026-11-13), one after the other:
   a. DONE (2026-11-14): structures, a separate layer placed on top of
      terrain squares, several squares allowed (docs/STRUCTURES.md).
   b. DONE (2026-11-14): animated terrain that also joins with its
      neighbours; bridges reach land (docs/GRAPHICS.md).
   c. DONE (2026-11-14): saving: Continue + slots, per-game setting
      (docs/SAVES.md).
   d. Alternative visual styles. Owner decisions (2026-11-14): a style is
      made separately by the game's maker and has a kind of view; the
      player switches between the styles a game ships that the platform
      can draw. Kinds: the current top-down 2D, 2D isometric, 3D isometric,
      full 3D (GameCube/Wii Fire Emblem-like). Isometric shows map heights
      as raised blocks. 2D isometric has one fixed angle; 3D isometric
      rotates. In 3D isometric the maker chooses per game between 2D
      sprites and 3D models for units. Full 3D: 3D map, free camera,
      fights on the map first; separate close-up battle scenes later.
      The 3D renderer must be portable to the console ports: an
      engine-owned 3D interface with swappable backends (OpenGL ES 3 /
      WebGL 2 first), no graphics-API or shader code in game data, fixed
      built-in materials, one directional light + ambient, modest
      power-of-two textures, CPU skinning, glTF models.
      Build order: d1 2D isometric (DONE 2026-11-14, contract:
      docs/VIEWS.md, with a Heights tool in the map editor); d2 the 3D
      drawing layer (first part DONE 2026-11-14, contract:
      docs/RENDER3D.md: interface, software backend that runs everywhere,
      glTF loading, test scene `--test-3d`; the OpenGL ES 3 / WebGL 2 /
      OpenGL 3.3 backend DONE 2026-11-14); d3 3D isometric (first part
      DONE 2026-11-14: blocks, sprites, turning camera, editor; still to
      do: nothing; units and structures can be animated glTF models, DONE
      2026-11-14, docs/GRAPHICS.md "Models"); d4 full 3D (map part DONE
      2026-11-14, docs/VIEWS.md "Full 3D": free camera with perspective
      over the block map, both demos have a "Full 3D" style; still to do:
      close-up battle scenes, low priority by owner decision; drawing
      3D at the display's resolution, smooth textures and a "Detailed
      3D" test style in `demo-srpg` from CC0 packs DONE 2026-11-14).
   e. Items (DONE 2026-11-14, docs/ITEMS.md: usable items, carried
      weapons with wear, accessories, unit inventories plus a shared
      storage per side, trading, shops, drops; `demo-srpg` uses them;
      not yet: selling, chests, stealing, scripted effects, carrying
      items between maps, AI that trades or shops).
   f. Story events and dialogue. Owner decisions 2026-11-14: triggers =
      battle start / end and turn number, a unit reaching a square or
      area, a unit defeated or fighting, a Talk command between two
      units; actions besides dialogue = add / remove / move units, give
      items or resources, set win / lose conditions, switches and
      variables; the first version includes a campaign (chapters in
      order, surviving units with levels and items carried over, scenes
      between maps); dialogue = bottom box with portrait; dialogue
      choices later; fallen units and free battles are the maker's
      settings. DONE 2026-11-14 (docs/EVENTS.md), the editor's
      Campaign screen included.
   g. More economy options. Owner decisions 2026-11-14: all four in the
      first version: fog of war (the maker switches it on per map;
      Advance Wars kind: the map is known, enemy units and far-away
      ownership are hidden outside sight range, some terrain hides units
      until adjacent), repair and supply (repair on own structures is the
      base; fuel and ammunition only for unit types / weapons the maker
      gives an amount), research (pay and wait: cost, turns (0 =
      instant), prerequisites, one at a time at a structure that allows
      it; unlocks unit types, structure upgrades or bonuses), upgradable
      structures (a structure becomes a better kind for a cost).
      DONE 2026-11-14 (docs/ECONOMY.md): core, player, editor, demo.
   h. Sound and music. Owner decisions 2026-11-14: music can be set for
      the title and per map (with a game default), per turn phase
      (player's turn / enemy's turn), for story scenes and by event
      commands (play music, stop music, play sound), and as victory /
      defeat / level-up jingles; players get separate music and sound
      volumes. Sound effects: one standard set per game with overrides
      per weapon, unit type and item. Starter audio: both a generated
      retro set (default for new projects) and a CC0 set (for the
      Detailed 3D demo style / as an alternative).
      Contract: docs/SOUND.md. DONE (engine, player, starter audio, the editor's Sound screen).
   i. Hexagonal grids (owner, 2026-11-14). DONE for the top-down view
      (the other three views on hexagons: later steps, not yet ordered).
      Owner (2026-11-14): maker chooses pointy-top or flat-top; one grid
      per game; top-down view first; structures of one hexagon or a
      hexagon and its ring of six. Contract: docs/HEX.md.
   j. A tutorial workflow for the editor (owner, 2026-11-14: after hex
      grids).
      Owner (2026-11-15): teaches the editor; hands-on steps that tick
      themselves; "Your first game" plus topic lessons; offered once,
      then behind a Learn button. Contract: docs/TUTORIAL.md. DONE (lesson texts
      to be reviewed by the owner).
   l. More strategy-RPG rules (owner, 2026-11-16; found while converting a private fan-game port, which stays
      outside this repository). Order: 1 weapon ranks and stat caps (rules + player, then editor screens);
      2 the defender's weapon visible to combat formulas; 3 character-locked weapons; 4 staves and status conditions.
      Owner's answers:
      - Ranks: the maker defines the ladder (E, D, C, B, A, S by default); growth with use is the maker's choice
        per game; a rank too low = the weapon cannot be equipped.
      - Stat caps: game default per stat, overridden by the class, adjusted by the character; no cap = uncapped.
      - Staves: ranged and area healing; moving units (send an ally away, pull one close); status conditions
        (with staves that cure them); utility (open doors/chests from afar, repair uses, temporary stat boost,
        light in fog). Conditions are built by the maker from blocks like skills, with a starter set. Whether a
        staff can miss is the maker's choice per staff (healing defaults to certain, hostile to a hit chance).
      5 doors, keys, chests and visiting (after staves). Owner's answers (2026-11-16):
      - A locked door opens by: a key item used next to it (maker decides which keys fit, and whether used up);
        a unit trait (thief-style, optionally needing a lockpick item with limited uses); a story event; or by
        being broken down (doors with hit points that can be attacked).
      - Chests and villages: the maker chooses per game between explicit Open / Visit actions in the unit's
        menu (uses the turn) and the current "stand on it" trigger.
      - Computer opponent: the maker marks per placed unit whether it goes for chests (taking the item),
        destroys villages, or opens doors with its own keys; unmarked units ignore them.
      6 dialogue choices (after doors; owner, 2026-11-16: "go ahead"). Assistant's default scope, for the
      owner to correct: a story command that asks a question with two to four answers, in campaign scenes and
      map events; each answer runs its own commands (lines, switches, items, joining units); a campaign can
      branch to a different chapter on an answer; the answer is saved; the simulator takes the first answer.
      6b story events read a unit's level (asked 2026-11-16 by the owner's separate game session; small, do
      right after choices merge, same files): a variable command that adds a unit's current level (missing unit
      adds 0) so sums and comparisons reuse the `variable` condition; map events (live units) and campaign
      scenes (roster levels). Starting shape `{ "addVariable": "bar", "level": "<unit>" }`, free to change.
      6b2 story command that takes an item from a unit (asked 2026-11-16 by the owner's separate game session,
      at the owner's request; built together with 6b): one copy or all, the equipped weapon included, map
      events and campaign scenes.
      6c campaign timeline in the editor (owner, 2026-11-16; build after dialogue choices merge, it draws
      their branches). Owner's answers: a flowchart, boxes joined by arrows left to right, branches drawn as
      separate paths; the maker can drag to reorder chapters, add / remove chapters and scenes, and click a
      box to open its normal editor (text editing stays in the existing forms); a box shows a map thumbnail
      (levels), a problems badge, and "what changes" markers (units joining / leaving, items, shops, switches,
      choices) - NOT a scene summary; a level box expands to show its in-map events in rough order.
      6d editor screen for status conditions (owner, 2026-11-16: "after stage three"): build a condition from
      blocks, the six starter conditions, and inflict / cure on staves, items and weapons.
      6e missing skill blocks (owner, 2026-11-16: "after doors and keys"). List from the private converter's
      step 7 (37 skills shown without effect), by how many each unlocks: auras limited to a kind of ally or
      to physical / magical damage (7); turn-number conditions (4); steal and unlock abilities (4, with doors
      and keys); follow-up strike control, formula-sized combat bonuses, weapon-experience multiplier (2
      each); then single blocks: move again but not after attacking, no carry penalty, "foe cannot counter"
      condition, switch off the foe's skills, heal the healer on staff use, ignore defence, cancel
      effectiveness, item-count condition, refresh an ally, staff range bonus, status cure, summon
      parameters, bonus-terrain condition, triangle multiplier, move past a neighbour. Also: a map unit with
      a skill of its own, and class skills that go away on promotion.
      7 battle and death quotes (after dialogue choices; owner, 2026-11-16: "go ahead"). Assistant's default
      scope, for the owner to correct: per character, lines said when it first fights (optionally only against
      a named character, e.g. boss vs lord, once per pair), lines when it is defeated (different lines for
      "falls" and "retreats" when the game keeps defeated characters alive), and a boss's defeat line; shown in
      the dialogue box before / after the fight; edited on the Characters screen; skipped by the simulator.
      8 preparation / deployment screen (after quotes). Owner's answers (2026-11-16): the screen lets the
      player pick who fights (map's slots and limit; maker-marked required units always in), swap starting
      positions, manage items (trade between units and shared storage), view the map and enemies, and buy from
      a preparation shop when the maker gave the chapter one. The maker chooses per map whether it has the
      screen; maps without one start as today. Needs the roster to supply units not placed on the map
      (Vision Quest converter gap: "the roster dresses only placed units").
      9 small rule gaps found by the private converter (2026-11-16; do after staves stage 1 lands, they
      touch the same core files): `addUnit` does not accept `ranks` although docs/SRPG.md section 7 says it
      does (bug: fix code to match the doc); a weapon lock that names classes (`wieldedByTypes`?) next to
      `wieldedBy`; `rankXp: 0` allowed; a per-unit exception to its class's weapon groups; `fw` for a foe
      that cannot counter at this range (currently still its weapon - decide and document); reaver-style
      triangle doubling needs nothing new (formulas can do it with a prop).
      10 support conversations (owner, 2026-11-16: "go ahead ... after the deployment screen"). The engine
      already has them (docs/SRPG.md supports: pairs, levels, conversations in battle / camp / both), so this
      is a private-converter step: read the ROM's support table (pairs, growth rates, bonuses by affinity) and
      the support scripts into data/supports.json. If something the original does is not expressible (affinity
      bonus tables, the limit of supports per character, paired endings) record it and build it then.
      11 skills conversion (owner, 2026-11-16: "go ahead ... after support conversations"). Private-converter
      step: find the hack's skill system in the ROM (which skills each class / character has and learns by
      level, skill names and descriptions from the text table), and express each with the engine's skill
      blocks (docs/SRPG.md skills). Skills whose behaviour is custom code cannot be read from data: map the
      ones that match a known effect, list the rest with their names as "not converted" (name and description
      shown, no effect), and record missing engine blocks to build.
      12 portraits (owner, 2026-11-16: after skills conversion; source "From the ROM, all of them"). Private-
      converter step: read every portrait from the owner's patched ROM (the hack artists' and any reused from
      the base game) into the private game folder only - never the public repo, never shared; the converter
      itself contains no art. Engine side: check what the dialogue box does with `portrait` today (step 2
      already writes the character id) and build what is missing (picture beside the box, left / right slots,
      a placeholder when a game has none) as an ordinary engine feature with the engine's own placeholder art.
      13 sound effects for the private port (owner, 2026-11-16: after portraits). Assistant's default, for
      the owner to correct: wire the engine's own starter sound set (tools/audio/make_starter_audio.py: menu,
      move, hit, miss, crit, heal, level-up, defeat...) into the converted game's data/audio.json, and convert
      the scripts' sound commands (dropped in step 3) where a starter sound fits. Not extracting the ROM's own
      effects: they are the base game's samples and synthesizer patches, hard to rebuild faithfully.
      14 map sprites and battle animations for the private port (owner, 2026-11-16: after sound effects).
      Map sprites: read each class's standing and moving map sprite from the owner's ROM into the engine's
      sprite-style unit art (assistant's assumption, following the portraits answer "From the ROM, all of
      them": private folder only, never shared; confirm when starting). Battle animations: the engine has no
      battle scene yet (owner earlier: "Battle scenes are low priority for now") - this is a large ENGINE
      feature first (a fight shown on its own screen: two fighters, attack / hit / miss / crit / magic
      animations, HP bars, a setting to turn it off), needing the owner's design answers (question dialog when
      reached), then the converter reads the ROM's animation frames and scripts into it.
   k. Strategy-RPG feature set. DONE 2026-11-15 (rules, computer opponent,
      player, editor screens, demo and template content, the lesson
      "Characters and skills"; contract and per-step "as built" notes:
      docs/SRPG.md). Open: wording and names are placeholders for the owner
      to review; new unit kinds reuse existing pictures (the Hound needs
      art); loose ends listed in HANDOFF.md.
      (owner, 2026-11-15: wants the features a
      GBA-era strategy RPG fan project needs; reference point named by
      the owner: the fan game "Vision Quest"). After the tutorial.
      Wanted: unit skills and abilities (passive and activated), shove
      and other moves that push or reposition units, summoning, support
      conversations and bonuses, rescue / carry, class promotion; close-up
      battle scenes stay low priority. Generic engine features only.
      Rebuilding that fan game's own story, maps and characters needs its
      authors' permission first (theirs, not ours); no Nintendo names,
      characters, maps, art or music in any case.
      Owner's answers (2026-11-15):
      - Skills come from the class, the character (so characters become
        their own thing: personal stats and growths on top of the class),
        weapons and items, and are learned at levels. Makers build skills
        from blocks (when / condition / effect); new games get a starter
        set made of those blocks.
      - Menu abilities (shove, swap, pull, summon...): the maker chooses
        per ability whether it uses the action, limited uses, a wait.
      - Summons: per ability the maker sets how many at once (default
        one), vanish with the summoner (default yes), optional lifetime.
      - Supports: closeness from ending turns side by side, fighting
        together, and story events; conversations in battle, between
        chapters, or both, chosen per game.
      - Rescue: both classic carry and pair up; each game picks one.
      - Promotion: by item, on reaching a level, by story event, with a
        choice when a class has several promotions.
      Contract: docs/SRPG.md (written; build order F1, F2, C1, K1...).
      Owner (2026-11-15, later): the editor says "class" once a game has
      a character, "unit type" otherwise. The demo and template drop the
      commercial series' name: neutral placeholder names chosen by the
      assistant (demo "Story Tactics Demo", "Sky Rider", "Salve").
      The other open questions of the contract run on its defaults.
   l. Standing terrain in the 3D views (owner, 2026-11-15: mountains and
      forests are flat pictures on top of the ground in 3D). A terrain
      in a 3D style can have a standing part on its ground: an upright
      picture facing the camera, or a 3D model; the maker picks per
      terrain, as for units. In progress alongside k.
   m. Proper models for the Detailed 3D style's stand-ins (owner,
      2026-11-15): Cavalier with a horse, the flying rider with a winged
      mount, Archer with a bow, Soldier with a lance, from further CC0
      packs combined with the current characters and credited. Starts
      after l is merged (same files).
   n. Hexagons in the other three views (owner, 2026-11-15: go ahead).
      Contract: docs/HEX.md "The other three views". 2D isometric first;
      the two 3D kinds after l (same drawing files).
   Not yet ordered: speed-based turns (FFT), height rules.
   Editor wording decided: "style" (never "look"); terrain lists keep the
   flat colour chip.
4. Native developer app (Dear ImGui) for advanced users.
5. Ports: Android, console/handheld backends, GLES3/SDL_GPU renderer, 3D
   grids, homebrew old-console ports via the SDL2 backend.

## Potential future features (not scheduled; owner to decide)

- **Obsidian vault projects** (idea, 2026-11-13). A project could be stored
  as an Obsidian vault instead of plain JSON files: one note per unit /
  weapon / terrain with stats in the note's Properties, `[[links]]` for
  references, design notes in the body, maps as a text block, Bases tables
  for side-by-side balancing. This is how OVERCLOCK's `game-data/` vault
  works (read it for the pattern; never edit it). Constraints found:
  the player should keep loading JSON (vault = authoring format, JSON
  generated on save/Play/export); Obsidian Properties are flat, so nested
  data becomes flat keys (`stat_hp`, `growth_hp`, `cost_funds`); needs a
  one-way (vault is the source) vs two-way (browser editor also edits the
  notes, preserving the body) decision. Recommended shape if built:
  two-way, as a second storage option inside `ote-editor` (the helper's
  on-disk revision check already detects Obsidian's edits).

## Settings model (owner decision, 2026-11-13)

Options are global: they apply to every game by default. A game can carry
a per-game setting that overrides one particular option for that game only;
everything not overridden keeps following the global value. Layers, lowest
to highest priority: engine default < game's own default in game.json
(e.g. its `font`) < user's global option (when not left on "game default")
< per-game override. (Confirm the middle ordering with the owner when the
options screen is shown.)

First options: font, text size, UI scale, visual style. "Visual style"
means a game can ship several switchable graphics styles (e.g. tilesets /
palettes) and the option picks one; design the sprite/tileset system with
named styles in mind. More options get fleshed out later.

## Known gaps / notes

- `versus` + per-weapon damage formulas cover AW damage charts and FE
  effectiveness; weapon triangle needs the defender's weapon exposed to
  formulas (not yet).
- A defeated side's buildings become neutral (AW gives them to the victor;
  make this a rule option when it matters).
- Unit ids index `Battle::units` and are never reused; dead units stay in
  the vector with `alive=false`.
- Player: the "+N Funds" toast is drawn over the side panel's terrain line
  at the start of a turn (seen in editor/web/screenshots/27). Cosmetic.
- Editor screenshots are lossily compressed (20-64 colour palettes), so
  colours in editor/web/screenshots can be off (24 shows forest as slate);
  judge colours in the running editor, not the PNGs.
