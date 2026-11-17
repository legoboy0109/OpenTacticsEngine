#include "campaign/campaign_logic.h"

namespace ote {

Next nextForPhase(Phase phase, bool battleInHand) {
  switch (phase) {
    case Phase::Before: return Next::BeforeScene;
    case Phase::Battle: return battleInHand ? Next::ResumeBattle : Next::StartBattle;
    case Phase::After: return Next::AfterScene;
    case Phase::Finished: return Next::EndFinished;
    case Phase::Over: return Next::EndOver;
  }
  return Next::None;
}

Next nextAfterBattle(ChapterResult result) {
  switch (result) {
    case ChapterResult::Won: return Next::AfterScene;
    case ChapterResult::Retry: return Next::RetryChoice;
    case ChapterResult::Ended: return Next::EndOver;
    default: return Next::None;
  }
}

bool continueIsWritten(bool savingOn, bool campaignGame, bool campaignBattle) {
  return savingOn && (!campaignGame || campaignBattle);
}

std::string chapterHeading(int index, const std::string& party) {
  return "Chapter " + std::to_string(index + 1) + (party.empty() ? "" : " - " + party);
}

std::string phaseWords(const std::string& phase) {
  if (phase == "before") return "Chapter begins";
  if (phase == "battle") return "Ready to fight";
  if (phase == "after") return "Battle won";
  if (phase == "finished") return "Story complete";
  if (phase == "over") return "Story ended";
  return "";
}

std::string saveTitle(const SaveInfo& info) {
  if (info.campaign && !info.chapterName.empty()) return info.chapterName;
  return info.mapName.empty() ? info.mapId : info.mapName;
}

std::string saveDetail(const SaveInfo& info) {
  std::string d = info.campaign && !info.inBattle ? phaseWords(info.phase) : "Turn " + std::to_string(info.round);
  if (info.campaign && !info.difficultyName.empty()) d += " - " + info.difficultyName;  // only a game with difficulties has one
  return d;
}

}  // namespace ote
