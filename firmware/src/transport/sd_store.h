// sd_store — M2 store-and-forward transport. Appends encoded frames to a file on the microSD card
// so captures survive without a live listener; a later session replays the file to the PC.
// This is the on-board "save" target.
#pragma once
#include "transport/transport.h"

namespace muninn {

class SdStoreTransport : public Transport {
 public:
  bool begin() override;
  size_t sendFrame(const uint8_t* frame, size_t len) override;
  size_t poll(uint8_t* buf, size_t cap) override;
  const char* name() const override { return "sd-store"; }

 private:
  bool ready_ = false;
};

}  // namespace muninn
