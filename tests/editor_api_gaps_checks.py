"""Editor-gap checks of the ote-editor API test (docs/EDITOR_UI.md "Editor gaps"): the shapes the editor's new controls write are accepted by the
real loader, and the mistakes its checks mirror are refused in the loader's words. No helper change: it keeps every key. The scratch project is the
committed fixture tests/fixtures/srpg-locks (characters, ranks, a map with a computer side). Called from editor_api_test.py; uses its helpers."""
import json
import os
import shutil


def run_gaps_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "gaps-test"
    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    dst = os.path.join(games, pid)
    shutil.copytree(os.path.join(repo, "tests", "fixtures", "srpg-locks"), dst)

    def read(rel):
        with open(os.path.join(dst, rel), "r", encoding="utf-8") as f:
            return json.load(f)

    def problems():
        st, p = api.json("GET", "/api/projects/%s/problems" % pid)
        return p["problems"] if st == 200 else [{"message": "HTTP %s" % st}]

    def said(text, file=None):
        return any(text in x["message"] and (file is None or x.get("file") == file) for x in problems())

    st, d = get_project(api, pid)
    check("gaps: the fixture opens and loads clean", st == 200 and problems() == [], problems())

    # Class locks (a unit passes by either list), rankXp 0 and a character's own groups: the shapes the forms write.
    weapons = d["weapons"]
    for w in weapons:
        if w["id"] == "steel_sword":
            w["wieldedByTypes"] = ["knight"]
            w["rankXp"] = 0
        if w["id"] == "dawn_blade":
            w["wieldedByTypes"] = ["knight", "fighter"]   # on top of "wieldedBy": ["kell"]
    chars = d["characters"]
    for c in chars:
        if c["id"] == "mara":
            c["wields"] = ["sword"]
            c["ranks"] = {"sword": "d"}
    st, res = put_project(api, pid, d, d["revision"], weapons=weapons, characters=chars)
    check("gaps: wieldedByTypes, rankXp 0 and a character's wields + rank in it are accepted", st == 200, (st, res))
    check("gaps: the loader accepts them", problems() == [], problems())
    w = {x["id"]: x for x in read("data/weapons.json")["weapons"]}
    check("gaps: the file keeps them as written", w["steel_sword"]["wieldedByTypes"] == ["knight"] and w["steel_sword"]["rankXp"] == 0 and
          w["dawn_blade"]["wieldedBy"] == ["kell"] and w["dawn_blade"]["wieldedByTypes"] == ["knight", "fighter"], w)
    check("gaps: a character's own groups are kept", [c for c in read("data/characters.json")["characters"] if c["id"] == "mara"][0]["wields"] == ["sword"])

    # The mistakes: refused in the loader's words, naming the file.
    st, d = get_project(api, pid)
    for x in d["weapons"]:
        if x["id"] == "steel_sword":
            x["wieldedByTypes"] = ["ghost"]
    put_project(api, pid, d, d["revision"], weapons=d["weapons"])
    check("gaps: an unknown class on the list is refused", said("\"wieldedByTypes\" names unknown unit type \"ghost\"", "data/weapons.json"), problems())
    st, d = get_project(api, pid)
    for x in d["weapons"]:
        if x["id"] == "steel_sword":
            x["wieldedByTypes"] = ["knight"]
            x["rankXp"] = 100
    put_project(api, pid, d, d["revision"], weapons=d["weapons"])
    check("gaps: rankXp 100 is refused, and the words say 0 is allowed", said("\"rankXp\" must be from 1 to 99, or 0 for a weapon that trains nothing", "data/weapons.json"), problems())
    st, d = get_project(api, pid)
    for x in d["weapons"]:
        if x["id"] == "steel_sword":
            x["rankXp"] = 0
    put_project(api, pid, d, d["revision"], weapons=d["weapons"])
    check("gaps: put right again, it loads", problems() == [], problems())

    # An Add a unit command with its own starting ranks and goes-for (the same controls as a placed unit's panel).
    st, d = get_project(api, pid)
    duel = d["maps"]["duel"]
    duel["events"] = [{"id": "reinforce", "when": {"turn": 1},
                       "do": [{"addUnit": {"type": "fighter", "side": "red", "x": 4, "y": 0, "ranks": {"sword": "b"}, "goesFor": ["chests", "steal"]}}]}]
    st, res = put_project(api, pid, d, d["revision"], maps=d["maps"])
    check("gaps: an addUnit with ranks and goesFor is accepted", st == 200, (st, res))
    check("gaps: the loader accepts it", problems() == [], problems())
    cmd = read("maps/duel.json")["events"][0]["do"][0]["addUnit"]
    check("gaps: the file keeps ranks and goesFor on the command", cmd["ranks"] == {"sword": "b"} and cmd["goesFor"] == ["chests", "steal"], cmd)
    st, d = get_project(api, pid)
    d["maps"]["duel"]["events"][0]["do"][0]["addUnit"]["side"] = "blue"
    put_project(api, pid, d, d["revision"], maps=d["maps"])
    check("gaps: goesFor on an addUnit for a side a person plays is refused", said("\"goesFor\" tells the computer opponent what to do", "maps/duel.json"), problems())
    st, d = get_project(api, pid)
    cmd = d["maps"]["duel"]["events"][0]["do"][0]["addUnit"]
    cmd["side"] = "red"
    cmd["ranks"] = {"sword": "zz"}
    put_project(api, pid, d, d["revision"], maps=d["maps"])
    check("gaps: a rank that is not on the ladder, on an addUnit, is refused", any("maps/duel.json" == x.get("file") and "zz" in x["message"] for x in problems()), problems())
