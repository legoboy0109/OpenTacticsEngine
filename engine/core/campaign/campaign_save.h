// Saving a campaign playthrough (contract: docs/SAVES.md "Campaign saves"). Like the battle save
// functions these only convert between objects and JSON; files are the player's job.
//
// A campaign save is the ordinary save file (same top-level fields) plus "playthrough", and
// "battle" only while a chapter's battle is under way. buildCampaignSave / restoreCampaignSave
// are the two calls the player needs.
#pragma once
#include <string>

#include "core/base/json.h"
#include "core/campaign/playthrough.h"

namespace ote {

namespace savejson {
class Reader;
// "names" of a playthrough (campaign_names.cpp): written only when a name was typed; read back with every name checked.
void writePlaythroughNames(const Playthrough& pt, Json& out);
bool readPlaythroughNames(Reader& rd, const Json& saved, Playthrough& pt);
}  // namespace savejson

// Must equal the player's kSaveFileVersion: a campaign save uses the same file format version.
constexpr int kCampaignSaveVersion = 1;

// The playthrough alone, as JSON (the value of "playthrough").
void savePlaythrough(const Database& db, const Playthrough& pt, Json& out);
// Reads it back with every field's type and range checked, ids checked against the game; `out`
// is only assigned when everything is valid. Messages name the field ("playthrough.roster[2].level: ...").
bool restorePlaythrough(const Database& db, const Json& saved, Playthrough& out, std::string& err);

// Hash for a campaign save's "dataHash": game.json rules, the data files, data/campaign.json (its
// wording not counted), the scripts, and the map file of EVERY chapter in chapter order, so a
// playthrough is refused when any chapter's rules changed. Same format as computeDataHash.
bool computeCampaignHash(const Database& db, std::string& out, std::string& err);

// Builds a whole save file. `battle` = the chapter's battle under way, or null; it must be given
// only in phase Battle (a battle that was not started yet is simply left out).
bool buildCampaignSave(const Database& db, const std::string& gameId, const Playthrough& pt, const Battle* battle,
                       long long savedAt, Json& out, std::string& err);

// Is this file a campaign save (has "playthrough")? Cheap; for the save lists.
bool isCampaignSave(const Json& root);

// Does the file fit this game as it is now? False with a plain sentence for the player in `reason`:
// another saveVersion / "different version" (hash), another game, a chapter the game no longer has.
// Does not read the playthrough itself; restoreCampaignSave does that.
bool campaignSaveUsable(const Database& db, const std::string& gameId, const Json& root, std::string& reason);

// Restores everything: the playthrough and, when the file has "battle", the chapter's battle
// (prepareChapterBattle + restoreBattleState, so it carries the roster again). `hasBattle` tells
// whether `battle` was filled. Nothing is assigned unless the whole file is valid; false with
// `err` (what is wrong and where) otherwise.
bool restoreCampaignSave(const Database& db, Formulas& formulas, const std::string& gameId, const Json& root,
                         Playthrough& pt, Battle& battle, bool& hasBattle, std::string& err);

}  // namespace ote
