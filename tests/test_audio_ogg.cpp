// Ogg Vorbis decoding through stb_vorbis: the two committed fixtures
// (tests/fixtures/audio/README.md), streaming, and damaged files.
#include <cstdio>
#include <cstring>

#include "audio/decode.h"
#include "audio/vorbis.h"
#include "audio_fixture.h"
#include "test.h"

using namespace ote;
using namespace audio_test;

namespace {

FileBytes loadFixture(const char* name) {
  std::string path = ote_test::sourceDir() + "/tests/fixtures/audio/" + name;
  std::vector<uint8_t>* bytes = new std::vector<uint8_t>();
  FILE* f = fopen(path.c_str(), "rb");
  if (f) {
    uint8_t buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) bytes->insert(bytes->end(), buf, buf + n);
    fclose(f);
  }
  return FileBytes(bytes);
}

// Frequency of a tone from its upward zero crossings (frames between the first and last).
double pitchOf(const std::vector<int16_t>& interleaved, int channels, int channel, int rate) {
  int first = -1, last = -1, n = 0;
  int frames = (int)interleaved.size() / channels;
  for (int i = 1; i < frames; ++i) {
    if (interleaved[(size_t)(i - 1) * channels + channel] < 0 && interleaved[(size_t)i * channels + channel] >= 0) {
      if (first < 0) first = i;
      last = i;
      ++n;
    }
  }
  return n < 2 ? 0 : (double)(n - 1) * rate / (last - first);
}

}  // namespace

TEST(audio_ogg_fixtures_decode_whole) {
  struct Case { const char* file; int rate, channels, frames; double left, right; } cases[] = {
      {"tone_mono_22k.ogg", 22050, 1, 11025, 440, 440},
      {"tone_stereo_44k.ogg", 44100, 2, 22050, 440, 660},
  };
  for (const Case& c : cases) {
    FileBytes f = loadFixture(c.file);
    CHECK(!f->empty());
    CHECK(f->size() < 20000);
    PcmBuffer b;
    std::string err;
    CHECK(decodeSound(f->data(), f->size(), b, err));
    CHECK_EQ(b.rate, c.rate);
    CHECK_EQ(b.channels, c.channels);
    // The encoder pads a little at the ends: within one Vorbis block of the source length.
    CHECK(std::abs((int)b.frames() - c.frames) <= 2048);
    double pl = pitchOf(b.samples, c.channels, 0, c.rate);
    CHECK(std::fabs(pl - c.left) < 8);
    if (c.channels == 2) CHECK(std::fabs(pitchOf(b.samples, 2, 1, c.rate) - c.right) < 12);
    double peak = 0;
    for (int16_t s : b.samples) peak = std::fmax(peak, std::fabs((double)s));
    CHECK(peak > 9000 && peak < 16000);  // 12000 in, lossy coding
  }
}

TEST(audio_ogg_streams_the_same_samples) {
  FileBytes f = loadFixture("tone_stereo_44k.ogg");
  PcmBuffer whole;
  std::string err;
  CHECK(decodeSound(f->data(), f->size(), whole, err));
  std::unique_ptr<MusicSource> m = openMusic(f, err);
  CHECK(m != nullptr);
  CHECK_EQ(m->rate(), 44100);
  CHECK_EQ(m->channels(), 2);
  std::vector<int16_t> all;
  int16_t buf[1000 * 2];
  int n;
  while ((n = m->read(buf, 1000)) > 0) all.insert(all.end(), buf, buf + n * 2);
  CHECK(all == whole.samples);
  CHECK(m->totalFrames() > 0);
  // Seek to the middle, then read: the same samples as in the whole decode.
  CHECK(m->seek(10000));
  CHECK_EQ(m->read(buf, 100), 100);
  CHECK(memcmp(buf, whole.samples.data() + 10000 * 2, 100 * 2 * sizeof(int16_t)) == 0);
  CHECK(m->seek(0));
  CHECK_EQ(m->read(buf, 100), 100);
  CHECK(memcmp(buf, whole.samples.data(), 100 * 2 * sizeof(int16_t)) == 0);
}

TEST(audio_ogg_effect_limit_refuses_long_files) {
  FileBytes f = loadFixture("tone_stereo_44k.ogg");
  PcmBuffer b;
  std::string err;
  // The decoded size is checked while decoding and against the declared length first.
  CHECK(!decodeOgg(f->data(), f->size(), 1000, b, err));
  CHECK(err.find("too long") != std::string::npos);
  CHECK(b.samples.empty());
}

TEST(audio_ogg_truncated_files_never_overread) {
  FileBytes f = loadFixture("tone_mono_22k.ogg");
  CHECK(f->size() > 100);
  for (size_t cut = 0; cut < f->size(); cut += (cut < 400 ? 1 : 37)) {
    FileBytes t(new std::vector<uint8_t>(f->begin(), f->begin() + cut));
    PcmBuffer b;
    std::string err;
    bool ok = decodeSound(t->data(), t->size(), b, err);
    CHECK(ok || !err.empty());
    if (ok) CHECK(b.frames() <= 11025 + 2048);
    std::unique_ptr<MusicSource> m = openMusic(t, err);  // streaming must survive the same cuts
    if (m) {
      int16_t buf[512 * 2];
      int total = 0, n;
      while ((n = m->read(buf, 512)) > 0 && total < 100000) total += n;
    }
  }
}

TEST(audio_ogg_garbage_never_crashes) {
  FileBytes f = loadFixture("tone_stereo_44k.ogg");
  unsigned seed = 777;
  auto next = [&seed]() { seed = seed * 1664525u + 1013904223u; return seed >> 8; };
  for (int round = 0; round < 300; ++round) {
    std::vector<uint8_t>* g = new std::vector<uint8_t>(*f);
    int hits = 1 + round % 8;
    for (int h = 0; h < hits; ++h) {
      // The first pages hold the setup headers: hit them often, they are the dangerous part.
      size_t where = (round % 2 == 0) ? next() % (g->size() < 3000 ? g->size() : 3000) : next() % g->size();
      (*g)[where] = (uint8_t)next();
    }
    FileBytes damaged(g);
    PcmBuffer b;
    std::string err;
    bool ok = decodeSound(damaged->data(), damaged->size(), b, err);
    CHECK(ok || !err.empty());
    std::unique_ptr<MusicSource> m = openMusic(damaged, err);
    if (m) {
      int16_t buf[512 * 2];  // stereo
      int total = 0, n;
      while ((n = m->read(buf, 512)) > 0 && total < 200000) total += n;
      m->seek(5000);
      m->read(buf, 512);
    }
  }
  // Plain noise, and a file that starts like Ogg but is not.
  std::vector<uint8_t>* noise = new std::vector<uint8_t>(2000);
  for (auto& c : *noise) c = (uint8_t)next();
  memcpy(noise->data(), "OggS", 4);
  FileBytes nf(noise);
  PcmBuffer b;
  std::string err;
  CHECK(!decodeSound(nf->data(), nf->size(), b, err));
  CHECK(err.find("OGG") != std::string::npos);
}
