"""Six optional rules through the ote-editor API (docs/SRPG.md "Guests", "An ability on its user", the area shapes "ring" and "field", "reach":
"equipped" on a strike, `arc` in an inflict chance; docs/JOBS.md a map's own "abilityPoints"). Each key comes back with GET, is written back
unchanged by PUT, the real loader (GET .../problems) accepts it and names what it refuses (the refusals the web editor's checks mirror).
Part 1 starts from tests/fixtures/srpg-conditions with a data/abilities.json added (abilities and weapon areas); part 2 from tests/fixtures/jobs
with a teaching item added (map "duel": guests, guestJoins, abilityPoints). Called from editor_api_test.py; uses its helpers."""
import copy
import json
import os
import shutil

TESTS = os.path.dirname(os.path.abspath(__file__))

ABILITIES = [
    {"id": "halo", "name": "Halo", "target": "unit", "range": [0, 0], "effect": {"heal": 5}, "area": {"shape": "ring", "radius": 1}},
    {"id": "rally", "name": "Rally", "target": "ally", "range": [0, 0], "inflict": {"condition": "regen"}, "area": {"shape": "field"}},
    {"id": "mend", "name": "Mend", "target": "ally", "range": [0, 2], "effect": {"heal": 8}},
    {"id": "stab", "name": "Stab", "target": "enemy", "range": [1, 1], "inflict": {"condition": "sleep", "chance": "arc * 40 + 10"}},
    {"id": "rend", "name": "Rend", "effect": {"strike": "whirl", "reach": "equipped"}},
]
WEAPONS_EXTRA = [
    {"id": "whirl", "name": "Whirl", "group": "sword", "range": [0, 0], "area": {"shape": "ring", "radius": 1}, "props": {"might": 2}},
    {"id": "tremor", "name": "Tremor", "group": "sword", "range": [1, 1], "area": {"shape": "field"}, "props": {"might": 2}},
]
TEACH_ITEMS = {"items": [{"id": "charm", "name": "Charm", "kind": "accessory", "teaches": [{"skill": "guard", "points": 30}]}]}


def _helpers(api, pid, root):
    def problems():
        st, res = api.json("GET", "/api/projects/%s/problems" % pid)
        return [p["message"] for p in res["problems"]] if st == 200 else [str(res)]

    def raw(rel):
        with open(os.path.join(root, rel), "rb") as f:
            return f.read()

    return problems, raw


def _abilities_and_weapons(api, games, check, get_project, put_project):
    pid = "rule-gaps-abilities"
    root = os.path.join(games, pid)
    shutil.copytree(os.path.join(TESTS, "fixtures", "srpg-conditions"), root)
    with open(os.path.join(root, "data", "abilities.json"), "w") as f:
        json.dump({"abilities": ABILITIES}, f)
    with open(os.path.join(root, "data", "weapons.json")) as f:
        weapons = json.load(f)
    weapons["weapons"] += WEAPONS_EXTRA
    with open(os.path.join(root, "data", "weapons.json"), "w") as f:
        json.dump(weapons, f)
    problems, raw = _helpers(api, pid, root)

    st, d = get_project(api, pid)
    check("rule gaps: abilities and weapons with the new keys load in the real loader", st == 200 and problems() == [], problems())
    a = {x["id"]: x for x in d.get("abilities", [])}
    w = {x["id"]: x for x in d.get("weapons", [])}
    check("rule gaps: GET carries ring / field areas, range 0, reach and an arc chance",
          a.get("halo", {}).get("area") == {"shape": "ring", "radius": 1} and a.get("rally", {}).get("area") == {"shape": "field"}
          and a.get("mend", {}).get("range") == [0, 2] and a.get("rend", {}).get("effect") == {"strike": "whirl", "reach": "equipped"}
          and a.get("stab", {}).get("inflict", {}).get("chance") == "arc * 40 + 10"
          and w.get("whirl", {}).get("area") == {"shape": "ring", "radius": 1} and w.get("tremor", {}).get("area") == {"shape": "field"},
          (sorted(a), sorted(w)))
    st, res = put_project(api, pid, d, d["revision"], abilities=d["abilities"], weapons=d["weapons"])
    after = raw("data/abilities.json") + raw("data/weapons.json")
    check("rule gaps: open + save keeps every key in the files",
          st == 200 and all(k in after for k in (b'"ring"', b'"field"', b'"reach"', b'"equipped"', b'arc * 40')), st)
    st, d2 = get_project(api, pid)
    check("rule gaps: ... no problems and the data read back is the same",
          problems() == [] and d2["abilities"] == d["abilities"] and d2["weapons"] == d["weapons"], problems())

    def refused(label, change, needle, key="abilities"):
        st, cur = get_project(api, pid)
        data = copy.deepcopy(cur[key])
        change({x["id"]: x for x in data})
        st, res = put_project(api, pid, cur, cur["revision"], **{key: data})
        ps = problems()
        check("rule gaps: %s is named by the loader" % label, st == 200 and any(needle in p for p in ps), (st, ps))
        st, cur = get_project(api, pid)
        put_project(api, pid, cur, cur["revision"], **{key: copy.deepcopy(d[key])})  # back to valid

    refused("a ring without a radius", lambda b: b["halo"]["area"].pop("radius"), "needs a \"radius\"")
    refused("a field with a radius", lambda b: b["rally"]["area"].__setitem__("radius", 2), "a \"field\" covers the whole map and takes no \"radius\"")
    refused("a weapon field with a radius", lambda b: b["tremor"]["area"].__setitem__("radius", 2), "takes no \"radius\"", key="weapons")
    refused("range 0 on an enemy ability without an area", lambda b: b["stab"].__setitem__("range", [0, 1]),
            "\"range\" may start at 0 (the user itself) only on")
    refused("reach without a strike", lambda b: b["mend"]["effect"].__setitem__("reach", "equipped"), "\"effect\": \"reach\" goes with \"strike\"")
    refused("reach other than equipped", lambda b: b["rend"]["effect"].__setitem__("reach", "far"), "\"effect\": \"reach\" must be \"equipped\"")
    st, cur = get_project(api, pid)
    check("rule gaps: abilities written valid again", problems() == [] and cur["abilities"] == d["abilities"], problems())


def _maps(api, games, check, get_project, put_project):
    pid = "rule-gaps-maps"
    root = os.path.join(games, pid)
    shutil.copytree(os.path.join(TESTS, "fixtures", "jobs"), root)
    with open(os.path.join(root, "data", "items.json"), "w") as f:
        json.dump(TEACH_ITEMS, f)
    problems, raw = _helpers(api, pid, root)
    st, d = get_project(api, pid)
    check("rule gaps: the jobs fixture with a teaching item loads", st == 200 and problems() == [], problems())

    def put_map(mutate):
        st, cur = get_project(api, pid)
        maps = copy.deepcopy(cur["maps"])
        mutate(maps["duel"])
        return put_project(api, pid, cur, cur["revision"], maps=maps)

    def good(m):
        m["abilityPoints"] = {"perBattle": 40}
        m["units"][2]["guest"] = True
        m["units"][2]["behaviour"] = "hold"
        m["events"] = [{"id": "join", "when": {"turn": 2}, "do": [
            {"addUnit": {"type": "squire", "id": "ritz", "side": "blue", "x": 0, "y": 4, "guest": True}},
            {"guestJoins": "plain"}]}]

    st, res = put_map(good)
    check("rule gaps: a map with abilityPoints, guests and guestJoins is saved", st == 200, (st, res))
    check("rule gaps: the real loader accepts it", problems() == [], problems())
    with open(os.path.join(root, "maps", "duel.json")) as f:
        saved = json.load(f)
    check("rule gaps: the map keeps its keys in the file",
          saved.get("abilityPoints") == {"perBattle": 40} and saved["units"][2].get("guest") is True
          and saved["events"][0]["do"][0]["addUnit"].get("guest") is True and saved["events"][0]["do"][1] == {"guestJoins": "plain"}, saved)
    before = raw("maps/duel.json")
    st, cur = get_project(api, pid)
    st, res = put_project(api, pid, cur, cur["revision"], maps=cur["maps"])
    check("rule gaps: a load and a save of the map changes nothing", st == 200 and raw("maps/duel.json") == before)

    cases = [
        ("a guest on a computer side", lambda m: m["units"][3].__setitem__("guest", True), "\"guest\" is for a unit on a side a person plays"),
        ("abilityPoints with another key", lambda m: m["abilityPoints"].__setitem__("perAction", 3), "unknown key \"perAction\""),
        ("abilityPoints past 9999", lambda m: m["abilityPoints"].__setitem__("perBattle", 10000), "\"perBattle\""),
        ("guestJoins with another key", lambda m: m["events"][0]["do"][1].__setitem__("side", "blue"), "unknown key \"side\""),
    ]
    good_map = json.loads(before)
    for what, mutate, words in cases:
        put_map(lambda m: (m.clear(), m.update(copy.deepcopy(good_map)), mutate(m)))
        ps = problems()
        check("rule gaps: %s is refused in plain words" % what, any(words in p for p in ps), ps)
    put_map(lambda m: (m.clear(), m.update(copy.deepcopy(good_map))))
    # Without a teaching item the map's abilityPoints has nothing to give.
    with open(os.path.join(root, "data", "items.json"), "w") as f:
        json.dump({"items": []}, f)
    ps = problems()
    check("rule gaps: abilityPoints without items that teach is refused", any("\"abilityPoints\" needs items that teach" in p for p in ps), ps)
    with open(os.path.join(root, "data", "items.json"), "w") as f:
        json.dump(TEACH_ITEMS, f)
    check("rule gaps: and the good map is accepted again", problems() == [], problems())


def run_rule_gaps_checks(api, tmp, games, templates, check, get_project, put_project):
    _abilities_and_weapons(api, games, check, get_project, put_project)
    _maps(api, games, check, get_project, put_project)
