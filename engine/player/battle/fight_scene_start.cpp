// Preparing a fight for the scene (FightScene::start and its steps): who fights, their art, the forecast chances, the effects, the
// timeline, the backdrop. The beats and the clock are in fight_scene.cpp.
#include <algorithm>
#include <cmath>

#include "battle/fight_scene.h"
#include "core/base/log.h"
#include "core/data/battle_pick.h"
#include "core/data/battle_stage.h"
#include "ui/text.h"

namespace ote {

namespace {

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
    for (const auto& snd : an.sounds) t.soundAt[a].push_back(std::make_pair(an.startMs(snd.first), snd.second));
    // Holding a pose (docs/BATTLE_SCENE.md "Holding a pose").
    if (an.holdFrame >= 0) {
      t.hold[a] = true;
      t.holdAt[a] = an.startMs(an.holdFrame);
      t.holdLen[a] = an.ms[(size_t)an.holdFrame];
    }
    if (an.startFrame >= 0) {
      t.start[a] = true;
      t.startAt[a] = an.startMs(an.startFrame);
      if (an.startFrame >= 1) {  // the frames before it are the brace; the last of them is the one held until the miss
        t.brace[a] = true;
        t.braceAt[a] = an.startMs(an.startFrame - 1);
        t.braceLen[a] = an.ms[(size_t)an.startFrame - 1];
      }
    }
    t.startDelay[a] = an.startDelayMs;  // the dodge proper starts this long after the miss
  }
  return t;
}

// The first strike of the run (or the staff use that opens it) and the two units it names; false when the run holds none or a unit is unknown.
bool findFightPair(const FightScene::Env& env, const std::vector<Event>& run, int pair[2]) {
  const Battle& b = *env.battle;
  const Event* first = nullptr;
  const bool staffUse = !run.empty() && run[0].type == EventType::StaffUsed;  // a staff used on one unit (docs/BATTLE_SCENE.md "Staves in the scene")
  if (staffUse) first = &run[0];
  else
    for (const Event& e : run)
      if (e.type == EventType::Attack) {
        first = &e;
        break;
      }
  if (!first) return false;
  pair[0] = first->a;
  pair[1] = staffUse ? first->c : first->b;
  for (int i = 0; i < 2; ++i)
    if (pair[i] < 0 || (size_t)pair[i] >= b.units.size() || (size_t)pair[i] >= env.view->units.size()) return false;
  return true;
}

// What the script builder needs to know about the two fighters as they were when the fight began; `cells` gets their squares.
FightInfo gatherFightInfo(const FightScene::Env& env, const std::vector<Event>& run, const int pair[2], Cell cells[2]) {
  const Battle& b = *env.battle;
  const Database& db = *b.db;
  FightInfo info;
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
  return info;
}

// The first weapon the unit holds (what it would attack with), or -1.
int heldWeapon(const Battle& b, const Unit& u) {
  std::vector<int> held;
  b.weaponsOf(u, held);
  return !held.empty() && held[0] >= 0 && (size_t)held[0] < b.db->weapons.size() ? held[0] : -1;
}

// The weapon this side struck with first (and so its group, which picks a set of its own), or -1.
int firstStrikeWeapon(const FightScript& script, const Database& db, int side) {
  for (const FightStrike& st : script.strikes)
    if (st.actor == side && st.weapon >= 0 && (size_t)st.weapon < db.weapons.size()) return st.weapon;
  return -1;
}

// An effect's timing facts for the timeline. "speed" (stage pixels per second): the flight takes as long as the distance between the
// two foot points needs, so it is worked out here where the stage layout is known.
EffectTimes effectTimes(const BattleEffect& be, const Database& db, const FightStrike& st, int distance, bool stageOn, const StageLayout& lay) {
  EffectTimes t;
  t.has = true;
  t.travel = be.travel;
  t.travelMs = be.travelMs;
  t.total = be.anim.totalMs();
  t.hitAtMs = be.hitAtMs;
  t.striker = be.atStriker;
  t.returns = be.returns;
  t.lingerMs = be.lingerMs;
  t.extended = be.extended;
  t.delayMs = be.delayAt(distance);
  t.rangeFixed = be.rangeFixed;
  t.panAtMs = be.panAtMs;
  t.sounds = be.sounds;
  t.flash = be.flash;
  t.shake = be.shake;
  t.hide = be.hide;
  t.fade = be.fade;
  if (be.travel && be.speed > 0 && stageOn) {
    const int to = 1 - st.actor;
    const double dx = lay.feet[to][0] - lay.feet[st.actor][0], dy = lay.feet[to][1] - lay.feet[st.actor][1];
    t.travelMs = std::max(20, std::min(5000, (int)std::lround(std::sqrt(dx * dx + dy * dy) * 1000.0 / be.speed)));
    t.bySpeed = true;
  }
  t.name = !be.id.empty() ? be.id : be.weapon >= 0 ? db.weapons[(size_t)be.weapon].id : be.item >= 0 ? db.items[(size_t)be.item].id : be.group;
  return t;
}

}  // namespace

bool FightScene::start(const Env& env, const std::vector<Event>& run) {
  active_ = false;
  env_ = env;
  const Battle& b = *env.battle;
  const Database& db = *b.db;
  int pair[2];
  if (!findFightPair(env, run, pair)) return false;
  Cell cells[2];
  script_ = buildFightScript(run, gatherFightInfo(env, run, pair, cells));
  if (!script_.valid) return false;

  for (int s = 0; s < 2; ++s) initFighter(s);
  applyForecast();
  const BattleArt& art = db.graphics.battle;
  const bool stageOn = art.stage.on;
  const StageLayout lay = stageOn ? stageLayoutFor(art.stage, script_.distance) : StageLayout();  // (range bands: the points of this fight's distance)
  const std::vector<std::vector<EffectTimes>> fx = collectEffects(stageOn, lay);
  FighterTimes times[2] = {fighter_[0].times, fighter_[1].times};
  tl_ = buildTimelineList(script_, times, fx, stageOn && lay.each ? lay.panMs : -1,
                         stageOn && lay.each ? lay.panStartMs : -1, art.timing.resultHoldMs, art.timing.resultHoldAgainMs);
  pickBackdrop(cells, pair);

  t_ = 0;
  nextBeat_ = 0;
  floats_.clear();
  flash_ = -10;
  skipped_ = false;
  active_ = true;
  logStart();
  return true;
}

// One fighter of the scene: name, weapon, art set. The set is checked against the loaded textures here, so a missing sheet falls back to
// the map picture before anything draws.
void FightScene::initFighter(int s) {
  const Battle& b = *env_.battle;
  const Database& db = *b.db;
  const BattleArt& art = db.graphics.battle;
  const GraphicsSet* gset = db.graphics.set(env_.styleIndex);
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
  int weapon = firstStrikeWeapon(script_, db, s);
  // A staff use: the user shows (and picks its set by) the staff; the target shows the weapon it holds, which picks its set as in a fight.
  std::string staffGroup;
  if (script_.staff() && script_.strikes[0].actor == s) {
    const int item = script_.strikes[0].item;
    if (item >= 0 && (size_t)item < db.items.size()) {
      f.weapon = db.items[(size_t)item].name;
      const int sd = db.items[(size_t)item].staff;
      if (sd >= 0 && (size_t)sd < db.staves.size()) staffGroup = db.staves[(size_t)sd].gear.group;
    }
  } else if (script_.staff()) {
    const int held = heldWeapon(b, u);
    if (held >= 0) weapon = held;
  }
  if (f.weapon.empty()) {
    if (weapon >= 0) f.weapon = db.weapons[(size_t)weapon].name;
    else {  // never struck (felled before its counter): the box still names the weapon it holds, the first it would attack with
      const int held = heldWeapon(b, u);
      if (held >= 0) f.weapon = db.weapons[(size_t)held].name;
    }
  }
  const std::string group = weapon >= 0 ? db.weapons[(size_t)weapon].group : staffGroup;
  f.sideId = b.sides[(size_t)u.side].id;
  f.set = art.resolveSet(u.character, u.type, group, weapon, f.sideId);
  if (f.set >= 0) {
    f.setName = art.sets[(size_t)f.set].id;
    // A set whose stand sheet has no texture (the file failed to load) is as good as none.
    if (!env_.sprites || env_.sprites->texture(sheetOf(s, art.sets[(size_t)f.set].anims[BaStand])) == 0) f.set = -1;
  }
  if (f.set >= 0) f.times = timesOf(art.sets[(size_t)f.set]);
  if (gset && (size_t)u.type < gset->units.size() && env_.sprites) {
    const Look& look = gset->units[(size_t)u.type].forSide(b.sides[(size_t)u.side].id);
    if (env_.sprites->has(look)) f.look = &look;
  }
}

// Hit and crit percentages from the forecast of the fight as ordered; a game without hit / crit formulas (everything 100 / 0) shows none.
void FightScene::applyForecast() {
  if (!env_.forecasts || script_.staff()) return;
  const int ini = script_.side[script_.initiator].unit, other = script_.side[1 - script_.initiator].unit;
  auto it = env_.forecasts->find(std::make_pair(ini, other));
  if (it == env_.forecasts->end()) return;
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

// Effects per strike (a spell or an arrow for the weapon in use), as the timeline's input; effectsOfStrike_ keeps which effects were kept.
// (docs/BATTLE_SCENE.md "Several effects, by unit type": the strike's list, picked by the weapon and the striker's unit type.)
std::vector<std::vector<EffectTimes>> FightScene::collectEffects(bool stageOn, const StageLayout& lay) {
  const Database& db = *env_.battle->db;
  const BattleArt& art = db.graphics.battle;
  effectsOfStrike_.clear();
  std::vector<std::vector<EffectTimes>> fx;
  for (const FightStrike& st : script_.strikes) {
    std::string group = st.weapon >= 0 && (size_t)st.weapon < db.weapons.size() ? db.weapons[(size_t)st.weapon].group : std::string();
    if (st.staff && st.item >= 0 && (size_t)st.item < db.items.size() && db.items[(size_t)st.item].staff >= 0)
      group = db.staves[(size_t)db.items[(size_t)st.item].staff].gear.group;
    std::vector<int> picked, kept;
    picked = art.resolveEffects(st.weapon, group, fighter_[st.actor].type, st.staff ? st.item : -1);
    std::vector<EffectTimes> times;
    for (int e : picked) {
      const BattleEffect& be = art.effects[(size_t)e];
      bool ok = env_.sprites != nullptr;  // an effect whose pictures did not load is as good as none
      for (size_t f = 0; ok && f < be.anim.frames.size(); ++f) ok = env_.sprites->texture(be.anim.sheetOfFrame((int)f)) != 0;
      if (!ok) continue;
      kept.push_back(e);
      times.push_back(effectTimes(be, db, st, script_.distance, stageOn, lay));
    }
    effectsOfStrike_.push_back(kept);
    fx.push_back(times);
  }
  return fx;
}

// Background and platform (docs/BATTLE_SCENE.md "Which background and platform"): by the terrain under the struck fighter, or under the
// attacker with game.json "battleBackgroundFrom": "attacker"; the map's own table first, then the game's.
void FightScene::pickBackdrop(const Cell cells[2], const int pair[2]) {
  const Battle& b = *env_.battle;
  const Database& db = *b.db;
  const BattleArt& art = db.graphics.battle;
  const int deciding = db.battleBackgroundFromAttacker ? script_.initiator : 1 - script_.initiator;
  const Cell where = cells[script_.side[deciding].unit == pair[0] ? 0 : 1];
  terrain_ = where != kNoCell ? (int)b.cells[(size_t)where].terrain : -1;
  const MapDef noMap;
  const MapDef& mapDef = env_.map ? *env_.map : noMap;
  background_ = pickBattleBackground(db, mapDef, terrain_);
  if (background_ >= 0 && (!env_.sprites || env_.sprites->texture(art.backgrounds[(size_t)background_].sheet) == 0)) background_ = -1;
  // Platforms: one for both from the same square as the background ("battlePlatformFrom": "same", the default), or each fighter's own from the terrain it stands on ("own").
  for (int s = 0; s < 2; ++s) {
    int terrainOf = terrain_;
    if (db.battlePlatformOwn) {
      const Cell own = cells[script_.side[s].unit == pair[0] ? 0 : 1];
      terrainOf = own != kNoCell ? (int)b.cells[(size_t)own].terrain : -1;
    }
    platform_[s] = pickBattlePlatform(db, mapDef, terrainOf);
    if (platform_[s] >= 0 && (!env_.sprites || env_.sprites->texture(art.platforms[(size_t)platform_[s]].sheet) == 0)) platform_[s] = -1;
  }
  platformFar_ = script_.ranged();
}

// The log lines of a starting scene (tests read them).
void FightScene::logStart() const {
  const Database& db = *env_.battle->db;
  const BattleArt& art = db.graphics.battle;
  const MapDef noMap;
  const MapDef& mapDef = env_.map ? *env_.map : noMap;
  std::string staffNote;  // a staff use: ", staff Heal Staff on Gorm" ends the start line
  if (script_.staff()) {
    const int item = script_.strikes[0].item;
    staffNote = ", staff " + (item >= 0 && (size_t)item < db.items.size() ? db.items[(size_t)item].name : std::string("?")) + " on " +
                fighter_[1 - script_.strikes[0].actor].name;
  }
  OTE_INFO("battle scene: start %s (Lv %d) vs %s (Lv %d), %s, %d strike(s)%s", fighter_[0].name.c_str(), fighter_[0].level,
           fighter_[1].name.c_str(), fighter_[1].level,
           script_.ranged() ? ("ranged, " + intStr(script_.distance) + " squares").c_str() : "melee", (int)script_.strikes.size(), staffNote.c_str());
  // Only a game that uses the newer picks (platforms, a map's table, "attacker") says which picture it chose: every other game logs what it always did.
  const bool pickLog = db.battleBackgroundFromAttacker || !art.platforms.empty() || !mapDef.bgByTerrain.empty() || mapDef.bgDefault >= 0;
  if (db.battleSceneSides != 0) OTE_INFO("battle scene: left %s, right %s", fighter_[0].name.c_str(), fighter_[1].name.c_str());
  if (pickLog && background_ >= 0) OTE_INFO("battle scene: background \"%s\" (terrain %s)", art.backgrounds[(size_t)background_].id.c_str(), terrain_ >= 0 ? db.terrains[(size_t)terrain_].id.c_str() : "none");
  if (!db.battlePlatformOwn) {
    if (platform_[0] >= 0) OTE_INFO("battle scene: platform \"%s\"%s", art.platforms[(size_t)platform_[0]].id.c_str(), platformFar_ && art.platforms[(size_t)platform_[0]].hasFar ? " (far)" : "");
  } else {  // "own": one line per fighter that has one, naming its side
    for (int s = 0; s < 2; ++s)
      if (platform_[s] >= 0)
        OTE_INFO("battle scene: platform \"%s\" (%s)%s", art.platforms[(size_t)platform_[s]].id.c_str(), s == 0 ? "left" : "right",
                 platformFar_ && art.platforms[(size_t)platform_[s]].hasFar ? " (far)" : "");
  }
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
}

}  // namespace ote
