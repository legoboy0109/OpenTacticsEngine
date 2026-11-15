// The parsed form of data/campaign.json (docs/EVENTS.md "Campaign"). Plain structs filled by
// loadCampaign (loader_campaign*.cpp) and only read by the campaign module (core/campaign/).
// Scenes are lists of a small subset of the event commands; they run between battles, so they
// know no map: units are named by roster id and sides by side id (text), checked when used.
#pragma once
#include <string>
#include <vector>

#include "core/data/database.h"
#include "core/data/events.h"  // CmpOp

namespace ote {

struct SceneCondition {
  enum class Kind { Switch, Variable, Alive, Dead };
  Kind kind = Kind::Switch;
  int index = -1;        // switch / variable index
  bool is = true;        // Switch
  CmpOp op = CmpOp::Eq;  // Variable
  int value = 0;         // Variable
  std::string unit;      // Alive, Dead: a roster id
};

struct SceneCommand {
  enum class Kind { Say, SetSwitch, SetVariable, AddVariable, GiveResource, If, Goto, PlayMusic, StopMusic, PlaySound };
  Kind kind = Kind::Say;
  // Say: indices into CampaignDef::texts (-1 = none); `unit` is the speaker's roster id ("" = none)
  int text = -1, speakerName = -1, portrait = -1;
  std::string unit;
  int index = -1;   // SetSwitch / SetVariable / AddVariable
  // PlayMusic / PlaySound: `text` is the id (index into CampaignDef::texts); PlayMusic / StopMusic: `value` is the fade in ms (-1 = not given)
  bool on = false;  // SetSwitch
  int value = 0;    // SetVariable / AddVariable
  std::string side;              // GiveResource: the side id the amounts go to
  std::vector<Amount> amounts;   // GiveResource
  std::vector<SceneCondition> cond;           // If
  std::vector<SceneCommand> thenDo, elseDo;   // If
  std::string chapter;                        // Goto: a chapter id
};

struct ChapterDef {
  std::string id, name;
  int map = -1;  // index into Database::maps
  std::vector<SceneCommand> before, after;
  std::string next;  // chapter id; "" = the following one in the list
};

struct CampaignDef {
  enum class FreeBattles { None, All, List };
  std::string name;
  bool fallenReturn = false;  // rules.fallen: "gone" (false) or "return"
  bool endOnDefeat = false;   // rules.onDefeat: "retry" (false) or "end"
  FreeBattles freeBattles = FreeBattles::None;
  std::vector<std::string> freeMaps;  // map ids, FreeBattles::List
  std::vector<ChapterDef> chapters;   // never empty
  // All wording (dialogue, speaker names, portrait ids) lives here; commands carry indices, as
  // in StoryData, and the wording stays out of the save hash.
  std::vector<std::string> texts;

  int findChapter(const std::string& id) const {
    for (size_t i = 0; i < chapters.size(); ++i)
      if (chapters[i].id == id) return (int)i;
    return -1;
  }
};

// Reads the optional data/campaign.json into db.campaign (left null when the file is missing).
// Needs maps, switches, variables and resources loaded. Messages start with the file path.
bool loadCampaign(const std::string& path, Database& db, std::string& err);

}  // namespace ote
