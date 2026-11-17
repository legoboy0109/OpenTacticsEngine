"""Staff checks of the ote-editor API test (docs/SRPG.md section 8 "Staves", docs/EDITOR_API.md): a staff is an item of kind "staff" in
data/items.json, so there is no new section; the helper must keep every staff key through GET and PUT (the editor only edits the keys it
shows, and a hand-edited file may carry others), and the real loader's refusals about a staff reach /problems in the engine's words.
Called from editor_api_test.py; uses its helpers. Starts from a copy of tests/fixtures/srpg-staves (it uses every staff key)."""
import json as _json
import os
import shutil

FIXTURE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fixtures", "srpg-staves")
ITEMS = "data/items.json"
STAFF_KEYS = {"id", "name", "kind", "description", "group", "rank", "rankXp", "wieldedBy", "props", "uses", "price", "range", "target", "area",
              "effects", "hit", "xp", "if"}


def run_staves_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "staves-api"
    shutil.copytree(FIXTURE, os.path.join(games, pid))
    root = os.path.join(games, pid)

    def raw():
        with open(os.path.join(root, ITEMS), "rb") as f:
            return f.read()

    def problems():
        st, p = api.json("GET", "/api/projects/%s/problems" % pid)
        return p["problems"] if st == 200 else [("status", st)]

    def messages():
        return [p["message"] for p in problems() if p.get("file") == ITEMS]

    on_disk = _json.loads(raw().decode())["items"]
    st, d = get_project(api, pid)
    check("staves: the fixture loads (no problems)", st == 200 and d["problems"] == [], d.get("problems"))
    check("staves: GET returns every item of the file, staff keys and all", d["items"] == on_disk, d["items"])
    staves = [i for i in d["items"] if i["kind"] == "staff"]
    seen = set().union(*[set(i) for i in staves])
    check("staves: the fixture has a staff using every staff key", STAFF_KEYS - {"price", "if", "description"} <= seen, sorted(STAFF_KEYS - seen))
    check("staves: reach comes back as a number, a list and a formula, area as 'all' and a radius",
          {type(i.get("range")).__name__ for i in staves} >= {"int", "list", "str"} and
          any(i.get("area") == "all" for i in staves) and any(i.get("area") == {"radius": 1} for i in staves))

    # a PUT of the same items keeps every key (file parses to the same thing)
    st, res = put_project(api, pid, d, d["revision"], items=d["items"])
    check("staves: PUT the items back -> 200, every staff key kept", st == 200 and _json.loads(raw().decode())["items"] == on_disk, res)
    check("staves: still no problems", problems() == [], problems())

    # what the editor writes for an edited staff loads: formula reach -> a list, a radius, a percentage, a chance, an experience formula
    st, d = get_project(api, pid)
    edited = [dict(i) for i in d["items"]]
    mend = next(i for i in edited if i["id"] == "mend_staff")
    mend.update({"range": [1, 5], "area": {"radius": 2}, "effects": [{"heal": {"percent": 30}}], "hit": "70 + a.mag", "xp": "a.level + 5", "target": "team"})
    put_project(api, pid, d, d["revision"], items=edited)
    check("staves: an edited staff loads in the real loader", problems() == [], problems())
    st, d = get_project(api, pid)
    check("staves: the edit round-trips", next(i for i in d["items"] if i["id"] == "mend_staff")["range"] == [1, 5])

    # keys the editor does not show (stage-2 / stage-3 ideas, hand edits) are kept in the file, and the loader names them
    odd = [dict(i) for i in d["items"]]
    next(i for i in odd if i["id"] == "mend_staff")["zzz_later"] = {"keep": [1, 2]}
    next(i for i in odd if i["id"] == "kin_staff")["effects"] = [{"heal": 5}, {"warp": {"range": 3}}]
    put_project(api, pid, d, d["revision"], items=odd)
    st, d = get_project(api, pid)
    check("staves: unknown keys and extra effects are kept", next(i for i in d["items"] if i["id"] == "mend_staff").get("zzz_later") == {"keep": [1, 2]} and
          len(next(i for i in d["items"] if i["id"] == "kin_staff")["effects"]) == 2)
    check("staves: the loader names the unknown key in its own words", any("unknown key" in m and "zzz_later" in m for m in messages()), messages())

    # what the form's checks mirror: the loader refuses these, and Problems shows it
    def refuse(label, patch, word):
        st, d = get_project(api, pid)
        items = [dict(i) for i in on_disk]
        item = next(i for i in items if i["id"] == "mend_staff")
        for k, v in patch.items():
            if v is None:
                item.pop(k, None)
            else:
                item[k] = v
        put_project(api, pid, d, d["revision"], items=items)
        check("staves: %s -> a problem on data/items.json" % label, any(word in m for m in messages()), messages())

    refuse("no group", {"group": None}, "\"group\" is required for a staff")
    refuse("reach above the limit", {"range": 21}, "\"range\" must be a whole number from 1 to 20")
    refuse("reach from above to", {"range": [4, 2]}, "min not above max")
    refuse("area radius above the limit", {"area": {"radius": 11}}, "\"radius\" must be a whole number from 1 to 10")
    refuse("healing a foe", {"target": "foe"}, "\"heal\" cannot target \"foe\"")
    refuse("a rank in a game with the ladder removed is checked below; an unknown rank", {"rank": "zz"}, "unknown rank")
    refuse("a lock naming nobody", {"wieldedBy": ["nobody"]}, "unknown character")

    # a game with no ranks file: a rank on a staff is refused
    st, d = get_project(api, pid)
    put_project(api, pid, d, d["revision"], items=on_disk, ranks=False)
    check("staves: ranks off with ranked staves -> the loader says the game has no weapon ranks",
          any("no weapon ranks" in m for m in messages()), messages())

    # back to the fixture's state: no problems again
    st, d = get_project(api, pid)
    put_project(api, pid, d, d["revision"], items=on_disk, ranks={"growth": "use", "ranks": [
        {"id": "e", "name": "E", "xp": 0}, {"id": "d", "name": "D", "xp": 10}, {"id": "c", "name": "C", "xp": 25}, {"id": "b", "name": "B", "xp": 45}]})
    check("staves: back to no problems", problems() == [], problems())

    # Stage two (warp, pull, swap, repair, light, an item's "repairable"): the Staff form edits these now; the helper still hands
    # them through GET and PUT untouched, and the loader must accept them (no problems).
    pid2 = "staves2-api"
    shutil.copytree(os.path.join(os.path.dirname(FIXTURE), "srpg-staves2"), os.path.join(games, pid2))
    root2 = os.path.join(games, pid2)
    with open(os.path.join(root2, ITEMS), "rb") as f:
        on_disk2 = _json.loads(f.read().decode())["items"]
    st, d2 = get_project(api, pid2)
    check("staves 2: the fixture with warp, pull, swap, repair and light loads (no problems)", st == 200 and d2["problems"] == [], d2.get("problems"))
    check("staves 2: GET returns every item, the new effect kinds and \"repairable\" untouched", d2["items"] == on_disk2, d2["items"])
    kinds = {k for i in d2["items"] if i["kind"] == "staff" for e in i["effects"] for k in e}
    check("staves 2: the fixture uses every new effect kind", {"warp", "pull", "swap", "repair", "light", "heal"} <= kinds, sorted(kinds))
    st, res = put_project(api, pid2, d2, d2["revision"], items=d2["items"])
    with open(os.path.join(root2, ITEMS), "rb") as f:
        after = _json.loads(f.read().decode())["items"]
    check("staves 2: PUT the items back keeps every new effect and flag", st == 200 and after == on_disk2, res)
    # An edit of a staff's uses leaves its later effect of another kind alone.
    st, d2 = get_project(api, pid2)
    edited = [dict(i) for i in d2["items"]]
    next(i for i in edited if i["id"] == "kindly_pull_staff")["uses"] = 9
    put_project(api, pid2, d2, d2["revision"], items=edited)
    st, d2 = get_project(api, pid2)
    got = next(i for i in d2["items"] if i["id"] == "kindly_pull_staff")
    check("staves 2: editing a staff's uses keeps its pull effect", got["uses"] == 9 and got["effects"] == [{"heal": 6}, {"pull": True}], got)
    check("staves 2: and the game still loads without problems", d2["problems"] == [], d2.get("problems"))

    # What the editor's Staff form checks mirror (model/staffChecks.ts): each stage-two refusal of the real loader reaches /problems in the
    # engine's words. The shapes the form writes are built end to end in editor/web/tests/e2e/srpg-staves2.spec.ts; here the refusals.
    base2 = [dict(i) for i in on_disk2]

    def refuse2(label, staff_id, patch, word):
        st, d = get_project(api, pid2)
        items = [dict(i) for i in base2]
        item = next(i for i in items if i["id"] == staff_id)
        for k, v in patch.items():
            if v is None:
                item.pop(k, None)
            else:
                item[k] = v
        put_project(api, pid2, d, d["revision"], items=items)
        st, p = api.json("GET", "/api/projects/%s/problems" % pid2)
        msgs = [x["message"] for x in p["problems"] if x.get("file") == ITEMS]
        check("staves 2: %s -> a problem on data/items.json" % label, any(word in m for m in msgs), msgs)

    refuse2("a warp with no reach", "warp_staff", {"effects": [{"warp": {}}]}, "\"warp\" needs \"range\"")
    refuse2("a warp reach above the limit", "warp_staff", {"effects": [{"warp": {"range": 21}}]}, "\"warp\": \"range\" must be a whole number from 1 to 20")
    refuse2("a warp reach from above to", "warp_staff", {"effects": [{"warp": {"range": [4, 2]}}]}, "min not above max")
    refuse2("a warp with another key", "warp_staff", {"effects": [{"warp": {"range": 3, "zzz": 1}}]}, "\"warp\": unknown key \"zzz\"")
    refuse2("a pull that is not true", "recall_staff", {"effects": [{"pull": False}]}, "\"pull\" must be true")
    refuse2("a repair of 0 uses", "fix_staff", {"effects": [{"repair": 0}]}, "\"repair\" must be a whole number of uses from 1 to 999")
    refuse2("a repair of half", "fix_staff", {"effects": [{"repair": "half"}]}, "\"repair\" must be \"full\" or a whole number")
    refuse2("a light with no radius", "lantern_staff", {"effects": [{"light": {"turns": 2}}]}, "\"light\": \"radius\" must be a whole number from 1 to 10")
    refuse2("a light of 0 turns", "lantern_staff", {"effects": [{"light": {"radius": 2, "turns": 0}}]}, "\"light\": \"turns\" must be a whole number from 1 to 10")
    refuse2("an unknown effect", "mend_staff", {"effects": [{"glow": "door"}]}, "unknown effect \"glow\"")
    refuse2("two moves", "recall_staff", {"effects": [{"pull": True}, {"swap": True}]}, "only one of \"warp\", \"pull\" and \"swap\"")
    refuse2("two repairs", "fix_staff", {"effects": [{"repair": 1}, {"repair": 2}]}, "only one \"repair\"")
    refuse2("two lights", "lantern_staff", {"effects": [{"light": {"radius": 1}}, {"light": {"radius": 2}}]}, "only one \"light\"")
    refuse2("repair with a move", "fix_staff", {"effects": [{"repair": 1}, {"pull": True}]}, "\"repair\" cannot be put together")
    refuse2("a light with a heal", "lantern_staff", {"effects": [{"light": {"radius": 1}}, {"heal": 3}]}, "cannot be put together with another effect")
    refuse2("a light with a target", "lantern_staff", {"target": "ally"}, "has no \"target\"")
    refuse2("a light with an area", "lantern_staff", {"area": "single"}, "has no \"area\"")
    refuse2("a light with a chance", "lantern_staff", {"hit": 50}, "no unit to roll \"hit\" for")
    refuse2("a light with conditions", "lantern_staff", {"if": []}, "no unit for \"if\" to judge")
    refuse2("a pull with a wide area", "recall_staff", {"area": "all"}, "\"area\" must be \"single\"")
    refuse2("a pull on the user", "recall_staff", {"target": "self", "range": None}, "needs another unit")
    refuse2("repairable that is not a boolean", "iron_blade", {"repairable": "no"}, "\"repairable\" must be true or false")
    refuse2("repairable on an item with no uses", "short_sword", {"repairable": False}, "\"repairable\" only matters for an item with \"uses\"")

    # The shapes the form writes load: heal + swap, a warp with a formula, heal + repair, a light, "repairable" off.
    st, d = get_project(api, pid2)
    built = [dict(i) for i in base2]
    built.append({"id": "built_a", "name": "Built A", "kind": "staff", "group": "staff", "uses": 5, "range": [1, 3], "effects": [{"heal": {"percent": 30}}, {"swap": True}]})
    built.append({"id": "built_b", "name": "Built B", "kind": "staff", "group": "staff", "uses": 5, "range": "1 + a.mag / 3", "effects": [{"warp": {"range": "2 + a.mag"}}]})
    built.append({"id": "built_c", "name": "Built C", "kind": "staff", "group": "staff", "uses": 5, "range": 2, "effects": [{"heal": 4}, {"repair": 2}]})
    built.append({"id": "built_d", "name": "Built D", "kind": "staff", "group": "staff", "uses": 5, "range": 2, "effects": [{"light": {"radius": 2, "turns": 1}}]})
    next(i for i in built if i["id"] == "iron_blade")["repairable"] = False
    put_project(api, pid2, d, d["revision"], items=built)
    st, p = api.json("GET", "/api/projects/%s/problems" % pid2)
    check("staves 2: the shapes the form writes load without problems", st == 200 and p["problems"] == [], p.get("problems"))
    put_project(api, pid2, d, d["revision"], items=base2)
