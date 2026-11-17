"""Status condition checks of the ote-editor API test (docs/SRPG.md section 9, docs/EDITOR_UI.md "Conditions"): the editor has no forms for
conditions yet, so the helper must leave data/conditions.json and every key that names a condition untouched through GET and PUT of the
project (a staff's inflict / cure effects, a usable item's cure / inflict, a weapon's inflict, a skill's immune / inflict blocks), and the
real loader's refusals about them reach /problems in the engine's words. Called from editor_api_test.py; uses its helpers. Starts from a
copy of tests/fixtures/srpg-conditions (it uses every route)."""
import json as _json
import os
import shutil

FIXTURE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fixtures", "srpg-conditions")
CONDITIONS = "data/conditions.json"


def run_conditions_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "conditions-api"
    shutil.copytree(FIXTURE, os.path.join(games, pid))
    root = os.path.join(games, pid)

    def raw(rel):
        with open(os.path.join(root, rel), "rb") as f:
            return f.read()

    def problems():
        st, p = api.json("GET", "/api/projects/%s/problems" % pid)
        return p["problems"] if st == 200 else [("status", st)]

    conditions_before = raw(CONDITIONS)
    items_before = _json.loads(raw("data/items.json").decode())["items"]
    weapons_before = _json.loads(raw("data/weapons.json").decode())["weapons"]
    skills_before = _json.loads(raw("data/skills.json").decode())["skills"]
    st, d = get_project(api, pid)
    check("conditions: the fixture loads (no problems)", st == 200 and d["problems"] == [], d.get("problems"))
    check("conditions: GET returns every item with its inflict / cure keys", d["items"] == items_before, d["items"])
    check("conditions: GET returns the weapons with their inflict key", d["weapons"] == weapons_before, d["weapons"])
    check("conditions: GET returns the skills with their immune / inflict blocks and the condition test", d["skills"] == skills_before, d["skills"])
    effects = {k for i in d["items"] if i["kind"] == "staff" for e in i["effects"] for k in e}
    check("conditions: the fixture uses both new staff effects", {"inflict", "cure"} <= effects, sorted(effects))

    st, res = put_project(api, pid, d, d["revision"], items=d["items"], weapons=d["weapons"], skills=d["skills"])
    check("conditions: PUT everything back -> 200", st == 200, res)
    check("conditions: data/conditions.json is untouched, byte for byte", raw(CONDITIONS) == conditions_before)
    check("conditions: the items, weapons and skills keep every key", _json.loads(raw("data/items.json").decode())["items"] == items_before and
          _json.loads(raw("data/weapons.json").decode())["weapons"] == weapons_before and
          _json.loads(raw("data/skills.json").decode())["skills"] == skills_before)
    check("conditions: still no problems", problems() == [], problems())

    # An edit of an item the form does show leaves the condition keys of its neighbours alone.
    st, d = get_project(api, pid)
    edited = [dict(i) for i in d["items"]]
    next(i for i in edited if i["id"] == "antidote")["uses"] = 5
    st, res = put_project(api, pid, d, d["revision"], items=edited)
    st, d = get_project(api, pid)
    check("conditions: editing an item's uses keeps its cure", next(i for i in d["items"] if i["id"] == "antidote")["cure"] == ["poison"] and
          next(i for i in d["items"] if i["id"] == "poison_staff")["effects"] == [{"inflict": {"condition": "poison"}}])
    check("conditions: and the file is still untouched and the game loads", raw(CONDITIONS) == conditions_before and problems() == [], problems())

    # What the loader refuses reaches /problems in its own words (the conditions file is edited by hand: there is no form).
    path = os.path.join(root, CONDITIONS)
    with open(path, "wb") as f:
        f.write(conditions_before.replace(b'"id": "poison"', b'"id": "venom"', 1))
    msgs = [p["message"] for p in problems()]
    check("conditions: a staff naming a condition that is gone is refused in plain words",
          any("names unknown condition \"poison\"" in m for m in msgs), msgs)
    with open(path, "wb") as f:
        f.write(conditions_before.replace(b'"turns": 3,', b'', 1))
    msgs = [p["message"] for p in problems()]
    check("conditions: a condition with no duration is refused", any("needs a duration" in m for m in msgs), msgs)
    with open(path, "wb") as f:
        f.write(conditions_before)
    check("conditions: restored: no problems again", problems() == [], problems())

    # The starter set ships with the strategy-RPG template.
    st, doc = api.json("POST", "/api/projects", {"id": "conditions-new", "name": "C", "template": "strategy-rpg"})
    check("conditions: a game made from the strategy-RPG template", st == 201, (st, doc))
    new_root = os.path.join(games, "conditions-new", CONDITIONS)
    # (the file begins with comment lines, which the engine's reader accepts: they are cut here)
    text = "\n".join(l for l in open(new_root).read().splitlines() if not l.strip().startswith("//")) if os.path.exists(new_root) else "{}"
    check("conditions: the template ships the six starter conditions",
          [c["id"] for c in _json.loads(text).get("conditions", [])] == ["poison", "sleep", "silence", "berserk", "stun", "curse"])
    st, d = get_project(api, "conditions-new")
    check("conditions: the new game has no problems", d["problems"] == [], d.get("problems"))
    before = open(new_root, "rb").read()
    put_project(api, "conditions-new", d, d["revision"], items=d["items"], skills=d["skills"], weapons=d["weapons"])
    check("conditions: and the helper leaves its conditions file alone", open(new_root, "rb").read() == before)
