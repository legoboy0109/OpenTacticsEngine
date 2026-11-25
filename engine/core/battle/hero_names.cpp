// Names the player typed for campaign characters (docs/EVENTS.md "Naming a character"). The campaign hands them in when a chapter's
// battle is set up; the battle only renames units with those story ids and answers "{name:<id>}" for the player's lines.
#include "core/battle/battle.h"

namespace ote {

void Battle::setHeroNames(std::shared_ptr<const HeroNames> names) {
  heroNames = names;
  if (!heroNames) return;
  for (Unit& u : units)
    if (const std::string* typed = typedName(u.storyId)) u.name = *typed;
}

const std::string* Battle::typedName(const std::string& id) const {
  if (!heroNames || id.empty()) return nullptr;
  for (const auto& n : *heroNames)
    if (n.first == id) return &n.second;
  return nullptr;
}

std::string Battle::heroName(const std::string& id) const {
  if (const std::string* typed = typedName(id)) return *typed;
  const int u = unitByStoryId(id);
  if (u >= 0) return units[(size_t)u].name;
  if (db) {
    const int ch = db->findCharacter(id);
    if (ch >= 0) return db->characters[(size_t)ch].name;
  }
  return id;
}

}  // namespace ote
