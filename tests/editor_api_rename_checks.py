"""Renaming a character's id: the loader checks of the ote-editor API test (docs/EDITOR_UI.md "Renaming a character").
The editor's rename (editor/web/src/model/characterRename.ts) is unit tested in tests/unit/characterRename.test.ts on the committed game
tests/fixtures/srpg-rename, where the character `hero` is named in every kind of place a loader accepts a character id; the unit test proves
the rename gives exactly what replacing the quoted id `"hero"` by `"captain"` in the game's files gives. This file proves the other half with
the real loader: the game loads without problems, the game with that replacement loads without problems too (so the result of a rename is a
game the engine accepts, every reference having moved together), a character renamed to an id some unit already has is refused by the loader
in the words the editor repeats, and the helper keeps every key through a load and a save of the renamed game. Called from editor_api_test.py."""
import json
import os
import shutil

OLD, NEW = "hero", "captain"


def run_rename_checks(api, tmp, games, templates, check, get_project, put_project):
    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    src = os.path.join(repo, "tests", "fixtures", "srpg-rename")
    before, after = "rename-before", "rename-after"
    shutil.copytree(src, os.path.join(games, before))
    # The same game with the quoted id replaced in every file (comments and names do not contain it).
    shutil.copytree(src, os.path.join(games, after))
    for root, _dirs, files in os.walk(os.path.join(games, after)):
        for f in files:
            if f.endswith(".json"):
                path = os.path.join(root, f)
                with open(path, "r", encoding="utf-8") as h:
                    text = h.read()
                with open(path, "w", encoding="utf-8") as h:
                    h.write(text.replace('"%s"' % OLD, '"%s"' % NEW))

    st, p = api.json("GET", "/api/projects/%s/problems" % before)
    check("rename: the real loader accepts the fixture game", st == 200 and p["problems"] == [], p)
    st, p = api.json("GET", "/api/projects/%s/problems" % after)
    check("rename: ... and the game with the character's id replaced everywhere", st == 200 and p["problems"] == [], p)

    st, d = get_project(api, after)
    dump = json.dumps(d)
    check("rename: the renamed game holds no old id anywhere the helper reads (characters, maps, supports, campaign, art, weapons)",
          st == 200 and dump.count('"%s"' % OLD) == 0, dump.count('"%s"' % OLD))
    check("rename: ... and has the new id in many places", dump.count('"%s"' % NEW) > 60, dump.count('"%s"' % NEW))
    check("rename: the character list carries the new id (and keeps the person's name)", [c["id"] for c in d["characters"]][0] == NEW and d["characters"][0]["name"] == "Aldric")

    # The helper keeps every key through a save of the renamed game.
    overrides = {k: d[k] for k in ("characters", "supports", "campaign", "graphics", "items", "structures", "skills", "conditions") if d.get(k) is not None}
    st, res = put_project(api, after, d, d["revision"], **overrides)
    check("rename: saving the renamed game back is accepted", st == 200, (st, res))
    st, p = api.json("GET", "/api/projects/%s/problems" % after)
    check("rename: ... and it still has no problems", st == 200 and p["problems"] == [], p)

    # The rule the editor repeats when it refuses an id: a unit's story name and a character's id share one namespace.
    st, d = get_project(api, before)
    maps = json.loads(json.dumps(d["maps"]))
    for u in maps["field"]["units"]:
        if u.get("id") == "g1":
            u["id"] = "ally"   # a character's id
    put_project(api, before, d, d["revision"], maps=maps)
    st, p = api.json("GET", "/api/projects/%s/problems" % before)
    messages = [x["message"] for x in p["problems"]]
    check("rename: a unit story name that is a character's id is a loader problem", st == 200 and any('is a character' in m for m in messages), messages)
