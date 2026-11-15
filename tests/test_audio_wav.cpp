// WAV decoder: formats, limits, and damaged files (docs/SOUND.md "Files and formats").
#include <cstring>

#include "audio/decode.h"
#include "audio/synth.h"
#include "audio/wav.h"
#include "audio_fixture.h"
#include "test.h"

using namespace ote;
using namespace audio_test;

namespace {

std::vector<uint8_t> wavOf(int rate, int channels, int frames) {
  std::shared_ptr<PcmBuffer> b = makeBuffer(rate, channels, frames, [](int i, int c) { return (i * 7 + c * 1000) % 20000 - 10000; });
  return encodeWav16(b->samples.data(), (size_t)frames, channels, rate);
}

bool decodes(const std::vector<uint8_t>& f, PcmBuffer& out, std::string& err) {
  return decodeSound(f.data(), f.size(), out, err);
}

}  // namespace

TEST(audio_wav_16bit_mono_and_stereo) {
  for (int ch = 1; ch <= 2; ++ch) {
    std::vector<uint8_t> f = wavOf(22050, ch, 300);
    PcmBuffer b;
    std::string err;
    CHECK(decodes(f, b, err));
    CHECK_EQ(b.rate, 22050);
    CHECK_EQ(b.channels, ch);
    CHECK_EQ(b.frames(), (int64_t)300);
    CHECK_EQ(b.samples[5 * ch + (ch - 1)], (int16_t)((5 * 7 + (ch - 1) * 1000) % 20000 - 10000));
  }
}

TEST(audio_wav_8bit_is_centred_and_scaled) {
  const uint8_t px[4] = {0, 128, 255, 64};
  std::vector<uint8_t> f = encodeWav8(px, 4, 1, 11025);
  PcmBuffer b;
  std::string err;
  CHECK(decodes(f, b, err));
  CHECK_EQ(b.samples[0], (int16_t)-32768);
  CHECK_EQ(b.samples[1], (int16_t)0);
  CHECK_EQ(b.samples[2], (int16_t)32512);
  CHECK_EQ(b.samples[3], (int16_t)-16384);
}

TEST(audio_wav_rate_range) {
  PcmBuffer b;
  std::string err;
  CHECK(decodes(wavOf(8000, 1, 10), b, err));
  CHECK(decodes(wavOf(48000, 1, 10), b, err));
  CHECK(!decodes(wavOf(7999, 1, 10), b, err));
  CHECK(err.find("sample rate") != std::string::npos);
  CHECK(!decodes(wavOf(48001, 1, 10), b, err));
  CHECK(!decodes(wavOf(96000, 1, 10), b, err));
}

TEST(audio_wav_refuses_unsupported_formats) {
  PcmBuffer b;
  std::string err;
  std::vector<uint8_t> f = wavOf(22050, 1, 10);
  std::vector<uint8_t> g = f;
  g[20] = 3;  // IEEE float
  CHECK(!decodes(g, b, err));
  CHECK(err.find("not supported") != std::string::npos);
  g = f;
  g[22] = 6;  // 6 channels
  CHECK(!decodes(g, b, err));
  CHECK(err.find("channels") != std::string::npos);
  g = f;
  g[34] = 24;  // 24-bit
  CHECK(!decodes(g, b, err));
  CHECK(err.find("24-bit") != std::string::npos);
  const char* text = "this is not a sound at all, just some words";
  CHECK(!decodeSound((const uint8_t*)text, strlen(text), b, err));
  CHECK(err.find("unknown sound format") != std::string::npos);
  CHECK(!decodeSound(nullptr, 0, b, err));
}

TEST(audio_wav_effect_limits) {
  PcmBuffer b;
  std::string err;
  // Exactly 10 s at the top rate and stereo is allowed: 1.92 MB decoded.
  CHECK(decodes(wavOf(48000, 2, 480000), b, err));
  CHECK(!decodes(wavOf(48000, 2, 480001), b, err));
  CHECK(err.find("longer than 10 seconds") != std::string::npos);
  CHECK(b.samples.empty());
  CHECK(!decodes(wavOf(8000, 1, 80001), b, err));
  // As music the length does not matter.
  FileBytes big(new std::vector<uint8_t>(wavOf(22050, 1, 22050 * 30)));
  std::unique_ptr<MusicSource> m = openMusic(big, err);
  CHECK(m != nullptr);
  CHECK_EQ(m->totalFrames(), (int64_t)22050 * 30);
}

TEST(audio_wav_truncated_files_never_overread) {
  // Every possible cut of a good file: either a clear error or the frames that are really there.
  std::vector<uint8_t> f = wavOf(22050, 2, 50);
  for (size_t cut = 0; cut <= f.size(); ++cut) {
    std::vector<uint8_t> t(f.begin(), f.begin() + cut);
    PcmBuffer b;
    std::string err;
    bool ok = decodeSound(t.data(), t.size(), b, err);
    if (ok) {
      CHECK(cut >= 45);
      CHECK(b.frames() >= 1 && b.frames() <= 50);
      CHECK_EQ(b.frames(), (int64_t)((cut - 44) / 4));
    } else {
      CHECK(!err.empty());
    }
  }
}

TEST(audio_wav_damaged_headers) {
  std::vector<uint8_t> f = wavOf(22050, 1, 100);
  PcmBuffer b;
  std::string err;
  // A data chunk that claims more than the file holds keeps what is there.
  std::vector<uint8_t> g = f;
  g[40] = g[41] = g[42] = g[43] = 0xFF;
  CHECK(decodes(g, b, err));
  CHECK_EQ(b.frames(), (int64_t)100);
  // A data size of 0 (a recorder that never came back to fix it) is "no sound data".
  g = f;
  g[40] = g[41] = g[42] = g[43] = 0;
  CHECK(!decodes(g, b, err));
  CHECK(err.find("no sound data") != std::string::npos);
  // fmt chunk too short, and missing chunks.
  g = f;
  g[16] = 8;
  CHECK(!decodes(g, b, err));
  CHECK(err.find("fmt") != std::string::npos);
  g = f;
  memcpy(&g[12], "junk", 4);
  CHECK(!decodes(g, b, err));
  CHECK(err.find("no fmt chunk") != std::string::npos);
  g = f;
  memcpy(&g[36], "junk", 4);
  CHECK(!decodes(g, b, err));
  CHECK(err.find("no data chunk") != std::string::npos);
  // A chunk length near 4 GB must not wrap the position.
  g = f;
  g[16] = g[17] = g[18] = g[19] = 0xFF;
  CHECK(!decodes(g, b, err));
}

TEST(audio_wav_garbage_never_crashes) {
  // Valid-looking prefix, random body; and random bytes everywhere in a good file. Only the absence of
  // a crash or an over-read (also run by hand under -fsanitize=address,undefined) and a message matter.
  unsigned seed = 12345;
  auto next = [&seed]() { seed = seed * 1664525u + 1013904223u; return (seed >> 16) & 0xFF; };
  std::vector<uint8_t> good = wavOf(22050, 2, 40);
  for (int round = 0; round < 400; ++round) {
    std::vector<uint8_t> f = good;
    int hits = 1 + round % 6;
    for (int h = 0; h < hits; ++h) f[next() * 7 % f.size()] = (uint8_t)next();
    PcmBuffer b;
    std::string err;
    bool ok = decodeSound(f.data(), f.size(), b, err);
    CHECK(ok || !err.empty());
    if (ok) CHECK(b.frames() > 0 && b.frames() <= 40);
  }
  std::vector<uint8_t> junk(300);
  for (auto& c : junk) c = (uint8_t)next();
  memcpy(junk.data(), "RIFF", 4);
  memcpy(junk.data() + 8, "WAVE", 4);
  PcmBuffer b;
  std::string err;
  CHECK(!decodeSound(junk.data(), junk.size(), b, err));
}

TEST(audio_wav_stream_reads_and_seeks) {
  FileBytes f(new std::vector<uint8_t>(wavOf(22050, 1, 1000)));
  std::string err;
  std::unique_ptr<MusicSource> m = openMusic(f, err);
  CHECK(m != nullptr);
  int16_t buf[400];
  int total = 0, n;
  while ((n = m->read(buf, 400)) > 0) total += n;
  CHECK_EQ(total, 1000);
  CHECK(m->seek(990));
  CHECK_EQ(m->read(buf, 400), 10);
  CHECK(!m->seek(1001));
  CHECK(m->seek(0));
  CHECK_EQ(m->read(buf, 5), 5);
  CHECK_EQ(buf[1], (int16_t)(7 % 20000 - 10000));
}
