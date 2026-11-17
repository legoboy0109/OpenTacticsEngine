"""Map weapon checks of the ote-editor API test (docs/SRPG.md "Map weapons", docs/EDITOR_UI.md). The helper must keep the new keys untouched through a
load and a save (a structure kind's "weapon" / "uses" / "by" / "ignoreRank" / "becomes", a placed structure's "uses", a unit's "goesFor": ["weapons"])
and the real loader's refusals must reach the Problems list in its own words, naming the file. The scratch project is the committed fixture
tests/fixtures/srpg-mapweapon. Called from editor_api_test.py; uses its helpers."""
import json
import os
import shutil


def run_mapweapon_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "mapweapon-test"
    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    src = os.path.join(repo, "tests", "fixtures", "srpg-mapweapon")
    dst = os.path.join(games, pid)
    shutil.copytree(src, dst)

    def read(rel):
        with open(os.path.join(dst, rel), "r", encoding="utf-8") as f:
            return json.load(f)

    def problems():
        st, p = api.json("GET", "/api/projects/%s/problems" % pid)
        return p["problems"] if st == 200 else None

    def says(part, file=None):
        return any(part in x["message"] and (file is None or x["file"] == file) for x in (problems() or []))

    st, d = get_project(api, pid)
    check("mapweapon: the fixture opens in the helper", st == 200, st)
    check("mapweapon: the real loader accepts it (no problems)", problems() == [], problems())
    ballista = [s for s in d["structures"] if s["id"] == "ballista"][0]
    check("mapweapon: a kind keeps its weapon, uses, who may use it and what it becomes",
          ballista["weapon"] == "ballista_bolt" and ballista["uses"] == 3 and ballista["by"] == {"tags": ["archer"]} and ballista["becomes"] == "ballista_wreck", ballista)
    check("mapweapon: a placed structure keeps its own uses", [p for p in d["maps"]["field"]["structures"] if p.get("id") == "b2"][0]["uses"] == 1)

    before = {rel: read(rel) for rel in ("game.json", "data/structures.json", "data/items.json", "maps/field.json", "maps/siege.json")}
    st, res = put_project(api, pid, d, d["revision"], structures=d["structures"], items=d["items"])
    check("mapweapon: saving it back is accepted", st == 200, (st, res))
    for rel in before:
        check("mapweapon: %s is unchanged by a load and a save" % rel, before[rel] == read(rel))

    # The shapes the editor writes: a limit of types and tags, ignoreRank, a unit that goes for weapons.
    st, d = get_project(api, pid)
    kinds = d["structures"]
    for s in kinds:
        if s["id"] == "catapult":
            s.update({"uses": 4, "by": {"types": ["knight"], "tags": ["archer"]}, "ignoreRank": True, "becomes": "ballista_wreck"})
    siege = d["maps"]["siege"]
    siege["units"][1]["goesFor"] = ["weapons"]
    st, res = put_project(api, pid, d, d["revision"], structures=kinds, maps=d["maps"])
    check("mapweapon: the shapes the card writes are accepted", st == 200, (st, res))
    check("mapweapon: and load", problems() == [], problems())
    check("mapweapon: the files keep them as written", read("data/structures.json")["structures"][2]["by"] == {"types": ["knight"], "tags": ["archer"]} and
          read("maps/siege.json")["units"][1]["goesFor"] == ["weapons"])

    # Each mistake the card's checks guard against is refused by the loader in its own words.
    def broken(patch, part, kind="ballista", label=None):
        st, d = get_project(api, pid)
        kinds = d["structures"]
        for s in kinds:
            if s["id"] == kind:
                s.update(patch)
        put_project(api, pid, d, d["revision"], structures=kinds)
        check("mapweapon: %s is refused" % (label or part), says(part, "data/structures.json"), problems())
        st, d = get_project(api, pid)
        put_project(api, pid, d, d["revision"], structures=json.load(open(os.path.join(src, "data", "structures.json")))["structures"])
        check("mapweapon: and the fixed kinds load again (%s)" % (label or part), problems() == [], problems())

    broken({"weapon": "nope"}, "unknown item \"nope\"")
    broken({"weapon": "potion"}, "which is not a weapon")
    broken({"uses": 0}, "\"uses\" must be a whole number from 1 to 9999")
    broken({"by": {"types": ["dragon"]}}, "unknown unit type \"dragon\"")
    broken({"by": {"tags": ["flying"]}}, "is not used by any unit type")
    broken({"becomes": "nowhere"}, "unknown structure \"nowhere\"")
    broken({"moveCost": {"foot": 0}}, "can stand", "ballista", "a kind nobody can stand on")
    broken({"uses": 2}, "belongs with a \"weapon\"", "tower", "uses without a weapon")

    # A placed structure's uses.
    st, d = get_project(api, pid)
    for p in d["maps"]["field"]["structures"]:
        if p["type"] == "tower":
            p["uses"] = 2
    put_project(api, pid, d, d["revision"], maps=d["maps"])
    check("mapweapon: a placed number on a kind without a weapon is refused", says("only belongs to a structure whose kind has a \"weapon\"", "maps/field.json"), problems())
    st, d = get_project(api, pid)
    for p in d["maps"]["field"]["structures"]:
        if p["type"] == "tower":
            p.pop("uses")
        if p.get("id") == "b2":
            p["uses"] = 0
    put_project(api, pid, d, d["revision"], maps=d["maps"])
    check("mapweapon: a placed number out of range is refused", says("\"uses\" must be a whole number from 1 to 9999", "maps/field.json"), problems())
    st, d = get_project(api, pid)
    for p in d["maps"]["field"]["structures"]:
        if p.get("id") == "b2":
            p["uses"] = 1
    put_project(api, pid, d, d["revision"], maps=d["maps"])
    check("mapweapon: and the corrected map loads", problems() == [], problems())
