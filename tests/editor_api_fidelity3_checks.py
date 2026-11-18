"""Fidelity 3 checks of the ote-editor API test (docs/SRPG.md "An ability that spends its item" / "A promotion level per character" / summon
candidates, docs/ECONOMY.md "Weather that closes a way", docs/EVENTS.md "Several shops", docs/SOUND.md). The helper must keep the new keys untouched
through a load and a save and the real loader's refusals must reach the Problems list in its own words, naming the file. The scratch project is the
committed fixture tests/fixtures/srpg-prep plus the files written here. Called from editor_api_test.py; uses its helpers."""
import json
import os
import shutil


def run_fidelity3_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "fidelity3-test"
    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    dst = os.path.join(games, pid)
    shutil.copytree(os.path.join(repo, "tests", "fixtures", "srpg-prep"), dst)

    def read(rel):
        with open(os.path.join(dst, rel), "r", encoding="utf-8") as f:
            return json.load(f)

    def write(rel, obj):
        path = os.path.join(dst, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8") as f:
            json.dump(obj, f, indent=1)

    # The new keys, written into the fixture's files.
    t = read("data/terrain.json")
    t["terrain"].append({"id": "bog", "name": "Bog", "moveCost": {"foot": 2}, "weatherCost": {"rain": {"foot": "blocked", "wing": 1}}})
    write("data/terrain.json", t)
    items = read("data/items.json")
    items["items"].append({"id": "wand", "name": "Wand", "kind": "accessory", "uses": 3, "skills": ["zapper"]})
    write("data/items.json", items)
    write("data/skills.json", {"skills": [{"id": "zapper", "name": "Zapper", "blocks": [{"when": "always", "do": [{"ability": "zap"}]}]}]})
    write("data/abilities.json", {"abilities": [
        {"id": "zap", "name": "Zap", "target": "ally", "effect": {"heal": 5}, "spendsItem": True},
        {"id": "call", "name": "Call", "target": "cell", "range": [1, 2],
         "effect": {"summon": {"type": "raider", "limit": 3, "items": [{"item": "potion", "minLevel": 1, "maxLevel": 4, "weight": 3}, {"item": None, "minLevel": 2}, {"item": "tonic", "weight": 9}],
                               "pick": "random"}}}]})
    u = read("data/units.json")
    for unit in u["units"]:
        if unit["id"] == "soldier":
            unit["items"] = ["wand"]
            unit["promotions"] = [{"to": "glider", "fromLevel": 5, "fromLevelFor": {"hero1": 2}}]
    write("data/units.json", u)
    write("data/characters.json", {"characters": [{"id": "hero1", "name": "Hero", "type": "soldier", "promotion": {"fromLevel": 3}}]})
    g = read("game.json")
    g["rules"]["progression"] = {"enabled": True, "xpPerLevel": 100, "maxLevel": 20}
    write("game.json", g)
    o = read("maps/orchard.json")
    o["prep"].pop("shop")
    o["prep"]["shops"] = [{"id": "armory", "name": "Armory", "sells": ["tonic"], "music": "armory"}, {"id": "market", "name": "Market", "sells": ["potion", "ring"], "buys": False}]
    o["legend"]["b"] = "bog"
    write("maps/orchard.json", o)
    write("data/audio.json", {
        "music": {"camp": {"file": "audio/camp.ogg"}, "armory": {"file": "audio/armory.ogg"}, "boot": {"file": "audio/boot.ogg", "loop": False}},
        "sounds": {"tick": {"file": "audio/tick.wav"}, "chime": {"file": "audio/chime.wav"}, "boom": {"file": "audio/boom.wav"}},
        "use": {"prepMusic": "camp", "shopMusic": "camp", "bootJingle": "boot", "promotionSounds": [{"atMs": 0, "sound": "boom"}, {"atMs": 400, "sound": "tick"}],
                "sounds": {"levelUpTick": "tick", "save": "chime"}}})

    def problems():
        st, p = api.json("GET", "/api/projects/%s/problems" % pid)
        return p["problems"] if st == 200 else None

    def says(part, file=None):
        return any(part in x["message"] and (file is None or x["file"] == file) for x in (problems() or []))

    st, d = get_project(api, pid)
    check("fidelity3: the project opens in the helper", st == 200, st)
    check("fidelity3: the real loader accepts every new key (no problems; missing audio files are only warnings)", problems() == [], problems())
    check("fidelity3: the ability, the summon candidates, the weather cost, the promotion keys and the shops arrive",
          [a for a in d["abilities"] if a["id"] == "zap"][0].get("spendsItem") is True and
          [a for a in d["abilities"] if a["id"] == "call"][0]["effect"]["summon"]["items"][1] == {"item": None, "minLevel": 2} and
          [x for x in d["terrain"] if x["id"] == "bog"][0]["weatherCost"]["rain"]["foot"] == "blocked" and
          d["characters"][0]["promotion"] == {"fromLevel": 3} and
          [x for x in d["units"] if x["id"] == "soldier"][0]["promotions"][0]["fromLevelFor"] == {"hero1": 2} and
          [s["id"] for s in d["maps"]["orchard"]["prep"]["shops"]] == ["armory", "market"])

    files = ("data/terrain.json", "data/abilities.json", "data/units.json", "data/characters.json", "data/audio.json", "maps/orchard.json", "data/skills.json", "data/items.json")
    before = {rel: read(rel) for rel in files}
    st, res = put_project(api, pid, d, d["revision"], abilities=d["abilities"], skills=d.get("skills"), characters=d.get("characters"), items=d.get("items"), audio=d.get("audio"))
    check("fidelity3: saving it back is accepted", st == 200, (st, res))
    for rel in files:
        check("fidelity3: %s is unchanged by a load and a save" % rel, read(rel) == before[rel], rel)

    def broken(label, part, file, mutate, named=None):
        obj = read(file)
        mutate(obj)
        write(file, obj)
        check("fidelity3: %s is refused" % label, says(part, named or file), problems())  # `named`: the file the loader's message names
        write(file, before[file])
        check("fidelity3: and the fixed file loads again (%s)" % label, problems() == [], problems())

    broken("an ability that spends an item nobody gives", "no item gives this ability", "data/items.json",
           lambda o: [i for i in o["items"] if i["id"] == "wand"][0].update({"skills": [], "bonus": {"def": 1}}), named="data/abilities.json")
    broken("an item that spends but never wears out", "unlimited uses", "data/items.json", lambda o: [i for i in o["items"] if i["id"] == "wand"][0].pop("uses"), named="data/abilities.json")
    broken("a spendsItem that is not on or off", "spendsItem", "data/abilities.json", lambda o: o["abilities"][0].update({"spendsItem": "yes"}))
    broken("a summon candidate weight of 0", "\"weight\" must be a whole number from 1 to 1000", "data/abilities.json",
           lambda o: o["abilities"][1]["effect"]["summon"]["items"][0].update({"weight": 0}))
    broken("a summon candidate ceiling below its floor", "\"maxLevel\" must be a whole number from \"minLevel\"", "data/abilities.json",
           lambda o: o["abilities"][1]["effect"]["summon"]["items"][0].update({"minLevel": 5, "maxLevel": 4}))
    broken("more than 32 summon candidates", "1 to 32 candidates", "data/abilities.json",
           lambda o: o["abilities"][1]["effect"]["summon"].update({"items": [{"item": "potion"}] * 33}))
    broken("a promotion level for a character that does not exist", "unknown character \"nobody\"", "data/units.json",
           lambda o: [x for x in o["units"] if x["id"] == "soldier"][0]["promotions"][0].update({"fromLevelFor": {"nobody": 2}}))
    broken("a character's promotion level of 0", "from 1 to 20", "data/characters.json", lambda o: o["characters"][0].update({"promotion": {"fromLevel": 0}}))
    broken("a weather cost word that is not blocked", "\"blocked\"", "data/terrain.json",
           lambda o: [x for x in o["terrain"] if x["id"] == "bog"][0]["weatherCost"]["rain"].update({"foot": "closed"}))
    broken("both a shop and a list of shops", "not both", "maps/orchard.json", lambda o: o["prep"].update({"shop": {"sells": ["potion"]}}))
    broken("two shops with the same name", "used twice", "maps/orchard.json", lambda o: o["prep"]["shops"][1].update({"name": "Armory"}))
    broken("a shop music that is not in the game's sound", "names unknown music", "maps/orchard.json", lambda o: o["prep"]["shops"][0].update({"music": "nowhere"}))
    broken("a boot jingle that repeats", "must have \"loop\": false", "data/audio.json",
           lambda o: o["music"]["boot"].update({"loop": True}))
    broken("a promotion sound list with an unknown sound", "names unknown sound", "data/audio.json",
           lambda o: o["use"]["promotionSounds"][0].update({"sound": "nowhere"}))
    broken("a shopMusic that is not a track", "names unknown music", "data/audio.json", lambda o: o["use"].update({"shopMusic": "nowhere"}))
    broken("a levelUpTick that is not a sound", "names unknown sound", "data/audio.json", lambda o: o["use"]["sounds"].update({"levelUpTick": "nowhere"}))
