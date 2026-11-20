// Loaders of the job system (docs/JOBS.md): game.json "rules"."jobs", data/jobs.json, and the job keys of characters and map units. Every refusal
// names the file, the entry and the key.
#pragma once
#include "core/data/database.h"
#include "core/data/loader_util.h"

namespace ote {

// game.json "rules"."jobs": the JP, the slots, the job-level table, where jobs may be changed. Without the block the defaults of JobRules stand.
bool loadJobRules(const LoadCtx& c, const Json& rules, Database& db);
// data/jobs.json (optional). After skills, abilities and unit types (it names them), before characters (which name jobs). Fills Database::jobs and
// jobEntries; a game without the file keeps both empty and is refused when game.json wrote "rules.jobs".
bool loadJobs(const std::string& path, Database& db, std::string& err);
// The job keys of a character or a map unit / addUnit: "job", "jobs", "equipped", "learned". `given` tells whether any was written (then `out` is the state
// they describe, replacing whatever it held). Refused in a game without jobs.
bool readJobKeys(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, JobState& out, bool& given);
// The keys a unit entry with job state may write (for key lists that refuse unknown keys).
bool isJobKey(const std::string& key);

}  // namespace ote
