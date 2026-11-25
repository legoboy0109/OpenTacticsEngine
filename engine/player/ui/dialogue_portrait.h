// Looks a portrait up for the dialogue box: shared by the battle scene and the campaign's scenes.
#pragma once
#include <string>

#include "battle/sprites.h"
#include "core/data/database.h"
#include "ui/dialogue_box.h"

namespace ote {

// The portrait `id` of the game's graphics for the style in use, as a place in its sheet's texture.
// A missing portrait (no id, no art loaded, an id the graphics do not have) gives an empty PortraitArt:
// the box is then drawn without a picture.
// `face` N > 0 (docs/EVENTS.md "Portrait expressions") takes the N-th of the portrait's "expressions" instead of its cell (no talk /
// blink frames then); a portrait with fewer shows its own cell.
PortraitArt portraitArt(const Database& db, const Sprites* sprites, int styleIdx, const std::string& id, int face = 0);

// The portrait as it looks now (docs/GRAPHICS.md "Portraits"): the "talk" frames cycle while `talking`, a "blink" frame shows for a moment
// every few seconds (when the times are worked out from `clock` seconds and `key`: the same on every machine, nothing random). A
// portrait with neither is returned as it is.
PortraitArt portraitFrame(const PortraitArt& art, bool talking, double clock, const std::string& key);
constexpr int kTalkFrameMs = 140, kBlinkMs = 130, kBlinkMinMs = 2800, kBlinkSpreadMs = 2600;

}  // namespace ote
