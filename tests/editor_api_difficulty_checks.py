"""Difficulties through the ote-editor API (docs/DIFFICULTY.md): game.json "difficulties", a map's "difficulty", a placement's "difficulty" and
"difficultyShift", a side's "difficultyShift", the story condition and addUnit's "difficulty" are passed through like every other key: they
open and save byte for byte, the real loader (GET .../problems) accepts them and names what it refuses. Starts from a copy of
tests/fixtures/srpg-skillblocks2 (a field map with named units). Called from editor_api_test.py; uses its helpers."""
import json
import os
import shutil

FIXTURE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fixtures", "srpg-skillblocks2")


def run_difficulty_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "difficulty-api"
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
    check("difficulty: the fixture loads", st == 200 and problems() == [], problems())
    mid = d["game"]["maps"][0]["id"]
    mfile = d["game"]["maps"][0]["file"]
    game = copy(d["game"])
    game["difficulties"] = {
        "default": "normal", "change": "free", "percentStats": ["hp", "str"],
        "list": [
            {"id": "easy", "name": "Easy", "description": "Gentler foes.", "enemy": {"levels": -2}, "xpPercent": 150, "resourcePercent": 150},
            {"id": "normal", "name": "Normal"},
            {"id": "hard", "name": "Hard", "enemy": {"levels": 2, "stats": {"def": 1}, "statsPercent": 110}, "xpPercent": 50}]}
    m = copy(d["maps"][mid])
    m["difficulty"] = {"hard": {"enemy": {"levels": 3}, "xpPercent": 60}}
    m["sides"][1]["difficultyShift"] = False
    m["units"][1]["difficulty"] = ["hard"]
    m["units"][1]["difficultyShift"] = False
    m["units"][4]["difficulty"] = ["easy", "normal"]
    m["events"] = [
        {"id": "start", "when": {"battle": "start"}, "do": [
            {"if": [{"difficulty": "hard"}], "then": [{"say": "Brace yourselves."}]},
            {"if": [{"difficulty": {"atLeast": "normal"}}], "then": [{"say": "Not easy."}]},
            {"addUnit": {"type": "guard", "side": "red", "x": 6, "y": 0, "difficulty": ["hard"]}}]}]
    st, res = put_project(api, pid, d, d["revision"], game=game, maps={mid: m})
    check("difficulty: PUT of the new keys -> 200", st == 200 and "revision" in res, (st, res))
    check("difficulty: the real loader accepts them", problems() == [], problems())

    files = [mfile, "game.json"]
    before = {f: raw(f) for f in files}
    st, d = get_project(api, pid)
    check("difficulty: GET returns the keys as written",
          d["game"]["difficulties"] == game["difficulties"] and d["maps"][mid]["difficulty"] == m["difficulty"] and d["maps"][mid]["units"][1]["difficulty"] == ["hard"]
          and d["maps"][mid]["events"] == m["events"], "mismatch")
    st, res = put_project(api, pid, d, d["revision"], game=d["game"], maps=d["maps"])
    after = {f: raw(f) for f in files}
    check("difficulty: open + save keeps every file byte for byte", st == 200 and after == before, [f for f in files if after[f] != before[f]])
    check("difficulty: the keys are in the files", b'"difficulties"' in after["game.json"] and b'"difficultyShift"' in after[mfile] and b'"atLeast"' in after[mfile])

    # ---- what the loader refuses comes back as problems in its words ----
    def refused(label, change, needle):
        st, d = get_project(api, pid)
        body = {"game": copy(game), "maps": {mid: copy(m)}}
        change(body)
        st, res = put_project(api, pid, d, d["revision"], **body)
        ps = problems()
        check("difficulty: %s is named by the loader" % label, st == 200 and any(needle in p for p in ps), ps)

    def diffs(b):
        return b["game"]["difficulties"]

    refused("a default that is not in the list", lambda b: diffs(b).__setitem__("default", "nope"), "\"default\" names unknown difficulty \"nope\"")
    refused("no default", lambda b: diffs(b).pop("default"), "missing required \"default\"")
    refused("a change rule that does not exist", lambda b: diffs(b).__setitem__("change", "sometimes"), "\"change\" must be \"never\", \"lowerOnly\" or \"free\"")
    refused("an enemy level shift out of range", lambda b: diffs(b)["list"][0]["enemy"].__setitem__("levels", 500), "\"levels\" must be from -98 to 98")
    refused("a percent below zero", lambda b: diffs(b)["list"][0].__setitem__("xpPercent", -1), "\"xpPercent\" must be from 0 to 1000")
    refused("an unknown stat in the added stats", lambda b: diffs(b)["list"][2]["enemy"]["stats"].__setitem__("luck", 1), "unknown stat \"luck\"")
    refused("an unknown key of an entry", lambda b: diffs(b)["list"][1].__setitem__("speed", 1), "unknown key \"speed\"")
    refused("two entries with one id", lambda b: diffs(b)["list"][1].__setitem__("id", "easy"), "duplicate id")
    refused("an empty list", lambda b: diffs(b).__setitem__("list", []), "needs at least one difficulty")
    refused("a stat in percentStats that is not there", lambda b: diffs(b).__setitem__("percentStats", ["luck"]), "\"percentStats\" names unknown stat \"luck\"")
    refused("a map override of an unknown difficulty", lambda b: b["maps"][mid]["difficulty"].__setitem__("extreme", {}), "unknown difficulty \"extreme\"")
    refused("an unknown key in a map override", lambda b: b["maps"][mid]["difficulty"]["hard"].__setitem__("speed", 1), "unknown key \"speed\"")
    refused("a placement limited to an unknown difficulty", lambda b: b["maps"][mid]["units"][1].__setitem__("difficulty", ["extreme"]), "names unknown difficulty \"extreme\"")
    refused("a placement limited to nothing", lambda b: b["maps"][mid]["units"][1].__setitem__("difficulty", []), "must list at least one difficulty")
    refused("a condition on an unknown difficulty", lambda b: b["maps"][mid]["events"][0]["do"][0]["if"][0].__setitem__("difficulty", "extreme"), "names unknown difficulty \"extreme\"")
    refused("an added unit limited to an unknown difficulty", lambda b: b["maps"][mid]["events"][0]["do"][2]["addUnit"].__setitem__("difficulty", ["extreme"]), "names unknown difficulty \"extreme\"")
    refused("a game without difficulties but a unit that names one", lambda b: b["game"].pop("difficulties"), "the game has none")

    # Back to a valid state: nothing lingers.
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], game=game, maps={mid: m})
    check("difficulty: valid data again -> no problems", st == 200 and problems() == [], problems())

    # One unit, different numbers per difficulty (units[1] is hard-only; the two new ones take its square for easy and normal): disjoint lists may share a square and a story name; overlapping ones are refused naming the difficulty.
    def variants(a, b, c):
        vm = copy(m)
        base = copy(m["units"][1])
        base["difficultyShift"] = False
        base.pop("id", None)
        for diff, atk in ((a, 3), (b, 6), (c, 9)):
            if diff is None:
                continue
            u = copy(base)
            u.update({"id": "twin", "difficulty": diff, "stats": {"str": atk}})
            vm["units"].append(u)
        return vm
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], game=game, maps={mid: variants(["easy"], ["normal"], None)})
    check("difficulty: variants of one unit on one square and one id -> 200", st == 200, (st, res))
    check("difficulty: the loader accepts per-difficulty variants", problems() == [], problems())
    st, d = get_project(api, pid)
    check("difficulty: variants come back as written", [u.get("difficulty") for u in d["maps"][mid]["units"] if u.get("id") == "twin"] == [["easy"], ["normal"]], "mismatch")
    st, res = put_project(api, pid, d, d["revision"], game=game, maps={mid: variants(["easy", "normal"], ["normal"], None)})
    ps = problems()
    check("difficulty: overlapping variants are refused naming the difficulty",
          any("already used by" in p and "on difficulty \"normal\"" in p for p in ps), ps)
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], game=game, maps={mid: m})
    check("difficulty: back without variants -> no problems", st == 200 and problems() == [], problems())

    # A campaign scene may read the difficulty too.
    campaign = {"name": "C", "chapters": [{"id": "c1", "name": "One", "map": mid, "after": [
        {"if": [{"difficulty": {"atMost": "normal"}}], "then": [{"say": "A gentle road."}]}]}]}
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], campaign=campaign)
    check("difficulty: a scene's difficulty condition -> 200", st == 200, (st, res))
    check("difficulty: the loader accepts a scene's difficulty condition", problems() == [], problems())
    st, d = get_project(api, pid)
    check("difficulty: GET returns the scene condition as written", d["campaign"]["chapters"][0]["after"] == campaign["chapters"][0]["after"], d.get("campaign"))
    bad = copy(campaign)
    bad["chapters"][0]["after"][0]["if"][0]["difficulty"] = "extreme"
    st, res = put_project(api, pid, d, d["revision"], campaign=bad)
    ps = problems()
    check("difficulty: a scene condition on an unknown difficulty is refused", any("unknown difficulty \"extreme\"" in p for p in ps), ps)
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], campaign=campaign)
    check("difficulty: valid scene again -> no problems", st == 200 and problems() == [], problems())
