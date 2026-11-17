"""The fidelity options through the ote-editor API (docs/SRPG.md section 11, docs/EVENTS.md): game.json "rules.hitRoll" and
"rules.progression" extras, a unit's "maxLevel", an ability's "xp" and "inflict", caps.json "computerCaps", a staff's "xpMiss", the story
conditions "rosterAlive" / "rosterDead" / "inRoster" and the commands "removeUnit" + "hide" and "showUnit" pass through like every other key:
they open and save byte for byte, the real loader (GET .../problems) accepts them and names what it refuses. Starts from a copy of
tests/fixtures/srpg-skillblocks2. Called from editor_api_test.py; uses its helpers."""
import json
import os
import shutil

FIXTURE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fixtures", "srpg-skillblocks2")


def run_fidelity_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "fidelity-api"
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
    check("fidelity: the fixture loads", st == 200 and problems() == [], problems())
    mid = d["game"]["maps"][0]["id"]
    mfile = d["game"]["maps"][0]["file"]
    unit_id = d["maps"][mid]["units"][0].get("id")

    def build():
        game = copy(d["game"])
        game["rules"]["hitRoll"] = "average2"
        game["rules"]["progression"].update({"xpNoDamage": "a.level + 1", "computerGains": False})
        units = copy(d["units"])
        units[0]["maxLevel"] = 0
        m = copy(d["maps"][mid])
        m["events"] = [{"id": "away", "when": {"battle": "start"}, "do": [
            {"removeUnit": {"unit": unit_id}, "hide": True},
            {"if": [{"rosterAlive": "kara"}, {"rosterDead": "dax", "party": "active"}, {"inRoster": "kara", "party": "any"}],
             "then": [{"say": "She lives."}]},
            {"showUnit": unit_id, "x": 1, "y": 1}]}]
        return game, units, m

    game, units, m = build()
    st, res = put_project(api, pid, d, d["revision"], game=game, units=units, maps={mid: m})
    check("fidelity: PUT of the new keys -> 200", st == 200 and "revision" in res, (st, res))
    check("fidelity: the real loader accepts them", problems() == [], problems())

    files = [mfile, "game.json", "data/units.json"]
    before = {f: raw(f) for f in files}
    st, d2 = get_project(api, pid)
    check("fidelity: GET returns the keys as written",
          d2["game"]["rules"]["hitRoll"] == "average2" and d2["units"][0]["maxLevel"] == 0 and d2["maps"][mid]["events"] == m["events"], "mismatch")
    st, res = put_project(api, pid, d2, d2["revision"], game=d2["game"], units=d2["units"], maps=d2["maps"])
    after = {f: raw(f) for f in files}
    check("fidelity: open + save keeps every file byte for byte", st == 200 and after == before, [f for f in files if after[f] != before[f]])

    def refused(label, change, needle):
        st, dd = get_project(api, pid)
        g, u, mm = build()
        body = {"game": g, "units": u, "maps": {mid: mm}}
        change(body)
        st, res = put_project(api, pid, dd, dd["revision"], **body)
        ps = problems()
        check("fidelity: %s is named by the loader" % label, st == 200 and any(needle in p for p in ps), ps)

    refused("a hit roll that does not exist", lambda b: b["game"]["rules"].__setitem__("hitRoll", "triple"), "\"hitRoll\" must be \"single\" or \"average2\"")
    refused("a bad xpNoDamage", lambda b: b["game"]["rules"]["progression"].__setitem__("xpNoDamage", -1), "\"xpNoDamage\" must be a whole number")
    refused("a level cap out of range", lambda b: b["units"][0].__setitem__("maxLevel", 100), "\"maxLevel\" must be a whole number from 0")
    refused("a roster condition with a bad party", lambda b: b["maps"][mid]["events"][0]["do"][1]["if"][0].__setitem__("party", "north"), "\"party\" must be \"any\" or \"active\"")
    refused("a roster condition with a bad id", lambda b: b["maps"][mid]["events"][0]["do"][1]["if"][0].__setitem__("rosterAlive", "not an id"), "must be a character id")
    refused("a hide that is not true or false", lambda b: b["maps"][mid]["events"][0]["do"][0].__setitem__("hide", "yes"), "hide")
    refused("a showUnit outside the map", lambda b: b["maps"][mid]["events"][0]["do"][2].__setitem__("x", 999), "outside the map")
    refused("a showUnit of an unknown unit", lambda b: b["maps"][mid]["events"][0]["do"][2].__setitem__("showUnit", "nobody"), "unknown unit")

    st, dd = get_project(api, pid)
    g, u, mm = build()
    st, res = put_project(api, pid, dd, dd["revision"], game=g, units=u, maps={mid: mm})
    check("fidelity: valid data again -> no problems", st == 200 and problems() == [], problems())
