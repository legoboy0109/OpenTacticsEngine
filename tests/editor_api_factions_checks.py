"""Factions through the ote-editor API (docs/ECONOMY.md "Factions"): data/factions.json comes back as "factions" (null when the file is absent),
is written back like the other lists (an empty list removes the file), a map side's "faction" key passes through, and the real loader
(GET .../problems) accepts the data and names what it refuses. Starts from a copy of tests/fixtures/factions. Called from
editor_api_test.py; uses its helpers."""
import json
import os
import shutil

FIXTURE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fixtures", "factions")


def run_factions_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "factions-api"
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
    check("factions: the fixture loads in the real loader", st == 200 and problems() == [], problems())
    fac = {f["id"]: f for f in d["factions"]} if d.get("factions") else {}
    check("factions: GET carries the list with every key",
          sorted(fac) == ["free", "horde", "order"] and fac["order"]["start"] == {"resources": {"gold": 50}}
          and fac["order"]["modifiers"][0]["filter"] == {"tag": "infantry"} and fac["horde"]["ai"]["weights"]["dmg"] == 2
          and fac["order"]["techs"] == ["drill", "forging"] and fac["order"]["tags"] == ["human", "disciplined"], sorted(fac))
    check("factions: a map side's \"faction\" comes back with the map",
          d["maps"]["field"]["sides"][0]["faction"] == "order" and d["maps"]["pick"]["sides"][0]["faction"] == "choose", "missing")

    before = {f: raw(f) for f in ("data/factions.json", "maps/field.json", "maps/pick.json")}
    st, res = put_project(api, pid, d, d["revision"])
    after = {f: raw(f) for f in before}
    check("factions: open + save keeps every key in the files",
          st == 200 and all(k in after["data/factions.json"] for k in (b'"units"', b'"structures"', b'"techs"', b'"start"', b'"modifiers"', b'"ai"', b'"tags"'))
          and b'"faction": "choose"' in after["maps/pick.json"] and b'"faction": "order"' in after["maps/field.json"], (st, [f for f in before if after[f] != before[f]]))
    st, d2 = get_project(api, pid)
    check("factions: ... the problems are still none and the data read back is the same",
          problems() == [] and d2["factions"] == d["factions"] and d2["maps"] == d["maps"], "differs")

    def refused(label, change, needle):
        st, cur = get_project(api, pid)
        body = {"factions": copy(cur["factions"]), "maps": copy(cur["maps"])}
        change(body)
        st, res = put_project(api, pid, cur, cur["revision"], **body)
        ps = problems()
        check("factions: %s is named by the loader" % label, st == 200 and any(needle in p for p in ps), (st, ps))
        st, cur = get_project(api, pid)
        put_project(api, pid, cur, cur["revision"], factions=copy(d["factions"]), maps=copy(d["maps"]))  # back to valid

    def faction(b, fid):
        return next(f for f in b["factions"] if f["id"] == fid)

    refused("an unknown unit", lambda b: faction(b, "order").__setitem__("units", ["soldier", "dragon"]), "\"units\" names unknown unit type \"dragon\"")
    refused("an unknown structure", lambda b: faction(b, "order").__setitem__("structures", ["castle"]), "\"structures\" names unknown structure \"castle\"")
    refused("an unknown technology", lambda b: faction(b, "order").__setitem__("techs", ["magic"]), "\"techs\" names unknown technology \"magic\"")
    refused("an unknown skill", lambda b: faction(b, "order").__setitem__("modifiers", ["nope"]), "names unknown skill \"nope\"")
    refused("a duplicate id", lambda b: faction(b, "horde").__setitem__("id", "order"), "duplicate id")
    refused("an unknown key", lambda b: faction(b, "horde").__setitem__("colour", 1), "unknown key \"colour\"")
    refused("an unknown weight", lambda b: faction(b, "horde")["ai"]["weights"].__setitem__("cunning", 1), "unknown key \"cunning\"")
    refused("a side naming an unknown faction", lambda b: b["maps"]["field"]["sides"][0].__setitem__("faction", "elves"), "\"faction\" names unknown faction \"elves\"")

    # An empty list removes the file (the loader would refuse an empty one), and then "choose" has nothing to choose from.
    st, cur = get_project(api, pid)
    body = {"factions": [], "maps": copy(cur["maps"])}
    for m in body["maps"].values():
        for s in m["sides"]:
            s.pop("faction", None)
    st, res = put_project(api, pid, cur, cur["revision"], **body)
    st2, none = get_project(api, pid)
    check("factions: an empty list removes data/factions.json and the game still loads",
          st == 200 and not os.path.exists(os.path.join(root, "data", "factions.json")) and none["factions"] is None and problems() == [], (st, problems()))
    st, cur = get_project(api, pid)
    put_project(api, pid, cur, cur["revision"], factions=copy(d["factions"]), maps=copy(d["maps"]))
    check("factions: written again -> valid", problems() == [] and os.path.exists(os.path.join(root, "data", "factions.json")), problems())
