#include "options/effective.h"

namespace ote {
namespace {

bool isBuiltinName(const std::string& s) { return s.find_first_of("/\\.") == std::string::npos; }

// First layer (in priority order) that sets the field.
template <class Pick>
const LookLayer* firstSetting(const LookLayer* const (&layers)[3], Pick set) {
  for (const LookLayer* l : layers)
    if (set(*l)) return l;
  return nullptr;
}

}  // namespace

EffectiveLook resolveLook(const std::string& assetsDir, const LookLayer& args, const LookLayer& perGame,
                          const LookLayer& global, const GameLook& game) {
  const LookLayer* const layers[3] = {&args, &perGame, &global};
  EffectiveLook out;

  // --- font: which one
  FontChoice choice;  // engine default: the built-in pixel font
  std::string root;
  bool gameFont = false;
  if (const LookLayer* l = firstSetting(layers, [](const LookLayer& x) { return !x.font.empty(); })) {
    if (isBuiltinName(l->font)) choice.name = l->font;
    else choice.file = l->font;
  } else if (game.present) {
    choice = game.font;
    root = game.root;
    gameFont = true;
  }
  // --- font: size and smoothing
  if (const LookLayer* l = firstSetting(layers, [](const LookLayer& x) { return x.fontSize > 0; }))
    choice.size = l->fontSize;
  else if (!gameFont)
    choice.size = 0;  // the game's size belongs to the game's font only
  if (const LookLayer* l = firstSetting(layers, [](const LookLayer& x) { return x.smooth >= 0; }))
    choice.smooth = l->smooth;
  else if (!gameFont)
    choice.smooth = -1;

  out.font = resolveFont(assetsDir, root, choice);

  // --- UI scale multiplies the final size
  if (const LookLayer* l = firstSetting(layers, [](const LookLayer& x) { return x.uiScale > 0; }))
    out.uiScale = l->uiScale;
  out.font.size = (out.font.size * out.uiScale + 50) / 100;
  if (out.font.size < 4) out.font.size = 4;
  if (out.font.size > 128) out.font.size = 128;

  // --- style: the game's default is its first style this player can draw; a
  // game that lacks the chosen id (or whose chosen style cannot be drawn here)
  // keeps that default. With nothing drawable, index 0 is drawn top-down.
  auto usable = [&game](size_t i) { return i >= game.styleUsable.size() || game.styleUsable[i]; };
  for (size_t i = 0; i < game.styleIds.size(); ++i)
    if (usable(i)) { out.styleIndex = (int)i; break; }
  if (const LookLayer* l = firstSetting(layers, [](const LookLayer& x) { return !x.style.empty(); })) {
    for (size_t i = 0; i < game.styleIds.size(); ++i)
      if (game.styleIds[i] == l->style && usable(i)) out.styleIndex = (int)i;
  }
  // --- what draws 3D: anything but the known names is "let the player decide"
  if (const LookLayer* l = firstSetting(layers, [](const LookLayer& x) { return x.draw3d == "opengl" || x.draw3d == "software"; }))
    out.draw3d = l->draw3d;
  return out;
}

}  // namespace ote
