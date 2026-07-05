#include "capture/trigger.h"

#include "config.h"

namespace muninn {

CaptureEvent CaptureTrigger::update(bool raw_pressed, uint32_t now_ms) {
  if (raw_pressed != last_raw_) {
    last_raw_ = raw_pressed;
    last_change_ms_ = now_ms;
  }
  // Accept the raw level as stable once it has held for the debounce window.
  if ((now_ms - last_change_ms_) >= CAPTURE_DEBOUNCE_MS && raw_pressed != last_stable_) {
    const bool rising = (!last_stable_ && raw_pressed);
    last_stable_ = raw_pressed;
    if (rising) {  // one toggle per physical press
      capturing_ = !capturing_;
      return capturing_ ? CaptureEvent::Started : CaptureEvent::Stopped;
    }
  }
  return CaptureEvent::None;
}

}  // namespace muninn
