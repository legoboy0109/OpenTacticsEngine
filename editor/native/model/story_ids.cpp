#include "story_ids.h"

#include "id_source.h"

namespace ote_native {

namespace {

void add(QStringList& out, const ote::Json& v) {
  if (!v.isString() || v.asString().empty()) return;
  const QString s = QString::fromStdString(v.asString());
  if (!out.contains(s)) out.append(s);
}

// addUnit commands anywhere in a command list: inside `if` branches (then / else) and a choice's answers.
void addedUnits(const ote::Json& commands, QStringList& out, int depth) {
  if (depth > 12) return;  // a pathological file must not recurse forever
  for (const ote::Json& c : commands.items()) {
    if (!c.isObject()) continue;
    const ote::Json& u = c["addUnit"];
    if (u.isObject()) {
      add(out, u["id"]);
      add(out, u["character"]);
    }
    addedUnits(c["then"], out, depth + 1);
    addedUnits(c["else"], out, depth + 1);
    for (const ote::Json& a : c["answers"].items()) addedUnits(a["do"], out, depth + 1);
  }
}

}  // namespace

QStringList storyUnitIds(const ote::Json& map) {
  QStringList out;
  for (const ote::Json& u : map["units"].items()) {
    add(out, u["id"]);
    add(out, u["character"]);
  }
  for (const ote::Json& e : map["events"].items()) addedUnits(e["do"], out, 0);
  return out;
}

QStringList teamIds(const ote::Json& map) {
  QStringList out, sides;
  for (const ote::Json& s : map["sides"].items()) {
    add(out, s["team"].isString() && !s["team"].asString().empty() ? s["team"] : s["id"]);
    add(sides, s["id"]);
  }
  for (const QString& s : sides)
    if (!out.contains(s)) out.append(s);
  return out;
}

QStringList resolveSources(const ote::Json& doc, IdSource* ids, const std::vector<std::string>& sources) {
  QStringList out;
  auto addAll = [&out](const QStringList& l) {
    for (const QString& id : l)
      if (!out.contains(id)) out.append(id);
  };
  std::vector<std::string> others;
  for (const std::string& src : sources) {
    if (src.rfind("keys:@", 0) == 0) {
      const ote::Json* obj = &doc;
      for (const QString& part : QString::fromStdString(src.substr(6)).split('.')) obj = &(*obj)[part.toStdString()];
      if (obj->isObject())
        for (const auto& m : obj->members()) addAll({QString::fromStdString(m.first)});
    } else if (src == "@storyUnits") {
      addAll(storyUnitIds(doc));
    } else if (src == "@teams") {
      addAll(teamIds(doc));
    } else if (!src.empty() && src[0] == '@') {
      // "@units": the list under that key path of the file being edited (a map's own units, sides, structures ...).
      const ote::Json* list = &doc;
      for (const QString& part : QString::fromStdString(src.substr(1)).split('.')) list = &(*list)[part.toStdString()];
      for (const ote::Json& item : list->items()) {
        const QString id = QString::fromStdString(item.isString() ? item.asString() : item["id"].asString());
        if (!id.isEmpty()) addAll({id});
      }
    } else {
      others.push_back(src);
    }
  }
  if (!others.empty() && ids) addAll(ids->ids(others));
  return out;
}

}  // namespace ote_native
