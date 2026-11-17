// Battle quotes in saves (docs/SAVES.md "Quotes"): which quotes were said already, as "quotes": { "said": [n, ...] }
// with the quotes' positions in file order (BattleQuote::flat), ascending. A battle writes the "map" and "campaign" quotes
// said so far, a campaign's playthrough only the "campaign" ones. Written only when something was said, so a save of a
// game without quotes (or before the first quote) is as it always was; a missing key reads as "nothing said".
#include "core/battle/save_srpg.h"
#include "core/campaign/playthrough.h"

namespace ote {
namespace savejson {

namespace {

// The quotes of the game whose flag is set and that `all` (every battle quote) or only the campaign ones.
void writeSaid(const Database& db, const std::vector<uint8_t>& said, bool campaignOnly, Json& out) {
  Json list = Json::makeArray();
  for (const CharacterDef& ch : db.characters)
    for (const BattleQuote& bq : ch.battleQuotes)
      if ((size_t)bq.flat < said.size() && said[(size_t)bq.flat] && bq.once != QuoteOnce::Always &&
          (!campaignOnly || bq.once == QuoteOnce::Campaign))
        list.push(Json::makeNumber(bq.flat));
  if (list.size() == 0) return;
  Json o = Json::makeObject();
  o.set("said", list);
  out.set("quotes", o);
}

// `saved["quotes"]` into `said` (sized to the game's quotes); `campaignOnly` refuses quotes that are not "campaign" ones.
bool readSaid(Reader& rd, const Database& db, const Json& saved, const std::string& where, bool campaignOnly,
              std::vector<uint8_t>& said) {
  said.assign((size_t)db.quotes.count, 0);
  if (!saved.has("quotes")) return true;
  if (db.quotes.count == 0) return rd.fail(where + ".quotes", "this game has no quotes");
  const Json* o;
  if (!rd.object(saved, "quotes", where, o)) return false;
  const std::string at = where + ".quotes";
  for (const Json::Member& m : o->members())
    if (m.first != "said") return rd.fail(at, "unknown key \"" + m.first + "\"");
  const Json* list;
  if (!rd.array(*o, "said", at, -1, db.quotes.count, list)) return false;
  std::vector<QuoteOnce> kinds((size_t)db.quotes.count, QuoteOnce::Map);
  for (const CharacterDef& ch : db.characters)
    for (const BattleQuote& bq : ch.battleQuotes) kinds[(size_t)bq.flat] = bq.once;
  int last = -1;
  for (size_t i = 0; i < list->size(); ++i) {
    const std::string w = at + ".said[" + std::to_string(i) + "]";
    const Json& v = (*list)[i];
    if (!v.isNumber() || v.asNumber() != std::floor(v.asNumber()) || v.asNumber() < 0 || v.asNumber() >= db.quotes.count)
      return rd.fail(w, "not a quote number from 0 to " + std::to_string(db.quotes.count - 1));
    const int n = (int)v.asNumber();
    if (n <= last) return rd.fail(w, "quote numbers must go up, each once");
    last = n;
    if (kinds[(size_t)n] == QuoteOnce::Always) return rd.fail(w, "quote " + std::to_string(n) + " is said every time; it is never kept");
    if (campaignOnly && kinds[(size_t)n] != QuoteOnce::Campaign)
      return rd.fail(w, "quote " + std::to_string(n) + " is not a once-per-campaign quote");
    said[(size_t)n] = 1;
  }
  return true;
}

}  // namespace

void writeBattleQuotes(const Battle& b, Json& out) { writeSaid(*b.db, b.quotes.said, false, out); }

bool readBattleQuotes(Reader& rd, const Json& saved, Battle& b) {
  // The state Battle::init made, except what the save says (and, for a battle begun in a campaign, what it carried:
  // the save holds those flags too).
  return readSaid(rd, *b.db, saved, "battle", false, b.quotes.said);
}

void writePlaythroughQuotes(const Database& db, const Playthrough& pt, Json& out) { writeSaid(db, pt.quotesSaid, true, out); }

bool readPlaythroughQuotes(Reader& rd, const Database& db, const Json& saved, Playthrough& pt) {
  return readSaid(rd, db, saved, "playthrough", true, pt.quotesSaid);
}

}  // namespace savejson
}  // namespace ote
