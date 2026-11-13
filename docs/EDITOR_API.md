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
- Request bodies are limited to 16 MB.
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
Copies the template into the games folder and sets the game's name.
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
  "problems": [ { "file": "data/units.json", "message": "unit \"x\": …" } ] }
```
`maps` is keyed by the map id from `game.maps`. `scripts` holds the text
of every file listed in `game.scripts`. A file that is missing or is not
valid JSON does not fail the request: its section is `null` and a problem
describes why. `revision` identifies the on-disk state (a hash of the
project's files).

### PUT /api/projects/:id
Body: `{ "baseRevision": "…", "game": …, "terrain": …, "weapons": …,
"units": …, "maps": { … }, "scripts": { … } }` (same shapes as GET; a
section that is absent or `null` is left untouched on disk).

- If `baseRevision` is not the current on-disk revision → `409
  { "error": …, "revision": "<current>" }` and nothing is written (the
  project changed on disk, e.g. synced from another machine).
- Otherwise writes `game.json`, `data/terrain.json`, `data/weapons.json`,
  `data/units.json`, each map to the `file` named in `game.maps`, and each
  script. Map files that were referenced by the previous `game.maps` but
  are no longer referenced are deleted (only inside the project's `maps/`
  folder). JSON is written pretty-printed with 2-space indentation and
  keys in the order received.
- Invalid game data is still saved (work in progress must not be lost);
  the response reports it: `200 { "revision": "<new>", "problems": [ … ] }`.

### GET /api/projects/:id/problems
`{ "problems": [ { "file": "maps/m1.json", "message": "…" } ] }`. Runs the
real loader on the saved files: `loadGame`, then every map with `loadMap`,
then compiles the Lua formulas and starts a battle on each map. `file` is
relative to the project folder ("" when unknown). The loader stops at the
first error per file group, so fixing one problem can reveal the next.

### GET /api/projects/:id/files
`{ "files": [ { "path": "game.json", "size": 1234 } ] }` — every regular
file in the project, for loading the project into the embedded player.

### GET /api/projects/:id/file/<path>
Raw bytes of one project file (`application/octet-stream`).

### GET /api/fonts
`{ "fonts": [ { "id": "pixel", "name": "Pixel (built-in)", "size": 8, "smooth": false }, … ] }`
(the built-in bitmap font plus `assets/fonts/fonts.json`).

### Static files
- `/assets/<path>` → the assets folder (fonts for the embedded player).
- `/player/<path>` → the WebAssembly build of the player.
- everything else → the web UI folder; unknown paths fall back to
  `index.html` (single-page app).
