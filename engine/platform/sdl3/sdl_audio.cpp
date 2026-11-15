// SDL3 audio output: an SDL_AudioStream in push mode. We only call SDL_PutAudioStreamData from the
// main loop; SDL's own audio thread pulls from the stream, and no callback ever enters our code.
#include <SDL3/SDL.h>

#include "core/base/log.h"
#include "platform/platform.h"

namespace ote {
namespace {

class SdlAudioOut : public AudioOut {
 public:
  SdlAudioOut(SDL_AudioStream* stream, int rate) : stream_(stream), rate_(rate) {}
  ~SdlAudioOut() override {
    SDL_DestroyAudioStream(stream_);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
  }
  bool available() const override { return true; }
  int rate() const override { return rate_; }
  int channels() const override { return 2; }
  int queuedFrames() const override {
    int bytes = SDL_GetAudioStreamQueued(stream_);
    return bytes < 0 ? 0 : bytes / (int)(2 * sizeof(int16_t));
  }
  void push(const int16_t* samples, int frames) override {
    if (!SDL_PutAudioStreamData(stream_, samples, frames * 2 * (int)sizeof(int16_t)))
      OTE_WARN("audio: cannot queue sound: %s", SDL_GetError());
  }
  const char* name() const override { return "SDL3 audio"; }

 private:
  SDL_AudioStream* stream_;
  int rate_;
};

}  // namespace

std::unique_ptr<AudioOut> openAudioOut(int rate, std::string& why) {
  if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
    why = std::string("SDL audio is unavailable: ") + SDL_GetError();
    return nullptr;
  }
  // A small device buffer (about 23 ms at 44.1 kHz): the player keeps ~80 ms queued, and a device
  // buffer bigger than that would be pulled half empty and crackle.
  SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, "1024");
  SDL_AudioSpec spec;
  SDL_zero(spec);
  spec.format = SDL_AUDIO_S16;
  spec.channels = 2;
  spec.freq = rate;
  SDL_AudioStream* stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
  if (!stream) {
    why = std::string("no sound device: ") + SDL_GetError();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    return nullptr;
  }
  SDL_ResumeAudioStreamDevice(stream);  // streams start paused
  return std::unique_ptr<AudioOut>(new SdlAudioOut(stream, rate));
}

bool audioWaitsForUserInput() {
#ifdef __EMSCRIPTEN__
  return true;  // browsers start audio only after a key press or tap
#else
  return false;
#endif
}

}  // namespace ote
