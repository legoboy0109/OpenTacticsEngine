// Event loader: the choice command (docs/EVENTS.md "Dialogue choices"), a question with two to four answers, each
// with its own list of commands. The same shape is read for campaign scenes in loader_campaign_choice.cpp.
#include <algorithm>

#include "core/data/loader_events.h"

namespace ote {

namespace {
std::string quoted(const std::string& s) { return "\"" + s + "\""; }
}  // namespace

bool StoryLoader::choiceCommand(const Json& e, const std::string& at, bool talk, Command& out, bool& usesEndTurn) {
  if (!onlyKeys(e, {"choice", "speaker", "speakerName", "portrait", "store", "answers"}, at)) return false;
  const Json& v = e["choice"];
  if (!v.isString() || v.asString().empty()) return fail(at, "\"choice\" must be the question to ask (text that is not empty)");
  out.text = text(v.asString());
  if (e.has("speaker") && e.has("speakerName")) return fail(at, "give \"speaker\" or \"speakerName\", not both");
  if (e.has("speaker") && !unitId(e["speaker"], at, "\"speaker\"", out.unit)) return false;
  std::string s;
  if (e.has("speakerName")) {
    if (!c_.str(e, "speakerName", at, s)) return false;
    out.speakerName = text(s);
  }
  if (e.has("portrait")) {
    if (!c_.str(e, "portrait", at, s)) return false;
    out.portrait = text(s);
  }
  if (e.has("store")) {
    if (!e["store"].isString() || (out.index = db_.findVariable(e["store"].asString())) < 0)
      return fail(at, "\"store\" names unknown variable " + quoted(e["store"].asString()) + " (game.json \"variables\")");
    // Two questions writing one variable would silently overwrite each other: nearly always a copy that was not edited.
    if (std::find(choiceStores.begin(), choiceStores.end(), out.index) != choiceStores.end())
      return fail(at, "\"store\" names variable " + quoted(e["store"].asString()) +
                          ", which another choice of this map already writes; use a different variable");
    choiceStores.push_back(out.index);
  }
  const Json* list;
  if (!c_.array(e, "answers", at, list, true)) return false;
  if ((int)list->size() < kMinChoiceAnswers || (int)list->size() > kMaxChoiceAnswers)
    return fail(at, "\"answers\" has " + std::to_string(list->size()) + " answer" + (list->size() == 1 ? "" : "s") +
                        "; a question needs two to four");
  for (size_t i = 0; i < list->size(); ++i) {
    const Json& a = (*list)[i];
    std::string where = at + ": answer " + std::to_string(i + 1);
    if (!a.isObject()) return fail(where, "must be an object such as { \"text\": \"Yes\", \"do\": [ ... ] }");
    if (!onlyKeys(a, {"text", "if", "do"}, where)) return false;
    ChoiceAnswer ans;
    if (!a.has("text") || !a["text"].isString() || a["text"].asString().empty())
      return fail(where, "\"text\" must be the words of the answer (text that is not empty)");
    for (const ChoiceAnswer& o : out.answers)
      if (data_.texts[(size_t)o.text] == a["text"].asString())
        return fail(where, "says the same as an earlier answer (" + quoted(a["text"].asString()) +
                               "); the player could not tell them apart");
    ans.text = text(a["text"].asString());
    if (a.has("if") && !conditions(a["if"], where, ans.cond)) return false;
    if (a.has("do") && !commands(a["do"], where + ": \"do\"", talk, ans.commands, usesEndTurn)) return false;
    out.answers.push_back(ans);
  }
  data_.hasChoice = true;
  return true;
}

}  // namespace ote
