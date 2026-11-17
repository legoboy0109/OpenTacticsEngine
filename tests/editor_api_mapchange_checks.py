"""Map changes and fog options through the ote-editor API (docs/EVENTS.md "Changing the map", docs/ECONOMY.md "Fog of war"): a map's `changes`
(squares and structure swaps), the commands setTerrain / applyChange / revertChange / setFog / setVision, `fogVision` / `aiSeesAll` (map and
side), terrain `blocksSight`, structure `light`, a door's `word`, a torch item's `light` and `goesFor: walls` are passed through like every
other key: they open and save byte for byte, the real loader (GET .../problems) accepts them and names what it refuses.
Called from editor_api_test.py; uses its helpers."""
import json
import os


def run_mapchange_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "mapchange-test"
    st, doc = api.json("POST", "/api/projects", {"id": pid, "name": "Map changes", "template": "economy-tactics"})
    check("mapchange: create scratch project", st == 201, (st, doc))
    root = os.path.join(games, pid)

    def raw(rel):
        with open(os.path.join(root, rel), "rb") as f:
            return f.read()

    def problems():
        st, res = api.json("GET", "/api/projects/%s/problems" % pid)
        return res["problems"] if st == 200 else [{"file": "?", "message": str(res)}]

    def copy(x):
        return json.loads(json.dumps(x))

    st, d = get_project(api, pid)
    mid = d["game"]["maps"][0]["id"]
    mfile = d["game"]["maps"][0]["file"]
    m = copy(d["maps"][mid])
    terrain = copy(d["terrain"])
    structures = copy(d["structures"])
    items = copy(d["items"]) if d.get("items") is not None else []
    move_types = [t["id"] for t in d["game"].get("moveTypes", [])]
    width, height = m["grid"]["width"], m["grid"]["height"]
    t0, t1 = terrain[0]["id"], terrain[1]["id"]
    sides = m["sides"]
    ai_side = next((s for s in sides if s.get("controller") == "ai"), sides[1])
    human_side = next(s for s in sides if s.get("controller") != "ai")

    # A free corner for a named structure, a twin kind of the same size, and a breakable wall kind.
    taken = {(u["x"], u["y"]) for u in m.get("units", [])} | {(s["x"], s["y"]) for s in m.get("structures", [])}
    spot = next(((x, y) for y in range(height - 1, -1, -1) for x in range(width - 1, -1, -1) if (x, y) not in taken))
    kind = structures[0]
    twin = copy(kind)
    twin["id"] = "twin_kind"
    twin["name"] = "Twin kind"
    twin["light"] = 3
    for k in ("produces", "income", "capturePoints", "upgrades", "research", "sells"):
        twin.pop(k, None)
    wall = {"id": "crack_wall", "name": "Cracked wall", "moveCost": {mt: 0 for mt in move_types},
            "door": {"opens": "twin_kind", "hp": 12, "by": ["break"], "word": "wall"}}
    structures += [twin, wall]
    terrain[0]["blocksSight"] = True
    items.append({"id": "torch_item", "name": "Torch", "kind": "usable", "uses": 2, "light": {"radius": 3, "turns": 3, "shrink": 1}})
    m["fog"] = True
    m["fogVision"] = -1
    m["aiSeesAll"] = True
    for s in m["sides"]:
        if s["id"] == ai_side["id"]:
            s["aiSeesAll"] = False
    m["structures"] = m.get("structures", []) + [{"type": kind["id"], "x": spot[0], "y": spot[1], "id": "gate1"}]
    m["units"][0]["id"] = "scout"
    m["changes"] = {
        "bridge": {"cells": [{"x": 0, "y": 0, "terrain": t1}, {"x": 1, "y": 0, "terrain": t1}],
                   "structures": [{"id": "gate1", "kind": "twin_kind"}]},
        "cavein": {"cells": [{"x": 2, "y": 0, "terrain": t0}]},
    }
    m["events"] = [
        {"id": "e_apply", "when": {"turn": 2}, "do": [{"applyChange": "bridge"}, {"setTerrain": t1, "area": {"x": 0, "y": 1, "w": 2, "h": 1}},
                                                       {"setTerrain": t0, "cells": [[2, 1], [3, 1]]}]},
        {"id": "e_back", "when": {"turn": 3}, "do": [{"revertChange": "bridge"}, {"setFog": False}, {"setVision": 2}, {"setFog": True}]},
    ]
    st, res = put_project(api, pid, d, d["revision"], terrain=terrain, structures=structures, items=items, maps={mid: m})
    check("mapchange: PUT of change keys -> 200", st == 200 and "revision" in res, (st, res))
    ps = problems()
    check("mapchange: the real loader accepts them", ps == [], ps)

    files = [mfile, "data/terrain.json", "data/structures.json", "data/items.json"]
    before = {f: raw(f) for f in files}
    st, d = get_project(api, pid)
    check("mapchange: GET returns the keys as written",
          d["maps"][mid]["changes"] == m["changes"] and d["maps"][mid]["events"] == m["events"] and d["maps"][mid]["fogVision"] == -1
          and d["maps"][mid]["aiSeesAll"] is True and d["terrain"][0]["blocksSight"] is True
          and next(s for s in d["structures"] if s["id"] == "twin_kind")["light"] == 3
          and next(s for s in d["structures"] if s["id"] == "crack_wall")["door"]["word"] == "wall"
          and next(i for i in d["items"] if i["id"] == "torch_item")["light"] == {"radius": 3, "turns": 3, "shrink": 1}, "mismatch")
    st, res = put_project(api, pid, d, d["revision"], terrain=d["terrain"], structures=d["structures"], items=d["items"], maps=d["maps"])
    after = {f: raw(f) for f in files}
    check("mapchange: open + save keeps every file byte for byte", st == 200 and after == before, [f for f in files if after[f] != before[f]])
    check("mapchange: the changes block is in the map file", b'"changes"' in after[mfile] and b'"bridge"' in after[mfile])

    # ---- what the loader refuses comes back as problems naming the key ----
    def refused(label, change, needle):
        st, d = get_project(api, pid)
        body = {"terrain": copy(terrain), "structures": copy(structures), "items": copy(items), "maps": {mid: copy(m)}}
        change(body)
        st, res = put_project(api, pid, d, d["revision"], **body)
        ps = problems()
        check("mapchange: %s is named by the loader" % label, st == 200 and any(needle in p["message"] for p in ps), ps)

    def mp(b):
        return b["maps"][mid]

    refused("a change square outside the map", lambda b: mp(b)["changes"]["bridge"]["cells"][0].__setitem__("x", width), "outside the map")
    refused("a change square with unknown terrain", lambda b: mp(b)["changes"]["bridge"]["cells"][0].__setitem__("terrain", "lava"), "unknown terrain")
    refused("a square twice in one change", lambda b: mp(b)["changes"]["bridge"]["cells"].append(dict(mp(b)["changes"]["bridge"]["cells"][0])), "twice")
    refused("an empty change", lambda b: mp(b)["changes"].__setitem__("cavein", {"cells": []}), "changes nothing")
    refused("a change name with a space", lambda b: mp(b)["changes"].__setitem__("a b", {"cells": [{"x": 0, "y": 0, "terrain": t1}]}), "letters, digits")
    refused("a structure swap of another size", lambda b: next(s for s in b["structures"] if s["id"] == "twin_kind").__setitem__("width", 3), "size")
    refused("a structure swap naming nobody", lambda b: mp(b)["changes"]["bridge"]["structures"][0].__setitem__("id", "nobody"), "not the \"id\" of a structure")
    refused("an applyChange of an unknown change", lambda b: mp(b)["events"][0]["do"][0].__setitem__("applyChange", "nope"), "not one of this map")
    refused("a revertChange of an unknown change", lambda b: mp(b)["events"][1]["do"][0].__setitem__("revertChange", "nope"), "not one of this map")
    refused("a setTerrain with both area and cells", lambda b: mp(b)["events"][0]["do"][1].__setitem__("cells", [[0, 0]]), "one of them")
    refused("a setTerrain of unknown terrain", lambda b: mp(b)["events"][0]["do"][1].__setitem__("setTerrain", "lava"), "unknown terrain")
    refused("a setTerrain square outside the map", lambda b: mp(b)["events"][0]["do"][2].__setitem__("cells", [[width, 0]]), "outside the map")
    refused("a setVision of 11", lambda b: mp(b)["events"][1]["do"][2].__setitem__("setVision", 11), "-10 to 10")
    refused("a setFog that is not a boolean", lambda b: mp(b)["events"][1]["do"][1].__setitem__("setFog", "no"), "setFog")
    refused("a fogVision of 11", lambda b: mp(b).__setitem__("fogVision", 11), "fogVision")
    refused("an aiSeesAll that is not a boolean", lambda b: mp(b).__setitem__("aiSeesAll", 1), "true or false")

    def no_fog(b):
        mp(b).pop("fog")

    refused("fogVision without fog", no_fog, "fog")
    refused("aiSeesAll on a human side", lambda b: next(s for s in mp(b)["sides"] if s["id"] == human_side["id"]).__setitem__("aiSeesAll", True), "computer sides")
    refused("a structure light of 11", lambda b: next(s for s in b["structures"] if s["id"] == "twin_kind").__setitem__("light", 11), "light")
    refused("a torch without a radius", lambda b: next(i for i in b["items"] if i["id"] == "torch_item").__setitem__("light", {"turns": 2}), "radius")
    refused("a torch that shrinks as much as its radius", lambda b: next(i for i in b["items"] if i["id"] == "torch_item").__setitem__("light", {"radius": 2, "shrink": 2}), "shrink")
    refused("a torch for an ally", lambda b: next(i for i in b["items"] if i["id"] == "torch_item").__setitem__("target", "ally"), "self")
    refused("a door word that is not text", lambda b: next(s for s in b["structures"] if s["id"] == "crack_wall")["door"].__setitem__("word", 5), "word")
    refused("terrain blocksSight as a number", lambda b: b["terrain"][0].__setitem__("blocksSight", 1), "true or false")

    def walls(b):
        u = next(u for u in mp(b)["units"] if u["side"] == ai_side["id"])
        u["goesFor"] = ["walls"]

    st, d = get_project(api, pid)
    body = {"terrain": copy(terrain), "structures": copy(structures), "items": copy(items), "maps": {mid: copy(m)}}
    walls(body)
    st, res = put_project(api, pid, d, d["revision"], **body)
    ps = problems()
    check("mapchange: goesFor walls is accepted by the loader", st == 200 and ps == [], ps)

    # Back to a valid state: nothing lingers.
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], terrain=terrain, structures=structures, items=items, maps={mid: m})
    ps = problems()
    check("mapchange: valid data again -> no problems", st == 200 and ps == [], ps)
