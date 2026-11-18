"""Scripted-style computer player checks of the ote-editor API test (docs/AI.md "Scripted-style computer players"). The helper must keep game.json's
"rules.ai" and every new behaviour key through a load and a save untouched, and the real loader (the helper's "problems") must accept the shapes the editor
writes and refuse the mistakes in its own words. The scratch project is the committed fixture tests/fixtures/ai-scripted (map "field"). Called from
editor_api_test.py; uses its helpers."""
import copy
import json
import os
import shutil


def run_scripted_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "scripted-test"
    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    shutil.copytree(os.path.join(repo, "tests", "fixtures", "ai-scripted"), os.path.join(games, pid))
    dst = os.path.join(games, pid)

    def read(rel):
        with open(os.path.join(dst, rel), "r", encoding="utf-8") as f:
            return json.load(f)

    def problems():
        st, p = api.json("GET", "/api/projects/%s/problems" % pid)
        return p["problems"] if st == 200 else [{"message": "HTTP %s" % st}]

    def save(mutate_game=None, mutate_map=None):
        st, d = get_project(api, pid)
        game = copy.deepcopy(d["game"])
        maps = copy.deepcopy(d["maps"])
        if mutate_game:
            mutate_game(game)
        if mutate_map:
            mutate_map(maps["field"])
        return put_project(api, pid, d, d["revision"], game=game, maps=maps)

    st, d = get_project(api, pid)
    check("scripted: the fixture opens and loads without problems", st == 200 and problems() == [], problems())
    before_game, before_map = read("game.json"), read("maps/field.json")
    st, res = put_project(api, pid, d, d["revision"], game=d["game"], maps=d["maps"])
    check("scripted: a load and a save changes nothing", st == 200 and read("game.json") == before_game and read("maps/field.json") == before_map, (st, res))
    st, d = get_project(api, pid)
    check("scripted: GET carries rules.ai and the behaviour keys",
          d["game"]["rules"]["ai"]["turnOrder"] == "order" and "classic" in d["game"]["rules"]["ai"]["weights"] and
          d["maps"]["field"]["units"][3]["behaviour"]["route"][1] == {"x": 5, "y": 5}, d["game"]["rules"].get("ai"))

    def restore():
        st, d = get_project(api, pid)
        put_project(api, pid, d, d["revision"], game=json.loads(json.dumps(before_game)), maps={"field": json.loads(json.dumps(before_map))})

    # Mistakes are refused by the loader in its words (the editor's own checks say them earlier).
    def ai(**kw):
        return lambda g: g["rules"]["ai"].update(kw)

    def beh(unit, value):
        return lambda m: m["units"][unit].update(behaviour=value)

    game_cases = [
        ("a bad turn order", ai(turnOrder="fast"), "\"turnOrder\" must be \"best\""),
        ("an unknown rules.ai key", ai(speed=1), "unknown key \"speed\""),
        ("a weight out of range", ai(weights={"w": {"dmg": 101}}), "\"dmg\" must be a whole number from 0 to 100"),
        ("a priority of a unit type that is not there", ai(weights={"w": {"priority": {"dragon": 1}}}), "unknown unit type \"dragon\""),
    ]
    for what, mutate, words in game_cases:
        save(mutate_game=mutate)
        check("scripted: %s is refused in plain words" % what, any(words in x["message"] for x in problems()), problems())
        restore()
    map_cases = [
        ("an order past the bound", beh(0, {"order": 100001}), "from -100000 to 100000"),
        ("a weight set that is not there", beh(0, {"weights": "ghost"}), "\"weights\" names \"ghost\""),
        ("a bad caution", beh(0, {"caution": "reckless"}), "\"caution\" must be \"normal\""),
        ("a retreat whose until is not above below", beh(0, {"retreat": {"below": 50, "until": 50}}), "must be more than"),
        ("a retreat to a terrain that is not there", beh(0, {"retreat": {"below": 30, "to": {"terrain": ["lava"]}}}), "unknown terrain \"lava\""),
        ("idleTurns past the bound", beh(0, {"idleTurns": 100}), "from 1 to 99"),
        ("a patrol without a route", beh(0, {"mode": "patrol"}), "needs a \"route\""),
        ("a route square outside the map", beh(0, {"mode": "patrol", "route": [{"x": 1, "y": 1}, {"x": 40, "y": 1}]}), "outside the map"),
        ("ignores naming a unit that is not there", beh(0, {"ignores": ["ghost"]}), "unknown unit \"ghost\""),
        ("measure without a guardArea", beh(0, {"measure": "moveCost"}), "\"measure\" goes with mode \"guardArea\""),
    ]
    for what, mutate, words in map_cases:
        save(mutate_map=mutate)
        check("scripted: %s is refused in plain words" % what, any(words in x["message"] for x in problems()), problems())
        restore()
    check("scripted: and the good game is accepted again", problems() == [], problems())
