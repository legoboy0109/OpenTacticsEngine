// The audio device that plays nothing (platform.h "Audio output").
#include "platform/platform.h"

namespace ote {
namespace {

class NullAudioOut : public AudioOut {
 public:
  explicit NullAudioOut(int rate) : rate_(rate) {}
  bool available() const override { return false; }
  int rate() const override { return rate_; }
  int channels() const override { return 2; }
  int queuedFrames() const override { return 0; }
  void push(const int16_t*, int) override {}
  const char* name() const override { return "none"; }

 private:
  int rate_;
};

}  // namespace

std::unique_ptr<AudioOut> createNullAudioOut(int rate) { return std::unique_ptr<AudioOut>(new NullAudioOut(rate)); }

}  // namespace ote
