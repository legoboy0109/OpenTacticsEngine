"""Fog of war data through the ote-editor API (docs/ECONOMY.md "Fog of war", docs/EDITOR_API.md): a map's `fog`,
units' `vision`, terrain / structures' `visionBonus` and `hides`, and a `sees` condition in a map event are passed
through like every other key. The checks: they open and save byte for byte, and the real loader (GET .../problems)
accepts them and names what it refuses (ranges, more than 32 sides with fog, a bad `sees`).
Called from editor_api_test.py; uses its helpers."""
import json
import os


def run_fog_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "fog-test"
    st, doc = api.json("POST", "/api/projects", {"id": pid, "name": "Fog", "template": "economy-tactics"})
    check("fog: create scratch project", st == 201, (st, doc))
    root = os.path.join(games, pid)

    def raw(rel):
        with open(os.path.join(root, rel), "rb") as f:
            return f.read()

    def problems():
        st, res = api.json("GET", "/api/projects/%s/problems" % pid)
        return res["problems"] if st == 200 else [{"file": "?", "message": str(res)}]

    def copy(x):
        return json.loads(json.dumps(x))

    st, d = get_project(api, pid)
    mid = d["game"]["maps"][0]["id"]
    mfile = d["game"]["maps"][0]["file"]
    m = copy(d["maps"][mid])
    sides = [s["id"] for s in m["sides"]]
    check("fog: template map has two sides or more", len(sides) >= 2, sides)
    units = copy(d["units"])
    units[0]["vision"] = 4
    units[1]["vision"] = 0
    terrain = copy(d["terrain"])
    terrain[0]["visionBonus"] = 3
    terrain[0]["hides"] = True
    terrain[1]["hides"] = False
    structures = copy(d["structures"])
    structures[0]["visionBonus"] = 10
    structures[0]["hides"] = True
    structures[0]["futureKey"] = {"keep": [1]}
    # The object form: extra sight for some unit types only (ids and labels), next to a plain number on another kind.
    terrain[1]["visionBonus"] = {"amount": 2, "units": [units[0]["id"], units[1]["id"]]}
    structures[1]["visionBonus"] = {"amount": 0, "units": [units[2]["id"]]}
    m["fog"] = True
    m["units"][0]["id"] = "scout"
    m["events"] = [{"id": "spotted", "when": {"turn": "every"},
                    "if": [{"sees": {"type": units[0]["id"]}, "team": sides[0]}, {"sees": {"unit": "scout"}, "team": sides[1]}],
                    "do": [{"say": "I see you."}]}]
    # The first PUT lets the server write its own layout; later saves must not change a byte.
    st, res = put_project(api, pid, d, d["revision"], units=units, terrain=terrain, structures=structures, maps={mid: m})
    check("fog: PUT of fog keys -> 200", st == 200 and "revision" in res, (st, res))
    check("fog: the real loader accepts them", problems() == [], problems())

    files = [mfile, "data/units.json", "data/terrain.json", "data/structures.json"]
    before = {f: raw(f) for f in files}
    st, d = get_project(api, pid)
    check("fog: GET returns the keys as written",
          d["maps"][mid]["fog"] is True and d["maps"][mid]["events"] == m["events"] and d["units"] == units
          and d["terrain"] == terrain and d["structures"] == structures, "mismatch")
    st, res = put_project(api, pid, d, d["revision"], units=d["units"], terrain=d["terrain"], structures=d["structures"],
                          maps=d["maps"], items=d["items"])
    after = {f: raw(f) for f in files}
    check("fog: open + save keeps every file byte for byte", st == 200 and after == before, [f for f in files if after[f] != before[f]])
    check("fog: unknown key on a structure survives", b'"futureKey"' in after["data/structures.json"])
    check("fog: the object form of visionBonus is written as it came (terrain and structure)",
          json.loads(after["data/terrain.json"])["terrain"][1]["visionBonus"] == terrain[1]["visionBonus"]
          and json.loads(after["data/structures.json"])["structures"][1]["visionBonus"] == structures[1]["visionBonus"]
          and b'"visionBonus": 3' in after["data/terrain.json"] and b'"visionBonus": 10' in after["data/structures.json"])

    # An edit elsewhere in a file keeps the fog keys (also a vision of 0 and an explicit hides: false).
    d["units"][0]["name"] = "Renamed"
    st, res = put_project(api, pid, d, d["revision"], units=d["units"])
    text = raw("data/units.json")
    check("fog: an edit elsewhere keeps vision (also 0)", st == 200 and b'"vision": 4' in text and b'"vision": 0' in text)
    check("fog: hides false is kept as written", b'"hides": false' in raw("data/terrain.json"))

    # ---- what the loader refuses comes back as problems naming the key ----
    def refused(label, change, needle):
        st, d = get_project(api, pid)
        body = {"units": copy(units), "terrain": copy(terrain), "structures": copy(structures), "maps": {mid: copy(m)}}
        change(body)
        st, res = put_project(api, pid, d, d["revision"], **body)
        ps = problems()
        check("fog: %s is named by the loader" % label, st == 200 and any(needle in p["message"] for p in ps), ps)

    refused("vision 21", lambda b: b["units"][0].__setitem__("vision", 21), "vision")
    refused("vision -1", lambda b: b["units"][0].__setitem__("vision", -1), "vision")
    refused("visionBonus 11", lambda b: b["terrain"][0].__setitem__("visionBonus", 11), "visionBonus")
    refused("a limited bonus above 10", lambda b: b["terrain"][1].__setitem__("visionBonus", {"amount": 11, "units": [units[0]["id"]]}), "amount")
    refused("a limited bonus with an empty list", lambda b: b["terrain"][1].__setitem__("visionBonus", {"amount": 2, "units": []}), "units")
    refused("a limited bonus naming an unknown unit", lambda b: b["structures"][1].__setitem__("visionBonus", {"amount": 2, "units": ["nobody"]}), "nobody")
    refused("a limited bonus without units", lambda b: b["terrain"][1].__setitem__("visionBonus", {"amount": 2}), "units")
    refused("hides as text", lambda b: b["structures"][0].__setitem__("hides", "yes"), "hides")

    def many_sides(b):
        mm = b["maps"][mid]
        mm["sides"] = [dict(mm["sides"][0], id="s%d" % i, name="S%d" % i) for i in range(33)]
        mm["units"] = []
        mm["structures"] = []
        mm.pop("events", None)
        mm.pop("objectives", None)

    refused("33 sides with fog", many_sides, "32")
    refused("a sees with an unknown team", lambda b: b["maps"][mid]["events"][0]["if"][0].__setitem__("team", "nobody"), "team")
    refused("a sees without a team", lambda b: b["maps"][mid]["events"][0]["if"][0].pop("team"), "team")

    # Back to a valid state: nothing lingers.
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], units=units, terrain=terrain, structures=structures, maps={mid: m})
    check("fog: valid data again -> no problems", st == 200 and problems() == [], problems())
