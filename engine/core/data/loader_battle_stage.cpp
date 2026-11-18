// data/graphics.json "battle" > "stage" (docs/BATTLE_SCENE.md "Fixed stage"): the stage's size, where each fighter stands, where the
// platforms are, how it is cropped, and the extra layouts by fight distance ("Range bands"). Presentation only.
#include <algorithm>

#include "core/data/loader_content.h"
#include "core/data/loader_graphics_art.h"
#include "core/data/loader_util.h"

namespace ote {
namespace {

std::string q(const std::string& s) { return "\"" + s + "\""; }

// { "left": [x, y], "right": [x, y] } with both points inside the stage: x from 0 to `maxX` (the stage's width - 1, or the world's end for a band
// that pans), y inside the height. `base` names the object for the messages.
bool readPairIn(const LoadCtx& c, const Json& st, const char* key, int maxX, int h, int out[2][2], const std::string& base) {
  const std::string where = base + " " + q(key);
  const Json& m = st[key];
  if (!m.isObject()) return c.fail(where, "must be { \"left\": [x, y], \"right\": [x, y] } in stage pixels");
  for (const Json::Member& e : m.members())
    if (e.first != "left" && e.first != "right") return c.fail(where, "unknown key " + q(e.first) + " (expected \"left\", \"right\")");
  for (int s = 0; s < 2; ++s) {
    const char* side = s == 0 ? "left" : "right";
    if (!m.has(side)) return c.fail(where, std::string("needs \"") + side + "\"");
    const Json& p = m[side];
    if (!p.isArray() || p.size() != 2 || !LoadCtx::isWhole(p[0]) || !LoadCtx::isWhole(p[1]))
      return c.fail(where, std::string(q(side)) + " must be [x, y], two whole numbers (stage pixels)");
    const int x = p[0].asInt(), y = p[1].asInt();
    if (x < 0 || y < 0 || x > maxX || y >= h)
      return c.fail(where, std::string(q(side)) + " [" + std::to_string(x) + ", " + std::to_string(y) + "] is outside the stage (" + std::to_string(maxX + 1) + " x " + std::to_string(h) + ")");
    out[s][0] = x;
    out[s][1] = y;
  }
  return true;
}

bool readPair(const LoadCtx& c, const Json& st, const char* key, int w, int h, int out[2][2]) {
  return readPairIn(c, st, key, w - 1, h, out, "battle stage");
}

// "ranges": [ { "from": 4, "feet": {...}, "platform": {...}, "view": "both" | "each", "pan": { "panMs": 250, "leftView": 0, "rightView": 120 } } ]
bool readRanges(const LoadCtx& c, const Json& st, BattleStage& s) {
  const Json& list = st["ranges"];
  if (!list.isArray() || list.size() < 1 || list.size() > 16)
    return c.fail("battle stage", "\"ranges\" must be a list of 1 to 16 range bands: { \"from\": 4, \"feet\": { \"left\": [x, y], \"right\": [x, y] } }");
  for (size_t i = 0; i < list.size(); ++i) {
    const Json& j = list[i];
    const std::string where = "battle stage range " + std::to_string(i + 1);
    if (!j.isObject()) return c.fail(where, "must be an object with \"from\" and \"feet\"");
    for (const Json::Member& e : j.members()) {
      const std::string& k = e.first;
      if (k != "from" && k != "feet" && k != "platform" && k != "view" && k != "pan")
        return c.fail(where, "unknown key " + q(k) + " (expected \"from\", \"feet\", \"platform\", \"view\", \"pan\")");
    }
    StageBand b;
    if (!c.integer(j, "from", where, b.from, true)) return false;
    if (b.from < 2 || b.from > 99)
      return c.fail(where, "\"from\" must be between 2 and 99 (the fight distance in squares the band starts at; 1 is the close layout \"feet\")");
    for (const StageBand& o : s.ranges)
      if (o.from == b.from) return c.fail(where, "a second band starts at " + std::to_string(b.from) + " (each \"from\" can be used once)");
    std::string view = "both";
    if (!c.str(j, "view", where, view)) return false;
    if (view != "both" && view != "each")
      return c.fail(where, "\"view\" must be \"both\" (both fighters in one picture, the default) or \"each\" (one fighter at a time, with a \"pan\")");
    b.each = view == "each";
    if (j.has("pan") && !b.each) return c.fail(where, "\"pan\" belongs to \"view\": \"each\" (a band that shows both fighters does not move)");
    if (b.each && !j.has("pan")) return c.fail(where, "\"view\": \"each\" needs a \"pan\": { \"panMs\": 250, \"leftView\": x, \"rightView\": x }");
    // A panning band lays its points out in a world wider than the stage; the others inside the stage.
    const int maxX = b.each ? 8191 : s.width - 1;
    if (!j.has("feet")) return c.fail(where, "needs \"feet\": { \"left\": [x, y], \"right\": [x, y] }");
    if (!readPairIn(c, j, "feet", maxX, s.height, b.feet, where)) return false;
    if (j.has("platform")) {
      if (!readPairIn(c, j, "platform", maxX, s.height, b.platform, where)) return false;
      b.hasPlatform = true;
    }
    if (b.each) {
      const Json& p = j["pan"];
      const std::string pw = where + " \"pan\"";
      if (!p.isObject()) return c.fail(pw, "must be { \"panMs\": 250, \"leftView\": x, \"rightView\": x }");
      for (const Json::Member& e : p.members())
        if (e.first != "panMs" && e.first != "leftView" && e.first != "rightView" && e.first != "backdrop" && e.first != "startMs")
          return c.fail(pw, "unknown key " + q(e.first) + " (expected \"panMs\", \"leftView\", \"rightView\", \"backdrop\", \"startMs\")");
      if (!c.integer(p, "panMs", pw, b.panMs) || !c.integer(p, "leftView", pw, b.leftView, true) || !c.integer(p, "rightView", pw, b.rightView, true)) return false;
      if (b.panMs < 0 || b.panMs > 5000) return c.fail(pw, "\"panMs\" must be between 0 and 5000 milliseconds (0 = the camera cuts)");
      if (b.leftView < 0 || b.leftView > 8191 || b.rightView < 0 || b.rightView > 8191)
        return c.fail(pw, "\"leftView\" and \"rightView\" must be between 0 and 8191 (the stage x of the picture's left edge)");
      std::string backdrop = "fixed";
      if (!c.str(p, "backdrop", pw, backdrop)) return false;
      if (backdrop != "fixed" && backdrop != "scroll")
        return c.fail(pw, "\"backdrop\" must be \"fixed\" (the picture stays put, the default) or \"scroll\" (it moves with the camera and repeats sideways)");
      b.scrollBackdrop = backdrop == "scroll";
      if (p.has("startMs")) {
        if (!c.integer(p, "startMs", pw, b.panStartMs)) return false;
        if (b.panStartMs < 0 || b.panStartMs > 15000)
          return c.fail(pw, "\"startMs\" must be between 0 and 15000 milliseconds (how long after the strike starts the camera moves to the struck fighter)");
      }
      // Each fighter must be in the picture while the camera is on it.
      const int views[2] = {b.leftView, b.rightView};
      const char* names[2] = {"left", "right"};
      for (int side = 0; side < 2; ++side)
        if (b.feet[side][0] < views[side] || b.feet[side][0] >= views[side] + s.width)
          return c.fail(pw, std::string("the ") + names[side] + " fighter's foot point x " + std::to_string(b.feet[side][0]) +
                                " is not inside the picture while the camera is on it (\"" + names[side] + "View\" " + std::to_string(views[side]) + " shows x " +
                                std::to_string(views[side]) + " to " + std::to_string(views[side] + s.width - 1) + ")");
    }
    s.ranges.push_back(b);
  }
  std::sort(s.ranges.begin(), s.ranges.end(), [](const StageBand& x, const StageBand& y) { return x.from < y.from; });
  return true;
}

}  // namespace

bool readBattleStage(const LoadCtx& c, const Json& battle, BattleStage& out) {
  out = BattleStage();
  if (!battle.has("stage")) return true;
  const Json& st = battle["stage"];
  const std::string where = "battle stage";
  if (!st.isObject()) return c.fail("battle", "\"stage\" must be an object with \"width\", \"height\" and \"feet\"");
  for (const Json::Member& e : st.members()) {
    const std::string& k = e.first;
    if (k != "width" && k != "height" && k != "feet" && k != "feetFar" && k != "platform" && k != "platformFar" && k != "align" && k != "fit" && k != "rangeLine" &&
        k != "ranges")
      return c.fail(where, "unknown key " + q(k) +
                               " (expected \"width\", \"height\", \"feet\", \"feetFar\", \"platform\", \"platformFar\", \"align\", \"fit\", \"rangeLine\", \"ranges\")");
  }
  BattleStage s;
  if (!c.integer(st, "width", where, s.width, true) || !c.integer(st, "height", where, s.height, true)) return false;
  if (s.width < 16 || s.width > 1024 || s.height < 16 || s.height > 1024)
    return c.fail(where, "\"width\" and \"height\" must be between 16 and 1024 pixels (got " + std::to_string(s.width) + " x " + std::to_string(s.height) + ")");
  if (!st.has("feet")) return c.fail(where, "needs \"feet\": { \"left\": [x, y], \"right\": [x, y] }");
  if (!readPair(c, st, "feet", s.width, s.height, s.feet)) return false;
  if (st.has("feetFar")) {
    if (!readPair(c, st, "feetFar", s.width, s.height, s.feetFar)) return false;
    s.hasFeetFar = true;
  }
  if (st.has("platform")) {
    if (!readPair(c, st, "platform", s.width, s.height, s.platform)) return false;
    s.hasPlatform = true;
  }
  if (st.has("platformFar")) {
    if (!s.hasPlatform) return c.fail(where, "\"platformFar\" needs \"platform\" too");
    if (!readPair(c, st, "platformFar", s.width, s.height, s.platformFar)) return false;
    s.hasPlatformFar = true;
  }
  std::string align = "center", fit = "cover";
  if (!c.str(st, "align", where, align) || !c.str(st, "fit", where, fit)) return false;
  if (align == "top") s.align = BattleStage::AlignTop;
  else if (align == "center") s.align = BattleStage::AlignCenter;
  else if (align == "bottom") s.align = BattleStage::AlignBottom;
  else return c.fail(where, "\"align\" must be \"top\", \"center\" or \"bottom\"");
  if (fit != "cover" && fit != "contain") return c.fail(where, "\"fit\" must be \"cover\" or \"contain\"");
  s.contain = fit == "contain";
  if (!c.boolean(st, "rangeLine", where, s.rangeLine)) return false;
  if (st.has("ranges") && !readRanges(c, st, s)) return false;
  s.on = true;
  out = s;
  return true;
}

}  // namespace ote
