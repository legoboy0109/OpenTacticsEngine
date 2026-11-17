"""Doors, keys, chests and visiting checks of the ote-editor API test (docs/SRPG.md section 10, docs/EDITOR_UI.md "Doors, chests and
places: hand-edited"). There is no editor form for them yet; what matters is that the helper keeps every new key untouched through
a load and a save: the "door" / "chest" / "visit" blocks and "blocksSight" of structure kinds, a key item's "opens", the "unlock"
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
