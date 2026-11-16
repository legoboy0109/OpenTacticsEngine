#include "battle/support_text.h"

#include <algorithm>

#include "core/battle/support_rules.h"

namespace ote {
namespace {

bool pairOk(const Database& db, int pair) { return pair >= 0 && (size_t)pair < db.supports.pairs.size(); }

std::string charName(const Database& db, int c) {
  return c >= 0 && (size_t)c < db.characters.size() ? db.characters[(size_t)c].name : "?";
}

}  // namespace

std::string supportPairName(const Database& db, int pair) {
  if (!pairOk(db, pair)) return "?";
  const SupportPairDef& p = db.supports.pairs[(size_t)pair];
  return charName(db, p.a) + " and " + charName(db, p.b);
}

std::string supportMenuLabel(const std::string& partner) { return "Support: " + partner; }

std::string supportBannerText(const Database& db, int pair, int level) {
  return supportPairName(db, pair) + ": support level " + supportLevelName(db.supports.rules, level);
}

const char* supportReadyLabel() { return "Support!"; }

std::vector<SupportRow> supportRows(const Database& db, const SupportState& state, int character) {
  std::vector<SupportRow> rows;
  if (character < 0 || !supportStateFits(db, state)) return rows;
  for (size_t i = 0; i < db.supports.pairs.size(); ++i) {
    const SupportPairDef& p = db.supports.pairs[i];
    if (p.a != character && p.b != character) continue;
    SupportRow r;
    r.partner = charName(db, p.a == character ? p.b : p.a);
    r.level = state.level[i];
    r.levels = (int)p.levels.size();
    r.levelName = r.level > 0 ? supportLevelName(db.supports.rules, r.level) : "";
    r.points = state.points[i];
    r.top = r.level >= r.levels;
    r.from = r.level > 0 ? p.levels[(size_t)r.level - 1].points : 0;
    r.to = r.top ? r.from : p.levels[(size_t)r.level].points;
    r.ready = supportTalkPending(db, state, (int)i);
    rows.push_back(r);
  }
  return rows;
}

float supportBarFraction(const SupportRow& row) {
  if (row.top || row.to <= row.from) return 1.0f;
  return std::max(0.0f, std::min(1.0f, (float)(row.points - row.from) / (float)(row.to - row.from)));
}

std::string supportLevelText(const SupportRow& row) {
  return row.level > 0 ? "Level " + row.levelName : "No support yet";
}

std::vector<CampRow> campRows(const Database& db, const SupportState& state, const std::vector<int>& pending) {
  std::vector<CampRow> rows;
  for (int pair : pending) {
    if (!pairOk(db, pair) || (size_t)pair >= state.level.size()) continue;
    CampRow r;
    r.pair = pair;
    r.label = supportPairName(db, pair);
    r.extra = "Level " + supportLevelName(db.supports.rules, state.level[(size_t)pair] + 1);
    rows.push_back(r);
  }
  return rows;
}

}  // namespace ote
