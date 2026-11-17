// Reading and writing the "behaviour" block of a computer unit (docs/AI.md).
#include "core/data/behaviour.h"

#include <cctype>
#include <cmath>

#include "core/data/database.h"

namespace ote {

bool AiBehaviour::isDefault() const {
  return mode == AiMode::Advance && fromTurn == 0 && group.empty() && prefer == AiPrefer::None;
}

const char* behaviourModeName(AiMode m) {
  switch (m) {
    case AiMode::Hold: return "hold";
    case AiMode::Guard: return "guard";
    case AiMode::GuardArea: return "guardArea";
    case AiMode::Seek: return "seek";
    default: return "advance";
  }
}

namespace {

std::string q(const std::string& s) { return "\"" + s + "\""; }

bool whole(const Json& j) { return j.isNumber() && j.asNumber() == std::floor(j.asNumber()) && std::fabs(j.asNumber()) < 2e9; }

bool no(std::string& err, const std::string& msg) {
  err = msg;
  return false;
}

bool readPrefer(const Json& v, const Database& db, AiBehaviour& b, std::string& err) {
  const std::string hint = "\"prefer\" must be \"weakest\", \"nearest\", { \"unit\": id }, { \"type\": id } or { \"tag\": text }";
  if (v.isString()) {
    if (v.asString() == "weakest") b.prefer = AiPrefer::Weakest;
    else if (v.asString() == "nearest") b.prefer = AiPrefer::Nearest;
    else return no(err, hint + ", not " + q(v.asString()));
    return true;
  }
  if (!v.isObject() || v.size() != 1 || !v.members()[0].second.isString() || v.members()[0].second.asString().empty())
    return no(err, hint);
  const std::string key = v.members()[0].first, val = v.members()[0].second.asString();
  b.preferText = val;
  if (key == "unit") b.prefer = AiPrefer::Unit;
  else if (key == "tag") b.prefer = AiPrefer::Tag;
  else if (key == "type") {
    b.prefer = AiPrefer::Type;
    b.preferType = db.findUnitType(val);
    if (b.preferType < 0) return no(err, "\"prefer\" names unknown unit type " + q(val));
  } else {
    return no(err, hint);
  }
  return true;
}

bool intKey(const Json& o, const char* key, int lo, int hi, int& out, std::string& err) {
  if (!whole(o[key]) || o[key].asInt() < lo || o[key].asInt() > hi)
    return no(err, std::string("\"") + key + "\" must be a whole number from " + std::to_string(lo) + " to " + std::to_string(hi));
  out = o[key].asInt();
  return true;
}

}  // namespace

bool parseBehaviour(const Json& v, const Database& db, AiBehaviour& out, std::string& err) {
  AiBehaviour b;
  const std::string modes = "\"advance\", \"hold\", \"guard\", \"guardArea\" or \"seek\"";
  if (v.isString()) {
    const std::string m = v.asString();
    if (m == "advance") b.mode = AiMode::Advance;
    else if (m == "hold") b.mode = AiMode::Hold;
    else if (m == "guard") b.mode = AiMode::Guard;
    else if (m == "guardArea" || m == "seek")
      return no(err, q(m) + " needs more than a name (" + (m == "seek" ? "what to head for" : "an area or a distance") +
                         "): write { \"mode\": " + q(m) + ", ... }");
    else return no(err, "\"behaviour\" must be " + modes + ", not " + q(m));
    out = b;
    return true;
  }
  if (!v.isObject()) return no(err, "\"behaviour\" must be a mode name such as \"hold\" or an object such as { \"mode\": \"guard\" }");
  static const char* const kKeys[] = {"mode", "wake", "group", "fromTurn", "area", "within", "unit", "place", "x", "y", "prefer"};
  for (const Json::Member& m : v.members()) {
    bool known = false;
    for (const char* k : kKeys) known = known || m.first == k;
    if (!known)
      return no(err, "\"behaviour\" has an unknown key " + q(m.first) + " (it may have mode, wake, group, fromTurn, area, within, unit, place, x, y, prefer)");
  }
  if (v.has("mode")) {
    if (!v["mode"].isString()) return no(err, "\"mode\" must be " + modes);
    const std::string m = v["mode"].asString();
    if (m == "advance") b.mode = AiMode::Advance;
    else if (m == "hold") b.mode = AiMode::Hold;
    else if (m == "guard") b.mode = AiMode::Guard;
    else if (m == "guardArea") b.mode = AiMode::GuardArea;
    else if (m == "seek") b.mode = AiMode::Seek;
    else return no(err, "\"mode\" must be " + modes + ", not " + q(m));
  }
  const bool guards = b.mode == AiMode::Guard || b.mode == AiMode::GuardArea;
  if (v.has("wake")) {
    if (b.mode != AiMode::Guard) return no(err, "\"wake\" goes with mode \"guard\" only");
    const std::string w = v["wake"].asString();
    if (!v["wake"].isString() || (w != "once" && w != "react"))
      return no(err, "\"wake\" must be \"once\" (wakes for good) or \"react\" (only hits what it can reach this turn)");
    b.react = w == "react";
  }
  if (v.has("group")) {
    const std::string g = v["group"].asString();
    if (!guards) return no(err, "\"group\" goes with mode \"guard\" or \"guardArea\" (a group wakes together)");
    if (!v["group"].isString() || g.empty()) return no(err, "\"group\" must be a name (text that is not empty)");
    for (char ch : g)
      if (!(std::isalnum((unsigned char)ch) || ch == '_' || ch == '-'))
        return no(err, "\"group\" must be letters, digits, \"_\" and \"-\" (got " + q(g) + ")");
    b.group = g;
  }
  if (v.has("fromTurn")) {
    int n = 0;
    if (!intKey(v, "fromTurn", 1, kMaxBehaviourTurn, n, err)) return false;
    b.fromTurn = n <= 1 ? 0 : n;
  }
  if ((v.has("area") || v.has("within")) && b.mode != AiMode::GuardArea) return no(err, "\"area\" and \"within\" go with mode \"guardArea\"");
  if (b.mode == AiMode::GuardArea) {
    if (v.has("area")) {
      const Json& a = v["area"];
      if (!a.isObject()) return no(err, "\"area\" must be a rectangle { \"x\": 5, \"y\": 2, \"w\": 3, \"h\": 2 }");
      for (const Json::Member& m : a.members())
        if (m.first != "x" && m.first != "y" && m.first != "w" && m.first != "h")
          return no(err, "\"area\" has an unknown key " + q(m.first) + " (a rectangle has x, y, w, h)");
      if (!a.has("x") || !a.has("y")) return no(err, "\"area\" needs \"x\" and \"y\" (its top-left square)");
      if (!intKey(a, "x", 0, 9999, b.ax, err) || !intKey(a, "y", 0, 9999, b.ay, err)) return false;
      if (a.has("w") && !intKey(a, "w", 1, 9999, b.aw, err)) return false;
      if (a.has("h") && !intKey(a, "h", 1, 9999, b.ah, err)) return false;
      b.hasArea = true;
    }
    if (v.has("within") && !intKey(v, "within", 1, kMaxBehaviourWithin, b.within, err)) return false;
    if (!b.hasArea && b.within == 0)
      return no(err, "mode \"guardArea\" needs an \"area\" and / or \"within\" (how many squares away someone wakes it)");
  }
  if ((v.has("unit") || v.has("place") || v.has("x") || v.has("y")) && b.mode != AiMode::Seek)
    return no(err, "\"unit\", \"place\", \"x\" and \"y\" go with mode \"seek\"");
  if (b.mode == AiMode::Seek) {
    const int targets = (v.has("unit") ? 1 : 0) + (v.has("place") ? 1 : 0) + ((v.has("x") || v.has("y")) ? 1 : 0);
    if (targets != 1) return no(err, "mode \"seek\" needs exactly one target: a \"unit\", a \"place\" or a square (\"x\" and \"y\")");
    if (v.has("unit")) {
      if (!v["unit"].isString() || v["unit"].asString().empty()) return no(err, "\"unit\" must be a unit id");
      b.seekUnit = v["unit"].asString();
    } else if (v.has("place")) {
      if (!v["place"].isString() || v["place"].asString().empty()) return no(err, "\"place\" must be the id of a structure of the map");
      b.seekPlace = v["place"].asString();
    } else {
      if (!v.has("x") || !v.has("y")) return no(err, "a square needs both \"x\" and \"y\"");
      if (!intKey(v, "x", 0, 9999, b.sx, err) || !intKey(v, "y", 0, 9999, b.sy, err)) return false;
      b.seekCell = true;
    }
  }
  if (v.has("prefer") && !readPrefer(v["prefer"], db, b, err)) return false;
  out = b;
  return true;
}

Json behaviourToJson(const AiBehaviour& b) {
  Json o = Json::makeObject();
  if (b.mode != AiMode::Advance) o.set("mode", Json::makeString(behaviourModeName(b.mode)));
  if (b.react) o.set("wake", Json::makeString("react"));
  if (!b.group.empty()) o.set("group", Json::makeString(b.group));
  if (b.fromTurn > 0) o.set("fromTurn", Json::makeNumber(b.fromTurn));
  if (b.hasArea) {
    Json a = Json::makeObject();
    a.set("x", Json::makeNumber(b.ax));
    a.set("y", Json::makeNumber(b.ay));
    a.set("w", Json::makeNumber(b.aw));
    a.set("h", Json::makeNumber(b.ah));
    o.set("area", a);
  }
  if (b.within > 0) o.set("within", Json::makeNumber(b.within));
  if (!b.seekUnit.empty()) o.set("unit", Json::makeString(b.seekUnit));
  if (!b.seekPlace.empty()) o.set("place", Json::makeString(b.seekPlace));
  if (b.seekCell) {
    o.set("x", Json::makeNumber(b.sx));
    o.set("y", Json::makeNumber(b.sy));
  }
  if (b.prefer == AiPrefer::Weakest) o.set("prefer", Json::makeString("weakest"));
  else if (b.prefer == AiPrefer::Nearest) o.set("prefer", Json::makeString("nearest"));
  else if (b.prefer != AiPrefer::None) {
    Json p = Json::makeObject();
    p.set(b.prefer == AiPrefer::Unit ? "unit" : b.prefer == AiPrefer::Type ? "type" : "tag", Json::makeString(b.preferText));
    o.set("prefer", p);
  }
  return o;
}

}  // namespace ote
