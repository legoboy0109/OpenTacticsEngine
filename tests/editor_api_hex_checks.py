"""Hexagon game checks of the ote-editor API test (docs/HEX.md, docs/EDITOR_API.md). Called from editor_api_test.py;
uses its helpers. The games are copies of the engine's own fixtures (tests/fixtures/hex-pointy, hex-flat): the helper only
passes `grid` and `rings` through, and the engine loader (linked into the helper) is what judges them."""
import copy
import os
import shutil

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def _strict(path):
    """Rewrites a hand-written fixture file (comments) as strict JSON, the way the editor stores it."""
    import json
    import re
    with open(path) as f:
        text = f.read()
    text = re.sub(r"^\s*//.*$", "", text, flags=re.M)
    text = re.sub(r",(\s*[}\]])", r"\1", text)
    with open(path, "w") as f:
        json.dump(json.loads(text), f, indent=2)
        f.write("\n")


def _copy_fixture(layout, dest):
    shutil.copytree(os.path.join(REPO, "tests", "fixtures", "hex-" + layout), dest)
    os.remove(os.path.join(dest, "README.md"))
    for root, _dirs, files in os.walk(dest):
        for name in files:
            if name.endswith(".json"):
                _strict(os.path.join(root, name))


def run_hex_checks(api, tmp, games, templates, check, get_project, put_project):
    problems_of = lambda d: [p["message"] for p in d["problems"]]

    for layout in ("pointy", "flat"):
        pid = "hex-" + layout
        _copy_fixture(layout, os.path.join(games, pid))

        st, d = get_project(api, pid)
        check("hex %s: GET gives the grid of game.json" % layout,
              st == 200 and d["game"]["grid"] == {"type": "hex", "layout": layout}, d.get("game", {}).get("grid"))
        check("hex %s: the maps carry grid.type hex and structures carry rings" % layout,
              d["maps"]["skirmish"]["grid"]["type"] == "hex" and
              any(s.get("id") == "hq" and s.get("rings") == 1 for s in d["structures"]))
        check("hex %s: the fixture loads without problems" % layout, d["problems"] == [], d["problems"])

        # saving keeps grid and rings exactly as they are
        st, res = put_project(api, pid, d, d["revision"], structures=d["structures"])
        check("hex %s: PUT -> 200 without problems" % layout, st == 200 and res["problems"] == [], res)
        st, d2 = get_project(api, pid)
        check("hex %s: grid and rings survive a save" % layout,
              d2["game"]["grid"] == {"type": "hex", "layout": layout} and d2["structures"] == d["structures"] and
              d2["maps"]["skirmish"]["grid"]["type"] == "hex")

        # the engine loader judges hexagon rules (the helper adds nothing of its own)
        bad = copy.deepcopy(d2)
        bad["game"]["styles"] = [{"id": "iso", "name": "Iso", "view": "sideways"}]
        st, res = put_project(api, pid, bad, d2["revision"])
        check("hex %s: a bad style is reported by the loader" % layout,
              st == 200 and any('"view" must be' in m for m in problems_of(res)), res)
        st, d3 = get_project(api, pid)
        check("hex %s: the refused style is still saved (work in progress is never lost)" % layout,
              d3["game"]["styles"][0]["view"] == "sideways")

        bad = copy.deepcopy(d2)
        st, res = put_project(api, pid, bad, d3["revision"], structures=[dict(s, width=2) if s["id"] == "city" else s for s in d2["structures"]])
        check("hex %s: width on a hexagon game is reported" % layout,
              any('"width" is for square games' in m for m in problems_of(res)), res)

        bad = copy.deepcopy(d2)
        bad["maps"]["skirmish"]["grid"]["type"] = "square"
        st, res = put_project(api, pid, bad, res["revision"], structures=d2["structures"])
        check("hex %s: a square map in a hexagon game is reported" % layout,
              any("this is a hexagon game" in m for m in problems_of(res)), res)

        # a structure with rings at the map edge, and one on top of another
        base = copy.deepcopy(d2)
        edge = copy.deepcopy(base["maps"]["skirmish"])
        edge["structures"] = [{"type": "hq", "x": 0, "y": 0}]
        st, res = put_project(api, pid, base, res["revision"], maps={"skirmish": edge}, structures=d2["structures"])
        check("hex %s: a ring that sticks out of the map is reported" % layout,
              any("six around it (rings 1) but they do not all fit inside the map" in m for m in problems_of(res)), res)
        overlap = copy.deepcopy(base["maps"]["skirmish"])
        overlap["structures"] = [{"type": "hq", "x": 5, "y": 4}, {"type": "city", "x": 5, "y": 4}]
        st, res = put_project(api, pid, base, res["revision"], maps={"skirmish": overlap}, structures=d2["structures"])
        check("hex %s: overlapping structures are reported" % layout,
              any("overlaps structures[0]" in m for m in problems_of(res)), res)

        # grid with an unknown key: refused by the loader, still written as received (unknown keys are never dropped)
        odd = copy.deepcopy(d2)
        odd["game"]["grid"]["extra"] = 1
        st, res = put_project(api, pid, odd, res["revision"], maps={"skirmish": d2["maps"]["skirmish"]}, structures=d2["structures"])
        check("hex %s: an unknown key in grid is reported by the loader" % layout,
              any("grid" in m and "unknown" in m.lower() for m in problems_of(res)), res)
        st, d4 = get_project(api, pid)
        check("hex %s: the unknown key was written as received" % layout, d4["game"]["grid"].get("extra") == 1)

    # new projects: the template list tells each template's grid
    _copy_fixture("flat", os.path.join(templates, "zz-hex-flat"))
    _copy_fixture("pointy", os.path.join(templates, "zz-hex-pointy"))
    st, tpl = api.json("GET", "/api/templates")
    by_id = {t["id"]: t for t in tpl["templates"]}
    check("templates: a square template says squares", by_id["blank"]["grid"] == {"type": "square"}, by_id.get("blank"))
    check("templates: a hexagon template tells type and layout",
          by_id["zz-hex-flat"]["grid"] == {"type": "hex", "layout": "flat"} and
          by_id["zz-hex-pointy"]["grid"]["type"] == "hex", (by_id.get("zz-hex-flat"), by_id.get("zz-hex-pointy")))

    # a project made from a hexagon template is a hexagon game
    st, res = api.json("POST", "/api/projects", {"id": "from-hex", "name": "From Hex", "template": "zz-hex-flat"})
    check("hex: create a project from a hexagon template", st == 201, (st, res))
    st, d = get_project(api, "from-hex")
    check("hex: the new project keeps the template's grid and its map",
          st == 200 and d["game"]["grid"] == {"type": "hex", "layout": "flat"} and d["maps"]["skirmish"]["grid"]["type"] == "hex" and
          d["problems"] == [], (d.get("game", {}).get("grid"), d.get("problems")))
