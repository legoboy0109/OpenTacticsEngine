#!/usr/bin/env python3
"""Cuts a .glb down to what a game uses: chosen animations and chosen parts.

Character packs ship every weapon on every model and dozens of animations
(megabytes of keys). This keeps the named animations (optionally renamed),
drops the meshes of the named nodes, and writes a new .glb holding only the
data still referenced.

  trim_glb.py in.glb out.glb --keep Idle --keep Move=Walking_A --drop 2H_Sword

--keep NAME or NEW=OLD (repeatable): animations to keep; everything else goes.
--drop NODE (repeatable): nodes whose mesh is removed (the node stays, so
                          the skeleton and other nodes are untouched).
"""
import argparse
import json
import struct
import sys


def read_glb(path):
    data = open(path, "rb").read()
    magic, version, _ = struct.unpack_from("<4sII", data, 0)
    if magic != b"glTF" or version != 2:
        sys.exit(f"{path}: not a glTF 2 binary file")
    off, doc, blob = 12, None, b""
    while off < len(data):
        length, kind = struct.unpack_from("<I4s", data, off)
        chunk = data[off + 8: off + 8 + length]
        if kind == b"JSON":
            doc = json.loads(chunk)
        elif kind == b"BIN\0":
            blob = chunk
        off += 8 + length
    if doc is None:
        sys.exit(f"{path}: no JSON chunk")
    return doc, blob


def write_glb(path, doc, blob):
    text = json.dumps(doc, separators=(",", ":")).encode()
    text += b" " * (-len(text) % 4)
    blob += b"\0" * (-len(blob) % 4)
    total = 12 + 8 + len(text) + 8 + len(blob)
    with open(path, "wb") as f:
        f.write(struct.pack("<4sII", b"glTF", 2, total))
        f.write(struct.pack("<I4s", len(text), b"JSON") + text)
        f.write(struct.pack("<I4s", len(blob), b"BIN\0") + blob)


def trim(doc, blob, keep, drop):
    # Animations: keep the asked ones, in the order asked, under their new names.
    by_name = {a.get("name"): a for a in doc.get("animations", [])}
    anims = []
    for new, old in keep:
        if old not in by_name:
            sys.exit(f"no animation named {old!r} (has: {', '.join(sorted(n for n in by_name if n))})")
        a = dict(by_name[old])
        a["name"] = new
        anims.append(a)
    if anims:
        doc["animations"] = anims
    else:
        doc.pop("animations", None)

    names = {n.get("name") for n in doc.get("nodes", [])}
    for d in drop:
        if d not in names:
            sys.exit(f"no node named {d!r}")
    for n in doc.get("nodes", []):
        if n.get("name") in drop:
            n.pop("mesh", None)
            n.pop("skin", None)

    # Meshes still used, renumbered.
    used_meshes = sorted({n["mesh"] for n in doc["nodes"] if "mesh" in n})
    mesh_map = {old: i for i, old in enumerate(used_meshes)}
    doc["meshes"] = [doc["meshes"][m] for m in used_meshes]
    for n in doc["nodes"]:
        if "mesh" in n:
            n["mesh"] = mesh_map[n["mesh"]]

    # Accessors still used.
    acc = set()
    for m in doc["meshes"]:
        for p in m["primitives"]:
            acc.update(p["attributes"].values())
            if "indices" in p:
                acc.add(p["indices"])
            for t in p.get("targets", []):
                acc.update(t.values())
    for s in doc.get("skins", []):
        if "inverseBindMatrices" in s:
            acc.add(s["inverseBindMatrices"])
    for a in doc.get("animations", []):
        for s in a["samplers"]:
            acc.update((s["input"], s["output"]))
    acc_list = sorted(acc)
    acc_map = {old: i for i, old in enumerate(acc_list)}

    def remap_prim(p):
        p["attributes"] = {k: acc_map[v] for k, v in p["attributes"].items()}
        if "indices" in p:
            p["indices"] = acc_map[p["indices"]]
        if "targets" in p:
            p["targets"] = [{k: acc_map[v] for k, v in t.items()} for t in p["targets"]]

    for m in doc["meshes"]:
        for p in m["primitives"]:
            remap_prim(p)
    for s in doc.get("skins", []):
        if "inverseBindMatrices" in s:
            s["inverseBindMatrices"] = acc_map[s["inverseBindMatrices"]]
    for a in doc.get("animations", []):
        a["samplers"] = [dict(s, input=acc_map[s["input"]], output=acc_map[s["output"]]) for s in a["samplers"]]
    doc["accessors"] = [doc["accessors"][a] for a in acc_list]
    for a in doc["accessors"]:
        if "sparse" in a:
            sys.exit("sparse accessors are not supported")

    # Buffer views still used (accessors and images), packed into a new buffer.
    views = {a["bufferView"] for a in doc["accessors"] if "bufferView" in a}
    views |= {i["bufferView"] for i in doc.get("images", []) if "bufferView" in i}
    view_list = sorted(views)
    view_map = {old: i for i, old in enumerate(view_list)}
    out = bytearray()
    new_views = []
    for old in view_list:
        v = dict(doc["bufferViews"][old])
        if v.get("buffer", 0) != 0:
            sys.exit("only files with one buffer are supported")
        start = v.get("byteOffset", 0)
        out += b"\0" * (-len(out) % 4)
        v["byteOffset"] = len(out)
        out += blob[start: start + v["byteLength"]]
        new_views.append(v)
    doc["bufferViews"] = new_views
    for a in doc["accessors"]:
        if "bufferView" in a:
            a["bufferView"] = view_map[a["bufferView"]]
    for i in doc.get("images", []):
        if "bufferView" in i:
            i["bufferView"] = view_map[i["bufferView"]]
    doc["buffers"] = [{"byteLength": len(out)}]
    return doc, bytes(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("src")
    ap.add_argument("dst")
    ap.add_argument("--keep", action="append", default=[])
    ap.add_argument("--drop", action="append", default=[])
    args = ap.parse_args()
    keep = [tuple(k.split("=", 1)) if "=" in k else (k, k) for k in args.keep]
    doc, blob = read_glb(args.src)
    doc, blob = trim(doc, blob, keep, set(args.drop))
    write_glb(args.dst, doc, blob)


if __name__ == "__main__":
    main()
