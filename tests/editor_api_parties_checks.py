"""Campaign parties through the ote-editor API (docs/EVENTS.md "Parties"): `parties`, a chapter's `party` and the scene commands `switchParty` and
`mergeParties` are passed through like every other key: they open and save byte for byte, the real loader (GET .../problems) accepts them and names what
it refuses. Starts from a copy of tests/fixtures/srpg-skillblocks2 (a field map with named units). Called from editor_api_test.py; uses its helpers."""
import json
import os
import shutil

FIXTURE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fixtures", "srpg-skillblocks2")


def run_parties_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "parties-api"
    shutil.copytree(FIXTURE, os.path.join(games, pid))
    root = os.path.join(games, pid)

    def problems():
        st, res = api.json("GET", "/api/projects/%s/problems" % pid)
        return [p["message"] for p in res["problems"]] if st == 200 else [str(res)]

    def copy(x):
        return json.loads(json.dumps(x))

    st, d = get_project(api, pid)
    check("parties: the fixture loads", st == 200 and problems() == [], problems())
    mid = d["game"]["maps"][0]["id"]
    campaign = {
        "name": "Two roads",
        "parties": [{"id": "main", "name": "The company"}, {"id": "north", "name": "The northern road"}],
        "chapters": [
            {"id": "c1", "name": "One", "map": mid},
            {"id": "c2", "name": "Two", "map": mid, "party": "north", "before": [{"say": "North."}]},
            {"id": "c3", "name": "Three", "map": mid, "before": [{"switchParty": "main"}, {"mergeParties": ["north"], "into": "main"}]},
            {"id": "c4", "name": "Four", "map": mid, "after": [{"if": [{"switch": "nothing"}], "then": []}]},
        ],
    }
    campaign["chapters"][3].pop("after")
    st, res = put_project(api, pid, d, d["revision"], campaign=campaign)
    check("parties: PUT of the new keys -> 200", st == 200 and "revision" in res, (st, res))
    check("parties: the real loader accepts them", problems() == [], problems())

    path = os.path.join(root, "data", "campaign.json")
    with open(path, "rb") as f:
        before = f.read()
    st, d = get_project(api, pid)
    check("parties: GET returns the keys as written",
          d["campaign"]["parties"] == campaign["parties"] and d["campaign"]["chapters"][1]["party"] == "north"
          and d["campaign"]["chapters"][2]["before"] == campaign["chapters"][2]["before"], d.get("campaign"))
    st, res = put_project(api, pid, d, d["revision"], campaign=d["campaign"])
    with open(path, "rb") as f:
        after = f.read()
    check("parties: open + save keeps the file byte for byte", st == 200 and after == before)
    check("parties: the keys are in the file", b'"parties"' in after and b'"switchParty"' in after and b'"mergeParties"' in after and b'"into"' in after)

    def refused(label, change, needle):
        st, d = get_project(api, pid)
        body = copy(campaign)
        change(body)
        st, res = put_project(api, pid, d, d["revision"], campaign=body)
        ps = problems()
        check("parties: %s is named by the loader" % label, st == 200 and any(needle in p for p in ps), ps)

    refused("a duplicate party id", lambda b: b["parties"].append({"id": "main"}), "duplicate party id")
    refused("a party id with a space", lambda b: b["parties"].__setitem__(0, {"id": "my party"}), "letters, digits")
    refused("an unknown key on a party", lambda b: b["parties"][0].__setitem__("color", "red"), "unknown key")
    refused("a switch to an unknown party", lambda b: b["chapters"][2]["before"].__setitem__(0, {"switchParty": "south"}), "names unknown party \"south\"")
    refused("a chapter party that is unknown", lambda b: b["chapters"][1].__setitem__("party", "south"), "names unknown party \"south\"")
    refused("a merge into an unknown party", lambda b: b["chapters"][2]["before"].__setitem__(1, {"mergeParties": ["north"], "into": "east"}), "names unknown party \"east\"")
    refused("a merge into itself", lambda b: b["chapters"][2]["before"].__setitem__(1, {"mergeParties": ["main"], "into": "main"}), "the party it merges into")
    refused("a merge listing a party twice", lambda b: b["chapters"][2]["before"].__setitem__(1, {"mergeParties": ["north", "north"], "into": "main"}), "twice")
    refused("a merge with nothing to merge", lambda b: b["chapters"][2]["before"].__setitem__(1, {"mergeParties": [], "into": "main"}), "list of party ids")
    refused("a merge without into", lambda b: b["chapters"][2]["before"].__setitem__(1, {"mergeParties": ["north"]}), "missing required \"into\"")
    refused("a switch with another key", lambda b: b["chapters"][2]["before"].__setitem__(0, {"switchParty": "main", "into": "north"}), "unknown key")
    refused("party commands in a campaign without parties", lambda b: b.pop("parties"), "needs the campaign to have parties")

    # Back to a valid state: nothing lingers.
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], campaign=campaign)
    check("parties: valid data again -> no problems", st == 200 and problems() == [], problems())
