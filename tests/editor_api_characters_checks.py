"""Characters on maps (editor step E1 of docs/SRPG.md, docs/EDITOR_API.md): a map unit placed as a character is
{ "character", "side", "x", "y" } with no "type"; the helper must keep it exactly as written, and GET .../problems
must give the loader's own words for the refusals of docs/SRPG.md section 1.
Called from editor_api_test.py; uses its helpers."""
import copy
import json as _json
import os


def run_characters_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "chars-test"
    st, doc = api.json("POST", "/api/projects", {"id": pid, "name": "Chars", "template": "blank"})
    check("characters: create scratch project", st == 201, (st, doc))
    root = os.path.join(games, pid)
    # The loader stops at its first refusal and reads the art first; the test setup has no image files, so the pictures
    # are dropped to let the map and character refusals come through.
    graphics = os.path.join(root, "data", "graphics.json")
    if os.path.exists(graphics):
        os.remove(graphics)

    def problems():
        st, d = api.json("GET", "/api/projects/%s/problems" % pid)
        # (the scratch game's art images are not copied by this test setup: that problem is not about characters)
        return [p for p in d.get("problems", []) if p.get("file") != "data/graphics.json"] if st == 200 else [{"message": "HTTP %s" % st}]

    def messages():
        return [p["message"] for p in problems()]

    def put_map(units, chars=None):
        st, d = get_project(api, pid)
        maps = copy.deepcopy(d["maps"])
        maps["start"]["units"] = units
        extra = {} if chars is None else {"characters": chars}
        return put_project(api, pid, d, d["revision"], maps=maps, **extra)

    hero = {"id": "hero", "name": "Hero", "type": "soldier", "stats": {"hp": 2}}
    placed = {"character": "hero", "side": "blue", "x": 0, "y": 0}
    plain = {"type": "soldier", "side": "red", "x": 6, "y": 3}

    st, res = put_map([placed, plain], [hero])
    check("characters: PUT a map with a character unit (no type) -> 200", st == 200, (st, res))
    with open(os.path.join(root, "maps", "start.json")) as f:
        on_disk = _json.load(f)
    check("characters: the character unit is written as { character, side, x, y } and nothing is added",
          on_disk["units"][0] == placed and on_disk["units"][1] == plain, on_disk["units"])
    st, d = get_project(api, pid)
    check("characters: GET returns the unit unchanged", d["maps"]["start"]["units"][0] == placed, d["maps"]["start"]["units"])
    check("characters: the game loads without problems", problems() == [], problems())

    # an edit that does not touch the map leaves the character unit byte-identical
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], characters=[dict(hero, name="Hero B")])
    with open(os.path.join(root, "maps", "start.json")) as f:
        check("characters: renaming the character leaves the map's unit as it was", _json.load(f)["units"][0] == placed)

    # the loader's words for the refusals
    put_map([placed, {"character": "ghost", "side": "blue", "x": 1, "y": 1}])
    check("characters: unknown character -> the loader's words",
          any('unknown character "ghost" (data/characters.json)' in m for m in messages()), messages())
    put_map([placed, {"character": "hero", "side": "blue", "x": 2, "y": 2}])
    check("characters: a character twice -> already placed",
          any('"character" "hero" is already placed by units[0]' in m for m in messages()), messages())
    put_map([dict(placed, id="hero")])
    check("characters: a character with an id -> remove it",
          any('is known to events by the character\'s id; remove "id"' in m for m in messages()), messages())
    put_map([placed, dict(plain, id="hero")])
    check("characters: a plain unit may not use a character's id",
          any('"id" "hero" is a character (data/characters.json); write "character": "hero"' in m for m in messages()), messages())

    # the map's own type (a different class on this map only) is accepted and kept
    st, res = put_map([dict(placed, type="soldier")])
    check("characters: a character unit with its own type loads", st == 200 and messages() == [], messages())

    # a character of an unknown unit type
    st, res = put_map([placed], [dict(hero, type="nope")])
    check("characters: unknown unit type -> the loader's words",
          any('character "hero": unknown unit type "nope"' in m for m in messages()), messages())

    # without the file, "character" is refused
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], characters=False)
    check("characters: no characters file and a character unit -> refused",
          any('"character" is given but the game has no characters (data/characters.json)' in m for m in messages()), messages())
