// Internal to the campaign loader (loader_campaign.cpp, loader_campaign_scene.cpp): the state
// shared while data/campaign.json is read. Every reader returns false after writing a message
//   data/campaign.json: chapter "ch1": "before": command 2 (say): <what is wrong>
#pragma once
#include <initializer_list>
#include <string>
#include <vector>

#include "core/data/campaign.h"
#include "core/data/events.h"
#include "core/data/loader_util.h"

namespace ote {

inline std::string quoted(const std::string& s) { return "\"" + s + "\""; }

class CampaignLoader {
 public:
  CampaignLoader(const Database& db, std::string& err, const std::string& file, CampaignDef& def)
      : db_(db), c_(err, file), def_(def) {}

  const Database& db() const { return db_; }
  const LoadCtx& ctx() const { return c_; }
  bool fail(const std::string& where, const std::string& msg) const { return c_.fail(where, msg); }
  // An object may only have these keys (a typo must not pass silently).
  bool onlyKeys(const Json& o, std::initializer_list<const char*> keys, const std::string& where) const;
  int text(const std::string& s) {
    def_.texts.push_back(s);
    return (int)def_.texts.size() - 1;
  }

  // A scene: a list of commands. `allowGoto`: only the "after" scene may name the next chapter.
  // Parties (docs/EVENTS.md "Parties"), loader_campaign_parties.cpp: the "parties" list (read before the chapters).
  bool readParties(const Json& root);
  // A chapter's "party" value: it must be one of the campaign's parties.
  bool partyRef(const Json& v, const std::string& at, const char* key, std::string& out) const;

  bool scene(const Json& list, const std::string& where, bool allowGoto, std::vector<SceneCommand>& out);
  // A chapter's "before" / "after": a scene, or { "music": "id", "commands": [...] } (docs/SOUND.md), which is the
  // same scene with a playMusic command in front.
  bool chapterScene(const Json& v, const std::string& where, bool allowGoto, std::vector<SceneCommand>& out);

 private:
  bool command(const Json& e, const std::string& where, bool allowGoto, SceneCommand& out);
  bool condition(const Json& e, const std::string& where, SceneCondition& out) const;
  // Strategy-RPG supports (docs/SRPG.md), loader_supports_story.cpp. The pair is only checked once supports.json is
  // loaded (resolveSupportRefs), because campaign.json is read before it.
  bool supportCommand(const Json& e, const std::string& where, SceneCommand& out);
  bool supportCondition(const Json& e, const std::string& where, SceneCondition& out) const;
  bool choiceCommand(const Json& e, const std::string& at, bool allowGoto, SceneCommand& out);  // loader_campaign_choice.cpp
  bool partyCommand(const Json& e, const std::string& at, SceneCommand& out);                   // loader_campaign_parties.cpp
  bool storeItemsCommand(const Json& e, const std::string& at, SceneCommand& out);              // loader_campaign_extras.cpp

  const Database& db_;
  LoadCtx c_;
  CampaignDef& def_;
  std::vector<int> choiceStores_;  // variables the choices of the scene being read write (one choice each; loader_campaign_choice.cpp)

 public:
  // Starts a new scene (a chapter's "before" or "after"): its choices may reuse variables other scenes use.
  void beginScene() { choiceStores_.clear(); }
};

}  // namespace ote
