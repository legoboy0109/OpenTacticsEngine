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
