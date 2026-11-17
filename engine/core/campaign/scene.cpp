// Running a chapter's scenes (docs/EVENTS.md "Campaign"): commands change the playthrough at once
// and leave lines for the player. Nothing waits for input, so a scene is deterministic.
#include <algorithm>

#include "core/campaign/difficulty.h"
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

// The number a scene command reads from a roster unit: 0 when the character is not in the roster or is gone (fell in
// an earlier chapter under "fallen": "gone"). A fallen unit that "returns" is not gone and is read.
int readRoster(const Database& db, const Playthrough& pt, const UnitReading& r) {
  const RosterUnit* u = pt.findUnit(r.unit);
  if (!u || u->gone) return 0;
  switch (r.kind) {
    case UnitReading::Kind::Level: return u->level;
    case UnitReading::Kind::MaxHp: return db.rules.statHp >= 0 && (size_t)db.rules.statHp < u->stats.size() ? u->stats[(size_t)db.rules.statHp] : 0;
    case UnitReading::Kind::Stat: return r.stat >= 0 && (size_t)r.stat < u->stats.size() ? u->stats[(size_t)r.stat] : 0;
    default: return 0;
  }
}

struct SceneRun {
  const Database& db;
  Playthrough& pt;
  std::vector<SceneLine>& lines;
  std::string gotoChapter;  // the last "goto" executed ("" = none)
  // Dialogue choices: `asking` = questions wait for the picks in `picks` (null = take the first offered answer);
  // `halted` = a question without a pick was reached, nothing after it runs.
  const std::vector<int>* picks = nullptr;
  size_t used = 0;
  bool halted = false;
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
      case SceneCondition::Kind::Support: {
        const int pair = db.findSupportPair(db.findCharacter(c.unit), db.findCharacter(c.with));
        return pair >= 0 && (size_t)pair < pt.supports.level.size() && compare(c.op, pt.supports.level[(size_t)pair], c.value);
      }
      case SceneCondition::Kind::Difficulty: {  // docs/DIFFICULTY.md: `value` = list position
        const int now = db.difficulties.find(pt.difficulty);
        return now >= 0 && compare(c.op, now, c.value);
      }
    }
    return false;
  }

  void run(const std::vector<SceneCommand>& list) {
    for (const SceneCommand& c : list) {
      if (halted) return;
      command(c);
    }
  }

  // The speaker of a Say or a Choice, resolved like a battle's Dialogue entry.
  void speaker(const SceneCommand& c, SceneLine& l) {
    const std::vector<std::string>& texts = db.campaign->texts;
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
  }

  void choice(const SceneCommand& c) {
    const std::vector<std::string>& texts = db.campaign->texts;
    std::vector<size_t> offered;
    for (size_t i = 0; i < c.answers.size(); ++i) {
      bool all = true;
      for (const SceneCondition& cnd : c.answers[i].cond) all = all && holds(cnd);
      if (all) offered.push_back(i);
    }
    if (offered.empty()) return;  // every answer is hidden: nothing to ask, nothing to record
    size_t picked = offered[0];
    if (offered.size() >= 2) {
      SceneLine l;
      speaker(c, l);
      if (!picks) {  // nobody to ask: the question is only said
        lines.push_back(l);
      } else {
        l.kind = SceneLine::Kind::Choice;
        for (size_t i : offered) l.answers.push_back(texts[(size_t)c.answers[i].text]);
        l.waiting = used >= picks->size();
        lines.push_back(l);
        if (l.waiting) {
          halted = true;
          return;
        }
        picked = offered[(size_t)std::max(0, std::min((*picks)[used++], (int)offered.size() - 1))];
      }
    }
    if (c.index >= 0) pt.variables[(size_t)c.index] = (int)picked + 1;
    run(c.answers[picked].commands);
  }

  void command(const SceneCommand& c) {
    using K = SceneCommand::Kind;
    const std::vector<std::string>& texts = db.campaign->texts;
    switch (c.kind) {
      case K::Choice: choice(c); break;
      case K::Say: {
        SceneLine l;
        speaker(c, l);
        l.position = c.fx.position;  // the presentation keys (docs/EVENTS.md "Scene presentation")
        l.narrate = c.fx.narrate;
        l.autoMs = std::max(0, c.fx.ms);
        lines.push_back(l);
        break;
      }
      case K::SetSwitch: pt.switches[(size_t)c.index] = c.on ? 1 : 0; break;
      case K::SetVariable: pt.variables[(size_t)c.index] = c.reading.kind == UnitReading::Kind::None ? c.value : clampStory(readRoster(db, pt, c.reading)); break;
      case K::AddVariable:
        pt.variables[(size_t)c.index] = clampStory((long long)pt.variables[(size_t)c.index] +
                                                     (c.reading.kind == UnitReading::Kind::None ? c.value : readRoster(db, pt, c.reading)));
        break;
      case K::TakeItem:
        for (RosterUnit& r : pt.roster) {
          if (r.id != c.unit || r.gone) continue;
          for (size_t s = 0; s < r.items.size();) {
            if (r.items[s].item != c.item) { ++s; continue; }
            r.items.erase(r.items.begin() + (long)s);
            if (!c.on) break;
          }
        }
        break;
      case K::StoreItems: sceneStoreItems(db, pt, c); break;
      case K::SwitchParty: switchParty(db, pt, c.party); break;
      case K::MergeParties: mergeParties(db, pt, c.parties, c.party); break;
      case K::GiveResource: {
        CarriedSide& s = pt.sideCarry(c.side, db.resources.size());
        for (const Amount& a : c.amounts) {
          int& r = s.resources[(size_t)a.resource];
          r = std::max(0, clampStory((long long)r + sceneResourceGain(db, pt, a.amount)));  // funds never go below zero; a gain follows the difficulty (docs/DIFFICULTY.md)
        }
        break;
      }
      case K::If: {
        bool all = true;
        for (const SceneCondition& cnd : c.cond) all = all && holds(cnd);
        run(all ? c.thenDo : c.elseDo);
        break;
      }
      case K::Fx: {  // scene presentation: nothing in the playthrough changes, the player acts on the line when it reaches it
        SceneLine l;
        l.kind = SceneLine::Kind::Fx;
        l.fx = c.fx.kind;
        l.fadeMs = c.fx.ms;
        l.color = c.fx.color;
        l.strength = c.fx.strength;
        l.out = c.fx.out;
        l.fadeTransition = c.fx.fadeTransition;
        l.position = c.fx.position;  // enter / move: the slot
        if (c.fx.portrait >= 0) l.portrait = texts[(size_t)c.fx.portrait];
        if (c.fx.text >= 0) l.text = texts[(size_t)c.fx.text];
        lines.push_back(l);
        break;
      }
      case K::Goto: gotoChapter = c.chapter; break;
      case K::AddSupport: sceneAddSupport(db, pt, db.findSupportPair(db.findCharacter(c.unit), db.findCharacter(c.with)), c.value); break;
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

namespace {

bool runSceneWith(const Database& db, Playthrough& pt, SceneKind which, std::vector<SceneLine>& lines,
                  const std::vector<int>* picks) {
  const CampaignDef* cd = db.campaign.get();
  if (!cd || pt.chapter < 0 || pt.chapter >= (int)cd->chapters.size()) return false;
  if (pt.phase != (which == SceneKind::Before ? Phase::Before : Phase::After)) return false;
  const ChapterDef& ch = cd->chapters[(size_t)pt.chapter];
  SceneRun r{db, pt, lines, std::string(), picks, 0, false, false, false, MapDef()};
  if (which == SceneKind::Before) {
    if (!ch.party.empty()) switchParty(db, pt, ch.party);  // the chapter's "party": active from the start of its scene
    r.run(ch.before);
    if (r.halted) return true;
    // A "goto" in a before scene skips this chapter's battle: the chapter it names is next, starting with its own before scene.
    if (!r.gotoChapter.empty() && cd->findChapter(r.gotoChapter) >= 0) {
      pt.chapter = cd->findChapter(r.gotoChapter);
      pt.phase = Phase::Before;
      return true;
    }
    pt.phase = Phase::Battle;
    return true;
  }
  r.run(ch.after);
  if (r.halted) return true;  // a question waits: the chapter has not been left
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

}  // namespace

bool runScene(const Database& db, Playthrough& pt, SceneKind which, std::vector<SceneLine>& lines) {
  return runSceneWith(db, pt, which, lines, nullptr);
}

bool runScene(const Database& db, Playthrough& pt, SceneKind which, std::vector<SceneLine>& lines,
              const std::vector<int>& answers) {
  return runSceneWith(db, pt, which, lines, &answers);
}

}  // namespace ote
