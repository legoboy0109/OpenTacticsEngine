"""Sound data and audio files through the ote-editor API (docs/SOUND.md, docs/EDITOR_API.md "Audio files").
Covers: data/audio.json as `audio` (one object like `campaign`; missing = null; `false` removes the file), every sound
key a game may carry (weapons / units / items `sounds`, a map's `music` and turn musics, a campaign scene as
`{ "music", "commands" }`, the story commands) surviving open + save byte for byte, the audio file endpoints (list with
length / rate / channels, upload with name rules and content sniffing, delete, content types), and what /problems says
(the real PLAYER loader runs there). Called from editor_api_test.py; uses its helpers. Builds every game it needs."""
import json
import os
import shutil
import struct
import wave

HERE = os.path.dirname(os.path.abspath(__file__))
OGG_FIXTURE = os.path.join(HERE, "fixtures", "audio", "tone_mono_22k.ogg")


def make_wav(seconds=0.5, rate=22050, channels=1):
    """A tiny silent-ish 16-bit PCM WAV (a few non-zero samples so it is not all zeros)."""
    import io
    buf = io.BytesIO()
    w = wave.open(buf, "wb")
    w.setnchannels(channels)
    w.setsampwidth(2)
    w.setframerate(rate)
    frames = int(seconds * rate)
    w.writeframes(b"".join(struct.pack("<h", (i % 50) * 40) * channels for i in range(frames)))
    w.close()
    return buf.getvalue()


def run_sound_checks(api, tmp, games, templates, check, get_project, put_project):
    pid = "sound-test"
    st, doc = api.json("POST", "/api/projects", {"id": pid, "name": "Sound", "template": "blank"})
    check("sound: create scratch project", st == 201, (st, doc))
    root = os.path.join(games, pid)
    # Whatever the template ships (it may get starter audio), start from a silent game with no pictures to look for.
    shutil.rmtree(os.path.join(root, "audio"), ignore_errors=True)
    for rel in ("data/audio.json", "data/graphics.json", "data/items.json", "data/campaign.json"):
        if os.path.exists(os.path.join(root, rel)):
            os.remove(os.path.join(root, rel))
    base = "/api/projects/%s/audio" % pid

    def raw(rel):
        with open(os.path.join(root, rel), "rb") as f:
            return f.read()

    def problems():
        st, res = api.json("GET", "/api/projects/%s/problems" % pid)
        return res["problems"] if st == 200 else [{"file": "?", "message": str(res)}]

    def copy(x):
        return json.loads(json.dumps(x))

    def put_file(name, data, overwrite=False):
        return api.call("PUT", base + "/" + name + ("?overwrite=1" if overwrite else ""), raw_body=data)

    def jbody(data):
        try:
            return json.loads(data.decode("utf-8")) if data else {}
        except ValueError:
            return {}

    wav = make_wav(0.5)
    with open(OGG_FIXTURE, "rb") as f:
        ogg = f.read()

    # ---- absent: null, no problem; the endpoints on a game with no audio/ folder ----
    st, d = get_project(api, pid)
    check("sound: absent audio.json -> null and no problem",
          st == 200 and "audio" in d and d["audio"] is None and not any(p["file"] == "data/audio.json" for p in d["problems"]), d.get("audio"))
    st, lst = api.json("GET", base)
    check("sound files: empty list without an audio folder", st == 200 and lst == {"audio": []}, (st, lst))
    st, res = put_project(api, pid, d, d["revision"], audio=None)
    check("sound: PUT null does not create the file", st == 200 and not os.path.exists(os.path.join(root, "data/audio.json")), res)
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], audio=False)
    check("sound: PUT false on a missing file -> 200", st == 200 and not os.path.exists(os.path.join(root, "data/audio.json")), (st, res))
    st, d = get_project(api, pid)
    for label, value in (("a list", []), ("a string", "x"), ("true", True), ("a number", 3)):
        st, res = put_project(api, pid, d, d["revision"], audio=value)
        check("sound: PUT %s -> 400" % label, st == 400 and "error" in res, (st, res))

    # ---- upload ----
    st, _, data = put_file("hit.wav", wav)
    up = jbody(data)
    check("sound upload wav -> 201 with file, size, length, rate, channels and revision",
          st == 201 and up.get("file") == "audio/hit.wav" and up.get("size") == len(wav) and up.get("format") == "wav"
          and up.get("rate") == 22050 and up.get("channels") == 1 and abs(up.get("seconds", 0) - 0.5) < 0.01
          and up.get("revision"), (st, up))
    check("sound upload: bytes on disk identical", raw("audio/hit.wav") == wav)
    check("sound upload: no temp file left", not [n for n in os.listdir(os.path.join(root, "audio")) if n.endswith(".ote-tmp")])
    st, _, data = put_file("title.ogg", ogg)
    up = jbody(data)
    check("sound upload ogg -> 201 with rate, channels and a length from the last page",
          st == 201 and up.get("format") == "ogg" and up.get("rate") == 22050 and up.get("channels") == 1
          and 0.05 < up.get("seconds", 0) < 20, (st, up))
    st, _, data = put_file("hit.wav", wav)
    check("sound upload over an existing file -> 409", st == 409, (st, data))
    wav2 = make_wav(0.25)
    st, _, data = put_file("hit.wav", wav2, overwrite=True)
    check("sound upload ?overwrite=1 replaces it", st == 201 and raw("audio/hit.wav") == wav2, st)
    st, _, _ = put_file("hit.wav", wav, overwrite=True)

    for bad in ("Hit.wav", "a%20b.wav", "x.mp3", "x.png", "-x.wav", ("a" * 41) + ".wav", "x", "x.WAV", ".wav", "x.wav.ogg"):
        st, _, data = put_file(bad, wav)
        check("sound upload bad name %r -> 400" % bad, st == 400 and b"error" in data, (st, data))
    st, _, data = put_file("..%2Fescape.wav", wav)
    check("sound upload path trick -> 400/404, nothing outside",
          st in (400, 404) and not os.path.exists(os.path.join(root, "escape.wav")), st)

    st, _, data = put_file("notes.wav", b"this is just a text file, not a sound at all")
    check("sound upload a text file named .wav -> 400, nothing written",
          st == 400 and b"not a sound file" in data and not os.path.exists(os.path.join(root, "audio", "notes.wav")), (st, data))
    st, _, data = put_file("fake.ogg", wav)
    check("sound upload a WAV named .ogg -> 400 saying what it is",
          st == 400 and b"a WAV" in data and not os.path.exists(os.path.join(root, "audio", "fake.ogg")), (st, data))
    st, _, data = put_file("fake.wav", ogg)
    check("sound upload an Ogg named .wav -> 400", st == 400 and b"an Ogg" in data, (st, data))
    st, _, data = put_file("empty.wav", b"")
    check("sound upload empty body -> 400", st == 400, (st, data))
    big = b"RIFF" + b"\0\0\0\0" + b"WAVE" + b"\0" * (8 * 1024 * 1024)
    st, _, data = put_file("big.wav", big)
    check("sound upload over 8 MB -> 413 and nothing written",
          st == 413 and not os.path.exists(os.path.join(root, "audio", "big.wav")), (st, data))
    # exactly at the limit is fine
    edge = b"RIFF" + b"\0\0\0\0" + b"WAVE" + b"\0" * (8 * 1024 * 1024 - 12)
    st, _, data = put_file("edge.wav", edge)
    check("sound upload of exactly 8 MB is accepted", st == 201, (st, data[:100]))
    api.call("DELETE", base + "/edge.wav")

    # ---- list ----
    put_file("step1.wav", make_wav(0.1, 44100, 2))
    st, lst = api.json("GET", base)
    names = [f["file"] for f in lst.get("audio", [])]
    check("sound files list: sorted, plain files only", st == 200 and names == ["audio/hit.wav", "audio/step1.wav", "audio/title.ogg"], names)
    s1 = next((f for f in lst["audio"] if f["file"] == "audio/step1.wav"), {})
    check("sound files list: stereo 44.1 kHz facts", s1.get("rate") == 44100 and s1.get("channels") == 2 and abs(s1.get("seconds", 0) - 0.1) < 0.01, s1)
    check("sound files list: nothing uses them yet", all(f["usedBy"] == [] for f in lst["audio"]), lst)
    # a file the header cannot be read from is still listed, with an error
    with open(os.path.join(root, "audio", "broken.wav"), "wb") as f:
        f.write(b"RIFF\0\0\0\0WAVE" + b"junk")
    with open(os.path.join(root, "audio", "Upper.wav"), "wb") as f:
        f.write(wav)
    st, lst = api.json("GET", base)
    br = next((f for f in lst["audio"] if f["file"] == "audio/broken.wav"), {})
    check("sound files list: a damaged header gives an error text, no length", "error" in br and "seconds" not in br, br)
    check("sound files list: a file with a name the editor would not accept is not listed",
          "audio/Upper.wav" not in [f["file"] for f in lst["audio"]])
    os.remove(os.path.join(root, "audio", "broken.wav"))
    os.remove(os.path.join(root, "audio", "Upper.wav"))

    # ---- serving for preview ----
    st, hdrs, data = api.call("GET", "/api/projects/%s/file/audio/hit.wav" % pid)
    check("GET file audio/hit.wav: audio/wav and the same bytes", st == 200 and hdrs.get("content-type", "").startswith("audio/wav") and data == wav, (st, hdrs.get("content-type")))
    st, hdrs, data = api.call("GET", "/api/projects/%s/file/audio/title.ogg" % pid)
    check("GET file audio/title.ogg: audio/ogg and the same bytes", st == 200 and hdrs.get("content-type", "").startswith("audio/ogg") and data == ogg, (st, hdrs.get("content-type")))

    # ---- the whole game with every sound key: valid for the player's loader, and open + save changes no byte ----
    put_file("step2.wav", make_wav(0.1))
    put_file("win.wav", make_wav(0.3))
    st, d = get_project(api, pid)
    mid = d["game"]["maps"][0]["id"]
    mfile = d["game"]["maps"][0]["file"]
    audio = {
        "music": {
            "title": {"file": "audio/title.ogg", "loopStart": 0.5},
            "battle": {"file": "audio/title.ogg", "volume": 80, "loop": True},
            "win": {"file": "audio/win.wav", "loop": False},
        },
        "sounds": {
            "hit": {"file": "audio/hit.wav"},
            "step": {"files": ["audio/step1.wav", "audio/step2.wav"], "volume": 60},
        },
        "use": {
            "titleMusic": "title", "mapMusic": "battle", "playerTurnMusic": "", "sceneMusic": "title", "victory": "win",
            "sounds": {"cursor": "hit", "step": "step", "hit": "hit", "select": "hit", "ambush": "hit"},
        },
    }
    weapons = copy(d["weapons"])
    weapons[0]["sounds"] = {"hit": "hit", "miss": "", "critical": "hit"}
    units = copy(d["units"])
    units[0]["sounds"] = {"step": "step", "hit": "hit", "miss": "hit", "critical": "hit", "defeated": "", "select": "hit", "heal": "hit"}
    items = [{"id": "potion", "name": "Potion", "kind": "usable", "heal": 5, "sounds": {"use": "hit"}}]
    m = copy(d["maps"][mid])
    m["music"] = "battle"
    m["playerTurnMusic"] = ""
    m["enemyTurnMusic"] = "title"
    m["events"] = [{"id": "intro", "when": {"battle": "start"},
                    "do": [{"playMusic": "title", "fade": 1.5}, {"playSound": "hit"}, {"stopMusic": True, "fade": 0.5},
                           {"if": [{"round": {"op": ">=", "value": 1}}], "then": [{"playMusic": "battle"}]}]}]
    campaign = {"name": "Story", "chapters": [
        {"id": "c1", "map": mid, "before": {"music": "title", "commands": [{"say": "Hi", "speakerName": "N"}]},
         "after": [{"playSound": "hit"}, {"stopMusic": True}], "next": "c2"},
        {"id": "c2", "map": mid, "before": {"music": "battle"}}]}
    st, res = put_project(api, pid, d, d["revision"], audio=audio, weapons=weapons, units=units, items=items, maps={mid: m}, campaign=campaign)
    check("sound: PUT of every sound key -> 200", st == 200 and "revision" in res, (st, res))
    check("sound: the player's loader (run by /problems) accepts all of it", problems() == [], problems())
    check("sound: audio.json is written pretty-printed, keys in the order received",
          raw("data/audio.json").decode().startswith('{\n  "music": {\n    "title": {\n      "file": "audio/title.ogg",\n      "loopStart": 0.5')
          and list(json.loads(raw("data/audio.json")).keys()) == ["music", "sounds", "use"] and raw("data/audio.json").endswith(b"}\n"))
    st, d = get_project(api, pid)
    check("sound: GET returns audio as written", d["audio"] == audio, d["audio"])
    check("sound: GET returns the overrides, the map keys and the scene object form unchanged",
          d["weapons"] == weapons and d["units"] == units and d["items"] == items and d["maps"][mid] == m and d["campaign"] == campaign, "mismatch")

    files = ["data/audio.json", "data/weapons.json", "data/units.json", "data/items.json", mfile, "data/campaign.json"]
    audio_files = ["audio/hit.wav", "audio/step1.wav", "audio/step2.wav", "audio/win.wav", "audio/title.ogg"]
    before = {f: raw(f) for f in files + audio_files}
    st, res = put_project(api, pid, d, d["revision"], audio=d["audio"], weapons=d["weapons"], units=d["units"], items=d["items"],
                          maps=d["maps"], campaign=d["campaign"], structures=d["structures"], research=d["research"])
    after = {f: raw(f) for f in files + audio_files}
    check("sound: open + save keeps every file and every audio file byte for byte", st == 200 and after == before,
          [f for f in before if after[f] != before[f]])
    check("sound: the object-form scene and the story commands are on disk as written",
          b'"music": "title",\n          "commands"' in after["data/campaign.json"] or b'"music": "title"' in after["data/campaign.json"])

    # unknown keys at every level are kept too (the loader refuses them, but work in progress is saved as sent)
    odd = copy(audio)
    odd["futureKey"] = {"keep": [1, 2]}
    odd["music"]["title"]["futureKey"] = True
    odd["sounds"]["hit"]["futureKey"] = "x"
    odd["use"]["futureKey"] = 1
    st, res = put_project(api, pid, d, d["revision"], audio=odd)
    st2, d2 = get_project(api, pid)
    check("sound: unknown keys in audio.json survive, key order kept", st == 200 and d2["audio"] == odd
          and list(d2["audio"].keys()) == ["music", "sounds", "use", "futureKey"], d2["audio"])
    check("sound: the loader names the unknown key", any("futureKey" in p["message"] for p in problems()), problems())
    st, res = put_project(api, pid, d2, d2["revision"], audio=audio)
    check("sound: back to the valid file -> no problems", st == 200 and problems() == [], problems())

    # ---- delete: refused while the SAVED audio.json uses the file ----
    st, _, data = api.call("DELETE", base + "/step1.wav")
    body = jbody(data)
    check("sound file delete: 409 with usedBy while a sound of the saved audio.json uses it",
          st == 409 and body.get("usedBy") == [{"kind": "sounds", "id": "step"}] and os.path.exists(os.path.join(root, "audio", "step1.wav")), (st, body))
    st, _, data = api.call("DELETE", base + "/title.ogg")
    body = jbody(data)
    check("sound file delete: usedBy names tracks", st == 409 and {"kind": "music", "id": "title"} in body.get("usedBy", []), (st, body))
    st, lst = api.json("GET", base)
    check("sound files list: usedBy tells what uses each file",
          next(f for f in lst["audio"] if f["file"] == "audio/win.wav")["usedBy"] == [{"kind": "music", "id": "win"}], lst)
    put_file("spare.wav", wav)
    st, _, data = api.call("DELETE", base + "/spare.wav")
    check("sound file delete: an unused file goes (200 + revision)", st == 200 and jbody(data).get("revision") and not os.path.exists(os.path.join(root, "audio", "spare.wav")), (st, data))
    st, _, data = api.call("DELETE", base + "/spare.wav")
    check("sound file delete: no such file -> 404", st == 404, st)
    st, _, data = api.call("DELETE", base + "/Bad.wav")
    check("sound file delete: bad name -> 400", st == 400, st)

    # ---- what /problems says (the player's own loader) ----
    def with_change(label, change, needle, file=None):
        st, dd = get_project(api, pid)
        body = {"audio": copy(audio), "weapons": copy(weapons), "units": copy(units), "items": copy(items), "maps": {mid: copy(m)}, "campaign": copy(campaign)}
        change(body)
        st, res = put_project(api, pid, dd, dd["revision"], **body)
        ps = problems()
        check("sound problems: %s" % label, st == 200 and any(needle in p["message"] and (file is None or p["file"] == file) for p in ps), ps)
        st, dd = get_project(api, pid)
        put_project(api, pid, dd, dd["revision"], audio=audio, weapons=weapons, units=units, items=items, maps={mid: m}, campaign=campaign)

    with_change("a weapon naming an unknown sound", lambda b: b["weapons"][0]["sounds"].__setitem__("hit", "nope"), "nope")
    with_change("a unit with a key it cannot have", lambda b: b["units"][0]["sounds"].__setitem__("gold", "hit"), "gold")
    with_change("an item naming an unknown sound", lambda b: b["items"][0]["sounds"].__setitem__("use", "nope"), "nope")
    with_change("a map naming an unknown track", lambda b: b["maps"][mid].__setitem__("music", "nope"), "nope")
    with_change("a story command naming an unknown sound", lambda b: b["maps"][mid]["events"][0]["do"][1].__setitem__("playSound", "nope"), "nope")
    with_change("a scene naming an unknown track", lambda b: b["campaign"]["chapters"][0]["before"].__setitem__("music", "nope"), "nope")
    with_change("a looping jingle", lambda b: b["audio"]["music"]["win"].__setitem__("loop", True), "loop")
    with_change("a volume over 100", lambda b: b["audio"]["music"]["title"].__setitem__("volume", 101), "volume")
    with_change("an empty variants list", lambda b: b["audio"]["sounds"].__setitem__("step", {"files": []}), "files")
    with_change("17 variants", lambda b: b["audio"]["sounds"].__setitem__("step", {"files": ["audio/hit.wav"] * 17}), "files")
    with_change("a bad id", lambda b: b["audio"]["sounds"].__setitem__("my sound", {"file": "audio/hit.wav"}), "id")
    with_change("a path that is not .wav / .ogg", lambda b: b["audio"]["sounds"]["hit"].__setitem__("file", "audio/hit.mp3"), "ogg")
    with_change("a path outside the game", lambda b: b["audio"]["sounds"]["hit"].__setitem__("file", "../hit.wav"), "inside the game folder")
    with_change("an unknown key in use.sounds", lambda b: b["audio"]["use"]["sounds"].__setitem__("zap", "hit"), "zap")

    # an effect longer than 10 seconds is refused by the real loader, naming the sound
    put_file("long.wav", make_wav(10.5, 8000, 1))
    with_change("an effect longer than 10 seconds", lambda b: b["audio"]["sounds"].__setitem__("long", {"file": "audio/long.wav"}), "10 seconds", "data/audio.json")
    # as MUSIC the same file is fine (music is streamed)
    st, dd = get_project(api, pid)
    long_audio = copy(audio)
    long_audio["music"]["longtrack"] = {"file": "audio/long.wav"}
    st, res = put_project(api, pid, dd, dd["revision"], audio=long_audio)
    check("sound problems: a long file as music is fine", st == 200 and problems() == [], problems())
    # a damaged effect file
    with open(os.path.join(root, "audio", "bad.wav"), "wb") as f:
        f.write(b"RIFF\0\0\0\0WAVEjunkjunkjunk")
    with_change("a damaged effect file", lambda b: b["audio"]["sounds"].__setitem__("bad", {"file": "audio/bad.wav"}), "bad", "data/audio.json")
    os.remove(os.path.join(root, "audio", "bad.wav"))
    os.remove(os.path.join(root, "audio", "long.wav"))
    st, dd = get_project(api, pid)
    put_project(api, pid, dd, dd["revision"], audio=audio)

    # a missing file is only a warning in the player: it is not a problem here
    gone = copy(audio)
    gone["sounds"]["ghost"] = {"file": "audio/ghost.wav"}
    gone["music"]["ghosttrack"] = {"file": "audio/ghost.ogg"}
    st, dd = get_project(api, pid)
    st, res = put_project(api, pid, dd, dd["revision"], audio=gone)
    check("sound problems: a missing sound file is not a problem (the game starts without it)", st == 200 and problems() == [], problems())
    st, dd = get_project(api, pid)
    put_project(api, pid, dd, dd["revision"], audio=audio)

    # ---- broken audio.json: only that section is null, with a problem; the rest of the game loads ----
    with open(os.path.join(root, "data", "audio.json"), "w") as f:
        f.write('{ "music": ')
    st, d = get_project(api, pid)
    check("sound: a broken audio.json is null plus a problem on the file; the other sections still load",
          st == 200 and d["audio"] is None and any(p["file"] == "data/audio.json" for p in d["problems"])
          and d["weapons"] == weapons and d["maps"][mid] == m and d["campaign"] == campaign, d["problems"])
    with open(os.path.join(root, "data", "audio.json"), "w") as f:
        f.write('[1, 2]')
    st, d = get_project(api, pid)
    check("sound: an audio.json that is not an object is null plus a problem",
          st == 200 and d["audio"] is None and any(p["file"] == "data/audio.json" for p in d["problems"]), d["problems"])
    st, res = put_project(api, pid, d, d["revision"], audio=audio)
    check("sound: PUT audio replaces a broken file", st == 200 and problems() == [], (st, problems()))

    # ---- story commands in a game with no audio.json are refused by the loader ----
    st, d = get_project(api, pid)
    st, res = put_project(api, pid, d, d["revision"], audio=False)
    check("sound: PUT false removes data/audio.json (the audio files stay)",
          st == 200 and not os.path.exists(os.path.join(root, "data", "audio.json")) and os.path.exists(os.path.join(root, "audio", "hit.wav")), (st, res))
    st, d = get_project(api, pid)
    check("sound: removed audio reads back as null", d["audio"] is None)
    ps = problems()
    check("sound problems: a story sound command in a game without audio.json is reported",
          any("audio.json" in p["message"] for p in ps), ps)
    st, res = put_project(api, pid, d, d["revision"], audio=audio)
    check("sound: audio back -> no problems", st == 200 and problems() == [], problems())

    # audio.json is not a name a map file may take
    st, d = get_project(api, pid)
    g = copy(d["game"])
    g["maps"].append({"id": "sneaky", "name": "Sneaky", "file": "data/audio.json"})
    st, res = put_project(api, pid, d, d["revision"], game=g, maps={"sneaky": copy(m)})
    check("sound: a map may not use data/audio.json as its file", st == 400, (st, res))
