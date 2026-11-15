#!/usr/bin/env python3
"""End-to-end test of the ote-editor HTTP API (docs/EDITOR_API.md).

usage: editor_api_test.py <path-to-ote-editor> <repo-root>

Copies games/ and editor/templates/ into a temp dir, starts the helper on a
free port against the COPY, runs the checks and always kills the helper.
Python 3 standard library only. Exit code 0 = all checks passed.
"""
import copy
import http.client
import json
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time

from editor_api_graphics_checks import run_graphics_checks
from editor_api_models_checks import run_models_checks
from editor_api_items_checks import run_items_checks
from editor_api_events_checks import run_events_checks
from editor_api_campaign_checks import run_campaign_checks
from editor_api_structures_checks import run_structures_checks
from editor_api_economy_checks import run_economy_checks
from editor_api_fog_checks import run_fog_checks

PASSED = 0
FAILED = 0


def check(name, cond, detail=""):
    global PASSED, FAILED
    if cond:
        PASSED += 1
        print("PASS  " + name)
    else:
        FAILED += 1
        print("FAIL  " + name + (("  -> " + str(detail)) if detail != "" else ""))
    return bool(cond)


class Api:
    def __init__(self, port):
        self.port = port

    def call(self, method, path, body=None, headers=None, raw_body=None, timeout=30):
        """Returns (status, headers, bytes). Uses http.client so paths and
        Host headers are sent exactly as given (no normalisation)."""
        conn = http.client.HTTPConnection("127.0.0.1", self.port, timeout=timeout)
        h = dict(headers or {})
        data = raw_body
        if body is not None:
            data = json.dumps(body).encode("utf-8")
            h.setdefault("Content-Type", "application/json")
        try:
            conn.request(method, path, body=data, headers=h)
            r = conn.getresponse()
            return r.status, dict((k.lower(), v) for k, v in r.getheaders()), r.read()
        finally:
            conn.close()

    def json(self, method, path, body=None, headers=None):
        status, hdrs, data = self.call(method, path, body, headers)
        try:
            doc = json.loads(data.decode("utf-8")) if data else None
        except ValueError:
            doc = None
        return status, doc


def free_port():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
    s.close()
    return port


def read_json(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def tree_files(root):
    out = []
    for d, _, files in os.walk(root):
        for f in files:
            out.append(os.path.join(d, f))
    return out


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    binary = os.path.abspath(sys.argv[1])
    repo = os.path.abspath(sys.argv[2])
    tmp = tempfile.mkdtemp(prefix="ote-editor-test-")
    proc = None
    try:
        games = os.path.join(tmp, "work", "games")
        templates = os.path.join(tmp, "work", "templates")
        shutil.copytree(os.path.join(repo, "games"), games)
        shutil.copytree(os.path.join(repo, "editor", "templates"), templates)
        sentinel = os.path.join(tmp, "work", "sentinel.txt")
        with open(sentinel, "w") as f:
            f.write("do not touch")
        port = free_port()
        proc = subprocess.Popen(
            [binary, "--games", games, "--templates", templates,
             "--assets", os.path.join(repo, "assets"),
             "--web", os.path.join(tmp, "no-web"), "--player", os.path.join(tmp, "no-player"),
             "--port", str(port)],
            stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        api = Api(port)
        up = False
        for _ in range(100):
            if proc.poll() is not None:
                break
            try:
                if api.call("GET", "/api/info")[0] == 200:
                    up = True
                    break
            except OSError:
                time.sleep(0.1)
        if not check("helper starts and answers /api/info", up,
                     proc.stderr.read().decode("utf-8", "replace") if proc.poll() is not None else ""):
            return 1
        # A second helper on the same port must fail loudly, not share the port.
        second = subprocess.run([binary, "--games", games, "--templates", templates, "--port", str(port)],
                                stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, timeout=20)
        check("second helper on a busy port exits non-zero with a clear message",
              second.returncode != 0 and b"port" in second.stderr.lower(), second.stderr[-200:])
        check("first helper unaffected by the failed second start", api.call("GET", "/api/info")[0] == 200)
        run_checks(api, port, tmp, games, sentinel, proc)
        run_graphics_checks(api, tmp, games, templates, check, get_project, put_project)
        run_models_checks(api, tmp, games, templates, check, get_project, put_project)
        run_structures_checks(api, tmp, games, templates, check, get_project, put_project)
        run_items_checks(api, tmp, games, templates, check, get_project, put_project)
        run_events_checks(api, tmp, games, templates, check, get_project, put_project)
        run_campaign_checks(api, tmp, games, templates, check, get_project, put_project)
        run_economy_checks(api, tmp, games, templates, check, get_project, put_project)
        run_fog_checks(api, tmp, games, templates, check, get_project, put_project)
        # The helper must survive everything above.
        st, doc = api.json("GET", "/api/info")
        check("helper still alive after all checks", proc.poll() is None and st == 200)
    except Exception as e:  # a crash in the test itself is a failure, not a pass
        import traceback
        traceback.print_exc()
        check("test script ran to completion", False, repr(e))
    finally:
        if proc is not None:
            proc.kill()
            proc.wait()
        shutil.rmtree(tmp, ignore_errors=True)
    print("\n%d passed, %d failed" % (PASSED, FAILED))
    return 0 if FAILED == 0 else 1


def get_project(api, pid):
    st, doc = api.json("GET", "/api/projects/" + pid)
    return st, doc


def put_project(api, pid, doc, rev, **overrides):
    body = {k: doc[k] for k in ("game", "terrain", "weapons", "units", "maps", "scripts") if k in doc}
    body.update(overrides)
    body["baseRevision"] = rev
    return api.json("PUT", "/api/projects/" + pid, body)


def run_checks(api, port, tmp, games, sentinel, proc):
    # ---- info / lists ----
    st, doc = api.json("GET", "/api/info")
    check("info: engine, apiVersion, formatVersion",
          st == 200 and doc == {"engine": "OpenTacticsEngine", "apiVersion": 1, "formatVersion": 1}, doc)

    st, doc = api.json("GET", "/api/projects")
    projs = {p["id"]: p for p in (doc or {}).get("projects", [])}
    check("project list has both demos, ok=true",
          st == 200 and projs.get("demo-srpg", {}).get("ok") is True and projs.get("demo-economy", {}).get("ok") is True,
          doc)
    check("project list names and map counts",
          projs.get("demo-srpg", {}).get("name") == "Emblem Demo" and projs["demo-srpg"].get("maps") == 2 and
          projs.get("demo-economy", {}).get("name") == "Wars Demo" and projs["demo-economy"].get("maps") == 1)
    check("project list entries have all fields",
          all(set(p) == {"id", "name", "description", "author", "maps", "ok", "error"} for p in projs.values()))

    st, doc = api.json("GET", "/api/templates")
    ids = sorted(t["id"] for t in (doc or {}).get("templates", []))
    check("templates list has the three ids", st == 200 and ids == ["blank", "economy-tactics", "strategy-rpg"], doc)
    check("templates have name and description",
          all(t.get("name") and t.get("description") for t in doc["templates"]))

    # ---- create ----
    for tpl in ("strategy-rpg", "economy-tactics", "blank"):
        pid = "new-" + tpl
        st, doc = api.json("POST", "/api/projects", {"id": pid, "name": "My " + tpl, "template": tpl})
        check("create from %s -> 201" % tpl, st == 201 and doc == {"id": pid}, (st, doc))
        st, plist = api.json("GET", "/api/projects")
        entry = {p["id"]: p for p in plist["projects"]}.get(pid)
        check("created %s listed, ok, named" % tpl,
              entry is not None and entry["ok"] and entry["name"] == "My " + tpl, entry)
        st, prob = api.json("GET", "/api/projects/%s/problems" % pid)
        check("created %s has zero problems" % tpl, st == 200 and prob == {"problems": []}, prob)
        st, pdoc = get_project(api, pid)
        check("created %s GET: name set, no problems, 1+ maps" % tpl,
              st == 200 and pdoc["game"]["name"] == "My " + tpl and pdoc["problems"] == [] and len(pdoc["maps"]) >= 1)
    st, doc = api.json("POST", "/api/projects", {"id": "new-blank", "name": "Again", "template": "blank"})
    check("create duplicate -> 409", st == 409 and "error" in doc, (st, doc))
    st, doc = api.json("POST", "/api/projects", {"id": "demo-srpg", "name": "x", "template": "blank"})
    check("create over existing demo -> 409", st == 409)
    for bad in ("../x", "Bad Id", "", "-lead", "a/b", "x" * 41):
        st, doc = api.json("POST", "/api/projects", {"id": bad, "name": "x", "template": "blank"})
        check("create bad id %r -> 400" % bad, st == 400 and "error" in doc, (st, doc))
    st, doc = api.json("POST", "/api/projects", {"id": "ok-id", "name": "x", "template": "nope"})
    check("create unknown template -> 400", st == 400)
    st, doc = api.json("POST", "/api/projects", {"id": "ok-id", "name": "x", "template": "../blank"})
    check("create template with path -> 400", st == 400)
    check("failed creates left no folders",
          not os.path.exists(os.path.join(games, "ok-id")) and not os.path.exists(os.path.join(games, "x")))
    st, doc = api.call("POST", "/api/projects", raw_body=b"{not json")[0], None
    check("create with invalid JSON body -> 400", st == 400)

    # ---- GET project ----
    st, srpg = get_project(api, "demo-srpg")
    check("GET demo-srpg: sections present",
          st == 200 and srpg["id"] == "demo-srpg" and isinstance(srpg["game"], dict) and
          isinstance(srpg["terrain"], list) and isinstance(srpg["weapons"], list) and
          isinstance(srpg["units"], list) and isinstance(srpg["maps"], dict) and isinstance(srpg["scripts"], dict))
    check("GET demo-srpg: maps keyed by id, scripts text present",
          list(srpg["maps"].keys()) == ["bridge", "north_road"] and "srpg_damage" in srpg["scripts"].get("scripts/formulas.lua", ""))
    check("GET demo-srpg: revision non-empty, problems empty", bool(srpg["revision"]) and srpg["problems"] == [])
    check("GET demo-srpg: comments are gone (strict JSON data)",
          "Emblem Demo" == srpg["game"]["name"] and len(srpg["units"]) == 7)
    st, doc = get_project(api, "nope-x")
    check("GET unknown project -> 404", st == 404 and "error" in doc)
    st, doc = get_project(api, "Bad%20Id")
    check("GET bad id -> 400", st == 400)

    # ---- files ----
    st, fl = api.json("GET", "/api/projects/demo-srpg/files")
    byp = {f["path"]: f["size"] for f in fl["files"]}
    gj = os.path.join(games, "demo-srpg", "game.json")
    check("files list has game.json with right size", byp.get("game.json") == os.path.getsize(gj), byp)
    check("files list has nested files", "maps/bridge.json" in byp and "scripts/formulas.lua" in byp)
    st, hdrs, data = api.call("GET", "/api/projects/demo-srpg/file/game.json")
    with open(gj, "rb") as f:
        disk = f.read()
    check("file endpoint returns exact bytes (octet-stream)",
          st == 200 and data == disk and hdrs.get("content-type", "").startswith("application/octet-stream"))
    st, hdrs, data = api.call("GET", "/api/projects/demo-srpg/file/nothing.json")
    check("file endpoint missing file -> 404", st == 404)

    # ---- fonts ----
    st, doc = api.json("GET", "/api/fonts")
    fids = [f["id"] for f in doc["fonts"]]
    check("fonts list contains pixel and sans", st == 200 and "pixel" in fids and "sans" in fids, fids)
    check("fonts entries have name/size/smooth",
          all({"id", "name", "size", "smooth"} <= set(f) for f in doc["fonts"]))

    # ---- PUT round trip ----
    st, d1 = get_project(api, "demo-srpg")
    st, res = put_project(api, "demo-srpg", d1, d1["revision"])
    check("PUT same document -> 200, zero problems, new revision",
          st == 200 and res["problems"] == [] and bool(res["revision"]), (st, res))
    st, d2 = get_project(api, "demo-srpg")
    same = all(d1[k] == d2[k] for k in ("game", "terrain", "weapons", "units", "maps", "scripts"))
    check("round trip: GET returns equal sections", same)
    check("round trip: PUT revision equals GET revision", d2["revision"] == res["revision"])
    st, res2 = put_project(api, "demo-srpg", d2, d2["revision"])
    st, d3 = get_project(api, "demo-srpg")
    check("second identical PUT is a no-op (revision stable)", st == 200 and d3["revision"] == d2["revision"])

    # stale revision: modify a file on disk between GET and PUT
    st, d4 = get_project(api, "demo-srpg")
    wpath = os.path.join(games, "demo-srpg", "data", "weapons.json")
    with open(wpath, "a") as f:
        f.write("\n")  # an external edit (e.g. synced from another machine)
    with open(wpath, "rb") as f:
        edited = f.read()
    changed = copy.deepcopy(d4)
    changed["units"][0]["name"] = "SHOULD NOT BE SAVED"
    st, res = put_project(api, "demo-srpg", changed, d4["revision"])
    with open(wpath, "rb") as f:
        after = f.read()
    st2, d5 = get_project(api, "demo-srpg")
    check("PUT with old revision -> 409 with current revision",
          st == 409 and res.get("revision") == d5["revision"] and res["revision"] != d4["revision"], (st, res))
    check("409 left files unchanged", after == edited and d5["units"][0]["name"] != "SHOULD NOT BE SAVED")
    st, res = put_project(api, "demo-srpg", d4, "")
    check("PUT with empty revision -> 409", st == 409)
    st, res = api.json("PUT", "/api/projects/demo-srpg", {"game": d5["game"]})
    check("PUT without baseRevision -> 400", st == 400)

    # ---- edit ----
    pid = "new-strategy-rpg"
    st, d = get_project(api, pid)
    d["units"][0]["stats"]["hp"] = 77
    st, res = put_project(api, pid, d, d["revision"])
    disk_units = read_json(os.path.join(games, pid, "data", "units.json"))
    check("edit unit stat: PUT 200", st == 200 and res["problems"] == [], (st, res))
    check("edit unit stat: file on disk changed, wrapped in {units: [...]}",
          list(disk_units.keys()) == ["units"] and disk_units["units"][0]["stats"]["hp"] == 77)
    st, d = get_project(api, pid)
    check("edit unit stat: visible through fresh GET", d["units"][0]["stats"]["hp"] == 77)
    with open(os.path.join(games, pid, "data", "units.json")) as f:
        txt = f.read()
    check("written JSON is pretty (2 spaces) and ends with newline", txt.endswith("}\n") and '\n  "units": [' in txt)

    # add a map
    d["game"]["maps"].append({"id": "second", "name": "Second", "file": "maps/second.json"})
    d["maps"]["second"] = copy.deepcopy(d["maps"]["ford"])
    d["maps"]["second"]["name"] = "Second"
    st, res = put_project(api, pid, d, d["revision"])
    p2 = os.path.join(games, pid, "maps", "second.json")
    check("add map: file created, project loads", st == 200 and res["problems"] == [] and os.path.isfile(p2), (st, res))
    st, d = get_project(api, pid)
    check("add map: GET lists both maps", sorted(d["maps"].keys()) == ["ford", "second"])
    # add a map in a new sub-folder
    d["game"]["maps"].append({"id": "deep", "name": "Deep", "file": "maps/sub/deep.json"})
    d["maps"]["deep"] = copy.deepcopy(d["maps"]["ford"])
    st, res = put_project(api, pid, d, d["revision"])
    check("add map in new sub-folder creates it",
          st == 200 and os.path.isfile(os.path.join(games, pid, "maps", "sub", "deep.json")), (st, res))
    st, d = get_project(api, pid)
    # remove maps
    d["game"]["maps"] = [m for m in d["game"]["maps"] if m["id"] not in ("second", "deep")]
    del d["maps"]["second"]
    del d["maps"]["deep"]
    st, res = put_project(api, pid, d, d["revision"])
    check("remove map from game.maps: its file is deleted",
          st == 200 and not os.path.exists(p2) and not os.path.exists(os.path.join(games, pid, "maps", "sub", "deep.json"))
          and os.path.isfile(os.path.join(games, pid, "maps", "ford.json")), (st, res))
    st, d = get_project(api, pid)
    check("remove map: GET lists one map and no problems", list(d["maps"].keys()) == ["ford"] and d["problems"] == [])

    # scripts
    d["scripts"] = {"scripts/extra.lua": "-- hello\nfunction f() return 1 end\n"}
    d["game"]["scripts"] = ["scripts/extra.lua"]
    st, res = put_project(api, pid, d, d["revision"])
    check("PUT script text: file written, project valid", st == 200 and res["problems"] == []
          and open(os.path.join(games, pid, "scripts", "extra.lua")).read().startswith("-- hello"), (st, res))
    st, d = get_project(api, pid)
    check("GET returns script text", d["scripts"].get("scripts/extra.lua", "").startswith("-- hello"))

    # null / absent sections untouched
    st, res = api.json("PUT", "/api/projects/" + pid, {"baseRevision": d["revision"], "units": None, "game": None})
    st2, d_after = get_project(api, pid)
    check("null sections are left untouched", st == 200 and d_after["units"] == d["units"] and d_after["game"] == d["game"])

    # ---- invalid data is saved but reported ----
    st, d = get_project(api, pid)
    good_weapons = copy.deepcopy(d["units"][0]["weapons"])
    d["units"][0]["weapons"] = ["no_such_weapon"]
    st, res = put_project(api, pid, d, d["revision"])
    probs = res.get("problems", [])
    check("unknown weapon: saved (200) and reported",
          st == 200 and any(p["file"] == "data/units.json" and "no_such_weapon" in p["message"] for p in probs), (st, res))
    disk_units = read_json(os.path.join(games, pid, "data", "units.json"))
    check("unknown weapon: invalid data really written", disk_units["units"][0]["weapons"] == ["no_such_weapon"])
    st, pr = api.json("GET", "/api/projects/%s/problems" % pid)
    check("/problems returns the same problems", st == 200 and pr["problems"] == probs, pr)
    st, plist = api.json("GET", "/api/projects")
    entry = {p["id"]: p for p in plist["projects"]}[pid]
    check("project list shows ok=false with error", entry["ok"] is False and "no_such_weapon" in entry["error"], entry)
    st, d = get_project(api, pid)
    check("GET reports the same problem", any("no_such_weapon" in p["message"] for p in d["problems"]))
    d["units"][0]["weapons"] = good_weapons
    st, res = put_project(api, pid, d, d["revision"])
    check("fixing it clears the problems", st == 200 and res["problems"] == [], res)

    # problems from other stages
    st, d = get_project(api, pid)
    d["maps"]["ford"]["units"][0]["type"] = "ghost"
    st, res = put_project(api, pid, d, d["revision"])
    check("map problem names the map file",
          st == 200 and any(p["file"] == "maps/ford.json" and "ghost" in p["message"] for p in res["problems"]), res)
    d["maps"]["ford"]["units"][0]["type"] = "lord"
    d["game"]["formulas"]["damage"] = "this is not lua ("
    st, res = put_project(api, pid, d, res["revision"])
    check("bad Lua formula is reported", st == 200 and len(res["problems"]) >= 1, res)
    st, d = get_project(api, pid)
    d["game"]["formulas"]["damage"] = "(a.str + w.might) - b.def"
    st, res = put_project(api, pid, d, d["revision"])
    check("fixed formula clears problems", st == 200 and res["problems"] == [], res)

    # ---- broken project ----
    bp = "new-blank"
    with open(os.path.join(games, bp, "data", "terrain.json"), "w") as f:
        f.write("this is { not json")
    st, plist = api.json("GET", "/api/projects")
    entry = {p["id"]: p for p in plist["projects"]}[bp]
    check("broken project listed with ok=false and an error", entry["ok"] is False and entry["error"] != "", entry)
    st, d = get_project(api, bp)
    check("broken project: terrain null + problem naming the file",
          st == 200 and d["terrain"] is None and any(p["file"] == "data/terrain.json" for p in d["problems"]), d.get("problems"))
    check("broken project: other sections intact",
          isinstance(d["game"], dict) and isinstance(d["units"], list) and isinstance(d["weapons"], list)
          and "start" in d["maps"])
    # a project whose game.json is garbage can still be opened
    bp2 = "new-economy-tactics"
    with open(os.path.join(games, bp2, "game.json"), "w") as f:
        f.write("{ oops")
    st, d = get_project(api, bp2)
    check("garbage game.json: GET still 200 with null game/maps/scripts and a problem",
          st == 200 and d["game"] is None and d["maps"] is None and d["scripts"] is None
          and any(p["file"] == "game.json" for p in d["problems"]) and isinstance(d["units"], list), d.get("problems"))
    os.remove(os.path.join(games, bp2, "data", "weapons.json"))
    st, d = get_project(api, "new-economy-tactics")
    check("missing file: section null + problem", d["weapons"] is None and any("weapons.json" in p["file"] for p in d["problems"]))
    # repair the first broken project through the API
    st, dd = get_project(api, bp)
    st, res = api.json("PUT", "/api/projects/" + bp, {
        "baseRevision": dd["revision"],
        "terrain": [{"id": "plain", "name": "Plain", "color": "#8fbf6a", "glyph": "."},
                    {"id": "wall", "name": "Wall", "color": "#555555", "glyph": "#", "moveCost": {"foot": 0}}]})
    check("PUT can repair a broken section", st == 200 and res["problems"] == [], (st, res))

    # ---- wrong-typed PUT sections ----
    pid = "new-strategy-rpg"
    st, d = get_project(api, pid)
    cases = [("game", []), ("terrain", {}), ("weapons", "x"), ("units", {"a": 1}), ("maps", []),
             ("maps", {"ford": 5}), ("scripts", []), ("scripts", {"scripts/a.lua": 5})]
    for key, val in cases:
        st, res = api.json("PUT", "/api/projects/" + pid, {"baseRevision": d["revision"], key: val})
        st2, d2 = get_project(api, pid)
        check("wrong-typed %s=%s -> 400, nothing written" % (key, json.dumps(val)),
              st == 400 and "error" in res and d2["revision"] == d["revision"], (st, res))
    st, res = api.json("PUT", "/api/projects/" + pid, {"baseRevision": d["revision"], "units": [1, 2]})
    check("PUT with garbage unit entries is saved and reported", st == 200 and len(res["problems"]) >= 1, (st, res))

    # ---- path safety ----
    sbefore = open(sentinel).read()
    st, d = get_project(api, "demo-economy")
    rev0 = d["revision"]
    attacks = [
        ("../evil.json", "map file ../evil.json"),
        ("/tmp/ote-evil.json", "absolute map file"),
        ("maps/../../evil.json", "map file maps/../../evil.json"),
        ("maps\\..\\evil.json", "backslash map file"),
        ("maps/x.txt", "map file without .json"),
        ("game.json", "map file = game.json"),
        ("", "empty map file"),
    ]
    for f, label in attacks:
        evil = copy.deepcopy(d)
        evil["game"]["maps"].append({"id": "evil", "name": "Evil", "file": f})
        evil["maps"]["evil"] = {"name": "evil"}
        st, res = put_project(api, "demo-economy", evil, rev0)
        st2, dd = get_project(api, "demo-economy")
        check("PUT %s -> 400, nothing written" % label, st == 400 and dd["revision"] == rev0, (st, res))
    evil = copy.deepcopy(d)
    evil["scripts"] = {"../evil.lua": "x"}
    st, res = put_project(api, "demo-economy", evil, rev0)
    check("PUT script ../evil.lua -> 400", st == 400)
    evil = copy.deepcopy(d)
    evil["maps"]["not_listed"] = {"name": "x"}
    st, res = put_project(api, "demo-economy", evil, rev0)
    check("PUT map id missing from game.maps -> 400", st == 400)
    evil = copy.deepcopy(d)
    evil["game"]["scripts"] = ["../outside.lua"]
    st, res = put_project(api, "demo-economy", evil, rev0)
    check("PUT game.scripts path escape -> 400", st == 400)
    for p in ("/api/projects/demo-srpg/file/..%2f..%2fetc%2fpasswd",
              "/api/projects/demo-srpg/file/../../../etc/passwd",
              "/api/projects/demo-srpg/file/%2fetc%2fpasswd",
              "/api/projects/demo-srpg/file//etc/passwd",
              "/api/projects/demo-srpg/file/..%2f..%2fsentinel.txt",
              "/api/projects/demo-srpg/file/maps/..%2f..%2f..%2fwork%2fsentinel.txt",
              "/api/projects/%2e%2e/file/x",
              "/api/projects/..%2f/problems"):
        st, hdrs, data = api.call("GET", p)
        check("file path attack %s -> 400/404, no data" % p, st in (400, 404) and b"root:" not in data and b"do not touch" not in data, st)
    sym = os.path.join(games, "demo-srpg", "escape-link")
    try:
        os.symlink(sentinel, sym)
        st, hdrs, data = api.call("GET", "/api/projects/demo-srpg/file/escape-link")
        check("symlink escape is rejected", st in (400, 404) and b"do not touch" not in data, st)
        st, fl = api.json("GET", "/api/projects/demo-srpg/files")
        check("symlinks are not listed as project files", all(f["path"] != "escape-link" for f in fl["files"]))
    except OSError:
        print("SKIP  symlink checks (cannot create symlinks)")
    finally:
        if os.path.islink(sym):
            os.remove(sym)
    check("sentinel file untouched", open(sentinel).read() == sbefore)
    evil_found = [f for f in tree_files(tmp) if os.path.basename(f) in ("evil.json", "evil.lua", "ote-evil.json")]
    check("no evil.* file exists anywhere", evil_found == [] and not os.path.exists("/tmp/ote-evil.json"), evil_found)

    # ---- Host header ----
    st, hdrs, data = api.call("GET", "/api/info", headers={"Host": "evil.example"})
    check("Host: evil.example -> 403", st == 403)
    st, hdrs, data = api.call("GET", "/api/projects", headers={"Host": "evil.example:%d" % port})
    check("Host: evil.example:port -> 403 on API", st == 403)
    st, hdrs, data = api.call("GET", "/", headers={"Host": "evil.example"})
    check("Host: evil.example -> 403 on static too", st == 403)
    for h in ("localhost:%d" % port, "127.0.0.1:%d" % port, "[::1]:%d" % port, "localhost"):
        st, hdrs, data = api.call("GET", "/api/info", headers={"Host": h})
        check("Host: %s -> 200" % h, st == 200)

    # ---- static ----
    st, hdrs, data = api.call("GET", "/")
    check("/ returns HTML 200 (built-in page when UI missing)",
          st == 200 and hdrs.get("content-type", "").startswith("text/html") and b"<html" in data.lower())
    st, hdrs, data = api.call("GET", "/some/spa/route")
    check("unknown non-API path without UI -> 404 (not a crash)", st == 404)
    st, hdrs, data = api.call("GET", "/assets/fonts/fonts.json")
    check("/assets/fonts/fonts.json -> 200 json", st == 200 and hdrs.get("content-type", "").startswith("application/json"))
    st, hdrs, data = api.call("GET", "/assets/fonts/Alegreya.ttf")
    check("/assets ttf -> 200 font/ttf", st == 200 and hdrs.get("content-type") == "font/ttf" and len(data) > 1000)
    st, hdrs, data = api.call("GET", "/assets/../games/index.json")
    check("/assets path escape rejected", st in (400, 404) and b"demo-srpg" not in data)
    st, hdrs, data = api.call("GET", "/assets/%2e%2e/%2e%2e/work/sentinel.txt")
    check("/assets encoded path escape rejected", st in (400, 404) and b"do not touch" not in data)
    st, hdrs, data = api.call("GET", "/player/ote-player.js")
    check("/player missing file -> 404", st == 404)
    st, doc = api.json("GET", "/api/nope")
    check("unknown /api/nope -> 404 JSON error", st == 404 and isinstance(doc, dict) and "error" in doc, (st, doc))
    st, hdrs, data = api.call("GET", "/api/nope")
    check("404 error is application/json", hdrs.get("content-type", "").startswith("application/json"))
    st, doc = api.json("POST", "/api/info", {})
    check("POST on a GET-only API route -> JSON 4xx", 400 <= st < 500 and isinstance(doc, dict) and "error" in doc, (st, doc))
    st, hdrs, data = api.call("GET", "/api/projects")
    check("API responses are application/json + Cache-Control: no-store",
          hdrs.get("content-type", "").startswith("application/json") and hdrs.get("cache-control") == "no-store")
    st, hdrs, data = api.call("GET", "/assets/fonts/fonts.json")
    check("static responses send Cache-Control: no-store", hdrs.get("cache-control") == "no-store")

    # ---- oversized body ----
    big = b"{\"id\":\"" + b"a" * (17 * 1024 * 1024) + b"\"}"
    try:
        st, hdrs, data = api.call("POST", "/api/projects", raw_body=big, timeout=60)
        check("oversized body (>16 MB) -> 413", st == 413, st)
    except (OSError, http.client.HTTPException) as e:
        check("oversized body (>16 MB): connection closed cleanly", True, repr(e))
    st, doc = api.json("GET", "/api/info")
    check("helper alive right after the oversized body", st == 200 and proc.poll() is None)
    # a large-but-allowed body works
    st, d = get_project(api, "demo-srpg")
    d["maps"]["bridge"]["padding"] = ["x" * 1000] * 3000  # ~3 MB
    st, res = put_project(api, "demo-srpg", d, d["revision"])
    check("PUT of a ~3 MB document works", st == 200, st)


if __name__ == "__main__":
    sys.exit(main())
