"""Story scene presentation through the ote-editor API (docs/EVENTS.md "Scene presentation", docs/GRAPHICS.md "Backgrounds"):
graphics.json's `backgrounds`, the scene commands in a map's events and in a campaign's scenes, and the extra keys of say,
focus and moveUnit pass through like every other key (they round-trip in order, unknown keys survive, the file is written
pretty-printed), and the real loader (GET .../problems) accepts a valid scene and names every broken one with its file, the
event or chapter and the command. Called from editor_api_test.py; uses its helpers."""
import json
import os

from editor_api_graphics_checks import make_png


def run_scene_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "scene-test"
    st, doc = api.json("POST", "/api/projects", {"id": pid, "name": "Scene", "template": "blank"})
    check("scene: create scratch project", st == 201, (st, doc))
    root = os.path.join(games, pid)
    for rel in ("data/graphics.json", "data/campaign.json", "data/audio.json"):
        if os.path.exists(os.path.join(root, rel)):
            os.remove(os.path.join(root, rel))

    def raw(rel):
        with open(os.path.join(root, rel), "rb") as f:
            return f.read().decode()

    def problems():
        st, res = api.json("GET", "/api/projects/%s/problems" % pid)
        return res["problems"] if st == 200 else [{"file": "?", "message": str(res)}]

    png = {"Content-Type": "image/png"}
    art = "/api/projects/%s/art" % pid
    st, _, _ = api.call("PUT", art + "/forest.png", raw_body=make_png(240, 135), headers=png)
    check("scene: upload a whole-image background", st == 201, st)
    st, _, _ = api.call("PUT", art + "/places.png", raw_body=make_png(240, 68), headers=png)
    check("scene: upload a sheet of two pieces", st == 201, st)
    st, _, _ = api.call("PUT", art + "/faces.png", raw_body=make_png(96, 48), headers=png)

    st, d = get_project(api, pid)
    graphics = {
        "sheets": [
            {"id": "forest", "file": "art/forest.png", "cellWidth": 240, "cellHeight": 135},
            {"id": "places", "file": "art/places.png", "cellWidth": 120, "cellHeight": 68},
            {"id": "faces", "file": "art/faces.png", "cellWidth": 48, "cellHeight": 48},
        ],
        "portraits": {"eirik": {"sheet": "faces", "cell": [0, 0]}, "gorm": {"sheet": "faces", "cell": [1, 0]}},
        "backgrounds": {
            "forest_dusk": {"sheet": "forest"},
            "hall": {"sheet": "places", "cell": [0, 0]},
            "cellar": {"sheet": "places", "cell": [1, 0]},
        },
    }
    m = json.loads(json.dumps(d["maps"]["start"]))
    m["units"][0]["id"] = "eirik"
    m["units"][0]["name"] = "Eirik"
    m["units"][1]["id"] = "gorm"
    m["events"] = [
        {"id": "intro", "when": {"battle": "start"}, "do": [
            {"fade": "out", "ms": 300, "color": "#102030"},
            {"background": "forest_dusk"},
            {"fade": "in", "ms": 300},
            {"say": "The forest is quiet.", "speaker": "eirik", "portrait": "eirik", "position": "left"},
            {"say": "Who goes there?", "speaker": "gorm", "position": "right", "auto": 800},
            {"wait": 400},
            {"shake": {"ms": 300, "strength": 3}},
            {"narrate": "Something stirs.", "auto": 600},
            {"leave": "gorm"},
            {"background": "hall", "transition": "fade", "ms": 500},
            {"background": None},
            {"clearStage": True},
            {"focus": {"x": 3, "y": 1}, "ms": 400, "wait": False},
            {"focus": "gorm", "ms": 0},
            {"weather": "snow"},
            {"moveUnit": "eirik", "x": 2, "y": 1, "walk": True, "wait": False},
            {"moveUnit": "gorm", "x": 4, "y": 1, "walk": True},
            {"weather": "none"},
        ]},
    ]
    campaign = {"name": "Scenes", "chapters": [{
        "id": "ch1", "name": "One", "map": "start",
        "before": [
            {"fade": "out", "ms": 200}, {"background": "cellar"}, {"fade": "in"},
            {"say": "We camp.", "speakerName": "Eirik", "portrait": "eirik", "position": "center"},
            {"narrate": "Night."}, {"wait": 300}, {"shake": {"ms": 200, "strength": 4}},
            {"leave": "Eirik"}, {"clearStage": True}, {"background": None, "transition": "fade", "ms": 250},
        ],
        "after": [{"background": "hall"}, {"say": "Home.", "speakerName": "Eirik", "auto": 900}],
    }]}
    st, res = put_project(api, pid, d, d["revision"], maps={"start": m}, graphics=graphics, campaign=campaign)
    check("scene: PUT backgrounds, scene commands and a campaign -> 200", st == 200 and "revision" in res, (st, res))
    check("scene: the real loader accepts it (no problems)", problems() == [], problems())
    st, d = get_project(api, pid)
    check("scene: GET returns the backgrounds exactly, in order, in order",
          d["graphics"].get("backgrounds") == graphics["backgrounds"] and list(d["graphics"]["backgrounds"]) == ["forest_dusk", "hall", "cellar"], d["graphics"])
    check("scene: GET returns the map's scene commands exactly (background null, ms, wait false, walk)", d["maps"]["start"]["events"] == m["events"])
    check("scene: GET returns the campaign's scenes exactly", d["campaign"] == campaign)
    check("scene: the file is pretty-printed", '"backgrounds": {\n    "forest_dusk": {' in raw("data/graphics.json"))
    check("scene: a null background is written as null", '"background": null' in raw("maps/start.json"))

    # ---- what the real loader refuses, named by file, event / chapter and command ----
    def bad_event(cmds):
        mm = json.loads(json.dumps(m))
        mm["events"] = [{"id": "oops", "when": {"battle": "start"}, "do": cmds}]
        st, d2 = get_project(api, pid)
        st, res = put_project(api, pid, d2, d2["revision"], maps={"start": mm})
        check("scene: a broken scene is still saved (200)", st == 200, (st, res))
        return problems()

    for what, cmds, needle in (
        ("an unknown background", [{"background": "swamp"}], 'unknown background "swamp"'),
        ("a transition that is neither", [{"background": "hall", "transition": "wipe"}], '"transition" must be "cut" or "fade"'),
        ("a fade time out of range", [{"fade": "out", "ms": 99999}], '"ms" is milliseconds'),
        ("a colour that is not one", [{"fade": "out", "color": "black"}], '"color" must be a colour'),
        ("a negative wait", [{"wait": -1}], '"wait" is milliseconds'),
        ("a shake strength of 0", [{"shake": {"strength": 0}}], '"strength" is pixels'),
        ("weather that is not weather", [{"weather": "fog"}], '"weather" must be "rain", "snow" or "none"'),
        ("a place that is not one", [{"say": "x", "speaker": "eirik", "position": "middle"}], '"position" must be'),
        ("a walk that is not true or false", [{"moveUnit": "eirik", "x": 1, "y": 1, "walk": "yes"}], '"walk" must be true or false'),
        ("a wait with no walk", [{"moveUnit": "eirik", "x": 1, "y": 1, "wait": False}], '"wait" only means something with "walk": true'),
    ):
        ps = bad_event(cmds)
        check("scene: the loader refuses " + what,
              any(p["file"] == "maps/start.json" and 'event "oops"' in p["message"] and needle in p["message"] for p in ps), ps)
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], maps={"start": m})
    check("scene: PUT repairs it", st == 200 and problems() == [], problems())

    # ---- a campaign scene: weather and map-only commands are refused, with the chapter named ----
    st, d = get_project(api, pid)
    cbad = json.loads(json.dumps(campaign))
    cbad["chapters"][0]["before"] = [{"weather": "rain"}, {"background": "swamp"}]
    st, res = put_project(api, pid, d, d["revision"], campaign=cbad)
    ps = problems()
    check("scene: a campaign scene may not use weather",
          any(p["file"] == "data/campaign.json" and 'chapter "ch1"' in p["message"] and '"weather" is for map events only' in p["message"] for p in ps), ps)
    cbad["chapters"][0]["before"] = [{"background": "swamp"}]
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], campaign=cbad)
    ps = problems()
    check("scene: a campaign scene names its unknown background",
          any(p["file"] == "data/campaign.json" and 'unknown background "swamp"' in p["message"] for p in ps), ps)
    cbad["chapters"][0]["before"] = [{"focus": {"x": 1, "y": 1}}]
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], campaign=cbad)
    check("scene: focus is not a scene command", any("is not a scene command" in p["message"] for p in problems()), problems())
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], campaign=campaign)
    check("scene: PUT repairs the campaign", st == 200 and problems() == [], problems())

    # ---- a graphics file with a broken background ----
    gbad = json.loads(json.dumps(graphics))
    gbad["backgrounds"]["cellar"]["cell"] = [9, 0]
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], graphics=gbad)
    ps = problems()
    check("scene: a background cell outside its sheet is named", any(p["file"] == "data/graphics.json" and 'background "cellar"' in p["message"] and "outside sheet" in p["message"] for p in ps), ps)
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], graphics=graphics)
    check("scene: PUT repairs the graphics", st == 200 and problems() == [], problems())

    # ---- removing the key removes it from disk ----
    g_no = json.loads(json.dumps(graphics))
    del g_no["backgrounds"]
    m_plain = json.loads(json.dumps(m))
    m_plain["events"] = [{"id": "intro", "when": {"battle": "start"}, "do": [{"wait": 100}]}]
    cplain = json.loads(json.dumps(campaign))
    cplain["chapters"][0]["before"] = [{"say": "Plain."}]
    cplain["chapters"][0]["after"] = [{"say": "Plain."}]
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], graphics=g_no, maps={"start": m_plain}, campaign=cplain)
    st, d = get_project(api, pid)
    check("scene: removing the backgrounds removes the key from disk", st == 200 and "backgrounds" not in d["graphics"] and '"backgrounds"' not in raw("data/graphics.json"), d["graphics"])
    check("scene: and the game without them has no problems", problems() == [], problems())
