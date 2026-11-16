// See support_rules.h.
#include "core/battle/support_rules.h"

#include <algorithm>

namespace ote {

namespace {

// A level is held only while its points are reached (level 0 needs none).
bool reached(const SupportPairDef& p, int level, int points) {
  return level <= 0 || points >= p.levels[(size_t)level - 1].points;
}

bool pairHas(const SupportPairDef& p, int character) { return p.a == character || p.b == character; }

}  // namespace

int supportLevelsHeld(const Database& db, const SupportState& state, int character) {
  int sum = 0;
  for (size_t i = 0; i < db.supports.pairs.size() && i < state.level.size(); ++i)
    if (pairHas(db.supports.pairs[i], character)) sum += state.level[i];
  return sum;
}

bool supportReady(const Database& db, const SupportState& state, int pair) {
  const size_t n = db.supports.pairs.size();
  if (pair < 0 || (size_t)pair >= n || state.level.size() != n || state.points.size() != n) return false;
  const SupportPairDef& p = db.supports.pairs[(size_t)pair];
  const int next = state.level[(size_t)pair];
  if (next >= (int)p.levels.size() || state.points[(size_t)pair] < p.levels[(size_t)next].points) return false;
  const int cap = db.supports.rules.maxLevels;
  if (cap > 0 && (supportLevelsHeld(db, state, p.a) + 1 > cap || supportLevelsHeld(db, state, p.b) + 1 > cap))
    return false;
  return true;
}

bool supportTalkPending(const Database& db, const SupportState& state, int pair) {
  if (!supportReady(db, state, pair)) return false;
  const SupportPairDef& p = db.supports.pairs[(size_t)pair];
  return !p.levels[(size_t)state.level[(size_t)pair]].talk.empty();
}

bool supportAddPoints(const Database& db, SupportState& state, int pair, int by) {
  const SupportPairDef& p = db.supports.pairs[(size_t)pair];
  int& points = state.points[(size_t)pair];
  const long long next = std::max(0LL, std::min((long long)kMaxSupportPoints, (long long)points + by));
  if (next == points) return false;
  points = (int)next;
  uint8_t& level = state.level[(size_t)pair];
  while (level > 0 && !reached(p, level, points)) --level;
  return true;
}

void supportSettle(const Database& db, SupportState& state, std::vector<std::pair<int, int>>* gained) {
  for (size_t i = 0; i < db.supports.pairs.size(); ++i) {
    // Gaining a level only ever makes another pair less ready (maxLevels), so one pass in pair order is enough.
    while (supportReady(db, state, (int)i) && !supportTalkPending(db, state, (int)i)) {
      ++state.level[i];
      if (gained) gained->push_back({(int)i, (int)state.level[i]});
    }
  }
}

SupportState startingSupports(const Database& db) {
  SupportState s;
  const size_t n = db.supports.pairs.size();
  s.points.resize(n);
  s.level.assign(n, 0);
  for (size_t i = 0; i < n; ++i) s.points[i] = db.supports.pairs[i].start;
  supportSettle(db, s, nullptr);
  return s;
}

bool supportStateFits(const Database& db, const SupportState& state, std::string* why) {
  const size_t n = db.supports.pairs.size();
  auto no = [&](const std::string& what) {
    if (why) *why = what;
    return false;
  };
  if (state.points.size() != n)
    return no("\"points\" has " + std::to_string(state.points.size()) + " entries, expected " + std::to_string(n));
  if (state.level.size() != n)
    return no("\"levels\" has " + std::to_string(state.level.size()) + " entries, expected " + std::to_string(n));
  for (size_t i = 0; i < n; ++i) {
    const SupportPairDef& p = db.supports.pairs[i];
    const std::string at = "[" + std::to_string(i) + "]";
    if (state.points[i] < 0 || state.points[i] > kMaxSupportPoints)
      return no("\"points\"" + at + " is outside 0.." + std::to_string(kMaxSupportPoints));
    if (state.level[i] > p.levels.size())
      return no("\"levels\"" + at + " is above the pair's " + std::to_string(p.levels.size()) + " levels");
    if (!reached(p, state.level[i], state.points[i])) return no("\"levels\"" + at + " is above what the pair's points allow");
  }
  return true;
}

std::string supportLevelName(const SupportRules& rules, int level) {
  if (level <= 0) return std::string();
  if ((size_t)level <= rules.levelNames.size()) return rules.levelNames[(size_t)level - 1];
  return std::to_string(level);
}

}  // namespace ote
