// sd_store — M2 store-and-forward buffer. Appends encoded frames to a file on the microSD card so
// captures survive without a live listener; drainTo() later replays the file to a live transport.
// Usable directly as the on-board "save" target, or as the backing store for StoreAndForwardTransport.
#pragma once
#include "transport/transport.h"

namespace muninn {

class SdStore : public Transport {
 public:
  bool begin() override;
  size_t sendFrame(const uint8_t* frame, size_t len) override;  // appends to the pending file
  size_t poll(uint8_t* buf, size_t cap) override;               // one-way sink; returns 0
  const char* name() const override { return "sd-store"; }

  bool hasPending();
  // Replay the pending file's bytes into `live`. On full success, clears the pending file and
  // returns bytes forwarded; returns 0 if nothing pending or the live transport rejected data.
  size_t drainTo(Transport& live);

 private:
  bool ready_ = false;
};

}  // namespace muninn
