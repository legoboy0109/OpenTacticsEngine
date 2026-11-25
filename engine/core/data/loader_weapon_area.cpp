// weapons.json "fire" and "area" (docs/SRPG.md "Area attacks and line fire"). Every refusal names the file, the weapon and the key.
#include "core/data/loader_content.h"
#include "core/data/loader_util.h"

namespace ote {

namespace {

std::string q(const std::string& s) { return "\"" + s + "\""; }

}  // namespace

// The "shape", "radius" and "length" of an area object (weapons and abilities share them; the caller checks the other keys).
bool readAreaShape(const LoadCtx& c, const Json& a, const std::string& where, AreaDef& d) {
  const std::string range = "a whole number from 1 to " + std::to_string(kMaxAreaSize);
  const std::string at = where + ", \"area\"";
  const char* shapes = "\"burst\", \"line\", \"radius\", \"cross\", \"ring\" or \"field\"";
  if (!a.has("shape")) return c.fail(where, std::string("\"area\" needs a \"shape\": ") + shapes);
  std::string shape;
  if (!c.str(a, "shape", at, shape)) return false;
  if (shape == "burst") d.shape = AreaShape::Burst;
  else if (shape == "line") d.shape = AreaShape::Line;
  else if (shape == "radius") d.shape = AreaShape::Radius;
  else if (shape == "cross") d.shape = AreaShape::Cross;
  else if (shape == "ring") d.shape = AreaShape::Ring;    // the squares around the aimed one, not the aimed one
  else if (shape == "field") d.shape = AreaShape::Field;  // the whole map
  else return c.fail(where, std::string("\"area\".\"shape\" must be ") + shapes + " (not " + q(shape) + ")");

  const bool wantsRadius = d.shape == AreaShape::Radius || d.shape == AreaShape::Cross || d.shape == AreaShape::Ring;
  const bool wantsLength = d.shape == AreaShape::Line;
  if (a.has("radius") && !wantsRadius)
    return c.fail(where, d.shape == AreaShape::Field ? "\"area\".\"radius\": a \"field\" covers the whole map and takes no \"radius\""
                                                     : "\"area\".\"radius\" belongs to the shapes \"radius\", \"cross\" and \"ring\" (a burst is always radius 1; a line has a \"length\")");
  if (a.has("length") && !wantsLength) return c.fail(where, "\"area\".\"length\" belongs to the shape \"line\" only");
  if (d.shape == AreaShape::Field && a.has("vertical"))
    return c.fail(where, "\"area\".\"vertical\": a \"field\" covers the whole map at every height (leave out \"vertical\")");
  if (wantsRadius) {
    if (!a.has("radius")) return c.fail(where, "\"area\" of shape " + q(shape) + " needs a \"radius\" (" + range + ")");
    if (!c.integerFrom(a, "radius", at, 1, kMaxAreaSize, d.radius)) return false;
  }
  if (wantsLength) {
    if (!a.has("length")) return c.fail(where, "\"area\" of shape \"line\" needs a \"length\" (" + range + ")");
    if (!c.integerFrom(a, "length", at, 1, kMaxAreaSize, d.length)) return false;
  }
  if (d.shape == AreaShape::Burst) d.radius = 1;
  return true;
}

bool readWeaponArea(const LoadCtx& c, const Json& e, const std::string& where, WeaponDef& w) {
  w.fire = FireMode::Direct;
  w.area = AreaDef();
  if (e.has("fire")) {
    std::string mode;
    if (!c.str(e, "fire", where, mode)) return false;
    if (mode == "line") w.fire = FireMode::Line;
    else if (mode != "direct") return c.fail(where, "\"fire\" must be \"direct\" or \"line\" (not " + q(mode) + ")");
  }
  if (!e.has("area")) return true;
  const Json* a;
  if (!c.object(e, "area", where, a)) return false;
  const std::string at = where + ", \"area\"";
  for (const Json::Member& m : a->members())
    if (m.first != "shape" && m.first != "radius" && m.first != "length" && m.first != "friendlyFire" && m.first != "apExtra" && m.first != "vertical")
      return c.fail(where, "\"area\" has an unknown key " + q(m.first) + " (it takes shape, radius, length, friendlyFire, apExtra, vertical)" + didYouMean(m.first, {"shape", "radius", "length", "friendlyFire", "apExtra", "vertical"}));
  if (!readAreaShape(c, *a, where, w.area)) return false;
  return c.boolean(*a, "friendlyFire", at, w.area.friendlyFire);
}

}  // namespace ote
