// The player's audio service: modes, the device pump, the audio log, the volume options.
#include <cstdio>

#include "args.h"
#include "audio_fixture.h"
#include "options/option_defs.h"
#include "sound/audio_service.h"
#include "test.h"

using namespace ote;
using namespace audio_test;

namespace {

// A device whose queue the test controls.
class FakeOut : public AudioOut {
 public:
  bool ready = true;
  int queued = 0, pushedFrames = 0, pushes = 0;
  std::vector<int16_t> data;
  bool available() const override { return ready; }
  int rate() const override { return AudioService::kRate; }
  int channels() const override { return 2; }
  int queuedFrames() const override { return queued; }
  void push(const int16_t* s, int frames) override {
    pushedFrames += frames;
    ++pushes;
    data.insert(data.end(), s, s + (size_t)frames * 2);
  }
  const char* name() const override { return "fake"; }
};

std::string writeTemp(const char* name, const std::vector<uint8_t>& bytes) {
  std::string path = ote_test::tempDir() + "/" + name;
  FILE* f = fopen(path.c_str(), "wb");
  if (f) {
    fwrite(bytes.data(), 1, bytes.size(), f);
    fclose(f);
  }
  return path;
}

std::string tone22k() {
  std::shared_ptr<PcmBuffer> t = makeTone(440, 0.5, 22050, 1, 0.5);
  return writeTemp("audio_service_tone.wav", encodeWav16(t->samples.data(), (size_t)t->frames(), 1, 22050));
}

}  // namespace

TEST(audio_service_pump_keeps_about_80ms_queued) {
  AudioService a;
  FakeOut* dev = new FakeOut();
  a.attachDevice(std::unique_ptr<AudioOut>(dev));
  CHECK(a.mode() == AudioService::Mode::Live);
  a.update(1.0 / 60);
  CHECK_EQ(dev->pushedFrames, 44100 * 80 / 1000);  // an empty queue is filled to 80 ms
  int before = dev->pushedFrames;
  dev->queued = 44100 * 80 / 1000;
  a.update(1.0 / 60);
  CHECK_EQ(dev->pushedFrames, before);  // already full: nothing pushed
  dev->queued = 44100 * 60 / 1000;      // 20 ms were played
  a.update(1.0 / 60);
  CHECK_EQ(dev->pushedFrames - before, 44100 * 20 / 1000);
  // After a long frame the queue ran dry; it is refilled to 80 ms, never more than 250 ms in one go.
  before = dev->pushedFrames;
  dev->queued = 0;
  a.update(0.1);
  CHECK_EQ(dev->pushedFrames - before, 44100 * 80 / 1000);
  CHECK(dev->pushedFrames - before <= 44100 * 250 / 1000);
}

TEST(audio_service_plays_sounds_and_music_through_the_device) {
  AudioService a;
  FakeOut* dev = new FakeOut();
  a.attachDevice(std::unique_ptr<AudioOut>(dev));
  a.setVolumes(10, 10);
  std::string err;
  std::string wav = tone22k();
  SoundId s = a.loadSound(wav, err);
  CHECK(s > 0);
  CHECK_EQ(a.loadSound(wav, err), s);  // cached by path
  a.playSound(s, 1.0f);
  a.update(1.0 / 60);
  CHECK(rms(dev->data, 0, (int)dev->data.size() / 2) > 1000);
  // Music streams from a file too.
  FakeOut* dev2 = new FakeOut();
  AudioService b;
  b.attachDevice(std::unique_ptr<AudioOut>(dev2));
  b.setVolumes(10, 10);
  MusicParams p;
  CHECK(b.playMusicFile(wav, p, err));
  CHECK(b.musicPlaying());
  b.update(1.0 / 60);
  CHECK(rms(dev2->data, 0, (int)dev2->data.size() / 2) > 1000);
  b.stopMusic(0);
  CHECK(!b.musicPlaying());
}

TEST(audio_service_null_device_and_off_mode_are_safe_no_ops) {
  AudioService a;  // starts Off with the null device
  a.log().keepLines(true);
  a.setVolumes(7, 7);
  std::string err;
  SoundId s = a.loadSound("does/not/exist.wav", err);  // Off reads nothing
  CHECK(s > 0);
  a.playSound(s);
  a.playSound(0);
  a.playSound(999);
  MusicParams p;
  CHECK(a.playMusicFile("does/not/exist.ogg", p, err));
  a.stopMusic(0.5f);
  a.update(1.0);
  CHECK(!a.musicPlaying());
  CHECK_EQ(a.mixer().activeSounds(), 0);
  // Still logged.
  CHECK_EQ(a.log().lines().size(), (size_t)4);
  CHECK(a.log().lines()[1] == std::string("t=0.000 sound does/not/exist.wav vol=100 level=7"));
  // A device that cannot be opened leaves the same state, with a warning from the caller only.
  AudioService b;
  b.setMode(AudioService::Mode::Live);
  b.deviceFailed();
  CHECK(b.mode() == AudioService::Mode::Off);
  AudioService c;
  c.setMode(AudioService::Mode::Live);
  FakeOut* dead = new FakeOut();
  dead->ready = false;
  c.attachDevice(std::unique_ptr<AudioOut>(dead));  // a "device" that says it is unavailable
  CHECK(c.mode() == AudioService::Mode::Off);
  c.update(0.5);
  CHECK_EQ(dead->pushes, 0);
}

TEST(audio_service_silent_mode_checks_files_but_plays_nothing) {
  AudioService a;
  a.setMode(AudioService::Mode::Silent);
  std::string err;
  CHECK(a.loadSound(tone22k(), err) > 0);
  CHECK_EQ(a.loadSound(writeTemp("audio_service_bad.wav", std::vector<uint8_t>(100, 7)), err), 0);
  CHECK(err.find("unknown sound format") != std::string::npos);
  CHECK(a.loadSound("does/not/exist.wav", err) == 0);
  CHECK(err.find("cannot read") != std::string::npos);
  MusicParams p;
  CHECK(!a.playMusicFile("does/not/exist.ogg", p, err));
  CHECK(a.playMusicFile(tone22k(), p, err));
  a.update(1.0);
  CHECK(!a.musicPlaying());  // nothing is mixed
}

TEST(audio_service_log_lines) {
  AudioService a;
  a.log().keepLines(true);
  a.setMode(AudioService::Mode::Silent);
  a.setVolumes(7, 7);
  a.update(0.25);
  SoundId s = a.addSound("thud", makeTone(100, 0.1, 22050));
  a.playSound(s, 0.5f);
  MusicParams p;
  p.fadeSeconds = 0.5f;
  p.loop = false;
  a.playMusic("theme", makeBufferSource(makeTone(100, 0.1, 22050)), p);
  a.update(0.25);
  a.stopMusic(1.0f);
  a.playTick(true);
  a.setVolumes(3, 7);
  const std::vector<std::string>& l = a.log().lines();
  CHECK_EQ(l.size(), (size_t)6);
  CHECK(l[0] == std::string("t=0.000 volumes music=7 sound=7"));
  CHECK(l[1] == std::string("t=0.250 sound thud vol=50 level=7"));
  CHECK(l[2] == std::string("t=0.250 music play theme fade=0.5 loop=0 level=7"));
  CHECK(l[3] == std::string("t=0.500 music stop fade=1"));
  CHECK(l[4] == std::string("t=0.500 sound tick vol=100 bus=music level=7"));
  CHECK(l[5] == std::string("t=0.500 volumes music=3 sound=7"));
}

TEST(audio_service_music_level_zero_opens_nothing_until_raised) {
  AudioService a;
  FakeOut* dev = new FakeOut();
  a.attachDevice(std::unique_ptr<AudioOut>(dev));
  a.setVolumes(0, 10);
  std::string err;
  MusicParams p;
  // The file does not even have to exist at level 0: nothing is read.
  CHECK(a.playMusicFile(tone22k(), p, err));
  CHECK(!a.musicPlaying());
  a.setVolumes(5, 10);  // raised: the asked-for music starts (fading in)
  CHECK(a.musicPlaying());
  a.stopMusic(0);
  a.setVolumes(0, 10);
  a.setVolumes(5, 10);  // stopped on purpose: it must not come back
  CHECK(!a.musicPlaying());
}

TEST(audio_service_volume_options_layer_args_game_global_default) {
  LookLayer none, args, perGame, global;
  EffectiveLook e = resolveLook("assets", none, none, none, GameLook());
  CHECK_EQ(e.musicVolume, 7);
  CHECK_EQ(e.soundVolume, 7);
  global.musicVolume = 3;
  global.soundVolume = 0;
  e = resolveLook("assets", none, none, global, GameLook());
  CHECK_EQ(e.musicVolume, 3);
  CHECK_EQ(e.soundVolume, 0);
  perGame.musicVolume = 9;
  e = resolveLook("assets", none, perGame, global, GameLook());
  CHECK_EQ(e.musicVolume, 9);
  CHECK_EQ(e.soundVolume, 0);
  args.musicVolume = 1;
  args.soundVolume = 10;
  e = resolveLook("assets", args, perGame, global, GameLook());
  CHECK_EQ(e.musicVolume, 1);
  CHECK_EQ(e.soundVolume, 10);
  global.musicVolume = 99;  // a hand-edited file: clamped
  e = resolveLook("assets", none, none, global, GameLook());
  CHECK_EQ(e.musicVolume, 10);
}

TEST(audio_service_volume_options_are_in_the_table) {
  int music = -1, sound = -1;
  for (size_t i = 0; i < optionDefs().size(); ++i) {
    std::string k = optionDefs()[i].key;
    if (k == "musicVolume") music = (int)i;
    if (k == "soundVolume") sound = (int)i;
  }
  CHECK(music >= 0 && sound >= 0);
  OptionContext ctx;
  std::vector<OptionChoice> c = optionDefs()[music].choices(ctx);
  CHECK_EQ(c.size(), (size_t)12);  // unset, Off, 1..10
  CHECK(c[0].value.isNull());
  CHECK(c[0].label == std::string("Default (7)"));
  CHECK(c[1].label == std::string("Off"));
  CHECK_EQ(c[11].value.asInt(), 10);
  Settings s;
  s.setGlobal("musicVolume", Json::makeNumber(4));
  CHECK_EQ(globalLayer(s).musicVolume, 4);
  CHECK_EQ(globalLayer(s).soundVolume, -1);
}

TEST(audio_service_command_line) {
  auto parse = [](std::vector<const char*> v) {
    v.insert(v.begin(), "ote-player");
    return parseArgs((int)v.size(), const_cast<char**>(v.data()));
  };
  Args a = parse({"--music-volume", "3", "--sound-volume", "10", "--no-audio", "--audio-log", "x.log"});
  CHECK(a.error.empty());
  CHECK_EQ(a.musicVolume, 3);
  CHECK_EQ(a.soundVolume, 10);
  CHECK(a.noAudio);
  CHECK(a.audioLog == std::string("x.log"));
  CHECK(!parse({"--music-volume", "11"}).error.empty());
  CHECK(!parse({"--sound-volume", "-1"}).error.empty());
  CHECK(!parse({"--music-volume", "loud"}).error.empty());
  CHECK(!parse({"--music-volume"}).error.empty());
  CHECK(!parse({"--seconds", "3"}).error.empty());
  CHECK(!parse({"--audio-test", "--audio-render", "x.wav"}).error.empty());
  CHECK(!parse({"--audio-file", "x.ogg"}).error.empty());
  a = parse({"--audio-render", "x.wav", "--seconds", "2.5", "--audio-file", "m.ogg"});
  CHECK(a.error.empty());
  CHECK(a.seconds == 2.5);
  CHECK(!parse({"--audio-render", "x.wav", "--seconds", "0"}).error.empty());
}
