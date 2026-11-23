// Battle and defeat quotes in a battle (docs/EVENTS.md "Battle and defeat quotes"). They are emitted as ordinary
// Dialogue entries, in the same event list as the strikes, so every consumer sees one ordered stream; nothing here waits.
#include "core/battle/battle.h"
#include "core/data/campaign.h"
#include "core/data/scene_fx.h"

namespace ote {

void Battle::quotesInit() { quotes.said.assign((size_t)db->quotes.count, 0); }

void Battle::setStartingQuotes(const std::vector<uint8_t>& carried) {
  if (carried.size() != quotes.said.size()) return;
  for (const CharacterDef& ch : db->characters)
    for (const BattleQuote& bq : ch.battleQuotes)
      if (bq.once == QuoteOnce::Campaign) quotes.said[(size_t)bq.flat] = carried[(size_t)bq.flat];
}

namespace {

// The quote `speaker` says when it fights `foe`: its quote against that very character, else its general one; a quote
// already said ("once") is passed over. Null when there is none.
const BattleQuote* pickQuote(const Database& db, const QuoteState& state, const Unit& speaker, const Unit& foe) {
  if (speaker.character < 0 || state.said.empty()) return nullptr;
  const BattleQuote* general = nullptr;
  for (const BattleQuote& bq : db.characters[(size_t)speaker.character].battleQuotes) {
    if (bq.once != QuoteOnce::Always && state.said[(size_t)bq.flat]) continue;
    if (bq.against >= 0) {
      if (bq.against == foe.character) return &bq;
    } else if (!general) {
      general = &bq;
    }
  }
  return general;
}

// One Dialogue entry per line. A line is spoken by the owner unless it names a character; a character that is not on
// the map (never placed) is shown by name, with its portrait (the loader put the portrait id on the line).
// `flag` marks the entries as a battle or a defeat quote (kDialogueQuote / kDialogueDefeat) so the player can tell them from other lines.
void sayLines(const Battle& b, int owner, const std::vector<QuoteLine>& lines, int flag, std::vector<Event>& events) {
  for (const QuoteLine& line : lines) {
    int speaker = -1;
    if (!line.speaker.empty()) speaker = line.speaker == b.units[(size_t)owner].storyId ? owner : b.unitByStoryId(line.speaker);
    Event e;
    e.type = EventType::Dialogue;
    e.a = speaker;
    e.b = kQuoteTextBase + line.text;
    e.c = speaker < 0 && line.speakerName >= 0 ? kQuoteTextBase + line.speakerName : -1;
    e.d = line.portrait >= 0 ? kQuoteTextBase + line.portrait : -1;
    e.flags = flag;
    events.push_back(e);
  }
}

}  // namespace

void Battle::fightQuotes(int attacker, int defender, std::vector<Event>& events) {
  if (quotes.said.empty()) return;
  // The attacked unit speaks first, then the attacker (docs/EVENTS.md). Both are picked before either is said, so what
  // one says never changes what the other may.
  const int order[2] = {defender, attacker};
  const BattleQuote* picked[2];
  for (int i = 0; i < 2; ++i) picked[i] = pickQuote(*db, quotes, units[(size_t)order[i]], units[(size_t)order[1 - i]]);
  bool spoke = false;
  for (int i = 0; i < 2; ++i) {
    if (!picked[i] || (spoke && db->quotesOnePerFight)) continue;  // "quotesPerFight": "one": the second speaker keeps its quote for another fight
    if (picked[i]->once != QuoteOnce::Always) quotes.said[(size_t)picked[i]->flat] = 1;
    sayLines(*this, order[i], picked[i]->lines, kDialogueQuote, events);
    spoke = true;
  }
}

void Battle::defeatQuote(int unit, std::vector<Event>& events) {
  const Unit& u = units[(size_t)unit];
  if (u.character < 0) return;
  const CharacterDef& ch = db->characters[(size_t)u.character];
  // "retreats" is for a unit of ours in a game whose campaign brings fallen units back; every other defeat "falls".
  const bool retreat = db->campaign && db->campaign->fallenReturn && !sides[(size_t)u.side].ai;
  sayLines(*this, unit, retreat ? ch.retreatsLines : ch.fallsLines, kDialogueDefeat, events);
}

}  // namespace ote
