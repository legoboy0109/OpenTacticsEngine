#include "views.h"
#include "options/option_defs.h"

#include "platform/common/font_registry.h"

namespace ote {
namespace {

OptionChoice choice(const std::string& label, const Json& v = Json()) {
  OptionChoice c;
  c.label = label;
  c.value = v;
  return c;
}

// ---- font -----------------------------------------------------------------
std::vector<OptionChoice> fontChoices(const OptionContext& ctx) {
  std::vector<OptionChoice> out{choice("Game default")};
  for (const BuiltinFont& f : listBuiltinFonts(ctx.assetsDir))
    out.push_back(choice(f.id == "pixel" ? "Pixel (built-in)" : f.name, Json::makeString(f.id)));
  return out;
}
void applyFont(const Json& v, LookLayer& l) { l.font = v.asString(); }

// ---- text size ---------------------------------------------------------------
std::vector<OptionChoice> sizeChoices(const OptionContext&) {
  std::vector<OptionChoice> out{choice("Default")};
  for (int px : {8, 9, 10, 11, 12, 14, 16, 18, 20})
    out.push_back(choice(std::to_string(px) + " px", Json::makeNumber(px)));
  return out;
}
void applySize(const Json& v, LookLayer& l) { l.fontSize = v.asInt(0); }

// ---- UI scale ------------------------------------------------------------------
std::vector<OptionChoice> scaleChoices(const OptionContext&) {
  std::vector<OptionChoice> out{choice("100%")};
  for (int pct : {125, 150, 175, 200}) out.push_back(choice(std::to_string(pct) + "%", Json::makeNumber(pct)));
  return out;
}
void applyScale(const Json& v, LookLayer& l) { l.uiScale = v.asInt(0); }

// ---- visual style -------------------------------------------------------------
// Per-game screen: that game's own styles. Global screen: every style id of
// every installed game once (display name, or the id if games disagree).
std::vector<OptionChoice> styleChoices(const OptionContext& ctx) {
  std::vector<OptionChoice> out{choice("Game default")};
  if (ctx.game) {
    out[0].label = "Use global";
    for (const StyleInfo& s : ctx.game->info.styles)
      if (viewSupported(s.view)) out.push_back(choice(s.name, Json::makeString(s.id)));
    return out;
  }
  std::vector<std::string> ids, names;
  if (ctx.games)
    for (const GameEntry& g : *ctx.games)
      for (const StyleInfo& s : g.info.styles) {
        if (!viewSupported(s.view)) continue;  // this player cannot draw it
        size_t i = 0;
        while (i < ids.size() && ids[i] != s.id) ++i;
        if (i == ids.size()) { ids.push_back(s.id); names.push_back(s.name); }
        else if (names[i] != s.name) names[i] = s.id;  // names differ between games
      }
  for (size_t i = 0; i < ids.size(); ++i) out.push_back(choice(names[i], Json::makeString(ids[i])));
  return out;
}
void applyStyle(const Json& v, LookLayer& l) { l.style = v.asString(); }

}  // namespace

const std::vector<OptionDef>& optionDefs() {
  static const std::vector<OptionDef> defs = {
      {"font", "Font", fontChoices, applyFont},
      {"fontSize", "Text size", sizeChoices, applySize},
      {"uiScale", "UI scale", scaleChoices, applyScale},
      {"style", "Visual style", styleChoices, applyStyle},
  };
  return defs;
}

LookLayer globalLayer(const Settings& s) {
  LookLayer l;
  for (const OptionDef& d : optionDefs())
    if (!s.global(d.key).isNull()) d.apply(s.global(d.key), l);
  return l;
}

LookLayer overrideLayer(const Settings& s, const std::string& gameId) {
  LookLayer l;
  for (const OptionDef& d : optionDefs())
    if (s.hasOverride(gameId, d.key)) d.apply(s.get(gameId, d.key), l);
  return l;
}

bool sameValue(const Json& a, const Json& b) { return a.dump(0) == b.dump(0); }

}  // namespace ote
