"""Battle scene checks, fifth batch, of the ote-editor API test (docs/BATTLE_SCENE.md "Holding a pose", "Platforms", "Effect timing by distance", "Range bands").
The helper must keep an animation's holdFrame / startFrame / startDelayMs, the "scene" durations, a travel effect's endOffset by distance, atRangeFixed, panAtMs and
game.json's battlePlatformFrom untouched through a load and a save, and the real loader's refusals must reach the Problems list in its own words. The scratch
project is written here. Called from editor_api_test.py; uses its helpers."""
import json
import os


def run_scene5_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "scene5-test"
    dst = os.path.join(games, pid)

    def write(rel, obj):
        path = os.path.join(dst, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8") as f:
            json.dump(obj, f, indent=1)

    def read(rel):
        with open(os.path.join(dst, rel), "r", encoding="utf-8") as f:
            return json.load(f)

    def png(rel, w, h):
        header = bytes([0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0, 0, 0, 13, 0x49, 0x48, 0x44, 0x52]) + w.to_bytes(4, "big") + h.to_bytes(4, "big") + bytes([8, 6])
        path = os.path.join(dst, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(header + bytes(8))

    game = {
        "formatVersion": 1, "name": "Scene five", "view": {"width": 480, "height": 270, "tileSize": 24},
        "stats": [{"id": "hp", "name": "HP"}, {"id": "atk", "name": "Attack"}, {"id": "mov", "name": "Move"}],
        "roles": {"hp": "hp", "move": "mov"}, "moveTypes": [{"id": "foot", "name": "Foot"}], "formulas": {"damage": "a.atk"},
        "battleScene": "scene", "battlePlatformFrom": "own", "maps": [{"id": "arena", "name": "Arena", "file": "maps/arena.json"}]}
    write("game.json", game)
    write("data/terrain.json", {"terrain": [{"id": "plain", "name": "Plain"}]})
    write("data/weapons.json", {"weapons": [{"id": "sword", "group": "blade"}, {"id": "longbow", "group": "bow", "range": [2, 2]}]})
    write("data/items.json", {"items": [{"id": "mend", "name": "Mend Staff", "kind": "staff", "group": "staff", "uses": 3, "range": 2, "target": "ally",
                                          "effects": [{"heal": 5}]}]})
    write("data/units.json", {"units": [
        {"id": "knight", "name": "Knight", "moveType": "foot", "stats": {"hp": 20, "atk": 5, "mov": 4}, "weapons": ["sword"]}]})
    png("art/fighters.png", 192, 32)
    png("art/fx.png", 64, 16)
    anims = {"stand": {"sheet": "fighters", "frames": [[0, 0]]},
             "attack": {"sheet": "fighters", "frames": [[0, 0], [1, 0], [2, 0], [3, 0]], "ms": [200, 200, 200, 100], "hit": 2, "holdFrame": 3},
             "dodge": {"sheet": "fighters", "frames": [[i, 0] for i in range(6)], "ms": 100, "startFrame": 2, "holdFrame": 3, "startDelayMs": 80}}
    effects = [
        {"id": "bolt", "weapon": "longbow", "sheet": "fx", "frames": [[0, 0]], "mode": "travel", "endOffset": -20, "panAtMs": 300,
         "atRange": [{"from": 2, "endOffset": 12}, {"from": 4, "delayMs": 100, "endOffset": -30}]},
        {"id": "glow", "weapon": "mend", "sheet": "fx", "frames": [[0, 0]], "atRange": [{"from": 2, "delayMs": 300}],
         "fade": [{"atMs": 0, "ms": 200, "holdMs": 300, "atRangeFixed": True}], "flash": [{"atMs": 0, "ms": 80, "atRangeFixed": True}]},
        {"id": "wash", "sheet": "fx", "frames": [[1, 0]], "layer": "screen", "atRangeFixed": True}]
    write("data/graphics.json", {"sheets": [{"id": "fighters", "file": "art/fighters.png", "cellWidth": 32, "cellHeight": 32},
                                              {"id": "fx", "file": "art/fx.png", "cellWidth": 16, "cellHeight": 16}],
                                 "battle": {"sets": {"knight": {"animations": anims}}, "assign": [{"set": "knight", "type": "knight"}], "effects": effects,
                                            "scene": {"resultHoldMs": 700, "resultHoldAgainMs": 120}}})
    write("maps/arena.json", {
        "name": "Arena", "grid": {"type": "square", "width": 4, "height": 1}, "legend": {".": "plain"}, "tiles": ["...."],
        "sides": [{"id": "blue", "controller": "human"}, {"id": "red", "controller": "ai"}],
        "units": [{"type": "knight", "id": "hero", "side": "blue", "x": 0, "y": 0, "leader": True}, {"type": "knight", "id": "foe", "side": "red", "x": 2, "y": 0}]})

    def problems():
        st, p = api.json("GET", "/api/projects/%s/problems" % pid)
        return p["problems"] if st == 200 else None

    def says(part, file="data/graphics.json"):
        return any(part in x["message"] and x["file"] == file for x in (problems() or []))

    st, d = get_project(api, pid)
    check("scene5: the project opens in the helper", st == 200, st)
    check("scene5: the real loader accepts every new key (no problems)", problems() == [], problems())
    check("scene5: the animations, effects and scene durations arrive exactly as written",
          d["graphics"]["battle"]["sets"]["knight"]["animations"] == anims and d["graphics"]["battle"]["effects"] == effects and
          d["graphics"]["battle"]["scene"] == {"resultHoldMs": 700, "resultHoldAgainMs": 120})
    check("scene5: game.json keeps battlePlatformFrom", d["game"].get("battlePlatformFrom") == "own")
    before, game_before = read("data/graphics.json"), read("game.json")
    st, res = put_project(api, pid, d, d["revision"], graphics=d["graphics"])
    check("scene5: saving it back is accepted", st == 200, (st, res))
    check("scene5: data/graphics.json and game.json are unchanged by a load and a save", read("data/graphics.json") == before and read("game.json") == game_before)

    def broken(label, part, edit, file="data/graphics.json"):
        st, d = get_project(api, pid)
        edit(d)
        put_project(api, pid, d, d["revision"], graphics=d["graphics"], game=d["game"])
        check("scene5: %s is refused" % label, says(part, file), problems())
        write("data/graphics.json", before)
        write("game.json", game_before)
        check("scene5: and the fixed files load again (%s)" % label, problems() == [], problems())

    an = lambda d: d["graphics"]["battle"]["sets"]["knight"]["animations"]
    fx = lambda d: d["graphics"]["battle"]["effects"]
    broken("a holdFrame outside the frames", "\"holdFrame\" must be a whole number from 0 to 3", lambda d: an(d)["attack"].update({"holdFrame": 9}))
    broken("a holdFrame before the hit frame", "comes before the hit frame 2", lambda d: an(d)["attack"].update({"holdFrame": 1}))
    broken("a startFrame on a strike", "\"startFrame\" is only for \"dodge\"", lambda d: an(d)["attack"].update({"startFrame": 1}))
    broken("a hold on the stand animation", "never waits for a result", lambda d: an(d)["stand"].update({"holdFrame": 0}))
    broken("a startDelayMs that is too long", "\"startDelayMs\" must be a whole number from 0 to 5000", lambda d: an(d)["dodge"].update({"startDelayMs": 6000}))
    broken("a scene duration that is too long", "\"resultHoldMs\" must be a whole number from 0 to 5000", lambda d: d["graphics"]["battle"]["scene"].update({"resultHoldMs": 9000}))
    broken("a hold key on an effect", "only for an animation of a battle set", lambda d: fx(d)[0].update({"holdFrame": 1}))
    broken("an endOffset step with no delay or offset", "needs a \"delayMs\"", lambda d: fx(d)[0]["atRange"][0].pop("endOffset"))
    broken("an endOffset that is too far past the target", "from -400 to 400", lambda d: fx(d)[0].update({"endOffset": -500}))
    broken("an endOffset step on an effect that does not fly", "\"endOffset\" is only for a \"travel\" effect", lambda d: fx(d)[1]["atRange"][0].update({"endOffset": 5}))
    broken("a panAtMs that is too late", "\"panAtMs\" must be a whole number from 0 to 15000", lambda d: fx(d)[0].update({"panAtMs": 20000}))
    broken("atRangeFixed together with atRange", "\"atRangeFixed\" is for an effect that is not delayed by distance", lambda d: fx(d)[1].update({"atRangeFixed": True}))
    broken("a row atRangeFixed that is not true or false", "\"atRangeFixed\"", lambda d: fx(d)[1]["fade"][0].update({"atRangeFixed": "yes"}))
    broken("battlePlatformFrom with another word", "\"battlePlatformFrom\" must be \"same\"", lambda d: d["game"].update({"battlePlatformFrom": "both"}), "game.json")
