"""The speed turn order and charged actions through the ote-editor API (docs/TURN_ORDER.md): game.json "rules"."turnOrder" (the object form), a weapon's and an ability's
"chargeSpeed" / "chargeTargets" / "interruptedBy" come back with GET, are written back unchanged by PUT, and the real loader (GET .../problems) accepts the data and names
what it refuses. No editor form exists for them (the editor is on hold); this only proves the server carries them. Starts from a copy of tests/fixtures/speed-order.
Called from editor_api_test.py; uses its helpers."""
import json
import os
import shutil

FIXTURE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fixtures", "speed-order")


def run_speed_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "speed-api"
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
    check("speed: the fixture loads in the real loader", st == 200 and problems() == [], problems())
    game = d["game"]
    order = game["rules"]["turnOrder"]
    check("speed: GET carries the turnOrder block", order == {"mode": "speed", "stat": "spd", "threshold": 100, "preview": 8}, order)
    w = {x["id"]: x for x in d["weapons"]}
    a = {x["id"]: x for x in d["abilities"]}
    check("speed: GET carries a weapon's chargeSpeed and interruptedBy and an ability's chargeSpeed and chargeTargets",
          w["fire"]["chargeSpeed"] == 25 and w["fire"]["interruptedBy"] == ["sleep"] and a["hush"]["chargeSpeed"] == 25 and a["hush"]["chargeTargets"] == "cell", (w["fire"], a["hush"]))

    st, res = put_project(api, pid, d, d["revision"])
    check("speed: open + save keeps every key in the files",
          st == 200 and b'"turnOrder"' in raw("game.json") and b'"chargeSpeed"' in raw("data/weapons.json") and b'"interruptedBy"' in raw("data/weapons.json")
          and b'"chargeTargets"' in raw("data/abilities.json"), st)
    st, d2 = get_project(api, pid)
    check("speed: ... the problems are still none and the data read back is the same",
          problems() == [] and d2["game"] == d["game"] and d2["weapons"] == d["weapons"] and d2["abilities"] == d["abilities"], "differs")

    def refused(label, change, needle):
        st, cur = get_project(api, pid)
        body = {"game": copy(cur["game"]), "weapons": copy(cur["weapons"]), "abilities": copy(cur["abilities"])}
        change(body["game"], {x["id"]: x for x in body["weapons"]}, {x["id"]: x for x in body["abilities"]})
        st, res = put_project(api, pid, cur, cur["revision"], **body)
        ps = problems()
        check("speed: %s is named by the loader" % label, st == 200 and any(needle in p for p in ps), (st, ps))
        st, cur = get_project(api, pid)
        put_project(api, pid, cur, cur["revision"], game=copy(d["game"]), weapons=copy(d["weapons"]), abilities=copy(d["abilities"]))  # back to valid

    refused("an unknown clock stat", lambda g, w, a: g["rules"]["turnOrder"].__setitem__("stat", "haste"), "unknown stat \"haste\"")
    refused("a threshold of 0", lambda g, w, a: g["rules"]["turnOrder"].__setitem__("threshold", 0), "\"threshold\" must be a whole number from 1")
    refused("an unknown key of the block", lambda g, w, a: g["rules"]["turnOrder"].__setitem__("speedy", 1), "unknown key \"speedy\"")
    refused("a turn that costs nothing", lambda g, w, a: g["rules"]["turnOrder"].__setitem__("ctCost", {"both": 100, "one": 0, "none": 60}), "every cost must be at least 1")
    refused("a charge speed out of range", lambda g, w, a: w["fire"].__setitem__("chargeSpeed", 5000), "\"chargeSpeed\" must be a whole number from 1 to 1000")
    refused("a charge target that is neither unit nor cell", lambda g, w, a: a["hush"].__setitem__("chargeTargets", "area"), "\"chargeTargets\" must be \"unit\"")
    refused("an interruptedBy condition that is not there", lambda g, w, a: w["fire"].__setitem__("interruptedBy", ["nap"]), "unknown condition \"nap\"")
    refused("a free charged ability", lambda g, w, a: a["mend"].__setitem__("cost", "free"), "needs \"cost\": \"action\"")

    def with_ap(g, w, a):
        g["rules"]["actionPoints"] = {"resource": "gold"}
    refused("speed together with action points", with_ap, "cannot be combined with \"actionPoints\"")

    def without_rule(g, w, a):
        del g["rules"]["turnOrder"]
    refused("chargeSpeed in a game without the speed rule", without_rule, "need \"rules\": { \"turnOrder\": \"speed\" }")
    st, cur = get_project(api, pid)
    check("speed: written valid again", problems() == [] and cur["game"] == d["game"], problems())
