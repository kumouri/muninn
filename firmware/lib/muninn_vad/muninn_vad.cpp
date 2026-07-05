#include "muninn_vad.h"

namespace muninn {

VadEvent Vad::update(int16_t level, uint32_t now_ms) {
  const bool loud = level >= threshold_;
  if (loud) {
    last_loud_ms_ = now_ms;
    primed_ = true;
    if (!active_) {
      active_ = true;
      return VadEvent::Started;
    }
    return VadEvent::None;
  }
  // Quiet: close the segment only after the hangover window elapses.
  if (active_ && primed_ && (now_ms - last_loud_ms_) >= hang_ms_) {
    active_ = false;
    return VadEvent::Stopped;
  }
  return VadEvent::None;
}

}  // namespace muninn
