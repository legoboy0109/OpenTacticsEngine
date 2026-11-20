"""Merit ranks and overlay jobs through the ote-editor API (docs/SRPG.md "Unit ranks" merit mode, docs/JOBS.md "Class jobs and overlay jobs"): game.json "rules"."ranks", a map unit's
"merit", an overlay job without "unitType" and a job's "bonus" come back with GET, are written back unchanged by PUT, and the real loader (GET .../problems) accepts the data and names
what it refuses. No editor form exists for them; this only proves the server carries them. Starts from a copy of tests/fixtures/combo. Called from editor_api_test.py."""
import json
import os
import shutil

FIXTURE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fixtures", "combo")


def run_merit_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "merit-api"
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
    check("merit: the combo fixture loads in the real loader", st == 200 and problems() == [], problems())
    ranks = d["game"]["rules"].get("ranks", {})
    check("merit: GET carries rules.ranks", ranks.get("name") == "Merit" and ranks.get("perHit") == 5 and ranks.get("perDefeat") == 15
          and [l["name"] for l in ranks.get("levels", [])] == ["Veteran", "Elite"], ranks)
    jobs = {j["id"]: j for j in d["jobs"]}
    check("merit: GET carries an overlay job (no unitType) and a job's bonus", "unitType" not in jobs["scout"] and jobs["scout"]["bonus"] == {"str": 1, "mov": 1}
          and jobs["squire"]["unitType"] == "squire", jobs["scout"])
    first = {u.get("character"): u for u in d["maps"]["first"]["units"] if "character" in u}
    check("merit: GET carries a map unit's merit", first["aldric"]["merit"] == 8, first["aldric"])

    st, res = put_project(api, pid, d, d["revision"], jobs=d["jobs"], characters=d["characters"])
    counts = (raw("game.json").count(b'"perHit"'), raw("data/jobs.json").count(b'"bonus"'), raw("maps/first.json").count(b'"merit"'))
    check("merit: open + save keeps every key in the files", st == 200 and counts == (1, 2, 1), (st, counts))
    st, d2 = get_project(api, pid)
    check("merit: ... the problems are still none and the data read back is the same",
          problems() == [] and d2["jobs"] == d["jobs"] and d2["game"] == d["game"] and d2["maps"] == d["maps"], "differs")

    def refused(label, change, needle):
        st, cur = get_project(api, pid)
        body = {"game": copy(cur["game"]), "maps": copy(cur["maps"]), "jobs": copy(cur["jobs"]), "characters": copy(cur["characters"])}
        change(body)
        st, res = put_project(api, pid, cur, cur["revision"], **body)
        ps = problems()
        check("merit: %s is named by the loader" % label, st == 200 and any(needle in p for p in ps), (st, ps))
        st, cur = get_project(api, pid)
        put_project(api, pid, cur, cur["revision"], game=copy(d["game"]), maps=copy(d["maps"]), jobs=copy(d["jobs"]), characters=copy(d["characters"]))  # back to valid

    refused("an unknown key in the block", lambda b: b["game"]["rules"]["ranks"].__setitem__("colour", 1), "rules.ranks: unknown key \"colour\"")
    def both(b):
        prog = b["game"]["rules"]["progression"]
        del prog["xpPerLevel"], prog["maxLevel"]
        prog["levels"] = [{"at": 5, "name": "x"}]
    refused("a table beside progression.levels", both, "rules.ranks: \"rules.progression\" already has a rank table")
    refused("a merit above the top rank", lambda b: b["maps"]["first"]["units"][0].__setitem__("merit", 99), "\"merit\" must be a whole number from 0 to 30")
    refused("an unknown stat in a job's bonus", lambda b: b["jobs"][3].__setitem__("bonus", {"luck": 1}), "\"bonus\" names unknown stat \"luck\"")

    def drop_type(b):
        for u in b["maps"]["duel"]["units"]:
            if u.get("id") == "raider":
                del u["type"]
    refused("an overlay-job unit without a type", drop_type, "keeps the unit's own class")
    st, cur = get_project(api, pid)
    check("merit: written valid again", problems() == [] and cur["game"] == d["game"], problems())
