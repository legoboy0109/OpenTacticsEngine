"""Commanders through the ote-editor API (docs/COMMANDERS.md): data/commanders.json comes back as "commanders" (null when the file is absent), is
written back like the other lists (an empty list removes the file), the map side's "commander" key and the faction's "commanders" list pass through, and
the real loader (GET .../problems) accepts the data and names what it refuses. There is no editor form: only the file and its keys must survive.
Starts from a copy of tests/fixtures/commanders. Called from editor_api_test.py; uses its helpers."""
import json
import os
import shutil

FIXTURE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fixtures", "commanders")


def run_commanders_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "commanders-api"
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
    check("commanders: the fixture loads in the real loader", st == 200 and problems() == [], problems())
    cmd = {c["id"]: c for c in d["commanders"]} if d.get("commanders") else {}
    check("commanders: GET carries the list with every key",
          sorted(cmd) == ["ada", "bram", "cato"] and cmd["ada"]["meter"] == {"max": 100, "perDamageDealt": 2, "perDamageTaken": 1}
          and cmd["ada"]["passive"][0] == {"skill": "bulwark", "filter": {"tag": "infantry"}}
          and cmd["ada"]["powers"][1]["costGrowthPercent"] == 50 and cmd["ada"]["powers"][1]["turns"] == 2
          and cmd["bram"]["powers"][0]["effects"][1] == {"giveResource": {"gold": 25}}
          and cmd["cato"]["powers"][1]["effects"][0]["filter"] == {"tag": "heavy"}, sorted(cmd))
    fac = {f["id"]: f for f in d["factions"]}
    check("commanders: a faction's \"commanders\" and a map side's \"commander\" come back",
          fac["order"]["commanders"] == ["ada", "cato"] and d["maps"]["field"]["sides"][0]["commander"] == "ada"
          and d["maps"]["pick"]["sides"][0]["commander"] == "choose", "missing")

    before = {f: raw(f) for f in ("data/commanders.json", "data/factions.json", "maps/field.json", "maps/pick.json")}
    st, res = put_project(api, pid, d, d["revision"], commanders=copy(d["commanders"]), factions=copy(d["factions"]))
    after = {f: raw(f) for f in before}
    check("commanders: open + save keeps every key in the files",
          st == 200 and all(k in after["data/commanders.json"] for k in (b'"passive"', b'"meter"', b'"perTurn"', b'"powers"', b'"costGrowthPercent"', b'"effects"', b'"giveResource"', b'"refresh"'))
          and b'"commanders"' in after["data/factions.json"] and b'"commander": "choose"' in after["maps/pick.json"]
          and b'"commander": "ada"' in after["maps/field.json"], (st, [f for f in before if after[f] != before[f]]))
    st, d2 = get_project(api, pid)
    check("commanders: ... the problems are still none and the data read back is the same",
          problems() == [] and d2["commanders"] == d["commanders"] and d2["maps"] == d["maps"] and d2["factions"] == d["factions"], "differs")

    def refused(label, change, needle):
        st, cur = get_project(api, pid)
        body = {"commanders": copy(cur["commanders"]), "factions": copy(cur["factions"]), "maps": copy(cur["maps"])}
        change(body)
        st, res = put_project(api, pid, cur, cur["revision"], **body)
        ps = problems()
        check("commanders: %s is named by the loader" % label, st == 200 and any(needle in p for p in ps), (st, ps))
        st, cur = get_project(api, pid)
        put_project(api, pid, cur, cur["revision"], commanders=copy(d["commanders"]), factions=copy(d["factions"]), maps=copy(d["maps"]))  # back to valid

    def commander(b, cid):
        return next(c for c in b["commanders"] if c["id"] == cid)

    refused("an unknown passive skill", lambda b: commander(b, "ada").__setitem__("passive", ["nope"]), "\"passive\": names unknown skill \"nope\"")
    refused("a cost over the meter", lambda b: commander(b, "ada")["powers"][0].__setitem__("cost", 101), "\"cost\" must be from 1 to the meter's \"max\" (100)")
    refused("a bad effect", lambda b: commander(b, "ada")["powers"][0].__setitem__("effects", [{"explode": 1}]), "has \"explode\" but no effect")
    refused("a duplicate id", lambda b: commander(b, "bram").__setitem__("id", "ada"), "duplicate id")
    refused("an unknown key", lambda b: commander(b, "cato").__setitem__("mood", 1), "unknown key \"mood\"")
    refused("a faction listing an unknown commander", lambda b: next(f for f in b["factions"] if f["id"] == "order").__setitem__("commanders", ["zed"]),
            "\"commanders\" names unknown commander \"zed\"")
    refused("a side naming an unknown commander", lambda b: b["maps"]["field"]["sides"][0].__setitem__("commander", "zed"), "\"commander\" names unknown commander \"zed\"")

    # An empty list removes the file (the loader would refuse an empty one); the factions' lists and the maps' keys must go with it.
    st, cur = get_project(api, pid)
    body = {"commanders": [], "factions": copy(cur["factions"]), "maps": copy(cur["maps"])}
    for f in body["factions"]:
        f.pop("commanders", None)
    for m in body["maps"].values():
        for s in m["sides"]:
            s.pop("commander", None)
    st, res = put_project(api, pid, cur, cur["revision"], **body)
    st2, none = get_project(api, pid)
    check("commanders: an empty list removes data/commanders.json and the game still loads",
          st == 200 and not os.path.exists(os.path.join(root, "data", "commanders.json")) and none["commanders"] is None and problems() == [], (st, problems()))
    st, cur = get_project(api, pid)
    put_project(api, pid, cur, cur["revision"], commanders=copy(d["commanders"]), factions=copy(d["factions"]), maps=copy(d["maps"]))
    check("commanders: written again -> valid", problems() == [] and os.path.exists(os.path.join(root, "data", "commanders.json")), problems())
