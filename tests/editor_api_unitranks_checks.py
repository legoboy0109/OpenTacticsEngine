"""Unit ranks through the ote-editor API (docs/SRPG.md "Unit ranks"): game.json "rules"."progression"."levels" and a map unit's "xp" come back with GET, are
written back unchanged by PUT, and the real loader (GET .../problems) accepts the data and names what it refuses. No editor form exists for them (the editor
is on hold); this only proves the server carries them. Starts from a copy of tests/fixtures/unit-ranks. Called from editor_api_test.py; uses its helpers."""
import json
import os
import shutil

FIXTURE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fixtures", "unit-ranks")


def run_unitranks_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "unitranks-api"
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
    check("unit ranks: the fixture loads in the real loader", st == 200 and problems() == [], problems())
    levels = d["game"]["rules"]["progression"]["levels"]
    check("unit ranks: GET carries the table", [l["name"] for l in levels] == ["Veteran", "Elite", "Hero"] and levels[1]["skills"] == ["steady"]
          and levels[1]["bonus"] == {"str": 2, "def": 1, "hp": 6}, levels)
    units = {u["id"]: u for u in d["maps"]["duel"]["units"] if "id" in u}
    check("unit ranks: GET carries a map unit's xp", units["vet"]["xp"] == 10 and units["elite"]["xp"] == 30, units)

    st, res = put_project(api, pid, d, d["revision"])
    check("unit ranks: open + save keeps every key in the files",
          st == 200 and raw("game.json").count(b'"badge"') == 3 and raw("maps/duel.json").count(b'"xp"') == 2, st)
    st, d2 = get_project(api, pid)
    check("unit ranks: ... the problems are still none and the data read back is the same",
          problems() == [] and d2["game"] == d["game"] and d2["maps"] == d["maps"], "differs")

    def refused(label, change, needle):
        st, cur = get_project(api, pid)
        body = {"game": copy(cur["game"]), "maps": copy(cur["maps"])}
        change(body["game"], body["maps"])
        st, res = put_project(api, pid, cur, cur["revision"], **body)
        ps = problems()
        check("unit ranks: %s is named by the loader" % label, st == 200 and any(needle in p for p in ps), (st, ps))
        st, cur = get_project(api, pid)
        put_project(api, pid, cur, cur["revision"], game=copy(d["game"]), maps=copy(d["maps"]))  # back to valid

    def table(g):
        return g["rules"]["progression"]["levels"]

    refused("an entry that does not rise", lambda g, m: table(g)[1].__setitem__("at", 10), "\"at\" must be higher than the one before it")
    refused("an unknown skill", lambda g, m: table(g)[1].__setitem__("skills", ["nosuch"]), "names unknown skill \"nosuch\"")
    refused("an unknown stat", lambda g, m: table(g)[0].__setitem__("bonus", {"luck": 1}), "names unknown stat \"luck\"")
    refused("levelling keys beside the table", lambda g, m: g["rules"]["progression"].__setitem__("maxLevel", 5), "\"maxLevel\" belongs to levelling")
    refused("a map unit's xp above the top rank", lambda g, m: m["duel"]["units"][1].__setitem__("xp", 99), "\"xp\" must be a whole number from 0 to 60")

    def without_table(g, m):
        del g["rules"]["progression"]["levels"]
    refused("a map unit's xp in a game without the table", without_table, "\"xp\" needs a rank table")
    st, cur = get_project(api, pid)
    check("unit ranks: written valid again", problems() == [] and cur["game"] == d["game"], problems())
