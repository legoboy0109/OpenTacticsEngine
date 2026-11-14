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

std::string chapterHeading(int index) { return "Chapter " + std::to_string(index + 1); }

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
  if (info.campaign && !info.inBattle) return phaseWords(info.phase);
  return "Turn " + std::to_string(info.round);
}

}  // namespace ote
