"""Standing-terrain checks of the ote-editor API test (docs/GRAPHICS.md "Standing terrain",
docs/EDITOR_API.md). Called from editor_api_test.py; uses its helpers."""
import json as _json
import os

from editor_api_models_checks import make_glb


def run_stand_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "stand-test"
    st, doc = api.json("POST", "/api/projects", {"id": pid, "name": "Stand", "template": "blank"})
    check("stand: create scratch project", st == 201, (st, doc))
    pdir = os.path.join(games, pid)
    models = "/api/projects/%s/models" % pid
    for name in ("tree.glb", "tree2.glb", "peak.glb"):
        st, _, data = api.call("PUT", models + "/" + name, raw_body=make_glb())
        check("stand: upload %s" % name, st == 201, (st, data))
    # a tiny picture sheet for the standing picture
    st, _, data = api.call("PUT", "/api/projects/%s/art/trees.png" % pid, raw_body=_png(48, 32))
    check("stand: upload a picture sheet", st == 201, (st, data))

    st, d = get_project(api, pid)
    terrain_ids = [t["id"] for t in d["terrain"]]
    tid = terrain_ids[-1]
    game = d["game"]
    game["styles"] = [{"id": "flat", "name": "Flat"}, {"id": "solid", "name": "Solid", "view": "isometric-3d"},
                      {"id": "full", "name": "Full", "view": "3d"}]
    sheets = [{"id": "trees", "file": "art/trees.png", "cellWidth": 24, "cellHeight": 32}]

    def graphics(stand, style="solid"):
        return {"sheets": sheets, "styles": {style: {"terrain": {tid: {"sheet": "trees", "cell": [0, 0], "stand": stand}}}}}

    def save(stand, style="solid"):
        s2, doc2 = get_project(api, pid)
        return put_project(api, pid, doc2, doc2["revision"], game=game, graphics=graphics(stand, style))

    # a picture stand and a model stand load without problems; the file is written exactly as sent
    picture = {"sheet": "trees", "variants": [[0, 0], [1, 0]], "width": 30, "scatter": True}
    st, res = save(picture)
    check("stand: a picture stand in a 3D style saves with no problems", st == 200 and res["problems"] == [], (st, res))
    st, back = get_project(api, pid)
    check("stand: graphics.json comes back with the stand untouched",
          back["graphics"]["styles"]["solid"]["terrain"][tid]["stand"] == picture, back.get("graphics"))
    with open(os.path.join(pdir, "data", "graphics.json"), encoding="utf-8") as f:
        on_disk = _json.load(f)
    check("stand: the file on disk has the stand", on_disk["styles"]["solid"]["terrain"][tid]["stand"] == picture, on_disk)

    model = {"variants": ["models/tree.glb", "models/tree2.glb"], "height": 1.2, "turn": 45, "scatter": True}
    st, res = save(model, "full")
    check("stand: model variants in a full 3D style save with no problems", st == 200 and res["problems"] == [], (st, res))

    # models in use by a stand: listed, and deleting them is refused
    st, lst = api.json("GET", models)
    used = {m["file"]: m["usedBy"] for m in lst["models"]} if st == 200 else {}
    check("stand: models list shows the terrain stand in usedBy (both variants)",
          used.get("models/tree.glb") == [{"style": "full", "kind": "terrain", "id": tid}] and
          used.get("models/tree2.glb") == [{"style": "full", "kind": "terrain", "id": tid}] and
          used.get("models/peak.glb") == [], used)
    st, _, data = api.call("DELETE", models + "/tree2.glb")
    body = _json.loads(data) if data else {}
    check("stand: deleting a model a stand uses -> 409 with usedBy",
          st == 409 and body.get("usedBy") == [{"style": "full", "kind": "terrain", "id": tid}] and
          os.path.exists(os.path.join(pdir, "models", "tree2.glb")), (st, body))
    st, res = save({"model": "models/peak.glb"}, "full")
    st, _, data = api.call("DELETE", models + "/tree2.glb")
    check("stand: once the stand names other files the model can go", st == 200, (st, data))

    # refusals come from the real loader, with the engine's words
    refusals = [
        ("a model that does not exist", {"model": "models/nobody.glb"}, "solid", "model file \"models/nobody.glb\" does not exist"),
        ("an unknown key", {"model": "models/peak.glb", "shine": 1}, "solid", "has an unknown key \"shine\""),
        ("picture and model keys mixed", {"sheet": "trees", "cell": [0, 0], "turn": 5}, "solid", "is a picture, so it cannot have \"turn\""),
        ("a missing sheet", {"sheet": "nope", "cell": [0, 0]}, "solid", "unknown sheet \"nope\""),
        ("a stand in a 2D style", {"sheet": "trees", "cell": [0, 0]}, "flat", "kind of view is 3D"),
    ]
    for what, stand, style, text in refusals:
        st, res = save(stand, style)
        msgs = [p["message"] for p in res.get("problems", [])] if st == 200 else []
        check("stand: %s -> a loader problem on data/graphics.json" % what,
              any(text in m for m in msgs) and all(p["file"] == "data/graphics.json" for p in res["problems"]), (st, res))


def _png(w, h):
    """A valid w x h RGBA PNG (stdlib only)."""
    import struct
    import zlib

    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)
    rows = b"".join(b"\x00" + b"\x20\x80\x30\xff" * w for _ in range(h))
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b""))
