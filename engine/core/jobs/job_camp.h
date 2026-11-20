// Jobs outside a battle (docs/JOBS.md "Between chapters"): changing a roster character's job, learning and equipping. Free functions over a Playthrough, like
// the preparation screen's (core/campaign/prep.h): all deterministic, every refusal returns false with a plain sentence in `why` and changes nothing.
#pragma once
#include <string>

#include "core/campaign/playthrough.h"

namespace ote {

// Does this roster character use the job system (the game has jobs and it has a job)?
bool jobsRosterUses(const Database& db, const RosterUnit& r);
// The roster characters that use it, in roster order (not the fallen).
std::vector<std::string> jobsRosterIds(const Database& db, const Playthrough& pt);

// Where may jobs be changed right now? "camp" = between chapters only; "camp+prep" = the preparation screen too. The camp is always allowed.
bool jobsChangeAllowedInPrep(const Database& db);

// Changes the character's job. The unit type becomes the job's and the stats follow the way a promotion's do: each stat moves by what the new type's base
// differs from the old type's (never below 0); level, experience, items, learned skills and weapon experience stay. A battle begins at full hit points,
// so there is nothing to keep of them. Refused: an unknown character, one that does not use jobs, the job it already has, a locked job.
bool jobsChange(const Database& db, Playthrough& pt, const std::string& id, int job, std::string& why);
// The same test without changing anything (the screen greys a locked job and says why).
bool jobsChangeReason(const Database& db, const Playthrough& pt, const std::string& id, int job, std::string& why);
// The same on the preparation screen (rules.jobs.changeIn "camp+prep"): refused when the game does not allow it there, and for a character deployed on a
// slot the new job's unit type cannot stand on.
bool jobsChangeInPrep(const Database& db, Playthrough& pt, const MapDef& map, const std::string& id, int job, std::string& why);
// Learning spends the JP of the entry's own job; equipping needs a learned entry of another job and a free slot of its kind.
bool jobsLearn(const Database& db, Playthrough& pt, const std::string& id, int flat, std::string& why);
bool jobsEquip(const Database& db, Playthrough& pt, const std::string& id, int flat, std::string& why);
bool jobsUnequip(const Database& db, Playthrough& pt, const std::string& id, int flat, std::string& why);

}  // namespace ote
