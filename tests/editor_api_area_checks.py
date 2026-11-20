"""Area attacks and line fire through the ote-editor API (docs/SRPG.md "Area attacks and line fire"): a weapon's "fire" and "area" keys come back
with GET, are written back unchanged by PUT, and the real loader (GET .../problems) accepts the data and names what it refuses. No editor form
exists for them (the editor is on hold); this only proves the server carries them. Starts from a copy of tests/fixtures/area-fire. Called from
editor_api_test.py; uses its helpers."""
import json
import os
import shutil

FIXTURE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fixtures", "area-fire")


def run_area_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "area-api"
    shutil.copytree(FIXTURE, os.path.join(games, pid))
    root = os.path.join(games, pid)

    def problems():
        st, res = api.json("GET", "/api/projects/%s/problems" % pid)
        return [p["message"] for p in res["problems"]] if st == 200 else [str(res)]

    def raw(rel):
        with open(os.path.join(root, rel), "rb") as f:
            return f.read()

    def copy(x):
        return json.loads(json.dumps(x))

    st, d = get_project(api, pid)
    check("area: the fixture loads in the real loader", st == 200 and problems() == [], problems())
    w = {x["id"]: x for x in d["weapons"]}
    check("area: GET carries fire and area with every key",
          w["rifle"]["fire"] == "line" and w["bomb"]["area"] == {"shape": "burst"} and w["mortar"]["area"] == {"shape": "radius", "radius": 2}
          and w["beam"]["area"] == {"shape": "line", "length": 3} and w["flak"]["area"]["friendlyFire"] is True
          and w["rail"]["fire"] == "line" and w["rail"]["area"]["length"] == 4 and "fire" not in w["sword"] and "area" not in w["sword"], sorted(w))

    before = raw("data/weapons.json")
    st, res = put_project(api, pid, d, d["revision"])
    after = raw("data/weapons.json")
    check("area: open + save keeps every key in the file",
          st == 200 and all(k in after for k in (b'"fire"', b'"area"', b'"shape"', b'"radius"', b'"length"', b'"friendlyFire"')), st)
    st, d2 = get_project(api, pid)
    check("area: ... the problems are still none and the data read back is the same", problems() == [] and d2["weapons"] == d["weapons"], "differs")

    def refused(label, change, needle):
        st, cur = get_project(api, pid)
        weapons = copy(cur["weapons"])
        change({x["id"]: x for x in weapons})
        st, res = put_project(api, pid, cur, cur["revision"], weapons=weapons)
        ps = problems()
        check("area: %s is named by the loader" % label, st == 200 and any(needle in p for p in ps), (st, ps))
        st, cur = get_project(api, pid)
        put_project(api, pid, cur, cur["revision"], weapons=copy(d["weapons"]))  # back to valid

    refused("an unknown shape", lambda b: b["bomb"]["area"].__setitem__("shape", "cone"), "\"area\".\"shape\" must be")
    refused("a radius out of range", lambda b: b["mortar"]["area"].__setitem__("radius", 9), "\"radius\" must be from 1 to 8")
    refused("a length on a radius shape", lambda b: b["mortar"]["area"].__setitem__("length", 2), "\"length\" belongs to the shape \"line\" only")
    refused("an unknown fire mode", lambda b: b["rifle"].__setitem__("fire", "arc"), "\"fire\" must be \"direct\" or \"line\"")
    refused("an unknown area key", lambda b: b["bomb"]["area"].__setitem__("colour", 1), "unknown key \"colour\"")
    st, cur = get_project(api, pid)
    check("area: written valid again", problems() == [] and cur["weapons"] == d["weapons"], problems())
