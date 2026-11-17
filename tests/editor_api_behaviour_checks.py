"""Computer unit behaviour checks of the ote-editor API test (docs/AI.md, docs/EDITOR_UI.md "Behaviour"). The helper must keep a map unit's
"behaviour" block, an addUnit's and the setBehaviour / wakeGroup commands untouched through a load and a save, and the real loader (the
helper's "problems") must accept the shapes the editor writes and refuse the mistakes in its own words. The scratch project is the committed
fixture tests/fixtures/srpg-doors (map "market"). Called from editor_api_test.py; uses its helpers."""
import copy
import json
import os
import shutil


def run_behaviour_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "behaviour-test"
    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    shutil.copytree(os.path.join(repo, "tests", "fixtures", "srpg-doors"), os.path.join(games, pid))
    dst = os.path.join(games, pid)

    def read(rel):
        with open(os.path.join(dst, rel), "r", encoding="utf-8") as f:
            return json.load(f)

    def problems():
        st, p = api.json("GET", "/api/projects/%s/problems" % pid)
        return p["problems"] if st == 200 else [{"message": "HTTP %s" % st}]

    def save_map(mutate):
        st, d = get_project(api, pid)
        maps = copy.deepcopy(d["maps"])
        mutate(maps["market"])
        st, res = put_project(api, pid, d, d["revision"], maps=maps, structures=d["structures"], items=d["items"], abilities=d["abilities"])
        return st, res

    st, d = get_project(api, pid)
    check("behaviour: the fixture opens and loads without problems", st == 200 and problems() == [])

    # Every shape the editor writes is accepted by the real loader and survives a save unchanged.
    def good(m):
        r1, r2, r3 = m["units"][1], m["units"][2], m["units"][3]
        r1["behaviour"] = "hold"
        r2["behaviour"] = {"mode": "guard", "wake": "react", "group": "pack", "fromTurn": 3, "prefer": "weakest"}
        r3["behaviour"] = {"mode": "seek", "place": "c1", "prefer": {"unit": "lord"}}
        m["units"].append({"type": "raider", "id": "r4", "side": "red", "x": 6, "y": 2, "items": [],
                           "behaviour": {"mode": "guardArea", "area": {"x": 4, "y": 0, "w": 2, "h": 3}, "within": 3, "group": "pack"}})
        m["units"].append({"type": "raider", "id": "r5", "side": "red", "x": 7, "y": 2, "items": [], "behaviour": {"mode": "seek", "x": 3, "y": 4}})
        m["events"] = [{"id": "ambush", "when": {"turn": 2}, "do": [
            {"setBehaviour": {"mode": "seek", "unit": "lord"}, "units": {"unit": "r1"}},
            {"setBehaviour": "advance", "units": {"side": "red"}},
            {"wakeGroup": "pack"},
            {"addUnit": {"type": "raider", "side": "red", "x": 9, "y": 1, "items": [], "behaviour": "hold"}}]}]

    st, res = save_map(good)
    check("behaviour: a map with every behaviour shape is saved", st == 200, (st, res))
    check("behaviour: the real loader accepts it", problems() == [], problems())
    saved = read("maps/market.json")
    check("behaviour: the units keep their blocks in the file",
          saved["units"][1]["behaviour"] == "hold" and saved["units"][2]["behaviour"]["group"] == "pack" and saved["units"][4]["behaviour"]["area"] == {"x": 4, "y": 0, "w": 2, "h": 3}, saved["units"][1:5])
    check("behaviour: the commands keep their blocks in the file",
          saved["events"][0]["do"][0] == {"setBehaviour": {"mode": "seek", "unit": "lord"}, "units": {"unit": "r1"}} and saved["events"][0]["do"][2] == {"wakeGroup": "pack"}, saved["events"][0])
    st, d = get_project(api, pid)
    check("behaviour: GET carries them", d["maps"]["market"]["units"][2]["behaviour"]["wake"] == "react")
    before = read("maps/market.json")
    st, res = put_project(api, pid, d, d["revision"], maps=d["maps"], structures=d["structures"], items=d["items"], abilities=d["abilities"])
    check("behaviour: a load and a save changes nothing", st == 200 and read("maps/market.json") == before)

    # Mistakes are refused by the loader in its words (the editor's own checks say them earlier).
    cases = [
        ("an unknown mode", lambda m: m["units"][1].update(behaviour="charge"), "\"behaviour\" must be \"advance\", \"hold\", \"guard\", \"guardArea\" or \"seek\""),
        ("a seek of a unit that is not there", lambda m: m["units"][1].update(behaviour={"mode": "seek", "unit": "ghost"}), "unknown unit \"ghost\""),
        ("a seek of a place that is not there", lambda m: m["units"][1].update(behaviour={"mode": "seek", "place": "tower"}), "not a structure of this map"),
        ("a start turn past the bound", lambda m: m["units"][1].update(behaviour={"fromTurn": 100}), "from 1 to 99"),
        ("a behaviour on a unit of a side a person plays", lambda m: m["units"][0].update(behaviour="hold"), "this unit is on a side a person plays"),
        ("a wakeGroup of a group nobody is in", lambda m: m.update(events=[{"id": "e", "when": {"turn": 2}, "do": [{"wakeGroup": "nobody"}]}]), "no unit of this map is in a group"),
    ]
    for what, mutate, words in cases:
        st, d = get_project(api, pid)
        maps = copy.deepcopy(d["maps"])
        market = copy.deepcopy(read("maps/market.json"))
        mutate(market)
        maps["market"] = market
        st, res = put_project(api, pid, d, d["revision"], maps=maps, structures=d["structures"], items=d["items"], abilities=d["abilities"])
        check("behaviour: %s is refused in plain words" % what, any(words in x["message"] for x in problems()), problems())
        # put the good map back for the next case
        st, d = get_project(api, pid)
        maps = copy.deepcopy(d["maps"])
        maps["market"] = json.loads(json.dumps(before))
        put_project(api, pid, d, d["revision"], maps=maps, structures=d["structures"], items=d["items"], abilities=d["abilities"])
    check("behaviour: and the good map is accepted again", problems() == [], problems())
