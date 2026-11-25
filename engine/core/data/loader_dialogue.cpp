#include "core/data/loader_dialogue.h"

#include <algorithm>

#include "core/data/events.h"
#include "core/data/loader_scene_fx.h"
#include "core/data/name_tokens.h"

namespace ote {

namespace {
std::string quotedText(const std::string& s) { return "\"" + s + "\""; }
}  // namespace

bool readDialogueLook(const LoadCtx& c, const Json& root, Database& db) {
  db.dialogue = Database::DialogueLook();
  const Json* d;
  if (!c.object(root, "dialogue", "", d)) return false;
  if (!root.has("dialogue")) return true;
  if (!c.onlyKeys(*d, {"style", "portrait", "names"}, "\"dialogue\"")) return false;
  std::string style = "box";
  if (!c.str(*d, "style", "\"dialogue\"", style)) return false;
  if (style != "box" && style != "bubble")
    return c.fail("\"dialogue\"", "\"style\" must be \"box\" (the box at the bottom of the screen, the default) or \"bubble\" (a speech bubble "
                                  "pointing at the unit that speaks), got " + quotedText(style));
  db.dialogue.bubbles = style == "bubble";
  if (!db.dialogue.bubbles && (d->has("portrait") || d->has("names")))
    return c.fail("\"dialogue\"", "\"portrait\" and \"names\" are for speech bubbles: they need \"style\": \"bubble\"");
  std::string portrait = "beside";
  if (!c.str(*d, "portrait", "\"dialogue\"", portrait)) return false;
  if (portrait != "beside" && portrait != "inside" && portrait != "none")
    return c.fail("\"dialogue\"", "\"portrait\" must be \"beside\" (next to the bubble, the default), \"inside\" (in the bubble) or \"none\", got " +
                                      quotedText(portrait));
  db.dialogue.portrait = portrait == "none" ? 0 : portrait == "inside" ? 1 : 2;
  return c.boolean(*d, "names", "\"dialogue\"", db.dialogue.names);
}

bool readFace(const LoadCtx& c, const Database& db, const Json& e, const std::string& at, SceneFx& fx) {
  if (!e.has("face")) return true;
  if (!c.integerIn(e, "face", at, 1, kMaxFaces, fx.face)) return false;
  if (fx.face > db.graphics.maxExpressions)
    return c.fail(at, "\"face\" " + std::to_string(fx.face) + " needs a portrait with at least " + std::to_string(fx.face) + " \"expressions\" " +
                          (db.graphics.maxExpressions == 0 ? std::string("(no portrait of data/graphics.json has any)")
                                                           : "(the most any portrait of data/graphics.json has is " +
                                                                 std::to_string(db.graphics.maxExpressions) + ")"));
  return true;
}

bool readChoiceLook(const LoadCtx& c, const Database& db, const Json& e, const std::string& at, size_t answers, SceneFx& fx) {
  if (!c.boolean(e, "notice", at, fx.notice)) return false;
  if (fx.notice)
    for (const char* k : {"speaker", "speakerName", "portrait", "face"})
      if (e.has(k))
        return c.fail(at, std::string("\"") + k + "\" makes no sense with \"notice\": true (the notice window has no speaker and no portrait)");
  if (!readFace(c, db, e, at, fx)) return false;
  if (e.has("cursor")) {
    const int most = (int)std::max<size_t>(1, answers);
    if (!c.integerIn(e, "cursor", at, 1, most, fx.cursor)) return false;
  }
  return true;
}

bool checkNameTokens(const LoadCtx& c, const std::string& text, const std::string& at) {
  std::string why;
  return nameTokensOk(text, why) || c.fail(at, why);
}

bool readNotice(const LoadCtx& c, const Database& db, bool mapEvent, const Json& e, const std::string& at, std::string& text, SceneFx& fx) {
  SceneFxReader r{c, db, mapEvent, nullptr};
  if (!r.onlyKeys(e, {"notice", "auto"}, at)) return false;
  if (!e["notice"].isString() || e["notice"].asString().empty()) return c.fail(at, "\"notice\" must be the text to show (not empty)");
  text = e["notice"].asString();
  if (!checkNameTokens(c, text, at)) return false;
  fx.notice = true;
  if (e.has("auto")) {
    fx.ms = 0;
    if (!r.ms(e, "auto", at, 1, kMaxAutoMs, fx.ms)) return false;
  }
  return true;
}

bool readLoopHead(const LoadCtx& c, const Json& e, const std::string& at, int& max) {
  if (!c.onlyKeys(e, {"loop", "max"}, at)) return false;
  if (!e["loop"].isArray() || e["loop"].size() == 0)
    return c.fail(at, "\"loop\" must be a list of commands (not empty) that runs again and again until a { \"break\": true } in it");
  max = kDefaultLoopMax;
  return c.integerIn(e, "max", at, 1, kMaxLoopMax, max);
}

bool isDialogueCommandIn(const Json& e, const char* key) {
  if (!e.has(key)) return false;
  return std::string(key) != "loop" || !e[key].isBool();
}

bool readBreak(const LoadCtx& c, const Json& e, const std::string& at, bool inLoop) {
  if (!c.onlyKeys(e, {"break"}, at)) return false;
  if (!e["break"].isBool() || !e["break"].asBool()) return c.fail(at, "write { \"break\": true }");
  if (!inLoop) return c.fail(at, "\"break\" ends a loop: it only works inside a { \"loop\": [ ... ] }");
  return true;
}

}  // namespace ote
