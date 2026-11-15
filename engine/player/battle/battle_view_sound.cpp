// The sounds of the events the battle view plays (docs/SOUND.md "Events"). Hooks in battle_view.cpp call these:
//   soundStarted  an entry began playing (every kind except the two that sound mid-animation)
//   soundImpact   the moment an attack lands: hit / critical / miss
//   soundStep     the walking unit entered a new square of its path
//
// Only entries the view actually plays get here: under fog the entries the viewer did not see are handled by
// applyUnseen() and never begin, so unseen enemy actions are silent. A skipped animation finishes entries without
// calling soundStep / soundImpact for the part skipped, and GameAudio plays one identical sound per frame at
// most, so skipping a long turn cannot stack dozens of sounds.
#include "battle/battle_view.h"
#include "sound/game_audio.h"

namespace ote {

void BattleView::soundStarted(const Battle& b) {
  if (!audio_) return;
  const Event& e = cur_.ev;
  GameAudio& a = *audio_;
  auto typeOf = [&](int unit) { return unit >= 0 && (size_t)unit < b.units.size() ? b.units[(size_t)unit].type : -1; };
  switch (e.type) {
    case EventType::TurnStarted:
      a.music().turnPhase(b.sides[(size_t)e.a].ai);  // "the enemy's turn" = a side no human on this device plays
      a.cue(Cue::TurnStart);
      break;
    case EventType::UnitMoved: stepped_ = -1; break;  // the steps come from soundStep as the unit walks
    case EventType::UnitDied: {
      CueCtx c;
      c.unitType = typeOf(e.a);
      a.cue(Cue::Defeated, c);
      break;
    }
    case EventType::UnitBuilt: a.cue(Cue::Build); break;
    case EventType::StructureCaptured: a.cue(Cue::Capture); break;
    case EventType::ItemUsed: {
      CueCtx c;
      c.item = e.b;
      c.unitType = typeOf(e.a);
      a.cue(Cue::ItemUse, c);
      break;
    }
    case EventType::UnitHealed: {
      CueCtx c;
      c.unitType = typeOf(e.a);
      a.cue(Cue::Heal, c);
      break;
    }
    case EventType::ResourceChanged:
      // The same entries that float "+N Funds" over the panel: the viewer's own gains.
      if (e.c > 0 && showsResources(b, e.a)) a.cue(Cue::Gold);
      break;
    case EventType::LevelUp: a.music().jingle(MusicDirector::Jingle::LevelUp); break;
    case EventType::StructureUpgraded: a.cue(Cue::Upgrade); break;
    case EventType::ResearchStarted:
      if (e.c > 0) a.cue(Cue::Research);  // an instant research is announced by its ResearchFinished
      break;
    case EventType::ResearchFinished: a.cue(Cue::Research); break;
    case EventType::UnitRepaired: a.cue(Cue::Repair); break;
    case EventType::UnitResupplied: a.cue(Cue::Supply); break;
    case EventType::MoveBlocked: a.cue(Cue::Ambush); break;
    // Story commands (docs/EVENTS.md): ids are opaque to the core, the player knows what they name.
    case EventType::MusicChanged: a.music().story(e.a >= 0 ? b.storyText(e.a) : std::string(), e.b); break;
    case EventType::SoundRequested: a.sound(b.storyText(e.a)); break;
    default: break;
  }
}

void BattleView::soundImpact(const Battle& b) {
  if (!audio_) return;
  const Event& e = cur_.ev;
  CueCtx c;
  c.weapon = e.c;
  c.unitType = e.a >= 0 && (size_t)e.a < b.units.size() ? b.units[(size_t)e.a].type : -1;  // the attacker's
  if (!(e.flags & kHit)) audio_->cue(Cue::Miss, c);
  else if ((e.flags & kCrit) && audio_->defines(Cue::Critical, c)) audio_->cue(Cue::Critical, c);
  else audio_->cue(Cue::Hit, c);
}

void BattleView::soundStep(const Battle& b, size_t index) {
  if (!audio_ || (int)index == stepped_) return;
  stepped_ = (int)index;
  CueCtx c;
  const int unit = cur_.ev.a;
  c.unitType = unit >= 0 && (size_t)unit < b.units.size() ? b.units[(size_t)unit].type : -1;
  audio_->cue(Cue::Step, c);
}

}  // namespace ote
