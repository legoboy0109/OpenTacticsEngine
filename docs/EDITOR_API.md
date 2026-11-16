# Editor helper API (version 1)

`ote-editor` is a small local program that serves the browser editor and
reads/writes game projects on disk. The browser UI talks to it only
through this HTTP API. It links `ote_core`, so "is this project valid?" is
always answered by the real engine loader.

```
ote-editor [--games DIR=games] [--assets DIR=assets]
           [--templates DIR=editor/templates] [--web DIR=editor/web/dist]
           [--player DIR=build-web/engine/player]
           [--host 127.0.0.1] [--port 8765] [--allow-host NAME]...
           [--learn-file FILE=<user data>/OpenTacticsEngine/editor/learn.json]
```

`--learn-file` is where the tutorial's remembered state lives (`GET`/`PUT /api/learn`). The default is the platform's user data
folder, never a game folder and never the repository: `~/.local/share` (or `$XDG_DATA_HOME`) on Linux, `~/Library/Application Support`
on macOS, `%APPDATA%` on Windows, plus `OpenTacticsEngine/editor/learn.json`. `--learn-file ""` (an empty value) keeps the state in
memory only: it is gone when the helper stops and no file is made anywhere. The file and its folders are created by the first `PUT`,
never by starting the helper or by a `GET`. **Tests must always pass `--learn-file` with a temporary path** (the test fixtures do), so
the real user data folder is never touched.

Safety rules (it writes files, so these are not optional):
- Binds to 127.0.0.1 unless `--host` says otherwise.
- Requests whose `Host` header is not localhost / 127.0.0.1 / [::1] / the
  `--host` value / an `--allow-host` value are rejected with 403 (blocks
  DNS-rebinding attacks from web pages).
- Project ids match `[a-z0-9][a-z0-9-]{0,39}`. Every file path is resolved
  inside the project (or assets) folder; `..`, absolute paths and symlink
  escapes are rejected with 400.
- Request bodies are limited to 16 MB (art uploads: 8 MB, see below).
- Files are written atomically (temp file in the same folder + rename).
- Nothing is ever deleted outside a project folder. A whole project is
  deleted only by `DELETE /api/projects/:id` (below), never as a side effect.

All API responses are JSON (`Content-Type: application/json`). Errors use
an HTTP status and `{ "error": "human readable message" }`.

## Endpoints

### GET /api/info
`{ "engine": "OpenTacticsEngine", "apiVersion": 1, "formatVersion": 1, "hexViews": ["top-down", "isometric", "isometric-3d", "3d"] }`

`hexViews` lists the kinds of view (`"view"` of a style in game.json) a hexagon game may use with the engine this helper is built
from (docs/HEX.md "Editor"): the loader accepts exactly these for hexagon games. The editor offers only these on the Game screen, so
its list follows the build; a helper that does not send the field means top-down only.

### GET /api/projects
Every sub-folder of the games folder that contains a `game.json`.
```json
{ "projects": [
  { "id": "demo-srpg", "name": "Story Tactics Demo", "description": "...",
    "author": "", "maps": 1, "ok": true, "error": "" } ] }
```
`ok` is false (with `error`) when the project does not load; it is still
listed and can still be opened for fixing.

### GET /api/templates
Starting points for new projects (sub-folders of the templates folder,
each a complete valid game). `{ "templates": [ { "id", "name", "description", "grid" } ] }`.
`grid` is the template's `grid` from its `game.json` (docs/HEX.md): `{ "type": "square" }` (also when the
file has none) or `{ "type": "hex", "layout": "pointy" | "flat" }`. The editor shows it on the new-game cards.

### POST /api/projects
Body `{ "id": "my-game", "name": "My Game", "template": "strategy-rpg" }`.
Copies the template (including its `art/` folder, byte for byte) into the games folder and sets the game's name.
`201 { "id": "my-game" }`; `409` if the id exists; `400` for a bad id or
unknown template.

### GET /api/projects/:id
The whole project as one document. Comments in hand-written files are
dropped (the document is strict JSON).
```json
{ "id": "demo-srpg",
  "revision": "9f3c2a…",
  "game": { …contents of game.json… },
  "terrain": [ …the "terrain" list of data/terrain.json… ],
  "weapons": [ …the "weapons" list… ],
  "units": [ …the "units" list… ],
  "maps": { "bridge": { …contents of that map file… } },
  "scripts": { "scripts/formulas.lua": "…text…" },
  "graphics": { …contents of data/graphics.json… },
  "structures": [ …the "structures" list of data/structures.json… ],
  "items": [ …the "items" list of data/items.json… ],
  "research": [ …the "technologies" list of data/research.json… ],
  "campaign": { …contents of data/campaign.json… },
  "audio": { …contents of data/audio.json… },
  "characters": [ …the "characters" list of data/characters.json… ],
  "skills": [ …the "skills" list of data/skills.json… ],
  "abilities": [ …the "abilities" list of data/abilities.json… ],
  "supports": { …contents of data/supports.json… },
  "problems": [ { "file": "data/units.json", "message": "unit \"x\": …" } ] }
```
`game.grid` (docs/HEX.md) is passed through untouched: missing or `{ "type": "square" }` is a square game, `{ "type": "hex",
"layout": "pointy" | "flat" }` a hexagon game, whose maps carry `"grid": { "type": "hex", ... }` and whose structures may have
`"rings": 0 | 1`. The helper adds no hexagon rules of its own: everything about hexagon games (a map of the wrong type, `width` /
`height` on a hexagon structure, `rings` on a square game, an isometric `tileHeight`, a ring that does not fit the map, overlapping
structures) is reported by the engine loader in `problems`, in the loader's own words. Saving a hexagon game keeps `grid`,
`rings` and every unknown key exactly as received (an unknown key inside `grid` is saved and reported, never dropped).
`maps` is keyed by the map id from `game.maps`. `scripts` holds the text
of every file listed in `game.scripts`. A file that is missing or is not
valid JSON does not fail the request: its section is `null` and a problem
describes why. The exception is `graphics`: a project without
`data/graphics.json` (no art) has `"graphics": null` and no problem (a file
that exists but is not a JSON object is `null` plus a problem on
`data/graphics.json`). `structures` works the same way: no
`data/structures.json` gives `"structures": null` and no problem; a file that
is not valid JSON or has no `structures` list gives `null` plus a problem on
`data/structures.json`. `items` (`data/items.json`, docs/ITEMS.md) behaves exactly
like `structures`. `research` (`data/research.json`, docs/ECONOMY.md "Research") behaves the same way, except that the
file calls its list `technologies`; a broken file is `null` plus a problem on `data/research.json` and blocks only the
Research screen. `campaign` (`data/campaign.json`, docs/EVENTS.md "Campaign") is the file's one object; no file gives
`"campaign": null` and no problem, a file that is not a JSON object gives `null` plus a problem on
`data/campaign.json`. `audio` (`data/audio.json`, docs/SOUND.md) is the same: the file's one object, `null` when there is no file
(a silent game), and a file that is not a JSON object is `null` plus a problem on `data/audio.json`; a broken file blocks only
the Sound screen. `revision` identifies the on-disk state (a hash of the
project's files).

**Strategy-RPG sections** (docs/SRPG.md "Editor"; `editor/server/srpg_files.cpp`). `characters`, `skills` and `abilities` are
lists exactly like `items` (each file is `{ "<key>": [ … ] }`; `null` when the file is absent, no problem; a file that is
unreadable, not an object, or without its list is `null` plus a problem on that file). `supports` is the file's one object, like
`campaign` (`null` when absent; a file that is not a JSON object is `null` plus a problem on `data/supports.json`). Entries
are passed through untouched: unknown keys and key order are kept. The engine loader checks the contents; its refusals come in
`problems` with the file name.

### DELETE /api/projects/:id?confirm=<id>

Deletes the game's folder for good (there is no trash). `confirm` must repeat
the id. `200 { "deleted": "<id>" }`.

- `400` when `confirm` is missing or differs; `404` unknown game.
- `403` for the demos that come with the engine (`demo-srpg`, `demo-economy`,
  `demo-hex`); `GET /api/projects` marks them `"protected": true`.
- `409` when `games/<id>` is itself a link to another folder. Links inside
  the folder are removed, never followed.
- The player's saves for that game (in the user data folder) are not touched.

### PUT /api/projects/:id
Body: `{ "baseRevision": "…", "game": …, "terrain": …, "weapons": …,
"units": …, "maps": { … }, "scripts": { … }, "graphics": { … },
"structures": [ … ], "items": [ … ], "research": [ … ], "campaign": { … }, "audio": { … } }` (same shapes as GET; a section that is absent or `null`
is left untouched on disk; `graphics` must be an object and `structures` and `items` a
list, anything else is `400`; `structures` is written as `{ "structures": [ … ] }`,
`items` as `{ "items": [ … ] }`, and an empty list is valid). `research` is a list too, written as
`{ "technologies": [ … ] }`; **an empty `research` list removes `data/research.json`** instead (the loader refuses an
empty technology list, so writing one would break the game; the project then reads back `"research": null`). `campaign` is an object, written as it is; **`false` removes
`data/campaign.json`** (the game goes back to single battles) and any other value is `400`. `audio` works like `campaign`: an
object is written as it is (key order and unknown keys kept), **`false` removes `data/audio.json`** ("remove all sound": the
`audio/` files stay) and any other value is `400`. Nothing is validated on write; `/problems` runs the player's sound loader.
**`characters`, `skills`, `abilities`** (lists; anything else is `400`) are written as `{ "<key>": [ … ] }`, but **an empty
list removes the file** (a game without the file and a game with an empty list are the same game, and opening a screen must not
change the data hash; removing a file that is not there is fine). **`supports`** is an object, written as it is; **`false`
removes `data/supports.json`**; any other value is `400`. Absent or `null` leaves each file alone. A map may not use one of
these four file names as its `file`.
Art, model and audio files are not part of this call: they have their own endpoints below.

- If `baseRevision` is not the current on-disk revision → `409
  { "error": …, "revision": "<current>" }` and nothing is written (the
  project changed on disk, e.g. synced from another machine).
- Otherwise writes `game.json`, `data/terrain.json`, `data/weapons.json`,
  `data/units.json`, `data/graphics.json`, `data/structures.json`, `data/items.json`, `data/research.json`, `data/campaign.json`, `data/audio.json`, `data/characters.json`, `data/skills.json`, `data/abilities.json`, `data/supports.json` (the last four as described above), each map to the `file` named in `game.maps`, and each
  script. Map files that were referenced by the previous `game.maps` but
  are no longer referenced are deleted (only inside the project's `maps/`
  folder). JSON is written pretty-printed with 2-space indentation and
  keys in the order received.
- Invalid game data is still saved (work in progress must not be lost);
  the response reports it: `200 { "revision": "<new>", "problems": [ … ] }`.

### Economy data (docs/ECONOMY.md)

Upgrades, research, repair, fuel, ammunition and supply need nothing new on the wire: structures `upgrades` /
`research` / `repairs`, units `fuel` / `fuelPerTurn` / `supplies`, weapons `ammo` and game.json `rules.supply` are part
of the sections above and pass through with key order and unknown keys kept (tests/editor_api_economy_checks.py proves
that file by file). Only `data/research.json` is a file of its own (see GET and PUT above); a map file may not use that name.

### Story data (docs/EVENTS.md)

The helper passes story data through like every other key: it is part of the sections above, nothing new is
validated on write (invalid data is still saved), and key order and keys it does not know are kept.

- A map file may have `events` (list), `objectives` (object) and, on each unit in `units`, an `id`. They come back in
  `maps.<id>` exactly as written.
- `game.json` may have `switches` and `variables` (lists of `{ "id", "name" }`); they are part of `game`.
- `data/graphics.json` may have a top-level `portraits` object, `{ "<id>": { "sheet": "<sheet id>", "cell": [x, y] } }`
  (dialogue portraits); it is part of `graphics`.

`GET .../problems` runs the real loader over all of it, so a story the game would refuse is reported with the map
file and the event, e.g. `maps/bridge.json: map "bridge": event "ambush": "do": command 2 (addUnit): unknown unit type "dragon"`.

### GET /api/projects/:id/problems
`{ "problems": [ { "file": "maps/m1.json", "message": "…" } ] }`. Runs the
real loader on the saved files: `loadGame`, then every map with `loadMap`,
(`loadGame` includes `data/graphics.json`, so art problems such as an unknown
sheet, a missing/oversized image or a sheet's `"smooth"` that is not `true` /
`false` are reported with `file: "data/graphics.json"`; `smooth` is an
optional key of a sheet, written and returned like any other), then compiles the Lua formulas and starts a battle on each map. `file` is
relative to the project folder ("" when unknown); any loader message that starts with a
project file's path (e.g. `data/structures.json`) is reported with that `file`. The loader stops at the
first error per file group, so fixing one problem can reveal the next.

### GET /api/projects/:id/files
`{ "files": [ { "path": "game.json", "size": 1234 } ] }` — every regular
file in the project, for loading the project into the embedded player.

### GET /api/projects/:id/file/<path>
Raw bytes of one project file: `image/png` for `.png` files, `audio/wav` for `.wav` and `audio/ogg` for `.ogg` files
(the Sound screen plays them from here), otherwise `application/octet-stream` (so `file/models/knight.glb` reads a model).
A `Range` request is answered by the HTTP library as usual; nothing depends on it.

### GET /api/projects/:id/art
Every `.png` in the project's `art/` folder, sorted by name (plain files
only; links are ignored). A project without an `art/` folder gives an empty
list.
```json
{ "art": [ { "file": "art/tiles.png", "width": 96, "height": 48, "size": 1234 } ] }
```
A file that is not a readable PNG is still listed, with `"width": 0`,
`"height": 0` and an extra `"error"` text.

### PUT /api/projects/:id/art/<name>.png
Uploads an image. The body is the raw PNG file (any `Content-Type`; the
bytes are checked, not the header). `<name>` must match
`[a-z0-9][a-z0-9_-]{0,39}\.png` (lower case, at most 40 characters before
`.png`); the name is checked before anything touches the disk. The file is
written atomically to `art/<name>.png` (the folder is created if needed).
`?overwrite=1` is needed to replace an existing file.

- `201 { "file": "art/tiles.png", "width": 96, "height": 48, "size": 1234,
  "revision": "<new>" }`
- `400` bad name; empty body; not a PNG (signature + header are checked);
  larger than 2048 x 2048 pixels; `art/` or the file is a symlink or leaves
  the project. `404` unknown project.
- `409` the file already exists and `?overwrite=1` was not given.
- `413` body larger than 8 MB (a body over 16 MB is refused earlier by the
  global limit, also `413`).

Every error is `{ "error": "…" }` with a plain-language message and leaves
nothing behind (no partial or temp file).

### DELETE /api/projects/:id/art/<name>.png
- `200 { "revision": "<new>" }` after deleting the file.
- `409 { "error": "…", "usedBy": ["main", …] }` while a sheet in the SAVED
  `data/graphics.json` uses `art/<name>.png`; `usedBy` lists the sheet ids.
  Nothing is deleted.
- `404` no such file (or not a plain file); `400` bad name.

The project `revision` covers every file in the project including art and
`data/graphics.json`, so uploads and deletes change it. A `PUT
/api/projects/:id` that carries an older `baseRevision` gets `409`.

### Sound data and audio files (docs/SOUND.md)

Sound needs nothing new on the wire beyond `audio` above: a weapon / unit / item `sounds` block, a map's `music` /
`playerTurnMusic` / `enemyTurnMusic`, a campaign scene written as `{ "music": …, "commands": [ … ] }` and the story commands
`playMusic` / `stopMusic` / `playSound` are keys of the sections they sit in and pass through with key order and unknown keys
kept (tests/editor_api_sound_checks.py proves that file by file, byte for byte). `GET .../problems` runs the **player's own**
sound loader after the core one (`engine/player/sound/audio_data.cpp`, in silent mode: files are really decoded, nothing is
played): a wrong `audio.json` (unknown key, bad id, a looping jingle, a volume outside 0..100, a `files` list that is empty or
longer than 16, a path that is not a `.wav` / `.ogg` inside the game), an unknown sound or track id in an override, a map, a
scene or a story command, a story sound command in a game without `audio.json`, a sound file that is damaged or an effect longer
than 10 seconds are reported with `file: "data/audio.json"` (or the file that names it). A sound file that is only missing is a
warning in the player (the game starts without that sound) and is **not** reported here.

**"Remove all audio"** is written as `"audio": false` (the file goes, the files in `audio/` stay). Deleting the last track and the
last sound leaves `{ "music": {}, "sounds": {} }`, which is valid.

### GET /api/projects/:id/audio
Every `.wav` and `.ogg` in the project's `audio/` folder (same name rule as art, plain files only), sorted by name. No `audio/`
folder gives an empty list. The kind is told by the file's first bytes, never by its name.
```json
{ "audio": [ { "file": "audio/hit.wav", "size": 22094, "format": "wav", "rate": 22050, "channels": 1, "seconds": 0.5,
               "usedBy": [ { "kind": "sounds", "id": "hit" } ] } ] }
```
`usedBy` = the tracks (`"music"`) and sounds (`"sounds"`) of the SAVED `data/audio.json` that name the file. A WAV gives rate,
channels and length from its header; an Ogg Vorbis file gives rate and channels from its first page and the length from the last
page's granule position (when found). A file whose header cannot be read is still listed, with an `"error"` text and no length.

### PUT /api/projects/:id/audio/<name>
Uploads a sound. The body is the raw file. `<name>` must match `[a-z0-9][a-z0-9_-]{0,39}\.(wav|ogg)`; checked before anything
touches the disk. The bytes are sniffed: a WAV starts with `RIFF` (and `WAVE` at byte 8), an Ogg file with `OggS`, and the name's
extension must say the same. Written atomically to `audio/<name>` (folder created if needed); `?overwrite=1` replaces a file.

- `201 { "file", "size", "format", "rate", "channels", "seconds", "revision" }` (the facts as in the list; an unreadable header
  gives `"error"` instead of rate, channels and length, and the file is still stored).
- `400` bad name; empty body; not a sound file; a WAV named `.ogg` or the other way round; `audio/` or the file is a link.
  `404` unknown project. `409` the file exists and `?overwrite=1` was not given. `413` body larger than 8 MB (music and effects
  alike; a body over 16 MB is refused earlier by the global limit, also `413`).

The engine's limits (an effect of at most 10 s, 8000..48000 Hz, mono or stereo, 8 / 16-bit PCM) are not checked here, because the
same file may be music; the loader reports them in `/problems` where the file is used as an effect.

### DELETE /api/projects/:id/audio/<name>
- `200 { "revision": "<new>" }` after deleting the file.
- `409 { "error": …, "usedBy": [ { "kind", "id" } ] }` while the SAVED `data/audio.json` uses the file. Nothing is deleted.
- `404` no such file; `400` bad name.

### GET /api/projects/:id/models
Every `.glb` and `.gltf` in the project's `models/` folder (same name rule as
art, plain files only), sorted by name, with the entries of the SAVED
`data/graphics.json` that name each file. No `models/` folder gives an empty list.
```json
{ "models": [ { "file": "models/knight.glb", "size": 1234,
                "usedBy": [ { "style": "solid", "kind": "units", "id": "knight" } ] } ] }
```
`kind` is `"units"`, `"structures"` or `"terrain"` (a terrain's standing part names the file in its `stand`, as `model` or in
`variants`, docs/GRAPHICS.md "Standing terrain").

### PUT /api/projects/:id/models/<name>.glb
Uploads a model. The body is the raw `.glb` file. Only `.glb` can be uploaded
(a `.gltf` with side files cannot). `<name>` follows the art rule
(`[a-z0-9][a-z0-9_-]{0,39}\.glb`). `?overwrite=1` is needed to replace a file.
The body must start with the glTF binary header: `glTF` and version 2.

- `201 { "file": "models/knight.glb", "size": 1234, "revision": "<new>" }`
- `400` bad name (a `.gltf` name says only `.glb` can be uploaded); empty
  body; not a glTF binary (`"this is not a .glb model file (it does not start
  with \"glTF\"); ..."`); a version other than 2; `models/` or the file is a
  link. `404` unknown project. `409` file exists without `?overwrite=1`.
- `413` body larger than 8 MB.

The file's contents beyond the header are not checked here; the player reports
a model it cannot read when a battle starts.

### DELETE /api/projects/:id/models/<name>.glb
- `200 { "revision": "<new>" }`.
- `409 { "error": "…", "usedBy": [ … ] }` while the SAVED `data/graphics.json`
  names the file in a unit, a structure or a terrain's standing part (`usedBy` as in the list). Nothing is deleted.
- `404` no such file; `400` bad name.

### GET /api/learn, PUT /api/learn
The editor tutorial's remembered state (docs/TUTORIAL.md "Remembered state"): ONE JSON object, kept by the helper so it follows the
maker to another browser or device. The helper stores it as is; the web editor owns its shape (`editor/web/src/learn/state.ts`:
`version`, `offerAnswered`, and per lesson `state` / `step` / `skipped` / `signals` / `game`).
- `GET` -> `200` the stored object. A missing, empty, unreadable, damaged (not JSON, not an object) or oversized file is **not an
  error**: the answer is `200 {}` ("no state"; the damaged file is only mentioned in the helper's log and replaced by the next `PUT`).
- `PUT` with a JSON object body (at most 64 KB) -> `200` the stored object. The file is written atomically (temp file in the same folder +
  rename), compact. `400` when the body is empty, not JSON or not an object; `413` over 64 KB; `500` when the file cannot be written
  (nothing changes). With `--learn-file ""` the object is kept in memory.
- Tests: `tests/editor_api_learn_checks.py`.

### GET /api/fonts
`{ "fonts": [ { "id": "pixel", "name": "Pixel (built-in)", "size": 8, "smooth": false }, … ] }`
(the built-in bitmap font plus `assets/fonts/fonts.json`).

### Static files
- `/assets/<path>` → the assets folder (fonts for the embedded player).
- `/player/<path>` → the WebAssembly build of the player.
- everything else → the web UI folder; unknown paths fall back to
  `index.html` (single-page app).
