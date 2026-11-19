// SDL2 audio output: SDL_QueueAudio (push mode, no callback), so SDL's audio thread plays the queue
// and never calls into our code. Mirrors engine/platform/sdl3/sdl_audio.cpp.
#include <SDL.h>

#include "core/base/log.h"
#include "platform/platform.h"

namespace ote {
namespace {

class SdlAudioOut : public AudioOut {
 public:
  SdlAudioOut(SDL_AudioDeviceID dev, int rate) : dev_(dev), rate_(rate) {}
  ~SdlAudioOut() override {
    SDL_CloseAudioDevice(dev_);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
  }
  bool available() const override { return true; }
  int rate() const override { return rate_; }
  int channels() const override { return 2; }
  int queuedFrames() const override { return (int)(SDL_GetQueuedAudioSize(dev_) / (2 * sizeof(int16_t))); }
  void push(const int16_t* samples, int frames) override {
    if (SDL_QueueAudio(dev_, samples, (Uint32)(frames * 2 * (int)sizeof(int16_t))) != 0)
      OTE_WARN("audio: cannot queue sound: %s", SDL_GetError());
  }
  const char* name() const override { return "SDL2 audio"; }

 private:
  SDL_AudioDeviceID dev_;
  int rate_;
};

}  // namespace

std::unique_ptr<AudioOut> openAudioOut(int rate, std::string& why) {
  if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
    why = std::string("SDL audio is unavailable: ") + SDL_GetError();
    return nullptr;
  }
#ifdef OTE_UWP
  // SDL 2.32's WinRT audio backend crashes (ReleaseWasapiDevice, SDL_wasapi.c) when it is asked to open a device on a
  // machine with no sound endpoint at all (seen in a VM without audio hardware). A console or PC always lists one.
  if (SDL_GetNumAudioDevices(0) <= 0) {
    why = "no sound device: the system lists no audio output";
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    return nullptr;
  }
#endif
  SDL_AudioSpec want;
  SDL_zero(want);
  want.freq = rate;
  want.format = AUDIO_S16SYS;
  want.channels = 2;
  want.samples = 1024;      // about 23 ms: smaller than the ~80 ms the player keeps queued
  want.callback = nullptr;  // queue mode
  // allowed_changes = 0: SDL converts to the hardware's own format, we always push what we asked for.
  SDL_AudioDeviceID dev = SDL_OpenAudioDevice(nullptr, 0, &want, nullptr, 0);
  if (dev == 0) {
    why = std::string("no sound device: ") + SDL_GetError();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    return nullptr;
  }
  SDL_PauseAudioDevice(dev, 0);  // devices start paused
  return std::unique_ptr<AudioOut>(new SdlAudioOut(dev, rate));
}

bool audioWaitsForUserInput() { return false; }

}  // namespace ote
