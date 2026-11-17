// The sounds of the events the battle view plays (docs/SOUND.md "Events"). Hooks in battle_view.cpp call these:
//   soundStarted  an entry began playing (every kind except the two that sound mid-animation)
//   soundImpact   the moment an attack lands: hit / critical / miss
//   soundStep     the walking unit entered a new square of its path
//
// Only entries the view actually plays get here: under fog the entries the viewer did not see are handled by
// applyUnseen() and never begin, so unseen enemy actions are silent. A skipped animation finishes entries without
// calling soundStep / soundImpact for the part skipped, and GameAudio plays one identical sound per frame at
// most, so skipping a long turn cannot stack dozens of sounds.
#include "battle/battle_music.h"
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
      a.music().turnPhase(phaseInfo(b, e.a));  // "the enemy's turn" = a side no human on this device plays; per side and few-enemies too
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
    case EventType::StaffUsed: {  // a=user, b=item: the staff's own "use" sound, else heal (a healing staff, when the game has it), else itemUse
      CueCtx c;
      c.item = e.b;
      c.unitType = typeOf(e.a);
      const bool own = e.b >= 0 && (size_t)e.b < a.data().items.size() && a.data().items[(size_t)e.b].sound[(int)Cue::ItemUse] != kNone;
      const ItemDef* it = e.b >= 0 && (size_t)e.b < b.db->items.size() ? &b.db->items[(size_t)e.b] : nullptr;
      const bool heals = it && it->staff >= 0 && (size_t)it->staff < b.db->staves.size() && b.db->staves[(size_t)it->staff].has(StaffEffect::Kind::Heal);
      CueCtx plain;
      plain.unitType = c.unitType;
      // A healing staff with no sound of its own is heard through the heal it causes (UnitHealed plays the heal
      // sound once per healed unit); playing it here too would double it.
      if (own || !heals || !a.defines(Cue::Heal, plain)) a.cue(Cue::ItemUse, c);
      break;
    }
    case EventType::StaffMissed: {  // a=the unit missed, b=user, c=item
      CueCtx c;
      c.unitType = typeOf(e.b);
      if (a.defines(Cue::Miss, c)) a.cue(Cue::Miss, c);
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
    case EventType::UnitPromoted: a.music().jingle(MusicDirector::Jingle::Promotion); break;
    case EventType::StructureUpgraded: a.cue(Cue::Upgrade); break;
    case EventType::ResearchStarted:
      if (e.c > 0) a.cue(Cue::Research);  // an instant research is announced by its ResearchFinished
      break;
    case EventType::ResearchFinished: a.cue(Cue::Research); break;
    case EventType::UnitRepaired: a.cue(Cue::Repair); break;
    case EventType::UnitResupplied: a.cue(Cue::Supply); break;
    case EventType::MoveBlocked: a.cue(Cue::Ambush); break;
    // Story commands (docs/EVENTS.md): ids are opaque to the core, the player knows what they name.
    case EventType::MusicChanged:
      if (e.flags & kMusicPhase)  // setPhaseMusic (docs/SOUND.md "Music by situation"): a = the music (-1 = the override is gone), c = the side
        a.music().phaseOverride(e.c >= 0 && (size_t)e.c < b.sides.size() ? b.sides[(size_t)e.c].id : std::string(),
                                e.a >= 0 ? b.storyText(e.a) : std::string(), e.a < 0);
      else a.music().story(e.a >= 0 ? b.storyText(e.a) : std::string(), e.b);
      break;
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
  if (c.unitType >= 0 && (size_t)c.unitType < b.db->unitTypes.size()) c.moveType = b.db->unitTypes[(size_t)c.unitType].moveType;  // its movement type's step sound
  audio_->cue(Cue::Step, c);
}

}  // namespace ote
