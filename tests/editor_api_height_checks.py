"""The height rules through the ote-editor API (docs/SRPG.md "Height rules"): game.json "rules"."height", a weapon's "heightRange" and an area's
"vertical", a unit type's "jump" and a map's heights come back with GET, are written back unchanged by PUT, and the real loader (GET .../problems)
accepts the data and names what it refuses. No editor form exists for them (the editor is on hold); this only proves the server carries them.
Starts from a copy of tests/fixtures/height-rules. Called from editor_api_test.py; uses its helpers."""
import json
import os
import shutil

FIXTURE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fixtures", "height-rules")


def run_height_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "height-api"
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
    check("height: the fixture loads in the real loader", st == 200 and problems() == [], problems())
    game = d["game"]
    h = game["rules"]["height"]
    check("height: GET carries the rules block",
          h["jump"] == {"default": 1} and h["flyingMoveTypes"] == ["air"] and h["rangeBonus"] == {"perLevels": 2, "max": 2} and h["lineOfSight"] is True, game["rules"])
    w = {x["id"]: x for x in d["weapons"]}
    u = {x["id"]: x for x in d["units"]}
    check("height: GET carries a weapon's heightRange, an area's vertical and a unit type's jump",
          w["sling"]["heightRange"] == {"up": 0} and w["pike"]["heightRange"] == {"up": 1, "down": 1} and w["bomb"]["area"]["vertical"] == 1
          and u["goat"]["jump"] == {"up": 3, "down": 1} and u["stiff"]["jump"] == 0 and u["diver"]["jump"] == {"down": 5} and "jump" not in u["lancer"], (w["sling"], u["goat"]))

    st, res = put_project(api, pid, d, d["revision"])
    check("height: open + save keeps every key in the files",
          st == 200 and b'"height"' in raw("game.json") and b'"rangeBonus"' in raw("game.json") and b'"heightRange"' in raw("data/weapons.json")
          and b'"vertical"' in raw("data/weapons.json") and b'"jump"' in raw("data/units.json"), st)
    st, d2 = get_project(api, pid)
    check("height: ... the problems are still none and the data read back is the same",
          problems() == [] and d2["game"] == d["game"] and d2["weapons"] == d["weapons"] and d2["units"] == d["units"], "differs")
    maps = {m["id"]: m for m in d2.get("maps", [])} if isinstance(d2.get("maps"), list) else {}
    cliffs = maps.get("cliffs")
    if cliffs is not None and isinstance(cliffs, dict):
        check("height: a map's heights come back", "heights" in json.dumps(cliffs), list(cliffs)[:8])

    def refused(label, change, needle):
        st, cur = get_project(api, pid)
        body = {"game": copy(cur["game"]), "weapons": copy(cur["weapons"]), "units": copy(cur["units"])}
        change(body["game"], {x["id"]: x for x in body["weapons"]}, {x["id"]: x for x in body["units"]})
        st, res = put_project(api, pid, cur, cur["revision"], **body)
        ps = problems()
        check("height: %s is named by the loader" % label, st == 200 and any(needle in p for p in ps), (st, ps))
        st, cur = get_project(api, pid)
        put_project(api, pid, cur, cur["revision"], game=copy(d["game"]), weapons=copy(d["weapons"]), units=copy(d["units"]))  # back to valid

    refused("a bad range bonus", lambda g, w, u: g["rules"]["height"]["rangeBonus"].__setitem__("perLevels", 0), "\"perLevels\" must be a whole number from 1 to 999")
    refused("an unknown flying move type", lambda g, w, u: g["rules"]["height"].__setitem__("flyingMoveTypes", ["wing"]), "unknown move type \"wing\"")
    refused("an unknown key of the block", lambda g, w, u: g["rules"]["height"].__setitem__("fall", 3), "unknown key \"fall\"")
    refused("a negative heightRange", lambda g, w, u: w["sling"]["heightRange"].__setitem__("up", -1), "\"up\" must be a whole number from 0 to 999")
    refused("a negative vertical", lambda g, w, u: w["bomb"]["area"].__setitem__("vertical", -2), "\"vertical\" must be a whole number from 0 to 999")
    refused("a jump that is not a number", lambda g, w, u: u["stiff"].__setitem__("jump", "high"), "\"jump\" must be a whole number")

    def without_rules(g, w, u):
        del g["rules"]["height"]
    refused("a height key in a game without the height rules", without_rules, "needs \"rules\": { \"height\": ... }")
    st, cur = get_project(api, pid)
    check("height: written valid again", problems() == [] and cur["game"] == d["game"], problems())
