"""Fidelity 2 checks of the ote-editor API test (docs/SRPG.md section 11, docs/EVENTS.md "A staged fight", docs/BATTLE_SCENE.md, docs/ECONOMY.md). The helper
must keep the new keys untouched through a load and a save (an ability with only an inflict, a summon's items / pick / replace, a hurt range, a terrain's
weatherCost, a map's weather and battle tables, a `fight` command, the game's two scene choices, a platform table in graphics.json) and the real
loader's refusals must reach the Problems list in its own words, naming the file. The scratch project is written here. Called from editor_api_test.py;
uses its helpers."""
import copy
import json
import os


def run_fidelity2_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "fidelity2-test"
    dst = os.path.join(games, pid)

    def write(rel, obj):
        path = os.path.join(dst, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8") as f:
            json.dump(obj, f, indent=1)

    def read(rel):
        with open(os.path.join(dst, rel), "r", encoding="utf-8") as f:
            return json.load(f)

    def png(rel, w, h):
        header = bytes([0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0, 0, 0, 13, 0x49, 0x48, 0x44, 0x52]) + w.to_bytes(4, "big") + h.to_bytes(4, "big") + bytes([8, 6])
        path = os.path.join(dst, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(header + bytes(8))

    game = {
        "formatVersion": 1, "name": "Fidelity 2", "view": {"width": 480, "height": 270, "tileSize": 24},
        "stats": [{"id": "hp", "name": "HP"}, {"id": "atk", "name": "Attack"}, {"id": "def", "name": "Defense"}, {"id": "mov", "name": "Move"}],
        "roles": {"hp": "hp", "move": "mov"}, "moveTypes": [{"id": "foot", "name": "Foot"}],
        "rules": {"defeat": ["noUnits"], "counterAttacks": True, "items": {"weapons": "carried", "inventorySize": 4},
                  "progression": {"enabled": True, "xpPerLevel": 100, "maxLevel": 20, "xpHit": "30", "xpKill": "60"}},
        "formulas": {"damage": "a.atk - b.def", "hit": "90"}, "battleScene": "scene",
        "battleBackgroundFrom": "attacker", "battleSceneSides": "playerRight",
        "maps": [{"id": "arena", "name": "Arena", "file": "maps/arena.json"}]}
    write("game.json", game)
    write("data/terrain.json", {"terrain": [
        {"id": "plain", "name": "Plain"},
        {"id": "swamp", "name": "Swamp", "moveCost": {"foot": 2}, "weatherCost": {"rain": {"foot": 4}, "snow": {"foot": 1}}}]})
    write("data/weapons.json", {"weapons": [{"id": "sword", "group": "blade"}]})
    write("data/items.json", {"items": [{"id": "iron_sword", "name": "Iron Sword", "kind": "weapon", "weapon": "sword", "uses": 3},
                                        {"id": "fang", "name": "Fang", "kind": "weapon", "weapon": "sword"}]})
    write("data/conditions.json", {"conditions": [
        {"id": "boost", "name": "Boost", "turns": 4, "blocks": [{"when": "always", "do": [{"stats": {"atk": 2}}]}]},
        {"id": "poison", "name": "Poison", "turns": 5, "blocks": [{"when": "turnStart", "do": [{"hurt": {"amount": [1, 3], "lethal": True}, "to": "self"}]}]}]})
    write("data/abilities.json", {"abilities": [
        {"id": "rally", "name": "Rally", "target": "ally", "inflict": {"condition": "boost", "turns": 4}, "xp": 20},
        {"id": "call_pet", "name": "Call Pet", "target": "cell", "range": [1, 2],
         "effect": {"summon": {"type": "hound", "limit": 2, "replace": True, "items": [{"item": "fang", "minLevel": 3}, {"item": "iron_sword"}], "pick": "random"}}}]})
    write("data/units.json", {"units": [
        {"id": "knight", "name": "Knight", "moveType": "foot", "stats": {"hp": 30, "atk": 8, "def": 2, "mov": 4}, "wields": ["blade"],
         "items": ["iron_sword"], "abilities": ["rally", "call_pet"]},
        {"id": "hound", "name": "Hound", "moveType": "foot", "stats": {"hp": 8, "atk": 3, "def": 1, "mov": 4}, "wields": ["blade"], "items": ["fang"]}]})
    png("art/bgs.png", 240, 68)
    png("art/plat.png", 96, 16)
    write("data/graphics.json", {
        "sheets": [{"id": "bgs", "file": "art/bgs.png", "cellWidth": 120, "cellHeight": 68}, {"id": "plat", "file": "art/plat.png", "cellWidth": 48, "cellHeight": 16}],
        "battle": {"backgrounds": {"meadow": {"sheet": "bgs", "cell": [0, 0]}, "cave": {"sheet": "bgs", "cell": [1, 0]}}, "terrainBackground": {"plain": "meadow"},
                   "defaultBackground": "meadow",
                   "platforms": {"grass": {"sheet": "plat", "cell": [0, 0], "far": [1, 0]}, "boards": {"sheet": "plat", "cell": [1, 0]}},
                   "terrainPlatform": {"plain": "grass"}, "defaultPlatform": "boards"}})
    write("maps/arena.json", {
        "name": "Arena", "grid": {"type": "square", "width": 6, "height": 3}, "legend": {".": "plain", "s": "swamp"},
        "tiles": ["......", "..ss..", "......"], "weather": "rain",
        "battleBackgrounds": {"swamp": "cave", "default": "meadow"}, "battlePlatforms": {"swamp": "boards", "default": "grass"},
        "sides": [{"id": "blue", "controller": "human"}, {"id": "red", "controller": "ai"}],
        "units": [{"type": "knight", "id": "hero", "side": "blue", "x": 1, "y": 1, "leader": True}, {"type": "knight", "id": "pal", "side": "blue", "x": 0, "y": 1},
                  {"type": "knight", "id": "bandit", "side": "red", "x": 2, "y": 1}],
        "events": [{"id": "duel", "when": {"battle": "start"}, "do": [
            {"fight": {"attacker": "hero", "defender": "bandit", "strikes": [{"by": "attacker", "result": "hit", "damage": 3}, {"by": "defender", "result": "miss"}],
                       "weapon": {"attacker": "sword"}, "lethal": False, "xp": True, "wear": True}}]}]})

    def problems():
        st, p = api.json("GET", "/api/projects/%s/problems" % pid)
        return p["problems"] if st == 200 else None

    def says(part, file=None):
        return any(part in x["message"] and (file is None or x["file"] == file) for x in (problems() or []))

    st, d = get_project(api, pid)
    check("fidelity2: the project opens in the helper", st == 200, st)
    check("fidelity2: the real loader accepts it (no problems)", problems() == [], problems())
    check("fidelity2: an ability with only an inflict keeps its keys",
          [a for a in d["abilities"] if a["id"] == "rally"][0] == {"id": "rally", "name": "Rally", "target": "ally", "inflict": {"condition": "boost", "turns": 4}, "xp": 20})
    check("fidelity2: the game's two scene choices and a terrain's weather costs arrive",
          d["game"].get("battleBackgroundFrom") == "attacker" and d["game"].get("battleSceneSides") == "playerRight" and
          [t for t in d["terrain"] if t["id"] == "swamp"][0]["weatherCost"] == {"rain": {"foot": 4}, "snow": {"foot": 1}})
    check("fidelity2: the map's weather, tables and the fight command arrive",
          d["maps"]["arena"]["weather"] == "rain" and d["maps"]["arena"]["battlePlatforms"]["default"] == "grass" and
          d["maps"]["arena"]["events"][0]["do"][0]["fight"]["strikes"][1] == {"by": "defender", "result": "miss"})

    before = {rel: read(rel) for rel in ("game.json", "data/terrain.json", "data/abilities.json", "data/conditions.json", "data/graphics.json", "maps/arena.json")}
    st, res = put_project(api, pid, d, d["revision"], abilities=d["abilities"], conditions=d["conditions"], graphics=d["graphics"])
    check("fidelity2: saving it back is accepted", st == 200, (st, res))
    for rel in before:
        check("fidelity2: %s is unchanged by a load and a save" % rel, before[rel] == read(rel), rel)

    def broken(label, part, file, edit):
        st, d = get_project(api, pid)
        edit(d)
        put_project(api, pid, d, d["revision"], abilities=d["abilities"], conditions=d["conditions"], graphics=d["graphics"])
        check("fidelity2: %s is refused" % label, says(part, file), problems())
        write(file, before[file])  # back to the good file
        check("fidelity2: and the fixed file loads again (%s)" % label, problems() == [], problems())

    broken("an ability with neither an effect nor an inflict", "missing required \"effect\"", "data/abilities.json",
           lambda d: d["abilities"][0].pop("inflict"))
    broken("a summon pick without items", "\"pick\" goes with \"items\"", "data/abilities.json",
           lambda d: d["abilities"][1]["effect"]["summon"].pop("items"))
    broken("a summon item the game does not have", "unknown item \"nope\"", "data/abilities.json",
           lambda d: d["abilities"][1]["effect"]["summon"]["items"].append({"item": "nope"}))
    broken("a hurt range with the highest below the lowest", "highest above the lowest", "data/conditions.json",
           lambda d: d["conditions"][1]["blocks"][0]["do"][0]["hurt"].update({"amount": [3, 1]}))
    broken("a weather cost of 0", "from 1 to 99", "data/terrain.json",
           lambda d: [t for t in d["terrain"] if t["id"] == "swamp"][0]["weatherCost"]["rain"].update({"foot": 0}))
    broken("a weather the game does not know", "unknown weather \"fog\"", "data/terrain.json",
           lambda d: [t for t in d["terrain"] if t["id"] == "swamp"][0]["weatherCost"].update({"fog": {"foot": 2}}))
    broken("a map weather that is not rain or snow", "\"weather\" must be \"rain\", \"snow\" or \"none\"", "maps/arena.json",
           lambda d: d["maps"]["arena"].update({"weather": "hail"}))
    broken("a map table naming a terrain the game does not have", "no terrain with this id", "maps/arena.json",
           lambda d: d["maps"]["arena"]["battleBackgrounds"].update({"lava": "cave"}))
    broken("a map table naming a platform the game does not have", "there is no platform \"nope\"", "maps/arena.json",
           lambda d: d["maps"]["arena"]["battlePlatforms"].update({"swamp": "nope"}))
    broken("a fight that is not played", "a miss does no damage", "maps/arena.json",
           lambda d: d["maps"]["arena"]["events"][0]["do"][0]["fight"]["strikes"][1].update({"damage": 4}))
    broken("a fight against itself", "cannot fight itself", "maps/arena.json",
           lambda d: d["maps"]["arena"]["events"][0]["do"][0]["fight"].update({"defender": "hero"}))
    broken("a battleSceneSides the game does not know", "\"battleSceneSides\" must be", "game.json",
           lambda d: d["game"].update({"battleSceneSides": "center"}))
    broken("a battleBackgroundFrom the game does not know", "\"battleBackgroundFrom\" must be", "game.json",
           lambda d: d["game"].update({"battleBackgroundFrom": "both"}))
    broken("a platform with a far picture outside its image", "outside sheet", "data/graphics.json",
           lambda d: d["graphics"]["battle"]["platforms"]["grass"].update({"far": [9, 0]}))
