"""The tile layer through the ote-editor API (docs/GRAPHICS.md "Tile layer (hand-painted maps)"): graphics.json `tilesets`, a map's `tileLayer`
(one string per row, four hex digits per square) and the tiles in `changes` and `setTerrain` commands are passed through like every other key
(they open and save byte for byte), the real loader (GET .../problems) accepts them and names what it refuses; the helper keeps working when a
tile layer is present. Called from editor_api_test.py; uses its helpers. The test sheet is made by tools/art/make_test_tileset.py."""
import importlib.util
import json
import os
import shutil
from ote_testkit import scratch_dir  # noqa: E402


def run_tiles_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "tiles-test"
    st, doc = api.json("POST", "/api/projects", {"id": pid, "name": "Tiles", "template": "economy-tactics"})
    check("tiles: create scratch project", st == 201, (st, doc))
    root = os.path.join(games, pid)
    # Templates ship starter art; these checks start from a project without any.
    shutil.rmtree(os.path.join(root, "art"), ignore_errors=True)
    if os.path.exists(os.path.join(root, "data", "graphics.json")):
        os.remove(os.path.join(root, "data", "graphics.json"))

    def raw(rel):
        with open(os.path.join(root, rel), "rb") as f:
            return f.read()

    def problems():
        st, res = api.json("GET", "/api/projects/%s/problems" % pid)
        return res["problems"] if st == 200 else [{"file": "?", "message": str(res)}]

    def copy(x):
        return json.loads(json.dumps(x))

    # The sheet: 8 x 4 tiles of 16 x 16 px (128 x 64), made by the generator script.
    here = os.path.dirname(os.path.abspath(__file__))
    spec = importlib.util.spec_from_file_location("make_test_tileset", os.path.join(here, "..", "tools", "art", "make_test_tileset.py"))
    gen = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(gen)
    work = scratch_dir('editor_api_tiles')
    try:
        sheet = os.path.join(work, "tiles.png")
        gen.write_sheet(sheet)
        with open(sheet, "rb") as f:
            png = f.read()
    finally:
        shutil.rmtree(work, ignore_errors=True)
    st, hdrs, data = api.call("PUT", "/api/projects/%s/art/tiles.png" % pid, raw_body=png, headers={"Content-Type": "image/png"})
    up = json.loads(data) if data else {}
    check("tiles: the test sheet uploads as 128 x 64", st == 201 and up.get("width") == 128 and up.get("height") == 64, (st, up))

    st, d = get_project(api, pid)
    mid = d["game"]["maps"][0]["id"]
    mfile = d["game"]["maps"][0]["file"]
    m = copy(d["maps"][mid])
    terrain = copy(d["terrain"])
    w, h = m["grid"]["width"], m["grid"]["height"]
    t0, t1 = terrain[0]["id"], terrain[1]["id"]
    graphics = {"sheets": [{"id": "tiles", "file": "art/tiles.png", "cellWidth": 16, "cellHeight": 16}],
                "tilesets": {"test": {"sheet": "tiles", "animations": {"8": {"frames": [8, 9, 10], "ms": 250}}}}}

    def good_row(y):
        return "".join("...." if (x + y) % 5 == 0 else "%04x" % ((x * 3 + y * 7) % 32) for x in range(w))

    m["tileLayer"] = {"tileset": "test", "cells": [good_row(y) for y in range(h)]}
    m["changes"] = {"dry": {"cells": [{"x": 0, "y": 0, "terrain": t1, "tile": 5}, {"x": 1, "y": 0, "terrain": t1},
                                      {"x": 2, "y": 0, "terrain": t1, "keepTile": True}, {"x": 3, "y": 0, "terrain": t1, "tile": None}]},
                    "kept": {"keepTile": True, "cells": [{"x": 0, "y": 1, "terrain": t1}]}}
    m["events"] = [{"id": "e_tiles", "when": {"turn": 2}, "do": [{"applyChange": "dry"}, {"setTerrain": t1, "cells": [[1, 1]], "tile": 7},
                                                                   {"setTerrain": t1, "cells": [[2, 1]], "keepTile": True},
                                                                   {"setTerrain": t0, "cells": [[3, 1]], "tile": -1}]}]
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], graphics=graphics, maps={mid: m})
    check("tiles: PUT of a tileset, a tile layer and tiles in changes -> 200", st == 200 and "revision" in res, (st, res))
    ps = problems()
    check("tiles: the real loader accepts them", ps == [], ps)

    files = [mfile, "data/graphics.json"]
    before = {f: raw(f) for f in files}
    st, d = get_project(api, pid)
    check("tiles: GET returns the keys as written",
          d["maps"][mid]["tileLayer"] == m["tileLayer"] and d["maps"][mid]["changes"] == m["changes"] and d["maps"][mid]["events"] == m["events"]
          and d["graphics"]["tilesets"] == graphics["tilesets"], "mismatch")
    st, res = put_project(api, pid, d, d["revision"], graphics=d["graphics"], maps=d["maps"])
    after = {f: raw(f) for f in files}
    check("tiles: open + save keeps every file byte for byte", st == 200 and after == before, [f for f in files if after[f] != before[f]])
    check("tiles: the layer is one string per row in the map file", raw(mfile).count(good_row(0).encode()) == 1)

    # ---- what the loader refuses comes back as problems naming the key ----
    def refused(label, change, needle):
        st, d = get_project(api, pid)
        body = {"graphics": copy(graphics), "maps": {mid: copy(m)}}
        change(body)
        st, res = put_project(api, pid, d, d["revision"], **body)
        ps = problems()
        check("tiles: %s is named by the loader" % label, st == 200 and any(needle in p["message"] for p in ps), ps)

    def mp(b):
        return b["maps"][mid]

    refused("an unknown tileset", lambda b: mp(b)["tileLayer"].__setitem__("tileset", "nope"), "unknown tileset \"nope\"")
    refused("a layer with too few rows", lambda b: mp(b)["tileLayer"]["cells"].pop(), "rows but the grid height")
    refused("a row of the wrong length", lambda b: mp(b)["tileLayer"]["cells"].__setitem__(0, "0001"), "characters but")
    refused("a square that is not hex", lambda b: mp(b)["tileLayer"]["cells"].__setitem__(0, "zzzz" + good_row(0)[4:]), "is not four hex digits")
    refused("a tile outside the sheet", lambda b: mp(b)["tileLayer"]["cells"].__setitem__(0, "0020" + good_row(0)[4:]), "outside tileset")
    refused("a tileLayer that is a list", lambda b: mp(b).__setitem__("tileLayer", []), "must be an object")
    refused("an unknown key in the layer", lambda b: mp(b)["tileLayer"].__setitem__("extra", 1), "unknown key \"extra\"")
    refused("a change tile outside the sheet", lambda b: mp(b)["changes"]["dry"]["cells"][0].__setitem__("tile", 99), "\"tile\" must be")
    refused("a change keepTile that is not true/false", lambda b: mp(b)["changes"]["kept"].__setitem__("keepTile", 3), "keepTile")
    refused("a setTerrain tile outside the sheet", lambda b: mp(b)["events"][0]["do"][1].__setitem__("tile", 99), "\"tile\" must be")
    refused("a tileset of an unknown image", lambda b: b["graphics"]["tilesets"]["test"].__setitem__("sheet", "nope"), "unknown sheet \"nope\"")
    refused("an animation frame outside the sheet", lambda b: b["graphics"]["tilesets"]["test"]["animations"]["8"].__setitem__("frames", [8, 99]), "frame 2")
    refused("an animation that is not a tile number", lambda b: b["graphics"]["tilesets"]["test"]["animations"].__setitem__("water", {"frames": [1, 2]}), "tile number")

    def no_layer(b):
        mp(b).pop("tileLayer")

    refused("a change tile on a map with no layer", no_layer, "needs a tile layer")

    # The layer as a list of numbers per row (-1 / null = none) is read too; the helper hands it back as it was written.
    st, d = get_project(api, pid)
    alt = copy(m)
    alt["tileLayer"]["cells"] = [[-1 if (x + y) % 5 == 0 else (x * 3 + y * 7) % 32 for x in range(w)] for y in range(h)]
    st, res = put_project(api, pid, d, d["revision"], graphics=graphics, maps={mid: alt})
    ps = problems()
    check("tiles: rows written as numbers load too", st == 200 and ps == [], ps)

    # Back to a valid state: nothing lingers.
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], graphics=graphics, maps={mid: m})
    ps = problems()
    check("tiles: valid data again -> no problems", st == 200 and ps == [], ps)
