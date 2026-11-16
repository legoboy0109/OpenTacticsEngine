"""Weapon ranks and stat caps checks of the ote-editor API test (docs/SRPG.md section 7 "Editor as built", docs/EDITOR_API.md):
`ranks` and `caps` are two more optional object sections like `supports` (the file's whole object; an empty ladder / empty caps
object is written, so the switch stays on; false removes the file; null leaves it alone), and what the editor writes for
weapon ranks, class and character ranks and caps loads in the real loader. Called from editor_api_test.py; uses its helpers.
Starts from a copy of tests/fixtures/srpg-ranks (it shows every piece of data)."""
import json as _json
import os
import shutil
import subprocess

FIXTURE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fixtures", "srpg-ranks")
RANKS = "data/ranks.json"
CAPS = "data/caps.json"
LADDER = {"growth": "use", "ranks": [{"id": "e", "name": "E", "xp": 0}, {"id": "d", "name": "D", "xp": 30}, {"id": "c", "name": "C", "xp": 70},
                                     {"id": "b", "name": "B", "xp": 120}, {"id": "a", "name": "A", "xp": 180}, {"id": "s", "name": "S", "xp": 250}]}


def run_ranks_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "ranks-api"
    shutil.copytree(FIXTURE, os.path.join(games, pid))
    root = os.path.join(games, pid)

    def path(rel):
        return os.path.join(root, rel)

    def raw(rel):
        with open(path(rel), "rb") as f:
            return f.read()

    def problems():
        st, p = api.json("GET", "/api/projects/%s/problems" % pid)
        return p["problems"] if st == 200 else [("status", st)]

    st, d = get_project(api, pid)
    check("ranks: the fixture loads (no problems)", st == 200 and d["problems"] == [], d.get("problems"))
    check("ranks: GET returns the two files' objects",
          d["ranks"] == _json.loads(raw(RANKS).decode()) and d["caps"] == {"caps": {"str": 10, "hp": 40}}, (d["ranks"], d["caps"]))
    check("ranks: weapon, class and character keys come back untouched",
          d["weapons"][1]["rank"] == "d" and d["weapons"][1]["rankXp"] == 2 and d["units"][0]["ranks"] == {"sword": "e"} and
          d["units"][1]["caps"] == {"str": 14} and d["characters"][0]["ranks"] == {"sword": 4} and d["characters"][1]["capAdjust"] == {"str": 2})

    # null / absent leaves both files byte-identical
    before = (raw(RANKS), raw(CAPS))
    st, res = put_project(api, pid, d, d["revision"], ranks=None, caps=None)
    check("ranks: null leaves both files byte-identical", st == 200 and (raw(RANKS), raw(CAPS)) == before, res)

    # the whole object is the unit of writing: a key the editor does not know survives (and the loader names it)
    st, d = get_project(api, pid)
    good = d["ranks"]
    odd = dict(good, zzz_unknown={"keep": [1, 2]})
    put_project(api, pid, d, d["revision"], ranks=odd)
    st, d = get_project(api, pid)
    check("ranks: PUT object -> written pretty-printed, unknown key kept", d["ranks"] == odd and raw(RANKS).decode().startswith('{\n  "growth"'), d["ranks"])
    check("ranks: the loader refuses the unknown key in its own words", any("unknown key" in p["message"] for p in problems()), problems())
    put_project(api, pid, d, d["revision"], ranks=good)

    # wrong types -> 400, nothing written
    st, d = get_project(api, pid)
    before = (raw(RANKS), raw(CAPS))
    for key in ("ranks", "caps"):
        for bad in ([], "x", 5, True):
            st, res = put_project(api, pid, d, d["revision"], terrain=d["terrain"], **{key: bad})
            check("ranks: PUT %s=%r -> 400" % (key, bad), st == 400, (st, res))
    check("ranks: rejected PUTs wrote nothing", (raw(RANKS), raw(CAPS)) == before)

    # broken files: null plus a problem on the file; a PUT repairs
    for key, rel in (("ranks", RANKS), ("caps", CAPS)):
        st, d = get_project(api, pid)
        keep = d[key]
        for label, text in (("invalid JSON", b"{ not json"), ("root is a list", b"[1,2]")):
            with open(path(rel), "wb") as f:
                f.write(text)
            st, d2 = get_project(api, pid)
            check("ranks: %s %s -> null plus a problem" % (key, label), d2[key] is None and any(p["file"] == rel for p in d2["problems"]), d2["problems"])
        put_project(api, pid, d2, d2["revision"], **{key: keep})
        st, d = get_project(api, pid)
        check("ranks: PUT repairs a broken %s file" % key, d[key] == keep and d["problems"] == [], (d[key], d["problems"]))

    # the editor's own edits load: a new ladder, weapon ranks, class and character ranks and caps
    st, d = get_project(api, pid)
    weapons, units, chars, maps = d["weapons"], d["units"], d["characters"], d["maps"]
    weapons[0].update({"rank": "e", "rankXp": 5})
    weapons[2]["rank"] = "s"
    units[0]["ranks"] = {"sword": "d"}
    units[0]["caps"] = {"hp": 99, "str": 20}
    units[1]["ranks"] = {"sword": 100}
    chars[0]["ranks"] = {"sword": "c"}
    chars[1]["capAdjust"] = {"str": -3, "hp": 5}
    for m in maps.values():
        if m.get("units"):
            m["units"][0]["ranks"] = {"sword": "a"}
    caps = {"caps": {"str": 30, "hp": 60, "def": 25}}
    st, res = put_project(api, pid, d, d["revision"], ranks=LADDER, caps=caps, weapons=weapons, units=units, characters=chars, maps=maps)
    check("ranks: PUT a new ladder, caps and all the per-item keys -> 200", st == 200, (st, res))
    check("ranks: the real loader accepts all of it", problems() == [], problems())
    st, d = get_project(api, pid)
    first_map = d["maps"][list(d["maps"])[0]]
    check("ranks: it all round-trips as written", d["ranks"] == LADDER and d["caps"] == caps and d["weapons"][2]["rank"] == "s" and
          first_map["units"][0]["ranks"] == {"sword": "a"}, (d["ranks"], d["caps"]))
    sim = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "build", "ote-sim")
    if os.path.exists(sim):
        r = subprocess.run([sim, root, d["game"]["maps"][0]["id"], "--runs", "1"], capture_output=True, timeout=120)
        check("ranks: ote-sim plays the map with those files", r.returncode == 0, (r.returncode, r.stderr[-300:]))

    # what the screens refuse to offer, the loader refuses too (so Problems shows it in the engine's words)
    for label, rk, word in (
            ("a ladder whose first rank needs experience", {"ranks": [{"id": "e", "name": "E", "xp": 5}]}, "first rank"),
            ("experience that does not rise", {"ranks": [{"id": "e", "name": "E", "xp": 0}, {"id": "d", "name": "D", "xp": 0}]}, "higher than"),
            ("a rank id with capitals", {"ranks": [{"id": "E", "name": "E", "xp": 0}]}, "lowercase")):
        st, d = get_project(api, pid)
        put_project(api, pid, d, d["revision"], ranks=rk)
        check("ranks: %s -> a problem on data/ranks.json" % label, any(p["file"] == RANKS and word in p["message"] for p in problems()), problems())
    st, d = get_project(api, pid)
    put_project(api, pid, d, d["revision"], ranks=LADDER)
    st, d = get_project(api, pid)
    d["weapons"][3]["rank"] = "zz"
    put_project(api, pid, d, d["revision"], weapons=d["weapons"])
    check("ranks: an unknown rank id on a weapon is a problem", any("unknown rank" in p["message"] for p in problems()), problems())
    st, d = get_project(api, pid)
    d["weapons"][3].pop("rank")
    d["weapons"][5]["rank"] = "d"  # the stick has no group
    put_project(api, pid, d, d["revision"], weapons=d["weapons"])
    check("ranks: a rank on a weapon without a group is a problem", any("needs a \"group\"" in p["message"] for p in problems()), problems())
    st, d = get_project(api, pid)
    d["weapons"][5].pop("rank")
    put_project(api, pid, d, d["revision"], weapons=d["weapons"])
    check("ranks: back to no problems", problems() == [], problems())

    # the switch sticks: an empty ladder and empty caps are written, and the game is the same as with no files
    st, d = get_project(api, pid)
    for w in d["weapons"]:
        w.pop("rank", None)
        w.pop("rankXp", None)
    for u in d["units"]:
        u.pop("ranks", None)
        u.pop("caps", None)
    for c in d["characters"]:
        c.pop("ranks", None)
        c.pop("capAdjust", None)
    for m in d["maps"].values():
        for mu in m.get("units", []):
            mu.pop("ranks", None)
    st, res = put_project(api, pid, d, d["revision"], ranks={"growth": "fixed", "ranks": []}, caps={"caps": {}},
                          weapons=d["weapons"], units=d["units"], characters=d["characters"], maps=d["maps"])
    check("ranks: PUT an empty ladder and empty caps -> 200, files written",
          st == 200 and _json.loads(raw(RANKS).decode()) == {"growth": "fixed", "ranks": []} and _json.loads(raw(CAPS).decode()) == {"caps": {}}, res)
    check("ranks: the empty files are no problem for the loader", problems() == [], problems())
    st, d = get_project(api, pid)
    check("ranks: after a reload both stay on", d["ranks"] == {"growth": "fixed", "ranks": []} and d["caps"] == {"caps": {}}, (d["ranks"], d["caps"]))

    # false removes the files; false with no file is fine
    st, res = put_project(api, pid, d, d["revision"], ranks=False, caps=False)
    check("ranks: PUT false removes both files", st == 200 and not os.path.exists(path(RANKS)) and not os.path.exists(path(CAPS)), res)
    st, d = get_project(api, pid)
    check("ranks: afterwards GET is null for both, no problem", d["ranks"] is None and d["caps"] is None and d["problems"] == [], d["problems"])
    st, res = put_project(api, pid, d, d["revision"], ranks=False, caps=False)
    check("ranks: PUT false with no file is fine", st == 200, res)

    # a map may not use the files' names
    for rel in (RANKS, CAPS):
        g2 = dict(d["game"])
        g2["maps"] = list(g2["maps"]) + [{"id": "evil", "file": rel}]
        st, res = put_project(api, pid, d, d["revision"], game=g2, maps=dict(d["maps"], evil={"x": 1}))
        check("ranks: map file %s is reserved -> 400" % rel, st == 400, (st, res))
