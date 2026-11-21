"""data/structures.json checks of the ote-editor API test (docs/STRUCTURES.md,
docs/EDITOR_API.md). Called from editor_api_test.py; uses its helpers.
Independent of whether the template ships a structures file: it is deleted first."""
import os


def run_structures_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "struct-test"
    st, doc = api.json("POST", "/api/projects", {"id": pid, "name": "Struct", "template": "blank"})
    check("structures: create scratch project", st == 201, (st, doc))
    sfile = os.path.join(games, pid, "data", "structures.json")
    if os.path.exists(sfile):
        os.remove(sfile)

    def raw():
        with open(sfile, "rb") as f:
            return f.read()

    def has_problem(d):
        return any(p["file"] == "data/structures.json" for p in d["problems"])

    # absent
    st, d = get_project(api, pid)
    check("structures: absent file -> null, no problem",
          st == 200 and "structures" in d and d["structures"] is None and not has_problem(d), d.get("structures"))
    rev0 = d["revision"]

    # absent / null in PUT leaves a missing file missing
    st, res = put_project(api, pid, d, d["revision"])
    check("structures: PUT without the key does not create the file", st == 200 and not os.path.exists(sfile), res)
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], structures=None)
    check("structures: PUT null does not create the file", st == 200 and not os.path.exists(sfile), res)

    # round trip, including empty
    lst = [{"id": "hq", "name": "Headquarters", "width": 2, "height": 2}, {"id": "city", "name": "City"}]
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], structures=lst)
    check("structures: PUT list -> 200", st == 200 and "revision" in res, (st, res))
    check("structures: written as {\"structures\": [...]} pretty-printed",
          os.path.exists(sfile) and raw().decode().startswith('{\n  "structures": [\n') and raw().endswith(b"}\n"))
    check("structures: no temp file left",
          not [n for n in os.listdir(os.path.dirname(sfile)) if n.endswith(".ote-tmp")])
    st, d = get_project(api, pid)
    check("structures: GET returns the list", d["structures"] == lst, d["structures"])
    check("structures: revision changed after the file appeared",
          d["revision"] != rev0 and d["revision"] == res["revision"])

    # revision tracks the file content
    rev1 = d["revision"]
    with open(sfile, "ab") as f:
        f.write(b"\n")
    st, d = get_project(api, pid)
    check("structures: editing the file changes the revision", d["revision"] != rev1)

    st, res = put_project(api, pid, d, d["revision"], structures=[])
    check("structures: PUT empty list -> 200", st == 200, (st, res))
    st, d = get_project(api, pid)
    check("structures: empty list round trips (not null)", d["structures"] == [] and not has_problem(d), d["structures"])

    # absent / null leave an existing file byte-identical
    st, res = put_project(api, pid, d, d["revision"], structures=lst)
    before = raw()
    st, d = get_project(api, pid)
    st, r1 = put_project(api, pid, d, d["revision"])
    st, d = get_project(api, pid)
    st, r2 = put_project(api, pid, d, d["revision"], structures=None)
    check("structures: absent or null leaves the file byte-identical",
          raw() == before and r1.get("revision") and r2.get("revision"))

    # wrong types -> 400, nothing written
    st, d = get_project(api, pid)
    for bad in ({"structures": []}, "x", 5, True):
        st, res = put_project(api, pid, d, d["revision"], structures=bad, terrain=d["terrain"])
        check("structures: PUT %r -> 400" % (bad,), st == 400, (st, res))
    check("structures: rejected PUT wrote nothing", raw() == before)

    # invalid JSON / wrong shape
    for label, text in (("invalid JSON", b"{ not json"), ("root is a list", b"[1,2]"),
                        ("structures not a list", b'{"structures": 5}'), ("root without key", b"{}")):
        with open(sfile, "wb") as f:
            f.write(text)
        st, d = get_project(api, pid)
        check("structures: %s -> null plus a problem on the file" % label,
              st == 200 and d["structures"] is None and has_problem(d), (d["structures"], d["problems"]))
    st, res = put_project(api, pid, d, d["revision"], structures=lst)
    check("structures: PUT repairs a broken file", st == 200 and raw().startswith(b'{\n  "structures"'), res)
    st, d = get_project(api, pid)
    check("structures: repaired file reads back", d["structures"] == lst and not has_problem(d))

    # saved even when the loader may dislike it
    odd = [5, "x"]
    st, res = put_project(api, pid, d, d["revision"], structures=odd)
    st, d = get_project(api, pid)
    check("structures: saved even if the loader may dislike it", st == 200 and d["structures"] == odd, d["structures"])

    # reserved name
    g2 = dict(d["game"])
    g2["maps"] = list(g2["maps"]) + [{"id": "evil", "file": "data/structures.json"}]
    st, res = put_project(api, pid, d, d["revision"], game=g2, maps=dict(d["maps"], evil={"x": 1}))
    check("structures: map file data/structures.json is reserved -> 400", st == 400, (st, res))
