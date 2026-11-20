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
      if (styleUsable(s)) out.push_back(choice(s.name, Json::makeString(s.id)));
    return out;
  }
  std::vector<std::string> ids, names;
  if (ctx.games)
    for (const GameEntry& g : *ctx.games)
      for (const StyleInfo& s : g.info.styles) {
        if (!styleUsable(s)) continue;  // this player cannot draw it
        size_t i = 0;
        while (i < ids.size() && ids[i] != s.id) ++i;
        if (i == ids.size()) { ids.push_back(s.id); names.push_back(s.name); }
        else if (names[i] != s.name) names[i] = s.id;  // names differ between games
      }
  for (size_t i = 0; i < ids.size(); ++i) out.push_back(choice(names[i], Json::makeString(ids[i])));
  return out;
}
void applyStyle(const Json& v, LookLayer& l) { l.style = v.asString(); }

#ifndef OTE_NXDK  // (the original Xbox has neither row: docs/XBOX_OG.md "3D")
// ---- what draws 3D (docs/RENDER3D.md) ----------------------------------------
// The software drawer works everywhere; OpenGL falls back to it by itself on
// a machine where it cannot be used.
std::vector<OptionChoice> draw3dChoices(const OptionContext& ctx) {
  return {choice(ctx.game ? "Use global" : "Automatic"), choice("OpenGL", Json::makeString("opengl")),
          choice("Software", Json::makeString("software"))};
}
void applyDraw3d(const Json& v, LookLayer& l) { l.draw3d = v.asString(); }

// ---- how big the 3D picture is (docs/RENDER3D.md "Resolution") ---------------
// Automatic: the display's resolution with OpenGL, the game's size with the
// (slower) software drawer.
std::vector<OptionChoice> res3dChoices(const OptionContext& ctx) {
  return {choice(ctx.game ? "Use global" : "Automatic"), choice("Display", Json::makeString("display")),
          choice("Game size", Json::makeString("game"))};
}
void applyRes3d(const Json& v, LookLayer& l) { l.res3d = v.asString(); }
#endif

// ---- battle animations (docs/BATTLE_SCENE.md "Settings") -----------------------------------------------------------
// The scene (side-view fighters) or the quick fight on the map; unset = what the game asks for.
std::vector<OptionChoice> battleSceneChoices(const OptionContext& ctx) {
  return {choice(ctx.game ? "Use global" : "Game default"), choice("Scene", Json::makeString("scene")),
          choice("On the map", Json::makeString("map"))};
}
void applyBattleScene(const Json& v, LookLayer& l) { l.battleScene = v.asString(); }

// ---- actions with nothing to act on (docs/SRPG.md "Menu options") -------------------------------------------------
// Greyed out in the action menu or left out of it; unset = what the game asks for.
std::vector<OptionChoice> menuUnavailableChoices(const OptionContext& ctx) {
  return {choice(ctx.game ? "Use global" : "Game default"), choice("Greyed out", Json::makeString("grey")),
          choice("Hidden", Json::makeString("hide"))};
}
void applyMenuUnavailable(const Json& v, LookLayer& l) { l.menuUnavailable = v.asString(); }

// ---- enemy ranges (docs/SRPG.md "Enemy ranges") --------------------------------------------------------------------
// What the danger-zone display of a marked enemy / "show all" draws. Unset = movement and attack; a game can switch the display off.
std::vector<OptionChoice> enemyRangesChoices(const OptionContext& ctx) {
  return {choice(ctx.game ? "Use global" : "Game default"), choice("Movement + attack", Json::makeString("both")),
          choice("Attack only", Json::makeString("attack")), choice("Off", Json::makeString("off"))};
}
void applyEnemyRanges(const Json& v, LookLayer& l) { l.enemyRanges = v.asString(); }

// ---- volumes (docs/SOUND.md "Options") ----------------------------------------
// 0..10; 0 = off (music is not even decoded). Element 0 is "not set": the default of 7, or the global value.
// Sliders (OptionDef::slider): element 1 + v is the value v.
std::vector<OptionChoice> volumeChoices(const OptionContext& ctx) {
  std::vector<OptionChoice> out{choice(ctx.game ? "Use global" : "7")};
  for (int v = 0; v <= 10; ++v) out.push_back(choice(v == 0 ? "Off" : std::to_string(v), Json::makeNumber(v)));
  return out;
}
void applyMusicVolume(const Json& v, LookLayer& l) { l.musicVolume = v.asInt(-1); }
void applySoundVolume(const Json& v, LookLayer& l) { l.soundVolume = v.asInt(-1); }

#ifdef OTE_NXDK
// ---- screen margin (original Xbox only, docs/XBOX_OG.md "Picture") ------------------------------------------------
// How much of the TV the picture uses; the rest is a black border for what an old TV's overscan hides. The picture
// resizes as the value changes, so the edges can be judged on the spot. Automatic = 90% on standard definition, 95% on HD.
std::vector<OptionChoice> safeAreaChoices(const OptionContext& ctx) {
  std::vector<OptionChoice> out{choice(ctx.game ? "Use global" : "Automatic")};
  for (int pct = 100; pct >= 76; pct -= 2) out.push_back(choice(std::to_string(pct) + "%", Json::makeNumber(pct)));
  return out;
}
void applySafeArea(const Json& v, LookLayer& l) { l.safeArea = v.asInt(0); }
#endif

}  // namespace

const std::vector<OptionDef>& optionDefs() {
  static const std::vector<OptionDef> defs = {
      {"font", "Font", fontChoices, applyFont},
      {"fontSize", "Text size", sizeChoices, applySize},
      {"uiScale", "UI scale", scaleChoices, applyScale},
      {"style", "Visual style", styleChoices, applyStyle},
#ifndef OTE_NXDK
      // Not on the original Xbox: its chip always draws 3D at the screen's resolution and software 3D is unusably slow there.
      {"draw3d", "3D graphics", draw3dChoices, applyDraw3d},
      {"res3d", "3D resolution", res3dChoices, applyRes3d},
#endif
      {"musicVolume", "Music volume", volumeChoices, applyMusicVolume, true, 7},
      {"soundVolume", "Sound volume", volumeChoices, applySoundVolume, true, 7},
      {"battleScene", "Battle animations", battleSceneChoices, applyBattleScene},
      {"menuUnavailable", "Unavailable actions", menuUnavailableChoices, applyMenuUnavailable},
      {"enemyRanges", "Enemy ranges", enemyRangesChoices, applyEnemyRanges},
#ifdef OTE_NXDK
      {"safeArea", "Screen margin", safeAreaChoices, applySafeArea},  // the one Xbox-only row (not in any other build)
#endif
      // (last, so the positions the existing option screens are driven by do not move)
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
