"""Strategy-RPG data checks of the ote-editor API test (docs/SRPG.md "Editor", docs/EDITOR_API.md): the sections
characters / skills / abilities (lists, like items; an empty list is written, false removes the file) and supports (one object, false
removes the file). Called from editor_api_test.py; uses its helpers."""
import json as _json
import os

LISTS = {
    "characters": ("data/characters.json", [
        {"id": "eirik", "name": "Eirik", "type": "knight", "zzz_unknown": {"keep": [1, 2]}, "stats": {"hp": 2}},
        {"id": "lyra", "name": "Lyra", "type": "archer", "level": 2}]),
    "skills": ("data/skills.json", [
        {"id": "heavy_blow", "name": "Heavy Blow", "blocks": [{"when": "strike", "do": [{"damagePercent": 200}]}]}]),
    "abilities": ("data/abilities.json", [
        {"id": "shove", "name": "Shove", "target": "unit", "effect": {"push": 1}}]),
}
SUPPORTS = ("data/supports.json", {"rules": {"range": 3}, "pairs": [{"a": "eirik", "b": "lyra", "levels": []}]})
ALL_KEYS = list(LISTS) + ["supports"]


def snapshot(root):
    out = {}
    for dirpath, _, names in os.walk(root):
        for n in names:
            full = os.path.join(dirpath, n)
            with open(full, "rb") as f:
                out[os.path.relpath(full, root)] = f.read()
    return out


def run_srpg_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "srpg-test"
    st, doc = api.json("POST", "/api/projects", {"id": pid, "name": "Srpg", "template": "blank"})
    check("srpg: create scratch project", st == 201, (st, doc))
    root = os.path.join(games, pid)

    def path(rel):
        return os.path.join(root, rel)

    def raw(rel):
        with open(path(rel), "rb") as f:
            return f.read()

    def has_problem(d, rel):
        return any(p["file"] == rel for p in d["problems"])

    for rel in [v[0] for v in LISTS.values()] + [SUPPORTS[0]]:
        if os.path.exists(path(rel)):
            os.remove(path(rel))

    # absent files: null, no problem; a PUT that leaves them out or null creates none
    st, d = get_project(api, pid)
    check("srpg: absent files -> four nulls, no problem",
          st == 200 and all(k in d and d[k] is None for k in ALL_KEYS) and
          not any(has_problem(d, v[0]) for v in LISTS.values()) and not has_problem(d, SUPPORTS[0]), d.get("characters"))
    full = {k: d[k] for k in ALL_KEYS}
    st, res = put_project(api, pid, d, d["revision"], **full)
    check("srpg: PUT with the four sections null creates no file",
          st == 200 and not any(os.path.exists(path(v[0])) for v in list(LISTS.values()) + [SUPPORTS]), res)

    for key, (rel, lst) in LISTS.items():
        st, d = get_project(api, pid)
        st, res = put_project(api, pid, d, d["revision"], **{key: lst})
        check("srpg %s: PUT list -> 200" % key, st == 200 and "revision" in res, (st, res))
        check("srpg %s: written as {\"%s\": [...]} pretty-printed" % (key, key),
              os.path.exists(path(rel)) and raw(rel).decode().startswith('{\n  "%s": [\n' % key) and raw(rel).endswith(b"}\n"))
        check("srpg %s: no temp file left" % key, not [n for n in os.listdir(os.path.dirname(path(rel))) if n.endswith(".ote-tmp")])
        st, d = get_project(api, pid)
        check("srpg %s: GET returns the list, unknown keys and order kept" % key, d[key] == lst, d[key])
        check("srpg %s: revision follows the write" % key, d["revision"] == res["revision"])

        # absent or null leaves the file byte-identical
        before = raw(rel)
        st, r1 = put_project(api, pid, d, d["revision"])
        st, d = get_project(api, pid)
        st, r2 = put_project(api, pid, d, d["revision"], **{key: None})
        check("srpg %s: absent or null leaves the file byte-identical" % key, raw(rel) == before and r1.get("revision") and r2.get("revision"))

        # wrong types -> 400, nothing written
        st, d = get_project(api, pid)
        for bad in ({key: []}, "x", 5, True):
            st, res = put_project(api, pid, d, d["revision"], terrain=d["terrain"], **{key: bad})
            check("srpg %s: PUT %r -> 400" % (key, bad), st == 400, (st, res))
        check("srpg %s: rejected PUT wrote nothing" % key, raw(rel) == before)

        # broken files: null plus a problem; a PUT repairs
        for label, text in (("invalid JSON", b"{ not json"), ("root is a list", b"[1,2]"),
                            ("list missing", b"{}"), ("not a list", b'{"%s": 5}' % key.encode())):
            with open(path(rel), "wb") as f:
                f.write(text)
            st, d = get_project(api, pid)
            check("srpg %s: %s -> null plus a problem on the file" % (key, label),
                  st == 200 and d[key] is None and has_problem(d, rel), (d[key], d["problems"]))
        st, res = put_project(api, pid, d, d["revision"], **{key: lst})
        st, d = get_project(api, pid)
        check("srpg %s: PUT repairs a broken file" % key, st == 200 and d[key] == lst, d[key])

        # an empty list is written (the switch stays on: owner, 2026-11-15); false removes the file
        st, res = put_project(api, pid, d, d["revision"], **{key: []})
        check("srpg %s: PUT empty list writes an empty file" % key,
              st == 200 and os.path.exists(path(rel)) and _json.loads(raw(rel).decode()) == {key: []}, res)
        st, d = get_project(api, pid)
        check("srpg %s: afterwards GET is an empty list" % key, d[key] == [], d[key])
        st, res = put_project(api, pid, d, d["revision"], **{key: False})
        check("srpg %s: PUT false removes the file" % key, st == 200 and not os.path.exists(path(rel)), res)
        st, d = get_project(api, pid)
        check("srpg %s: afterwards GET is null (the same game as before)" % key, d[key] is None and not has_problem(d, rel), d[key])
        st, res = put_project(api, pid, d, d["revision"], **{key: False})
        check("srpg %s: PUT false with no file is fine" % key, st == 200 and not os.path.exists(path(rel)), res)

        # a map may not use the file's name
        st, d = get_project(api, pid)
        g2 = dict(d["game"])
        g2["maps"] = list(g2["maps"]) + [{"id": "evil", "file": rel}]
        st, res = put_project(api, pid, d, d["revision"], game=g2, maps=dict(d["maps"], evil={"x": 1}))
        check("srpg %s: map file %s is reserved -> 400" % (key, rel), st == 400, (st, res))

    # supports: one object; false removes the file
    rel, sup = SUPPORTS
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], supports=sup)
    check("srpg supports: PUT object -> written", st == 200 and raw(rel).decode().startswith('{\n  "rules"'), (st, res))
    st, d = get_project(api, pid)
    check("srpg supports: GET returns the object", d["supports"] == sup, d["supports"])
    before = raw(rel)
    st, r1 = put_project(api, pid, d, d["revision"], supports=None)
    check("srpg supports: null leaves the file byte-identical", st == 200 and raw(rel) == before, r1)
    st, d = get_project(api, pid)
    for bad in ([], "x", 5, True):
        st, res = put_project(api, pid, d, d["revision"], terrain=d["terrain"], supports=bad)
        check("srpg supports: PUT %r -> 400" % (bad,), st == 400, (st, res))
    check("srpg supports: rejected PUT wrote nothing", raw(rel) == before)
    for label, text in (("invalid JSON", b"{ not json"), ("root is a list", b"[1]")):
        with open(path(rel), "wb") as f:
            f.write(text)
        st, d = get_project(api, pid)
        check("srpg supports: %s -> null plus a problem" % label, d["supports"] is None and has_problem(d, rel), d["problems"])
    st, res = put_project(api, pid, d, d["revision"], supports=False)
    check("srpg supports: false removes the file (even a broken one)", st == 200 and not os.path.exists(path(rel)), res)
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], supports=False)
    check("srpg supports: false with no file is fine", st == 200, res)

    # the files are part of the revision
    st, d = get_project(api, pid)
    rev0 = d["revision"]
    st, res = put_project(api, pid, d, rev0, characters=LISTS["characters"][1])
    check("srpg: a new file changes the revision", res.get("revision") and res["revision"] != rev0, res)
    st, res = put_project(api, pid, d, rev0, characters=[])
    check("srpg: a stale revision is a 409", st == 409, (st, res))

    # an existing game without the files opens and saves byte-identically and gains none
    st, doc = api.json("POST", "/api/projects", {"id": "srpg-plain", "name": "Plain", "template": "strategy-rpg"})
    check("srpg: create a game from the strategy-rpg template", st == 201, (st, doc))
    snap = snapshot(os.path.join(games, "srpg-plain"))
    st, d = get_project(api, "srpg-plain")
    # (Sections the editor does send are re-serialised, as for every other section; the web client only sends
    # what changed. What matters here: the four new sections as null touch nothing and create nothing.)
    full = {k: d[k] for k in ALL_KEYS}
    check("srpg: the template has none of the new files", all(d[k] is None for k in ALL_KEYS), [d[k] for k in ALL_KEYS])
    st, res = put_project(api, "srpg-plain", {"revision": d["revision"]}, d["revision"], **full)
    after = snapshot(os.path.join(games, "srpg-plain"))
    check("srpg: saving a game without the files changes nothing and adds nothing",
          st == 200 and after == snap, [k for k in set(after) | set(snap) if after.get(k) != snap.get(k)])
