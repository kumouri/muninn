#include "transport/store_and_forward.h"

namespace muninn {

bool StoreAndForwardTransport::begin() {
  // The SD buffer is required; the primary may legitimately start down (listener offline).
  bool sd_ok = store_.begin();
  primary_.begin();
  return sd_ok;
}

size_t StoreAndForwardTransport::sendFrame(const uint8_t* frame, size_t len) {
  // If a backlog exists, try to flush it first so ordering is preserved.
  if (store_.hasPending()) {
    if (store_.drainTo(primary_) == 0) {
      return store_.sendFrame(frame, len);  // primary still down — keep buffering
    }
  }
  size_t n = primary_.sendFrame(frame, len);
  if (n == len) return n;
  return store_.sendFrame(frame, len);  // primary just went down — buffer this frame
}

}  // namespace muninn
