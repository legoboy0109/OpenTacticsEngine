"""Battle scene effects checks of the ote-editor API test (docs/BATTLE_SCENE.md "Effects", docs/EDITOR_UI.md "Art"). The helper must keep every
effect key untouched through a load and a save (sounds, hitAtMs, layer, mirror, flash, shake, at, returns, lingerMs, a frame from another image,
a list of effects, byType, an entry that only assigns) and the real loader's refusals must reach the Problems list in its own words, naming
data/graphics.json. The scratch project is written here. Called from editor_api_test.py; uses its helpers."""
import json
import os


def run_battle_effects_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "battlefx-test"
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

    write("game.json", {
        "formatVersion": 1, "name": "Battle effects", "view": {"width": 480, "height": 270, "tileSize": 24},
        "stats": [{"id": "hp", "name": "HP"}, {"id": "atk", "name": "Attack"}, {"id": "mov", "name": "Move"}],
        "roles": {"hp": "hp", "move": "mov"}, "moveTypes": [{"id": "foot", "name": "Foot"}], "formulas": {"damage": "a.atk"},
        "battleScene": "scene", "maps": [{"id": "arena", "name": "Arena", "file": "maps/arena.json"}]})
    write("data/terrain.json", {"terrain": [{"id": "plain", "name": "Plain"}]})
    write("data/weapons.json", {"weapons": [{"id": "sword", "group": "blade"}, {"id": "tome", "group": "magic", "range": [1, 2]}]})
    write("data/units.json", {"units": [
        {"id": "knight", "name": "Knight", "moveType": "foot", "stats": {"hp": 20, "atk": 5, "mov": 4}, "weapons": ["sword"]},
        {"id": "mage", "name": "Mage", "moveType": "foot", "stats": {"hp": 12, "atk": 6, "mov": 4}, "weapons": ["tome"]}]})
    png("art/fx.png", 64, 16)
    png("art/fx2.png", 64, 16)
    effects = [
        {"id": "charge", "sheet": "fx", "frames": [[0, 0], [1, 0, "fx2"], [2, 0]], "ms": 90, "at": "striker", "hitAtMs": 300,
         "sounds": {"0": "zap", "300": "boom"}, "flash": [{"atMs": 100, "ms": 80, "color": "#ff8000"}], "shake": [{"atMs": 100, "ms": 200, "strength": 3}]},
        {"id": "wash", "sheet": "fx", "frames": [[0, 0]], "layer": "screen", "mirror": True},
        {"id": "bolt", "sheet": "fx", "frames": [[3, 0]], "mode": "travel", "travelMs": 300, "returns": True, "lingerMs": 120, "layer": "behind"},
        {"weapon": "tome", "effects": ["charge", "bolt"], "byType": {"knight": "wash", "mage": []}},
        {"group": "blade", "sheet": "fx", "frames": [[0, 0]], "effects": "wash"}]
    write("data/graphics.json", {
        "sheets": [{"id": "fx", "file": "art/fx.png", "cellWidth": 16, "cellHeight": 16}, {"id": "fx2", "file": "art/fx2.png", "cellWidth": 16, "cellHeight": 16}],
        "battle": {"effects": effects}})
    import wave
    os.makedirs(os.path.join(dst, "audio"), exist_ok=True)
    for name in ("zap", "boom"):  # two tiny generated sounds (effect sounds are checked against the game's sounds)
        with wave.open(os.path.join(dst, "audio", name + ".wav"), "wb") as w:
            w.setnchannels(1)
            w.setsampwidth(2)
            w.setframerate(8000)
            w.writeframes(bytes(400))
    write("data/audio.json", {"sounds": {"zap": {"file": "audio/zap.wav"}, "boom": {"file": "audio/boom.wav"}}})
    write("maps/arena.json", {
        "name": "Arena", "grid": {"type": "square", "width": 4, "height": 1}, "legend": {".": "plain"}, "tiles": ["...."],
        "sides": [{"id": "blue", "controller": "human"}, {"id": "red", "controller": "ai"}],
        "units": [{"type": "knight", "id": "hero", "side": "blue", "x": 0, "y": 0, "leader": True}, {"type": "mage", "id": "foe", "side": "red", "x": 2, "y": 0}]})

    def problems():
        st, p = api.json("GET", "/api/projects/%s/problems" % pid)
        return p["problems"] if st == 200 else None

    def says(part):
        return any(part in x["message"] and x["file"] == "data/graphics.json" for x in (problems() or []))

    st, d = get_project(api, pid)
    check("battlefx: the project opens in the helper", st == 200, st)
    check("battlefx: the real loader accepts every new key (no problems)", problems() == [], problems())
    check("battlefx: the effects arrive exactly as written", d["graphics"]["battle"]["effects"] == effects)
    before = read("data/graphics.json")
    st, res = put_project(api, pid, d, d["revision"], graphics=d["graphics"])
    check("battlefx: saving it back is accepted", st == 200, (st, res))
    check("battlefx: data/graphics.json is unchanged by a load and a save", read("data/graphics.json") == before)

    def broken(label, part, edit):
        st, d = get_project(api, pid)
        edit(d["graphics"]["battle"]["effects"])
        put_project(api, pid, d, d["revision"], graphics=d["graphics"])
        check("battlefx: %s is refused" % label, says(part), problems())
        write("data/graphics.json", before)
        check("battlefx: and the fixed file loads again (%s)" % label, problems() == [], problems())

    broken("a hit time on a flying effect", "\"hitAtMs\" is for an effect that appears", lambda e: e[2].update({"hitAtMs": 100}))
    broken("a layer that is not one", "\"layer\" must be \"front\", \"behind\" or \"screen\"", lambda e: e[2].update({"layer": "above"}))
    broken("a flying screen effect", "cannot be a \"travel\" effect", lambda e: e[2].update({"layer": "screen"}))
    broken("mirror off the screen layer", "\"mirror\" is only for \"layer\": \"screen\"", lambda e: e[0].update({"mirror": True}))
    broken("a charge that flies", "not a \"travel\" effect", lambda e: e[2].update({"at": "striker"}))
    broken("returns on an effect that does not fly", "\"returns\" is only for a \"travel\" effect", lambda e: e[0].update({"returns": True}))
    broken("lingering on an effect that does not fly", "\"lingerMs\" is only for a \"travel\" effect", lambda e: e[0].update({"lingerMs": 100}))
    broken("a flash with a bad colour", "must be a colour such as", lambda e: e[0]["flash"][0].update({"color": "orange"}))
    broken("a shake that is too strong", "\"strength\" must be between 1 and 64", lambda e: e[0]["shake"][0].update({"strength": 99}))
    broken("a sound key that is not a time", "must be the milliseconds after the effect starts", lambda e: e[0]["sounds"].update({"soon": "zap"}))
    broken("a frame from an image the game does not have", "names unknown sheet \"nowhere\"", lambda e: e[0]["frames"].__setitem__(1, [1, 0, "nowhere"]))
    broken("a list naming an effect that does not exist", "there is no effect with the id \"ghost\"", lambda e: e[3].update({"effects": ["charge", "ghost"]}))
    broken("a by-type choice for a unit type that does not exist", "there is no unit type with the id \"wizard\"", lambda e: e[3]["byType"].update({"wizard": "wash"}))
    broken("a list on an effect that has only a name", "belong to an entry with a \"weapon\" or a \"group\"", lambda e: e[1].update({"effects": ["charge"]}))
    broken("two effects with one name", "a second effect with the id \"wash\"", lambda e: e[2].update({"id": "wash"}))
