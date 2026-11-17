"""Battle and defeat quotes checks of the ote-editor API test (docs/EVENTS.md "Battle and defeat quotes"): the helper must keep a
character's "quotes" untouched through a load and a save, and GET .../problems must give the loader's own words for a bad quote.
The scratch project is the committed fixture tests/fixtures/srpg-quotes. Called from editor_api_test.py; uses its helpers."""
import json
import os
import shutil


def run_quotes_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "quotes-test"
    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    dst = os.path.join(games, pid)
    shutil.copytree(os.path.join(repo, "tests", "fixtures", "srpg-quotes"), dst)

    def read(rel):
        # the fixture's files may start with // comment lines
        with open(os.path.join(dst, rel), "r", encoding="utf-8") as f:
            return json.loads("".join(l for l in f if not l.lstrip().startswith("//")))

    st, p = api.json("GET", "/api/projects/%s/problems" % pid)
    check("quotes: the real loader accepts the fixture", st == 200 and p["problems"] == [], p)
    st, d = get_project(api, pid)
    chars = {c["id"]: c for c in d["characters"]}
    check("quotes: GET carries the quotes as written", chars["gorm"]["quotes"] == read("data/characters.json")["characters"][3]["quotes"], chars["gorm"])
    st, res = put_project(api, pid, d, d["revision"], characters=d["characters"])
    check("quotes: saving the characters back is accepted", st == 200, (st, res))
    check("quotes: the file keeps every quote", read("data/characters.json")["characters"][0]["quotes"]["battle"][0]["against"] == "gorm")

    original = json.loads(json.dumps(d["characters"]))

    def bad(mutate):
        st, d = get_project(api, pid)
        cs = json.loads(json.dumps(original))  # every case starts from the fixture's characters
        mutate({c["id"]: c for c in cs})
        put_project(api, pid, d, d["revision"], characters=cs)
        st, p = api.json("GET", "/api/projects/%s/problems" % pid)
        return [x["message"] for x in p["problems"]]

    m = bad(lambda c: c["eirik"]["quotes"]["battle"][0].update(against="nobody"))
    check("quotes: an unknown opponent -> the loader's words", any('battle quote 1: "against" names unknown character "nobody"' in x for x in m), m)
    m = bad(lambda c: c["eirik"]["quotes"]["battle"][0].update(against="eirik"))
    check("quotes: a quote against oneself", any('names the character itself' in x for x in m), m)
    m = bad(lambda c: c["eirik"]["quotes"]["battle"].append({"against": "gorm", "lines": [{"say": "x"}]}))
    check("quotes: a second quote against the same character", any('a second quote against "gorm"' in x for x in m), m)
    m = bad(lambda c: c["bram"]["quotes"]["battle"][0].update(lines=[]))
    check("quotes: a quote with no lines", any('"lines" must be a list with at least one say command' in x for x in m), m)
