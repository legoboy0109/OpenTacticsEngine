// Campaign parties (docs/EVENTS.md "Parties"): the "parties" list, a chapter's "party", and the scene commands
//   { "switchParty": "north" }        { "mergeParties": ["north"], "into": "main" }
// Every refusal names the file, the place and what to write instead.
#include <algorithm>

#include "core/data/loader_campaign.h"

namespace ote {

bool CampaignLoader::readParties(const Json& root) {
  if (!root.has("parties")) return true;
  const Json& list = root["parties"];
  if (!list.isArray()) return fail("", "\"parties\" must be a list such as [ { \"id\": \"main\", \"name\": \"The company\" } ]");
  for (size_t i = 0; i < list.size(); ++i) {
    const Json& e = list[i];
    const std::string where = "party " + std::to_string(i + 1);
    if (!e.isObject()) return fail(where, "must be an object such as { \"id\": \"main\", \"name\": \"The company\" }");
    PartyDef p;
    if (!c_.str(e, "id", where, p.id, true)) return false;
    if (!isStoryId(p.id)) return fail(where, "\"id\" must be letters, digits, \"_\" and \"-\" (got " + quoted(p.id) + ")");
    if (def_.findParty(p.id) >= 0) return fail("party " + quoted(p.id), "duplicate party id");
    if (!onlyKeys(e, {"id", "name"}, "party " + quoted(p.id))) return false;
    p.name = p.id;
    if (!c_.str(e, "name", "party " + quoted(p.id), p.name)) return false;
    def_.parties.push_back(p);
  }
  return true;  // an empty list is the same as no key: a campaign with one roster
}

// A party id named by a command or a chapter: it must be listed in "parties".
bool CampaignLoader::partyRef(const Json& v, const std::string& at, const char* key, std::string& out) const {
  if (def_.parties.empty())
    return fail(at, std::string("\"") + key + "\" needs the campaign to have parties: add \"parties\": [ { \"id\": \"main\" }, ... ] to data/campaign.json");
  if (!v.isString() || def_.findParty(v.asString()) < 0) {
    std::string list;
    for (const PartyDef& p : def_.parties) list += (list.empty() ? "" : ", ") + quoted(p.id);
    return fail(at, std::string("\"") + key + "\" names unknown party " + quoted(v.isString() ? v.asString() : std::string("?")) +
                        " (the campaign's parties are " + list + ")");
  }
  out = v.asString();
  return true;
}

bool CampaignLoader::partyCommand(const Json& e, const std::string& at, SceneCommand& out) {
  if (e.has("switchParty")) {
    out.kind = SceneCommand::Kind::SwitchParty;
    return onlyKeys(e, {"switchParty"}, at) && partyRef(e["switchParty"], at, "switchParty", out.party);
  }
  out.kind = SceneCommand::Kind::MergeParties;
  if (!onlyKeys(e, {"mergeParties", "into"}, at)) return false;
  if (!e.has("into")) return fail(at, "missing required \"into\" (the party that takes the others in)");
  if (!partyRef(e["into"], at, "into", out.party)) return false;
  const Json& list = e["mergeParties"];
  if (!list.isArray() || list.size() == 0) return fail(at, "\"mergeParties\" must be a list of party ids, such as [\"north\"]");
  for (size_t i = 0; i < list.size(); ++i) {
    std::string id;
    if (!partyRef(list[i], at, "mergeParties", id)) return false;
    if (id == out.party) return fail(at, "\"mergeParties\" lists " + quoted(id) + ", the party it merges into");
    if (std::find(out.parties.begin(), out.parties.end(), id) != out.parties.end())
      return fail(at, "\"mergeParties\" lists " + quoted(id) + " twice");
    out.parties.push_back(id);
  }
  return true;
}

}  // namespace ote
