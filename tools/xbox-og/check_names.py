#!/usr/bin/env python3
"""Checks that every file and folder under the given folders can exist on the original Xbox's hard disk (FATX).

usage: check_names.py [--prefix PATH] FOLDER...

The rules are those of engine/core/base/fatx_names.h (docs/XBOX_OG.md "File name limits"), repeated here so the build
script needs no compiled helper:
  - a name is 1..42 characters
  - allowed: ASCII letters, digits, space and ! # $ % & ' ( ) - . @ [ ] ^ _ ` { } ~
  - names are compared without regard to case: two names in one folder that differ only by case are a clash
  - the whole path (PREFIX + the path inside FOLDER) stays under 240 characters (the kernel's limit is about 250)

PREFIX is where the folder will sit on the console (default E:\\Apps\\OpenTacticsEngine\\), so the depth counts.
Exit status 0 = fine, 1 = problems (listed one per line), 2 = bad usage.
"""
import os
import sys

MAX_NAME = 42
MAX_PATH = 240
ALLOWED = set("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!#$%&'()-.@[]^_`{}~ ")


def name_problem(name):
    if not name or name in (".", ".."):
        return "is not a file name"
    if len(name.encode("utf-8")) > MAX_NAME:
        return "is longer than %d characters (%d)" % (MAX_NAME, len(name.encode("utf-8")))
    for ch in name:
        if ch not in ALLOWED:
            if ord(ch) < 0x20 or ord(ch) >= 0x7F:
                return "contains the non-ASCII or control character U+%04X" % ord(ch)
            return "contains '%s', which FATX does not allow" % ch
    return ""


def check(root, prefix):
    problems = []
    for dirpath, dirnames, filenames in os.walk(root):
        rel_dir = os.path.relpath(dirpath, root)
        names = dirnames + filenames
        seen = {}
        for n in names:
            rel = n if rel_dir == "." else os.path.join(rel_dir, n)
            why = name_problem(n)
            if why:
                problems.append("%s: '%s' %s" % (rel, n, why))
            full = prefix + rel.replace("/", "\\")
            if len(full) > MAX_PATH:
                problems.append("%s: the path on the console would be %d characters (%s), over %d" % (rel, len(full), full, MAX_PATH))
            low = n.lower()
            if low in seen and seen[low] != n:
                problems.append("%s: differs only by case from '%s' in the same folder" % (rel, seen[low]))
            seen.setdefault(low, n)
    return problems


def main(argv):
    prefix = "E:\\Apps\\OpenTacticsEngine\\"
    args = argv[1:]
    if args[:1] == ["--prefix"] and len(args) >= 2:
        prefix = args[1]
        args = args[2:]
    if not args:
        sys.stderr.write(__doc__)
        return 2
    bad = []
    for root in args:
        if not os.path.isdir(root):
            sys.stderr.write("not a folder: %s\n" % root)
            return 2
        bad += ["%s: %s" % (root, p) for p in check(root, prefix)]
    for p in bad:
        print(p)
    if bad:
        print("%d problem(s): fix these names before copying to the console" % len(bad))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
