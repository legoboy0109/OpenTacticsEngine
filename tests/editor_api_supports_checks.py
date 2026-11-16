"""E4 checks of the ote-editor API test (docs/SRPG.md "Editor", step E4): what the Supports screen and the story forms write
(data/supports.json with rules, pairs, bonus and conversations for several levels; an addSupport command, a support condition and
trigger in a map event; an addSupport command in a campaign scene) must load in the real game loader: GET .../problems is empty and
the headless simulator plays the map. Also the sticky switch: characters and supports on with nothing in them are the same game as
none, and stay on after a reload. Called from editor_api_test.py; uses its helpers."""
import os
import subprocess


from editor_api_plain_units import plain_units


def run_supports_checks(api, tmp, games, templates, check, get_project, put_project):
    # --- The sticky switch: both features on, nothing added.
    pid = "supports-e4-empty"
    st, doc = api.json("POST", "/api/projects", {"id": pid, "name": "E4 empty", "template": "strategy-rpg"})
    check("e4: create project", st == 201, (st, doc))
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], characters=[], supports={"pairs": []}, maps=plain_units(d))
    check("e4: PUT empty characters and supports -> 200", st == 200, (st, res))
    st, p = api.json("GET", "/api/projects/%s/problems" % pid)
    check("e4: empty characters and supports: Problems is empty", st == 200 and p["problems"] == [], p)
    st, d = get_project(api, pid)
    check("e4: after a reload both stay on", d["characters"] == [] and d["supports"] == {"pairs": []}, (d["characters"], d["supports"]))

    # --- A full set.
    pid = "supports-e4"
    st, doc = api.json("POST", "/api/projects", {"id": pid, "name": "E4", "template": "strategy-rpg"})
    check("e4: create scratch project", st == 201, (st, doc))
    st, d = get_project(api, pid)
    game = d["game"]
    mid = game["maps"][0]["id"]
    maps = plain_units(d)
    characters = [{"id": "eirik", "name": "Eirik", "type": "lord"}, {"id": "lyra", "name": "Lyra", "type": "archer"},
                  {"id": "bram", "name": "Bram", "type": "soldier"}]
    maps[mid]["units"] = [{"character": "eirik", "side": "blue", "x": 1, "y": 3}, {"character": "lyra", "side": "blue", "x": 1, "y": 4}] + \
        [u for u in maps[mid]["units"] if u["side"] != "blue"]
    maps[mid]["events"] = [
        {"id": "bond", "when": {"support": ["eirik", "lyra"], "level": 2}, "do": [{"say": "Closer."}]},
        {"id": "any", "when": {"support": ["lyra", "eirik"]}, "do": [{"say": "Something changed."}]},
        {"id": "gift", "when": {"battle": "start"}, "if": [{"support": ["eirik", "lyra"], "op": ">", "value": 0}],
         "do": [{"addSupport": ["eirik", "lyra"], "by": 5}, {"addSupport": ["eirik", "lyra"], "by": -2}]},
    ]
    supports = {
        "rules": {"levelNames": ["C", "B", "A"], "points": {"endTurnAdjacent": 1, "fightTogether": 2, "heal": 2}, "range": 4,
                  "maxLevels": 4, "conversations": "battle"},
        "pairs": [
            {"a": "eirik", "b": "lyra", "start": 5, "levels": [
                {"points": 20, "bonus": {"hit": 5, "avoid": 5, "damageTaken": -1},
                 "talk": [{"say": "You never miss.", "speaker": "eirik"}, {"say": "I miss plenty.", "speaker": "lyra"}]},
                {"points": 60, "bonus": {"hit": 10, "crit": 5, "damage": 1, "stats": {"def": 1}},
                 "talk": [{"say": "Stay close.", "speaker": "lyra", "portrait": "lyra_calm"}, {"say": "Always.", "speakerName": "Eirik"}]},
                {"points": 100}]},
            {"a": "lyra", "b": "bram", "levels": [{"points": 10}]},
        ],
    }
    campaign = {"chapters": [{"id": "ch1", "name": "The Ford", "map": mid, "before": [{"addSupport": ["lyra", "bram"], "by": 3}]}]}
    st, res = put_project(api, pid, d, d["revision"], game=game, maps=maps, characters=characters, supports=supports, campaign=campaign)
    check("e4: PUT supports, story and scene -> 200", st == 200, (st, res))
    st, p = api.json("GET", "/api/projects/%s/problems" % pid)
    check("e4: the real loader accepts all of it (no problems)", st == 200 and p["problems"] == [], p)
    st, d = get_project(api, pid)
    check("e4: the supports round trip as written", d["supports"] == supports, d["supports"])

    root = os.path.join(games, pid)
    sim = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "build", "ote-sim")
    if os.path.exists(sim):
        r = subprocess.run([sim, root, mid, "--runs", "1"], capture_output=True, timeout=120)
        check("e4: ote-sim plays the map with those files", r.returncode == 0, (r.returncode, r.stderr[-300:]))

    # What the screen refuses to offer is refused by the loader too.
    st, d = get_project(api, pid)
    d["maps"][mid]["events"] = [{"id": "x", "when": {"battle": "start"}, "do": [{"addSupport": ["eirik", "bram"], "by": 1}]}]
    st, res = put_project(api, pid, d, d["revision"], maps=d["maps"])
    st, p = api.json("GET", "/api/projects/%s/problems" % pid)
    check("e4: a command about a pair that is not declared is a problem in the loader's words",
          any("not a support pair" in x["message"] for x in p["problems"]), p)
