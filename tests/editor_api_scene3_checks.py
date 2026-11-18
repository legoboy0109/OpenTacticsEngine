"""Battle scene checks, third batch, of the ote-editor API test (docs/BATTLE_SCENE.md "Staves in the scene", "Blending", "Hiding and fading", "Range bands").
The helper must keep game.json's "battleSceneStaves", an effect's blend / hide / fade / order / speed, a staff item as an effect's weapon and the stage's
"ranges" untouched through a load and a save, and the real loader's refusals must reach the Problems list in its own words. The scratch project is written
here. Called from editor_api_test.py; uses its helpers."""
import json
import os


def run_scene3_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "scene3-test"
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
        "formatVersion": 1, "name": "Scene three", "view": {"width": 480, "height": 270, "tileSize": 24},
        "stats": [{"id": "hp", "name": "HP"}, {"id": "atk", "name": "Attack"}, {"id": "mov", "name": "Move"}],
        "roles": {"hp": "hp", "move": "mov"}, "moveTypes": [{"id": "foot", "name": "Foot"}], "formulas": {"damage": "a.atk"},
        "battleScene": "scene", "battleSceneStaves": True, "maps": [{"id": "arena", "name": "Arena", "file": "maps/arena.json"}]}
    write("game.json", game)
    write("data/terrain.json", {"terrain": [{"id": "plain", "name": "Plain"}]})
    write("data/weapons.json", {"weapons": [{"id": "sword", "group": "blade"}]})
    write("data/items.json", {"items": [
        {"id": "mend", "name": "Mend Staff", "kind": "staff", "group": "staff", "uses": 3, "range": 2, "target": "ally", "effects": [{"heal": 5}]}]})
    write("data/units.json", {"units": [
        {"id": "knight", "name": "Knight", "moveType": "foot", "stats": {"hp": 20, "atk": 5, "mov": 4}, "weapons": ["sword"]}]})
    png("art/fx.png", 64, 16)
    stage = {"width": 240, "height": 160, "feet": {"left": [88, 104], "right": [152, 104]}, "feetFar": {"left": [56, 104], "right": [184, 104]},
             "ranges": [{"from": 3, "feet": {"left": [30, 110], "right": [210, 110]}},
                        {"from": 5, "feet": {"left": [40, 100], "right": [400, 100]}, "platform": {"left": [8, 92], "right": [368, 92]}, "view": "each",
                         "pan": {"panMs": 250, "leftView": 0, "rightView": 200}}]}
    effects = [
        {"id": "glow", "weapon": "mend", "sheet": "fx", "frames": [[0, 0], [1, 0]], "blend": "add", "hitAtMs": 200,
         "hide": [{"atMs": 0, "ms": 900, "what": ["backdrop", "platforms"], "color": "#102030"}],
         "fade": [{"atMs": 0, "ms": 200, "to": "#000000", "alpha": 0.6, "holdMs": 300, "over": "backdrop"}]},
        {"id": "wash", "sheet": "fx", "frames": [[0, 0]], "layer": "screen", "order": "front"},
        {"id": "arrow", "group": "blade", "sheet": "fx", "frames": [[2, 0]], "mode": "travel", "speed": 240}]
    write("data/graphics.json", {"sheets": [{"id": "fx", "file": "art/fx.png", "cellWidth": 16, "cellHeight": 16}], "battle": {"effects": effects, "stage": stage}})
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
    check("scene3: the project opens in the helper", st == 200, st)
    check("scene3: the real loader accepts every new key (no problems)", problems() == [], problems())
    check("scene3: the stage's range bands and the effects arrive exactly as written", d["graphics"]["battle"]["stage"] == stage and d["graphics"]["battle"]["effects"] == effects)
    check("scene3: game.json keeps battleSceneStaves", d["game"].get("battleSceneStaves") is True)
    before, game_before = read("data/graphics.json"), read("game.json")
    st, res = put_project(api, pid, d, d["revision"], graphics=d["graphics"])
    check("scene3: saving it back is accepted", st == 200, (st, res))
    check("scene3: data/graphics.json and game.json are unchanged by a load and a save", read("data/graphics.json") == before and read("game.json") == game_before)

    def broken(label, part, edit, file="data/graphics.json"):
        st, d = get_project(api, pid)
        edit(d)
        put_project(api, pid, d, d["revision"], graphics=d["graphics"], game=d["game"])
        check("scene3: %s is refused" % label, says(part, file), problems())
        write("data/graphics.json", before)
        write("game.json", game_before)
        check("scene3: and the fixed files load again (%s)" % label, problems() == [], problems())

    fx = lambda d: d["graphics"]["battle"]["effects"]
    sg = lambda d: d["graphics"]["battle"]["stage"]
    broken("battleSceneStaves that is not true or false", "battleSceneStaves", lambda d: d["game"].update({"battleSceneStaves": "yes"}), "game.json")
    broken("a blend that is not normal or add", "\"blend\" must be \"normal\"", lambda d: fx(d)[0].update({"blend": "multiply"}))
    broken("an order on an effect that is not a screen effect", "\"order\" is only for \"layer\": \"screen\"", lambda d: fx(d)[0].update({"order": "front"}))
    broken("a speed together with travelMs", "not both", lambda d: fx(d)[2].update({"travelMs": 300}))
    broken("a speed on an effect that does not fly", "\"speed\" is only for a \"travel\" effect", lambda d: fx(d)[0].update({"speed": 200}))
    broken("a speed with no fixed stage", "needs a fixed \"stage\"", lambda d: d["graphics"]["battle"].pop("stage"))
    broken("a hide that names the fighters", "can name \"backdrop\" and \"platforms\" only", lambda d: fx(d)[0]["hide"][0].update({"what": ["fighters"]}))
    broken("a fade that is not strong enough to see", "\"alpha\" must be above 0", lambda d: fx(d)[0]["fade"][0].update({"alpha": 0}))
    broken("a fade over something that is not there", "\"over\" must be \"all\"", lambda d: fx(d)[0]["fade"][0].update({"over": "fighters"}))
    broken("an effect for a staff that does not exist", "there is no weapon with the id \"ghost\"", lambda d: fx(d)[0].update({"weapon": "ghost"}))
    broken("a band that starts at 1", "\"from\" must be between 2 and 99", lambda d: sg(d)["ranges"][0].update({"from": 1}))
    broken("two bands that start at the same distance", "a second band starts at 3", lambda d: sg(d)["ranges"][1].update({"from": 3}))
    broken("a panning band without a pan", "needs a \"pan\"", lambda d: sg(d)["ranges"][1].pop("pan"))
    broken("a pan that does not show a fighter", "is not inside the picture while the camera is on it", lambda d: sg(d)["ranges"][1]["pan"].update({"rightView": 100}))
    broken("a band that shows both but has a pan", "\"pan\" belongs to \"view\": \"each\"", lambda d: sg(d)["ranges"][0].update({"pan": {"leftView": 0, "rightView": 0}}))
    broken("a band with a foot point outside the stage", "is outside the stage", lambda d: sg(d)["ranges"][0]["feet"].update({"right": [300, 10]}))
