"""Areas and several effects for abilities through the ote-editor API (docs/SRPG.md "Areas and several effects for abilities"): an ability's "area",
a range from 0, an "effect" with heal and cure together and an "inflict" list come back with GET, are written back unchanged by PUT, and the real
loader (GET .../problems) accepts them and names what it refuses (the same refusals the web editor's checks mirror). Starts from a copy of
tests/fixtures/srpg-conditions with a data/abilities.json added. Called from editor_api_test.py; uses its helpers."""
import json
import os
import shutil

FIXTURE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fixtures", "srpg-conditions")

ABILITIES = [
    {"id": "cura", "name": "Cura", "target": "unit", "range": [0, 3], "effect": {"heal": 10}, "area": {"shape": "radius", "radius": 1}},
    {"id": "esunaga", "name": "Esunaga", "target": "ally", "range": [0, 3], "effect": {"cure": ["poison", "sleep"]}, "area": {"shape": "burst"}},
    {"id": "first_aid", "name": "First Aid", "target": "unit", "range": [1, 1], "effect": {"heal": 8, "cure": ["poison"]}},
    {"id": "barrier", "name": "Barrier", "target": "ally", "range": [1, 2], "inflict": [{"condition": "regen", "turns": 2}, {"condition": "stun"}]},
    {"id": "gaze", "name": "Gaze", "target": "enemy", "range": [1, 3], "inflict": [{"condition": "sleep", "chance": 40}, {"condition": "poison", "chance": "50"}],
     "area": {"shape": "cross", "radius": 1}},
]


def run_ability_area_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "ability-area-api"
    root = os.path.join(games, pid)
    shutil.copytree(FIXTURE, root)
    with open(os.path.join(root, "data", "abilities.json"), "w") as f:
        json.dump({"abilities": ABILITIES}, f)

    def problems():
        st, res = api.json("GET", "/api/projects/%s/problems" % pid)
        return [p["message"] for p in res["problems"]] if st == 200 else [str(res)]

    def raw(rel):
        with open(os.path.join(root, rel), "rb") as f:
            return f.read()

    def copy(x):
        return json.loads(json.dumps(x))

    st, d = get_project(api, pid)
    check("ability area: the fixture with the new keys loads in the real loader", st == 200 and problems() == [], problems())
    a = {x["id"]: x for x in d.get("abilities", [])}
    check("ability area: GET carries area, range 0, heal + cure and inflict lists",
          a.get("cura", {}).get("area") == {"shape": "radius", "radius": 1} and a["cura"]["range"] == [0, 3]
          and a.get("first_aid", {}).get("effect") == {"heal": 8, "cure": ["poison"]}
          and a.get("barrier", {}).get("inflict") == ABILITIES[3]["inflict"] and a.get("gaze", {}).get("area") == {"shape": "cross", "radius": 1},
          sorted(a))

    st, res = put_project(api, pid, d, d["revision"], abilities=d["abilities"])
    after = raw("data/abilities.json")
    check("ability area: open + save keeps every key in the file",
          st == 200 and all(k in after for k in (b'"area"', b'"shape"', b'"radius"', b'"cure"', b'"inflict"', b'"chance"', b'"turns"')), st)
    st, d2 = get_project(api, pid)
    check("ability area: ... no problems and the data read back is the same", problems() == [] and d2["abilities"] == d["abilities"], "differs")

    def refused(label, change, needle):
        st, cur = get_project(api, pid)
        abilities = copy(cur["abilities"])
        change({x["id"]: x for x in abilities})
        st, res = put_project(api, pid, cur, cur["revision"], abilities=abilities)
        ps = problems()
        check("ability area: %s is named by the loader" % label, st == 200 and any(needle in p for p in ps), (st, ps))
        st, cur = get_project(api, pid)
        put_project(api, pid, cur, cur["revision"], abilities=copy(d["abilities"]))  # back to valid

    refused("an area on a self ability", lambda b: b["cura"].update({"target": "self", "range": None}) or b["cura"].pop("range"),
            "\"area\" needs \"target\" \"ally\", \"enemy\" or \"unit\"")
    refused("an area with friendlyFire", lambda b: b["cura"]["area"].__setitem__("friendlyFire", True), "\"friendlyFire\" is for weapons")
    refused("an area on a push", lambda b: b["first_aid"].update({"effect": {"push": 1}, "area": {"shape": "burst"}}), "\"area\" goes with")
    refused("range 0 without an area", lambda b: b["first_aid"].__setitem__("range", [0, 1]), "whole numbers from 1 to 20")
    refused("heal with push", lambda b: b["first_aid"]["effect"].__setitem__("push", 1), "only heal and cure may go together")
    refused("a condition twice in a list", lambda b: b["barrier"]["inflict"].append({"condition": "regen"}), "names \"regen\" twice")
    refused("an empty inflict list", lambda b: b["barrier"].__setitem__("inflict", []), "a list of 1 to 8")
    st, cur = get_project(api, pid)
    check("ability area: written valid again", problems() == [] and cur["abilities"] == d["abilities"], problems())
