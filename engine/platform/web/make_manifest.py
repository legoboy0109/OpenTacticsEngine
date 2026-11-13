#!/usr/bin/env python3
"""Lists every file under games/ and assets/ of a web output folder.

usage: make_manifest.py OUTPUT_DIR MANIFEST_FILE
Writes {"files": [{"path": "games/x/game.json", "size": 123}, ...]} (sorted),
which index.html uses to fill the player's virtual file system.
"""
import json
import os
import sys


def main():
    root, out = sys.argv[1], sys.argv[2]
    files = []
    for top in ("games", "assets"):
        for dirpath, _dirs, names in os.walk(os.path.join(root, top)):
            for name in names:
                full = os.path.join(dirpath, name)
                rel = os.path.relpath(full, root).replace(os.sep, "/")
                files.append({"path": rel, "size": os.path.getsize(full)})
    files.sort(key=lambda f: f["path"])
    with open(out, "w") as f:
        json.dump({"files": files}, f, indent=1)
        f.write("\n")


main()
