"""data/items.json checks of the ote-editor API test (docs/ITEMS.md,
docs/EDITOR_API.md). Called from editor_api_test.py; uses its helpers.
Independent of whether the template ships a items file: it is deleted first."""
import os


def run_items_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "items-test"
    st, doc = api.json("POST", "/api/projects", {"id": pid, "name": "Items", "template": "blank"})
    check("items: create scratch project", st == 201, (st, doc))
    sfile = os.path.join(games, pid, "data", "items.json")
    if os.path.exists(sfile):
        os.remove(sfile)

    def raw():
        with open(sfile, "rb") as f:
            return f.read()

    def has_problem(d):
        return any(p["file"] == "data/items.json" for p in d["problems"])

    # absent
    st, d = get_project(api, pid)
    check("items: absent file -> null, no problem",
          st == 200 and "items" in d and d["items"] is None and not has_problem(d), d.get("items"))
    rev0 = d["revision"]

    # absent / null in PUT leaves a missing file missing
    st, res = put_project(api, pid, d, d["revision"])
    check("items: PUT without the key does not create the file", st == 200 and not os.path.exists(sfile), res)
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], items=None)
    check("items: PUT null does not create the file", st == 200 and not os.path.exists(sfile), res)

    # round trip, including empty
    lst = [{"id": "potion", "name": "Potion", "kind": "usable", "heal": 5}, {"id": "ring", "name": "Ring", "kind": "accessory", "bonus": {"hp": 1}}]
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], items=lst)
    check("items: PUT list -> 200", st == 200 and "revision" in res, (st, res))
    check("items: written as {\"items\": [...]} pretty-printed",
          os.path.exists(sfile) and raw().decode().startswith('{\n  "items": [\n') and raw().endswith(b"}\n"))
    check("items: no temp file left",
          not [n for n in os.listdir(os.path.dirname(sfile)) if n.endswith(".ote-tmp")])
    st, d = get_project(api, pid)
    check("items: GET returns the list", d["items"] == lst, d["items"])
    check("items: revision changed after the file appeared",
          d["revision"] != rev0 and d["revision"] == res["revision"])

    # revision tracks the file content
    rev1 = d["revision"]
    with open(sfile, "ab") as f:
        f.write(b"\n")
    st, d = get_project(api, pid)
    check("items: editing the file changes the revision", d["revision"] != rev1)

    st, res = put_project(api, pid, d, d["revision"], items=[])
    check("items: PUT empty list -> 200", st == 200, (st, res))
    st, d = get_project(api, pid)
    check("items: empty list round trips (not null)", d["items"] == [] and not has_problem(d), d["items"])

    # absent / null leave an existing file byte-identical
    st, res = put_project(api, pid, d, d["revision"], items=lst)
    before = raw()
    st, d = get_project(api, pid)
    st, r1 = put_project(api, pid, d, d["revision"])
    st, d = get_project(api, pid)
    st, r2 = put_project(api, pid, d, d["revision"], items=None)
    check("items: absent or null leaves the file byte-identical",
          raw() == before and r1.get("revision") and r2.get("revision"))

    # wrong types -> 400, nothing written
    st, d = get_project(api, pid)
    for bad in ({"items": []}, "x", 5, True):
        st, res = put_project(api, pid, d, d["revision"], items=bad, terrain=d["terrain"])
        check("items: PUT %r -> 400" % (bad,), st == 400, (st, res))
    check("items: rejected PUT wrote nothing", raw() == before)

    # invalid JSON / wrong shape
    for label, text in (("invalid JSON", b"{ not json"), ("root is a list", b"[1,2]"),
                        ("items not a list", b'{"items": 5}'), ("root without key", b"{}")):
        with open(sfile, "wb") as f:
            f.write(text)
        st, d = get_project(api, pid)
        check("items: %s -> null plus a problem on the file" % label,
              st == 200 and d["items"] is None and has_problem(d), (d["items"], d["problems"]))
    st, res = put_project(api, pid, d, d["revision"], items=lst)
    check("items: PUT repairs a broken file", st == 200 and raw().startswith(b'{\n  "items"'), res)
    st, d = get_project(api, pid)
    check("items: repaired file reads back", d["items"] == lst and not has_problem(d))

    # saved even when the loader may dislike it
    odd = [5, "x"]
    st, res = put_project(api, pid, d, d["revision"], items=odd)
    st, d = get_project(api, pid)
    check("items: saved even if the loader may dislike it", st == 200 and d["items"] == odd, d["items"])

    # reserved name
    g2 = dict(d["game"])
    g2["maps"] = list(g2["maps"]) + [{"id": "evil", "file": "data/items.json"}]
    st, res = put_project(api, pid, d, d["revision"], game=g2, maps=dict(d["maps"], evil={"x": 1}))
    check("items: map file data/items.json is reserved -> 400", st == 400, (st, res))
