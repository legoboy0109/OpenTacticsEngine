// A stream of music that is decoded a little at a time. The mixer pulls from
// it on the main loop (no threads); an implementation owns whatever memory it
// reads from, so a source can outlive the code that opened it.
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ote {

class MusicSource {
 public:
  virtual ~MusicSource() {}
  virtual int rate() const = 0;      // 8000..48000
  virtual int channels() const = 0;  // 1 or 2
  virtual int64_t totalFrames() const = 0;  // -1 when unknown
  // Decodes up to maxFrames frames (16-bit, interleaved) into out; returns how
  // many it wrote, 0 at the end of the music (or on damaged data: it never
  // reads outside its buffer, whatever the file says).
  virtual int read(int16_t* out, int maxFrames) = 0;
  // Next read() continues at this frame. False when the stream cannot seek there.
  virtual bool seek(int64_t frame) = 0;
};

// The bytes of a file, shared so one read can feed several decoders.
typedef std::shared_ptr<const std::vector<uint8_t>> FileBytes;

}  // namespace ote
