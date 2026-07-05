// store_and_forward — M2 composite transport. Sends via a primary (e.g. Wi-Fi); when the primary is
// unavailable, buffers frames to an SdStore and replays the backlog (in order) once it recovers.
// This is what makes the "wireless PC" path resilient to the listener being briefly offline.
#pragma once
#include "transport/sd_store.h"
#include "transport/transport.h"

namespace muninn {

class StoreAndForwardTransport : public Transport {
 public:
  StoreAndForwardTransport(Transport& primary, SdStore& store)
      : primary_(primary), store_(store) {}
  bool begin() override;
  size_t sendFrame(const uint8_t* frame, size_t len) override;
  size_t poll(uint8_t* buf, size_t cap) override { return primary_.poll(buf, cap); }
  const char* name() const override { return "store-and-forward"; }

 private:
  Transport& primary_;
  SdStore& store_;
};

}  // namespace muninn
