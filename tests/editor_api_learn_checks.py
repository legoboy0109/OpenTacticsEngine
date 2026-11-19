"""The tutorial's remembered state through the ote-editor API (docs/TUTORIAL.md "Remembered state", docs/EDITOR_API.md
GET/PUT /api/learn): one JSON object, kept in one file outside the games folder, written atomically, a missing or damaged
file means "no state", and `--learn-file ""` keeps it in memory only.
Every helper started here gets a temp --learn-file or temp HOME, never the real user data folder.
Called from editor_api_test.py; uses its helpers."""
import json
import os
import shutil
import socket
import subprocess
import time


def _free_port():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
    s.close()
    return port


class _Helper:
    """A second helper process, with its own flags and environment."""

    def __init__(self, api_class, binary, repo, tmp, name, flags, env=None):
        self.root = os.path.join(tmp, name)
        os.makedirs(self.root)
        self.port = _free_port()
        self.proc = subprocess.Popen(
            [binary, "--games", os.path.join(self.root, "games"), "--templates", os.path.join(repo, "editor", "templates"),
             "--assets", os.path.join(repo, "assets"), "--web", os.path.join(self.root, "no-web"),
             "--player", os.path.join(self.root, "no-player"), "--port", str(self.port)] + flags,
            stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, env=env)
        self.api = api_class(self.port)
        for _ in range(100):
            try:
                if self.api.call("GET", "/api/info")[0] == 200:
                    return
            except OSError:
                pass
            time.sleep(0.1)
        raise RuntimeError("helper %s did not start" % name)

    def stop(self):
        self.proc.kill()
        self.proc.wait()


def run_learn_checks(api, binary, repo, tmp, learn_file, check):
    games_dir = os.path.join(tmp, "work", "games")
    state = {"version": 1, "offerAnswered": True,
             "lessons": {"first-game": {"state": "running", "step": 3, "skipped": ["a"], "game": "practice-my-first-game"}}}

    # Nothing stored yet: an empty object, not an error, and no file is made by reading.
    st, doc = api.json("GET", "/api/learn")
    check("learn: nothing stored reads as {}", st == 200 and doc == {}, (st, doc))
    check("learn: reading creates no file", not os.path.exists(learn_file))

    # Round trip.
    st, doc = api.json("PUT", "/api/learn", state)
    check("learn: PUT answers 200 with the stored state", st == 200 and doc == state, (st, doc))
    st, doc = api.json("GET", "/api/learn")
    check("learn: GET returns what was PUT (unknown keys kept)", st == 200 and doc == state, (st, doc))
    check("learn: the state file is where --learn-file says", os.path.isfile(learn_file))
    with open(learn_file, "r", encoding="utf-8") as f:
        on_disk = json.load(f)
    check("learn: the file holds the same JSON", on_disk == state, on_disk)
    left = [n for n in os.listdir(os.path.dirname(learn_file)) if n != "learn.json"]
    check("learn: no temp file is left beside it (atomic write)", left == [], left)
    check("learn: nothing is written into the games folder",
          not any(n.startswith("learn") for n in os.listdir(games_dir)), os.listdir(games_dir))

    # A newer PUT replaces the whole state.
    st, _ = api.json("PUT", "/api/learn", {"version": 1})
    st2, doc = api.json("GET", "/api/learn")
    check("learn: a PUT replaces the previous state", st == 200 and st2 == 200 and doc == {"version": 1}, doc)

    # Bad bodies: nothing changes.
    for name, raw in (("an array", b"[1]"), ("a string", b'"x"'), ("a number", b"5"), ("null", b"null"),
                      ("broken JSON", b"{not json"), ("an empty body", b"")):
        st, _, _ = api.call("PUT", "/api/learn", raw_body=raw, headers={"Content-Type": "application/json"})
        check("learn: PUT of %s is refused with 400" % name, st == 400, st)
    big = {"version": 1, "pad": "x" * (70 * 1024)}
    st, doc = api.json("PUT", "/api/learn", big)
    check("learn: a state over 64 KB is refused with 413", st == 413, (st, doc))
    st, doc = api.json("GET", "/api/learn")
    check("learn: refused PUTs changed nothing", st == 200 and doc == {"version": 1}, doc)
    nearly = {"version": 1, "pad": "y" * (60 * 1024)}
    st, _ = api.json("PUT", "/api/learn", nearly)
    check("learn: a state just under the limit is accepted", st == 200, st)

    # Damaged file = no state, never an error; and the next PUT repairs it.
    for name, raw in (("garbage", b"\x00\xffnot json at all"), ("an array", b"[1,2]"), ("an empty file", b""),
                      ("a truncated file", b'{"version":1,"lessons":{"first-g'), ("an oversized file", b'{"a":"' + b"z" * (80 * 1024) + b'"}')):
        with open(learn_file, "wb") as f:
            f.write(raw)
        st, doc = api.json("GET", "/api/learn")
        check("learn: %s in the file reads as {} (not an error)" % name, st == 200 and doc == {}, (st, doc))
    st, _ = api.json("PUT", "/api/learn", state)
    st2, doc = api.json("GET", "/api/learn")
    check("learn: a PUT after a damaged file works", st == 200 and doc == state, (st, st2, doc))
    os.remove(learn_file)
    st, doc = api.json("GET", "/api/learn")
    check("learn: a deleted file reads as {}", st == 200 and doc == {}, (st, doc))

    # Wrong method and the Host rule still apply.
    st, _ = api.json("DELETE", "/api/learn")
    check("learn: DELETE is not allowed", st in (404, 405), st)
    st, _ = api.json("GET", "/api/learn", headers={"Host": "evil.example"})
    check("learn: a foreign Host header is refused", st == 403, st)

    # The state survives a restart of the helper (it is a file, not memory).
    api.json("PUT", "/api/learn", state)
    h = _Helper(type(api), binary, repo, tmp, "learn-restart", ["--learn-file", learn_file])
    try:
        st, doc = h.api.json("GET", "/api/learn")
        check("learn: a restarted helper reads the same file", st == 200 and doc == state, (st, doc))
    finally:
        h.stop()

    # Memory only: --learn-file "" keeps it until the helper stops, and writes no file anywhere.
    mem_home = os.path.join(tmp, "learn-mem-home")
    os.makedirs(mem_home)
    env = dict(os.environ, HOME=mem_home)
    env.pop("XDG_DATA_HOME", None)
    h = _Helper(type(api), binary, repo, tmp, "learn-mem", ["--learn-file", ""], env)
    try:
        st, doc = h.api.json("GET", "/api/learn")
        check("learn: memory-only starts empty", st == 200 and doc == {}, (st, doc))
        st, _ = h.api.json("PUT", "/api/learn", state)
        st2, doc = h.api.json("GET", "/api/learn")
        check("learn: memory-only keeps what was PUT", st == 200 and doc == state, (st, st2, doc))
    finally:
        h.stop()
    check("learn: memory-only wrote no file (not even under HOME)", os.listdir(mem_home) == [], os.listdir(mem_home))
    h = _Helper(type(api), binary, repo, tmp, "learn-mem2", ["--learn-file", ""], env)
    try:
        st, doc = h.api.json("GET", "/api/learn")
        check("learn: memory-only state is gone after a restart", st == 200 and doc == {}, (st, doc))
    finally:
        h.stop()

    # Default place (no flag): <user data>/OpenTacticsEngine/editor/learn.json. Linux: $XDG_DATA_HOME or ~/.local/share
    # (HOME is a temp folder, so the real one is safe); Windows: %APPDATA%.
    windows = os.name == "nt"
    home = os.path.join(tmp, "learn-home")
    os.makedirs(home)
    env = dict(os.environ, APPDATA=home) if windows else dict(os.environ, HOME=home)
    env.pop("XDG_DATA_HOME", None)
    h = _Helper(type(api), binary, repo, tmp, "learn-default", [], env)
    data_base = home if windows else os.path.join(home, ".local", "share")
    expected = os.path.join(data_base, "OpenTacticsEngine", "editor", "learn.json")
    place = "%APPDATA%" if windows else "~/.local/share"
    try:
        st, doc = h.api.json("GET", "/api/learn")
        check("learn: default place: reading before any PUT works and makes nothing",
              st == 200 and doc == {} and os.listdir(home) == [], (st, doc))
        st, _ = h.api.json("PUT", "/api/learn", state)
        check("learn: default place is %s/OpenTacticsEngine/editor/learn.json" % place,
              st == 200 and os.path.isfile(expected), (st, os.listdir(home)))
    finally:
        h.stop()
    if not windows:
        # A relative or unusable XDG_DATA_HOME is ignored; an absolute one is honoured.
        xdg = os.path.join(tmp, "learn-xdg")
        os.makedirs(xdg)
        env = dict(os.environ, HOME=home, XDG_DATA_HOME=xdg)
        h = _Helper(type(api), binary, repo, tmp, "learn-xdg-helper", [], env)
        try:
            st, _ = h.api.json("PUT", "/api/learn", {"version": 1})
            check("learn: XDG_DATA_HOME is honoured", st == 200 and os.path.isfile(os.path.join(xdg, "OpenTacticsEngine", "editor", "learn.json")), st)
        finally:
            h.stop()

    # A state file that cannot be written (the folder is a file): 500 with a message, helper survives.
    blocked = os.path.join(tmp, "learn-blocked")
    with open(blocked, "w") as f:
        f.write("a file where a folder should be")
    h = _Helper(type(api), binary, repo, tmp, "learn-blocked-helper", ["--learn-file", os.path.join(blocked, "learn.json")])
    try:
        st, doc = h.api.json("PUT", "/api/learn", state)
        check("learn: an unwritable place answers 500 with a message", st == 500 and isinstance(doc, dict) and "error" in doc, (st, doc))
        st, doc = h.api.json("GET", "/api/learn")
        check("learn: ... and reading still gives {}", st == 200 and doc == {}, (st, doc))
    finally:
        h.stop()
    shutil.rmtree(os.path.join(tmp, "learn-home"), ignore_errors=True)
