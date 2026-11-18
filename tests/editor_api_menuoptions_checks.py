"""Menu option checks of the ote-editor API test (docs/SRPG.md "Menu options"): game.json's "menuUnavailable" and "attackWeaponChoice" must come
through a load and a save untouched, and the real loader's refusals must reach the Problems list in its own words. The scratch project is
written here. Called from editor_api_test.py; uses its helpers."""
import json
import os


def run_menuoptions_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "menuoptions-test"
    dst = os.path.join(games, pid)

    def write(rel, obj):
        path = os.path.join(dst, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8") as f:
            json.dump(obj, f, indent=1)

    def read(rel):
        with open(os.path.join(dst, rel), "r", encoding="utf-8") as f:
            return json.load(f)

    game = {
        "formatVersion": 1, "name": "Menu options", "view": {"width": 480, "height": 270, "tileSize": 24},
        "stats": [{"id": "hp", "name": "HP"}, {"id": "atk", "name": "Attack"}, {"id": "mov", "name": "Move"}],
        "roles": {"hp": "hp", "move": "mov"}, "moveTypes": [{"id": "foot", "name": "Foot"}], "formulas": {"damage": "a.atk"},
        "rules": {"items": {"weapons": "carried", "inventorySize": 4}},
        "menuUnavailable": "hide", "attackWeaponChoice": "pick", "maps": [{"id": "arena", "name": "Arena", "file": "maps/arena.json"}]}
    write("game.json", game)
    write("data/terrain.json", {"terrain": [{"id": "plain", "name": "Plain"}]})
    write("data/weapons.json", {"weapons": [{"id": "sword", "name": "Sword", "group": "blade"}]})
    write("data/items.json", {"items": [{"id": "iron_sword", "name": "Iron Sword", "kind": "weapon", "weapon": "sword"}]})
    write("data/units.json", {"units": [{"id": "knight", "name": "Knight", "moveType": "foot", "stats": {"hp": 20, "atk": 5, "mov": 4}, "wields": ["blade"]}]})
    write("maps/arena.json", {
        "name": "Arena", "grid": {"type": "square", "width": 4, "height": 1}, "legend": {".": "plain"}, "tiles": ["...."],
        "sides": [{"id": "blue", "controller": "human"}, {"id": "red", "controller": "ai"}],
        "units": [{"type": "knight", "id": "hero", "side": "blue", "x": 0, "y": 0, "items": ["iron_sword"]}, {"type": "knight", "id": "foe", "side": "red", "x": 2, "y": 0}]})

    def problems():
        st, p = api.json("GET", "/api/projects/%s/problems" % pid)
        return p["problems"] if st == 200 else None

    def says(part):
        return any(part in x["message"] and x["file"] == "game.json" for x in (problems() or []))

    st, d = get_project(api, pid)
    check("menuoptions: the project opens in the helper", st == 200, st)
    check("menuoptions: the real loader accepts both keys (no problems)", problems() == [], problems())
    check("menuoptions: both keys arrive as written", d["game"].get("menuUnavailable") == "hide" and d["game"].get("attackWeaponChoice") == "pick", d["game"])
    before = read("game.json")
    st, res = put_project(api, pid, d, d["revision"])
    check("menuoptions: saving it back is accepted", st == 200, (st, res))
    check("menuoptions: game.json is unchanged by a load and a save", read("game.json") == before)

    def broken(label, part, edit):
        st, d = get_project(api, pid)
        edit(d)
        put_project(api, pid, d, d["revision"])
        check("menuoptions: %s is refused" % label, says(part), problems())
        write("game.json", before)
        check("menuoptions: and the fixed file loads again (%s)" % label, problems() == [], problems())

    broken("a menuUnavailable that is neither grey nor hide", "\"menuUnavailable\" must be \"grey\"", lambda d: d["game"].update({"menuUnavailable": "gray"}))
    broken("an attackWeaponChoice that is neither equipped nor pick", "\"attackWeaponChoice\" must be \"equipped\"", lambda d: d["game"].update({"attackWeaponChoice": "ask"}))
    broken("a menuUnavailable that is not a word", "menuUnavailable", lambda d: d["game"].update({"menuUnavailable": True}))
    # removing the keys is a plain game again
    st, d = get_project(api, pid)
    d["game"].pop("menuUnavailable")
    d["game"].pop("attackWeaponChoice")
    put_project(api, pid, d, d["revision"])
    check("menuoptions: a game without the keys loads and saves without them",
          problems() == [] and "menuUnavailable" not in read("game.json") and "attackWeaponChoice" not in read("game.json"), problems())
