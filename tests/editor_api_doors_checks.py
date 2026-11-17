"""Doors, keys, chests and visiting checks of the ote-editor API test (docs/SRPG.md section 10, docs/EDITOR_UI.md "Doors, chests and
places"). The editor now has forms for them (the shapes they write are checked at the end); the first thing that matters is that the
helper keeps every key untouched through a load and a save: the "door" / "chest" / "visit" blocks and "blocksSight" of structure kinds, a key item's "opens", the "unlock"
effect of an ability and of a staff, a map structure's "id" / "contents" / "locked" / "visitSides" / "visitUnits", a map unit's
"goesFor", events with a "place" trigger and the door commands, and game.json's rules.places. The scratch project is the committed
fixture tests/fixtures/srpg-doors. Called from editor_api_test.py; uses its helpers."""
import json
import os
import shutil


def run_doors_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "doors-test"
    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    src = os.path.join(repo, "tests", "fixtures", "srpg-doors")
    dst = os.path.join(games, pid)
    shutil.copytree(src, dst)

    def read(rel):
        with open(os.path.join(dst, rel), "r", encoding="utf-8") as f:
            return json.load(f)

    st, d = get_project(api, pid)
    check("doors: the fixture opens in the helper", st == 200, st)
    st, p = api.json("GET", "/api/projects/%s/problems" % pid)
    check("doors: the real loader accepts it (no problems)", st == 200 and p["problems"] == [], p)
    check("doors: GET carries the structure kinds with their door / chest / visit blocks",
          d["structures"] == read("data/structures.json")["structures"])
    gate = [s for s in d["structures"] if s["id"] == "gate"][0]
    check("doors: a door block keeps its hp, stats and keys", gate["door"]["hp"] == 20 and gate["door"]["stats"] == {"def": 3} and gate["blocksSight"] is True, gate)
    check("doors: a key item keeps its opens list", [i for i in d["items"] if i["id"] == "master_key"][0]["opens"] == ["door", "chest"])
    hall = d["maps"]["hall"]
    check("doors: map structures keep id / contents / locked", hall["structures"][0]["id"] == "d1" and hall["structures"][2]["contents"][1] == {"resource": "gold", "amount": 50},
          hall["structures"][:3])
    check("doors: map events keep the place trigger", hall["events"][0]["when"] == {"place": "d1", "is": "opened"}, hall["events"][0])
    check("doors: a map unit keeps goesFor", d["maps"]["market"]["units"][1]["goesFor"] == ["chests"], d["maps"]["market"]["units"][1])

    # Round trip: save what was loaded, plus the SRPG files the helper handles itself; nothing may change.
    before = {rel: read(rel) for rel in ("game.json", "data/structures.json", "data/items.json", "data/abilities.json",
                                          "maps/hall.json", "maps/market.json", "maps/gatehouse.json", "maps/corridor.json")}
    overrides = {"structures": d["structures"], "items": d["items"]}
    if d.get("abilities") is not None:
        overrides["abilities"] = d["abilities"]
    st, res = put_project(api, pid, d, d["revision"], **overrides)
    check("doors: saving it back is accepted", st == 200, (st, res))
    after = {rel: read(rel) for rel in before}
    for rel in before:
        check("doors: %s is unchanged by a load and a save" % rel, before[rel] == after[rel])
    st, p = api.json("GET", "/api/projects/%s/problems" % pid)
    check("doors: still no problems after the save", st == 200 and p["problems"] == [], p)

    # A bad value is reported by the real loader in the usual words, naming the file.
    st, d = get_project(api, pid)
    kinds = d["structures"]
    for s in kinds:
        if s["id"] == "cell_door":
            s["door"]["keys"] = ["nope"]
    st, res = put_project(api, pid, d, d["revision"], structures=kinds)
    st, p = api.json("GET", "/api/projects/%s/problems" % pid)
    check("doors: a key list naming an unknown item is a problem in data/structures.json",
          any("\"keys\" names unknown item \"nope\"" in x["message"] and x["file"] == "data/structures.json" for x in p["problems"]), p)
    for s in kinds:
        if s["id"] == "cell_door":
            s["door"]["keys"] = ["brass_key", "master_key"]
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], structures=kinds)
    st, p = api.json("GET", "/api/projects/%s/problems" % pid)
    check("doors: and the fixed list is accepted again", st == 200 and p["problems"] == [], p)

    # The shapes the editor's forms write (docs/EDITOR_UI.md "Doors, chests and places"): story-only door, a one-kind unlock staff and ability, the
    # automatic rule. Each is accepted by the real loader; the mistakes the forms and checks guard against are refused in its words.
    st, d = get_project(api, pid)
    kinds = d["structures"]
    for s in kinds:
        if s["id"] == "cell_door":
            s["door"] = {"opens": "cell_door_open", "locked": False, "by": [], "from": "on"}   # keys went with "A key"
    items = d["items"]
    for i in items:
        if i["id"] == "wild_staff":
            i["effects"] = [{"unlock": ["chest"]}]
    abilities = d["abilities"]
    for a in abilities:
        if a["id"] == "deft":
            a["effect"] = {"unlock": {"needs": "lockpick"}}
    game = d["game"]
    game.setdefault("rules", {})["places"] = {"actions": "auto"}
    st, res = put_project(api, pid, d, d["revision"], game=game, structures=kinds, items=items, abilities=abilities)
    check("doors: the shapes the forms write are accepted", st == 200, (st, res))
    st, p = api.json("GET", "/api/projects/%s/problems" % pid)
    check("doors: a story-only door, one-kind staff, lockpick ability and rules.places auto load", st == 200 and p["problems"] == [], p)
    check("doors: the file keeps them as written", read("data/structures.json")["structures"][2]["door"]["by"] == [] and
          read("game.json")["rules"]["places"] == {"actions": "auto"} and read("data/items.json")["items"][8]["effects"] == [{"unlock": ["chest"]}])

    # A chest with nothing inside, a door that stops nobody, a break with no hit points: refused in the loader's words.
    st, d = get_project(api, pid)
    hall = d["maps"]["hall"]
    hall["structures"][2].pop("contents")
    st, res = put_project(api, pid, d, d["revision"], maps=d["maps"], structures=d["structures"], items=d["items"], abilities=d["abilities"])
    st, p = api.json("GET", "/api/projects/%s/problems" % pid)
    check("doors: a chest with no contents is a problem naming the map file",
          any("needs \"contents\"" in x["message"] and x["file"] == "maps/hall.json" for x in p["problems"]), p)
    st, d = get_project(api, pid)   # each accepted save moves the revision on
    d["maps"]["hall"]["structures"][2]["contents"] = ["potion", {"resource": "gold", "amount": 50}]
    kinds = d["structures"]
    for s in kinds:
        if s["id"] == "cell_door":
            s["moveCost"] = {"foot": 1}
    st, res = put_project(api, pid, d, d["revision"], maps=d["maps"], structures=kinds, items=d["items"], abilities=d["abilities"])
    st, p = api.json("GET", "/api/projects/%s/problems" % pid)
    check("doors: a door that stops no kind of unit is a problem",
          any("a door must stop at least one movement type" in x["message"] for x in p["problems"]), p)
    st, d = get_project(api, pid)
    kinds = d["structures"]
    for s in kinds:
        if s["id"] == "cell_door":
            s["moveCost"] = {"foot": 0, "fly": 0}
            s["door"]["by"] = ["break"]
    st, res = put_project(api, pid, d, d["revision"], maps=d["maps"], structures=kinds, items=d["items"], abilities=d["abilities"])
    st, p = api.json("GET", "/api/projects/%s/problems" % pid)
    check("doors: breaking without hit points is a problem",
          any("\"by\" has \"break\" but the door has no \"hp\"" in x["message"] for x in p["problems"]), p)
    # Put it right again: the project is accepted once more.
    st, d = get_project(api, pid)
    for s in d["structures"]:
        if s["id"] == "cell_door":
            s["door"] = {"opens": "cell_door_open", "keys": ["brass_key", "master_key"]}
    st, res = put_project(api, pid, d, d["revision"], structures=d["structures"], maps=d["maps"], items=d["items"], abilities=d["abilities"])
    st, p = api.json("GET", "/api/projects/%s/problems" % pid)
    check("doors: and the corrected door is accepted again", st == 200 and p["problems"] == [], p)
