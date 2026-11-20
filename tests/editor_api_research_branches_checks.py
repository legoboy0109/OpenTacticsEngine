"""Branching research through the ote-editor API (docs/ECONOMY.md "Branching research"): a technology's "tree", "tier", "exclusiveGroup",
"requiresStructures", "bonus.income" and "grants" and game.json's "rules.research.cancelRefundPercent" are passed through like every other key:
they open and save without loss, the real loader (GET .../problems) accepts them and names what it refuses. Starts from a copy of
tests/fixtures/research-branches. Called from editor_api_test.py; uses its helpers."""
import json
import os
import shutil

FIXTURE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fixtures", "research-branches")


def run_research_branches_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "research-branches-api"
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
    check("research branches: the fixture loads in the real loader", st == 200 and problems() == [], problems())
    tech = {t["id"]: t for t in d["research"]}
    check("research branches: GET carries the keys",
          tech["shield_wall"]["tree"] == "Military" and tech["shield_wall"]["tier"] == 2 and tech["shield_wall"]["exclusiveGroup"] == "doctrine"
          and tech["smelting"]["requiresStructures"] == ["forge"] and tech["trade"]["bonus"]["income"] == {"gold": 30}
          and tech["skirmish"]["grants"]["skills"][0]["filter"] == {"type": "archer"}
          and d["game"]["rules"]["research"]["cancelRefundPercent"] == 50, "missing keys")

    files = ["game.json", "data/research.json"]
    before = {f: raw(f) for f in files}
    st, res = put_project(api, pid, d, d["revision"])
    after = {f: raw(f) for f in files}
    check("research branches: open + save keeps every key in the files",
          st == 200 and all(k in after["data/research.json"] for k in (b'"tree"', b'"tier"', b'"exclusiveGroup"', b'"requiresStructures"', b'"income"', b'"grants"'))
          and b'"cancelRefundPercent"' in after["game.json"], (st, [f for f in files if after[f] != before[f]]))
    st, d2 = get_project(api, pid)
    check("research branches: ... the problems are still none and the data read back is the same",
          problems() == [] and d2["research"] == d["research"] and d2["game"] == d["game"], "differs")

    def refused(label, change, needle):
        st, cur = get_project(api, pid)
        body = {"game": copy(cur["game"]), "research": copy(cur["research"])}
        change(body)
        st, res = put_project(api, pid, cur, cur["revision"], **body)
        ps = problems()
        check("research branches: %s is named by the loader" % label, st == 200 and any(needle in p for p in ps), (st, ps))
        st, cur = get_project(api, pid)
        put_project(api, pid, cur, cur["revision"], game=copy(d["game"]), research=copy(d["research"]))  # back to valid

    def entry(b, tid):
        return next(t for t in b["research"] if t["id"] == tid)

    refused("an unknown structure", lambda b: entry(b, "smelting").__setitem__("requiresStructures", ["castle"]), "\"requiresStructures\" names unknown structure \"castle\"")
    refused("a tier that is not whole", lambda b: entry(b, "tactics").__setitem__("tier", 1.5), "\"tier\" must be a whole number")
    refused("an unknown granted skill", lambda b: entry(b, "shield_wall")["grants"]["skills"].__setitem__(0, "nope"), "names unknown skill \"nope\"")
    refused("an income of an unknown resource", lambda b: entry(b, "trade")["bonus"].__setitem__("income", {"mana": 1}), "names unknown resource \"mana\"")
    refused("a refund above 100", lambda b: b["game"]["rules"]["research"].__setitem__("cancelRefundPercent", 101), "\"cancelRefundPercent\" must be a whole number from 0 to 100")

    check("research branches: valid data again -> no problems", problems() == [], problems())
