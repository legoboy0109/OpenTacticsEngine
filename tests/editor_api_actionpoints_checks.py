"""The action-point turn system and resource refills through the ote-editor API (docs/SRPG.md "Action points", docs/ECONOMY.md "Refilling resources"):
game.json "rules"."actionPoints" and a resource's "refill", a weapon's "apCost" and "area"."apExtra" and a unit type's "ap" come back with GET, are written back
unchanged by PUT, and the real loader (GET .../problems) accepts the data and names what it refuses. No editor form exists for them (the editor is on hold);
this only proves the server carries them. Starts from a copy of tests/fixtures/action-points. Called from editor_api_test.py; uses its helpers."""
import json
import os
import shutil

FIXTURE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fixtures", "action-points")


def run_actionpoints_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "ap-api"
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
    check("actionPoints: the fixture loads in the real loader", st == 200 and problems() == [], problems())
    game = d["game"]
    ap = game["rules"]["actionPoints"]
    check("actionPoints: GET carries the rules block and the resource's refill",
          ap["resource"] == "ap" and ap["move"]["softCap"] == 4 and ap["attack"] == {"cost": 2, "attacksPerTurn": 1}
          and game["resources"][0]["refill"] == {"perTurn": 20, "carryOver": 10}, game["rules"])
    w = {x["id"]: x for x in d["weapons"]}
    u = {x["id"]: x for x in d["units"]}
    check("actionPoints: GET carries a weapon's apCost, an area's apExtra and a unit type's ap",
          w["spear"]["apCost"] == 1 and w["mortar"]["area"]["apExtra"] == 1 and u["scout"]["ap"] == {"step": 2, "softCap": 2}, (w["spear"], u["scout"]))

    st, res = put_project(api, pid, d, d["revision"])
    check("actionPoints: open + save keeps every key in the files",
          st == 200 and b'"actionPoints"' in raw("game.json") and b'"refill"' in raw("game.json") and b'"apCost"' in raw("data/weapons.json")
          and b'"apExtra"' in raw("data/weapons.json") and b'"softCap"' in raw("data/units.json"), st)
    st, d2 = get_project(api, pid)
    check("actionPoints: ... the problems are still none and the data read back is the same",
          problems() == [] and d2["game"] == d["game"] and d2["weapons"] == d["weapons"] and d2["units"] == d["units"], "differs")

    def refused(label, change, needle):
        st, cur = get_project(api, pid)
        body = {"game": copy(cur["game"]), "weapons": copy(cur["weapons"]), "units": copy(cur["units"])}
        change(body["game"], {x["id"]: x for x in body["weapons"]}, {x["id"]: x for x in body["units"]})
        st, res = put_project(api, pid, cur, cur["revision"], **body)
        ps = problems()
        check("actionPoints: %s is named by the loader" % label, st == 200 and any(needle in p for p in ps), (st, ps))
        st, cur = get_project(api, pid)
        put_project(api, pid, cur, cur["revision"], game=copy(d["game"]), weapons=copy(d["weapons"]), units=copy(d["units"]))  # back to valid

    refused("an unknown pool resource", lambda g, w, u: g["rules"]["actionPoints"].__setitem__("resource", "mana"), "unknown resource \"mana\"")
    refused("a negative price per step", lambda g, w, u: g["rules"]["actionPoints"]["move"].__setitem__("costPerStep", -1), "\"costPerStep\" must be a whole number from 0 to 99")
    refused("no attack per turn", lambda g, w, u: g["rules"]["actionPoints"]["attack"].__setitem__("attacksPerTurn", 0), "\"attacksPerTurn\" must be a whole number from 1 to 99")
    refused("a percent out of range", lambda g, w, u: g["rules"]["actionPoints"]["move"].__setitem__("softCapPenaltyPercent", 5000), "\"softCapPenaltyPercent\" must be")
    refused("a negative refill", lambda g, w, u: g["resources"][0]["refill"].__setitem__("perTurn", -5), "\"perTurn\" must be a whole number from 0 to 100000")
    refused("a negative apCost", lambda g, w, u: w["spear"].__setitem__("apCost", -1), "\"apCost\" must be a whole number from 0 to 99")
    refused("an unknown key of a unit's ap", lambda g, w, u: u["scout"]["ap"].__setitem__("speed", 3), "unknown key \"speed\"")

    def without_rules(g, w, u):
        del g["rules"]["actionPoints"]
    refused("apCost in a game without actionPoints", without_rules, "\"apCost\" needs \"rules\"")
    st, cur = get_project(api, pid)
    check("actionPoints: written valid again", problems() == [] and cur["game"] == d["game"], problems())
