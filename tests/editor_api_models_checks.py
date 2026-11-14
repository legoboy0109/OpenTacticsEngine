"""Model-file checks of the ote-editor API test (docs/GRAPHICS.md "Models",
docs/EDITOR_API.md). Called from editor_api_test.py; uses its helpers."""
import json as _json
import os
import struct


def make_glb(extra_json=None, version=2, pad_to=0):
    """A minimal .glb: header + one JSON chunk (enough for the loader's checks)."""
    doc = {"asset": {"version": "2.0"}}
    doc.update(extra_json or {})
    js = _json.dumps(doc).encode("utf-8")
    js += b" " * ((4 - len(js) % 4) % 4)
    body = struct.pack("<II", len(js), 0x4E4F534A) + js
    out = b"glTF" + struct.pack("<II", version, 12 + len(body)) + body
    return out + b"\0" * max(0, pad_to - len(out))


def run_models_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "model-test"
    st, doc = api.json("POST", "/api/projects", {"id": pid, "name": "Model", "template": "blank"})
    check("models: create scratch project", st == 201, (st, doc))
    pdir = os.path.join(games, pid)
    base = "/api/projects/%s/models" % pid
    glb = make_glb({"animations": [{"name": "Walk"}]})

    st, lst = api.json("GET", base)
    check("models list: empty without a models folder", st == 200 and lst == {"models": []}, (st, lst))

    st, _, data = api.call("PUT", base + "/knight.glb", raw_body=glb)
    up = _json.loads(data) if data else {}
    check("model upload -> 201 with file/size/revision",
          st == 201 and up.get("file") == "models/knight.glb" and up.get("size") == len(glb) and up.get("revision"),
          (st, up))
    with open(os.path.join(pdir, "models", "knight.glb"), "rb") as f:
        check("model upload: bytes on disk identical", f.read() == glb)
    check("model upload: no temp file left",
          [n for n in os.listdir(os.path.join(pdir, "models")) if n.endswith(".ote-tmp")] == [])

    st, _, data = api.call("PUT", base + "/knight.glb", raw_body=glb)
    check("model upload over an existing file -> 409", st == 409, (st, data))
    glb2 = make_glb({"animations": [{"name": "Run"}]})
    st, _, data = api.call("PUT", base + "/knight.glb?overwrite=1", raw_body=glb2)
    with open(os.path.join(pdir, "models", "knight.glb"), "rb") as f:
        check("model upload ?overwrite=1 replaces it", st == 201 and f.read() == glb2, st)

    for bad in ("Knight.glb", "a%20b.glb", "x.png", "-x.glb", ("a" * 41) + ".glb", "x"):
        st, _, data = api.call("PUT", base + "/" + bad, raw_body=glb)
        check("model upload bad name %r -> 400" % bad, st == 400 and b"error" in data, (st, data))
    st, _, data = api.call("PUT", base + "/side.gltf", raw_body=b"{}")
    check("model upload .gltf -> 400 saying only .glb", st == 400 and b".glb" in data, (st, data))
    st, _, data = api.call("PUT", base + "/..%2Fescape.glb", raw_body=glb)
    check("model upload path trick -> 400/404, nothing outside",
          st in (400, 404) and not os.path.exists(os.path.join(pdir, "escape.glb")), st)

    st, _, data = api.call("PUT", base + "/bad.glb", raw_body=b"this is not a model file")
    check("model upload bad magic -> 400",
          st == 400 and b"glTF" in data and not os.path.exists(os.path.join(pdir, "models", "bad.glb")), (st, data))
    st, _, data = api.call("PUT", base + "/v1.glb", raw_body=make_glb(version=1))
    check("model upload glTF version 1 -> 400", st == 400 and b"version" in data, (st, data))
    st, _, data = api.call("PUT", base + "/empty.glb", raw_body=b"")
    check("model upload empty body -> 400", st == 400, (st, data))
    st, _, data = api.call("PUT", base + "/big.glb", raw_body=make_glb(pad_to=8 * 1024 * 1024 + 1))
    check("model upload over 8 MB -> 413",
          st == 413 and not os.path.exists(os.path.join(pdir, "models", "big.glb")), (st, data))

    # a model named by graphics.json: loads cleanly, and blocks deletion
    st, d = get_project(api, pid)
    game = d["game"]
    game["styles"] = [{"id": "solid", "name": "Solid", "view": "isometric-3d"}]
    gfx = {"styles": {"solid": {"units": {"soldier": {"model": "models/knight.glb", "height": 1.2}}}}}
    st, res = put_project(api, pid, d, d["revision"], game=game, graphics=gfx)
    check("graphics naming an uploaded model: saved with no problems", st == 200 and res["problems"] == [], (st, res))
    st, lst = api.json("GET", base)
    m = lst["models"][0] if st == 200 and lst["models"] else {}
    check("models list: file, size and usedBy",
          m.get("file") == "models/knight.glb" and m.get("size") == len(glb2) and
          m.get("usedBy") == [{"style": "solid", "kind": "units", "id": "soldier"}], lst)
    st, _, data = api.call("DELETE", base + "/knight.glb")
    body = _json.loads(data) if data else {}
    check("model delete while used -> 409 with usedBy",
          st == 409 and body.get("usedBy") and os.path.exists(os.path.join(pdir, "models", "knight.glb")), (st, body))
    st, _, data = api.call("GET", "/api/projects/%s/file/models/knight.glb" % pid)
    check("GET file/models/... returns the bytes", st == 200 and data == glb2, st)

    st, d = get_project(api, pid)
    gfx = {"styles": {"solid": {"units": {"soldier": {"model": "models/missing.glb"}}}}}
    st, res = put_project(api, pid, d, d["revision"], graphics=gfx)
    check("graphics naming a missing model -> loader problem on data/graphics.json",
          st == 200 and res["problems"] and res["problems"][0]["file"] == "data/graphics.json" and
          "models/missing.glb" in res["problems"][0]["message"], res)
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], graphics={"styles": {"solid": {}}})
    st, _, data = api.call("DELETE", base + "/knight.glb")
    check("model delete when unused -> 200 and gone",
          st == 200 and not os.path.exists(os.path.join(pdir, "models", "knight.glb")), (st, data))
    st, _, data = api.call("DELETE", base + "/knight.glb")
    check("model delete of a missing file -> 404", st == 404, st)
    st, _, data = api.call("DELETE", base + "/Bad.glb")
    check("model delete bad name -> 400", st == 400, st)
