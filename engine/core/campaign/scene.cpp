// Running a chapter's scenes (docs/EVENTS.md "Campaign"): commands change the playthrough at once
// and leave lines for the player. Nothing waits for input, so a scene is deterministic.
#include <algorithm>

#include "core/campaign/playthrough.h"
#include "core/data/loader.h"

namespace ote {

namespace {

bool compare(CmpOp op, int a, int b) {
  switch (op) {
    case CmpOp::Eq: return a == b;
    case CmpOp::Ne: return a != b;
    case CmpOp::Lt: return a < b;
    case CmpOp::Le: return a <= b;
    case CmpOp::Gt: return a > b;
    case CmpOp::Ge: return a >= b;
  }
  return false;
}

int clampStory(long long v) {
  const long long lim = kMaxStoryNumber;
  return (int)std::max(-lim, std::min(lim, v));
}

struct SceneRun {
  const Database& db;
  Playthrough& pt;
  std::vector<SceneLine>& lines;
  std::string gotoChapter;  // the last "goto" executed ("" = none)
  // The chapter's map, read once when a speaker is not in the party yet: before the first battle
  // the roster is empty, yet the hero must be able to speak under their name.
  bool mapTried = false, mapOk = false;
  MapDef map;

  std::string nameFromMap(const std::string& id) {
    if (!mapTried) {
      mapTried = true;
      const ChapterDef& ch = db.campaign->chapters[(size_t)pt.chapter];
      std::string err;
      mapOk = ch.map >= 0 && ch.map < (int)db.maps.size() && loadMap(db, db.maps[(size_t)ch.map].id, map, err);
    }
    if (!mapOk) return std::string();
    for (const MapUnitDef& u : map.units)
      if (u.id == id) return u.name.empty() ? db.unitTypes[(size_t)u.type].name : u.name;
    return std::string();
  }

  bool holds(const SceneCondition& c) const {
    switch (c.kind) {
      case SceneCondition::Kind::Switch: return (pt.switches[(size_t)c.index] != 0) == c.is;
      case SceneCondition::Kind::Variable: return compare(c.op, pt.variables[(size_t)c.index], c.value);
      case SceneCondition::Kind::Alive: {
        const RosterUnit* r = pt.findUnit(c.unit);
        return r && !r->gone;
      }
      case SceneCondition::Kind::Dead: {
        const RosterUnit* r = pt.findUnit(c.unit);
        return r && r->gone;
      }
    }
    return false;
  }

  void run(const std::vector<SceneCommand>& list) {
    for (const SceneCommand& c : list) command(c);
  }

  void command(const SceneCommand& c) {
    using K = SceneCommand::Kind;
    const std::vector<std::string>& texts = db.campaign->texts;
    switch (c.kind) {
      case K::Say: {
        SceneLine l;
        l.speakerId = c.unit;
        if (!c.unit.empty()) {
          const RosterUnit* r = pt.findUnit(c.unit);
          // In the party: its name there. Not yet: the name the chapter's map gives that unit;
          // unknown to both: no name, like a Dialogue entry with no unit.
          l.speaker = r ? r->name : nameFromMap(c.unit);
        } else if (c.speakerName >= 0) {
          l.speaker = texts[(size_t)c.speakerName];
        }
        if (c.portrait >= 0) l.portrait = texts[(size_t)c.portrait];
        l.text = texts[(size_t)c.text];
        lines.push_back(l);
        break;
      }
      case K::SetSwitch: pt.switches[(size_t)c.index] = c.on ? 1 : 0; break;
      case K::SetVariable: pt.variables[(size_t)c.index] = c.value; break;
      case K::AddVariable:
        pt.variables[(size_t)c.index] = clampStory((long long)pt.variables[(size_t)c.index] + c.value);
        break;
      case K::GiveResource: {
        CarriedSide& s = pt.sideCarry(c.side, db.resources.size());
        for (const Amount& a : c.amounts) {
          int& r = s.resources[(size_t)a.resource];
          r = std::max(0, clampStory((long long)r + a.amount));  // funds never go below zero
        }
        break;
      }
      case K::If: {
        bool all = true;
        for (const SceneCondition& cnd : c.cond) all = all && holds(cnd);
        run(all ? c.thenDo : c.elseDo);
        break;
      }
      case K::Goto: gotoChapter = c.chapter; break;
      case K::PlayMusic:
      case K::StopMusic:
      case K::PlaySound: {  // presentation only: nothing in the playthrough changes
        SceneLine l;
        l.kind = c.kind == K::PlayMusic ? SceneLine::Kind::PlayMusic
                 : c.kind == K::StopMusic ? SceneLine::Kind::StopMusic : SceneLine::Kind::PlaySound;
        l.fadeMs = c.value;
        if (c.text >= 0) l.text = texts[(size_t)c.text];
        lines.push_back(l);
        break;
      }
    }
  }
};

}  // namespace

bool runScene(const Database& db, Playthrough& pt, SceneKind which, std::vector<SceneLine>& lines) {
  const CampaignDef* cd = db.campaign.get();
  if (!cd || pt.chapter < 0 || pt.chapter >= (int)cd->chapters.size()) return false;
  if (pt.phase != (which == SceneKind::Before ? Phase::Before : Phase::After)) return false;
  const ChapterDef& ch = cd->chapters[(size_t)pt.chapter];
  SceneRun r{db, pt, lines, std::string(), false, false, MapDef()};
  if (which == SceneKind::Before) {
    r.run(ch.before);
    pt.phase = Phase::Battle;
    return true;
  }
  r.run(ch.after);
  // The next chapter: the last "goto" that ran, else the chapter's "next", else the following one.
  int next = pt.chapter + 1;
  if (!r.gotoChapter.empty()) next = cd->findChapter(r.gotoChapter);
  else if (!ch.next.empty()) next = cd->findChapter(ch.next);
  if (next < 0 || next >= (int)cd->chapters.size()) {
    pt.phase = Phase::Finished;
  } else {
    pt.chapter = next;
    pt.phase = Phase::Before;
  }
  return true;
}

}  // namespace ote
