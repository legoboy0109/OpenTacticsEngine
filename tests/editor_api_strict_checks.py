"""Unknown keys through the ote-editor API (docs/GAME_FORMAT.md "Strict files"): the loader refuses a key it does not know and names it, the
editor's Problems list shows that message, and the server neither crashes nor strips the key when the project is saved. Covers a key in the
root of game.json, in a row of data/weapons.json and in a map, the "editor" key of game.json that is accepted, and a close key that gets a
"did you mean". Starts from a copy of tests/fixtures/facing. Called from editor_api_test.py; uses its helpers."""
import json
import os
import shutil

FIXTURE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fixtures", "facing")


def run_strict_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "strict-api"
    shutil.copytree(FIXTURE, os.path.join(games, pid))
    root = os.path.join(games, pid)

    def problems():
        st, res = api.json("GET", "/api/projects/%s/problems" % pid)
        return [(p["file"], p["message"]) for p in res["problems"]] if st == 200 else [("?", str(res))]

    def raw(rel):
        with open(os.path.join(root, rel), "rb") as f:
            return f.read()

    def copy(x):
        return json.loads(json.dumps(x))

    st, d = get_project(api, pid)
    check("strict: the fixture loads in the real loader", st == 200 and problems() == [], problems())

    # An unknown key in the root of game.json: reported with file and key (and the closest key), kept by the save.
    game = copy(d["game"])
    game["descripton"] = "typo"
    st, res = put_project(api, pid, d, d["revision"], game=game)
    ps = problems()
    check("strict: a typo in a root key of game.json is a problem naming file and key",
          st == 200 and len(ps) == 1 and ps[0][0] == "game.json" and 'unknown key "descripton"' in ps[0][1] and 'did you mean "description"?' in ps[0][1], ps)
    check("strict: the key is still in the file after the save (the server never strips what it does not know)", b'"descripton"' in raw("game.json"))
    st, d2 = get_project(api, pid)
    check("strict: GET still works and the problem comes with it",
          st == 200 and d2["game"].get("descripton") == "typo" and any('unknown key "descripton"' in p["message"] for p in d2["problems"]), d2.get("problems"))
    st, res = put_project(api, pid, d2, d2["revision"], game=d2["game"])
    check("strict: open + save with the problem keeps the key and does not crash", st == 200 and b'"descripton"' in raw("game.json") and problems() != [], (st, res))

    # Remove it again: the game is valid.
    st, d3 = get_project(api, pid)
    game = copy(d3["game"])
    game.pop("descripton")
    st, res = put_project(api, pid, d3, d3["revision"], game=game)
    check("strict: without the key the game loads again", st == 200 and problems() == [], problems())

    # "editor" is the web editor's own key in game.json: accepted, and kept.
    st, d4 = get_project(api, pid)
    game = copy(d4["game"])
    game["editor"] = {"formulas": {"damage": {"preset": "plain"}}}
    st, res = put_project(api, pid, d4, d4["revision"], game=game)
    check("strict: the editor's own \"editor\" key is not a problem and is kept", st == 200 and problems() == [] and b'"editor"' in raw("game.json"), problems())
    st, d4 = get_project(api, pid)
    game = copy(d4["game"])
    game.pop("editor")
    put_project(api, pid, d4, d4["revision"], game=game)

    # A row of a data file.
    st, d5 = get_project(api, pid)
    weapons = copy(d5["weapons"])
    weapons[0]["rnage"] = [1, 1]
    st, res = put_project(api, pid, d5, d5["revision"], weapons=weapons)
    ps = problems()
    check("strict: a typo in a weapon row names the weapon, the key and the closest key",
          st == 200 and len(ps) == 1 and ps[0][0] == "data/weapons.json" and 'weapon "%s": unknown key "rnage"' % weapons[0]["id"] in ps[0][1]
          and 'did you mean "range"?' in ps[0][1], ps)
    check("strict: the weapon's unknown key survives the save", b'"rnage"' in raw("data/weapons.json"))
    st, d5 = get_project(api, pid)
    weapons = copy(d5["weapons"])
    weapons[0].pop("rnage")
    put_project(api, pid, d5, d5["revision"], weapons=weapons)
    check("strict: ... and the game is valid once it is removed", problems() == [], problems())

    # A map.
    st, d6 = get_project(api, pid)
    mid = sorted(d6["maps"].keys())[0]
    maps = copy(d6["maps"])
    maps[mid]["unitz"] = []
    st, res = put_project(api, pid, d6, d6["revision"], maps=maps)
    ps = problems()
    check("strict: a typo in a root key of a map is a problem naming the map file and the key",
          st == 200 and len(ps) == 1 and 'unknown key "unitz"' in ps[0][1] and 'did you mean "units"?' in ps[0][1], ps)
    check("strict: the map's unknown key survives the save", b'"unitz"' in raw([m["file"] for m in d6["game"]["maps"] if m["id"] == mid][0]))
    st, d6 = get_project(api, pid)
    maps = copy(d6["maps"])
    maps[mid].pop("unitz")
    put_project(api, pid, d6, d6["revision"], maps=maps)
    check("strict: ... and the game is valid once it is removed (map)", problems() == [], problems())
