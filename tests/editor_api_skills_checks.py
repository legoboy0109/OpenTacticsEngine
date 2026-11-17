"""E2 checks of the ote-editor API test (docs/SRPG.md "Editor", step E2): what the Skills screen writes (the whole starter set, a skill of every
`when`, every condition and effect kind, skills on unit types / learns / characters / accessories / weapons, and the teachSkill /
forgetSkill / hasSkill story forms) must load in the real game loader: GET .../problems is empty and the headless simulator plays the map.
Called from editor_api_test.py; uses its helpers."""
import os
import subprocess

EXTRA = [
    {"id": "every_condition", "name": "Every condition", "blocks": [
        {"when": "combat", "if": [{"hp": {"op": "<=", "percent": 50}}, {"near": "enemy", "within": 3, "op": ">=", "count": 1, "match": "archer"},
                                  {"weapon": "iron_sword"}, {"terrain": "forest"}, {"unit": "soldier", "who": "foe"}, {"carrying": False},
                                  {"round": {"op": ">=", "value": 2}}, {"initiating": True}],
         "do": [{"damage": 2}]},
        {"when": "strike", "if": [{"distance": {"op": "<=", "value": 2}}, {"formula": "a.level >= 1"}, {"structure": True}], "chance": 40,
         "do": [{"crit": 10}]}]},
    {"id": "every_effect", "name": "Every effect", "blocks": [
        {"when": "combat", "do": [{"stats": {"str": 1}}, {"hit": 5, "to": "foe"}, {"strikes": -1, "to": "foe"}, {"avoid": 5}, {"critAvoid": 5},
                                  {"damageTaken": -1}, {"damageTakenPercent": 90}, {"damagePercent": 110}]},
        {"when": "always", "do": [{"growths": {"str": 5}}, {"xpPercent": 120}, {"ability": "shove"}]},
        {"when": "combat", "aura": {"to": "allies", "within": 3}, "if": [{"hp": {"op": ">", "percent": 10}}], "do": [{"stats": {"def": 1}}, {"crit": 5}]},
        {"when": "turnStart", "chance": "a.skl * 2", "do": [{"heal": 3}, {"hurt": 1, "to": "enemies", "within": 2}]}]},
    # The general blocks (docs/SRPG.md "More skill blocks"): turn tests, formula amounts, follow-up control, an aura limited to a kind of unit,
    # the counterattack / inventory / damage-kind tests, and the single standing effects.
    {"id": "general_blocks", "name": "General blocks", "blocks": [
        {"when": "combat", "if": [{"turn": {"odd": True}}, {"turn": {"from": 2, "to": 9}}, {"turn": {"every": 3, "offset": 1}}, {"turn": {"even": True}},
                                  {"inventory": "notFull"}, {"weaponKind": "magic", "who": "foe"}],
         "do": [{"damage": "turn * 2"}, {"hit": "a.skl - b.skl"}, {"stats": {"str": "turn"}}, {"followUp": "force"},
                {"followUp": "forbid", "to": "foe"}, {"followShift": 2}, {"trianglePercent": 200}]},
        {"when": "combat", "aura": {"to": "allies", "within": 2, "match": "lord"}, "do": [{"hit": "b.str"}]},
        {"when": "strike", "if": [{"counterattack": False}, {"inventory": {"op": ">=", "value": 1}}], "do": [{"damage": 3}]},
        {"when": "always", "do": [{"moveAgain": {"notAfterAttack": True}}, {"weaponXpPercent": 150}, {"noCarryPenalty": True},
                                  {"cancelEffective": True}]},
        {"when": "turnStart", "do": [{"cure": "all"}]}]},
    # The second set (docs/SRPG.md "Skill and ability blocks, second set"): nullify, a staff-used point, staff range, carry bonus, a terrain list, tags.
    {"id": "second_set", "name": "Second set", "tags": ["bonus"], "blocks": [
        {"when": "combat", "do": [{"nullifySkills": {"tag": "bonus"}}]},
        {"when": "combat", "if": [{"terrain": ["forest", "plain"]}], "do": [{"nullifySkills": ["tough"]}, {"damage": 1}]},
        {"when": "always", "do": [{"staffRange": {"amount": 2}}, {"carryBonus": 2}]},
        {"when": "staffUse", "chance": 50, "do": [{"heal": {"percent": 10}}, {"cure": "all"}]}]},
]


def _no_events(maps):
    for m in maps.values():
        m.pop("events", None)
    return maps


from editor_api_plain_units import plain_units


def run_skills_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "skills-e2"
    st, doc = api.json("POST", "/api/projects", {"id": pid, "name": "E2", "template": "strategy-rpg"})
    check("e2: create scratch project", st == 201, (st, doc))
    st, d = get_project(api, pid)
    check("e2: the template ships the starter set (thirty skills)", len(d["skills"] or []) == 30, len(d["skills"] or []))
    skills = d["skills"] + EXTRA
    for u in d["units"]:
        if u["id"] == "lord":
            u["skills"] = ["tough", "heavy_blow", "every_condition", "general_blocks", "second_set"]
            u["abilities"] = ["pick", "encore", "leap"]
            u["learns"] = [{"level": 2, "skill": "catch_breath"}, {"level": 3, "skill": "every_effect"}]
    for w in d["weapons"]:
        if w["id"] == "iron_sword":
            w["skills"] = ["opening_blow"]
            w["damageKind"] = "magic"  # a weapon's kind of damage, tested by "general_blocks"
    items = (d["items"] or []) + [{"id": "ring", "name": "Ring", "kind": "accessory", "skills": ["light_step"], "stealable": False}]  # the template's own stay
    abilities = [{"id": "shove", "name": "Shove", "target": "enemy", "effect": {"push": 1}},
                 {"id": "pick", "name": "Pick", "target": "enemy", "effect": {"steal": {"if": "a.spd > b.spd", "weapons": True}}},
                 {"id": "encore", "name": "Encore", "target": "ally", "effect": {"refresh": {"perTurn": 2}}},
                 {"id": "leap", "name": "Leap", "target": "unit", "effect": {"movePast": True}}]
    chars = [{"id": "eirik", "name": "Eirik", "type": "lord", "tags": ["scout"], "skills": ["on_my_own"], "learns": [{"level": 2, "skill": "gifted"}]}]
    game = d["game"]
    mid = game["maps"][0]["id"]
    maps = plain_units(d)
    maps[mid]["units"][0]["id"] = "hero"
    maps[mid]["events"] = [
        {"id": "teach", "when": {"battle": "start"}, "if": [{"hasSkill": "tough"}],
         "do": [{"teachSkill": "heavy_blow", "to": {"unit": "hero"}}, {"forgetSkill": "heavy_blow", "from": {}}]},
    ]
    st, res = put_project(api, pid, d, d["revision"], units=d["units"], weapons=d["weapons"], items=items, abilities=abilities,
                          skills=skills, characters=chars, maps=maps, supports={"pairs": []})  # the template's pair names its own people
    check("e2: PUT skills, sources and story forms -> 200", st == 200, (st, res))
    st, p = api.json("GET", "/api/projects/%s/problems" % pid)
    check("e2: the real loader accepts all of it (no problems)", st == 200 and p["problems"] == [], p)
    root = os.path.join(games, pid)
    sim = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "build", "ote-sim")
    if os.path.exists(sim):
        r = subprocess.run([sim, root, mid, "--runs", "1"], capture_output=True, timeout=120)
        check("e2: ote-sim plays the map with those files", r.returncode == 0, (r.returncode, r.stderr[-300:]))

    # Shapes the builder never offers are the loader's refusals, in its words.
    st, d = get_project(api, pid)
    bad = [{"id": "bad", "name": "Bad", "blocks": [{"when": "always", "chance": 10, "do": [{"drain": 5}]}]}]
    st, res = put_project(api, pid, d, d["revision"], skills=bad, units=[dict(u, skills=[], learns=[]) for u in d["units"]],
                          weapons=[dict(w, skills=[]) for w in d["weapons"]], items=[], characters=[], maps=_no_events(d["maps"]))
    st, p = api.json("GET", "/api/projects/%s/problems" % pid)
    check("e2: a chance on an always rule is a problem in the loader's words",
          any("\"chance\" cannot be used in an \"always\" block" in x["message"] for x in p["problems"]), p)
