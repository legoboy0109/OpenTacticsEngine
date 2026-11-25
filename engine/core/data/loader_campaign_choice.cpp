// Campaign loader: the choice command of a scene (docs/EVENTS.md "Dialogue choices"). Same JSON as the event command
// (loader_story_choice.cpp); an answer's commands are scene commands, so "goto" is legal in them wherever it is legal
// in the scene itself (a chapter's "after").
#include <algorithm>

#include "core/data/loader_campaign.h"
#include "core/data/loader_dialogue.h"

namespace ote {

bool CampaignLoader::choiceCommand(const Json& e, const std::string& at, bool allowGoto, SceneCommand& out) {
  if (!onlyKeys(e, {"choice", "speaker", "speakerName", "portrait", "store", "answers", "notice", "cursor", "face"}, at)) return false;
  const Json& v = e["choice"];
  if (!v.isString() || v.asString().empty()) return fail(at, "\"choice\" must be the question to ask (text that is not empty)");
  if (!checkNameTokens(c_, v.asString(), at)) return false;
  // "notice", "cursor" and "face" (docs/EVENTS.md "Notices"), as for a map event's choice.
  if (!readChoiceLook(c_, db_, e, at, e["answers"].isArray() ? e["answers"].size() : 0, out.fx)) return false;
  out.text = text(v.asString());
  if (e.has("speaker") && e.has("speakerName")) return fail(at, "give \"speaker\" or \"speakerName\", not both");
  if (e.has("speaker")) {
    if (!e["speaker"].isString() || !isStoryId(e["speaker"].asString()))
      return fail(at, "\"speaker\" must be a roster unit id (letters, digits, \"_\" and \"-\")");
    out.unit = e["speaker"].asString();
  }
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
      return fail(at, "\"store\" names unknown variable " + quoted(e["store"].isString() ? e["store"].asString() : "") +
                          " (game.json \"variables\")");
    if (std::find(choiceStores_.begin(), choiceStores_.end(), out.index) != choiceStores_.end())
      return fail(at, "\"store\" names variable " + quoted(e["store"].asString()) +
                          ", which another choice of this scene already writes; use a different variable");
    choiceStores_.push_back(out.index);
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
    SceneAnswer ans;
    if (!a.has("text") || !a["text"].isString() || a["text"].asString().empty())
      return fail(where, "\"text\" must be the words of the answer (text that is not empty)");
    for (const SceneAnswer& o : out.answers)
      if (def_.texts[(size_t)o.text] == a["text"].asString())
        return fail(where, "says the same as an earlier answer (" + quoted(a["text"].asString()) +
                               "); the player could not tell them apart");
    if (!checkNameTokens(c_, a["text"].asString(), where)) return false;
    ans.text = text(a["text"].asString());
    if (a.has("if")) {
      if (!a["if"].isArray()) return fail(where, "\"if\" must be a list of conditions");
      for (size_t k = 0; k < a["if"].size(); ++k) {
        SceneCondition cnd;
        if (!condition(a["if"][k], where + ": condition " + std::to_string(k + 1), cnd)) return false;
        ans.cond.push_back(cnd);
      }
    }
    if (a.has("do") && !scene(a["do"], where + ": \"do\"", allowGoto, ans.commands)) return false;
    out.answers.push_back(ans);
  }
  return true;
}

}  // namespace ote
