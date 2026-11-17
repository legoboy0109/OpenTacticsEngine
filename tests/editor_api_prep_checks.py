"""The preparation screen's checks of the ote-editor API test (docs/EVENTS.md "Preparation", docs/EDITOR_UI.md). The helper keeps a
map's "prep" block untouched through a load and a save, and the real game loader (what /problems asks) refuses a bad block in
plain words, naming the file and the map. The scratch project is the committed fixture tests/fixtures/srpg-prep. Called from
editor_api_test.py; uses its helpers."""
import json
import os
import shutil


def run_prep_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "prep-test"
    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    dst = os.path.join(games, pid)
    shutil.copytree(os.path.join(repo, "tests", "fixtures", "srpg-prep"), dst)

    def read(rel):
        with open(os.path.join(dst, rel), "r", encoding="utf-8") as f:
            return json.load(f)

    st, d = get_project(api, pid)
    check("prep: the fixture opens in the helper", st == 200, st)
    st, p = api.json("GET", "/api/projects/%s/problems" % pid)
    check("prep: the real loader accepts it (no problems)", st == 200 and p["problems"] == [], p)
    orchard = d["maps"]["orchard"]
    check("prep: GET carries the map's prep block untouched", orchard["prep"] == read("maps/orchard.json")["prep"], orchard.get("prep"))
    check("prep: the slots, limit, required slot, fixed unit and shop are all there",
          len(orchard["prep"]["slots"]) == 4 and orchard["prep"]["limit"] == 3 and orchard["prep"]["required"] == [{"character": "sera", "slot": 1}]
          and orchard["prep"]["fixed"] == ["guard"] and orchard["prep"]["shop"] == {"sells": ["potion", "tonic"]})
    check("prep: a map without one has no prep key", "prep" not in d["maps"]["meadow"])

    before = {rel: read(rel) for rel in ("game.json", "maps/meadow.json", "maps/orchard.json", "maps/hilltop.json", "data/campaign.json")}
    st, res = put_project(api, pid, d, d["revision"])
    check("prep: saving it back is accepted", st == 200, (st, res))
    for rel in before:
        check("prep: %s is unchanged by a load and a save" % rel, read(rel) == before[rel])

    def bad(name, edit, expect):
        st, d2 = get_project(api, pid)
        m = d2["maps"]["orchard"]
        edit(m["prep"])
        st, res = put_project(api, pid, d2, d2["revision"])
        st, p = api.json("GET", "/api/projects/%s/problems" % pid)
        hit = [x for x in p["problems"] if expect in x["message"] and x["file"] == "maps/orchard.json"]
        check("prep: %s is a problem in maps/orchard.json" % name, bool(hit), p)
        # put the good block back
        st, d3 = get_project(api, pid)
        d3["maps"]["orchard"]["prep"] = before["maps/orchard.json"]["prep"]
        put_project(api, pid, d3, d3["revision"])
        st, p = api.json("GET", "/api/projects/%s/problems" % pid)
        check("prep: ... and the fixed block is accepted again", p["problems"] == [], p)

    bad("a limit beyond the slots", lambda b: b.update(limit=9), "\"limit\" 9 is more than the map's 4 slots")
    bad("a slot outside the map", lambda b: b["slots"].append({"x": 99, "y": 0}), "is outside the map")
    bad("a required character fixed to a slot that does not exist", lambda b: b.update(required=[{"character": "sera", "slot": 8}]), "does not exist")
    bad("a shop item without a price", lambda b: b.update(shop={"sells": ["token"]}), "has no \"price\"")
    bad("a fixed unit the map does not place", lambda b: b.update(fixed=["nobody"]), "does not place a unit with that id")
