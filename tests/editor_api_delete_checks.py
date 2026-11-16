"""Delete-a-game checks of the ote-editor API test (docs/EDITOR_API.md "DELETE /api/projects/:id").
Called from editor_api_test.py; uses its helpers."""
import os


def run_delete_checks(api, games, check):
    pid = "delete-test"
    st, doc = api.json("POST", "/api/projects", {"id": pid, "name": "Delete me", "template": "blank"})
    check("delete: create scratch project", st == 201, (st, doc))
    pdir = os.path.join(games, pid)

    st, doc = api.json("GET", "/api/projects")
    flags = {p["id"]: p.get("protected") for p in doc.get("projects", [])}
    check("delete: the list marks the demos as protected and other games not",
          flags.get("demo-srpg") is True and flags.get("demo-economy") is True and flags.get(pid) is False, flags)

    st, doc = api.json("DELETE", "/api/projects/" + pid)
    check("delete: refused without confirm", st == 400 and os.path.isdir(pdir), (st, doc))
    st, doc = api.json("DELETE", "/api/projects/%s?confirm=demo-srpg" % pid)
    check("delete: refused when confirm names another game", st == 400 and os.path.isdir(pdir), (st, doc))
    st, doc = api.json("DELETE", "/api/projects/demo-srpg?confirm=demo-srpg")
    check("delete: a demo is refused", st == 403 and os.path.isfile(os.path.join(games, "demo-srpg", "game.json")), (st, doc))
    st, doc = api.json("DELETE", "/api/projects/..?confirm=..")
    check("delete: a bad id is refused", st in (400, 404) and os.path.isdir(games), (st, doc))
    st, doc = api.json("DELETE", "/api/projects/no-such-game?confirm=no-such-game")
    check("delete: an unknown game is 404", st == 404, (st, doc))

    # A link inside the game must be removed, never followed: the file it points at stays.
    outside = os.path.join(os.path.dirname(games), "delete-outside.txt")
    with open(outside, "w") as f:
        f.write("keep me")
    os.symlink(outside, os.path.join(pdir, "link.txt"))
    before = sorted(os.listdir(games))
    st, doc = api.json("DELETE", "/api/projects/%s?confirm=%s" % (pid, pid))
    check("delete: confirmed delete answers 200 with the id", st == 200 and doc.get("deleted") == pid, (st, doc))
    check("delete: the folder is gone and nothing else in games/ changed",
          not os.path.exists(pdir) and sorted(os.listdir(games)) == [n for n in before if n != pid])
    check("delete: a link inside the game was not followed", os.path.isfile(outside) and open(outside).read() == "keep me")
    os.remove(outside)
    st, doc = api.json("GET", "/api/projects/" + pid)
    check("delete: the game can no longer be opened", st == 404, (st, doc))
    st, doc = api.json("DELETE", "/api/projects/%s?confirm=%s" % (pid, pid))
    check("delete: deleting it again is 404", st == 404, (st, doc))

    # A games/<id> that is itself a link to a folder elsewhere is refused.
    real = os.path.join(os.path.dirname(games), "delete-linked-game")
    os.makedirs(real)
    with open(os.path.join(real, "game.json"), "w") as f:
        f.write("{}")
    os.symlink(real, os.path.join(games, "linked-game"))
    st, doc = api.json("DELETE", "/api/projects/linked-game?confirm=linked-game")
    check("delete: a game folder that is a link is refused",
          st == 409 and os.path.isfile(os.path.join(real, "game.json")) and os.path.islink(os.path.join(games, "linked-game")), (st, doc))
    os.remove(os.path.join(games, "linked-game"))
    os.remove(os.path.join(real, "game.json"))
    os.rmdir(real)
