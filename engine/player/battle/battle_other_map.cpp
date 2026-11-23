// A story scene on another map of the game (docs/EVENTS.md "Showing another map"): the battle screen's side.
//
// The rules have run the whole scene already; the shown map's entries sit in the view's queue between MapShown and MapReturned and name the
// units and squares of that map, not of the battle. When the view reaches MapShown the screen puts the battle's own map aside (its Battle,
// view, projection, 3D drawer and camera, in OtherMap) and puts the shown map in their place: the session's Battle becomes the shown map
// as the scene left it, a fresh view starts from the map as it was set up and plays those entries. So everything the screen can draw (the
// four kinds of view, the side-view battle scene, dialogue, the stage) works on the shown map unchanged. MapReturned swaps everything back
// and the battle's own view goes on after its MapShown. The scene's presentation (fade, background, standing portraits) and the player's
// skip carry over both ways, so a fade-out before showMap hides the change of map. Nothing of the shown map is saved: a save written while
// it is on screen (onSuspend) is the battle's own.
#include "battle/art_preload.h"
#include "battle/battle_scene.h"
#include "battle/map3d.h"
#include "core/base/log.h"

namespace ote {

struct BattleScene::OtherMap {
  Battle battle;  // the battle's own (the session holds the shown map's Battle while it is on screen)
  BattleView view;  // the battle's own view, waiting at its MapShown
  MapProj proj;
  std::unique_ptr<Map3D> map3d;
  Camera cam;
  FreeCam freeCam;
  Scroll scroll;
  int scrollSerial = 0, turn = 0;
  float turnPos = 0;
  Cell cursor = 0;
  Overlay overlay;
  std::shared_ptr<const MapDef> def;  // the shown map's data (the battle scene's backdrop by terrain)
  int index = -1;                     // into Battle::shownMaps
};

const Battle& BattleScene::ownBattle() const { return other_ ? other_->battle : battle_; }

void BattleScene::swapOwnBattle() {
  if (other_) std::swap(battle_, other_->battle);
}

const MapDef& BattleScene::shownMapDef() const { return other_ && other_->def ? *other_->def : session_.map; }

void BattleScene::updateOtherMap() {
  const int want = view_.mapRequest();
  if (want > 0 && !other_) showOtherMap();
  else if (want < 0 && other_) returnFromOtherMap();
  else if (want != 0) {  // (a MapReturned with nothing shown, or a MapShown while one is: never made by the rules)
    OTE_WARN("scene: a map entry out of place; passed over");
    view_.mapSwapped();
  }
}

void BattleScene::showOtherMap() {
  const int index = view_.mapIndex();
  std::vector<Event> run = view_.takeShownMapRun();
  if (index < 0 || index >= (int)battle_.shownMaps.size() || !battle_.shownMaps[(size_t)index].start || !battle_.shownMaps[(size_t)index].end) {
    OTE_WARN("scene: shown map %d is not known; its %d entries are passed over", index, (int)run.size());
    view_.mapSwapped();
    return;
  }
  const Battle::ShownMap sm = battle_.shownMaps[(size_t)index];
  const SceneStage stage = view_.stage;  // the fade, the background and the stage go on over the shown map
  const bool skipping = view_.skippingRun();
  const int dialogueSerial = view_.dialogue.serial, focusSerial = view_.focusSerial, fightSerial = view_.fight.serial;

  auto o = std::make_shared<OtherMap>();
  o->index = index;
  o->def = sm.def;
  o->battle = std::move(battle_);
  o->view = std::move(view_);
  o->proj = proj_;
  o->map3d = std::move(map3d_);
  o->cam = cam_;
  o->freeCam = freeCam_;
  o->scroll = scroll_;
  o->scrollSerial = scrollSerial_;
  o->turn = turn_;
  o->turnPos = turnPos_;
  o->cursor = cursor_;
  o->overlay = std::move(overlay_);
  other_ = o;

  // The shown map, set up the way the constructor sets up the battle's.
  battle_ = *sm.end;
  overlay_ = Overlay();
  turn_ = 0;
  turnPos_ = 0;
  proj_ = MapProj();
  proj_.setViewport(layout_.vw, layout_.vh);
  proj_.init(battle_, &session_.db.styles[styleIdx_], layout_.tile);
  if (proj_.view3d()) map3d_.reset(new Map3D(host_.renderer3d(), host_.show3d(), session_.dir, session_.db.graphics));
  if (host_.sprites() && !proj_.view3d()) preloadMapArt(*host_.sprites(), battle_, styleIdx_);
  view_ = BattleView();
  view_.init(*sm.start, layout_.tile, autoplay_);  // the units as the map placed them; the entries bring them to the end state
  view_.setAudio(&host_.gameAudio());
  int rx, ry;
  panelResourceAnchor(layout_, (int)session_.db.resources.size(), rx, ry);
  view_.setResourceAnchor(rx, ry);
  view_.stage = stage;
  view_.stage.setWeather(sm.start->storyState.weather);  // the shown map's own weather (a weather command of the scene changes it)
  view_.setSkipping(skipping);
  // The serials the screen follows keep counting up across the two views, so the first line / focus / fight of the shown map is new.
  view_.dialogue.serial = dialogueSerial;
  view_.focusSerial = focusSerial;
  view_.fight.serial = fightSerial;
  view_.startOnMap(battle_, sm.focus);
  cursor_ = battle_.grid.valid(sm.focus) ? sm.focus : 0;
  hovered_ = kNoCell;
  scroll_ = Scroll();
  cam_ = Camera();
  freeCam_.snap();
  float wx, wy;
  proj_.toWorld(view_.focusX, view_.focusY, wx, wy);
  if (!proj_.free3d()) cam_.centre(wx, wy, layout_, proj_.worldW(), proj_.worldH());
  scrollSerial_ = view_.focusSerial;  // the camera is there already: no scroll to it
  view_.push(run);
  OTE_INFO("scene: showing map %s (%d entries)", sm.def ? sm.def->id.c_str() : "?", (int)run.size());
}

void BattleScene::returnFromOtherMap() {
  std::shared_ptr<OtherMap> o = other_;
  const SceneStage stage = view_.stage;
  const bool skipping = view_.skippingRun();
  const int dialogueSerial = view_.dialogue.serial, focusSerial = view_.focusSerial, fightSerial = view_.fight.serial;
  if (fightScene_.active()) fightScene_.close();  // (never open here: MapReturned plays after the fights)

  battle_ = std::move(o->battle);
  view_ = std::move(o->view);
  proj_ = o->proj;
  map3d_ = std::move(o->map3d);
  cam_ = o->cam;
  freeCam_ = o->freeCam;
  scroll_ = o->scroll;
  scrollSerial_ = o->scrollSerial;
  turn_ = o->turn;
  turnPos_ = o->turnPos;
  cursor_ = o->cursor;
  overlay_ = std::move(o->overlay);
  other_.reset();
  hovered_ = kNoCell;

  const std::string weather = view_.stage.weather();  // the battle's own weather comes back with its map
  view_.stage = stage;
  view_.stage.setWeather(weather);
  view_.setSkipping(skipping);
  view_.dialogue.serial = std::max(view_.dialogue.serial, dialogueSerial);
  view_.fight.serial = std::max(view_.fight.serial, fightSerial);
  view_.focusSerial = std::max(view_.focusSerial, focusSerial);
  scrollSerial_ = view_.focusSerial;  // the camera is where the battle's map left it
  view_.mapSwapped();
  OTE_INFO("scene: back to the battle's map");
}

}  // namespace ote
