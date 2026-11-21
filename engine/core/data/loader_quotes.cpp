// Battle and defeat quotes of data/characters.json (docs/EVENTS.md "Battle and defeat quotes"):
//   "quotes": { "battle": [ { "against": "<character id>", "once": "map", "lines": [ { "say": "..." } ] } ],
//               "defeat": { "falls": [ lines ], "retreats": [ lines ] } }
// Read after every character is known (a quote may name any of them, a line may be spoken by any of them).
// The lines are ordinary "say" commands (readSayLines, shared with support conversations); their wording goes to
// Database::quotes.texts.
#include "core/data/loader_srpg.h"
#include "core/data/loader_util.h"

namespace ote {

namespace {

std::string q(const std::string& s) { return "\"" + s + "\""; }

bool onlyKeysOf(const LoadCtx& c, const Json& o, std::initializer_list<const char*> keys, const std::string& where) {
  return c.onlyKeys(o, keys, where);
}

bool readBattleQuote(const LoadCtx& c, Database& db, const CharacterDef& owner, const Json& e, size_t index, BattleQuote& out) {
  const std::string where = "character " + q(owner.id) + ": battle quote " + std::to_string(index + 1);
  if (!e.isObject()) return c.fail(where, "must be an object such as { \"lines\": [ { \"say\": \"Hello\" } ] }");
  if (!onlyKeysOf(c, e, {"against", "once", "lines"}, where)) return false;
  if (e.has("against")) {
    if (!c.str(e, "against", where, out.againstId)) return false;
    out.against = db.findCharacter(out.againstId);
    if (out.against < 0) return c.fail(where, "\"against\" names unknown character " + q(out.againstId));
    if (out.againstId == owner.id) return c.fail(where, "\"against\" names the character itself; a character cannot fight itself");
  }
  if (e.has("once")) {
    std::string v;
    if (!c.str(e, "once", where, v)) return false;
    if (v == "map") out.once = QuoteOnce::Map;
    else if (v == "campaign") out.once = QuoteOnce::Campaign;
    else if (v == "always") out.once = QuoteOnce::Always;
    else return c.fail(where, "\"once\" must be \"map\", \"campaign\" or \"always\", not " + q(v));
  }
  if (!e.has("lines") || !e["lines"].isArray() || e["lines"].size() == 0)
    return c.fail(where, "\"lines\" must be a list with at least one say command");
  return readSayLines(c, db, db.quotes.texts, e["lines"], where + ": \"lines\"", "a quote", owner.id, out.lines);
}

bool readCharacterQuote(const LoadCtx& c, Database& db, CharacterDef& ch, const Json& e) {
  const std::string where = "character " + q(ch.id) + ": \"quotes\"";
  if (!e.isObject()) return c.fail(where, "must be an object such as { \"battle\": [], \"defeat\": {} }");
  if (!onlyKeysOf(c, e, {"battle", "defeat"}, where)) return false;
  if (e.has("battle")) {
    if (!e["battle"].isArray()) return c.fail(where, "\"battle\" must be a list of quotes");
    for (size_t i = 0; i < e["battle"].size(); ++i) {
      BattleQuote bq;
      if (!readBattleQuote(c, db, ch, e["battle"][i], i, bq)) return false;
      // One quote for each opponent (and one for anyone): a second for the same would never be said.
      for (size_t k = 0; k < ch.battleQuotes.size(); ++k)
        if (ch.battleQuotes[k].againstId == bq.againstId)
          return c.fail("character " + q(ch.id) + ": battle quote " + std::to_string(i + 1),
                        std::string("a second quote ") + (bq.againstId.empty() ? "against anyone" : "against " + q(bq.againstId)) +
                            " (quote " + std::to_string(k + 1) + " has it already)");
      bq.flat = db.quotes.count++;
      ch.battleQuotes.push_back(bq);
    }
  }
  if (e.has("defeat")) {
    const Json& d = e["defeat"];
    const std::string at = where + ": \"defeat\"";
    if (!d.isObject()) return c.fail(at, "must be an object such as { \"falls\": [ { \"say\": \"Farewell\" } ] }");
    if (!onlyKeysOf(c, d, {"falls", "retreats"}, at)) return false;
    if (d.has("falls") && !readSayLines(c, db, db.quotes.texts, d["falls"], at + ": \"falls\"", "a quote", ch.id, ch.fallsLines))
      return false;
    if (d.has("retreats") &&
        !readSayLines(c, db, db.quotes.texts, d["retreats"], at + ": \"retreats\"", "a quote", ch.id, ch.retreatsLines))
      return false;
  }
  return true;
}

}  // namespace

bool readCharacterQuotes(const LoadCtx& c, Database& db, const Json& list) {
  db.quotes = QuoteData();
  for (size_t i = 0; i < db.characters.size() && i < list.size(); ++i)
    if (list[i].has("quotes") && !readCharacterQuote(c, db, db.characters[i], list[i]["quotes"])) return false;
  return true;
}

}  // namespace ote
