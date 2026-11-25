"""Speech bubbles, notices, names, faces and loops through the ote-editor API (docs/EVENTS.md "Speech bubbles, notices, names, faces and
loops"): game.json "dialogue", the notice / loop / break commands, a choice's notice / cursor / face, a say's face, a portrait's
"expressions", the campaign scene's askName and the campaign's / a scene's "backdrop" come back with GET, are written back unchanged by
PUT, and the real loader (GET .../problems) accepts them and names the main refusals. Called from editor_api_test.py; uses its helpers."""
import json
import os

from editor_api_graphics_checks import make_png


def run_dialogue_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "dialogue-test"
    st, doc = api.json("POST", "/api/projects", {"id": pid, "name": "Dialogue", "template": "blank"})
    check("dialogue: create scratch project", st == 201, (st, doc))
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

    def copy(x):
        return json.loads(json.dumps(x))

    png = {"Content-Type": "image/png"}
    art = "/api/projects/%s/art" % pid
    st, _, _ = api.call("PUT", art + "/places.png", raw_body=make_png(240, 68), headers=png)
    check("dialogue: upload a sheet of two backgrounds", st == 201, st)
    st, _, _ = api.call("PUT", art + "/faces.png", raw_body=make_png(144, 48), headers=png)
    check("dialogue: upload a sheet of three faces", st == 201, st)

    st, d = get_project(api, pid)
    graphics = {
        "sheets": [
            {"id": "places", "file": "art/places.png", "cellWidth": 120, "cellHeight": 68},
            {"id": "faces", "file": "art/faces.png", "cellWidth": 48, "cellHeight": 48},
        ],
        "portraits": {"eirik": {"sheet": "faces", "cell": [0, 0], "expressions": [[1, 0], [2, 0]]}},
        "backgrounds": {"hall": {"sheet": "places", "cell": [0, 0]}, "cellar": {"sheet": "places", "cell": [1, 0]}},
    }
    game = copy(d["game"])
    game["dialogue"] = {"style": "bubble", "portrait": "inside", "names": False}
    m = copy(d["maps"]["start"])
    m["units"][0]["id"] = "eirik"
    m["units"][0]["name"] = "Eirik"
    m["events"] = [{"id": "lesson", "when": {"battle": "start"}, "do": [
        {"say": "Kupo!", "speaker": "eirik", "face": 2},
        {"notice": "Your clan has grown stronger.", "auto": 1500},
        {"loop": [
            {"say": "Now, attack!", "speaker": "eirik", "face": 1},
            {"choice": "Repeat lesson?", "notice": True, "cursor": 2,
             "answers": [{"text": "Yes"}, {"text": "No", "do": [{"break": True}]}]},
            {"choice": "Ready?", "speaker": "eirik", "face": 1, "cursor": 1, "answers": [{"text": "Yes"}, {"text": "No"}]},
        ], "max": 5},
    ]}]
    campaign = {"name": "Names", "backdrop": "hall", "chapters": [{
        "id": "ch1", "name": "One", "map": "start",
        "before": {"backdrop": "cellar", "commands": [
            {"askName": "eirik", "default": "Eirik", "max": 8, "prompt": "Enter your name."},
            {"say": "Welcome, {name:eirik}!", "speakerName": "Guide", "face": 1},
            {"loop": [{"notice": "Again?"}, {"choice": "Repeat?", "notice": True, "cursor": 2,
                                              "answers": [{"text": "Yes"}, {"text": "No", "do": [{"break": True}]}]}]},
        ]},
        "after": {"backdrop": None, "commands": [{"narrate": "Done."}]},
    }]}
    st, res = put_project(api, pid, d, d["revision"], game=game, maps={"start": m}, graphics=graphics, campaign=campaign)
    check("dialogue: PUT the keys -> 200", st == 200 and "revision" in res, (st, res))
    check("dialogue: the real loader accepts them (no problems)", problems() == [], problems())
    st, d = get_project(api, pid)
    check("dialogue: GET returns game.dialogue as written", d["game"].get("dialogue") == game["dialogue"], d["game"].get("dialogue"))
    check("dialogue: GET returns the portrait's expressions", d["graphics"]["portraits"]["eirik"]["expressions"] == [[1, 0], [2, 0]], d["graphics"])
    check("dialogue: GET returns the map event's notice / loop / break / cursor / face exactly", d["maps"]["start"]["events"] == m["events"])
    check("dialogue: GET returns the campaign (backdrops, askName, loop) exactly", d["campaign"] == campaign, d["campaign"])
    check("dialogue: the files keep every new key",
          '"dialogue"' in raw("game.json") and '"expressions"' in raw("data/graphics.json") and '"notice"' in raw("maps/start.json")
          and '"cursor"' in raw("maps/start.json") and '"askName"' in raw("data/campaign.json") and '"backdrop"' in raw("data/campaign.json"))
    check("dialogue: a null backdrop is written as null", '"backdrop": null' in raw("data/campaign.json"))

    def refused(label, needle, file, game=None, graphics=None, maps=None, campaign=None, where=None):
        st, cur = get_project(api, pid)
        over = {}
        if game is not None:
            over["game"] = game
        if graphics is not None:
            over["graphics"] = graphics
        if maps is not None:
            over["maps"] = maps
        if campaign is not None:
            over["campaign"] = campaign
        st, res = put_project(api, pid, cur, cur["revision"], **over)
        ps = problems()
        check("dialogue: the loader refuses " + label,
              st == 200 and any(p["file"] == file and needle in p["message"] and (where is None or where in p["message"]) for p in ps), (st, ps))
        st, cur = get_project(api, pid)  # back to valid
        put_project(api, pid, cur, cur["revision"], game=game_ok, maps={"start": m}, graphics=graphics_ok, campaign=campaign_ok)

    game_ok = copy(game)
    graphics_ok = copy(graphics)
    campaign_ok = copy(campaign)

    def event_map(cmds):
        mm = copy(m)
        mm["events"] = [{"id": "oops", "when": {"battle": "start"}, "do": cmds}]
        return {"start": mm}

    def scene_campaign(cmds):
        c = copy(campaign)
        c["chapters"][0]["before"] = {"commands": cmds}
        return c

    g = copy(game)
    g["dialogue"] = {"style": "box", "names": True}
    refused("portrait / names with the box style", "need \"style\": \"bubble\"", "game.json", game=g)
    g = copy(game)
    g["dialogue"] = {"style": "balloon"}
    refused("an unknown style", "style", "game.json", game=g)
    refused("a say's face above the portrait's expressions", "needs a portrait with at least", "maps/start.json",
            maps=event_map([{"say": "x", "speaker": "eirik", "face": 3}]))
    refused("a face of 16", "face", "maps/start.json", maps=event_map([{"say": "x", "speaker": "eirik", "face": 16}]))
    refused("a notice window with a speaker", "makes no sense with \"notice\": true", "maps/start.json",
            maps=event_map([{"choice": "x?", "notice": True, "speaker": "eirik", "answers": [{"text": "a"}, {"text": "b"}]}]))
    refused("a cursor past the answers", "cursor", "maps/start.json",
            maps=event_map([{"choice": "x?", "cursor": 3, "answers": [{"text": "a"}, {"text": "b"}]}]))
    refused("a break outside a loop", "\"break\" ends a loop", "maps/start.json", maps=event_map([{"break": True}]))
    refused("a loop with max 101", "max", "maps/start.json", maps=event_map([{"loop": [{"wait": 1}], "max": 101}]))
    refused("a notice with an unknown key", "notice", "maps/start.json", maps=event_map([{"notice": "x", "speaker": "eirik"}]))
    refused("askName in a map event", "\"askName\" is for campaign scenes only", "maps/start.json", maps=event_map([{"askName": "eirik"}]))
    refused("a name token with no closing brace", "\"{name:\" must be followed by a character id", "maps/start.json",
            maps=event_map([{"say": "Hi {name:eirik"}]))
    refused("an askName with max 25", "max", "data/campaign.json", campaign=scene_campaign([{"askName": "eirik", "max": 25}]))
    refused("an askName default longer than max", "default", "data/campaign.json",
            campaign=scene_campaign([{"askName": "eirik", "default": "Longname", "max": 3}]))
    c = copy(campaign)
    c["backdrop"] = "swamp"
    refused("a campaign backdrop that is no background", "swamp", "data/campaign.json", campaign=c)
    c = copy(campaign)
    c["chapters"][0]["after"] = {"backdrop": "swamp", "commands": [{"narrate": "x"}]}
    refused("a scene backdrop that is no background", "swamp", "data/campaign.json", campaign=c)
    gr = copy(graphics)
    gr["portraits"]["eirik"]["expressions"] = [[9, 0]]
    refused("an expression outside the sheet", "outside sheet", "data/graphics.json", graphics=gr)

    st, d = get_project(api, pid)
    check("dialogue: after the refusals the valid game is back and clean", problems() == [] and d["game"]["dialogue"] == game["dialogue"], problems())

    # ---- removing the keys removes them from disk ----
    g_no = copy(game)
    del g_no["dialogue"]
    m_plain = copy(m)
    m_plain["events"] = [{"id": "lesson", "when": {"battle": "start"}, "do": [{"say": "Plain."}]}]
    gr_plain = copy(graphics)
    del gr_plain["portraits"]["eirik"]["expressions"]
    c_plain = {"name": "Plain", "chapters": [{"id": "ch1", "map": "start", "before": [{"say": "Plain."}]}]}
    st, res = put_project(api, pid, d, d["revision"], game=g_no, maps={"start": m_plain}, graphics=gr_plain, campaign=c_plain)
    check("dialogue: removing the keys removes them from disk",
          st == 200 and '"dialogue"' not in raw("game.json") and '"expressions"' not in raw("data/graphics.json")
          and '"backdrop"' not in raw("data/campaign.json") and problems() == [], (st, problems()))
