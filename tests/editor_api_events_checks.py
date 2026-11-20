"""Story events through the ote-editor API (docs/EVENTS.md, docs/EDITOR_API.md): a map's `events`, `objectives` and
unit `id`, game.json's `switches` and `variables`, and graphics.json's `portraits`. The helper passes these through
like every other key, so the checks are: they round-trip, keep their order and unknown keys, are written
pretty-printed, and the real loader (GET .../problems) accepts a valid story and names a broken one.
Called from editor_api_test.py; uses its helpers."""
import json
import os


def run_events_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "events-test"
    st, doc = api.json("POST", "/api/projects", {"id": pid, "name": "Events", "template": "blank"})
    check("events: create scratch project", st == 201, (st, doc))
    # Earlier checks may have emptied the template's art folder: start from a project without any art, so the
    # only problems the loader can report are about the story.
    gfile = os.path.join(games, pid, "data", "graphics.json")
    if os.path.exists(gfile):
        os.remove(gfile)

    def raw(rel):
        with open(os.path.join(games, pid, rel), "rb") as f:
            return f.read()

    def problems():
        st, res = api.json("GET", "/api/projects/%s/problems" % pid)
        return res["problems"] if st == 200 else [{"file": "?", "message": str(res)}]

    st, d = get_project(api, pid)
    check("events: blank project has no story and no problems",
          st == 200 and "events" not in d["maps"]["start"] and "switches" not in d["game"] and not d["problems"], d["problems"])

    # ---- a valid story: every trigger kind, every command kind, nested ifs, objectives ----
    game = json.loads(json.dumps(d["game"]))
    game["switches"] = [{"id": "village_saved", "name": "Village saved"}]
    game["variables"] = [{"id": "gold_found", "name": "Gold found"}]
    m = json.loads(json.dumps(d["maps"]["start"]))
    m["units"][0]["id"] = "eirik"
    m["units"][0]["name"] = "Eirik"
    m["units"][1]["id"] = "gorm"
    events = [
        {"id": "opening", "when": {"battle": "start"}, "do": [
            {"say": "Now! Strike from the forest!", "speaker": "gorm", "portrait": "gorm"},
            {"say": "Who goes there?", "speakerName": "Villager"},
            {"focus": {"x": 6, "y": 3}}, {"focus": "gorm"}]},
        {"id": "won", "when": {"battle": "won"}, "do": [{"setSwitch": "village_saved", "to": True}]},
        {"id": "lost", "when": {"battle": "lost"}, "do": [{"say": "Defeat."}]},
        {"id": "ended", "when": {"battle": "end"}, "do": [{"say": "The end."}]},
        {"id": "ambush", "when": {"turn": 4, "side": "red"}, "if": [{"switch": "village_saved", "is": False}], "repeat": False, "do": [
            {"addUnit": {"type": "soldier", "side": "red", "x": 7, "y": 1, "id": "scout", "name": "Scout", "level": 2, "leader": False}},
            {"moveUnit": "scout", "x": 5, "y": 1},
            {"changeSide": {"unit": "scout"}, "side": "blue", "ready": True},
            {"removeUnit": {"type": "soldier"}},
            {"setVariable": "gold_found", "to": 3}, {"addVariable": "gold_found", "by": -1},
            {"objective": {"text": "Hold out", "win": [{"survive": 6}], "lose": [{"round": 10}]}},
            {"win": "blue"}, {"lose": "red"}]},
        {"id": "every", "when": {"turn": "every"}, "do": [
            {"if": [{"variable": "gold_found", "op": ">=", "value": 2}, {"round": {"op": ">", "value": 1}}], "then": [
                {"if": [{"alive": {"unit": "gorm"}}, {"dead": {"side": "red"}}], "then": [
                    {"if": [{"switch": "village_saved"}], "then": [{"say": "three deep"}], "else": [{"say": "three deep else"}]}]}],
             "else": [{"say": "no"}]}]},
        {"id": "arrive", "when": {"reach": {"x": 5, "y": 2, "w": 2, "h": 1}, "by": {"team": "blue"}}, "do": [{"say": "Arrived"}]},
        {"id": "down", "when": {"defeated": {"unit": "gorm"}, "by": {"side": "blue"}}, "do": [{"say": "Gorm falls"}]},
        {"id": "clash", "when": {"fight": {"tag": "flier"}, "with": {}}, "repeat": True, "do": [{"say": "Fight!"}]},
        {"id": "chat", "when": {"talk": "eirik", "to": "gorm", "label": "Parley"}, "do": [{"say": "Hello"}, {"endTurn": True}]},
    ]
    m["events"] = events
    m["objectives"] = {"text": "Defeat Gorm", "win": [{"defeated": {"unit": "gorm"}}, {"reach": {"x": 0, "y": 0, "w": 2, "h": 2}, "by": {}}], "lose": [{"defeated": {"unit": "eirik"}}, {"round": 15}]}
    # Order of the map's keys is the order sent (events and objectives come last, after units).
    st, res = put_project(api, pid, d, d["revision"], game=game, maps={"start": m})
    check("events: PUT a full story -> 200", st == 200 and "revision" in res, (st, res))
    check("events: the real loader accepts it (no problems)", problems() == [], problems())
    st, d = get_project(api, pid)
    check("events: GET returns the events exactly", d["maps"]["start"]["events"] == events)
    check("events: GET returns the objectives exactly", d["maps"]["start"]["objectives"] == m["objectives"])
    check("events: unit ids come back", [u.get("id") for u in d["maps"]["start"]["units"]] == ["eirik", "gorm"])
    check("events: switches and variables come back", d["game"]["switches"] == game["switches"] and d["game"]["variables"] == game["variables"])
    check("events: key order of the map is kept", list(d["maps"]["start"].keys())[-2:] == ["events", "objectives"], list(d["maps"]["start"].keys()))
    check("events: key order inside an event is kept", list(d["maps"]["start"]["events"][4].keys()) == ["id", "when", "if", "repeat", "do"])
    check("events: key order of the game is kept (switches, variables last)", list(d["game"].keys())[-2:] == ["switches", "variables"], list(d["game"].keys()))
    text = raw("maps/start.json").decode()
    check("events: written pretty-printed with 2-space indentation", '\n  "events": [\n    {\n      "id": "opening",' in text)
    check("events: no temp file left", not [n for n in os.listdir(os.path.join(games, pid, "maps")) if n.endswith(".ote-tmp")])
    check("events: switches are written to game.json", '"switches": [' in raw("game.json").decode())

    # ---- an absent / null section leaves the story where it is ----
    before = raw("maps/start.json")
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], game=None, maps=None)
    check("events: PUT without maps and game leaves the map file untouched", st == 200 and raw("maps/start.json") == before, (st, res))

    # ---- a story the loader refuses is still saved, reported with the map file and the event ----
    bad = json.loads(json.dumps(d["maps"]["start"]))
    bad["events"] = [{"id": "oops", "when": {"battle": "start"}, "do": [{"setSwitch": "gone", "to": True}], "x-note": "typo"}]
    st, res = put_project(api, pid, d, d["revision"], maps={"start": bad})
    check("events: a broken story is still saved (200)", st == 200, (st, res))
    st, d = get_project(api, pid)
    check("events: unknown keys survive load -> save", d["maps"]["start"]["events"][0].get("x-note") == "typo")
    ps = problems()
    check("events: the loader names the file and the event",
          any(p["file"] == "maps/start.json" and 'event "oops"' in p["message"] for p in ps), ps)
    st, res = put_project(api, pid, d, d["revision"], maps={"start": json.loads(json.dumps(m))})
    check("events: PUT repairs it", st == 200 and problems() == [], problems())

    # ---- emptied lists: the editor drops the keys, and a project without them is the same as before ----
    plain = json.loads(json.dumps(m))
    del plain["events"]
    del plain["objectives"]
    g2 = json.loads(json.dumps(game))
    del g2["switches"]
    del g2["variables"]
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], game=g2, maps={"start": plain})
    st, d = get_project(api, pid)
    check("events: removing the keys removes them from disk",
          st == 200 and "events" not in d["maps"]["start"] and "objectives" not in d["maps"]["start"] and "switches" not in d["game"] and not d["problems"], d["problems"])

    # ---- portraits in graphics.json ----
    sheet = "main"
    g = {"sheets": [{"id": sheet, "file": "art/main.png", "cellWidth": 24, "cellHeight": 24}]}
    g["portraits"] = {"gorm": {"sheet": sheet, "cell": [1, 0]}, "eirik": {"sheet": sheet, "cell": [0, 0], "x-note": 1}}
    g["x-future"] = {"keep": True}
    st, res = put_project(api, pid, d, d["revision"], graphics=g)
    check("portraits: PUT graphics with portraits -> 200", st == 200, (st, res))
    st, d = get_project(api, pid)
    check("portraits: they come back exactly", d["graphics"].get("portraits") == g["portraits"], d["graphics"])
    check("portraits: their order is kept", list(d["graphics"]["portraits"].keys()) == ["gorm", "eirik"])
    check("portraits: unknown keys survive", d["graphics"].get("x-future") == {"keep": True})
    check("portraits: written under a top-level key", '"portraits": {' in raw("data/graphics.json").decode())
    g_no = json.loads(json.dumps(g))
    del g_no["portraits"]
    st, res = put_project(api, pid, d, d["revision"], graphics=g_no)
    st, d = get_project(api, pid)
    check("portraits: removing the key removes it from disk", "portraits" not in d["graphics"] and "portraits" not in raw("data/graphics.json").decode())
