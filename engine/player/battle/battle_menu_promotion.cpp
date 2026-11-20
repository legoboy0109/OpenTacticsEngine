// Promotion in the battle scene (docs/SRPG.md "Player UI > Promotion", step U2): the Promote entry of the action menu,
// the question box (one possible promotion) and the promotion screen (several), for both ways in: the Promote action
// (an offer at a level) and a promotion item picked in the item list.
//
//   Promote / a promotion item -> openPromotion(slot, types)
//       one type   -> ConfirmBox "Promote Kell to Captain?" + the changes   (Yes submits, No / Cancel goes back)
//       several    -> PromotionScreen, Confirm submits the highlighted type, Cancel goes back
//   submit: slot < 0 -> Action::promote(unit, type);  slot >= 0 -> Action::useItem(unit, slot, unit) with unitType = type
// Back = the action menu (Promote) or the item list (item). The result plays as UnitPromoted (battle_view_promotion.cpp).
#include "battle/battle_scene.h"
#include "battle/promotion_text.h"
#include "battle/sprites.h"
#include "core/base/log.h"
#include "ui/text.h"

namespace ote {

void BattleScene::addPromotionEntries(std::vector<MenuItem>& items) {
  if (sel_ < 0) return;
  std::vector<int> types;
  battle_.promotionOptions(sel_, -1, types);
  // Shown only when it can be done now (a unit that may only move or wait gets no Promote entry).
  if (types.empty() || !battle_.validate(Action::promote(sel_, types[0]))) return;
  items.push_back({"Promote", "", true, kPromote});
  std::string names;  // the log line is what the script tests read
  for (int t : types) names += (names.empty() ? "" : ", ") + battle_.db->unitTypes[(size_t)t].name;
  OTE_INFO("promote entry for %s: %s", battle_.units[(size_t)sel_].name.c_str(), names.c_str());
}

bool BattleScene::promotionChosen(int id) {
  if (id != kPromote || sel_ < 0) return false;
  std::vector<int> types;
  battle_.promotionOptions(sel_, -1, types);
  if (types.empty()) return false;
  openPromotion(-1, types);
  return true;
}

// Called first thing by chooseUseTarget (battle_items.cpp): a promotion item used by a unit that can promote with it
// goes through the question / screen instead of the "Use on" list. True = taken.
bool BattleScene::promotionItemUse(int slot) {
  std::vector<int> types, targets;
  battle_.promotionOptions(sel_, slot, types);
  if (types.empty()) return false;
  battle_.useTargets(sel_, slot, targets);
  bool onSelf = false;
  for (int t : targets) onSelf = onSelf || t == sel_;
  if (!onSelf) return false;
  openPromotion(slot, types);
  return true;
}

void BattleScene::openPromotion(int slot, const std::vector<int>& types) {
  if (slot >= 0) closeItems();
  menu_.close();
  overlay_ = Overlay();
  mode_ = Mode::Promotion;
  promoSlot_ = slot;
  std::vector<PromotionPreview> previews;
  for (int t : types) previews.push_back(previewPromotion(battle_, sel_, slot, t));
  promoAsk_ = previews.size() == 1;
  if (promoAsk_) {
    promoType_ = previews[0].to;
    confirm_.open(promotionQuestion(battle_, sel_, previews[0]), "Yes", "No");
  } else {
    promoScreen_.open("Promote " + battle_.units[(size_t)sel_].name, previews);
  }
  std::string names;  // the log line is what the script tests read
  for (const PromotionPreview& p : previews) names += (names.empty() ? "" : ", ") + p.name;
  OTE_INFO("promotion %s: %s", promoAsk_ ? "question" : "screen", names.c_str());
}

void BattleScene::promotionBack() {
  confirm_.close();
  promoScreen_.close();
  if (promoSlot_ >= 0) openItems(promoSlot_);
  else openActionMenu();
}

void BattleScene::promotionInput(const InputEvent& e) {
  Renderer& r = host_.renderer();
  int type = -1;
  if (promoAsk_) {
    ConfirmBox::Result res = confirm_.onInput(e, r);
    if (res == ConfirmBox::None) return;
    if (res == ConfirmBox::No) { promotionBack(); return; }
    type = promoType_;
  } else {
    promoScreen_.setArea(0, 0, layout_.vw, layout_.vh);
    PromotionScreen::Result res = promoScreen_.onInput(e, r);
    if (res == PromotionScreen::None) return;
    if (res == PromotionScreen::Cancelled) { promotionBack(); return; }
    type = promoScreen_.selectedType();
  }
  confirm_.close();
  promoScreen_.close();
  Action a = promoSlot_ >= 0 ? Action::useItem(sel_, promoSlot_, sel_) : Action::promote(sel_, type);
  a.unitType = type;  // for the item: which promotion it triggers
  if (submit(a)) finishOrder();  // either way costs the unit's turn
  else promotionBack();
}

// The picture of a unit type in the promotion screen: its art in the unit's side colours. A style whose units are
// models has no picture for it (the models only exist inside the 3D scene), so the first other style of the game
// that has a picture of the type lends its own (the game's first style first: normally the plain top-down one).
// Only a game without any picture of the type shows the placeholder (side-coloured block with the type's letter).
void BattleScene::drawPromotionPicture(Renderer& r, int type, int x, int y, int w, int h) {
  const Unit& u = battle_.units[(size_t)sel_];
  const Database& db = session_.db;
  DrawCtx ctx{battle_, view_, layout_, cam_, time_, &db.styles[styleIdx_], host_.sprites(), styleIdx_, &proj_, host_.isoTiles(), map3d_.get()};
  Color side = Color::rgb(battle_.sides[(size_t)u.side].color);
  for (int k = -1; k < (int)db.styles.size(); ++k) {  // -1 = the style being played
    const int style = k < 0 ? styleIdx_ : k;
    if (k == styleIdx_) continue;
    const GraphicsSet* set = db.graphics.set(style);
    if (!set || type < 0 || (size_t)type >= set->units.size()) continue;
    const Look& look = set->units[(size_t)type].forSide(battle_.sides[(size_t)u.side].id);
    if (!ctx.sprites || !ctx.sprites->has(look)) continue;
    SpriteSrc s = lookSrc(ctx, look);
    int dw = w - 4, dh = std::max(1, dw * s.h / std::max(1, s.w));  // keep the cell's proportions
    if (dh > h - 4) { dh = h - 4; dw = std::max(1, dh * s.w / std::max(1, s.h)); }
    r.fillRect(x + 2, y + h - 4, w - 4, 2, side);  // the side marker under the feet
    r.drawTexture(s.tex, s.x, s.y, s.w, s.h, x + (w - dw) / 2, y + h - 3 - dh, dw, dh, Color(255, 255, 255));
    return;
  }
  r.fillRect(x + 4, y + 4, w - 8, h - 8, side);
  drawGlyphCentered(r, x + w / 2, y + h / 2, db.unitTypes[(size_t)type].glyph, readableOn(side), std::max(1, (w - 8) * 3 / 4));
}

void BattleScene::drawPromotion(Renderer& r) {
  if (promoAsk_) {
    confirm_.draw(r);
    return;
  }
  promoScreen_.setArea(0, 0, layout_.vw, layout_.vh);
  promoScreen_.draw(r, [this](Renderer& rr, int t, int x, int y, int w, int h) { drawPromotionPicture(rr, t, x, y, w, h); });
}

std::string BattleScene::promotionHint() const {
  return promoAsk_ ? "{lrok}:answer  {cancel}:no" : "{udok}:choose  {cancel}:back";
}

}  // namespace ote
