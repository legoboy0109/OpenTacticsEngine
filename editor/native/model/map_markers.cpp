#include "map_markers.h"

#include <QObject>

namespace ote_native {

using ote::Json;

namespace {
QColor sideColor(const Json& map, const std::string& side) {
  if (!side.empty())
    for (const Json& s : map["sides"].items())
      if (s["id"].asString() == side) {
        const QColor c(QString::fromStdString(s["color"].asString()));
        if (c.isValid()) return c;
      }
  return QColor(150, 150, 150);
}

bool placed(const Json& j) { return j.isObject() && j["x"].isNumber() && j["y"].isNumber(); }

QString idSuffix(const Json& j) {
  return j["id"].isString() ? QStringLiteral(" (%1)").arg(QString::fromStdString(j["id"].asString())) : QString();
}
}  // namespace

std::vector<MapMarker> readMapMarkers(const Json& map, const std::map<std::string, ote_editor::Footprint>& footprints) {
  std::vector<MapMarker> out;
  for (size_t i = 0; i < map["structures"].size(); ++i) {
    const Json& s = map["structures"][i];
    if (!placed(s)) continue;
    MapMarker m;
    m.kind = MapMarker::Kind::Structure;
    m.index = i;
    m.x = s["x"].asInt();
    m.y = s["y"].asInt();
    const std::string type = s["type"].asString();
    m.letter = QString::fromStdString(type.substr(0, 1)).toUpper();
    m.color = sideColor(map, s["side"].asString());
    const auto fp = footprints.find(type);
    if (fp != footprints.end()) {
      m.rings = fp->second.rings;
      m.w = fp->second.w;
      m.h = fp->second.h;
    }
    const QString side = s["side"].isString() ? QString::fromStdString(s["side"].asString()) : QObject::tr("neutral");
    m.tip = QObject::tr("Structure %1%2\nSide: %3\nSquare %4, %5")
                .arg(QString::fromStdString(type), idSuffix(s), side, QString::number(m.x), QString::number(m.y));
    out.push_back(m);
  }
  for (size_t i = 0; i < map["units"].size(); ++i) {
    const Json& u = map["units"][i];
    if (!placed(u)) continue;
    MapMarker m;
    m.kind = MapMarker::Kind::Unit;
    m.index = i;
    m.x = u["x"].asInt();
    m.y = u["y"].asInt();
    // A unit is placed by its kind ("type"), as a story character ("character") or by its job.
    std::string type = u["type"].asString();
    if (type.empty()) type = u["character"].asString();
    if (type.empty()) type = u["job"].asString("?");
    m.letter = QString::fromStdString(type.substr(0, 1)).toUpper();
    m.color = sideColor(map, u["side"].asString());
    QString who = QString::fromStdString(type);
    if (u["name"].isString()) who += QStringLiteral(" \"%1\"").arg(QString::fromStdString(u["name"].asString()));
    who += idSuffix(u);
    m.tip = QObject::tr("Unit %1\nSide: %2\nSquare %3, %4")
                .arg(who, QString::fromStdString(u["side"].asString()), QString::number(m.x), QString::number(m.y));
    out.push_back(m);
  }
  const Json& slotList = map["prep"]["slots"];
  for (size_t i = 0; i < slotList.size(); ++i) {
    if (!placed(slotList[i])) continue;
    MapMarker m;
    m.kind = MapMarker::Kind::PrepSlot;
    m.index = i;
    m.x = slotList[i]["x"].asInt();
    m.y = slotList[i]["y"].asInt();
    m.letter = QString::number((int)i + 1);
    m.color = QColor(240, 200, 60);
    m.tip = QObject::tr("Deployment slot %1 (preparation screen)\nSquare %2, %3").arg(i + 1).arg(m.x).arg(m.y);
    out.push_back(m);
  }
  return out;
}

}  // namespace ote_native
