// muninn_vad — a simple energy-gated voice-activity detector with hangover, usable as an
// alternative capture trigger. Portable C++17, unit-tested on the host. Feed it a signal level
// (e.g. per-block peak of the voice channel) and a millisecond clock; it reports Started when the
// level crosses the threshold and Stopped once the level has stayed below it for `hang_ms`.
#pragma once
#include <cstdint>

namespace muninn {

enum class VadEvent { None, Started, Stopped };

class Vad {
 public:
  Vad(int16_t threshold, uint32_t hang_ms) : threshold_(threshold), hang_ms_(hang_ms) {}

  // `level` is a non-negative amplitude (peak or RMS). `now_ms` is a millis() clock.
  VadEvent update(int16_t level, uint32_t now_ms);

  bool active() const { return active_; }

 private:
  int16_t threshold_;
  uint32_t hang_ms_;
  bool active_ = false;
  bool primed_ = false;
  uint32_t last_loud_ms_ = 0;
};

}  // namespace muninn
