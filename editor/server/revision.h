// Project revision = hash of the project's files, used to detect that the
// folder changed on disk (e.g. synced from another machine) between a GET
// and a PUT.
#pragma once
#include <string>
#include <utility>
#include <vector>

#include "config.h"

namespace ote_editor {

// Every regular file (symlinks skipped, temp files skipped) as
// {relative path with '/', absolute path}, sorted by relative path.
std::vector<std::pair<std::string, fs::path>> listProjectFiles(const fs::path& dir);

// 16 hex chars: FNV-1a 64 over sorted relative paths and file bytes. Cached per project by (path, size, mtime) of every
// file, so an unchanged folder costs a directory scan, not a read of every byte (see revision.cpp). Thread-safe.
std::string computeRevision(const fs::path& dir);

}  // namespace ote_editor
