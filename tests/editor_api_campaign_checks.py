"""data/campaign.json checks of the ote-editor API test (docs/EVENTS.md "Campaign", docs/EDITOR_API.md).
Called from editor_api_test.py; uses its helpers. Unlike items, the file holds ONE object (not a wrapped list), and
`"campaign": false` removes the file (null / absent leave it alone)."""
import json
import os


def run_campaign_checks(api, tmp, games, templates, check, get_project, put_project):
    # The demo ships a campaign (with comments in the file): it is served as a plain object, without problems.
    st, d = get_project(api, "demo-srpg")
    check("campaign: the demo's campaign is served as an object",
          st == 200 and isinstance(d.get("campaign"), dict) and d["campaign"]["name"] == "The Ford War"
          and [c["id"] for c in d["campaign"]["chapters"]] == ["ford", "pass"], d.get("campaign"))
    check("campaign: the demo has no problem on the file",
          not any(p["file"] == "data/campaign.json" for p in d["problems"]), d["problems"])

    pid = "campaign-test"
    st, doc = api.json("POST", "/api/projects", {"id": pid, "name": "Campaign", "template": "blank"})
    check("campaign: create scratch project", st == 201, (st, doc))
    sfile = os.path.join(games, pid, "data", "campaign.json")
    if os.path.exists(sfile):
        os.remove(sfile)
    # The template's art points at pictures it does not ship; the loader stops at the first complaint, so drop it.
    gfile = os.path.join(games, pid, "data", "graphics.json")
    if os.path.exists(gfile):
        os.remove(gfile)

    def raw():
        with open(sfile, "rb") as f:
            return f.read()

    def has_problem(dd):
        return any(p["file"] == "data/campaign.json" for p in dd["problems"])

    # absent
    st, d = get_project(api, pid)
    check("campaign: absent file -> null, no problem",
          st == 200 and "campaign" in d and d["campaign"] is None and not has_problem(d), d.get("campaign"))
    rev0 = d["revision"]
    first_map = d["game"]["maps"][0]["id"]

    # absent / null in PUT leaves a missing file missing; false on a missing file is fine
    st, res = put_project(api, pid, d, d["revision"])
    check("campaign: PUT without the key does not create the file", st == 200 and not os.path.exists(sfile), res)
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], campaign=None)
    check("campaign: PUT null does not create the file", st == 200 and not os.path.exists(sfile), res)
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], campaign=False)
    check("campaign: PUT false on a missing file -> 200", st == 200 and not os.path.exists(sfile), (st, res))

    # a valid campaign loads without a problem
    ok = {"name": "Valid", "chapters": [{"id": "a", "map": first_map}]}
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], campaign=ok)
    check("campaign: a valid campaign has no problem", st == 200 and not any(p["file"] == "data/campaign.json" for p in res["problems"]), res)

    # round trip: key order and keys the editor does not know are kept (the loader refuses such keys, but the
    # helper saves work in progress as sent)
    camp = {"name": "The Story", "rules": {"fallen": "gone", "onDefeat": "retry"}, "freeBattles": "all",
            "futureKey": {"keep": [1, 2]},
            "chapters": [{"id": "ch1", "name": "One", "map": first_map, "before": [{"say": "Hi", "speakerName": "Narrator"}],
                          "after": [], "next": "ch1", "extra": True}]}
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], campaign=camp)
    check("campaign: PUT object -> 200", st == 200 and "revision" in res, (st, res))
    check("campaign: written pretty-printed with the keys in the order received",
          os.path.exists(sfile) and raw().decode().startswith('{\n  "name": "The Story",\n  "rules"') and raw().endswith(b"}\n"))
    check("campaign: no temp file left", not [n for n in os.listdir(os.path.dirname(sfile)) if n.endswith(".ote-tmp")])
    check("campaign: key order on disk",
          list(json.loads(raw()).keys()) == ["name", "rules", "freeBattles", "futureKey", "chapters"])
    st, d = get_project(api, pid)
    check("campaign: GET returns it unchanged (unknown keys included)", d["campaign"] == camp, d["campaign"])
    check("campaign: revision changed after the file appeared", d["revision"] != rev0 and d["revision"] == res["revision"])

    # a loader complaint is reported (and saved anyway); the loader runs on the file
    bad = {"name": "Bad", "chapters": [{"id": "ch1", "map": "nope"}]}
    st, res = put_project(api, pid, d, d["revision"], campaign=bad)
    check("campaign: PUT of a campaign the loader refuses -> 200 plus a problem on the file",
          st == 200 and any(p["file"] == "data/campaign.json" and "nope" in p["message"] for p in res["problems"]), res)
    st, d = get_project(api, pid)
    check("campaign: the refused campaign is saved as sent", d["campaign"] == bad)

    # absent / null leave the file byte-identical
    before = raw()
    st, r1 = put_project(api, pid, d, d["revision"])
    st, d = get_project(api, pid)
    st, r2 = put_project(api, pid, d, d["revision"], campaign=None)
    check("campaign: absent or null leaves the file byte-identical", raw() == before and r1.get("revision") and r2.get("revision"))

    # wrong types -> 400, nothing written
    st, d = get_project(api, pid)
    for badv in ([], "x", 5, True):
        st, res = put_project(api, pid, d, d["revision"], campaign=badv, terrain=d["terrain"])
        check("campaign: PUT %r -> 400" % (badv,), st == 400, (st, res))
    check("campaign: rejected PUT wrote nothing", raw() == before)

    # invalid JSON / wrong shape -> null plus a problem; a PUT repairs it
    for label, text in (("invalid JSON", b"{ not json"), ("root is a list", b"[1,2]"), ("root is a string", b'"x"')):
        with open(sfile, "wb") as f:
            f.write(text)
        st, d = get_project(api, pid)
        check("campaign: %s -> null plus a problem on the file" % label,
              st == 200 and d["campaign"] is None and has_problem(d), (d["campaign"], d["problems"]))
    st, res = put_project(api, pid, d, d["revision"], campaign=camp)
    check("campaign: PUT repairs a broken file", st == 200 and raw().startswith(b'{\n  "name"'), res)

    # false removes the file, and the project reads as "no campaign" again
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], campaign=False)
    check("campaign: PUT false removes the file", st == 200 and not os.path.exists(sfile), (st, res))
    st, d = get_project(api, pid)
    check("campaign: after removal GET says null, no problem", d["campaign"] is None and not has_problem(d))

    # reserved name
    g2 = dict(d["game"])
    g2["maps"] = list(g2["maps"]) + [{"id": "evil", "file": "data/campaign.json"}]
    st, res = put_project(api, pid, d, d["revision"], game=g2, maps=dict(d["maps"], evil={"x": 1}))
    check("campaign: map file data/campaign.json is reserved -> 400", st == 400, (st, res))
