"""Downed units and revive through the ote-editor API (docs/SRPG.md "Downed units and revive"): game.json rules.downed, an ability's "revive" effect,
a usable item's and a staff's "revive" and a condition's "autoRevive" come back with GET, are written back unchanged by PUT, and the real loader
(GET .../problems) accepts them and names what it refuses (the same refusals the web editor's checks mirror). Starts from a copy of
tests/fixtures/srpg-conditions with the keys added. Called from editor_api_test.py; uses its helpers."""
import json
import os
import shutil

FIXTURE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fixtures", "srpg-conditions")

DOWNED = {"afterBattle": "fallen", "removeAfter": 2}
ABILITIES = [
    {"id": "raise", "name": "Raise", "target": "ally", "range": [1, 3], "effect": {"revive": {"percent": 50}}},
    {"id": "arise", "name": "Arise", "target": "unit", "range": [0, 3], "effect": {"revive": "full"}, "area": {"shape": "burst"}},
]
ITEMS = [
    {"id": "phoenix", "name": "Phoenix", "kind": "usable", "uses": 1, "target": "ally", "revive": 5},
    {"id": "life_staff", "name": "Life Staff", "kind": "staff", "group": "staff", "uses": 3, "range": 2, "target": "ally", "effects": [{"revive": "a.mag"}]},
]
RERAISE = {"id": "reraise", "name": "Reraise", "kind": "helpful", "until": "cured", "autoRevive": {"percent": 10}, "blocks": []}


def run_downed_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "downed-api"
    root = os.path.join(games, pid)
    shutil.copytree(FIXTURE, root)

    def edit(rel, fn):
        path = os.path.join(root, rel)
        with open(path) as f:
            d = json.load(f)
        fn(d)
        with open(path, "w") as f:
            json.dump(d, f)

    edit("game.json", lambda g: g["rules"].__setitem__("downed", DOWNED))
    edit("data/items.json", lambda d: d["items"].extend(ITEMS))
    edit("data/conditions.json", lambda d: d["conditions"].append(RERAISE))
    with open(os.path.join(root, "data", "abilities.json"), "w") as f:
        json.dump({"abilities": ABILITIES}, f)

    def problems():
        st, res = api.json("GET", "/api/projects/%s/problems" % pid)
        return [p["message"] for p in res["problems"]] if st == 200 else [str(res)]

    def raw(rel):
        with open(os.path.join(root, rel), "rb") as f:
            return f.read()

    def copy(x):
        return json.loads(json.dumps(x))

    st, d = get_project(api, pid)
    check("downed: the fixture with the new keys loads in the real loader", st == 200 and problems() == [], problems())
    items = {x["id"]: x for x in d.get("items", [])}
    conds = {x["id"]: x for x in d.get("conditions", [])}
    abil = {x["id"]: x for x in d.get("abilities", [])}
    check("downed: GET carries rules.downed, revive (ability, item, staff) and autoRevive",
          d["game"]["rules"].get("downed") == DOWNED and abil.get("raise", {}).get("effect") == ABILITIES[0]["effect"]
          and items.get("phoenix", {}).get("revive") == 5 and items.get("life_staff", {}).get("effects") == ITEMS[1]["effects"]
          and conds.get("reraise", {}).get("autoRevive") == {"percent": 10}, (d["game"]["rules"].get("downed"), sorted(abil)))

    st, res = put_project(api, pid, d, d["revision"], items=d["items"], conditions=d["conditions"], abilities=d["abilities"])
    check("downed: open + save keeps every key in the files",
          st == 200 and b'"downed"' in raw("game.json") and b'"removeAfter"' in raw("game.json") and b'"revive"' in raw("data/items.json")
          and b'"autoRevive"' in raw("data/conditions.json") and b'"revive"' in raw("data/abilities.json"), st)
    st, d2 = get_project(api, pid)
    check("downed: ... no problems and the data read back is the same",
          problems() == [] and d2["game"] == d["game"] and d2["items"] == d["items"] and d2["conditions"] == d["conditions"]
          and d2["abilities"] == d["abilities"], "differs")

    def refused(label, change, needle):
        st, cur = get_project(api, pid)
        body = {k: copy(cur[k]) for k in ("game", "items", "conditions", "abilities")}
        change(body)
        st, res = put_project(api, pid, cur, cur["revision"], **body)
        ps = problems()
        check("downed: %s is named by the loader" % label, st == 200 and any(needle in p for p in ps), (st, ps))
        st, cur = get_project(api, pid)
        put_project(api, pid, cur, cur["revision"], **{k: copy(d[k]) for k in ("game", "items", "conditions", "abilities")})  # back to valid

    refused("a wrong afterBattle", lambda b: b["game"]["rules"]["downed"].__setitem__("afterBattle", "gone"), "\"afterBattle\" must be")
    refused("removeAfter 100", lambda b: b["game"]["rules"]["downed"].__setitem__("removeAfter", 100), "\"removeAfter\" must be")
    refused("revive without the rule", lambda b: b["game"]["rules"].pop("downed"), "needs \"rules\": { \"downed\": {} }")
    refused("a revive ability on an enemy", lambda b: b["abilities"][0].__setitem__("target", "enemy"), "\"revive\" needs \"target\" \"ally\"")
    refused("a revive ability with inflict", lambda b: b["abilities"][0].__setitem__("inflict", {"condition": "poison"}), "\"revive\" cannot go with \"inflict\"")
    refused("a revive item without target ally", lambda b: [i for i in b["items"] if i["id"] == "phoenix"][0].pop("target"),
            "\"revive\" needs \"target\": \"ally\"")
    refused("a revive item that also heals", lambda b: [i for i in b["items"] if i["id"] == "phoenix"][0].__setitem__("heal", 5),
            "\"revive\" cannot go with \"heal\"")
    refused("a revive staff on a foe", lambda b: [i for i in b["items"] if i["id"] == "life_staff"][0].__setitem__("target", "foe"),
            "\"revive\" needs \"target\" \"ally\", \"team\" or \"any\"")
    refused("an autoRevive formula", lambda b: [c for c in b["conditions"] if c["id"] == "reraise"][0].__setitem__("autoRevive", "a.hp"),
            "\"autoRevive\" must be")
    st, cur = get_project(api, pid)
    check("downed: written valid again", problems() == [] and cur["game"] == d["game"] and cur["items"] == d["items"], problems())
