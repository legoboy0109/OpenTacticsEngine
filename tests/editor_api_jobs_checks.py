"""The job system through the ote-editor API (docs/JOBS.md): data/jobs.json (the list under "jobs"), game.json "rules"."jobs", a character's and a map unit's job keys come back
with GET, are written back unchanged by PUT, and the real loader (GET .../problems) accepts the data and names what it refuses. No editor form exists for them (the editor is on
hold); this only proves the server carries them. Starts from a copy of tests/fixtures/jobs. Called from editor_api_test.py; uses its helpers."""
import json
import os
import shutil

FIXTURE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fixtures", "jobs")


def run_jobs_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "jobs-api"
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
    check("jobs: the fixture loads in the real loader", st == 200 and problems() == [], problems())
    jobs = d.get("jobs")
    check("jobs: GET carries the list", isinstance(jobs, list) and [j["id"] for j in jobs] == ["squire", "knight", "mage"], jobs)
    check("jobs: ... with the requirements, level table and learn entries",
          jobs[1]["requires"] == [{"job": "squire", "level": 2}] and jobs[2]["jpLevels"] == [30, 90]
          and jobs[0]["learn"][1] == {"ability": "shove", "cost": 40, "slot": "action"}, jobs)
    jr = d["game"]["rules"]["jobs"]
    check("jobs: GET carries rules.jobs", jr["jp"] == {"perAction": 10, "perDefeat": 20} and jr["slots"]["support"] == 1 and jr["changeIn"] == "camp+prep", jr)
    chars = {c["id"]: c for c in d["characters"]}
    check("jobs: GET carries a character's job keys", chars["cora"]["job"] == "mage" and chars["cora"]["equipped"] == ["guard"]
          and chars["cora"]["jobs"]["squire"] == {"jp": 25, "learned": ["guard"]}, chars["cora"])
    units = {u.get("id"): u for u in d["maps"]["duel"]["units"]}
    check("jobs: GET carries a map unit's job keys", units["shaman"]["job"] == "mage" and units["shaman"]["learned"] == ["mend"], units["shaman"])

    st, res = put_project(api, pid, d, d["revision"], jobs=d["jobs"], characters=d["characters"])
    check("jobs: open + save keeps the file and every key", st == 200 and raw("data/jobs.json").count(b'"learn"') == 3
          and raw("data/characters.json").count(b'"equipped"') == 1 and raw("maps/duel.json").count(b'"learned"') == 1, st)
    st, d2 = get_project(api, pid)
    check("jobs: ... the problems are still none and the data read back is the same",
          problems() == [] and d2["jobs"] == d["jobs"] and d2["characters"] == d["characters"] and d2["game"] == d["game"] and d2["maps"] == d["maps"], "differs")

    def refused(label, change, needle):
        st, cur = get_project(api, pid)
        body = {"game": copy(cur["game"]), "maps": copy(cur["maps"]), "jobs": copy(cur["jobs"]), "characters": copy(cur["characters"])}
        change(body)
        st, res = put_project(api, pid, cur, cur["revision"], **body)
        ps = problems()
        check("jobs: %s is named by the loader" % label, st == 200 and any(needle in p for p in ps), (st, ps))
        st, cur = get_project(api, pid)
        put_project(api, pid, cur, cur["revision"], game=copy(d["game"]), maps=copy(d["maps"]), jobs=copy(d["jobs"]), characters=copy(d["characters"]))  # back to valid

    refused("an unknown unit type", lambda b: b["jobs"][0].__setitem__("unitType", "ghost"), "names unknown unit type \"ghost\"")
    refused("an unknown skill", lambda b: b["jobs"][0]["learn"][0].__setitem__("skill", "nosuch"), "names unknown skill \"nosuch\"")
    refused("a requirement cycle", lambda b: b["jobs"][0].__setitem__("requires", [{"job": "knight", "level": 1}]), "goes round in a circle")
    refused("a skill taught twice", lambda b: b["jobs"][1]["learn"][0].__setitem__("skill", "guard"), "is listed twice")
    refused("a starting learned entry the job does not teach", lambda b: b["characters"][2]["jobs"]["squire"].__setitem__("learned", ["mend"]), "does not teach")
    refused("equipped beyond the slots", lambda b: b["game"]["rules"]["jobs"]["slots"].__setitem__("support", 0), "more equipped entries than support slots (0)")
    refused("a map unit's job that does not exist", lambda b: b["maps"]["duel"]["units"][5].__setitem__("job", "jester"), "names unknown job \"jester\"")

    # Removing the list removes the file; the game then refuses the rules.jobs block and the job keys (the plain form of "jobs keys without jobs.json").
    st, cur = get_project(api, pid)
    st, res = put_project(api, pid, cur, cur["revision"], jobs=False)
    ps = problems()
    check("jobs: \"jobs\": false removes the file and the loader says rules.jobs needs it", st == 200 and not os.path.exists(os.path.join(root, "data/jobs.json"))
          and any("rules.jobs: needs data/jobs.json" in p for p in ps), (st, ps))
    st, cur = get_project(api, pid)
    put_project(api, pid, cur, cur["revision"], jobs=copy(d["jobs"]))
    st, cur = get_project(api, pid)
    check("jobs: written valid again", problems() == [] and cur["jobs"] == d["jobs"], problems())
