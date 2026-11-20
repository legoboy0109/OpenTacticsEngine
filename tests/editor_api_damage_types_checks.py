"""Damage types through the ote-editor API (docs/SRPG.md "Damage types"): game.json "damageTypes" / "resistRange", a weapon's "damageType" and a
unit type's "resist" are passed through like every other key: they open and save byte for byte, the real loader (GET .../problems) accepts them and
names what it refuses. Starts from a copy of tests/fixtures/damage-types (which also has a character with its own "resist" and a skill with the
{ "damageType": ... } condition; those files are not touched by a save). Called from editor_api_test.py; uses its helpers."""
import json
import os
import shutil

FIXTURE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fixtures", "damage-types")


def run_damage_types_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "damage-types-api"
    shutil.copytree(FIXTURE, os.path.join(games, pid))
    root = os.path.join(games, pid)

    def problems():
        st, res = api.json("GET", "/api/projects/%s/problems" % pid)
        return [p["message"] for p in res["problems"]] if st == 200 else [str(res)]

    def copy(x):
        return json.loads(json.dumps(x))

    def raw(rel):
        with open(os.path.join(root, rel), "rb") as f:
            return f.read()

    st, d = get_project(api, pid)
    check("damage types: the fixture loads in the real loader", st == 200 and problems() == [], problems())
    check("damage types: GET carries the keys", len(d["game"]["damageTypes"]) == 3 and d["game"]["resistRange"] == [-3, 3]
          and any(w.get("damageType") == "fire" for w in d["weapons"]) and any(u.get("resist") for u in d["units"]), "missing keys")

    files = ["game.json", "data/weapons.json", "data/units.json"]
    before = {f: raw(f) for f in files}
    st, res = put_project(api, pid, d, d["revision"])
    after = {f: raw(f) for f in files}
    check("damage types: open + save loses nothing (the keys are still in the files)", st == 200 and b'"damageTypes"' in after["game.json"] and b'"resistRange"' in after["game.json"]
          and b'"damageType"' in after["data/weapons.json"] and b'"resist"' in after["data/units.json"], (st, [f for f in files if after[f] != before[f]]))
    st, d2 = get_project(api, pid)
    check("damage types: ... and the problems are still none", problems() == [], problems())
    check("damage types: ... and the data read back is the same", d2["game"] == d["game"] and d2["weapons"] == d["weapons"] and d2["units"] == d["units"], "differs")

    def wall(b):
        return next(u for u in b["units"] if u["id"] == "wall")

    def refused(label, change, needle):
        st, cur = get_project(api, pid)
        body = {"game": copy(cur["game"]), "weapons": copy(cur["weapons"]), "units": copy(cur["units"])}
        change(body)
        st, res = put_project(api, pid, cur, cur["revision"], **body)
        ps = problems()
        check("damage types: %s is named by the loader" % label, st == 200 and any(needle in p for p in ps), (st, ps))
        st, cur = get_project(api, pid)
        put_project(api, pid, cur, cur["revision"], game=copy(d["game"]), weapons=copy(d["weapons"]), units=copy(d["units"]))  # back to valid

    refused("a weapon of an unknown type", lambda b: b["weapons"][1].__setitem__("damageType", "plasma"), "\"damageType\" names unknown damage type \"plasma\"")
    refused("a duplicate type id", lambda b: b["game"]["damageTypes"][1].__setitem__("id", "kinetic"), "duplicate id")
    refused("an unknown key of a type", lambda b: b["game"]["damageTypes"][0].__setitem__("colour", 1), "unknown key \"colour\"")
    refused("a resistance that is not a whole number", lambda b: wall(b)["resist"].__setitem__("kinetic", 1.5), "must be a whole number")
    refused("a resistance of an unknown type", lambda b: wall(b)["resist"].__setitem__("plasma", 1), "\"resist\" names unknown damage type \"plasma\"")
    refused("a resistance outside resistRange", lambda b: wall(b)["resist"].__setitem__("kinetic", 9), "must be from -3 to 3")
    refused("types removed while a weapon names one", lambda b: (b["game"].pop("damageTypes"), b["game"].pop("resistRange")), "needs \"damageTypes\" in game.json")

    st, cur = get_project(api, pid)
    check("damage types: valid data again -> no problems", problems() == [], problems())
