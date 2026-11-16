"""E3 checks of the ote-editor API test (docs/SRPG.md "Editor", step E3): what the Abilities, Promotion and Carrying screens write
(abilities.json with a push and a summon, unit type abilities and promotions, a promotion item, rules.rescue / rules.promotion,
a Promote story command) must load in the real game loader: GET .../problems is empty, and the headless simulator plays the map.
Called from editor_api_test.py; uses its helpers."""
import os
import subprocess


def run_abilities_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "abilities-e3"
    st, doc = api.json("POST", "/api/projects", {"id": pid, "name": "E3", "template": "strategy-rpg"})
    check("e3: create scratch project", st == 201, (st, doc))
    st, d = get_project(api, pid)
    units = d["units"]
    for u in units:
        if u["id"] == "lord":
            u["abilities"] = ["shove", "call_hound"]
            u["promotions"] = [{"to": "cavalier", "fromLevel": 5, "items": ["writ"], "atLevel": "offer", "levelAfter": "keep",
                                "gains": {"str": 2, "spd": -1}}]
            u["pairBonus"] = {"str": 2}
        if u["id"] == "archer":
            u["items"] = ["writ"]
    items = [{"id": "writ", "name": "Writ", "kind": "usable", "heal": 5, "promotes": True}]
    abilities = [
        {"id": "shove", "name": "Shove", "target": "enemy", "effect": {"push": 2}, "cost": "free", "uses": 3, "cooldown": 2,
         "afterMove": False, "if": [{"hp": {"op": ">", "percent": 50}}]},
        {"id": "call_hound", "name": "Call Hound", "target": "cell", "range": [1, 3],
         "effect": {"summon": {"type": "soldier", "level": 3, "limit": 2, "vanish": False, "lifetime": 4, "canAct": True, "givesXp": False}}},
    ]
    game = d["game"]
    game["rules"]["rescue"] = {"mode": "pairUp", "canCarry": "a.str >= b.def", "carrierStats": {"spd": {"percent": 50}, "mov": -1},
                               "pairBonus": {"str": 1, "def": 1}}
    game["rules"]["promotion"] = {"levelAfter": "keep"}
    maps = d["maps"]
    mid = game["maps"][0]["id"]
    maps[mid]["units"][0]["id"] = "eirik"
    maps[mid]["events"] = [{"id": "rise", "when": {"battle": "start"}, "do": [{"promote": "eirik", "to": "cavalier"}]},
                           {"id": "cheer", "when": {"promoted": {}}, "do": [{"say": "Well done!"}]}]
    st, res = put_project(api, pid, d, d["revision"], units=units, game=game, maps=maps, items=items, abilities=abilities)
    check("e3: PUT abilities, promotion, carrying and story -> 200", st == 200, (st, res))
    st, p = api.json("GET", "/api/projects/%s/problems" % pid)
    check("e3: the real loader accepts all of it (no problems)", st == 200 and p["problems"] == [], p)

    root = os.path.join(games, pid)
    sim = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "build", "ote-sim")
    if os.path.exists(sim):
        r = subprocess.run([sim, root, mid, "--runs", "1"], capture_output=True, timeout=120)
        check("e3: ote-sim plays the map with those files", r.returncode == 0, (r.returncode, r.stderr[-300:]))

    # What the screens would refuse is refused by the loader too: a unit type that promotes into itself, a pair bonus without pair up.
    st, d = get_project(api, pid)
    for u in d["units"]:
        if u["id"] == "lord":
            u["promotions"] = [{"to": "lord"}]
    st, res = put_project(api, pid, d, d["revision"], units=d["units"])
    st, p = api.json("GET", "/api/projects/%s/problems" % pid)
    check("e3: a promotion into itself is a problem in the loader's words",
          any("\"to\" is the unit type itself" in x["message"] for x in p["problems"]), p)
    st, d = get_project(api, pid)
    for u in d["units"]:
        u.pop("promotions", None)
    del d["game"]["rules"]["rescue"]
    st, res = put_project(api, pid, d, d["revision"], units=d["units"], game=d["game"])
    st, p = api.json("GET", "/api/projects/%s/problems" % pid)
    check("e3: pairBonus without pair up is a problem", any("pairBonus" in x["message"] for x in p["problems"]), p)
