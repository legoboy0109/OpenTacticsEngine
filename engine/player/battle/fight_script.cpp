#include "battle/fight_script.h"

#include <algorithm>

namespace ote {

bool belongsToFight(const Event& e, int u0, int u1) {
  auto mine = [&](int u) { return u >= 0 && (u == u0 || u == u1); };
  switch (e.type) {
    case EventType::Attack: return (e.a == u0 && e.b == u1) || (e.a == u1 && e.b == u0);
    case EventType::SkillActivated:
    case EventType::UnitHealed:
    case EventType::UnitHurt:
    case EventType::UnitDied:
    case EventType::ConditionApplied:
    case EventType::ConditionResisted:
    case EventType::ItemBroke: return mine(e.a);
    default: return false;
  }
}

bool belongsToStaffUse(const Event& e, int user, int target) {
  switch (e.type) {
    case EventType::StaffMissed: return e.a == target && e.b == user;
    case EventType::UnitHealed:
    case EventType::ItemRepaired:
    case EventType::ConditionApplied:
    case EventType::ConditionEnded:
    case EventType::ConditionResisted: return e.a == target;
    default: return false;
  }
}

bool staffUseInScene(const Event& used, const std::vector<Event>& following) {
  if (used.type != EventType::StaffUsed || used.c < 0 || used.c == used.a || used.d != 1) return false;
  for (const Event& e : following) {
    if (e.type == EventType::UnitRelocated) return false;  // moved: stays on the map
    if (!belongsToStaffUse(e, used.a, used.c) && e.type != EventType::ItemBroke) break;
  }
  return true;
}

namespace {

std::string nameOf(const std::vector<std::string>& names, int index, const char* plain) {
  return index >= 0 && (size_t)index < names.size() && !names[(size_t)index].empty() ? names[(size_t)index] : std::string(plain);
}

}  // namespace

bool initiatorStandsLeft(int mode, bool initiatorHuman, bool otherHuman, int initiatorX, int otherX) {
  if ((mode == 1 || mode == 2) && initiatorHuman != otherHuman) {
    const bool humanLeft = mode == 2;
    return initiatorHuman ? humanLeft : !humanLeft;
  }
  return initiatorX <= otherX;  // the map rule: the unit nearer the left edge, else the one that started it
}

namespace {

// The script of a staff use: one strike by the user (`staff`), the result on the target.
FightScript buildStaffScript(const Event& used, const std::vector<Event>& run, const FightInfo& info) {
  FightScript out;
  auto unitInfo = [&](int u) {
    auto it = info.units.find(u);
    if (it != info.units.end()) return it->second;
    FightUnitInfo blank;
    blank.unit = u;
    blank.name = "?";
    return blank;
  };
  const FightUnitInfo ui = unitInfo(used.a), ut = unitInfo(used.c);
  // The user stands where its side stands (a person's fighter on the right with "playerRight"); the target on the other side, even when both
  // are a person's (the original puts an ally opposite). `battleSceneSides` "map": the unit nearer the left edge of the map stands on the left.
  const bool userLeft = initiatorStandsLeft(info.sidesMode, ui.human, !ui.human, ui.x, ut.x);
  out.side[0] = userLeft ? ui : ut;
  out.side[1] = userLeft ? ut : ui;
  out.initiator = userLeft ? 0 : 1;
  out.distance = std::max(1, info.distance);
  FightStrike st;
  st.staff = true;
  st.item = used.b;
  st.actor = out.initiator;
  const int t = 1 - st.actor;
  int hp = out.side[t].hp;
  const int maxHp = std::max(1, out.side[t].maxHp);
  for (const Event& e : run) {
    if (!belongsToStaffUse(e, used.a, used.c)) continue;
    switch (e.type) {
      case EventType::StaffMissed: st.missed = true; break;
      case EventType::UnitHealed: {
        const int gained = std::max(0, std::min(e.b, maxHp - hp));
        hp += gained;
        st.healed += gained;
        break;
      }
      case EventType::ItemRepaired: st.notes.push_back({t, nameOf(info.itemNames, e.b, "Item") + " +" + std::to_string(e.c), 1}); break;
      case EventType::ConditionApplied: st.notes.push_back({t, nameOf(info.conditionNames, e.b, "Condition"), 0}); break;
      case EventType::ConditionResisted: st.notes.push_back({t, nameOf(info.conditionNames, e.b, "Condition") + " resisted", 0}); break;
      case EventType::ConditionEnded:
        if (e.c == kConditionCured) st.notes.push_back({t, nameOf(info.conditionNames, e.b, "Condition") + " cured", 0});
        break;
      default: break;
    }
  }
  st.hit = !st.missed;
  st.hpBefore[0] = out.side[0].hp;
  st.hpBefore[1] = out.side[1].hp;
  st.hpAfterHit[0] = st.hpEnd[0] = out.side[0].hp;
  st.hpAfterHit[1] = st.hpEnd[1] = out.side[1].hp;
  st.hpAfterHit[t] = st.hpEnd[t] = hp;
  out.strikes.push_back(st);
  out.valid = true;
  return out;
}

}  // namespace

FightScript buildFightScript(const std::vector<Event>& run, const FightInfo& info) {
  FightScript out;
  if (!run.empty() && run[0].type == EventType::StaffUsed) return buildStaffScript(run[0], run, info);
  const Event* first = nullptr;
  for (const Event& e : run)
    if (e.type == EventType::Attack) {
      first = &e;
      break;
    }
  if (!first) return out;
  auto unitInfo = [&](int u) {
    auto it = info.units.find(u);
    if (it != info.units.end()) return it->second;
    FightUnitInfo blank;
    blank.unit = u;
    blank.name = "?";
    return blank;
  };
  // Who started it: the striker of the first strike that is not a counter; a run of counters only (the foe's reply)
  // counts the unit that was struck as the one that started.
  int initiatorUnit = first->b;
  for (const Event& e : run)
    if (e.type == EventType::Attack && !(e.flags & kCounter)) {
      initiatorUnit = e.a;
      break;
    }
  const int other = initiatorUnit == first->a ? first->b : first->a;
  FightUnitInfo ui = unitInfo(initiatorUnit), uo = unitInfo(other);
  // By default the unit nearer the left edge of the map stands on the left; on the same column the one that started it does.
  const bool initiatorLeft = initiatorStandsLeft(info.sidesMode, ui.human, uo.human, ui.x, uo.x);
  out.side[0] = initiatorLeft ? ui : uo;
  out.side[1] = initiatorLeft ? uo : ui;
  out.initiator = initiatorLeft ? 0 : 1;
  out.distance = std::max(1, info.distance);

  int hp[2] = {out.side[0].hp, out.side[1].hp};
  const int maxHp[2] = {std::max(1, out.side[0].maxHp), std::max(1, out.side[1].maxHp)};
  std::vector<FightTag> pending;
  for (const Event& e : run) {
    if (!belongsToFight(e, out.side[0].unit, out.side[1].unit)) continue;
    const int s = out.sideOf(e.a);
    switch (e.type) {
      case EventType::SkillActivated: pending.push_back({s, nameOf(info.skillNames, e.b, "Skill")}); break;
      case EventType::ConditionApplied: pending.push_back({s, nameOf(info.conditionNames, e.b, "Condition")}); break;
      case EventType::ConditionResisted: pending.push_back({s, nameOf(info.conditionNames, e.b, "Condition") + " resisted"}); break;
      case EventType::Attack: {
        FightStrike st;
        st.actor = out.sideOf(e.a);
        const int t = 1 - st.actor;
        st.weapon = e.c;
        st.counter = (e.flags & kCounter) != 0;
        st.hit = (e.flags & kHit) != 0;
        st.crit = st.hit && (e.flags & kCrit) != 0;
        st.kill = st.hit && (e.flags & kKill) != 0;
        st.survived = st.hit && (e.flags & kSurvived) != 0;
        st.damage = st.hit ? e.d : 0;
        st.tags.swap(pending);
        if (st.survived) st.tags.push_back({t, "Survived!"});
        st.hpBefore[0] = hp[0];
        st.hpBefore[1] = hp[1];
        if (st.hit) hp[t] = std::max(0, hp[t] - st.damage);
        if (st.kill) {
          hp[t] = 0;
          st.dies[t] = true;
        }
        st.hpAfterHit[0] = st.hpEnd[0] = hp[0];
        st.hpAfterHit[1] = st.hpEnd[1] = hp[1];
        out.strikes.push_back(st);
        break;
      }
      case EventType::UnitHealed: {  // drain: the striker takes back a share of what it dealt
        if (out.strikes.empty() || s < 0) break;
        FightStrike& st = out.strikes.back();
        const int gained = std::min(e.b, maxHp[s] - hp[s]);
        hp[s] += std::max(0, gained);
        st.after.push_back({s, std::max(0, gained)});
        st.hpEnd[0] = hp[0];
        st.hpEnd[1] = hp[1];
        break;
      }
      case EventType::UnitHurt: {  // a skill's recoil
        if (out.strikes.empty() || s < 0) break;
        FightStrike& st = out.strikes.back();
        const int lost = std::min(e.b, hp[s]);
        hp[s] -= lost;
        st.after.push_back({s, -lost});
        st.hpEnd[0] = hp[0];
        st.hpEnd[1] = hp[1];
        break;
      }
      case EventType::UnitDied:
        if (!out.strikes.empty() && s >= 0) {
          out.strikes.back().dies[s] = true;
          hp[s] = 0;
          out.strikes.back().hpEnd[s] = 0;
        }
        break;
      case EventType::ItemBroke:
        if (!out.strikes.empty()) {
          out.strikes.back().weaponBroke = true;
          out.strikes.back().brokeText = nameOf(info.itemNames, e.b, "Weapon") + " broke";
        }
        break;
      default: break;
    }
  }
  if (!pending.empty() && !out.strikes.empty())  // end-of-fight activations stay with the last strike
    for (const FightTag& t : pending) out.strikes.back().tags.push_back(t);
  out.valid = !out.strikes.empty();
  return out;
}

}  // namespace ote
