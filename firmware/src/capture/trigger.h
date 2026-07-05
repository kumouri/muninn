// CaptureTrigger — debounced push-button state machine. Each press toggles capture on/off;
// transitions are reported so main can emit CAPTURE_START / CAPTURE_STOP control frames and set
// the CAPTURING flag on audio frames.
#pragma once
#include <cstdint>

namespace muninn {

enum class CaptureEvent { None, Started, Stopped };

class CaptureTrigger {
 public:
  // `now_ms` is a millisecond clock (millis()). `pressed` is the debounced-raw button state
  // (true = pressed). Returns an event on a confirmed press edge.
  CaptureEvent update(bool raw_pressed, uint32_t now_ms);

  bool capturing() const { return capturing_; }

 private:
  bool capturing_ = false;
  bool last_stable_ = false;   // debounced button level
  bool last_raw_ = false;
  uint32_t last_change_ms_ = 0;
};

}  // namespace muninn
