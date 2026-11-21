# ote-mcp: an MCP server for building games

`ote-mcp` lets an AI assistant build and edit OpenTacticsEngine games through the editor's own rules instead of hand-editing
JSON. It speaks the **Model Context Protocol** (MCP), an open standard, over standard input and output, so it works with any MCP
client and any model; nothing in it is specific to one vendor.

What it gives the assistant:

- the **schemas** (what settings exist, their types, ranges, allowed words, which ids a reference may point at) and the engine's
  **loader as the judge** of validity: every change is checked against the schema first and the engine's project check runs after it;
- tools to look around (`get_overview`, `describe`, `get`, `search`), to change things (`set`, `add_entry`, `rename_id`,
  `set_tiles` ...), to make games and maps (`create_game`, `new_map`), to check (`validate`) and to look and measure
  (`render_map`, `simulate`);
- **direct saving to the game files**, with a **dry run** on every writing tool that shows the exact diff and the problems the
  result would have, without writing anything.

It is a small console program: no Qt, no network port, no extra libraries. Everything it needs (the schemas and these docs) is
inside the program.

## Building

`ote-mcp` is built wherever `ote-editor` is (option `OTE_BUILD_EDITOR`, on by default):

```
cmake -S . -B build
cmake --build build --target ote-mcp
```

It is found at `build/ote-mcp`. For `render_map` and `simulate` it needs the player and the simulator; they are looked for next to
`ote-mcp`, and in the build tree layout (`build/engine/player/ote-player`, `build/ote-sim`). `create_game` needs the game
templates (`editor/templates`), found next to the program or one or two folders up (`build/ote-mcp` finds `../editor/templates`).
Each can be given explicitly (below). The Windows branch of the process code is written but has never been compiled or run.

## Command line

```
ote-mcp [--root DIR]... [--read-only] [--player PATH] [--sim PATH] [--assets DIR] [--templates DIR] [--schema-dir DIR]
```

| option | meaning |
|---|---|
| `--root DIR` | A folder that holds game folders. Repeatable. Default: the current folder. **Only files below the roots are ever touched.** |
| `--read-only` | No tool that writes anything is offered or accepted. |
| `--player PATH`, `--sim PATH` | `ote-player` (for `render_map`) and `ote-sim` (for `simulate`). Also `OTE_PLAYER`, `OTE_SIM`. |
| `--assets DIR` | The `assets` folder the player uses for `render_map` (also `OTE_ASSETS`). |
| `--templates DIR` | The game templates for `create_game` (also `OTE_TEMPLATES_DIR`). |
| `--schema-dir DIR` | Read the editor schemas from this folder instead of the copy built in (also `OTE_SCHEMA_DIR`). |

Standard output carries protocol messages and nothing else; all logging goes to standard error. Closing standard input ends the
program.

### Adding it to an MCP client

Any client that can start a local (stdio) MCP server takes a command and arguments. The generic entry:

```json
{
  "command": "ote-mcp",
  "args": ["--root", "/path/to/your/games"]
}
```

Where that entry goes depends on the client. As one example, Claude Desktop reads a file `claude_desktop_config.json` (macOS:
`~/Library/Application Support/Claude/`, Windows: `%APPDATA%\Claude\`, Linux: `~/.config/Claude/`) with this shape (use the full
path of the program, and on Windows double the backslashes):

```json
{
  "mcpServers": {
    "ote": {
      "command": "/path/to/ote-mcp",
      "args": ["--root", "/path/to/your/games"]
    }
  }
}
```

Other clients use the same two fields (a command and its arguments) in their own settings; check their documentation for the
file. Restart the client after changing its settings. The server also offers prompts (`build_a_game`, `review_game`) and
resources (below) for clients that show them.

## Protocol revisions

The server answers the revisions of the MCP specification that exist today (looked up on modelcontextprotocol.io):

- **2026-07-28** (the current one): stateless; every request carries `io.modelcontextprotocol/protocolVersion` and
  `io.modelcontextprotocol/clientCapabilities` in `params._meta`, results carry `resultType`, `server/discover` tells a client what
  the server supports. A request with an unsupported version gets error `-32022` listing the supported versions.
- **2025-11-25, 2025-06-18, 2025-03-26, 2024-11-05**: the handshake revisions (`initialize`, then `notifications/initialized`).
  The server answers with the client's version when it knows it, else with `2025-11-25`. `structuredContent` and tool titles are
  only sent from `2025-06-18` on. JSON-RPC batches (allowed up to `2025-03-26`) are answered too.

Methods: `initialize`, `ping`, `server/discover`, `tools/list`, `tools/call`, `resources/list`, `resources/templates/list`,
`resources/read`, `prompts/list`, `prompts/get`, `logging/setLevel` (accepted; log lines go to standard error), and the
notifications (ignored: every call is answered at once). Messages are one JSON object per line. A line over 4 MiB is refused with
error `-32600` and the server carries on. Protocol errors use the standard JSON-RPC codes (`-32700` parse error, `-32600` invalid
request, `-32601` unknown method, `-32602` invalid params or unknown tool). A tool that fails is not a protocol error: it returns a
result with `isError: true` and a text that says what to change and what to call next.

## Safety model

- **Roots.** Games are only found and touched below the `--root` folders. Every path is resolved (symlinks and `..` included) and
  refused if it leaves the game folder; a game folder that is a link leading out of its root is not a game. A path given as `..`,
  absolute, or through a link that points outside is an error. A root that was not given is simply not there.
- **No deleting.** No tool deletes a game folder. The only deletions are of keys and list entries inside JSON files you ask to
  change, and of nothing else. `remove_entry` refuses to remove an entry other places still refer to.
- **Atomic writes.** Each file is written to a temporary file and renamed; when a call changes several files and one write
  fails, the ones already written are put back.
- **Conflict check.** The server remembers what it last showed you of each file. A write to a file that was changed on disk since
  (for example in an editor) is refused, with advice to read it again, unless the call says `force=true`.
- **Dry run.** Every writing tool takes `dryRun`. It writes nothing; the answer has the per-file diff and the problems the game
  would have. (The check runs on a temporary copy of the game that is removed again.) The real run then produces the same diff.
- **Read-only mode.** With `--read-only` the writing tools are not even listed, and calling one is an error.
- **Schema check.** A write that the schema refuses (unknown key in a described section, wrong type, number out of range, a word
  that is not allowed, a reference to an id that does not exist, a missing required key) is refused with the reason and the
  allowed values. Parts the schema does not describe (the raw tile grid and the like) need `allowUnknown=true`.
- **Comments.** The engine reads `//` comments in JSON files, but saving cannot keep them. The first write to a file that has
  comments says so (also in a dry run). Files are saved as plain 2-space JSON; the report notes it when that changes the spacing
  outside your edit. Keys the schema does not know, and numbers, are kept.
- **Silence and no network.** The player started by `render_map` is always run hidden (`--headless --no-audio`) with temporary
  settings and data folders, never the user's own. The server opens no network port.

## Tools

All tools take `game` (a folder name from `list_games`; optional when only one game exists). Writing tools also take `dryRun` and
`force`. Results are JSON, compact, and tell you the next call where one is obvious. Lists are paged with `limit` / `offset`: when a
result has `nextOffset`, call again with it. **Paths** in `get`, `set` and friends: keys joined by dots, list entries as `[index]`
or `[id]`: `units[archer].stats.hp`. In a table file (`data/units.json`, `data/weapons.json` ...) the table's own key may be left out:
`[archer].stats.hp`. `[-1]` is the last entry; `[key=value]` picks an entry by another key; `["odd.key"]` quotes a key.

### Looking around

**`list_games`**: the games under the roots (id, name, grid kind, number of maps).

**`get_overview`**: one call that orients you: name, grid, maps, how many entries each data file has, ids of stats / resources /
move types, which optional rule systems are on, units and events placed on maps, and the project problems.

```
> get_overview {"game": "orchard"}
< {"game":"orchard","name":"Orchard Skirmish","grid":{"type":"square"},"maps":[{"id":"start",...}],
   "data":{"terrain":2,"units":1,"weapons":1},"ids":{"stats":["hp","mov"],"moveTypes":["foot"]},
   "rules":{"on":["counterAttacks"],"off":["hitRoll","economy","fog","actionPoints",...]},"problems":0}
```

**`list_files`**: the files of a game (size, kind, schema name), paged; `dir` narrows it.

**`describe`**: what a file or place accepts: types, required keys, defaults, ranges, enum words, references with the **current**
valid ids, help text, optional blocks and whether they are enabled, and for lists of several shapes (story commands, conditions,
skill effects) the kinds with one line of help each. No arguments: the list of schemas. `depth` (1-3) unfolds more levels.

```
> describe {"game": "orchard", "file": "data/units.json", "path": "[soldier].moveType"}
< {"path":"units[soldier].moveType","node":{"type":"ref","help":"How the unit moves over terrain.","required":true,
   "reference":{"from":["game.json#moveTypes"],"count":1,"validIds":["foot"]}},"current":"foot"}
```

**`get`**: reads a value, row or section, paged. `mode: "summary"` gives one line per list entry (the schema's summaries, for
example the kind and text of each story command) or a preview per key, to survey big lists before reading entries. Each entry
carries its `path`.

**`search`**: `mode: "usages"` (default) finds an exact id in every value and key of every JSON file and in Lua scripts ("where
is the weapon used?"); `mode: "text"` finds a substring (dialogue, names). Hits carry file and path.

**`validate`**: the engine's project check (loader on every file and map, formulas, a battle start per map): `ok` and the problems
as file plus message. It reports at most one problem per stage or map, so fixing one can reveal the next.

### Changing things

All writing tools answer with the same report: `files` (per file: `action`, a `diff`, `notes`), `wrote` (or `wouldWrite` in a
dry run), `problemsBefore`, `problemsAfter`, `newProblems`, `fixedProblems`, the current `problems`, and `warnings`.

**`set`**: one or many `{file, path, value}` changes in one atomic call: all or nothing. Values may be whole objects or lists.

```
> set {"game":"orchard","changes":[{"file":"data/units.json","path":"[archer].weapons","value":["crossbow"]}]}
< {"error":"refused, nothing was written: data/units.json:units[archer].weapons[0]: \"crossbow\" does not exist; valid values are sword, bow",
   "refused":[{"path":"data/units.json:units[archer].weapons[0]","problem":"...","allowed":["sword","bow"]}],"written":false}
```

**`add_entry`**: a new list entry: a table row, a map's unit or side, a story event, a command in an event. Without `value` it
starts from the schema's defaults; `kind` picks the shape for lists of several (a story command `say`, `addUnit` ...), `fields` lays
keys over it, `id` names it (a table row without one gets `new_entry`), `index` places it. Duplicate ids are refused.

**`duplicate_entry`**, **`remove_entry`**, **`move_entry`**: copy an entry (the copy gets `newId` or `<id>_copy`), remove an entry or
key (refused while other places still reference its id, listed in `usedBy`; `ignoreReferences` overrides), and reorder a list.

**`rename_id`**: renames an entry's id **and every reference to it** that the schemas know of, in all files, in one change, and
reports what it changed. References inside Lua scripts are plain text: they are listed, not changed.

**`enable_block`**, **`disable_block`**: many rule systems are optional blocks (`rules.actionPoints`, `rules.fog`, ...); absent means
off. Enabling writes exactly what the editor's Enable switch writes; a block that needs something that does not exist yet
(action points need a resource) is refused with a hint.

### Maps

**`get_map`**: size, legend, the tile grid as rows of legend symbols (top row first; `x` from the left, `y` from the top, both from
0), heights, sides, placed units and structures. Big maps are cropped to 80 x 80 unless you pass `rect`.

**`set_tiles`**: paints without touching the raw grid: `paint` is a list of rectangles `{x, y, w, h, terrain, height}` (`w`, `h`
default 1). The legend is kept in step. Squares outside the map are refused with the map's size.

**`new_map`**: a new map file filled with one terrain, with sides, registered in `game.json`, in one atomic change. A map needs
units on its sides to be a playable battle: add them with `add_entry` on the map's `units`.

### Games

**`list_templates`**: the starting points (the same templates as the browser editor's new-game dialog).

**`create_game`**: a new game folder from a template below a root: `folder` (lowercase letters, digits, `-`), `template`, `title`.
It is checked before it returns (a template starts clean) and never overwrites anything.

### Seeing and measuring

**`render_map`**: runs `ote-player` hidden and silent on the saved files and returns the battle screen as a PNG image block
(shrunk to `maxSize`, default 960 pixels on the long side). `style` picks a visual style, `turn` rotates the camera, `input` adds
player input tokens. When the player is missing the error says how to enable it.

**`simulate`**: runs `ote-sim` computer-against-computer for `runs` battles and returns the winner and rounds of each plus a
summary (wins per side, stalls, average rounds): balance feedback after changing units or maps.

### Resources and prompts

Resources: `ote://schema/<file>` (each editor schema as JSON), `ote://docs/<name>` (the engine's documents: GAME_FORMAT, SRPG,
ECONOMY, EVENTS, JOBS, TURN_ORDER, COMMANDERS, HEX, AI, ITEMS, STRUCTURES, DIFFICULTY, GRAPHICS, SOUND, BATTLE_SCENE and this one;
long ones are cut at 100 KB and say how to continue with `?offset=N`), and `ote://game/<id>/problems`. Prompts: `build_a_game`
(a step-by-step plan) and `review_game`.

## A worked session

Create a game, add a weapon and a unit, make and paint a map, place the units, check, look at it.

```
create_game   {"folder":"orchard","template":"blank","title":"Orchard Skirmish"}        -> created, problems 0
add_entry     {"game":"orchard","file":"data/weapons.json","value":{"id":"bow","name":"Bow"}}
add_entry     {"game":"orchard","file":"data/units.json","id":"archer",
               "fields":{"name":"Archer","glyph":"A","moveType":"foot","stats":{"hp":8,"mov":5},"weapons":["bow"]}}
set           {"game":"orchard","changes":[{"file":"data/units.json","path":"[archer].stats.hp","value":9}]}
new_map       {"game":"orchard","map":"grove","name":"The Grove","width":10,"height":6}  -> wrote game.json, maps/grove.json
set_tiles     {"game":"orchard","map":"grove","paint":[{"x":4,"y":0,"w":2,"h":6,"terrain":"wall"},{"x":4,"y":2,"w":2,"h":2,"terrain":"plain"}]}
add_entry     {"game":"orchard","file":"maps/grove.json","path":"units","value":{"type":"archer","side":"blue","x":1,"y":2}}
add_entry     {"game":"orchard","file":"maps/grove.json","path":"units","value":{"type":"archer","side":"red","x":8,"y":3}}
validate      {"game":"orchard"}                                                         -> ok, 0 problems
render_map    {"game":"orchard","map":"grove"}                                           -> a PNG of the battle
simulate      {"game":"orchard","map":"grove","runs":10}                                 -> wins per side, average rounds
```

Each writing step returns `problemsBefore` and `problemsAfter`; if a step introduces a problem, `newProblems` names it and the next
call can fix it. Use `dryRun=true` first whenever a change is unclear.

## How it works, for people who extend it

The schema engine (parsing `editor/schema/*.json`, variants, `when`, value checks, defaults, id sources) is one Qt-free library in
`editor/core` (`schema_engine.*`, `schema_values.cpp`, `schema_ids.*`). The native editor's model (`editor/native/model/schema.*`,
`id_source.*`) is a thin wrapper over it; `ote-mcp` (`editor/mcp`) uses it directly. A new key is added to the schema file once
(docs/NATIVE_EDITOR.md "Adding a key when the engine gains one") and both programs know it. The schemas and docs are compiled into
the program by `editor/mcp/gen/embed_gen.cpp` at build time. Tests: `ote_editor_core_tests` (the schema engine and the
I/O-free parts) and `tests/mcp_test.py` (the protocol, every tool, refusals, dry runs, a fuzz run), both in ctest.

## Troubleshooting

- **The client says the server failed to start.** Run `ote-mcp --root <folder>` in a terminal: it prints its settings on standard
  error and waits for input (press Ctrl-D to end). Check the path in the client's settings is the full path of the program.
- **"no game found under the roots".** The `--root` folder must hold game folders (each with a `game.json`) or be one. The default
  root is the folder the client starts the program in, which is often not what you want: pass `--root`.
- **`render_map` or `simulate` says the program was not found.** Build the engine so `ote-player` / `ote-sim` exist
  (`cmake --build build`), or pass `--player` / `--sim`. The message names the target.
- **`create_game` says the templates folder was not found.** Pass `--templates <path to editor/templates>`.
- **A write is refused with "changed on disk".** Someone else saved the file after you read it. Read it again (`get`) and redo the
  change, or pass `force=true` if overwriting is what you want.
- **A write is refused for an unknown key.** The schema does not know it. `describe` the place to see the keys; if the engine really
  takes it, pass `allowUnknown=true` and check with `validate`.
- **Results seem to be missing entries.** Long lists are paged: follow `nextOffset`, or use `mode: "summary"`.
