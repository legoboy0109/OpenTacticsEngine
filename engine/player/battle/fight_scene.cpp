#include "battle/fight_scene.h"

#include <algorithm>
#include <cmath>

#include "core/base/log.h"
#include "sound/game_audio.h"
#include "ui/text.h"

namespace ote {

namespace {

const Color kDamage(255, 245, 210), kCritColor(255, 215, 60), kMiss(190, 195, 215), kHeal(120, 255, 140), kHurt(255, 140, 120), kTag(255, 225, 110);

// The cue context of a blow: the weapon of the strike and the striker's unit type.
CueCtx weaponCue(const FightStrike* st, int unitType) {
  CueCtx c;
  c.weapon = st ? st->weapon : -1;
  c.unitType = unitType;
  return c;
}

}  // namespace

const BattleSet* FightScene::setOf(int side) const {
  const int s = fighter_[side].set;
  return s >= 0 ? &env_.battle->db->graphics.battle.sets[(size_t)s] : nullptr;
}

void FightScene::addFloat(const std::string& text, int side, Color c, double life, bool plate, int rise) {
  Float f;
  f.text = text;
  f.side = side;
  f.born = t_;
  f.life = life;
  f.color = c;
  f.plate = plate;
  f.rise = rise;
  floats_.push_back(f);
}

// The staff's own sound (as on the map: its use sound, else heal when it heals, else itemUse); the beat has no log line.
void FightScene::fireStaffUse(const BeatCtx& x) {
  if (!x.audio) return;
  const Battle& b = *env_.battle;
  const ItemDef& it = b.db->items[(size_t)x.st->item];
  CueCtx c, plain;
  c.item = x.st->item;
  c.unitType = x.who.type;
  plain.unitType = x.who.type;
  const bool own = x.st->item >= 0 && (size_t)x.st->item < x.audio->data().items.size() && x.audio->data().items[(size_t)x.st->item].sound[(int)Cue::ItemUse] != kNone;
  const bool heals = it.staff >= 0 && b.db->staves[(size_t)it.staff].has(StaffEffect::Kind::Heal);
  if (own || !heals || !x.audio->defines(Cue::Heal, plain)) x.audio->cue(Cue::ItemUse, c);
}

void FightScene::fireHit(const BeatCtx& x) {  // who = the struck fighter
  const FightBeat& beat = x.beat;
  const int striker = 1 - (beat.side & 1);
  OTE_INFO("battle scene: %s %s -> %s %d", beat.crit ? "crit" : "hit", fighter_[striker].name.c_str(), x.who.name.c_str(), beat.value);
  addFloat(intStr(beat.value), beat.side, kDamage, 0.95);
  if (beat.crit) {
    addFloat("CRIT!", beat.side, kCritColor, 1.0, false, 10);
    flash_ = t_;
  }
  if (x.st && x.st->survived) addFloat("Survived!", beat.side, Color(255, 150, 120), 1.2, false, 20);
  if (x.audio) {
    CueCtx c = weaponCue(x.st, fighter_[striker].type);
    if (beat.crit && x.audio->defines(Cue::Critical, c)) x.audio->cue(Cue::Critical, c);
    else x.audio->cue(Cue::Hit, c);
  }
}

void FightScene::fireMiss(const BeatCtx& x) {
  const int striker = 1 - (x.beat.side & 1);
  OTE_INFO("battle scene: miss %s -> %s", fighter_[striker].name.c_str(), x.who.name.c_str());
  addFloat("MISS", x.beat.side, kMiss, 0.9);
  if (x.audio) x.audio->cue(Cue::Miss, weaponCue(x.st, fighter_[striker].type));
}

void FightScene::fireStaffHeal(const BeatCtx& x) {  // who = the target
  OTE_INFO("battle scene: staff heal %s +%d", x.who.name.c_str(), x.beat.value);
  addFloat("+" + intStr(x.beat.value), x.beat.side, kHeal, 1.0);
  if (x.audio) {
    CueCtx c;
    c.item = x.st ? x.st->item : -1;  // the staff's own "heal" sound (or its "healScene"), or "" for none (docs/SOUND.md)
    c.scene = true;
    c.unitType = x.who.type;
    x.audio->cue(Cue::Heal, c);
  }
}

void FightScene::fireStaffMiss(const BeatCtx& x) {
  OTE_INFO("battle scene: staff miss %s", x.who.name.c_str());
  addFloat("MISS", x.beat.side, kMiss, 0.9);
  if (x.audio) {
    CueCtx c;
    c.unitType = x.foe.type;  // (the user's)
    if (x.audio->defines(Cue::Miss, c)) x.audio->cue(Cue::Miss, c);
  }
}

void FightScene::fireDrain(const BeatCtx& x) {
  OTE_INFO("battle scene: drain %s +%d", x.who.name.c_str(), x.beat.value);
  addFloat("+" + intStr(x.beat.value), x.beat.side, kHeal, 1.0);
  if (x.audio) {
    CueCtx c;
    c.unitType = x.who.type;
    x.audio->cue(Cue::Heal, c);
  }
}

// One beat of the timeline: its log line (for tests), its sound and its floating text. `quiet` (a skipped scene): the log line only.
void FightScene::fire(const FightBeat& beat, bool quiet) {
  const FightStrike* st = beat.strike >= 0 ? &script_.strikes[(size_t)beat.strike] : nullptr;
  const BeatCtx x{beat, st, fighter_[beat.side & 1], fighter_[1 - (beat.side & 1)], quiet ? nullptr : env_.audio};
  const Fighter& who = x.who;
  switch (beat.kind) {
    case BeatKind::Begin: break;
    case BeatKind::Sound:  // an animation's own sound at its frame (docs/BATTLE_SCENE.md "Sounds")
      OTE_INFO("battle scene: sound %s (%s)", beat.text.c_str(), who.name.c_str());
      if (x.audio) x.audio->sound(beat.text);
      break;
    case BeatKind::FxSound:  // an effect's own sound, a set time after the effect starts (docs/BATTLE_SCENE.md "Effect sounds")
      OTE_INFO("battle scene: sound %s (effect %s)", beat.text.c_str(), beat.label.c_str());
      if (x.audio) x.audio->sound(beat.text);
      break;
    case BeatKind::FxStart:
      OTE_INFO("battle scene: effect %s start", beat.label.c_str());
      break;
    case BeatKind::FxFlight:
      OTE_INFO("battle scene: effect %s flight %d ms", beat.label.c_str(), beat.value);
      break;
    case BeatKind::Tag:
      OTE_INFO("battle scene: skill %s (%s)", beat.text.c_str(), who.name.c_str());
      addFloat(beat.text, beat.side, kTag, 1.3, true);
      break;
    case BeatKind::StrikeStart:
      if (st && st->staff) {
        fireStaffUse(x);
        break;
      }
      OTE_INFO("battle scene: strike %s -> %s%s%s", who.name.c_str(), x.foe.name.c_str(), st && st->counter ? " (counter)" : "",
               st && st->crit ? " (crit)" : "");
      break;
    case BeatKind::Hit: fireHit(x); break;
    case BeatKind::Miss: fireMiss(x); break;
    case BeatKind::StaffHeal: fireStaffHeal(x); break;
    case BeatKind::StaffMiss: fireStaffMiss(x); break;
    case BeatKind::StaffNote:
      OTE_INFO("battle scene: staff %s %s (%s)", beat.value == 1 ? "repair" : "condition", beat.text.c_str(), who.name.c_str());
      addFloat(beat.text, beat.side, kTag, 1.3, true, 12);  // (above the "+N" that came first)
      break;
    case BeatKind::Heal: fireDrain(x); break;
    case BeatKind::Hurt:
      OTE_INFO("battle scene: recoil %s -%d", who.name.c_str(), beat.value);
      addFloat("-" + intStr(beat.value), beat.side, kHurt, 0.9);
      break;
    case BeatKind::Broke:
      OTE_INFO("battle scene: %s", beat.text.c_str());
      addFloat(beat.text, beat.side, Color(255, 170, 110), 1.2, false, 20);
      break;
    case BeatKind::Defeated:
      OTE_INFO("battle scene: defeated %s", who.name.c_str());
      if (x.audio) {
        CueCtx c;
        c.unitType = who.type;
        x.audio->cue(Cue::Defeated, c);
      }
      break;
    case BeatKind::End:
      OTE_INFO("battle scene: end%s", skipped_ ? " (skipped)" : "");
      break;
  }
}

void FightScene::update(double dt) {
  if (!active_) return;
  t_ += dt;
  while (nextBeat_ < tl_.beats.size() && tl_.beats[nextBeat_].t <= t_) fire(tl_.beats[nextBeat_++], false);
  if (t_ >= tl_.total) t_ = tl_.total;
  floats_.erase(std::remove_if(floats_.begin(), floats_.end(), [&](const Float& f) { return t_ - f.born >= f.life; }), floats_.end());
}

void FightScene::skip() {
  if (!active_ || t_ >= tl_.total) return;
  skipped_ = true;
  while (nextBeat_ < tl_.beats.size()) fire(tl_.beats[nextBeat_++], true);
  t_ = tl_.total;
  floats_.clear();
}

}  // namespace ote
