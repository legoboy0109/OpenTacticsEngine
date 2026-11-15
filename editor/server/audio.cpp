#include "audio.h"

#include <algorithm>
#include <cmath>
#include <fstream>

#include "audio_info.h"
#include "log.h"
#include "paths.h"
#include "projects.h"
#include "revision.h"

namespace ote_editor {

using ote::Json;

namespace {

Json jstr(const std::string& s) { return Json::makeString(s); }

const char* const kBadName =
    "bad file name: use lower-case letters, digits, '-' and '_' (up to 40 characters, "
    "starting with a letter or digit) and end in .wav or .ogg, for example \"hit.wav\"";

bool endsWith(const std::string& s, const std::string& ext) {
  if (s.size() < ext.size()) return false;
  for (size_t i = 0; i < ext.size(); ++i) {
    char c = s[s.size() - ext.size() + i];
    if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    if (c != ext[i]) return false;
  }
  return true;
}

// Resolves audio/<name> inside the project and refuses links, so a symlink can never carry a read, write or delete
// outside the project.
bool audioPath(const fs::path& dir, const std::string& name, fs::path& out, std::string& why) {
  fs::path resolved;
  if (!resolveInside(dir, "audio/" + name, resolved, why)) return false;
  fs::path direct = dir / "audio" / name;
  std::error_code ec;
  fs::file_status st = fs::symlink_status(direct, ec);
  if (fs::exists(st) && !fs::is_regular_file(st)) {
    why = "audio/" + name + " is not a plain file (links are not followed)";
    return false;
  }
  out = direct;
  return true;
}

// Entries of the SAVED data/audio.json that name audio/<name>: [{ "kind": "music"|"sounds", "id": ... }].
Json entriesUsing(const fs::path& dir, const std::string& name) {
  Json out = Json::makeArray();
  Json doc;
  Problem p;
  if (!readJsonRel(dir, "data/audio.json", doc, p) || !doc.isObject()) return out;
  const std::string file = "audio/" + name;
  auto add = [&](const char* kind, const std::string& id) {
    Json u = Json::makeObject();
    u.set("kind", jstr(kind));
    u.set("id", jstr(id));
    out.push(u);
  };
  if (doc["music"].isObject())
    for (const auto& m : doc["music"].members())
      if (m.second.isObject() && m.second["file"].isString() && m.second["file"].asString() == file) add("music", m.first);
  if (doc["sounds"].isObject()) {
    for (const auto& m : doc["sounds"].members()) {
      if (!m.second.isObject()) continue;
      bool uses = m.second["file"].isString() && m.second["file"].asString() == file;
      const Json& list = m.second["files"];
      for (size_t i = 0; i < list.size() && !uses; ++i) uses = list[i].isString() && list[i].asString() == file;
      if (uses) add("sounds", m.first);
    }
  }
  return out;
}

Json factsJson(const AudioFacts& f, AudioKind kind) {
  Json o = Json::makeObject();
  o.set("format", jstr(kind == AudioKind::Wav ? "wav" : kind == AudioKind::Ogg ? "ogg" : "unknown"));
  if (f.ok) {
    o.set("rate", Json::makeNumber(f.rate));
    o.set("channels", Json::makeNumber(f.channels));
    if (f.seconds >= 0) o.set("seconds", Json::makeNumber(std::round(f.seconds * 1000.0) / 1000.0));
  } else {
    o.set("error", jstr(f.error));
  }
  return o;
}

}  // namespace

bool validAudioName(const std::string& name) {
  if (name.size() < 5) return false;
  const std::string ext = name.substr(name.size() - 4);
  if (ext != ".wav" && ext != ".ogg") return false;  // lower case only, like the art names
  const std::string stem = name.substr(0, name.size() - 4);
  if (stem.size() > 40) return false;
  for (size_t i = 0; i < stem.size(); ++i) {
    char c = stem[i];
    bool alnum = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
    if (!(alnum || (i > 0 && (c == '-' || c == '_')))) return false;
  }
  return true;
}

std::string audioContentType(const std::string& rel) {
  if (endsWith(rel, ".wav")) return "audio/wav";
  if (endsWith(rel, ".ogg")) return "audio/ogg";
  return "";
}

ApiResult listAudioFiles(const Config& cfg, const std::string& id) {
  fs::path dir;
  ApiResult err;
  if (!locateProject(cfg, id, dir, err)) return err;
  std::vector<std::string> names;
  fs::path adir;
  std::string why;
  std::error_code ec;
  // An audio/ that is a link leaving the project is treated as empty.
  if (resolveInside(dir, "audio", adir, why) && fs::is_directory(adir, ec)) {
    for (fs::directory_iterator it(adir, ec), end; !ec && it != end; it.increment(ec)) {
      std::error_code e2;
      std::string n = it->path().filename().string();
      if (validAudioName(n) && fs::is_regular_file(it->symlink_status(e2))) names.push_back(n);
    }
  }
  std::sort(names.begin(), names.end());
  Json arr = Json::makeArray();
  for (const std::string& n : names) {
    std::error_code e2;
    std::string bytes;
    readWholeFile(adir / n, bytes);
    AudioKind kind = sniffAudio(bytes);
    Json o = factsJson(readAudioFacts(bytes, kind), kind);
    // Key order of the entry: file, size, then the facts, then usedBy.
    Json entry = Json::makeObject();
    entry.set("file", jstr("audio/" + n));
    entry.set("size", Json::makeNumber((double)fs::file_size(adir / n, e2)));
    for (const auto& m : o.members()) entry.set(m.first, m.second);
    entry.set("usedBy", entriesUsing(dir, n));
    arr.push(entry);
  }
  Json body;
  body.set("audio", arr);
  return apiOk(body);
}

ApiResult putAudioFile(const Config& cfg, const std::string& id, const std::string& name,
                       const std::string& bytes, bool overwrite) {
  fs::path dir;
  ApiResult err;
  if (!locateProject(cfg, id, dir, err)) return err;
  // Name first: nothing below touches the filesystem for a bad name.
  if (!validAudioName(name)) return apiError(400, kBadName);
  fs::path target;
  std::string why;
  if (!audioPath(dir, name, target, why)) return apiError(400, why);

  if (bytes.empty()) return apiError(400, "the request body is empty; send the sound file's bytes");
  if (bytes.size() > kMaxAudioBytes)
    return apiError(413, "the sound file is larger than 8 MB; shorten it, or save it as .ogg (much smaller than .wav)");
  // The first bytes say what the file is, whatever its name says.
  const AudioKind kind = sniffAudio(bytes);
  const bool wantWav = name.compare(name.size() - 4, 4, ".wav") == 0;
  if (kind == AudioKind::Unknown)
    return apiError(400, "this is not a sound file (it must be a WAV, starting with \"RIFF\", or an Ogg Vorbis file, "
                         "starting with \"OggS\"); export it from your sound program as .wav or .ogg");
  if ((kind == AudioKind::Wav) != wantWav)
    return apiError(400, std::string("this file is ") + (kind == AudioKind::Wav ? "a WAV" : "an Ogg") +
                             " file but its name ends in ." + (wantWav ? "wav" : "ogg") + "; name it ." +
                             (kind == AudioKind::Wav ? "wav" : "ogg"));
  std::error_code ec;
  if (fs::exists(target, ec) && !overwrite)
    return apiError(409, "audio/" + name + " already exists; send ?overwrite=1 to replace it");

  fs::path tmp = target;
  tmp += kTempSuffix;
  fs::create_directories(target.parent_path(), ec);
  if (ec) return apiError(500, "cannot create the audio folder: " + ec.message());
  auto cleanup = [&]() { std::error_code e; fs::remove(tmp, e); };
  {
    std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
    if (f) { f.write(bytes.data(), (std::streamsize)bytes.size()); f.flush(); }
    if (!f) {
      f.close();
      cleanup();
      elog("audio upload %s/%s: cannot write temp file", id.c_str(), name.c_str());
      return apiError(500, "saving failed (disk full or folder not writable)");
    }
  }
  fs::rename(tmp, target, ec);
  if (ec) {
    cleanup();
    elog("audio upload %s/%s: rename failed: %s", id.c_str(), name.c_str(), ec.message().c_str());
    return apiError(500, "saving failed: " + ec.message());
  }
  Json facts = factsJson(readAudioFacts(bytes, kind), kind);
  Json o = Json::makeObject();
  o.set("file", jstr("audio/" + name));
  o.set("size", Json::makeNumber((double)bytes.size()));
  for (const auto& m : facts.members()) o.set(m.first, m.second);
  o.set("revision", jstr(computeRevision(dir)));
  return apiOk(o, 201);
}

ApiResult deleteAudioFile(const Config& cfg, const std::string& id, const std::string& name) {
  fs::path dir;
  ApiResult err;
  if (!locateProject(cfg, id, dir, err)) return err;
  if (!validAudioName(name)) return apiError(400, kBadName);
  fs::path target;
  std::string why;
  if (!audioPath(dir, name, target, why)) return apiError(400, why);
  std::error_code ec;
  if (!fs::is_regular_file(fs::symlink_status(target, ec))) return apiError(404, "no such file: audio/" + name);

  Json used = entriesUsing(dir, name);
  if (used.size() > 0) {
    ApiResult r = apiError(409, "audio/" + name + " is still used by a track or sound in the saved sound settings; "
                                    "remove it there (and save) first");
    r.body.set("usedBy", used);
    return r;
  }
  fs::remove(target, ec);
  if (ec) {
    elog("audio delete %s/%s: %s", id.c_str(), name.c_str(), ec.message().c_str());
    return apiError(500, "could not delete the file: " + ec.message());
  }
  Json o = Json::makeObject();
  o.set("revision", jstr(computeRevision(dir)));
  return apiOk(o);
}

}  // namespace ote_editor
