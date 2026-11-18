#include "battle/fight_scene.h"

#include <algorithm>

#include "core/base/log.h"
#include "core/data/battle_pick.h"
#include "sound/game_audio.h"
#include "ui/text.h"

namespace ote {

namespace {

const Color kDamage(255, 245, 210), kCritColor(255, 215, 60), kMiss(190, 195, 215), kHeal(120, 255, 140), kHurt(255, 140, 120), kTag(255, 225, 110);

// Where a unit stood when the fight began: its square, or (a unit that fell) the square its UnitDied event names.
Cell cellOf(const Battle& b, const std::vector<Event>& run, int unit) {
  if ((size_t)unit < b.units.size() && b.units[(size_t)unit].cell != kNoCell) return b.units[(size_t)unit].cell;
  for (const Event& e : run)
    if (e.type == EventType::UnitDied && e.a == unit) return e.b;
  return kNoCell;
}

FighterTimes timesOf(const BattleSet& s) {
  FighterTimes t;
  for (int a = 0; a < kBattleAnims; ++a) {
    const BattleAnim& an = s.anims[a];
    t.spawnAt[a] = -1;
    if (!an.any()) continue;
    t.has[a] = true;
    t.total[a] = an.totalMs();
    t.hitAt[a] = an.startMs(an.hitFrame());
    if (an.spawn >= 0) t.spawnAt[a] = an.startMs(an.spawn);
    for (const auto& s : an.sounds) t.soundAt[a].push_back(std::make_pair(an.startMs(s.first), s.second));
  }
  return t;
}

}  // namespace

const BattleSet* FightScene::setOf(int side) const {
  const int s = fighter_[side].set;
  return s >= 0 ? &env_.battle->db->graphics.battle.sets[(size_t)s] : nullptr;
}

bool FightScene::start(const Env& env, const std::vector<Event>& run) {
  active_ = false;
  env_ = env;
  const Battle& b = *env.battle;
  const Database& db = *b.db;
  const Event* first = nullptr;
  for (const Event& e : run)
    if (e.type == EventType::Attack) {
      first = &e;
      break;
    }
  if (!first) return false;
  const int pair[2] = {first->a, first->b};
  for (int u : pair)
    if (u < 0 || (size_t)u >= b.units.size() || (size_t)u >= env.view->units.size()) return false;

  FightInfo info;
  Cell cells[2];
  for (int i = 0; i < 2; ++i) {
    const Unit& u = b.units[(size_t)pair[i]];
    FightUnitInfo f;
    f.unit = u.id;
    f.name = u.name;
    // The rules have applied the fight's experience already; the scene shows the fighter as it was when the fight began.
    auto gained = [&](const std::map<int, int>& m) { auto it = m.find(u.id); return it == m.end() ? 0 : it->second; };
    f.level = std::max(1, u.level - gained(env.levelsGained));
    f.hp = env.view->units[(size_t)u.id].hp;  // what the map shows now: the fight has not played yet
    f.maxHp = std::max(f.hp, std::max(1, b.maxHp(u) - gained(env.hpGained)));
    cells[i] = cellOf(b, run, u.id);
    f.x = cells[i] == kNoCell ? 0 : b.grid.x(cells[i]);
    f.human = !b.sides[(size_t)u.side].ai;
    info.units[u.id] = f;
  }
  info.distance = cells[0] != kNoCell && cells[1] != kNoCell ? std::max(1, b.grid.distance(cells[0], cells[1])) : 1;
  info.sidesMode = db.battleSceneSides;
  for (const SkillDef& s : db.skills) info.skillNames.push_back(s.name);
  for (const ConditionDef& c : db.conditions) info.conditionNames.push_back(c.name);
  for (const ItemDef& it : db.items) info.itemNames.push_back(it.name);
  script_ = buildFightScript(run, info);
  if (!script_.valid) return false;

  const BattleArt& art = db.graphics.battle;
  const GraphicsSet* gset = db.graphics.set(env.styleIndex);
  for (int s = 0; s < 2; ++s) {
    Fighter& f = fighter_[s];
    f = Fighter();
    const Unit& u = b.units[(size_t)script_.side[s].unit];
    f.unit = u.id;
    f.name = u.name;
    f.level = script_.side[s].level;
    f.maxHp = script_.side[s].maxHp;
    f.startHp = script_.side[s].hp;
    f.type = u.type;
    f.glyph = db.unitTypes[(size_t)u.type].glyph;
    f.color = Color::rgb(b.sides[(size_t)u.side].color);
    // The weapon this fighter struck with first (and so its group, which picks a set of its own).
    int weapon = -1;
    for (const FightStrike& st : script_.strikes)
      if (st.actor == s && st.weapon >= 0 && (size_t)st.weapon < db.weapons.size()) {
        weapon = st.weapon;
        break;
      }
    if (weapon >= 0) f.weapon = db.weapons[(size_t)weapon].name;
    const std::string group = weapon >= 0 ? db.weapons[(size_t)weapon].group : std::string();
    f.sideId = b.sides[(size_t)u.side].id;
    f.set = art.resolveSet(u.character, u.type, group, weapon, f.sideId);
    if (f.set >= 0) {
      f.setName = art.sets[(size_t)f.set].id;
      // A set whose stand sheet has no texture (the file failed to load) is as good as none.
      if (!env.sprites || env.sprites->texture(sheetOf(s, art.sets[(size_t)f.set].anims[BaStand])) == 0) f.set = -1;
    }
    if (f.set >= 0) f.times = timesOf(art.sets[(size_t)f.set]);
    if (gset && (size_t)u.type < gset->units.size() && env.sprites) {
      const Look& look = gset->units[(size_t)u.type].forSide(b.sides[(size_t)u.side].id);
      if (env.sprites->has(look)) f.look = &look;
    }
  }
  // Hit and crit percentages from the forecast of the fight as ordered; a game without hit / crit formulas (everything 100 / 0) shows none.
  if (env.forecasts) {
    const int ini = script_.side[script_.initiator].unit, other = script_.side[1 - script_.initiator].unit;
    auto it = env.forecasts->find(std::make_pair(ini, other));
    if (it != env.forecasts->end()) {
      const Forecast& fc = it->second;
      const bool showHit = fc.attack.hit < 100 || (fc.counter.can && fc.counter.hit < 100);
      const bool showCrit = fc.attack.crit > 0 || (fc.counter.can && fc.counter.crit > 0);
      const StrikeForecast* sf[2] = {&fc.attack, &fc.counter};
      Fighter& fi = fighter_[script_.initiator];
      Fighter& fo = fighter_[1 - script_.initiator];
      if (sf[0]->can && showHit) fi.hit = sf[0]->hit;
      if (sf[0]->can && showCrit) fi.crit = sf[0]->crit;
      if (sf[1]->can && showHit) fo.hit = sf[1]->hit;
      if (sf[1]->can && showCrit) fo.crit = sf[1]->crit;
    }
  }
  // Effects per strike (a spell or an arrow for the weapon in use).
  effectOfStrike_.clear();
  std::vector<EffectTimes> fx;
  for (const FightStrike& st : script_.strikes) {
    const std::string group = st.weapon >= 0 && (size_t)st.weapon < db.weapons.size() ? db.weapons[(size_t)st.weapon].group : std::string();
    int e = art.resolveEffect(st.weapon, group);
    if (e >= 0 && (!env.sprites || env.sprites->texture(art.effects[(size_t)e].anim.sheet) == 0)) e = -1;
    effectOfStrike_.push_back(e);
    EffectTimes t;
    if (e >= 0) {
      t.has = true;
      t.travel = art.effects[(size_t)e].travel;
      t.travelMs = art.effects[(size_t)e].travelMs;
      t.total = art.effects[(size_t)e].anim.totalMs();
    }
    fx.push_back(t);
  }
  FighterTimes times[2] = {fighter_[0].times, fighter_[1].times};
  tl_ = buildTimeline(script_, times, fx);

  // Background and platform (docs/BATTLE_SCENE.md "Which background and platform"): by the terrain under the struck fighter, or under the
  // attacker with game.json "battleBackgroundFrom": "attacker"; the map's own table first, then the game's.
  const int deciding = db.battleBackgroundFromAttacker ? script_.initiator : 1 - script_.initiator;
  const Cell where = cells[script_.side[deciding].unit == pair[0] ? 0 : 1];
  terrain_ = where != kNoCell ? (int)b.cells[(size_t)where].terrain : -1;
  const MapDef noMap;
  const MapDef& mapDef = env.map ? *env.map : noMap;
  background_ = pickBattleBackground(db, mapDef, terrain_);
  if (background_ >= 0 && (!env.sprites || env.sprites->texture(art.backgrounds[(size_t)background_].sheet) == 0)) background_ = -1;
  platform_ = pickBattlePlatform(db, mapDef, terrain_);
  platformFar_ = script_.ranged();
  if (platform_ >= 0 && (!env.sprites || env.sprites->texture(art.platforms[(size_t)platform_].sheet) == 0)) platform_ = -1;

  t_ = 0;
  nextBeat_ = 0;
  floats_.clear();
  flash_ = -10;
  skipped_ = false;
  active_ = true;
  OTE_INFO("battle scene: start %s (Lv %d) vs %s (Lv %d), %s, %d strike(s)", fighter_[0].name.c_str(), fighter_[0].level,
           fighter_[1].name.c_str(), fighter_[1].level,
           script_.ranged() ? ("ranged, " + intStr(script_.distance) + " squares").c_str() : "melee", (int)script_.strikes.size());
  if (background_ >= 0) OTE_INFO("battle scene: background \"%s\" (terrain %s)", art.backgrounds[(size_t)background_].id.c_str(), terrain_ >= 0 ? db.terrains[(size_t)terrain_].id.c_str() : "none");
  if (platform_ >= 0) OTE_INFO("battle scene: platform \"%s\"%s", art.platforms[(size_t)platform_].id.c_str(), platformFar_ && art.platforms[(size_t)platform_].hasFar ? " (far)" : "");
  OTE_INFO("battle scene: left %s, right %s", fighter_[0].name.c_str(), fighter_[1].name.c_str());
  for (int s = 0; s < 2; ++s)
    if (fighter_[s].hit >= 0 || fighter_[s].crit >= 0)
      OTE_INFO("battle scene: %s chances: hit %d%% crit %d%%", fighter_[s].name.c_str(), fighter_[s].hit, fighter_[s].crit);
  for (int s = 0; s < 2; ++s)
    OTE_INFO("battle scene: %s art: %s", fighter_[s].name.c_str(),
             fighter_[s].set >= 0 ? ("set \"" + fighter_[s].setName + "\"").c_str() : "map picture");
  // A side's own sheet (docs/BATTLE_SCENE.md "By side"): named only when it replaces the set's, so a game without any logs as before.
  for (int s = 0; s < 2; ++s)
    if (fighter_[s].set >= 0) {
      const BattleAnim& stand = art.sets[(size_t)fighter_[s].set].anims[BaStand];
      const int sheet = sheetOf(s, stand);
      if (sheet != stand.sheet)
        OTE_INFO("battle scene: %s sheet: \"%s\" (side %s)", fighter_[s].name.c_str(), db.graphics.sheets[(size_t)sheet].id.c_str(),
                 fighter_[s].sideId.c_str());
    }
  return true;
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

// One beat of the timeline: its log line (for tests), its sound and its floating text. `quiet` (a skipped scene): the log line only.
void FightScene::fire(const FightBeat& beat, bool quiet) {
  const Battle& b = *env_.battle;
  GameAudio* audio = quiet ? nullptr : env_.audio;
  const FightStrike* st = beat.strike >= 0 ? &script_.strikes[(size_t)beat.strike] : nullptr;
  const Fighter& who = fighter_[beat.side & 1];
  const Fighter& foe = fighter_[1 - (beat.side & 1)];
  auto weaponCtx = [&](int unitType) {
    CueCtx c;
    c.weapon = st ? st->weapon : -1;
    c.unitType = unitType;
    return c;
  };
  switch (beat.kind) {
    case BeatKind::Begin: break;
    case BeatKind::Sound:  // an animation's own sound at its frame (docs/BATTLE_SCENE.md "Sounds")
      OTE_INFO("battle scene: sound %s (%s)", beat.text.c_str(), who.name.c_str());
      if (audio) audio->sound(beat.text);
      break;
    case BeatKind::Tag:
      OTE_INFO("battle scene: skill %s (%s)", beat.text.c_str(), who.name.c_str());
      addFloat(beat.text, beat.side, kTag, 1.3, true);
      break;
    case BeatKind::StrikeStart:
      OTE_INFO("battle scene: strike %s -> %s%s%s", who.name.c_str(), foe.name.c_str(), st && st->counter ? " (counter)" : "",
               st && st->crit ? " (crit)" : "");
      break;
    case BeatKind::Hit: {  // who = the struck fighter
      const int striker = 1 - (beat.side & 1);
      OTE_INFO("battle scene: %s %s -> %s %d", beat.crit ? "crit" : "hit", fighter_[striker].name.c_str(), who.name.c_str(), beat.value);
      addFloat(intStr(beat.value), beat.side, kDamage, 0.95);
      if (beat.crit) {
        addFloat("CRIT!", beat.side, kCritColor, 1.0, false, 10);
        flash_ = t_;
      }
      if (st && st->survived) addFloat("Survived!", beat.side, Color(255, 150, 120), 1.2, false, 20);
      if (audio) {
        CueCtx c = weaponCtx(fighter_[striker].type);
        if (beat.crit && audio->defines(Cue::Critical, c)) audio->cue(Cue::Critical, c);
        else audio->cue(Cue::Hit, c);
      }
      break;
    }
    case BeatKind::Miss: {
      const int striker = 1 - (beat.side & 1);
      OTE_INFO("battle scene: miss %s -> %s", fighter_[striker].name.c_str(), who.name.c_str());
      addFloat("MISS", beat.side, kMiss, 0.9);
      if (audio) audio->cue(Cue::Miss, weaponCtx(fighter_[striker].type));
      break;
    }
    case BeatKind::Heal:
      OTE_INFO("battle scene: drain %s +%d", who.name.c_str(), beat.value);
      addFloat("+" + intStr(beat.value), beat.side, kHeal, 1.0);
      if (audio) {
        CueCtx c;
        c.unitType = who.type;
        audio->cue(Cue::Heal, c);
      }
      break;
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
      if (audio) {
        CueCtx c;
        c.unitType = who.type;
        audio->cue(Cue::Defeated, c);
      }
      break;
    case BeatKind::End:
      OTE_INFO("battle scene: end%s", skipped_ ? " (skipped)" : "");
      break;
  }
  (void)b;
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
