// setPhaseMusic (docs/SOUND.md "Music by situation"): the story changes which music a side's phase plays for the rest of the
// battle. Presentation only: the rules never read it. The change is kept in StoryState::phaseMusic (saved with the battle) and
// reaches the player as a MusicChanged entry with the kMusicPhase flag.
#include "core/battle/battle.h"

namespace ote {

void Battle::storyPhaseMusic(const Command& c, std::vector<Event>& events) {
  if (c.side < 0 || c.side >= (int)sides.size()) return;
  const std::string& id = sides[(size_t)c.side].id;
  auto& list = storyState.phaseMusic;
  for (size_t i = 0; i < list.size(); ++i)
    if (list[i].first == id) {
      list.erase(list.begin() + (long)i);
      break;
    }
  if (c.text >= 0) list.push_back(std::make_pair(id, storyText(c.text)));
  Event e;
  e.type = EventType::MusicChanged;
  e.a = c.text;
  e.b = -1;
  e.c = c.side;
  e.flags = kMusicPhase;
  events.push_back(e);
}

}  // namespace ote
