// The camp's Jobs screen (docs/JOBS.md "Player UI"): opened from the "Jobs" row of the camp. A scene that hosts the JobsScreen over the story background;
// every change is written to the Continue save at once, and leaving goes back to the camp (campaignSceneDone saves and runs the flow again).
#pragma once
#include <memory>

#include "jobs/jobs_screen.h"
#include "scene.h"

namespace ote {

class JobsScene : public Scene {
 public:
  explicit JobsScene(SceneHost& host);
  void onInput(const InputEvent& e) override;
  void draw(Renderer& r) override;
  SceneAudio audioContext() const override { SceneAudio a; a.kind = SceneAudio::Scene; return a; }

 private:
  void saveNow();
  SceneHost& host_;
  Color bg_;
  std::unique_ptr<JobsScreen> screen_;
  bool left_ = false;
};

}  // namespace ote
