"""The four small additions through the ote-editor API (docs/EVENTS.md "Branching on who acted" and "Sending a unit's items to storage",
docs/SRPG.md "Effects that fade" and "Linking two strike blocks"): a condition's `scale`, the skill effect `mark` and condition `marked`, an event's
`actor` condition and the commands `storeItems` (a map event's and a campaign scene's) are passed through like every other key: they open and save
byte for byte, the real loader (GET .../problems) accepts them and names what it refuses. Starts from a copy of tests/fixtures/srpg-skillblocks2
(skills, conditions, carried items, a field map with named units). Called from editor_api_test.py; uses its helpers."""
import json
import os
import shutil

FIXTURE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fixtures", "srpg-skillblocks2")


def run_extras_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "extras-api"
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
    check("extras: the fixture loads", st == 200 and problems() == [], problems())
    mid = d["game"]["maps"][0]["id"]
    mfile = d["game"]["maps"][0]["file"]
    game = copy(d["game"])
    game["rules"]["items"]["storage"] = "anywhere"
    conditions = copy(d["conditions"]) + [
        {"id": "ward", "name": "Ward", "tag": "Wd", "turns": 4, "scale": "turnsLeft", "blocks": [{"when": "always", "do": [{"stats": {"def": 1}}, {"vision": 1}]}]}]
    skills = copy(d["skills"]) + [
        {"id": "link", "name": "Link", "blocks": [
            {"when": "strike", "if": [{"strike": "first"}], "chance": 30, "do": [{"damage": 3}, {"mark": "hit"}]},
            {"when": "strike", "if": [{"strike": 2}, {"marked": "hit"}], "do": [{"damage": 5}]},
            {"when": "afterCombat", "if": [{"marked": "hit", "is": False}], "do": [{"heal": 1}]}]}]
    m = copy(d["maps"][mid])
    m["events"] = [
        {"id": "who", "when": {"reach": {"x": 3, "y": 0, "w": 1, "h": 3}}, "repeat": True, "if": [{"actor": {"side": "blue"}}], "do": [
            {"if": [{"actor": {"unit": "thief"}}], "then": [{"storeItems": {"unit": "thief"}, "keep": ["iron_sword"]}], "else": [{"storeItems": {"side": "blue"}}]}]},
        {"id": "ask", "when": {"fight": {"side": "blue"}}, "do": [
            {"choice": "Which?", "answers": [{"text": "A", "if": [{"actor": {}}]}, {"text": "B"}]}]},
    ]
    st, res = put_project(api, pid, d, d["revision"], game=game, conditions=conditions, skills=skills, maps={mid: m})
    check("extras: PUT of the new keys -> 200", st == 200 and "revision" in res, (st, res))
    check("extras: the real loader accepts them", problems() == [], problems())

    files = [mfile, "data/conditions.json", "data/skills.json", "game.json"]
    before = {f: raw(f) for f in files}
    st, d = get_project(api, pid)
    ward = next(c for c in d["conditions"] if c["id"] == "ward")
    link = next(s for s in d["skills"] if s["id"] == "link")
    check("extras: GET returns the keys as written",
          ward["scale"] == "turnsLeft" and link["blocks"][0]["do"][1] == {"mark": "hit"} and link["blocks"][1]["if"][1] == {"marked": "hit"}
          and link["blocks"][2]["if"][0] == {"marked": "hit", "is": False} and d["maps"][mid]["events"] == m["events"], "mismatch")
    st, res = put_project(api, pid, d, d["revision"], game=d["game"], conditions=d["conditions"], skills=d["skills"], maps=d["maps"])
    after = {f: raw(f) for f in files}
    check("extras: open + save keeps every file byte for byte", st == 200 and after == before, [f for f in files if after[f] != before[f]])
    check("extras: the keys are in the files", b'"scale"' in after["data/conditions.json"] and b'"mark"' in after["data/skills.json"]
          and b'"storeItems"' in after[mfile] and b'"actor"' in after[mfile])

    # ---- what the loader refuses comes back as problems in its words ----
    def refused(label, change, needle):
        st, d = get_project(api, pid)
        body = {"game": copy(game), "conditions": copy(conditions), "skills": copy(skills), "maps": {mid: copy(m)}}
        change(body)
        st, res = put_project(api, pid, d, d["revision"], **body)
        ps = problems()
        check("extras: %s is named by the loader" % label, st == 200 and any(needle in p for p in ps), ps)

    def cond(b, cid):
        return next(c for c in b["conditions"] if c["id"] == cid)

    def skill(b, sid):
        return next(s for s in b["skills"] if s["id"] == sid)

    def ev(b, eid):
        return next(e for e in b["maps"][mid]["events"] if e["id"] == eid)

    refused("a scale that is not turnsLeft", lambda b: cond(b, "ward").__setitem__("scale", "always"), "\"scale\" must be \"turnsLeft\"")
    refused("a scale on a condition that never counts", lambda b: (cond(b, "ward").pop("turns"), cond(b, "ward").__setitem__("until", "cured")), "needs a condition that counts turns")
    refused("a scale with nothing to scale", lambda b: cond(b, "ward").__setitem__("blocks", [{"when": "combat", "do": [{"hit": 5}]}]), "has nothing to scale")
    refused("a marked in a combat block", lambda b: skill(b, "link")["blocks"].append({"when": "combat", "if": [{"marked": "hit"}], "do": [{"hit": 1}]}), "\"marked\" can only be asked in")
    refused("a mark in a combat block", lambda b: skill(b, "link")["blocks"].append({"when": "combat", "do": [{"mark": "hit"}]}), "\"mark\" cannot be used in a \"combat\" block")
    refused("an empty mark name", lambda b: skill(b, "link")["blocks"][0]["do"].__setitem__(1, {"mark": ""}), "\"mark\" must be the name of a mark")
    refused("a marked with an is that is not a boolean", lambda b: skill(b, "link")["blocks"][2]["if"][0].__setitem__("is", "no"), "is")
    refused("an actor naming a unit that is not there", lambda b: ev(b, "who").__setitem__("if", [{"actor": {"unit": "nobody"}}]), "unknown unit \"nobody\"")
    refused("an actor with an unknown key", lambda b: ev(b, "who").__setitem__("if", [{"actor": {}, "x": 1}]), "unknown key")
    refused("a storeItems naming a unit that is not there", lambda b: ev(b, "who")["do"][0]["then"][0].__setitem__("storeItems", {"unit": "nobody"}), "unknown unit \"nobody\"")
    refused("a storeItems keeping an item that is not there", lambda b: ev(b, "who")["do"][0]["then"][0].__setitem__("keep", ["nothing"]), "\"keep\" names unknown item")
    refused("a storeItems keep that is not a list", lambda b: ev(b, "who")["do"][0]["then"][0].__setitem__("keep", "iron_sword"), "must be a list of item ids")
    refused("a storeItems in a game with no storage", lambda b: b["game"]["rules"]["items"].__setitem__("storage", "off"), "\"storeItems\" needs a shared storage")

    # Back to a valid state: nothing lingers.
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], game=game, conditions=conditions, skills=skills, maps={mid: m})
    check("extras: valid data again -> no problems", st == 200 and problems() == [], problems())

    # A campaign scene: storeItems on a roster id, and the actor refused.
    campaign = {"name": "C", "chapters": [{"id": "c1", "name": "One", "map": mid, "after": [{"storeItems": "thief", "keep": ["iron_sword"]}]}]}
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], campaign=campaign)
    check("extras: a scene's storeItems -> 200", st == 200, (st, res))
    check("extras: the loader accepts a scene's storeItems", problems() == [], problems())
    st, d = get_project(api, pid)
    check("extras: GET returns the scene command as written", d["campaign"]["chapters"][0]["after"] == campaign["chapters"][0]["after"], d.get("campaign"))
    bad = copy(campaign)
    bad["chapters"][0]["after"] = [{"if": [{"actor": {"unit": "thief"}}], "then": [{"say": "x"}]}]
    st, res = put_project(api, pid, d, d["revision"], campaign=bad)
    ps = problems()
    check("extras: a scene's actor is refused in the loader's words", any("\"actor\" is for map events only" in p for p in ps), ps)
    st, d = get_project(api, pid)
    bad["chapters"][0]["after"] = [{"storeItems": {"side": "blue"}}]
    st, res = put_project(api, pid, d, d["revision"], campaign=bad)
    ps = problems()
    check("extras: a scene's storeItems takes a roster id, not a filter", any("roster unit id" in p for p in ps), ps)
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], campaign=campaign)
    check("extras: valid scene again -> no problems", st == 200 and problems() == [], problems())
