#include "txn.h"

#include <algorithm>
#include <atomic>
#include <chrono>

#include "diff.h"
#include "paths.h"
#include "revision.h"
#include "validate.h"

namespace ote_mcp {

using ote_editor::Problem;

Json problemsJson(const std::vector<Problem>& p, size_t max) {
  Json a = jarr();
  for (size_t i = 0; i < p.size() && i < max; ++i) {
    Json o = jobj();
    o.set("file", jstr(p[i].file));
    o.set("message", jstr(p[i].message));
    a.push(o);
  }
  return a;
}

Json* Txn::file(const std::string& rel, std::string& err) {
  if (Entry* e = entry(rel)) {
    if (e->removed) { err = rel + " is being removed in this same call"; return nullptr; }
    return &e->working;
  }
  LoadedJson l;
  // Not "seen": the model has not been shown this file by this call; the conflict check compares against what it saw earlier.
  if (!loadJsonFile(ctx_, game_, rel, l, err, false)) return nullptr;
  Entry e;
  e.rel = rel;
  ctx_.resolveFile(game_, rel, e.path, err);
  e.working = l.value;
  e.originalBytes = l.bytes;
  e.existed = true;
  e.hadComments = l.hasComments;
  return &(files_[rel] = std::move(e)).working;
}

bool Txn::create(const std::string& rel, const Json& content, std::string& err) {
  fs::path full;
  if (!ctx_.resolveFile(game_, rel, full, err)) return false;
  if (!ote_editor::hasExtension(rel, ".json")) { err = "\"" + rel + "\" must end in .json"; return false; }
  std::error_code ec;
  if (fs::exists(fs::symlink_status(full, ec)) || entry(rel)) { err = rel + " already exists (use set / add_entry to change it)"; return false; }
  Entry e;
  e.rel = rel;
  e.path = full;
  e.working = content;
  files_[rel] = std::move(e);
  return true;
}

bool Txn::remove(const std::string& rel, std::string& err) {
  fs::path full;
  if (!ctx_.resolveFile(game_, rel, full, err)) return false;
  std::error_code ec;
  if (!fs::is_regular_file(fs::symlink_status(full, ec))) { err = "no such file: " + rel; return false; }
  if (rel == "game.json") { err = "game.json cannot be removed (it is what makes the folder a game)"; return false; }
  Entry* e = entry(rel);
  if (!e) {
    Entry n;
    n.rel = rel;
    n.path = full;
    n.existed = true;
    ote_editor::readWholeFile(full, n.originalBytes);
    n.hadComments = false;
    files_[rel] = std::move(n);
    e = entry(rel);
  }
  e->removed = true;
  return true;
}

ote_editor::DocLookup Txn::docLookup() {
  return [this](const std::string& rel) -> const Json* { return peek(rel); };
}

const Json* Txn::peek(const std::string& rel) {
  if (Entry* e = entry(rel)) return e->removed ? nullptr : &e->working;
  auto it = peeked_.find(rel);
  if (it != peeked_.end()) return &it->second;
  if (missing_.count(rel)) return nullptr;
  LoadedJson l;
  std::string err;
  if (!loadJsonFile(ctx_, game_, rel, l, err, false)) { missing_.insert(rel); return nullptr; }
  return &(peeked_[rel] = l.value);
}

std::vector<std::string> Txn::knownFiles() {
  std::set<std::string> names;
  for (const auto& f : ote_editor::listProjectFiles(game_.dir))
    if (ote_editor::hasExtension(f.first, ".json")) names.insert(f.first);
  for (const auto& kv : files_) {
    if (kv.second.removed) names.erase(kv.first);
    else names.insert(kv.first);
  }
  return std::vector<std::string>(names.begin(), names.end());
}

namespace {

std::string key(const Problem& p) { return p.file + "\n" + p.message; }

std::string uniqueTempName() {
  static std::atomic<unsigned> counter{0};
  const auto t = std::chrono::steady_clock::now().time_since_epoch().count();
  return "ote-mcp-" + std::to_string((unsigned long long)t) + "-" + std::to_string(counter++);
}

// Copies the game folder (regular files only, links skipped) so a dry run can be checked without touching the real one.
bool copyGame(const fs::path& from, const fs::path& to, std::string& err) {
  std::error_code ec;
  fs::create_directories(to, ec);
  fs::copy(from, to, fs::copy_options::recursive | fs::copy_options::skip_symlinks, ec);
  if (ec) { err = "cannot make a temporary copy for the dry run: " + ec.message(); return false; }
  return true;
}

}  // namespace

Json Txn::commit(const CommitOptions& opt, bool& failed) {
  failed = false;
  Json report = jobj();
  report.set("dryRun", jbool(opt.dryRun));
  Json warnings = jarr();
  Json filesJson = jarr();

  struct Planned { Entry* e; std::string after; std::string beforeNorm; bool changed; };
  std::vector<Planned> plan;
  for (auto& kv : files_) {
    Entry& e = kv.second;
    Planned p{&e, "", "", false};
    if (e.removed) {
      p.changed = true;
    } else {
      p.after = e.working.dump(2) + "\n";
      if (e.existed) {
        Json orig;
        std::string perr;
        Json::parse(e.originalBytes, orig, perr);
        p.beforeNorm = orig.dump(2) + "\n";
        p.changed = p.after != p.beforeNorm;
      } else {
        p.changed = true;
      }
    }
    plan.push_back(p);
  }

  // Conflicts: a file that changed on disk since the model last read it, or since this call loaded it.
  std::vector<std::string> conflicts;
  for (const Planned& p : plan) {
    if (!p.changed || !p.e->existed) continue;
    std::string now;
    if (!ote_editor::readWholeFile(p.e->path, now)) { conflicts.push_back(p.e->rel + " vanished from disk"); continue; }
    if (now != p.e->originalBytes) { conflicts.push_back(p.e->rel + " changed on disk while the call was running"); continue; }
    auto seen = ctx_.seen.find(p.e->path.string());
    if (seen != ctx_.seen.end() && seen->second != hashHex(now))
      conflicts.push_back(p.e->rel + " was changed on disk (by someone else, e.g. an editor) after you last read it");
  }
  if (!conflicts.empty() && !opt.force) {
    failed = true;
    std::string all;
    for (const std::string& c : conflicts) all += (all.empty() ? "" : "; ") + c;
    report.set("error", jstr("nothing was written: " + all));
    report.set("next", jstr("read the file again (get) to see the current content and redo the change on it, or pass force=true to overwrite the disk version anyway"));
    return report;
  }
  if (!conflicts.empty()) {
    std::string all;
    for (const std::string& c : conflicts) all += (all.empty() ? "" : "; ") + c;
    warnings.push(jstr("force: overwrote changes made on disk: " + all));
  }

  // Per-file report and warnings.
  bool anyChange = false;
  for (const Planned& p : plan) {
    Json f = jobj();
    f.set("file", jstr(p.e->rel));
    Json notes = jarr();
    if (p.e->removed) {
      f.set("action", jstr("removed"));
      anyChange = true;
    } else if (!p.e->existed) {
      f.set("action", jstr("created"));
      DiffResult d = unifiedDiff("", p.after, 2, 40);
      f.set("lines", jnum((double)d.added));
      f.set("diff", jstr(d.text));
      anyChange = true;
    } else if (!p.changed) {
      f.set("action", jstr("unchanged"));
    } else {
      f.set("action", jstr("changed"));
      DiffResult d = unifiedDiff(p.beforeNorm, p.after);
      f.set("diff", jstr(d.text));
      f.set("added", jnum(d.added));
      f.set("removed", jnum(d.removed));
      anyChange = true;
      if (p.e->hadComments) notes.push(jstr("this file has // comments; saving drops them (this server writes plain JSON)"));
      if (p.e->originalBytes != p.beforeNorm) notes.push(jstr("the file is re-written as plain 2-space JSON, so spacing outside the edit may change; the data is the same"));
    }
    if (notes.size() > 0) f.set("notes", notes);
    filesJson.push(f);
  }
  report.set("files", filesJson);
  if (!anyChange) {
    report.set("changed", jbool(false));
    report.set("note", jstr("nothing to change: the values are already as requested"));
    report.set("warnings", warnings);
    return report;
  }
  report.set("changed", jbool(true));

  // Problems before (cached per folder revision) and after.
  const std::vector<Problem> before = ctx_.problemsOf(game_);
  std::vector<Problem> after;
  std::vector<std::pair<fs::path, std::string>> rollback;  // (file, original bytes; "\0" marker = did not exist)
  std::vector<fs::path> createdNow;
  if (opt.dryRun) {
    const fs::path tmp = fs::temp_directory_path() / uniqueTempName();
    const fs::path copy = tmp / game_.dir.filename();
    std::string err;
    if (!copyGame(game_.dir, copy, err)) {
      std::error_code ec;
      fs::remove_all(tmp, ec);
      failed = true;
      report.set("error", jstr(err));
      return report;
    }
    for (const Planned& p : plan) {
      if (!p.changed) continue;
      fs::path target = copy / fs::relative(p.e->path, game_.dir);
      std::error_code ec;
      if (p.e->removed) fs::remove(target, ec);
      else ote_editor::writeFileAtomic(target, p.after, err);
    }
    after = ote_editor::validateProject(copy);
    // Messages name the copy's folder inside their text sometimes; show the real name.
    std::error_code ec;
    fs::remove_all(tmp, ec);
  } else {
    std::string err;
    bool ok = true;
    for (const Planned& p : plan) {
      if (!p.changed) continue;
      std::error_code ec;
      const bool existed = fs::exists(p.e->path, ec);
      std::string old;
      if (existed) ote_editor::readWholeFile(p.e->path, old);
      if (p.e->removed) {
        fs::remove(p.e->path, ec);
        if (ec) { ok = false; err = "cannot remove " + p.e->rel + ": " + ec.message(); }
      } else {
        ok = ote_editor::writeFileAtomic(p.e->path, p.after, err);
      }
      if (!ok) break;
      if (existed) rollback.emplace_back(p.e->path, old);
      else createdNow.push_back(p.e->path);
    }
    if (!ok) {
      // Put every file back the way it was; the game never stays half-edited.
      std::string ignore;
      for (const auto& r : rollback) ote_editor::writeFileAtomic(r.first, r.second, ignore);
      for (const fs::path& c : createdNow) { std::error_code ec; fs::remove(c, ec); }
      failed = true;
      report.set("error", jstr("nothing was changed: " + err));
      return report;
    }
    for (const Planned& p : plan) {
      if (!p.changed) continue;
      if (p.e->removed) ctx_.seen.erase(p.e->path.string());
      else ctx_.noteSeen(p.e->path, p.after);
    }
    after = ote_editor::validateProject(game_.dir);
    ctx_.rememberProblems(game_, after);
  }

  std::set<std::string> beforeKeys, afterKeys;
  for (const Problem& p : before) beforeKeys.insert(key(p));
  for (const Problem& p : after) afterKeys.insert(key(p));
  std::vector<Problem> fresh;
  size_t fixed = 0;
  for (const Problem& p : after) if (!beforeKeys.count(key(p))) fresh.push_back(p);
  for (const Problem& p : before) if (!afterKeys.count(key(p))) ++fixed;

  Json names = jarr();
  for (const Planned& p : plan) if (p.changed) names.push(jstr(p.e->rel));
  report.set(opt.dryRun ? "wouldWrite" : "wrote", names);
  report.set("problemsBefore", jnum((double)before.size()));
  report.set("problemsAfter", jnum((double)after.size()));
  if (!fresh.empty()) report.set("newProblems", problemsJson(fresh));
  if (fixed > 0) report.set("fixedProblems", jnum((double)fixed));
  if (!after.empty()) report.set("problems", problemsJson(after));
  if (!fresh.empty())
    warnings.push(jstr(std::string(opt.dryRun ? "this change would introduce " : "this change introduced ") + std::to_string(fresh.size()) +
                       " project problem(s) (see newProblems); fix them or undo the change"));
  report.set("warnings", warnings);
  return report;
}

}  // namespace ote_mcp
