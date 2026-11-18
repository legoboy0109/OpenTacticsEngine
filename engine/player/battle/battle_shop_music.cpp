// The music of a shop screen (docs/SOUND.md "Shop music"): while a preparation shop's screens (buy, who gives, sell) are open the shop's own
// "music" plays, else the game's `use.shopMusic`; the map's shop (a unit next to a shop square) plays `shopMusic`. Leaving the shop lets the
// director fall back to what it played before (the preparation music, or the map's). Asked every frame; the director only acts on a change.
#include "battle/battle_prep_ui.h"
#include "battle/battle_scene.h"
#include "sound/game_audio.h"

namespace ote {

void BattleScene::updateShopMusic() {
  MusicDirector& music = host_.gameAudio().music();
  if (prep_) {
    using S = PrepUi::Screen;
    const S s = prep_->screen;
    const bool inShop = s == S::Shop || s == S::ShopFor || s == S::SellWho || s == S::SellItems;
    const PrepDef& def = *prep_->map.prep;
    const bool valid = inShop && prep_->shopIx >= 0 && prep_->shopIx < (int)def.shops.size();
    music.shop(valid, valid ? def.shops[(size_t)prep_->shopIx].music : std::string());
    return;
  }
  music.shop(mode_ == Mode::Items && itemScreen_ == ItemScreen::Shop, std::string());
}

}  // namespace ote
