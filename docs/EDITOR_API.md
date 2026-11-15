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
```

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
- Nothing is ever deleted outside a project folder; version 1 has no
  "delete project".

All API responses are JSON (`Content-Type: application/json`). Errors use
an HTTP status and `{ "error": "human readable message" }`.

## Endpoints

### GET /api/info
`{ "engine": "OpenTacticsEngine", "apiVersion": 1, "formatVersion": 1 }`

### GET /api/projects
Every sub-folder of the games folder that contains a `game.json`.
```json
{ "projects": [
  { "id": "demo-srpg", "name": "Emblem Demo", "description": "...",
    "author": "", "maps": 1, "ok": true, "error": "" } ] }
```
`ok` is false (with `error`) when the project does not load; it is still
listed and can still be opened for fixing.

### GET /api/templates
Starting points for new projects (sub-folders of the templates folder,
each a complete valid game). `{ "templates": [ { "id", "name", "description" } ] }`

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
  "problems": [ { "file": "data/units.json", "message": "unit \"x\": …" } ] }
```
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
like `structures`. `revision` identifies the on-disk state (a hash of the
project's files).

### PUT /api/projects/:id
Body: `{ "baseRevision": "…", "game": …, "terrain": …, "weapons": …,
"units": …, "maps": { … }, "scripts": { … }, "graphics": { … },
"structures": [ … ], "items": [ … ] }` (same shapes as GET; a section that is absent or `null`
is left untouched on disk; `graphics` must be an object and `structures` and `items` a
list, anything else is `400`; `structures` is written as `{ "structures": [ … ] }`,
`items` as `{ "items": [ … ] }`, and an empty list is valid). Art image files are
not part of this call: they have their own endpoints below.

- If `baseRevision` is not the current on-disk revision → `409
  { "error": …, "revision": "<current>" }` and nothing is written (the
  project changed on disk, e.g. synced from another machine).
- Otherwise writes `game.json`, `data/terrain.json`, `data/weapons.json`,
  `data/units.json`, `data/graphics.json`, `data/structures.json`, `data/items.json`, each map to the `file` named in `game.maps`, and each
  script. Map files that were referenced by the previous `game.maps` but
  are no longer referenced are deleted (only inside the project's `maps/`
  folder). JSON is written pretty-printed with 2-space indentation and
  keys in the order received.
- Invalid game data is still saved (work in progress must not be lost);
  the response reports it: `200 { "revision": "<new>", "problems": [ … ] }`.

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
Raw bytes of one project file: `image/png` for `.png` files, otherwise
`application/octet-stream` (so `file/models/knight.glb` reads a model).

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

### GET /api/projects/:id/models
Every `.glb` and `.gltf` in the project's `models/` folder (same name rule as
art, plain files only), sorted by name, with the entries of the SAVED
`data/graphics.json` that name each file. No `models/` folder gives an empty list.
```json
{ "models": [ { "file": "models/knight.glb", "size": 1234,
                "usedBy": [ { "style": "solid", "kind": "units", "id": "knight" } ] } ] }
```
`kind` is `"units"` or `"structures"`.

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
  names the file (`usedBy` as in the list). Nothing is deleted.
- `404` no such file; `400` bad name.

### GET /api/fonts
`{ "fonts": [ { "id": "pixel", "name": "Pixel (built-in)", "size": 8, "smooth": false }, … ] }`
(the built-in bitmap font plus `assets/fonts/fonts.json`).

### Static files
- `/assets/<path>` → the assets folder (fonts for the embedded player).
- `/player/<path>` → the WebAssembly build of the player.
- everything else → the web UI folder; unknown paths fall back to
  `index.html` (single-page app).
