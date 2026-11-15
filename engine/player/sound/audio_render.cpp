#include "sound/audio_render.h"

#include <cmath>
#include <cstdio>

#include "audio/synth.h"
#include "core/base/log.h"
#include "sound/audio_selftest.h"
#include "sound/audio_service.h"

namespace ote {
namespace {

// A device that keeps what it is given and "plays" it at real speed on a clock the render loop drives,
// so the service's keep-80-ms-queued logic runs exactly as with a real device.
class RecordingAudioOut : public AudioOut {
 public:
  explicit RecordingAudioOut(int rate) : rate_(rate) {}
  bool available() const override { return true; }
  int rate() const override { return rate_; }
  int channels() const override { return 2; }
  int queuedFrames() const override { return (int)(pushed_ - played_); }
  void push(const int16_t* samples, int frames) override {
    data.insert(data.end(), samples, samples + (size_t)frames * 2);
    pushed_ += frames;
  }
  const char* name() const override { return "recording"; }
  void play(double seconds) {
    owed_ += seconds * rate_;
    int64_t whole = (int64_t)owed_;
    owed_ -= (double)whole;
    played_ += whole;
    if (played_ > pushed_) played_ = pushed_;
  }
  std::vector<int16_t> data;

 private:
  int rate_;
  int64_t pushed_ = 0, played_ = 0;
  double owed_ = 0;
};

}  // namespace

int runAudioRender(const Args& args) {
  AudioService audio;
  audio.setMode(AudioService::Mode::Live);
  RecordingAudioOut* rec = new RecordingAudioOut(AudioService::kRate);
  audio.attachDevice(std::unique_ptr<AudioOut>(rec));
  std::string err;
  if (!audio.openLog(args.audioLog, err)) {
    fprintf(stderr, "error: %s\n", err.c_str());
    return 1;
  }
  audio.setVolumes(args.musicVolume >= 0 ? args.musicVolume : 7, args.soundVolume >= 0 ? args.soundVolume : 7);

  AudioSelfTest test(audio, args.audioFile);
  const double dt = args.fixedDt > 0 ? args.fixedDt : 1.0 / 60.0;
  const int64_t want = (int64_t)std::llround(args.seconds * AudioService::kRate);
  while ((int64_t)rec->data.size() / 2 < want + AudioService::kRate / 5) {
    test.update();
    audio.update(dt);
    rec->play(dt);
  }
  rec->data.resize((size_t)want * 2);

  std::vector<uint8_t> wav = encodeWav16(rec->data.data(), (size_t)want, 2, AudioService::kRate);
  FILE* f = fopen(args.audioRender.c_str(), "wb");
  if (!f || fwrite(wav.data(), 1, wav.size(), f) != wav.size()) {
    fprintf(stderr, "error: cannot write %s\n", args.audioRender.c_str());
    if (f) fclose(f);
    return 1;
  }
  fclose(f);
  printf("wrote %s: %lld frames, %d Hz stereo\n", args.audioRender.c_str(), (long long)want, AudioService::kRate);
  return 0;
}

}  // namespace ote
