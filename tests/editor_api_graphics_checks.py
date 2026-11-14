"""Graphics / art checks of the ote-editor API test (docs/GRAPHICS.md,
docs/EDITOR_API.md). Called from editor_api_test.py; uses its helpers.
Standard library only: PNGs are built with zlib + struct."""
import json as _json
import os
import shutil
import struct
import zlib


def make_png(w, h, rgba=(200, 30, 30, 255)):
    """A real, valid w x h RGBA PNG (solid colour)."""
    def chunk(tag, data):
        body = tag + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)
    row = b"\x00" + bytes(rgba) * w
    raw = row * h
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))


def header_only_png(w, h):
    """Just a signature + IHDR claiming w x h (enough for the size check)."""
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + struct.pack(">I", 13) + b"IHDR" + ihdr +
            struct.pack(">I", zlib.crc32(b"IHDR" + ihdr) & 0xFFFFFFFF))


def run_graphics_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "gfx-test"
    st, doc = api.json("POST", "/api/projects", {"id": pid, "name": "Gfx", "template": "blank"})
    check("graphics: create scratch project", st == 201, (st, doc))
    pdir = os.path.join(games, pid)
    art = "/api/projects/%s/art" % pid
    # Templates ship starter art; these checks start from a project without any.
    shutil.rmtree(os.path.join(pdir, "art"), ignore_errors=True)
    if os.path.exists(os.path.join(pdir, "data", "graphics.json")):
        os.remove(os.path.join(pdir, "data", "graphics.json"))
    png_hdr = {"Content-Type": "image/png"}

    # ---- graphics section: absent / round trip / invalid ----
    st, d = get_project(api, pid)
    check("graphics: absent file -> null, no problem about it",
          st == 200 and "graphics" in d and d["graphics"] is None and
          not any(p["file"] == "data/graphics.json" for p in d["problems"]), d.get("graphics"))
    st, lst = api.json("GET", art)
    check("art list: empty without an art folder", st == 200 and lst == {"art": []}, (st, lst))

    # ---- uploads ----
    tiles = make_png(48, 24)
    st, hdrs, data = api.call("PUT", art + "/tiles.png", raw_body=tiles, headers=png_hdr)
    up = _json.loads(data) if data else {}
    check("art upload -> 201 with file/width/height/size/revision",
          st == 201 and up.get("file") == "art/tiles.png" and up.get("width") == 48 and up.get("height") == 24 and
          up.get("size") == len(tiles) and isinstance(up.get("revision"), str) and up["revision"], (st, up))
    on_disk = os.path.join(pdir, "art", "tiles.png")
    with open(on_disk, "rb") as f:
        check("art upload: bytes on disk identical", f.read() == tiles)
    check("art upload: no temp file left",
          [n for n in os.listdir(os.path.join(pdir, "art")) if n.endswith(".ote-tmp")] == [])
    st, d2 = get_project(api, pid)
    check("art upload changes the project revision, response revision matches",
          d2["revision"] == up["revision"] and d2["revision"] != d["revision"])
    st, lst = api.json("GET", art)
    check("art list shows the upload", lst == {"art": [{"file": "art/tiles.png", "width": 48, "height": 24,
                                                         "size": len(tiles)}]}, lst)

    st, hdrs, data = api.call("GET", "/api/projects/%s/file/art/tiles.png" % pid)
    check("file endpoint serves .png as image/png with identical bytes",
          st == 200 and hdrs.get("content-type") == "image/png" and data == tiles, (st, hdrs.get("content-type")))
    st, hdrs, data = api.call("GET", "/api/projects/%s/file/game.json" % pid)
    check("file endpoint keeps octet-stream for other files", hdrs.get("content-type") == "application/octet-stream")

    # second sheet, listed in name order, plus a broken file
    st, _, _ = api.call("PUT", art + "/a-first.png", raw_body=make_png(16, 16), headers=png_hdr)
    check("second upload -> 201", st == 201)
    with open(os.path.join(pdir, "art", "broken.png"), "wb") as f:
        f.write(b"not a png at all, just text padding")
    st, lst = api.json("GET", art)
    names = [a["file"] for a in lst["art"]]
    broken = [a for a in lst["art"] if a["file"] == "art/broken.png"]
    check("art list sorted by name", names == ["art/a-first.png", "art/broken.png", "art/tiles.png"], names)
    check("art list: non-PNG listed with 0x0 and an error text",
          len(broken) == 1 and broken[0]["width"] == 0 and broken[0]["height"] == 0 and broken[0].get("error"),
          broken)
    os.remove(os.path.join(pdir, "art", "broken.png"))
    api.call("DELETE", art + "/a-first.png")

    # overwrite rules
    other = make_png(24, 24, (0, 0, 255, 255))
    st, hdrs, data = api.call("PUT", art + "/tiles.png", raw_body=other, headers=png_hdr)
    check("upload over existing without flag -> 409", st == 409 and b"overwrite" in data, (st, data))
    with open(on_disk, "rb") as f:
        check("409 left the old file untouched", f.read() == tiles)
    st, hdrs, data = api.call("PUT", art + "/tiles.png?overwrite=1", raw_body=other, headers=png_hdr)
    check("upload with ?overwrite=1 -> 201 new size", st == 201 and _json.loads(data)["width"] == 24, (st, data))
    with open(on_disk, "rb") as f:
        check("overwrite replaced the bytes", f.read() == other)
    st, _, _ = api.call("PUT", art + "/tiles.png?overwrite=1", raw_body=tiles, headers=png_hdr)
    check("restore original tiles.png", st == 201)

    # rejected bodies
    before = sorted(os.listdir(os.path.join(pdir, "art")))
    st, hdrs, data = api.call("PUT", art + "/text.png", raw_body=b"hello, I am not an image " * 5, headers=png_hdr)
    msg = _json.loads(data).get("error", "") if data else ""
    check("upload of a non-PNG -> 400 with a readable message", st == 400 and "PNG" in msg, (st, msg))
    st, hdrs, data = api.call("PUT", art + "/empty.png", raw_body=b"", headers=png_hdr)
    check("upload with empty body -> 400", st == 400, st)
    st, hdrs, data = api.call("PUT", art + "/huge.png", raw_body=header_only_png(4096, 4096), headers=png_hdr)
    msg = _json.loads(data).get("error", "") if data else ""
    check("upload of a 4096 px image -> 400 naming the limit", st == 400 and "2048" in msg, (st, msg))
    st, hdrs, data = api.call("PUT", art + "/wide.png", raw_body=header_only_png(2049, 16), headers=png_hdr)
    check("upload 2049 px wide -> 400", st == 400, st)
    st, hdrs, data = api.call("PUT", art + "/edge.png", raw_body=header_only_png(2048, 2048), headers=png_hdr)
    check("upload exactly 2048 x 2048 header -> 201", st == 201, st)
    api.call("DELETE", art + "/edge.png")
    big = make_png(16, 16) + b"\0" * (8 * 1024 * 1024)
    st, hdrs, data = api.call("PUT", art + "/big.png", raw_body=big, headers=png_hdr, timeout=60)
    msg = _json.loads(data).get("error", "") if data else ""
    check("upload over 8 MB -> 413 with a readable message", st == 413 and "8 MB" in msg, (st, msg))
    check("rejected uploads left nothing behind (no files, no temp files)",
          sorted(os.listdir(os.path.join(pdir, "art"))) == before, os.listdir(os.path.join(pdir, "art")))

    # bad names
    bad = ["Tiles.png", "tiles.PNG", "..png", "-x.png", "_x.png", "a" * 41 + ".png", "tiles.jpg", "tiles",
           ".png", "a%20b.png", "a%2fb.png", "a%5cb.png", "..%2f..%2fx.png", "%2e%2e%2fx.png", "a.png.png",
           "x.png%00.png", "d%C3%A9j%C3%A0.png"]
    for name in bad:
        st, hdrs, data = api.call("PUT", art + "/" + name, raw_body=tiles, headers=png_hdr)
        check("bad art name %r -> 400/404, nothing written" % name, st in (400, 404), st)
    st, hdrs, data = api.call("PUT", art + "/sub/x.png", raw_body=tiles, headers=png_hdr)
    check("art name with a slash -> 400", st == 400, st)
    st, hdrs, data = api.call("PUT", art + "/../game.json", raw_body=tiles, headers=png_hdr)
    check("art name with .. -> rejected", st in (400, 404), st)
    st, hdrs, data = api.call("PUT", "/api/projects/%s/art/%s.png" % (pid, "a" * 40), raw_body=tiles,
                              headers=png_hdr)
    check("40-character name is accepted", st == 201, st)
    api.call("DELETE", "/api/projects/%s/art/%s.png" % (pid, "a" * 40))
    st, hdrs, data = api.call("PUT", art + "/ok_name-2.png", raw_body=tiles, headers=png_hdr)
    check("name with digits, '-' and '_' accepted", st == 201, st)
    api.call("DELETE", art + "/ok_name-2.png")
    check("bad names wrote nothing outside art/ (game.json intact, sentinel-free)",
          sorted(os.listdir(os.path.join(pdir, "art"))) == before and
          os.path.getsize(os.path.join(pdir, "game.json")) > 100)
    st, hdrs, data = api.call("PUT", "/api/projects/nope-x/art/a.png", raw_body=tiles, headers=png_hdr)
    check("upload to unknown project -> 404", st == 404)
    st, hdrs, data = api.call("PUT", "/api/projects/Bad%20Id/art/a.png", raw_body=tiles, headers=png_hdr)
    check("upload to bad project id -> 400", st == 400)

    # ---- symlinks never carry a write out of the project ----
    outside = os.path.join(tmp, "work", "outside-art")
    os.makedirs(outside, exist_ok=True)
    try:
        os.symlink(outside, os.path.join(pdir, "linked-art"))
        linked_ok = True
    except (OSError, NotImplementedError):
        linked_ok = False
    if linked_ok:
        os.remove(os.path.join(pdir, "linked-art"))
        shutil.rmtree(os.path.join(pdir, "art"))
        os.symlink(outside, os.path.join(pdir, "art"))
        st, hdrs, data = api.call("PUT", art + "/evil.png", raw_body=tiles, headers=png_hdr)
        check("art/ symlinked outside the project: upload refused",
              st == 400 and os.listdir(outside) == [], (st, os.listdir(outside)))
        st, lst = api.json("GET", art)
        check("art/ symlinked outside the project: list is empty", st == 200 and lst == {"art": []}, lst)
        os.remove(os.path.join(pdir, "art"))
        os.makedirs(os.path.join(pdir, "art"))
        with open(os.path.join(outside, "secret.png"), "wb") as f:
            f.write(tiles)
        os.symlink(os.path.join(outside, "secret.png"), os.path.join(pdir, "art", "link.png"))
        st, hdrs, data = api.call("PUT", art + "/link.png?overwrite=1", raw_body=other, headers=png_hdr)
        check("art file that is a symlink: overwrite refused, target untouched",
              st == 400 and open(os.path.join(outside, "secret.png"), "rb").read() == tiles, st)
        st, hdrs, data = api.call("DELETE", art + "/link.png")
        check("art file that is a symlink: delete refused, target kept",
              st in (400, 404) and os.path.exists(os.path.join(outside, "secret.png")), st)
        st, lst = api.json("GET", art)
        check("symlinked art file is not listed", [a["file"] for a in lst["art"]] == [], lst)
        os.remove(os.path.join(pdir, "art", "link.png"))
        st, _, _ = api.call("PUT", art + "/tiles.png", raw_body=tiles, headers=png_hdr)
        check("re-upload tiles.png after symlink tests", st == 201, st)

    # ---- graphics PUT round trip, loader problems ----
    st, d = get_project(api, pid)
    gfx = {"sheets": [{"id": "main", "file": "art/tiles.png", "cellWidth": 24, "cellHeight": 24}],
           "terrain": {"plain": {"sheet": "main", "cell": [0, 0]}}}
    st, res = put_project(api, pid, d, d["revision"], graphics=gfx)
    check("PUT graphics -> 200, no problems", st == 200 and res["problems"] == [], (st, res))
    gpath = os.path.join(pdir, "data", "graphics.json")
    check("PUT graphics wrote data/graphics.json", os.path.isfile(gpath))
    st, d = get_project(api, pid)
    check("GET returns graphics exactly as written", d["graphics"] == gfx, d["graphics"])
    with open(gpath, "rb") as f:
        written = f.read()
    st, res = put_project(api, pid, d, d["revision"])
    st2, d = get_project(api, pid)
    check("PUT without graphics leaves the file untouched",
          st == 200 and open(gpath, "rb").read() == written and d["graphics"] == gfx)
    st, res = put_project(api, pid, d, d["revision"], graphics=None)
    check("PUT with graphics null leaves the file untouched", st == 200 and open(gpath, "rb").read() == written)
    for badval in ([], "x", 5):
        st, res = put_project(api, pid, d, d["revision"], graphics=badval)
        check("PUT graphics=%r -> 400" % (badval,), st == 400 and "error" in res, (st, res))

    bad_gfx = {"sheets": [{"id": "main", "file": "art/tiles.png", "cellWidth": 24, "cellHeight": 24}],
               "terrain": {"plain": {"sheet": "nope", "cell": [0, 0]}}}
    st, res = put_project(api, pid, d, d["revision"], graphics=bad_gfx)
    check("PUT graphics with unknown sheet: saved, problem file is data/graphics.json",
          st == 200 and len(res["problems"]) >= 1 and res["problems"][0]["file"] == "data/graphics.json" and
          "nope" in res["problems"][0]["message"], res)
    st, pr = api.json("GET", "/api/projects/%s/problems" % pid)
    check("GET problems reports the graphics problem with the right file",
          st == 200 and pr["problems"] and pr["problems"][0]["file"] == "data/graphics.json", pr)
    st, d = get_project(api, pid)
    check("GET project reports it too", any(p["file"] == "data/graphics.json" for p in d["problems"]), d["problems"])
    st, res = put_project(api, pid, d, d["revision"], graphics=gfx)
    check("fixing graphics clears the problem", st == 200 and res["problems"] == [], res)

    # a sheet's optional "smooth" flag (3D views blend its pixels) round-trips, and must be a boolean
    smooth_gfx = {"sheets": [{"id": "main", "file": "art/tiles.png", "cellWidth": 24, "cellHeight": 24, "smooth": True}]}
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], graphics=smooth_gfx)
    check("PUT graphics with smooth true -> 200, no problems", st == 200 and res["problems"] == [], (st, res))
    st, d = get_project(api, pid)
    check("GET returns the smooth flag as written", d["graphics"] == smooth_gfx, d["graphics"])
    bad_smooth = {"sheets": [{"id": "main", "file": "art/tiles.png", "cellWidth": 24, "cellHeight": 24, "smooth": "yes"}]}
    st, res = put_project(api, pid, d, d["revision"], graphics=bad_smooth)
    check("smooth that is not true/false -> problem on data/graphics.json naming it",
          st == 200 and res["problems"] and res["problems"][0]["file"] == "data/graphics.json" and
          "smooth" in res["problems"][0]["message"], res)
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], graphics=gfx)
    check("restoring the plain sheet clears the problem again", st == 200 and res["problems"] == [], res)

    # missing image file referenced by a sheet
    missing = {"sheets": [{"id": "gone", "file": "art/gone.png", "cellWidth": 8, "cellHeight": 8}]}
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], graphics=missing)
    check("sheet pointing at a missing image -> problem on data/graphics.json",
          st == 200 and res["problems"] and res["problems"][0]["file"] == "data/graphics.json", res)
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], graphics=gfx)

    # ---- delete while used / unused / missing ----
    st, hdrs, data = api.call("DELETE", art + "/tiles.png")
    dd = _json.loads(data)
    check("delete of a used sheet image -> 409 with usedBy",
          st == 409 and dd.get("usedBy") == ["main"] and dd.get("error"), (st, dd))
    check("409 delete kept the file", os.path.isfile(on_disk))
    api.call("PUT", art + "/spare.png", raw_body=make_png(8, 8), headers=png_hdr)
    st, d = get_project(api, pid)
    st, hdrs, data = api.call("DELETE", art + "/spare.png")
    dd = _json.loads(data)
    check("delete of an unused image -> 200 with new revision",
          st == 200 and set(dd) == {"revision"} and dd["revision"] != d["revision"], (st, dd))
    check("deleted file is gone", not os.path.exists(os.path.join(pdir, "art", "spare.png")))
    st, d3 = get_project(api, pid)
    check("delete response revision matches the project's", d3["revision"] == dd["revision"])
    st, hdrs, data = api.call("DELETE", art + "/spare.png")
    check("delete of a missing file -> 404", st == 404, st)
    st, hdrs, data = api.call("DELETE", art + "/Bad.png")
    check("delete with a bad name -> 400", st == 400, st)
    st, hdrs, data = api.call("DELETE", art + "/..%2fgame.json")
    check("delete with an escape name -> rejected, game.json kept",
          st in (400, 404) and os.path.isfile(os.path.join(pdir, "game.json")), st)
    # usedBy reads the SAVED file: an unsaved change in the UI does not matter
    st, res = put_project(api, pid, d3, d3["revision"], graphics={"sheets": []})
    st, hdrs, data = api.call("DELETE", art + "/tiles.png")
    check("delete after the sheet was removed and saved -> 200", st == 200, (st, data))
    st, lst = api.json("GET", art)
    check("art list empty again", lst == {"art": []}, lst)

    # ---- revision and stale baseRevision ----
    st, d = get_project(api, pid)
    old_rev = d["revision"]
    st, _, data = api.call("PUT", art + "/late.png", raw_body=tiles, headers=png_hdr)
    st, res = put_project(api, pid, d, old_rev)
    check("PUT with a baseRevision from before an upload -> 409 with the new revision",
          st == 409 and res.get("revision") == _json.loads(data)["revision"], (st, res))
    st, d = get_project(api, pid)
    api.call("DELETE", art + "/late.png")
    st, res = put_project(api, pid, d, d["revision"])
    check("PUT with a baseRevision from before a delete -> 409", st == 409, st)
    st, d = get_project(api, pid)
    api.call("PUT", art + "/same.png", raw_body=tiles, headers=png_hdr)
    st, d_a = get_project(api, pid)
    api.call("PUT", art + "/same.png?overwrite=1", raw_body=other, headers=png_hdr)
    st, d_b = get_project(api, pid)
    check("same-name overwrite with different bytes changes the revision", d_a["revision"] != d_b["revision"])
    api.call("DELETE", art + "/same.png")
    with open(gpath, "a") as f:
        f.write(" ")
    st, d_c = get_project(api, pid)
    with open(gpath, "a") as f:
        f.write("\n")
    st, d_d = get_project(api, pid)
    check("revision covers data/graphics.json", d_c["revision"] != d_d["revision"])

    # ---- invalid graphics.json on disk ----
    with open(gpath, "w") as f:
        f.write("{ this is not json")
    st, d = get_project(api, pid)
    check("invalid graphics.json: graphics null plus a problem on that file",
          st == 200 and d["graphics"] is None and any(p["file"] == "data/graphics.json" for p in d["problems"]),
          d["problems"])
    st, pr = api.json("GET", "/api/projects/%s/problems" % pid)
    check("invalid graphics.json shows in /problems", any(p["file"] == "data/graphics.json" for p in pr["problems"]), pr)
    with open(gpath, "w") as f:
        f.write("[1, 2]")
    st, d = get_project(api, pid)
    check("graphics.json that is not an object: null plus a problem",
          d["graphics"] is None and any(p["file"] == "data/graphics.json" for p in d["problems"]), d["problems"])
    st, res = put_project(api, pid, d, d["revision"], graphics={"sheets": []})
    check("PUT repairs a broken graphics.json", st == 200 and res["problems"] == [], res)

    # a map cannot be pointed at the graphics file
    st, d = get_project(api, pid)
    g2 = dict(d["game"])
    g2["maps"] = list(g2["maps"]) + [{"id": "evil", "file": "data/graphics.json"}]
    st, res = put_project(api, pid, d, d["revision"], game=g2, maps=dict(d["maps"], evil={"x": 1}))
    check("map file data/graphics.json is reserved -> 400", st == 400, (st, res))

    # ---- template copy keeps art byte for byte ----
    tpl_art = os.path.join(templates, "blank", "art")
    os.makedirs(tpl_art, exist_ok=True)
    blob = make_png(32, 16) + bytes(range(256)) * 40  # includes NUL, CR, LF, 0xFF
    with open(os.path.join(tpl_art, "pic.png"), "wb") as f:
        f.write(blob)
    st, doc = api.json("POST", "/api/projects", {"id": "gfx-from-tpl", "name": "T", "template": "blank"})
    check("create from a template that has art -> 201", st == 201, (st, doc))
    with open(os.path.join(games, "gfx-from-tpl", "art", "pic.png"), "rb") as f:
        check("template art copied byte for byte", f.read() == blob)
    st, lst = api.json("GET", "/api/projects/gfx-from-tpl/art")
    check("copied art is listed", "art/pic.png" in [a["file"] for a in lst["art"]], lst)
    shutil.rmtree(tpl_art)
