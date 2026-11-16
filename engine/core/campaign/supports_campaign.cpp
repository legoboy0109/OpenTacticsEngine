// Supports in a campaign (docs/SRPG.md "Supports": state, "Story and campaign"): the closeness a playthrough
// carries between chapters, the story's addSupport in scenes, and the conversations of the camp between chapters.
// The rules themselves (which level is ready, ...) are the battle's (core/battle/support_rules.h).
#include "core/battle/support_rules.h"
#include "core/campaign/playthrough.h"

namespace ote {

namespace {

// Both characters are in the roster and neither fell for good.
bool inParty(const Database& db, const Playthrough& pt, const SupportPairDef& p) {
  const RosterUnit* a = pt.findUnit(db.characters[(size_t)p.a].id);
  const RosterUnit* b = pt.findUnit(db.characters[(size_t)p.b].id);
  return a && b && !a->gone && !b->gone;
}

bool fits(const Database& db, const Playthrough& pt) { return supportStateFits(db, pt.supports); }

}  // namespace

void beginSupports(const Database& db, Playthrough& pt) { pt.supports = startingSupports(db); }

void takeSupports(const Database&, Playthrough& pt, const Battle& battle) { pt.supports = battle.supports; }

void pendingSupports(const Database& db, const Playthrough& pt, std::vector<int>& pairs) {
  pairs.clear();
  if (db.supports.rules.conversations == SupportTalks::Battle || !fits(db, pt)) return;
  for (size_t i = 0; i < db.supports.pairs.size(); ++i)
    if (supportTalkPending(db, pt.supports, (int)i) && inParty(db, pt, db.supports.pairs[i])) pairs.push_back((int)i);
}

bool playSupport(const Database& db, Playthrough& pt, int pair, std::vector<SceneLine>& lines) {
  std::vector<int> pending;
  pendingSupports(db, pt, pending);
  bool found = false;
  for (int p : pending) found = found || p == pair;
  if (!found) return false;
  const SupportPairDef& def = db.supports.pairs[(size_t)pair];
  for (const SupportLine& line : def.levels[(size_t)pt.supports.level[(size_t)pair]].talk) {
    SceneLine l;
    l.speakerId = line.speaker;
    const RosterUnit* r = line.speaker.empty() ? nullptr : pt.findUnit(line.speaker);
    // A roster member speaks under its own name; anybody else under the name stored with the line.
    if (r) l.speaker = r->name;
    else if (line.speakerName >= 0) l.speaker = db.supports.texts[(size_t)line.speakerName];
    if (line.portrait >= 0) l.portrait = db.supports.texts[(size_t)line.portrait];
    l.text = db.supports.texts[(size_t)line.text];
    lines.push_back(l);
  }
  ++pt.supports.level[(size_t)pair];
  supportSettle(db, pt.supports, nullptr);  // the next levels that need no conversation follow at once
  return true;
}

// A scene's addSupport: points to a pair of the roster (nothing for one that fell for good).
void sceneAddSupport(const Database& db, Playthrough& pt, int pair, int by) {
  if (pair < 0 || (size_t)pair >= db.supports.pairs.size() || !fits(db, pt)) return;
  const SupportPairDef& p = db.supports.pairs[(size_t)pair];
  const RosterUnit* a = pt.findUnit(db.characters[(size_t)p.a].id);
  const RosterUnit* b = pt.findUnit(db.characters[(size_t)p.b].id);
  if ((a && a->gone) || (b && b->gone)) return;
  supportAddPoints(db, pt.supports, pair, by);
  supportSettle(db, pt.supports, nullptr);
}

}  // namespace ote
