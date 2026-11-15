"""Economy data in the ote-editor API test (docs/ECONOMY.md, docs/EDITOR_API.md): game files that already use upgrades,
research, repair, fuel, ammunition and supply must open and save with every key kept, and data/research.json is read
and written like data/items.json (with one difference: the loader refuses an empty list, so [] removes the file).
Called from editor_api_test.py; uses its helpers."""
import json
import os


def rawb(path):
    with open(path, "rb") as f:
        return f.read()


def run_economy_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "eco-test"
    st, doc = api.json("POST", "/api/projects", {"id": pid, "name": "Eco", "template": "economy-tactics"})
    check("economy: create scratch project", st == 201, (st, doc))
    root = os.path.join(games, pid)
    rfile = os.path.join(root, "data", "research.json")
    if os.path.exists(rfile):
        os.remove(rfile)

    def has_problem(d, f="data/research.json"):
        return any(p["file"] == f for p in d["problems"])

    # ---- 1. a game that already uses the new keys: nothing is dropped on open + save ----
    with open(os.path.join(root, "game.json")) as f:
        game = json.load(f)
    game["rules"]["supply"] = {"repair": {"amount": 20, "costShare": 50}, "structures": True}
    structures = [
        {"id": "city", "name": "City", "color": "#b7b7b7", "glyph": "c", "props": {"defense": 3}, "capturePoints": 20,
         "income": {"funds": 1000}, "research": True,
         "upgrades": [{"to": "metro", "cost": {"funds": 3000}, "requires": "masonry"}], "repairs": ["tank", "armored"]},
        {"id": "metro", "name": "Metropolis", "color": "#999999", "glyph": "M", "capturePoints": 20, "income": {"funds": 2000}, "repairs": []},
        {"id": "factory", "name": "Factory", "color": "#9a9a9a", "glyph": "F", "capturePoints": 20, "produces": ["infantry", "tank"]},
        {"id": "hq", "name": "Headquarters", "color": "#d4af37", "glyph": "H", "width": 2, "height": 2, "capturePoints": 20, "tags": ["hq"],
         "futureKey": {"keep": [1, 2]}},
    ]
    units = [
        {"id": "infantry", "name": "Infantry", "glyph": "i", "moveType": "foot", "stats": {"hp": 100, "mov": 3},
         "weapons": ["rifle"], "cost": {"funds": 1000}, "canCapture": True},
        {"id": "tank", "name": "Tank", "glyph": "T", "moveType": "tread", "stats": {"hp": 100, "mov": 6}, "weapons": ["tank_cannon"],
         "cost": {"funds": 7000}, "tags": ["armored"], "fuel": 60, "fuelPerTurn": 5, "supplies": True,
         "sounds": {"step": "tread_roll"}},
    ]
    weapons = [
        {"id": "rifle", "name": "Rifle", "versus": {"infantry": 55, "tank": 5}},
        {"id": "tank_cannon", "name": "Tank Cannon", "ammo": 6, "versus": {"infantry": 75, "tank": 55}},
    ]
    research = [
        {"id": "tools", "name": "Tools", "description": "Basic tools.", "cost": {"funds": 1000}, "turns": 0},
        {"id": "masonry", "name": "Masonry", "description": "Cities can be built up.", "cost": {"funds": 4000}, "turns": 2,
         "requires": ["tools"], "unlocks": {"units": ["tank"]}, "bonus": {"types": ["infantry"], "stats": {"hp": 10}}},
    ]
    # The server writes the files in its own layout; the first PUT lets it do that, the rest must not change a byte.
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], game=game, structures=structures, units=units, weapons=weapons, research=research)
    check("economy: PUT of a game that uses the new keys -> 200", st == 200 and "revision" in res, (st, res))
    st, pr = api.json("GET", "/api/projects/%s/problems" % pid)
    # (the template map may use structure kinds the test list leaves out: only the economy files matter here)
    econ = [p for p in pr["problems"] if p["file"] in ("game.json", "data/structures.json", "data/units.json", "data/weapons.json", "data/research.json")]
    check("economy: the game files load with no problems", econ == [], econ)

    files = ["game.json", "data/structures.json", "data/units.json", "data/weapons.json", "data/research.json"]
    before = {f: rawb(os.path.join(root, f)) for f in files}
    st, d = get_project(api, pid)
    check("economy: GET returns the keys as written",
          d["structures"] == structures and d["units"] == units and d["weapons"] == weapons and d["research"] == research
          and d["game"]["rules"]["supply"] == game["rules"]["supply"], (d["structures"], d["research"]))
    check("economy: key order is kept in GET",
          list(d["structures"][0].keys()) == list(structures[0].keys()) and list(d["units"][1].keys()) == list(units[1].keys())
          and list(d["research"][1].keys()) == list(research[1].keys()))
    st, res = put_project(api, pid, d, d["revision"], structures=d["structures"], units=d["units"], weapons=d["weapons"],
                          research=d["research"], items=d["items"])
    after = {f: rawb(os.path.join(root, f)) for f in files}
    check("economy: open + save keeps every file byte for byte", st == 200 and after == before,
          [f for f in files if after[f] != before[f]])
    # An edit elsewhere in units.json rewrites the file: every economy key and the unknown one must come through.
    d["units"][0]["name"] = "Infantry!"
    st, res = put_project(api, pid, d, d["revision"], units=d["units"])
    text = rawb(os.path.join(root, "data", "units.json"))
    check("economy: an edit elsewhere in units.json keeps fuel, fuelPerTurn, supplies and unknown keys",
          st == 200 and b'"fuel": 60' in text and b'"fuelPerTurn": 5' in text and b'"supplies": true' in text and b'"sounds"' in text)
    check("economy: unknown key on a structure survives", b'"futureKey"' in after["data/structures.json"])

    # ---- 2. data/research.json like data/items.json ----
    os.remove(rfile)
    st, d = get_project(api, pid)
    check("economy: absent research file -> null, no problem",
          st == 200 and "research" in d and d["research"] is None and not has_problem(d))
    rev0 = d["revision"]
    st, res = put_project(api, pid, d, d["revision"], game=d["game"])
    check("economy: PUT without the key does not create the file", st == 200 and not os.path.exists(rfile), res)
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], research=None)
    check("economy: PUT null does not create the file", st == 200 and not os.path.exists(rfile), res)

    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], research=research)
    check("economy: PUT list -> 200", st == 200 and "revision" in res, (st, res))
    raw = rawb(rfile)
    check("economy: written as {\"technologies\": [...]} pretty-printed",
          raw.decode().startswith('{\n  "technologies": [\n') and raw.endswith(b"}\n"))
    check("economy: no temp file left", not [n for n in os.listdir(os.path.dirname(rfile)) if n.endswith(".ote-tmp")])
    st, d = get_project(api, pid)
    check("economy: GET returns the list (under \"research\")", d["research"] == research, d["research"])
    check("economy: revision changed after the file appeared", d["revision"] != rev0 and d["revision"] == res["revision"])
    rev1 = d["revision"]
    with open(rfile, "ab") as f:
        f.write(b"\n")
    st, d = get_project(api, pid)
    check("economy: editing the file changes the revision", d["revision"] != rev1)

    # absent / null leave an existing file byte-identical; wrong types are refused
    st, res = put_project(api, pid, d, d["revision"], research=research)
    before_r = rawb(rfile)
    st, d = get_project(api, pid)
    st, r1 = put_project(api, pid, d, d["revision"])
    st, d = get_project(api, pid)
    st, r2 = put_project(api, pid, d, d["revision"], research=None)
    check("economy: absent or null leaves the file byte-identical", rawb(rfile) == before_r and r1.get("revision") and r2.get("revision"))
    st, d = get_project(api, pid)
    for bad in ({"technologies": []}, "x", 5, True):
        st, res = put_project(api, pid, d, d["revision"], research=bad, terrain=d["terrain"])
        check("economy: PUT research %r -> 400" % (bad,), st == 400, (st, res))
    check("economy: rejected PUT wrote nothing", rawb(rfile) == before_r)

    # broken files: null plus a problem on the file, the rest of the project still loads
    for label, text in (("invalid JSON", b"{ not json"), ("root is a list", b"[1,2]"),
                        ("technologies not a list", b'{"technologies": 5}'), ("root without key", b"{}")):
        with open(rfile, "wb") as f:
            f.write(text)
        st, d = get_project(api, pid)
        check("economy: %s -> null plus a problem on the file" % label,
              st == 200 and d["research"] is None and has_problem(d) and isinstance(d["structures"], list), (d["research"], d["problems"]))
    st, res = put_project(api, pid, d, d["revision"], research=research)
    st, d = get_project(api, pid)
    check("economy: PUT repairs a broken file", st == 200 and d["research"] == research and not has_problem(d))

    # the loader refuses an empty list, so [] removes the file (the editor sends [] after the last technology is deleted)
    st, res = put_project(api, pid, d, d["revision"], research=[])
    check("economy: PUT [] removes the file", st == 200 and not os.path.exists(rfile), res)
    st, d = get_project(api, pid)
    check("economy: after that GET gives null and no problem on the file", d["research"] is None and not has_problem(d))
    st, res = put_project(api, pid, d, d["revision"], research=[])
    check("economy: PUT [] with no file is fine", st == 200 and not os.path.exists(rfile), res)

    # saved even when the loader may dislike it; the real loader's words come back as problems
    odd = [{"id": "a", "name": "A", "requires": ["b"]}, {"id": "b", "name": "B", "requires": ["a"]}]
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], research=odd)
    st, d = get_project(api, pid)
    check("economy: saved even if the loader dislikes it", st == 200 and d["research"] == odd, d["research"])
    st, pr = api.json("GET", "/api/projects/%s/problems" % pid)
    check("economy: the loader names the prerequisite loop", any("loop" in p["message"] for p in pr["problems"]), pr)

    # reserved file name
    g2 = dict(d["game"])
    g2["maps"] = list(g2["maps"]) + [{"id": "evil", "file": "data/research.json"}]
    st, res = put_project(api, pid, d, d["revision"], game=g2, maps=dict(d["maps"], evil={"x": 1}))
    check("economy: map file data/research.json is reserved -> 400", st == 400, (st, res))
