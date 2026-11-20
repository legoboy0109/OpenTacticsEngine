"""Facing through the ote-editor API (docs/SRPG.md "Facing"): game.json "rules"."facing" and a map unit's "facing" come back with GET, are written back
unchanged by PUT, and the real loader (GET .../problems) accepts the data and names what it refuses. No editor form exists for them (the editor is on
hold); this only proves the server carries them. Starts from a copy of tests/fixtures/facing. Called from editor_api_test.py; uses its helpers."""
import json
import os
import shutil

FIXTURE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fixtures", "facing")


def run_facing_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "facing-api"
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
    check("facing: the fixture loads in the real loader", st == 200 and problems() == [], problems())
    game = d["game"]
    check("facing: GET carries the rules block", game["rules"]["facing"] == {"arcs": {"back": {"name": "Backstab"}}}, game["rules"])
    units = {u["id"]: u for u in d["maps"]["duel"]["units"]}
    check("facing: GET carries a map unit's facing", units["lan"]["facing"] == "east" and units["foe"]["facing"] == "north" and units["far"]["facing"] == "south", units)

    st, res = put_project(api, pid, d, d["revision"])
    check("facing: open + save keeps every key in the files",
          st == 200 and b'"facing"' in raw("game.json") and b'"Backstab"' in raw("game.json") and raw("maps/duel.json").count(b'"facing"') == 4, st)
    st, d2 = get_project(api, pid)
    check("facing: ... the problems are still none and the data read back is the same",
          problems() == [] and d2["game"] == d["game"] and d2["maps"] == d["maps"], "differs")

    def refused(label, change, needle):
        st, cur = get_project(api, pid)
        body = {"game": copy(cur["game"]), "maps": copy(cur["maps"])}
        change(body["game"], body["maps"])
        st, res = put_project(api, pid, cur, cur["revision"], **body)
        ps = problems()
        check("facing: %s is named by the loader" % label, st == 200 and any(needle in p for p in ps), (st, ps))
        st, cur = get_project(api, pid)
        put_project(api, pid, cur, cur["revision"], game=copy(d["game"]), maps=copy(d["maps"]))  # back to valid

    def bad_direction(g, m):
        m["duel"]["units"][0]["facing"] = "northeast"  # a square map has four directions
    refused("a direction the grid does not have", bad_direction, "\"facing\" must be one of \"north\", \"east\", \"south\", \"west\"")
    refused("an unknown key of the block", lambda g, m: g["rules"]["facing"].__setitem__("speed", 3), "unknown key \"speed\"")
    refused("an unknown arc", lambda g, m: g["rules"]["facing"]["arcs"].__setitem__("behind", {"name": "x"}), "unknown key \"behind\"")
    refused("an empty arc name", lambda g, m: g["rules"]["facing"]["arcs"]["back"].__setitem__("name", ""), "\"name\" must be 1 to 16 characters")

    def without_rules(g, m):
        del g["rules"]["facing"]
    refused("a map unit's facing in a game without the rules", without_rules, "needs \"rules\": { \"facing\": ... }")
    st, cur = get_project(api, pid)
    check("facing: written valid again", problems() == [] and cur["game"] == d["game"], problems())
