// data/graphics.json "battle" > "stage" (docs/BATTLE_SCENE.md "Fixed stage"): the stage's size, where each fighter stands, where the
// platforms are, and how it is cropped. Presentation only.
#include "core/data/loader_content.h"
#include "core/data/loader_graphics_art.h"
#include "core/data/loader_util.h"

namespace ote {
namespace {

std::string q(const std::string& s) { return "\"" + s + "\""; }

// { "left": [x, y], "right": [x, y] } with both points inside the stage.
bool readPair(const LoadCtx& c, const Json& st, const char* key, int w, int h, int out[2][2]) {
  const std::string where = std::string("battle stage ") + q(key);
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
    if (x < 0 || y < 0 || x >= w || y >= h)
      return c.fail(where, std::string(q(side)) + " [" + std::to_string(x) + ", " + std::to_string(y) + "] is outside the stage (" + std::to_string(w) + " x " + std::to_string(h) + ")");
    out[s][0] = x;
    out[s][1] = y;
  }
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
    if (k != "width" && k != "height" && k != "feet" && k != "feetFar" && k != "platform" && k != "platformFar" && k != "align" && k != "fit" && k != "rangeLine")
      return c.fail(where, "unknown key " + q(k) + " (expected \"width\", \"height\", \"feet\", \"feetFar\", \"platform\", \"platformFar\", \"align\", \"fit\", \"rangeLine\")");
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
  s.on = true;
  out = s;
  return true;
}

}  // namespace ote
