# Handoff

Current state of the build, for whoever (or whichever model) continues.
Read docs/PLAN.md (roadmap + owner decisions), docs/ARCHITECTURE.md
(structure + binding portability rules) and docs/GAME_FORMAT.md first.

## Latest (2026-11-24) — START HERE

**origin/main = local main (4902f52e), worktrees all removed.** Full check on the merged engine: sdl3/sdl2 2912 (2940 with the dialogue
branch, whose own full check passed), editor API 1640/1669, ctest 92/93, sims identical, webtest 280+59, vitest 1832/1864, e2e 632, native 25/25.
Pushed since 2026-11-22 (all optional keys; games without them are byte-identical; the owner KEPT every name, docs say "kept by the owner"):
- Scene shows another map (`showMap`/`returnMap`, cutScene maps), empty side panel then; HP bars of scene-added units (UnitAdded c = hp).
- Ability `area` for heal / cure / inflict-only, heal+cure in one effect, `inflict` as a list (docs/SRPG.md "Areas and several effects").
- Rules: `guest` / `guestJoins`, area shapes `ring` and `field`, range from 0 on the user, `arc` in inflict chance, a map's `abilityPoints`,
  strike `reach: "equipped"`. Downed units: `rules.downed`, `revive` (ability / item / staff), `autoRevive` condition.
- Graphics: map `backdrop` (+ `over` / `pieces`), `characters` looks, poses `attack`/`hurt`/`ko` + diagonals/back frames, sheet `unitOffset`, `portraitSize`.
- Dialogue: `dialogue.style` box|bubble, `notice` window, choice `cursor`, `askName` + `{name:id}` (campaign scenes only), scene `backdrop`,
  portrait `expressions` + `face`, story `loop`/`break`.
Merge lesson: three parallel worktree branches conflicted only additively; keep-both broke one CMake add_test (shared closing line) — build after.
Parallel agents: run check_all through `flock /tmp/ote-check-all.lock`, unique log names (the scratchpad is shared), link main's node_modules.

**Owner's PC rules learned:** ComfyUI runs on the RX 6900 XT (`~/ComfyUI/run.sh` + `gpu_watchdog.sh`, http://127.0.0.1:8188). Never run Xenia or
other GPU-heavy tests while it runs: tools/xbox360/xenia_smoke.sh crashed it once (VRAM page fault). Agents must not stop processes they didn't start.

**Xbox 360:** big-endian 2817/2817, xex builds, Xenia smoke OK on 8c578aac; console folder `~/.local/share/xbox360/OpenTacticsEngine-app`
replaced with that build (owner OK). Waiting for the owner's real-console `OpenTacticsEngine.log`.

**Vision Quest (private):** steps 49 (cut-scene maps) and 50 (ch23/ch24 staged fights) done and installed; tree clean.

**FFTA port (private, /home/kadenm/Projects/FFTAPort):** steps 7–14 done this session (statuses, immunities, AP, Regen, techniques, graphics,
areas, story campaign missions 1–8 with full dialogue, levelling, KO/revive, guests, prerendered maps + poses, bubbles / notice / name entry /
scene backdrops). Installed in games/ffta-port, validates clean. The automated campaign stops at chapter 5 (The Cheetahs) unless `--test-level 10`.
Open engine gaps (docs/CONVERSION.md steps 12–14): EXP for multi-target actions ("largest"), 0-damage hits earning EXP, askName only in campaign
scenes, `--campaign` not drawing a map backdrop (`--map` does), box fallback shows names despite `names:false`, strike knockback, an ability
that also affects its user, revive+heal in one, undead, per-map sky colour, weapon-specific attack poses, random-side field areas, speed order
with Haste/Slow. Port next: missions 9+, clan-name entry, sound/music. Materite Now's palette not live-checked.

## 2026-11-22 (previous)

**Correction:** the 2026-11-21 "next" items are already built (hexagons in all four views: docs/HEX.md "Status"; standing terrain in 3D:
docs/VIEWS.md "Standing terrain"). The thief work (165c877) is pushed.

**Local main (not pushed yet):** merges `scene-hit-timing` (effect keys `fromHitFrame`, `hitAfterMs`; docs/BATTLE_SCENE.md) and `native-map-painter`
(slots / units on the canvas, brush sizes, isometric preview, ote-mcp legend cleanup). Full check 71: sdl3/sdl2 2706, editor API 1587, ctest 84,
sims identical, vitest 1639, e2e 632, webtest chromium 280; webtest firefox needs `DISPLAY=:0` in an agent shell (notes). Push after it passes (owner OK).

**Branches waiting (worktrees under .claude/worktrees/):**
- `hit-hold` (mine): `battle.scene.hitHold` (a hit waits by the HP changed; docs/BATTLE_SCENE.md). Engine 2708, vitest pass; merge, then native tests.
- `fft-abilities`: ability points / mastery / equipment teaching / JP sharing / jobs unlocked by mastery; strike abilities, heal formulas, MP
  (`unitResources`). A code review found 6 bugs; the agent is fixing them and merging main in. Many owner decisions listed in its report (key
  names, "AP"/"MP" labels, defaults): ask the owner before or after merging.
- `ai-staff-first`: AI options `staffFirst` (roll before attacking), `staffPick: "lastPlaced"`, a silence-like target filter (in progress).

**Vision Quest (private):** step 46 A/B committed (spells from the hit frame, shots hit in flight; installed, check_game clean). Step 46 C
(`hitHold` in `conv_battle.SCENE` + `check_holds` rule check) is UNCOMMITTED in tools/conv_battle.py and tools/check_holds.py: commit and
reconvert after `hit-hold` merges. Findings 16 (hit gauge rule) and 17 (every staff row of the computer; staff-first roll) committed. Next: emit the
staff AI keys from findings 17 once `ai-staff-first` merges; remaining scene gaps: an area staff scene (no enemy uses fortify), per-set blow offset
(0..13 ticks), class-2 follow-up hold, atRange starting inside an effect.

**FFTA port (private, new):** `/home/kadenm/Projects/FFTAPort` (local git only; ROM on the 2 TB drive's ROMS folder). Steps 1-4: tables decoded
and checked, level-up and plain damage read from the ROM's code, headless mGBA emulator (`tools/emu.py`, silent), four battles installed in
`games/ffta-port`. Step 5 running (hit roll, mission -> formation, level shift, more missions). The PS1 FFT comes later (owner dumping a disc).

## Latest (2026-11-21, evening) (previous)

**origin/main = local main** (full check 69 clean: ote_tests 2694 both builds, editor API 1579, ctest 84, webtest 272+59, vitest 1629,
e2e 628 after the last changes). Pushed this session:
- Edge scrolling calmer (owner: started too easily, moved too fast): band 1/8 tile, dwell 0.5 s, 5 tiles/s (`battle/edge_scroll.h`).
- Editor Play panel slimmer so 3x fits a 1080p browser window: 4 px margin, tighter header, header selects capped at 13rem (one row at
  1440 px), one-line controls hint. 1920x900 now gives 3x (play-fit e2e case). Owner confirmed the black lines are gone.
- Vision Quest (private repo): step 43 item icons from the ROM (`tools/conv_icons.py`, 203 of 205 items, Dance/Play none); installed in
  `games/vision-quest`, check_game clean after fixing check_places' chest demo (thief stands on the chest since step 40).

Later the same evening (pushed, full check 70 clean but for `srpg_player_prep`, flaky under load, passes alone): edge-scroll band
back to 1/4 tile (1/8 unusable in a browser; owner said scrolling stopped working), dwell 0.5 s and 5 tiles/s kept; webtest
`edgescroll` suite. Owner confirmed the Play window now fills the screen. Vision Quest step 44 (private): spells at range, a caster
lead-in stays on the caster and screen-fixed pictures are screen parts (`tools/efx_follow.py`); installed, check_game 0 problems.

Owner (2026-11-21, late): item icons and the Play window size look good; the B-button fix is confirmed on the real Series X.
**Thief work done (2026-11-21, merged to main 90f3f3f, not pushed):** engine behaviour modes `loot` (plays its goesFor, never walks
toward the enemy, opens the doors it goes for; when nothing is left it escapes for good, saved as `escaping`) and `escape` (nearest of
the map's new `exits`, then the new action LeaveMap = removed like removeUnit); key `"attacks": false` (never starts a fight, still
counters); `rules.items.drops: "last"` (one dropsOf for all three defeat paths). Docs AI.md "Looting and escaping" / "Never attacking",
ITEMS.md "Drops"; tests tests/test_ai_loot.cpp. Editors: web behaviour panel (Loots, then leaves / Escapes / Never attacks: placeholder
wording), Map tab "Exits" list, Rules "When a unit is defeated"; schemas; native conformance. check_all fully green incl. web + e2e.
Port step 45 (VisionQuestPort 0f08eff): AI2 5 -> loot (lockpick counts for units with the lockpick ability), AI2 12 -> escape + map
exits from the ROM (16 maps), AI1 6 -> attacks false, thieves' last item drop + drops "last"; check_game 0 problems; installed.
Seen in ch06: the thief opens the door, the chest, then leaves at 14,1. Owner to play-test a thief chapter (ch06 / ch08).
**Next:** hexagons in the other three views (plan item n, docs/HEX.md "The other three views"), then standing terrain in 3D (item l).

**Ports caught up (2026-11-21 night):** Windows (fresh build, ote_tests 2701/0, ctest 84/84 after the ote-mcp Windows fixes, zip
smoke-tested from a non-ASCII folder), UWP appx 0.1.1122.426 (built clean, installed and played on the VM desktop, D3D11), Android APK
(autoplay battle + hardware 3D 60 fps in the emulator), original Xbox (autoplay battle in xemu, 60 fps, 21 of 64 MB). Builds for the
owner in the VM folder's `builds/`. Open: a half-bright line in Android-emulator 3D (notes); local commits not pushed (ask).
Still waiting on the owner: mouse scrolling and the spell animations feedback.

## Earlier (2026-11-21, afternoon)

The owner asked to stop here and continue in a fresh session. Everything below is merged into local `main`.

**Pushed to GitHub (full checks 65, 66, 67 clean):** native editor map painter / battle-art forms / drag reordering; deployment slot
tint under the unit; whole-device-pixel 2D rectangles; unlock ability follows a chest/door's `from` (AI thieves use the trait); test
level (weapon ranks rise, `auto` = enemy average + 2, `rules.promotion.testAt` + `testPromotedTag`); item icons in lists
(`graphics.json` `icons`); pointer scrolling only at the picture's edge (`engine/player/battle/edge_scroll.h`: band 0.25 tile, dwell
0.25 s, 8 tiles/s); scrolling seam regression suite.

**Merged locally, built, unit-tested; full check 68 was started on it** (log: scratchpad `combined68_verify.txt`; if this session
ended before it finished, re-run the full check and push `main` when clean — pushing `main` after a clean full check is agreed):
- Preparation lists ("Who fights", items, sell) show the deployed unit's class/level as it stands in the battle (`BattleScene::prepLive`).
- No seams on SDL's SOFTWARE renderer (`OutputMap::nudge`, `tools/webtest/suites/software.mjs`). Cause of the owner's black lines:
  their browser (Vivaldi) apparently gives no WebGL, so the web player runs on the software renderer. Inferred, not confirmed —
  ask them to check `vivaldi://gpu` / hardware acceleration; without WebGL the game is also slower.
- Whole-number scaling: player setting `integerScale` (default on), Options row after "Visual style", `--integer-scale on|off`,
  `embed.html?integerscale=`; the editor's Play dialog sizes itself from the browser window to the largest whole multiple
  (`editor/web/src/play/wholeFit.ts`). `tests/audio_player_test.py` was adjusted for the new row (down counts +1, the list scrolls one
  row after a tap, left arrow at x=200).

**Next steps, in order:**
1. Confirm full check 68 (or re-run), push.
2. Vision Quest item icons: the engine feature is in; the private converter (`/home/kadenm/Projects/VisionQuestPort`) must extract the
   ROM's item icons into a sheet and write `graphics.json` `icons` (owner: art from the ROM is fine for the private port, never public).
3. Ask the owner how the Play panel, the lines and the mouse scrolling feel now; they have not confirmed any of the three.
4. Windows VM / UWP / Android / original Xbox builds have not been rebuilt with any of today's changes (nxdk path of the new Options
   row was guarded out but not compiled).

**Vision Quest port** (private, local git, last commit "Step 42…"; engine copy in git-ignored `games/vision-quest` is current, 0
problems): steps 39–42 in its `docs/CONVERSION.md` — promotion by item, turn events once, thieves without Locktouch, chests from on
top, creating-row order (Titus's Dire Thunder + 8 others), rescue from the ROM, test level promotes at 15 with the ROM's promoted
bit as `testPromotedTag`. The last change (`testPromotedTag`) was patched into game.json by hand and into `conv_rules.py`; the next
full conversion reproduces it.

**Owner decisions still open:** native editor page/tool names; test-level +2 amount; slot number drawn over the sprite; enemy thieves
opening chests with a lockpick; edge-scroll numbers; whole-number scaling row position; older list below.

## Earlier (2026-11-21, night)

- **origin/main = local main** (full check 64 clean: ote_tests 2659, editor API 1572, ctest 81, vitest 1619, e2e 620);
  includes `ote-mcp`.
- **Mechanics** (all pushed): damage types, research trees, factions, area/line fire, action points, height, facing, ranks,
  jobs, speed turn order, merit ranks + overlay jobs, commanders (enemy meters; bar only at UI 150), `changeSide ready`.
- **Stricter loaders** (pushed): unknown keys refused with "did you mean", numeric ranges, `ote-editor --validate <folder>`
  (read-only). Quiet keys are listed in docs/GAME_FORMAT.md "Strict files" (`editor` at game.json root, `name` on graphics
  sheets: the web editor writes both). Web editor number boxes follow the loader (`editor/web/src/model/dataLimits.ts`).
  Known gap: `model/skillChecks.ts` (pinned mirror) has no 0..100 check for a block's percent chance.
- **Original Xbox** (pushed): 60 Hz in xemu; flicker fixed (never draw into the shown buffer; detector
  `tools/xbox-og/flicker_check.py`, `SHOT_BURST` in OGXbox/run-xemu.sh). Owner's test files: `OpenTacticsEngine-OGXbox/for-testing/`
  (built from the flicker-fix branch; boot smoke-tested). The owner's xemu must load THAT iso (it once loaded a stale
  `build/player/ote-og-player.iso`; `dvd_path` was reset, backup `xemu.toml.before-disc-fix`). Real console never tested.
- **Native editor** (`editor/native`, Qt 6, option `OTE_BUILD_NATIVE_EDITOR`; docs/NATIVE_EDITOR.md): table/tree editors,
  schema-driven forms for every data file incl. story events, campaign, audio, graphics (raw: tiles/tileLayer/heights, battle
  scene art). Built in `build-native/` of the main tree; launcher `~/.local/share/applications/ote-editor-native.desktop`.
  Tests: `QT_QPA_PLATFORM=offscreen ctest --test-dir build-native -R native` (17; conformance takes 3+ minutes; the full
  check does NOT build this option, so run these by hand after any editor/ or loader change).
- **ote-mcp** (`editor/mcp`, docs/MCP.md): stdio MCP server, no Qt, 23 tools, schema engine now Qt-free in `editor/core`
  (native model wraps it). Tests `ctest -R "mcp|ote_editor_core"`. Unverified: Windows branch, any real MCP client, the
  claimed 2026-07-28 protocol revision (looked up by the agent; newer than my knowledge).
- **Vision Quest (private)**: converter step 38 (no `_rain`/`_snow` working keys in game.json); validates clean under the
  strict loader.
- **Next candidates**: owner tries the Xbox build, native editor and MCP; map painter; battle-scene art forms; Lua/script
  tools; in-process preview; Windows builds of native editor (Qt in the VM: ask) and ote-mcp; MCP `requireClean`, bundled
  templates; rebuild Windows zip / UWP / Android (stale since 278e11e).
- **Owner decisions pending**: native editor name / wording / comment handling; merit for every side; class-job units cannot
  promote; commander meter rules; older list (AP pool, research refund, faction start resources, facing, speed-turn numbers).
- **Heavy parallel jobs slow the owner's PC**: full checks under `nice`, few agents at once.

## Earlier (2026-11-19, night) — read this first

origin/main = 75795e5 (full check 47: ote_tests 2309 both builds, editor API 1399, ctest 66/66, vitest 1614, e2e 616 in
15.7 min, sims identical). Local main is two merges ahead (enemy ranges, stacked storage lists): full check 48 was started;
push only after it passes. The section below this one is older; the ports part of it is still right.

Pushed today, all fully checked:
- Audit jobs D6 (`-DOTE_SANITIZE`, `.clang-tidy`, docs/TESTING.md), D7 (`tools/check_all.sh`), D8 (test scratch under
  `<build>/test-tmp`).
- One B press arriving from two sources is one press (`engine/platform/common/press_dedupe.*`, docs/XBOX.md). Not yet
  confirmed on the owner's Series X pad.
- Art loads on demand inside a budget (`battle/sprite_cache.cpp`, `battle/art_budget.*`, `artBudgetMB()`): a 640-sheet game
  peaked at 2.6 GB at load, now under 200 MB. 3D sheets/models and sound effects are not under the budget.
- **Original Xbox (2001) port, step 1: the 2D player via nxdk** (docs/XBOX_OG.md; `engine/platform/nxdk/`, `tools/xbox-og/`).
  Runs in xemu only: hard-disk install, user data in `<launch folder>\userdata`, B on the launcher asks then returns to the
  dashboard, "Screen margin" option (90% at 480, 95% at 720p; CRT-era title safe was 80%, action safe 90%), FATX-safe names
  (`core/base/fatx_names.*`), 640x480 default, 720p opt-in. Sound is unproven (xemu's chip consumed nothing). Toolchain and
  the emulator loop live outside the repo in `/home/kadenm/Projects/OpenTacticsEngine-OGXbox/` (README.txt there).
- Web player: Emscripten's `Browser` helper is linked, so a browser that refuses WebGL draws in software instead of
  stopping ("Browser is not defined", seen in the owner's Vivaldi); a warning line says why WebGL was refused.
- List icons of units are sized as on the map (tall frames were shrunk to half).
- Free battles: Items and the shops in the preparation screen, and `rules.items.freeBattleStorage`
  (`{each, except, items, funds}`; docs/GAME_FORMAT.md). Known gap: the Continue save of a free battle does not carry
  preparation trades.
- Hint lines name the device in use (`engine/player/ui/input_hints.*`; keyboard, controller, touch, mouse = "click";
  White/Black on the original Xbox; `OTE_INPUT_DEVICE`, script word `device:NAME`; docs/PORTING.md).
- `tests/art_memory_player_test.py` works on Windows.

Merged locally, in full check 48:
- Enemy ranges (docs/SRPG.md "Enemy ranges"): Confirm on a visible enemy marks its move (violet) and attack (orange)
  squares; the new `Button::Ranges` (`T`, pad Back/View, map-menu row) toggles all; option `enemyRanges`; game rule
  `rules.display.enemyRanges`. Confirm on an enemy no longer opens the map menu; Cancel on the idle map clears marks
  (both await the owner's review).
- Identical copies in storage lists are one row with a count (`battle/storage_stack.h`, docs/ITEMS.md).

Running as agent jobs when this was written: the native NV2A 3D backend for the original Xbox (step 2), and damage types
(first of the new mechanics). The plan for the new mechanics (damage types, side-wide effects + branching research,
factions, line fire + area attacks, AP turns, height, facing, rank table, CO powers, speed turns, jobs) is in
`.agent/notes.md` "New mechanics plan"; the owner's OverClock project (`/home/kadenm/Projects/UntitledTBT`) is ideas only
and is never edited.

Builds in the Windows VM from 278e11e: Windows desktop zero warnings, ote_tests 2299, ctest 63/63 with the test fix;
`builds/uwp/OpenTacticsPlayer_0.1.1120.446_x64.appx` (B fix + on-demand art) is the package to install on the Series X
when the console is free; nothing newer than 278e11e has been built for Windows, UWP or Android.

The private Vision Quest port (`/home/kadenm/Projects/VisionQuestPort`, never in this repo): step 36 fixed classes, levels
and items of already-recruited characters (the ROM ignores those list rows), forced deployment (ROM table), area events'
trigger side, and added the free-battle stock. Open with the owner: which "ruins" tiles should be impassable (the ROM says
passable), recruit music restore (by report, not by disassembly), the "fluctuating" recruit theme (in the song data).

## Earlier (2026-11-16 to 2026-11-19)

Merged, fully checked (ote_tests 1321 both builds, editor_api 758, vitest 897, webtest 152/59, e2e 427 passed / 29
skipped, demo sims line-for-line identical) and pushed: weapon ranks and stat caps (rules, player, editor screens;
docs/SRPG.md section 7), the foe's weapon in formulas (`fw`) plus a weapon's `id` / `group` as text in `w` and `fw`,
character-locked weapons (`wieldedBy`, rules and player only).

Also pushed since (all on `origin/main` up to bc1421c unless marked): the `wieldedBy` card; staves stage 1, 2
(warp, pull, swap, repair, light) and their editor forms; five small rule gaps; dialogue choices (`choice`, `goto` in
before scenes); story commands reading a unit's numbers into a variable and `takeItem`; the campaign timeline
(Campaign screen, Timeline tab); status conditions (docs/SRPG.md section 9, starter set of six) with the editor's
Conditions screen and story commands; doors, keys, chests and visiting (section 10, no editor forms yet); a sidebar
fix (the section list scrolls on a short window).

Also pushed since (2026-11-17, every merge fully checked): battle and defeat quotes; skill blocks (three sets, docs/SRPG.md);
the preparation screen with selling and Equip; the phone section menu; editor forms for doors, chests, places, class
locks, character wields, ranks on addUnit; the battle scene (docs/BATTLE_SCENE.md) with per-side sheets, assign by
weapon and side, 512 frames, ranged and miss slots; walking animations for map units (docs/GRAPHICS.md "Walking");
per-unit behaviour for computer units (docs/AI.md); renaming a character's id; doors, chests and villages in the
strategy-RPG template; staff sounds.

Pushed later on 2026-11-17 (origin/main = 04c3c47, every merge fully checked): map changes (`setTerrain`, named
`changes`, breakable walls for the computer, fog options: docs/EVENTS.md "Changing the map", docs/ECONOMY.md "Fog
options"); story scene presentation (docs/EVENTS.md "Scene presentation"); the `actor` condition, `storeItems`,
fading conditions (`scale: turnsLeft`) and strike marks; campaign parties (campaign doc "Parties"); difficulty options
(docs/DIFFICULTY.md).

Pushed after that (origin/main = 5eefb11 plus this note; every merge fully checked): the rules batch (docs/SRPG.md:
`hitRoll: average2`, formula `side`/`human`, experience extras, per-type `maxLevel`, `computerCaps`, ability `inflict`,
roster conditions, hide / `showUnit`); stage slots, `enter`/`move`, portrait `talk`/`blink`, music by situation, step and
animation frame sounds, difficulty row on the game page, `focus` speed; map weapons (ballista-style structures);
per-difficulty variants of one unit on one square / id; the tile layer (docs/GRAPHICS.md "Tile layer": `tilesets`,
a map's `tileLayer`, tiles in changes; hex games refused); the second fidelity batch (inflict-only abilities, staged
`fight` command, `battleBackgrounds` / `battlePlatforms` per map + `battleBackgroundFrom`, terrain `weatherCost` + map
`weather`, `hurt` ranges, summon `items`/`pick`/`replace`, `battleSceneSides`). No engine job is running.

Pushed on 2026-11-18 (origin/main = 7addf9b): the fixed stage for battle scenes, test levels and playing from a chapter
(the editor's Play panel), battle effect features, free battles that wait for the story (`freeBattles` object form,
`progress.json`: docs/SAVES.md), and three fidelity batches, all opt-in:
- battle scene 3 (docs/BATTLE_SCENE.md): `battleSceneStaves`, effect `blend: add`, `hide`, `fade`, screen `order`,
  `stage.ranges` with `view: each` + `pan`, travel `speed`, `hitAtMs` up to 15000;
- fidelity 3: ability `spendsItem`, summon `weight` / `maxLevel` / `item: null` (32 candidates), character
  `promotion.fromLevel` and `fromLevelFor`, `weatherCost: "blocked"`, `prep.shops` with music, sound slots
  `levelUpTick`, `save`, `shopMusic`, `bootJingle`, `promotionSounds`;
- scripted-style computer players (docs/AI.md): `rules.ai.turnOrder: order` + behaviour `order`, `rules.ai.weights`
  sets, `caution: none`, `retreat`, `idleTurns`, mode `patrol`, `ignores`, `guardArea.measure: moveCost`.
Also pushed on 2026-11-18 (origin/main = 4f65853, full check: ote_tests 2218 both builds, editor API 1365, ctest 34,
vitest 1589, e2e 613): classic menu options asked for by the owner (docs/SRPG.md "Menu options"): unit pictures in the
preparation unit lists (always), `menuUnavailable: grey|hide` (also a player setting), `attackWeaponChoice:
equipped|pick`. The options screen gained a row, so `tests/audio_player_test.py` taps and row counts moved with it.
Known small flaws: the phone shop panel cuts an item's price ("20 G.."); at UI scale 150% on a phone-shaped screen the
preparation list cuts unit names; weight sets cannot be renamed in the editor.

Pushed later on 2026-11-18 (origin/main = 6bf5aa7, every batch fully checked: ote_tests 2253 both builds, editor API
1399, ctest 61, vitest 1614, e2e 615): the 22 player tests missing from CTest; faster fight forecasts (stats once per
forecast, generational Lua collector) and AI distance fields remembered across steps; loader helpers
(`pickCommandKind`, `LoadCtx::integerIn` / `integerFrom`); healers that heal only retreating allies (`heals`), an item's
own heal sound (`sounds.heal`, `healScene`); battle scene 4 and 5 (docs/BATTLE_SCENE.md: `pan.backdrop`, `pan.startMs`,
`atRange` with `delayMs` / `endOffset`, `atRangeFixed`, `panAtMs`, held poses `holdFrame` / `startFrame` /
`startDelayMs` with `battle.scene.resultHoldMs`, `battlePlatformFrom: own`); port groundwork (docs/PORTING.md: content
and user roots, life cycle events with `--input` tokens `background` / `foreground`, back key, MSVC flag block); the
first Android build (docs/ANDROID.md, `tools/android/build_apk.sh`, hardware 3D through an EGL pbuffer context, back on
the launcher leaves the app: owner's decision). Tests must not count the games in the checkout's `games/` folder (it
holds untracked games): use a temp folder.

Private converter (separate folder, never public): steps 22-35 done and installed in the git-ignored
`games/vision-quest`. Owner's play-test findings fixed: scrambled enemy-side platform (wrong half of the tile blob), a
rout ending a chapter (`rules.defeat` now empty), dodges far too short (the original holds a pose until the result has
been shown; command words cost a tick each). Reinstall = `python3 -I tools/convert.py`, `python3 -I tools/check_game.py`
(about 45 min; must end "45 maps checked, 0 problems"; read the result BEFORE copying; do not run it while the engine
is being rebuilt), then replace the folder. Engine features the port still lacks (step 35's list): a shot's hit before
its arrival, an effect's lead-in counted from the hit frame, `atRange` starting inside an effect, a follow-up hold per
distance class, the damage gauge's length by hit points, a retreat state without a destination (one healer, ch17).
Sounds with no source found: victory, chapter card, preparation music, turn banner, steal, trade, shop buy / sell,
experience bar, healing-item use. Not reproduced: the world map, the original's menus (need not match, owner). Next:
the owner plays again.

Ports (owner's order; research in `.agent/notes.md`, decisions in Claude memory). State on 2026-11-19, origin/main =
a4cdd8d, every merge fully checked (ote_tests 2257 both builds, editor API 1399, ctest 63, vitest 1614, e2e 616 in
about 16 min):
- Android (docs/ANDROID.md): builds and runs on the emulator (touch, back, suspend with Continue save, hardware 3D,
  rebuilt after a simulated context loss). Never run on a real device or on arm64. Open: app name / id / icon
  (placeholders), safe-area insets, loss of SDL's own 2D context.
- Windows desktop (docs/WINDOWS.md): MSVC build of player, tools and tests; `engine/core/base/fileio.*` (`openFile`
  UTF-8 paths, `replaceFile`, `executableDir`), `-DOTE_MSVC_STATIC_RUNTIME` (/MT default), `tools/windows/package.ps1`
  makes the zip; user folder `%APPDATA%\OpenTacticsEngine\player\`. Never run on a real PC (sound, controller, full
  screen, a real GPU). The editor helper with non-ASCII game paths on Windows is unchecked.
- Direct3D 11 backend (`engine/gfx3d/d3d11/`, docs/RENDER3D.md): the hardware 3D choice on Windows; HLSL precompiled
  to headers by `tools/windows/make_d3d11_shaders.ps1` (rerun after editing `shaders/render3d.hlsl`); device loss
  through `ResilientRenderer3D` (`engine/gfx3d/resilient3d.*`). Switches: `OTE_3D_API=opengl`, `OTE_3D_D3D11_OWN=1`,
  `OTE_3D_D3D11_LEVEL=10_1`, `OTE_3D_D3D11_DRIVER=warp`, `OTE_WINDOW_HIDDEN=1`. Verified only on WARP (software
  Direct3D) at levels 11_1 and 10_1; ctest `render3d_d3d11` is skipped off Windows. One unexplained exit -1 of a
  windowless run in the VM, not reproduced in 48 runs.
- Xbox / UWP (docs/XBOX.md, `tools/uwp/build.ps1`): the SDL2 platform as a UWP app (`OTE_UWP`), SDL2 2.32.10 built from
  pinned source and patched to export `SDL_RenderGetD3D11Device`; content from the install folder, saves, `log.txt`
  and `args.txt` in LocalState, extra games in `LocalState\games`. B on the launcher asks "Exit the player?" (owner's
  decision; `launcherBack()` in `engine/platform/platform.h`: Android exits, UWP confirms, desktop nothing;
  `OTE_LAUNCHER_BACK` overrides). Checked on the VM's desktop with a keyboard and with the Windows App Certification
  Kit (27 of 27). NEVER run on a console: controller, sound, TV safe area, the 1 GB memory budget and whether the
  Device Portal accepts the test signature are all open. Xbox One (level 10.1) untested.
- Windows VM (QEMU/KVM, no window) on the external drive, folder `OpenTacticsEngine-WindowsVM` (README and scripts
  there: `vm-start.sh` (8 CPUs, 10G, UTC clock), `vm-stop.sh`, `vm-ssh.sh`, `vm-shot.sh`, `vm-type.py` with `wait:N`
  keys; key login as `builder` on 127.0.0.1:2222; Windows 11 Enterprise 90-day evaluation installed 2026-11-18, so it
  expires mid-February 2027). Installed: VS 2022 Build Tools (v143, UWP tools), Windows SDK 10.0.26100, Python 3.12,
  Pillow, ffmpeg, SDL3 VC package. Trees `C:\work`, `C:\work2`, `C:\uwp`; the test certificate lives only in
  `C:\uwp\cert`. ssh sessions have no desktop: start windows through a scheduled task with `/it`. `PC-Backup`
  beside the VM folder is the owner's: never touch it.
- Builds for the owner in the VM folder's `builds/` (demo games only): `OpenTacticsEngine-windows-x64.zip` and
  `uwp/OpenTacticsPlayer_0.1.1119.1452_x64.appx` with `Microsoft.VCLibs.x64.14.00.appx` and `ote-test.cer`.
  Waiting for the owner: install the appx through the console's Device Portal (or give the console's address), try
  the zip on a real PC.
- After Android and Xbox work: a one-day feasibility test for the original Xbox (nxdk + xemu, 480 and 720p), owner's
  go-ahead for the test only. Then the advanced native editor (Linux and Windows first); the web editor's UI overhaul
  waits for a UI framework somebody else is writing: no large editor UI work until the owner says so.

Editor: the 17 section screens load on demand (`React.lazy` in `editor/web/src/shell/ProjectShell.tsx`, first load
1572 -> 482 kB). That needs the helper's `httplib::ThreadPool(64)` and 1 s keep-alive in `editor/server/server.cpp`:
without them the many chunk requests starve the helper and the e2e suite takes twice as long with ~20 failures. A
suite that suddenly takes twice as long is a real signal, not load.

Engine audit list (order in `.agent/notes.md`): done D3+D1, C2, C1, A1+A9, A2, C3 (as a cache), C7, D2 (`tests/ote_testkit.py`), A3, D5 (`ote_player_lib`: the player's
SDL-free sources compile once; real builds done on Windows, UWP and Android), A4, A5 (the build has no warnings: keep it
so), A7, A8, C4 (only 2 %: the cost is the ~52,000 forecasts per run, not the copy; a forecast cache would be the real
fix), C6 (revision cached by path, size and time; entries younger than 2 s are not trusted). Next: B5, D6, D7, D8, docs
B1-B4, the phone shop price and prep list flaws. Known and left: the first save of a project re-formats data files the
editor did not change.

Original Xbox (nxdk) feasibility test, 2026-11-19: built, never run (details in `.agent/notes.md`; local branch
`xbox-og-feasibility`, not for merging; toolchain and `run-xemu.sh` in `~/Projects/OpenTacticsEngine-OGXbox/`). Core
and player compile and link with a handful of `OTE_NXDK` switches; software 3D is far too slow there (a native NV2A
backend would be needed); memory (35-45 of 64 MB, estimated) is the open risk. Blocked on the owner dumping the boot
ROM, BIOS and disk image from their own console; never download those.

Open with the owner: the wording "Exit the player?"; the app's name, identity and logos on Android and Xbox (changing
the identity later loses saves); the 3D option's "OpenGL" label now means Direct3D 11 on Windows; names of the three
difficulties; "Only on" difficulty limited to computer-side units; all new
editor wording is placeholder; shop labels in the port; a locked door without a key is hidden under `hide`. See
`.agent/notes.md` on a committed `node_modules` link before merging any agent branch.

Test notes: under load (several jobs at once) editor e2e and webtest flake; re-run failures alone before judging.
A failure that repeats alone is real (the sidebar bug was found that way).

## Earlier (2026-11-15, evening)

Pushed up to `82ed289` (2026-11-15 night): the rules core, AI1, U1–U5, E1–E4, hexagons in the 3D kinds, the 3D unit
models, standing pictures moved onto their tiles in 3D (owner's report), and muted test browsers (owner's report:
tests played sound; Playwright config and `tools/webtest/run.mjs`). docs/SRPG.md has an "as built" note per step.

- **Strategy-RPG feature set** (PLAN k, docs/SRPG.md): every step is merged, T1 (lesson "Characters and skills",
  started from the blank template; document pointers) last. Demo saves on the owner's machine are refused again
  (D1 changed demo-srpg). D1: ids `sky_rider`, `salve`, `mending_salve`; e2e `game.create('strategy-rpg')` strips
  the template's D1 content unless `{ srpgContent: true }`; helper checks use `tests/editor_api_plain_units.py`.
  The seven new unit types have their own art in every style (2026-11-16; `tools/art/*_promoted.py`, `glb_gear.py`,
  `tests/srpg_art_test.py`; `make_detailed_style.py` needs `--hound DIR` with the CC0 Husky.glb, see CREDITS). The
  promotion screen of a model style borrows the picture of a sprite style. Weak: Detailed 3D Outrider.
  Owner decided (2026-11-15): the strategy-RPG template keeps the 30 starter skills in its list.
  Known nits: the
  empty Skills / Supports screens show the empty text twice; E2 did not replace E3's own "Only if" rows; characters'
  personal stat bonuses are not cleaned when a stat is deleted; F2's `Battle::testMods` is always compiled; A1 left
  summons out of `changeSide`. Player loose ends: the promotion banner covers the promoted unit; at phone size
  greyed ability entries lose their reason, the Abilities page shows one ability then "+N more" and the pick-mode
  hint line is cut; the camp screen is plain. Each U step has its own `tests/srpg_player_<step>_test.py`; they pick
  menu entries by position (pick Wait with Up, it wraps to the last entry). An empty feature file (the editor's
  remembered switch) is the same game as no file.
- **Hexagons in the 3D kinds** (PLAN n): done; docs/HEX.md "Implementation notes (3D)", `tests/hex3d_player_test.py`.
- **3D unit models** (PLAN m): done for cavalier, sky rider, archer, soldier (Quaternius horse, bow, spear, CC0, via
  Poly Pizza; wings generated by `tools/art/glb_wings.py`); rebuild command in `games/demo-srpg/CREDITS.md`. The owner
  has not yet said whether the blocky horse is good enough.
- **Verifying a merge**: both builds + `ote_tests` (1255 with the player steps), sims (table in docs/PLAN or the last report),
  `tests/*_player_test.py`, `render3d_backends_test.py`, `hex_content_test.py`, `editor_api_test.py`, editor
  typecheck/build/unit, web build + `tools/webtest`, full e2e. With several agents building, e2e times out at random
  (30 s `saved()` waits, mostly Firefox): re-run the failed specs alone before believing a failure.
- Pins that follow demo data: `tests/test_srpg_compat.cpp` holds the demo-hex data hash and the demo-srpg campaign
  save hash; both were re-taken today for deliberate data changes (map heights, the demo's name).
- Owner's own games (`games/test`, `games/practice-*`) are untracked: never touch. Demo saves on the owner's machine
  are refused after today's data changes (demo-srpg names, demo-hex heights).
- Vision Quest rebuild: only if its team gives permission (owner is asking on the Fire Emblem Universe forum); it
  would live in a git-ignored game folder, never in this repository, with no Nintendo assets or names.

## State (2026-11-14)

Everything below is merged on `main`, which is also on GitHub:
https://github.com/legoboy0109/OpenTacticsEngine (public, remote `origin`;
owner said yes to uploading on 2026-11-14). Keep it up to date as work
lands; never force-push. Commits use the account's GitHub no-reply address
(set in this repo's git config), never a personal email.

- Story events, build steps 1 and 2 (docs/EVENTS.md): the rules core is done
  and tested: unit ids, switches / variables, events, objectives, Talk, saves
  and hash, `ote-sim` output. The player shows them inside one battle:
  dialogue box (letter by letter, pages, portrait, skip; `--input` token
  `skip`), Focus / add / remove / relocate / side-change effects, Talk in the
  action menu, objective banner and "Objective" in the battle menu, portraits
  in `data/graphics.json` (docs/GRAPHICS.md). Not done: editor, web
  build check of the new code. No shipped game has events yet (so nothing
  looks different in the demos; checked byte for byte).
- Campaign, build step 4 (docs/EVENTS.md "Campaign" and "Campaign (player)", docs/SAVES.md "Campaign saves"): done in
  the core and in the player. Core: `data/campaign.json`, `engine/core/campaign/`, tests `tests/test_campaign_*.cpp`,
  `ote-sim <game> --campaign`. Player: game page (Continue / New game / Load / Free battles), title card + scenes in the
  battle's dialogue box, chapter flow with retry / end screens, campaign saves in Continue and the slots,
  `--campaign`, `--autoplay` through a story (`engine/player/app_campaign.cpp`, `campaign/`, `battle/battle_campaign.cpp`);
  tests `tests/test_campaign_player.cpp`, `tests/campaign_player_test.py` (windowless playthroughs on a generated
  three-chapter game, also the generator for the web test), `tools/webtest/suites/campaign.mjs`. No shipped game has a
  campaign yet and the editor has no Campaign screen (step 5), so nothing looks different in the demos (launcher and
  game pages compared pixel for pixel with the pre-campaign build). Decisions the owner can change: a free battle of a
  campaign game never writes the Continue file ("not saved automatically"; by hand into a slot is fine); the title card
  stays 2.5 s unless confirmed; Game options and Back sit above the Free battles list; the first chapter's before scene
  cannot name roster units (the roster is empty then: use `speakerName`); removeUnit counts as fallen; a unit that starts on
  a computer side and is recruited by an event is dressed from the roster only from the next chapter on; carried storage
  has no size limit; Retry starts the battle with a new seed.
- Rules core (`engine/core`): JSON, loader, grid, battle, Lua formulas, AI;
  `ote-sim` plays both demos AI-vs-AI. `ote_tests`: 759 green (SDL3 and
  SDL2 builds).
- Player (`ote-player`): launcher, options with per-game overrides, real
  fonts; SDL3 and SDL2 backends; web build (Emscripten). Webtest:
  Chromium 134 (incl. the new audio suite), Firefox 59.
- Editor: helper `ote-editor` (API test 250 green) and the browser UI with
  every screen incl. map editor, Play, Art and Structures (258 unit tests,
  171 e2e in Chromium + Firefox smoke, 15 screenshot specs skipped unless
  `SHOTS=1`).
- Art (docs/GRAPHICS.md): `data/graphics.json` + `art/*.png`; joining
  terrain can be animated (`"join"` as a list of 4x4 blocks) and can join
  with "everything except" (`joinExcept`, used by bridges so they reach
  land). Starter art comes from `python3 tools/art/make_starter_art.py`.
- Structures (docs/STRUCTURES.md): their own layer on top of terrain, can
  cover several squares; capture, income, building units and headquarters
  live there. Old-format games are refused by the loader with a message;
  the editor offers Convert (one undo step). An empty map `"owners": []`
  is still accepted (older editor versions always wrote one).
- Saving (docs/SAVES.md): automatic Continue save + manual slots, per-game
  setting `game.saving` (`any`/`continue`/`off`, Rules screen "Saving"
  card), "Leave battle" in the battle menu. A save stays valid when a game
  changes only cosmetically (names, colours, letters, fonts, styles, art);
  rule or map changes make it "made with a different version". A Move
  alone does not write the Continue save (owner: wanted).
- Waiting on the owner: verdict on editor wording/layout (incl. Art,
  Structures, Convert, Saving card, picture dialog's animated joining),
  starter art, launcher layout, font picks. README is a first short version.

## Editor helper

`ote-editor` (editor/server) serves the browser editor and the HTTP API in
docs/EDITOR_API.md, using the real loader for validation. Starter projects
live in editor/templates (strategy-rpg, economy-tactics, blank). Build and
run (does not need the player target):

```
cmake -S . -B build -G Ninja -DOTE_BUILD_PLAYER=OFF && cmake --build build
./build/ote-editor            # from the repo root; http://127.0.0.1:8765/
python3 tests/editor_api_test.py ./build/ote-editor .   # also ctest "editor_api"
python3 tests/render3d_backends_test.py ./build/engine/player/ote-player .   # OpenGL vs software pictures
```

Notes: all /api handlers run under one mutex (engine core is not
thread-safe); `/` shows a built-in placeholder page until the web UI is built
(editor/web/dist). Creating a project does not touch
games/index.json. The helper is built with exceptions on (httplib, v0.26.0 in
third_party/httplib); the engine itself stays exception-free.
`ote_tests` writes a settings file into `<repo>/build/`, so that folder must
exist when running it from another build dir name.

## Editor web UI

`editor/web/` is the browser editor (TypeScript, React 18, Vite; runtime deps: react, react-dom only). All screens are
done: project picker, Game, Rules, Units, Weapons, Terrain, **Structures** (cities, forts, factories; the Terrain form points to it),
**Art** (images, pictures, picker), **Maps (full map editor with the Structures tool, drawn with the art)** and **Play** (embedded web
player). Projects made before structures existed get a one-click, one-undo-step **Convert** offer (a banner on every screen).
Layout: `src/api` (the only fetch calls), `src/model` (pure doc edits, no React; `map*.ts` = the map grid model,
`graphics*.ts` + `pickCell.ts` = art, `structures*.ts` + `mapStructures.ts` = structures incl. `structuresEffective.ts` (the engine's
terrain + structure combination rule) and `structuresConvert.ts` (old projects)), `src/store` (undo, autosave, conflict; `art.ts` = the image files),
`src/art` (drawing art, shared by canvas/palettes/previews), `src/ui` (components; `picture/` = Picture card, picker, piece grid),
`src/screens/<section>` (`maps/editor/` = canvas, tools, panels, `drawStructures.ts`; `art/` = the Art section; `structures/`), `src/play` (Play button logic + panel),
`src/shell` (top bar, sidebar). Spec and decisions: docs/EDITOR_UI.md (the art notes explain the upload/autosave handling, the structures notes the Convert flow).

Run the whole editor end to end from a clean checkout (needs CMake, Ninja, Node 20+, Emscripten for Play):

```
cmake -S . -B build -G Ninja -DOTE_BUILD_PLAYER=OFF && cmake --build build            # the helper -> build/ote-editor
PATH="$HOME/.local/share/emsdk:$HOME/.local/share/emsdk/upstream/emscripten:$PATH" EM_CONFIG="$HOME/.local/share/emsdk/.emscripten" \
  emcmake cmake -S . -B build-web -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build-web   # web player (Play), ~1 min
(cd editor/web && npm install && npm run build)                                        # the UI -> editor/web/dist
./build/ote-editor            # from the repo root; open http://127.0.0.1:8765/
```
The helper's defaults (`--web editor/web/dist`, `--player build-web/engine/player`, `--games games`) match this layout.
Without the web player build everything but Play works (Play explains how to build it).

```
cd editor/web
npm run dev            # Vite on :5173, proxies /api /assets /player to 127.0.0.1:8765 (a helper must be running)
npm run typecheck      # tsc, zero errors expected
npm test               # vitest unit tests (model incl. map grid, presets, art, the C++-pinned pickCell hash, store, autosave, uploads,
                       #   structures: effective numbers pinned to tests/test_structures.cpp, placing, cascades, the Convert flow)
OTE_EDITOR_BIN=../../build/ote-editor OTE_PLAYER_DIR=../../build-web/engine/player npm run e2e
npm run screenshots    # regenerate editor/web/screenshots (same env vars), then: python3 tools/compress_screenshots.py screenshots
```
Notes: Vite's asset folder is `app/` because the helper owns `/assets/`. e2e needs `npm run build` first, a built
`ote-editor` (`OTE_EDITOR_BIN`, default `../../build/ote-editor`) and, for the Play tests, the web player build
(`OTE_PLAYER_DIR`, default `../../build-web/engine/player`; the Play tests FAIL with the build command when it is missing).
e2e starts the real helper on a temp copy of games/ and editor/templates (never the repo's own) plus a second helper without
a player for the "player not built" test. One structures test also runs the native simulator on the saved game (`OTE_SIM_BIN`, default
`../../build/ote-sim`); the Convert tests write a project in the pre-structures format into the temp game folder (`tests/e2e/oldkit.ts`,
fixture `tests/unit/oldProject.ts`). Firefox runs the `@smoke` tests only; the phone tests (360 x 740, touch) are
Chromium only. Play tests read the player's picture from screenshots (WebGL), so they need the player's intro to settle.
The templates carry starter art now: tests that need an art-free game use `bareGame` / `seedArt` (`tests/e2e/artkit.ts`, which also
makes PNGs on the fly); the new art screenshots are `tests/e2e/screenshots-art.spec.ts` and the structures ones (53 to 72)
`tests/e2e/screenshots-structures.spec.ts` (generate into a temp folder with `SHOTS_DIR`, compress, then copy only the new or changed files, so
the older screenshots are not re-quantised).

## Build and test

```
cmake -S . -B build -G Ninja && cmake --build build
./build/ote_tests            # all unit tests
./build/ote_tests battle     # subset by name
```

Player backend (SDL3 default; SDL2 for old ports, same sources):

```
cmake -S . -B build -G Ninja && cmake --build build                       # SDL3
cmake -S . -B build-sdl2 -G Ninja -DOTE_PLATFORM=sdl2 && cmake --build build-sdl2
./build/engine/player/ote-player --game demo-srpg --font sans             # fonts: pixel pixel-ttf arcade sans serif mono
./build/engine/player/ote-player --headless --game demo-srpg --map bridge \
    --input "wait:14,confirm,wait:2" --screenshot /tmp/x.bmp               # --font-size N, --font-smooth 0|1, --assets DIR
```

Launcher (default when no `--game` is given): Home -> game page -> map select
-> battle. Options (font, text size, UI scale, visual style) are global with
optional per-game overrides and live in `settings.json` in the user data
folder (Linux: `~/.local/share/OpenTacticsEngine/player/`). Tests and
automation should pass `--settings /tmp/x.json`; `--headless` without it uses
no settings file at all. Flags `--font --font-size --font-smooth --style
--ui-scale` override the stored options for one run.

```
./build/engine/player/ote-player                                    # the launcher
./build/engine/player/ote-player --game demo-srpg --map bridge      # straight into a battle
./build/engine/player/ote-player --settings /tmp/s.json             # use another settings file
./build/engine/player/ote-player --headless --settings /tmp/s.json --input "down,down,down,confirm,wait:2" --screenshot /tmp/x.bmp
```

## Web build

Emscripten 7.0.0 (`~/.local/share/emsdk`; `source ~/.local/share/emsdk/emsdk_env.sh`
puts `emcmake` on PATH). SDL3 backend only (SDL2's Emscripten branch is untested).

```
emcmake cmake -S . -B build-web -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build-web
python3 -m http.server -d build-web/engine/player 8000     # any static server works
# http://localhost:8000/index.html  (standalone launcher)
# http://localhost:8000/index.html?args=--game+demo-srpg+--map+bridge+--autoplay+--frames+300   (debug args)
# embed.html?project=<id>&map=<id> needs the editor helper's API (docs/EDITOR_API.md)
```

`build-web/engine/player/` is self-contained: `ote-player.js/.wasm` (factory
`createOtePlayer`, main is not auto-run), `index.html`, `embed.html`,
`player.js/.css`, copies of `games/` + `assets/` and `games.manifest.json`.
Sources of the pages: `engine/platform/web/`. Details: docs/ARCHITECTURE.md "Web".
Size: wasm ~1.5 MB (490 KB gzip), js 163 KB (41 KB gzip); first frame ~0.1 s locally.

Browser checks (Playwright; Chromium + Firefox), screenshots land in `tools/webtest/out/`:

```
cd tools/webtest && npm install && npx playwright install chromium firefox   # once
node run.mjs                       # standalone, pointer/touch, resize, embed (mock API), perf: Chromium
node run.mjs --browser firefox     # smoke test in Firefox
node run.mjs --only embed --build /path/to/web/output
```

## Working agreements

- The owner makes design/naming/direction calls; technical calls are ours.
- Heavy model: architecture, contracts (headers), core rules. Lighter
  models: well-specified work packages against those contracts, verified
  by the delegating session before merging.
- Commit style: `feat(core): ...`, `test: ...`, `content: ...`, `docs: ...`.
- Licence: GPL-3.0 (owner decision 2026-11-14); a game's own data is its
  maker's. Pushing to `origin` is agreed; anything else public (releases,
  posts, other sites) still needs the owner's explicit yes.

## Kinds of view (2026-11-14)

Owner decisions and the contract: docs/VIEWS.md (also docs/PLAN.md item
3d). Build order: d1 2D isometric (DONE, on main), d2 the 3D drawing layer
(must be portable to the console ports), d3 3D isometric, d4 full 3D.

d1: core `engine/core/view/iso.*` + `StyleDef::view`; player
`battle/map_proj.*`, `iso_tiles.*`, `iso_draw.cpp`, `views.h`; isometric
starter art `tools/art/art_iso.py`; webtest suite `views`; editor: kind of
view per style (Game screen), Heights tool in the map editor (key H), Play
style choice, both real templates ship an Isometric style. Verified:
`ote_tests` 109 (SDL3 + SDL2, isometric screenshot pixel-identical between
them), API 250, editor unit 274, e2e 185 (+18 skipped screenshot specs),
webtest Chromium 92 / Firefox 25.

## The 3D drawing layer (2026-11-14)

Contract: docs/RENDER3D.md. `engine/gfx3d/` (library `ote_gfx3d`):
`render3d.h` (the interface), `math3d.h`, `soft3d.*` (software backend,
the reference), `shapes.*`, `model*.{h,cpp}` (glTF loader). Player:
`PlayerApp::renderer3d()` owns the backend, `show3d.*` puts the picture on
screen (`Renderer::updateTexture`), `model_view.*` uploads and draws a
model, `scene_test3d.*` is the test scene (`--test-3d`). Tests:
`tests/test_soft3d.cpp`, `tests/test_model.cpp`, webtest suite `render3d`.
Nothing in a game uses it yet: `viewSupported` (views.h) still offers only
top-down and isometric.

## 3D isometric view (2026-11-14)

Contract: docs/VIEWS.md "3D isometric". Core `engine/core/view/iso3d.*`
(`Iso3DGeometry`: projection, pick, hidden; tests/test_views3d.cpp). Player:
`MapProj` has a third mode (`iso3d()`, `setTurn`), `battle/map3d*.cpp`
(`Map3D`, owned by BattleScene, draws through the 3D layer), `sheets3d.*`
(art sheets as 3D textures), `batch3d.*` (per-frame meshes per material),
`Button::TurnLeft/TurnRight`, on-screen turn buttons
(`BattleScene::turnButtonBox`). Both demos have a "3D Isometric" style
(written by tools/art/make_starter_art.py: top-down tiles on block tops,
iso unit sprites, top-down structure pictures). The editor offers the kind.
Webtest suite `views` covers it.

## 3D models and animation (2026-11-14)

Contract: docs/GRAPHICS.md "Models", docs/RENDER3D.md "Models". Loader
`engine/gfx3d/model*.cpp` reads skins and animation clips; `poseModel` /
`skinPart` (model_pose.cpp) pose on the CPU. Art file: `UnitLook::model`
(`ModelLook`) for units and structures in styles of the 3D kinds. Player:
`battle/models3d.*` (one `ModelAsset` per file, one `ModelActor` per thing),
`battle/map3d_models.cpp` (animation from what BattleView does: sliding =
Move, lunging = Attack, fading = Die; facing). Demo models are generated by
`tools/art/make_unit_models.py` into `games/demo-srpg/models/`. Editor:
model upload endpoints and a picture-or-model choice on the Units and
Structures screens.

## Full 3D view (2026-11-14)

Contract: docs/VIEWS.md "Full 3D". Core `engine/core/view/free3d.*`
(`Free3DGeometry`: perspective camera circling a focus, project, pick,
hidden, shiftIntoView; tests/test_free3d.cpp). Player: `MapProj` has a
fourth mode (`free3d()`, `cam3()`) and hides which camera looks at the 3D
space (`project3`, `pixelsAt`, `artPixels`, ...), so `Map3D` draws both 3D
kinds; `battle/free_cam.*` (`FreeCam`: turn / tilt / zoom, following the
cursor). Platform: `Button::TiltUp/TiltDown/ZoomIn/ZoomOut`,
`InputEvent::CameraDrag`, mouse wheel, camera stick and triggers (SDL3 and
SDL2). Both demos have a "Full 3D" style (same art as 3D isometric). Editor:
"Full 3D" kind on the Game screen, models allowed in such styles.
Player option "3D graphics" (Automatic / OpenGL / Software): settings key
`draw3d`, docs/RENDER3D.md "Choosing".
3D at the display's real resolution (docs/RENDER3D.md "Resolution"): option
"3D resolution" (key `res3d`), `--3d-res`, headless `--3d-scale N` (screenshot
written at N times the size), `Renderer::displaySize()`, OpenGL 4x
multisampling. Graphics sheets take `"smooth": true` (3D views only: linear +
mipmaps; the editor's Art screen has the checkbox); models follow the glTF
sampler. Not verified by hand: a real window, the web build's canvas size.
Not verified by hand: gamepad stick/triggers (no gamepad in tests); mouse
wheel and right-drag are checked in the browser suite only.

## Items (2026-11-14)

Contract: docs/ITEMS.md. Core: `engine/core/data/loader_items.cpp`,
`engine/core/battle/items.cpp`, `unit_stats.h`, item actions / events /
queries on `Battle`, carried weapons in `combat.cpp`, AI healing rule
`engine/core/ai/ai_items.cpp`; saves carry inventories and storage; the
save hash mixes `data/items.json` only when the file exists (games
without items keep their old hash). Player: `battle/battle_items.cpp`
(state machine), `battle/item_text.*`, `ui/item_panel.*`,
`ui/trade_panel.*`; action menu entries Items / Trade / Storage / Shop.
Editor: Items screen (`editor/web/src/screens/items/`), Items card on
Rules, item fields on Weapons, Units, Structures and the map editor;
server reads/writes `data/items.json` (docs/EDITOR_API.md).
`demo-srpg` uses carried weapons, gold, a shop at the fort, storage next
to the leader and two enemy drops (its saves from before are refused).
All wording is placeholder until the owner approves it. Open owner
questions: should Trade / Storage / Shop stay open for several actions;
show Storage when empty; id renaming for items.

## Upgradable structures and research, rules core (2026-11-14)

docs/ECONOMY.md build order step 1, done in `engine/core` only (no player UI, no editor screens, no demo content:
steps 4 and 5 are open; both demos play exactly as before, checked: `ote-sim` bridge Blue 17 / Red 13, twin_rivers
Red 30, pinned hashes and save bytes unchanged). Where things are: data `data/loader_research.cpp`,
`loader_upgrades.cpp` (`Database::research`, `StructureDef::upgrades` / `research`); battle `battle/upgrades.cpp`,
`research.cpp`, bonuses in `unit_stats.h` (`unitStat(db, unit, stat, &sides)`; the formula host gets the sides
through `FormulaCtx::sides`), saves `save_research.*`, AI `ai/ai_economy.cpp`, campaign `CarriedSide::research`.
Tests: `tests/test_upgrades.cpp`, `test_research*.cpp`, `research_fixture.h`, `json_edit.h`.

What the player and the editor need (all on `Battle` / `Database`):
- Actions: `Action::upgrade(structureIndex, toKind)` (`UpgradeStructure`) and `Action::research(techIndex)`
  (`StartResearch`, for the active side); both free of any unit, no unit order is needed or consumed.
- Menus: `upgradeOptions(structure, out)` (for the active side's own structure; each entry: `to`, `def->cost`,
  `def->needs`, `ok`, `why`), `researchOptions(side, out)` (`tech`, `ok`, `why`; finished and under-way ones are not
  listed), `researching(side)` / `researchTurnsLeft(side)` (the panel line), `hasTechnology(side, tech)`,
  `canResearch(side)` (rule `research.needsStructure` off: the game has technologies; on: the side owns a `research` structure; "paused" otherwise),
  `buildOptions(cell, out, includeLocked)` + `unlockedBy(unitType, out)` for a greyed "needs Armor or Forge" line,
  `canBuild(cell, unitType, &why)` (the whole check). Technology data: `db->research[i]` (id, name, description,
  cost, turns, needs, unlocksUnits, bonusTypes, bonus per stat).
- Events (after the old ones): `StructureUpgraded` (a structure, b old kind, c new kind), `ResearchStarted` (a side,
  b technology, c turns; 0 = at once), `ResearchFinished` (a side, b technology). A payment arrives first as
  `ResourceChanged`. The state is already updated when the events are animated, so the structure is drawn in its new
  kind from the start of the animation (the player just ignores the three events today, `battle_view.cpp`).
- A stat shown to a player: use `Battle::stat` (includes accessories and research); `Unit::stats` is the unit's own value.
- Editor: new keys `upgrades` / `research` on structure kinds and the new file `data/research.json`. The helper
  validates through the real loader, which reads them now (messages name the file, the technology or structure and
  the entry, plain sentences, safe to show); the helper's project file list and the web model have not been taught
  about `data/research.json` or the two keys (nothing in the editor was touched).
- Campaign scenes did not get `researched` / `grantResearch` (scenes have no map sides).
Open for the owner: wording of every message (placeholder); whether a free upgrade should exist at all (the AI never
takes one); whether research under way should carry between chapters (today it is lost).

## Repair and supply, rules core (2026-11-14)

docs/ECONOMY.md build order step 2, done in `engine/core` only (no player UI, no editor screens, no demo content).
Both demos play exactly as before (`ote-sim` bridge Blue 17 / Red 13, twin_rivers Red 30, campaign finished 5; pinned
hashes and save bytes unchanged). Where things are: data `data/loader_supply.cpp` (`rules.supply`, `repairs`, `fuel`,
`fuelPerTurn`, `supplies`, `ammo`); battle `battle/repair.cpp`, `fuel.cpp`, `ammo.cpp`, `supply.cpp` (turn start order in
`turn.cpp`, reach limit in `movement.cpp`, weapon choice in `items.cpp` `weaponsOf` and `combat.cpp`), saves
`save_supply.*`, AI `ai/ai_supply.cpp`. Tests: `test_supply_loader.cpp`, `test_repair.cpp`, `test_fuel.cpp`,
`test_ammo.cpp`, `test_supply.cpp`, `test_supply_save.cpp`, `test_supply_ai.cpp`, `supply_fixture.h`.

What the player and the editor need (all on `Battle` / `Database`; the rules are in docs/ECONOMY.md):
- Action: `Action::supply(unit)` (`ActionType::Supply`): a supplier unit that has not acted, next to a unit of its side
  that lacks something; may move first; ends the unit's turn. Offer it in the unit's action menu when `canSupply(unit)`.
  `supplyTargetsFrom(unit, cell, out)` lists whom a supplier standing on `cell` would fill (for a preview).
- Unit state: `Unit::fuel` (-1 = none) and `Unit::ammo` (one entry per entry of the type's `weapons`, -1 = unlimited, empty =
  nothing tracked) or `fuel(unit)`, `maxFuel(unit)`, `ammo(unit, weapon)`, `maxAmmo(unit, weapon)` (-1 = none / unlimited);
  `needsSupply(unit)`. Show them in the info panel only when not -1. `weaponsOf` already leaves an empty weapon out;
  to grey it out, walk `typeOf(u).weapons` and ask `ammo()`.
- Repair: `repairPreview(unit, plan)` (what the next turn start would do for the unit where it stands, from the side's
  funds now; false = nothing) and `repairPreviewAt(unit, cell, plan)`; `RepairPlan { heal, cost per resource index }`.
- Events (after the old ones): `UnitRepaired` (a unit, b HP restored, c HP now, data cost per resource index, empty =
  free), `UnitResupplied` (a unit). The payment of a repair arrives first as `ResourceChanged`. The player today draws
  nothing for them and just sets the displayed HP when a repair event ends (`battle_view.cpp`); `ote-sim` prints both.
- Editor: new keys `rules.supply` (`repair.amount` / `repair.costShare` / `structures`), structures `repairs`, units `fuel` /
  `fuelPerTurn` / `supplies`, weapons `ammo`; the helper validates through the real loader (messages name the file and the
  entry); the helper's model and the web model were not taught the new keys.
Decisions the owner can change: the first turn of the battle already burns `fuelPerTurn` (a plane that starts with 20 and
burns 4 has 16 at its first move); `fuelPerTurn` equal to or above `fuel` is refused; repair prices are rounded up per
resource; resupply by structures needs no `repairs` match and goes to every unit of the owner; suppliers fill only their
own side (not allies of another side); fuel and ammunition are not carried between chapters (full at each start); a unit
running dry is destroyed by `fuelPerTurn` only (others just cannot move); the AI's thresholds (40% HP, a quarter of the
tank, out of every usable weapon) are constants in `ai_supply.cpp`; wording of every loader message is placeholder.

## Economy in the player: upgrades, research, repair, supply (2026-11-14)

Build order step 4 of docs/ECONOMY.md, the part for upgrades, research, repair and supply (fog drawing is separate).
Files: `battle/battle_economy.cpp` (structure menu, Upgrade list, research panel, Supply highlight, `[info]` line for a
hovered unit with fuel / ammunition, upgrade flash), `battle/economy_text.*` (ALL wording of this part: `EconomyWords`
plus the text builders, like `item_text` for items), `battle/battle_view_economy.cpp` (events), small edits in
`battle_scene.*`, `battle_menus.cpp`, `battle_input.cpp`, `panel.cpp`, `ui/menu.h` (`selectedId`), `ui/item_panel.*`
(`setDetailLines`, `setMaxWidth`) and four one-line hooks in `battle_view.cpp`. Test: `python3 tests/economy_player_test.py
./build/engine/player/ote-player .` (24 checks on a generated game; `--shots DIR` keeps its screenshots; `--make-games
DIR` only writes the game). It reads the `[info]` lines the player logs: `structure menu:`, `build menu:`, `upgrade menu:`,
`research panel:` (every row with price / turns / `[greyed]`), `info:` (hovered unit's fuel and ammunition), `upgrade:`,
`research started:`, `research finished:`, `repair:`, `resupply:`.
Behaviour: a structure that offers only one thing (a plain factory) opens that screen at once, as before; two or more open
the structure menu (Build / Upgrade / Research); Cancel in a sub-screen goes back to it. A unit that can still act and stands
on the structure takes the Confirm (select it first, or let it act). Greyed entries show their reason where the price is
(a greyed row cannot be selected, so a hint line would never show it); the research panel shows the reason in its detail
lines. Build menu shows locked unit types greyed with "needs <tech>". Supply is in the action menu; while it is the
highlighted entry the units it would fill are tinted (the move-range overlay, so all four kinds of view draw it). Info panel:
a "Fuel" row (red at a quarter or less), fixed weapons show "3/6 r1" and a weapon out of ammunition shows "EMPTY" in red
(it is not in `weaponsOf`, so the panel walks the type's weapons); the side's research shows under its resources ("Armor   1",
"paused" when no research structure). The hint line says "Low fuel: move limited to N" when the tank is the limit of a move.
Events: banner "Blue Army: Armor researched" (+ what it gives), started banner, "+N" heal number for a repair, "Resupplied"
float, white flash on the squares of an upgraded structure; 1.3 s for the viewed side, 0.5 s / 0.1 s for others.
Not checked: a real window, gamepad, touch (hit testing is the shared Menu / ItemPanel / RowList code). The old comments in
`battle_view.cpp` ("no player UI yet") on the event list are stale; left alone to keep the merge with the fog branch clean.

## Fog of war, rules core (2026-11-14)

docs/ECONOMY.md build order step 3, done in `engine/core` only (no player drawing, no editor screens, no demo content:
steps 4 and 5 are open; the player keeps showing everything and just skips the new `MoveBlocked` event). Without fog
nothing changed: `ote-sim` bridge Blue 17 / Red 13, twin_rivers Red 30, campaign finished 5, the pinned demo hash and save
fnv unchanged, and the verbose event output of all three runs is byte for byte the old one. Where things are: data
`data/loader_fog.cpp` (`vision`, `visionBonus`, `hides`, the map's `fog`); battle `battle/sight.cpp` (per-team sight, queries,
last seen owners), `fog_rules.cpp` (ambush: `pathStop`, `noticeAlong`), `event_visibility.cpp` (the masks; the table of what
"saw" means per event is at its top and in docs/ECONOMY.md), `save_fog.*`; AI `ai/ai_knowledge.cpp` (`knownEnemy`,
`knownOwner`, `knownCapturable`, `enemyGoals`; every AI read of an enemy unit, owner or capture state goes through it);
`Grid::within(cell, radius, out)`; story condition `sees` (`Condition::Kind::Sees`). Tests: `tests/test_fog_*.cpp`,
`fog_fixture.h` (`FogWorld`, with the extra unit type "blind", vision 0).

What the player and the editor need (all on `Battle` / `Database`):
- Queries, all answering "everything visible / the true owner" without fog so no special case is needed: `fogOn()`,
  `visibleTo(team, cell)`, `unitVisibleTo(team, unit)`, `structureVisibleTo(team, structure)`, `seenOwner(team, structure)`,
  side forms `visibleToSide`, `unitVisibleToSide`, `seenOwnerForSide`. Teams are `sides[i].team`. A viewer (the human, or in
  hot-seat the side about to play) draws: squares with `!visibleTo` dimmed; units only when `unitVisibleTo`; structures
  with the owner `seenOwner` (not `structures[i].owner`, which is the truth); capture points only when
  `structureVisibleTo`; a "show everything" toggle for the maker simply uses the viewer-less path (ignore the queries).
  Everything that lists units or owners (info panel, unit lists, forecasts, objectives) must be filtered the same way:
  `Battle` itself never hides anything.
- The state of the Battle is the truth and is already updated when events are animated. Every `Event` has `seen` (bit `t` =
  team `t` saw it; `kSeenByAll` without fog and for public events) and `UnitMoved` has `seenSteps` (per team two indices into
  `data`: first and last square where that team saw the mover, -1 -1 = never). A view for team `v` animates an event only when
  `(e.seen >> v) & 1`; an event whose bit is clear still has its effect on the view's own copy of the state, applied at once
  without animation (a hidden enemy simply "is" somewhere else when it next becomes visible). A partially seen move: the
  unit appears at `data[seenSteps[2v]]` and walks to `data[seenSteps[2v+1]]`; if it ends out of sight it vanishes there. A
  walk into view of a unit that was hidden: the same. `MoveBlocked` (a = unit, b = the unit in the way, c = square it stopped on,
  d = the enemy's square) follows the `UnitMoved` of the squares walked (none when it never left): show a short "stopped" cue
  and the revealed enemy.
- Hot-seat ("Pass the device to <side>"): between two human sides' turns cover the screen before drawing anything for the
  next side; draw with `v` = the next side's team. AI turns of other teams are shown only through their `seen` bits.
- The play menus need no new query: `attackOptions`, `forecast`, `reachable`, `talkOptions` already leave out unseen units
  (a `forecast` / `Attack` on an unseen unit is refused with "that attack is not possible"). The unit's turn ends after an
  ambush (`moved` and `done` set), so an action menu after `MoveBlocked` offers nothing for it.
- Editor: new keys: map `"fog": true`; units `"vision"` (0 to 20, default 2); terrain and structures `"visionBonus"` (0 to
  10) and `"hides"`; the story condition `{ "sees": <filter>, "team": ... }`. The helper validates through the real loader
  (messages name the file, the entry and the key); the helper's model and the web model were not taught the new keys and
  `storyChecks.ts` does not know `sees` (nothing in `editor/` was touched). Fog map: at most 32 sides (the loader says so).
- AI behaviour under fog (documented in docs/ECONOMY.md): plays on its team's sight and memory only; with no enemy in sight it
  heads for the structures it last saw in enemy hands, else the unseen square farthest from its own units. Forced-fog sim
  (`ote-sim <game> <map> --force-fog`): bridge 30 runs Blue 16 / Red 14 (avg 4.3 rounds), north_road Blue 10 / Red 20 (6.0),
  twin_rivers Blue 30 (9.0 rounds, avg 7 captures, 13 builds); no stalls anywhere.
Decisions the owner can change: vision is a plain per-type number (not a stat, research does not touch it); terrain and structure
bonuses add up; `hides` shows a unit to adjacent units only (an adjacent unit with vision 0 still notices it); there is no
line-of-sight blocking by terrain or height yet; the ambush ends the unit's turn even when it stopped on its own square and
even when the enemy was visible by then (the plan is fixed at the order); a defender still counterattacks an attacker its team
cannot see; the old owner of a captured structure always learns of it; a defeated side's structures become known neutral to
everyone; `Focus` is a public event (the player decides what to do with a focus on a hidden square); the AI treats every
unit type as a possible target when none is in sight. Not verified: nothing in a real window (no drawing exists); the player
was only compiled and run headless (campaign test, 3D backends test) with the new event skipped.

## Fog of war, player (2026-11-14)

docs/ECONOMY.md build order step 4 (fog part): the player draws what the viewer's team sees; the rules core was not touched
(vision for the "as drawn" dimming is worked out in the player from public data). Details and behaviour in docs/ECONOMY.md
"Fog of war > Player". Where things are: `battle/battle_view_fog.cpp` (viewer, unseen events, partial walks, ambush, shown
squares), `battle/battle_fog.cpp` (viewer choice, AI step loop, hot-seat cover), `ui/pass_device.*` (the cover), wording in
`battle/fog_text.h`; dimming in `battle_draw.cpp` (top-down), `iso_draw.cpp`, `map3d_things.cpp` / `map3d_models.cpp` (3D: a
blended black quad per unseen square in the highlight batch, tinted structures); helpers `fogDim`, `structureDimmed`,
`shownOwner` in `battle_draw.h`. `--show-all` / embed `showall=1` draws without fog (for the editor's Play panel; the editor
page has to pass the parameter, nothing in `editor/` was touched). Without fog every screenshot is byte-identical to a build of
`main` (checked on both demos in all four kinds of view). New log lines (the tests read them): `fog: viewer ...`, `fog: cover
up/down ...`, `ambush: ...`, `fog: <unit> seen on squares ...`, `turn: <side>, round N t=...`, `ai: <side> acts ... t=...`.
Test: `tests/fog_player_test.py` (57 checks; top-down and 3D isometric by pixels against the same state with `--show-all`, the
hardware 3D backend when it exists). Not verified: a real window, WebGL, gamepad, touch (the cover takes Confirm or any tap, the
rest is the shared input code). Known gaps: the kind a structure was upgraded to is not hidden; a unit sprite taller than a tile
can overhang a dimmed square in the top-down view; the "vanish when a walk ends unseen" path has no test of its own (the walk
into view has); everything the AI does is silent for the viewer, so the only sign of it is what appears afterwards.

## Research in the battle menu and the ambush rule (2026-11-14)

Two owner decisions, built end to end (docs/ECONOMY.md, docs/GAME_FORMAT.md).
- Research needs no building by default: game.json `rules.research.needsStructure` (default false). Core: `Battle::canResearch`
  (`research.cpp`), loader `data/loader_options.cpp` (also reads `rules.fog.ambush`), fields `RulesConfig::researchNeedsStructure`
  / `ambush`. Games without the key keep their save fingerprint but research no longer needs a building. Player: **Research**
  in the battle menu (`battle_menus.cpp`, after Objective, human side, games with technologies), the structure menu lists
  only Build / Upgrade; Cancel in the panel returns to the battle menu. Editor: Rules "Research" group, the Structures
  checkbox and the Research screen's hint only with the rule on. `games/demo-economy`: HQ `research: true` removed (no effect).
- Ambush: game.json `rules.fog.ambush` `"endsTurn"` (default) or `"canAct"` (the unit is `moved`, not `done`). Core
  `actions.cpp` `applyMove`; AI picks again after a blocked move (`ai/ai.cpp`); player `battle_input.cpp` (`ambushed_`: no undo,
  Cancel just closes the menu), words in `fog_text.h`. Editor: Rules "Fog of war" group. A unit with vision 0 does not see the
  neighbour that stopped it, so it cannot attack it (it can still wait / capture).
- Tests: `tests/test_research*.cpp`, `test_fog_ambush.cpp`, `economy_player_test.py` (also a needsStructure copy of the game),
  `demo_economy_player_test.py`, `fog_player_test.py` (a `canAct` copy), `editor_api_economy_checks.py`, editor `optionRules.test.ts`
  and `economy.spec.ts`. Webtest: demo-srpg has no technologies, so its battle menu is unchanged (checked byte for byte with
  a screenshot against the build before); the web player build in `build-web` is older and does not have this change.

## Story events in the editor (2026-11-14)

Build order step 3 of docs/EVENTS.md (inside one map; the Campaign screen is step 5). Model: `editor/web/src/model/story*.ts`
(`storyWalk.ts` = the ONE walk over every id an event names: rename, clean-up, "used by" lines and checks all use it;
`storyChecks.ts` mirrors the loader; `storyUnits.ts` = unit / side deletes as one undo step), `portraits.ts`. UI:
`screens/maps/editor/story/` (Events and Objectives tabs, one file per form group), unit "Story name" in `panels/UnitProps.tsx`,
switches / variables on Rules (`screens/rules/StoryListsCards.tsx`), Portraits card on Art (`screens/art/Portraits*.tsx`).
Picking a square / area on the map: `MapViewState.picking` + `story/pickStroke.ts`; open event areas are drawn by `drawStory.ts`.
The helper passes the new keys through unchanged (docs/EDITOR_API.md "Story data"). Squares are shown 1-based, stored 0-based.
Tests: `tests/unit/story.test.ts`, `tests/e2e/story*.spec.ts`, `tests/editor_api_events_checks.py`. All wording is placeholder.

## Economy in the editor (2026-11-14)

Build order step 5 (editor part) of docs/ECONOMY.md: Research screen, structure upgrades / research / repairs, rules.supply on Rules,
fuel / supplies on Units, ammunition on Weapons, story condition `researched` and command `grantResearch`. Notes: docs/EDITOR_UI.md
"Economy options". Helper: `data/research.json` as `research` (empty list removes the file). Tests: `tests/editor_api_economy_checks.py`,
`editor/web/tests/unit/economy.test.ts`, `tests/e2e/economy.spec.ts`. `demo-economy` content is still open.

## Fog: hidden upgrades and sight bonus for some unit types (2026-11-14)

Two follow-ups to fog, built end to end (rules core, player, editor, docs, `demo-economy`). (1) A team remembers the **last
seen kind** of every structure next to the last seen owner (`Battle::seenKind` / `seenKindForSide` / `seenDef`,
`numbersAtSeen`; refreshed in `sight.cpp` and `learnOwner` under the same conditions); the player draws and describes
structures through `shownStructure` / `shownKind` (`battle_draw.h`), the AI through `ai::knownDef`. Saves: `"seenKinds"` is
written only for a fog battle where some team believes a kind other than the map's, and is optional on read (absent = map
kinds), so fog saves of games that never upgraded stay byte-identical. (2) `visionBonus` may be `{ "amount": N, "units":
[ids or tags] }` (`TerrainDef` / `StructureDef::visionUnits`, empty = every unit; `Battle::visionAt` is the one rule, the
player's walking estimate uses it too). Editor: `GroundSightCard`, `model/fog.ts` (`bonusFor`, `setVisionUnits`),
`researchRefs.ts` cleans the lists when a unit type is deleted. `demo-economy` mountain: infantry and mech (only foot units
can enter it anyway, so the sim numbers did not move; its old saves are refused as the data changed). Tests:
`tests/test_fog_kinds.cpp`, `fog_player_test.py` (upgrade scenario, game `fogup`), `fogLimited.test.ts` / `fogLimited.spec.ts`.
Not seen in a real window. Terrain / structure props of a hidden structure still apply for the AI's defence estimate of a
square it could move to (`terrainDefense` reads the truth); it stays on the truth for rules as designed.

## Fog of war in the editor (2026-11-14)

Map settings "Fog of war" (disabled above 32 sides) and the canvas aid "Show what a side sees" (`model/fogSight.ts`, drawn by `drawFog.ts`, `data-fog` on the canvas); Units "Sight" (default 2, never written), Terrain / Structures "Extra sight" and "Hides" (`screens/fog/`); story condition `sees` (`model/story*.ts`, not in scenes); checks `model/fogChecks.ts` (banner via `EconomyChecks`, map ones in `mapWarnings`); Play panel "Show everything" adds `showall=1` (`ote.playShowAll.<project>`). The helper already passed every fog key through (tests/editor_api_fog_checks.py). Tests: `tests/unit/fog.test.ts`, `tests/e2e/fog.spec.ts`. Aid ignores line of sight (the engine has none yet).

## demo-economy content (2026-11-14)

Build order step 5, demo part, of docs/ECONOMY.md (all numbers placeholders; the list is in that file). `games/demo-economy` now has
city -> town -> metropolis, research at the HQ (Armor Doctrine / Reinforced Plating / Urbanism), repair + fuel + ammunition + an APC,
and the fog map `misty_ford`. Art for the new kinds comes from `tools/art` (re-run `make_starter_art.py`, never hand-edit).
**Plain-game pins moved**: the old demo is frozen as `tests/fixtures/plain-economy` (`savetest::Rig::loadPlain`, `loadPlainEconomy()`
in the editor unit fixtures); save bytes `7bd8c1c0aa74081e` and data hash `1e44dae676a88a38` are identical to before. A demo-economy save made
earlier is refused (the rules changed). `ote-sim` (deterministic, all seeds equal): twin_rivers Red 30 / Blue 0 (10 rounds, 6 captures, 14 builds;
before: Red 30, 15 rounds, 9 captures, 26 builds), misty_ford Blue 30 / Red 0 (11 rounds, 5 captures, 13 builds), no stalls; demo-srpg unchanged
(bridge Blue 17 / Red 13). The one-sidedness is move order (the first side's lone tank walks into the other's gun), not data; starting funds
and removing the tanks did not flip it. Test: `tests/demo_economy_player_test.py`. Not run here: the editor's e2e and the browser webtest
(need the web build); they reference demo-economy only in `art-play.spec.ts` (opens it, expects "no problems" and unchanged graphics.json) and
`editing.spec.ts` ("Wars Demo" link).

## Sound, step 1: the audio foundation (2026-11-14)

docs/SOUND.md build order step 1, as built ("Step 1 as built" there has the refinements). No game content, no
`audio.json`, nothing wired to events yet: that is step 2. Tests: `ote_tests` `audio_*` (decoders incl. damaged and
truncated files, mixer, service; decoder and mixer tests also clean under ASan + UBSan), `tests/audio_player_test.py` (flags, options, null device,
`--audio-render` measurements), `tools/webtest/suites/audio.mjs`. **Real sound output was never heard here** (no device is opened
on this machine by rule): run `ote-player --audio-test` on a desktop, and open the web build, press a key, and move the
Music / Sound volume sliders in Options (each step ticks).

API for step 2 (`#include "sound/audio_service.h"`; scenes call `host.audio()`, the App owns it):
```
SoundId id = audio.loadSound(path, err);          // read + decode once, cached by path; 0 + message on damage / limits
audio.playSound(id, volume01);                    // logs "sound <path> vol=<percent> level=<n>"
MusicParams p; p.fadeSeconds = 0.5f; p.loop = true; p.loopStartSeconds = 2.5; p.volume = 0.8f; p.pauseCurrent = false;
audio.playMusicFile(path, p, err);                // false + message when it cannot be opened; level 0 opens nothing
audio.stopMusic(fadeSeconds);
audio.setVolumes(musicLevel, soundLevel);         // 0..10; PlayerApp::applyLook does this from the options
audio.addSound(name, pcmBuffer) / audio.playMusic(name, musicSource, p)   // in-memory (tests, synthesized)
audio.update(dt)                                  // PlayerApp::update calls it once per frame
audio.mixer()                                     // musicPlaying(), activeSounds() for tests
```
Jingles: `p.pauseCurrent = true; p.loop = false;` pauses the current track and resumes it when the jingle ends. "Same track
asked again keeps playing" is the music director's job (step 2): the service always restarts. The clock for the log is the
sum of the `dt`s, so a headless run is exact. Off / Silent / Live modes: Silent (headless) decodes effects and opens music
files (so a broken file is a test failure) but mixes nothing; Off (`--no-audio`, no device) reads nothing, so `loadSound`
returns an id for any path there: a loader that wants to report missing files must not run in Off mode (or use
`audio.mode()`). Platform side: `AudioOut` / `openAudioOut` / `createNullAudioOut` / `audioWaitsForUserInput` in
`platform.h`, `App::onStop()` (called by both backends before SDL shuts down; releases the device).
Build the offline checks with `--audio-render x.wav --seconds N [--music-volume N] [--audio-file F]`.

Known limits: effects and music are linearly resampled (fine for game sound, not hi-fi); one music track is read through a
2048-frame block; the web build uses SDL3's ScriptProcessorNode (Chromium logs one deprecation warning, harmless); the
sound rate is fixed at 44.1 kHz (device rates are converted by SDL), a "lower rate on weak targets" setting is not built;
stepping a volume slider from "Default" goes to Off first (the unset choice is element 0 of every option).

## Sound, step 2: the player plays sound (2026-11-14)

docs/SOUND.md build order step 2, as built ("Step 2 as built" there has the full tables). No shipped game has an
`audio.json` yet at that point (step 3 below gave the demos and templates theirs). **Real sound was never heard here** (no device is opened on this machine by rule): everything is
checked through `--audio-log` and offline.

What it is: `data/audio.json` (+ `"sounds"` on weapons / units / items, `"music"` / `"playerTurnMusic"` / `"enemyTurnMusic"` on maps, a
scene `{ "music", "commands" }`) loaded strictly by the player (`engine/player/sound/audio_data.*`); the core only lets the keys
pass and gained the story commands `playMusic` / `stopMusic` / `playSound` (events `MusicChanged`, `SoundRequested`, the saved story
music key). Sounds come from `GameAudio` (`game_audio.*`, owned by `PlayerApp`, reached as `host.gameAudio()`): the cue lookup
(`sound_bank.*`), the music director (`music_director.*`), `uiCue()` (`cue_sink.h`) for widgets, `BattleView::soundStarted /
soundImpact / soundStep` (`battle/battle_view_sound.cpp`) for events, `Scene::audioContext()` for music. Tests: `ote_tests`
(775: `test_sound_core.cpp` core + hash / save neutrality, `test_audio_data.cpp` loader, lookup, director), `tests/sound_player_test.py`
(48, ctest `sound_player`; it builds a game in a temp folder with `tests/sound_game.py` and the generator's audio).

**Step 3 (starter audio into the demos and templates) needs:**
- `python3 tools/audio/make_starter_audio.py --out DIR --ogg` writes `DIR/audio/*` and `DIR/audio.json.example`; copy `audio/` into
  the game and the example to `data/audio.json`. The example is accepted as it is. The shape the loader accepts (strictly):
  `{ "music": { id: { "file", "loopStart"?, "volume"? (0..100), "loop"? } }, "sounds": { id: { "file" | "files": [1..16], "volume"? } },
  "use": { "titleMusic", "mapMusic", "playerTurnMusic", "enemyTurnMusic", "sceneMusic", "victory", "defeat", "levelUp" (music ids,
  "" = none; the three jingles need `"loop": false`), "sounds": { cue: sound id } } }`; cues: cursor confirm cancel error step hit miss
  critical heal defeated capture build itemUse gold dialogue turnStart upgrade research repair supply ambush select. Ids are letters,
  digits, `_`, `-`; files are `.ogg` / `.wav` inside the game folder.
- Optional per-game flavour: `"sounds"` blocks (weapons: hit miss critical; units: step hit miss critical defeated select heal; items: use),
  map `"music"` keys, a scene `{ "music": ..., "commands": [...] }`, the story commands. The generator also makes `sword_hit`,
  `bow_shot`, `magic_cast`, `cannon_shot`, `hooves`, `tank_move`, `potion`, `ambush`, `levelup_tick` for them.
- The web build copies `games/` as it is, so audio files grow the web download: check sizes. `games/demo-srpg/CREDITS.md` /
  the CC0 set need their credits file. The demos' existing player tests assume silence (`tests/sound_player_test.py` "demo ..." checks
  must be updated, and a real listening pass done: `ote-player --game demo-srpg --map bridge` on a desktop with the sound on).
- Every `games/` content change that touches `weapons.json` / `units.json` / `items.json` / maps with sound keys keeps the save hash
  (the keys are cosmetic to it); a `playMusic` / `playSound` command in a map does change it (the command exists), the track does not.

**Step 4 (editor) needs:** the Sound screen reads / writes exactly the audio.json shape above (keep key order; `files` for several
variants; `""` for "none" in `use`); pickers for the override keys listed in the tables of docs/SOUND.md "Step 2 as built"; the scene
object form for the campaign editor (`{ "music", "commands" }`; a plain list stays valid and is what an editor writes when there is
no music); the three story commands (`playMusic` string + optional `fade` seconds, `stopMusic: true` + `fade`, `playSound` string). The
editor's "problems" check runs the real core loader, which accepts all of these without checking the ids (the ids are checked
by the player's `loadAudioFile` / `loadAudioOverrides` / `checkStoryAudio`, `engine/player/sound/audio_data.cpp`: the editor server would
need that code or a port of it for "unknown sound id" problems). Play in the editor runs the web player, which checks them.

The launcher's home list and global options play the player's own menu sounds (`assets/launcher`, owner's choice
2026-11-16: built-in set, no home music; docs/SOUND.md "The launcher's own sounds"). Open points: the same sound at most once per frame and a minimum gap for `step` (0.1 s), `dialogue`
(0.045 s) and `cursor` (0.03 s) are code constants (`game_audio.cpp`); the dialogue blip fires per typed letter, which may want a
slower rate once heard.

## Sound, step 3: starter audio in the demos and templates (2026-11-14)

docs/SOUND.md "Step 3 as built" has the tables. `python3 tools/audio/make_starter_audio.py .` writes the retro set (`audio/` + `data/audio.json`)
into `games/demo-economy` and the three templates (what each gets: `tools/audio/targets.py`; OGG re-encoded only when the SHA-256 of
its source WAV in `tools/audio/starter_audio.manifest.json` changed, so a rerun is a no-op on any machine); `games/demo-srpg` has the
CC0 set (`tools/audio/make_cc0_set.py --src <folder of the downloads>`; sources in `games/demo-srpg/CREDITS.md`, originals not
committed). The overrides (weapons / units / items) and the bridge map's `playSound alarm` / `playMusic boss` / `stopMusic` are hand-written
in the games' data. Repository grows by about 6 MB (4 retro copies of ~0.95 MB, the CC0 set 2.1 MB); the web package by the same
(it copies `games/`). The demo-srpg rules hash changed (a new story command): older demo-srpg saves of the bridge are refused. **Nothing
was heard**: the loops of the CC0 music were picked by comparing the two ends numerically and the sounds by name, so listen once
(`ote-player --game demo-srpg --campaign`, `--game demo-economy --map twin_rivers`, a project made from each template) and tell what
should change. For the editor step: the web editor's campaign screen does not read the scene object form (`{ "music", "commands" }`) or
the commands `playMusic` / `stopMusic` / `playSound` in a scene (its unit test on the demo campaign fails on them), so `demo-srpg`'s
`campaign.json` is left as it was and scene music comes from `sceneMusic`.

## Sound, step 4: the editor (2026-11-14)

docs/SOUND.md "Step 4 as built". Helper: `audio` section (`data/audio.json`, `false` removes it), `/audio` file endpoints, the player's
loader in `/problems` (`editor/server/audio_check.cpp`; the ote-editor target now links `ote_audio` and the player's `sound/` sources).
Web: Sound screen, pickers, story commands, scene music. Tests: `tests/editor_api_sound_checks.py`, `tests/unit/audio.test.ts`,
`tests/e2e/sound*.spec.ts` (the main one also runs the real `ote-player --headless` on the result; `OTE_PLAYER_BIN` overrides its path).
Open: Play panel sound toggle (needs one line in `embed.html`), no starter-set installer on the empty screen (step 3's files).

## Next concrete step

Owner's order (docs/PLAN.md item 3): a structures, b animated joining
terrain and c saving are done; of d (kinds of view) 2D isometric, the 3D
layer (software backend and the OpenGL ES 3 / WebGL 2 / OpenGL 3.3 one,
docs/RENDER3D.md "OpenGL backend": own GL context, picture read back, so
the 2D renderer is untouched; `--3d auto|software|hardware`), glTF models
with animation and 3D isometric are done. The full 3D view's map part
is done too (close-up battle scenes later, by owner decision). Nothing in game data may name a
graphics API or carry shader code (console ports must be able to
implement the interface). Items are done, and so is drawing 3D at the display's
resolution with smooth textures (docs/RENDER3D.md "Resolution"; option
"3D resolution", key `res3d`; `--3d-res`, `--3d-scale N` for headless
screenshots at N times the size) plus `demo-srpg`'s "Detailed 3D" style
from CC0 packs (`games/demo-srpg/CREDITS.md`,
`tools/art/make_detailed_style.py`). Not checked in a real window.
Story events, dialogue and the campaign are built (docs/EVENTS.md) and
`demo-srpg` has a story and the two-chapter campaign "The Ford War"
(`data/campaign.json`, `maps/north_road.json`; `ote-sim games/demo-srpg
--campaign`). The editor's Campaign screen is built (`editor/web/src/screens/campaign/`, model
`model/campaign*.ts`; scenes reuse the story command list through a made-up "scene map"; the store's `edit` runs
`reconcileCampaign` so deletes of switches / sides / maps / story names clean the scenes in the same undo step;
`"campaign": false` in the PUT removes the file); Play on the Campaign screen runs the story (`campaign=1` on the embed
page). Economy options (docs/PLAN.md 3g, docs/ECONOMY.md) are done end to end: rules core, player menus and fog
drawing, editor screens (Research, upgrades, repair and supply, fog settings, sight aid, "Show everything" in Play),
and `demo-economy` content (town upgrades, three technologies, supply, fog map `misty_ford`; the frozen plain copy
for "untouched game" pins is `tests/fixtures/plain-economy/`). Nothing of it was checked in a real window.
Sound and music (docs/PLAN.md 3h, docs/SOUND.md) are done end to end: engine, player, starter audio in the demos and
templates, the editor's Sound screen (see "Sound, step 1" to "step 4" above). Nobody has listened to any of it yet.
Shipped audio file names must be lower case (the editor's rule, `validAudioName`): the cues `itemUse` / `turnStart`
use the sounds `item_use` / `turn_start` (`tools/audio/targets.py` STEMS; `tests/starter_audio_test.py` checks names).
Hexagonal grids (PLAN 3i, contract docs/HEX.md with the owner's four decisions) are done for the top-down view:
rules core, player (`engine/core/view/hex_geo.*`, `engine/player/battle/hex_draw.*`), starter art (`tools/art/make_hex_art.py`,
`preview_hex.py`), templates `hex-tactics-pointy` / `hex-tactics-flat`, `games/demo-hex` (maps fortress_ford, foggy_hills),
editor (`editor/web/src/model/hex.ts`, `gridSpec.ts`, `mapGeo.ts`, canvas `drawHex.ts`), Play panel Sound checkbox.
Open: hexagons in the other three views; bridges on zigzag rivers fork oddly (join mask); demo maps one-sided; terrain
palette chips without art are square; names of the templates and demo are placeholders; odd tileSize can leave a seam.
The editor tutorial (PLAN 3j, docs/TUTORIAL.md) is built: framework (`editor/web/src/learn/`, server `editor/server/learn.*`,
`GET/PUT /api/learn`, flag `--learn-file`; tests always pass a temp one) and eight lessons (first-game, structures, items,
story, economy, sound, art, hexagons). Open: owner reviews lesson wording; phone layout (lesson sheet vs the map editor's
tabs; ring now stays while folded); `point: 'a|b'` alternatives and the signals `pictureChooser` / `soundPreviewed` are
documented in EDITOR_UI.md only, not in TUTORIAL.md's "How to write a lesson".
Next (PLAN 3k): the strategy-RPG feature set (skills, shove, summoning, supports, rescue, promotion): design questions to
the owner first. A private rebuild of the fan game "Vision Quest" is the owner's idea: only with placeholder art/sound,
in a git-ignored game folder, never pushed, until its authors agree (owner is contacting them).
Close-up battle scenes: low priority.
Small leftovers: README; heights editing in the map editor; arrow-key
navigation inside the editor's option groups; "+N Funds" toast overlap;
night style does not darken unit sprites; editor paint stroke with art
~45 ms on 64 x 64; starter-art limits (water corners that touch only
diagonally are square, a sand strip shows between squares of a bridge
longer than one square); no pixel-level check of animated water on the
editor canvas; the editor's map canvas and picture previews do not draw
isometric (Play does); isometric starter art has no joining pieces and
the buildings are the top-down pictures on plinths; a capture was never clicked through in the embedded player
(the embed page has no scripted input; `ote-sim` covers captures).
