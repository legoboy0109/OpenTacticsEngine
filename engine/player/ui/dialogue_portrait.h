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
PortraitArt portraitArt(const Database& db, const Sprites* sprites, int styleIdx, const std::string& id);

}  // namespace ote
