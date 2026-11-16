"""For checks that start from the strategy-rpg template and bring their own characters: the template's maps place
characters (since step D1), so a check that replaces the character list first turns those placed people back into plain
units of the character's type."""


def plain_units(d):
    types = {c["id"]: c["type"] for c in (d.get("characters") or [])}
    for m in d["maps"].values():
        for u in m.get("units", []):
            if "character" in u:
                u.setdefault("type", types.get(u["character"]))
                del u["character"]
    return d["maps"]
