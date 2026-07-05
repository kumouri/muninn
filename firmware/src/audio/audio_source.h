// AudioSource — the swappable audio front-end. v1 = USB-Audio passthrough (usb_uac_source);
// M3 stretch = Bluetooth A2DP relay (bt_relay_source). Everything downstream (DSP, capture,
// transport) is written against this interface so the front-end can change without touching them.
#pragma once
#include <cstddef>
#include <cstdint>

namespace muninn {

// Invoked with a block of tapped audio at the host sample rate (interleaved int16). The callback
// must be cheap and non-blocking — it runs in the audio path. Copy/queue, don't process inline.
using TapCallback = void (*)(const int16_t* interleaved, size_t frames, int channels, void* user);

class AudioSource {
 public:
  virtual ~AudioSource() = default;

  // Bring up the front-end (USB stack + DAC, or Bluetooth links). Returns false on failure.
  virtual bool begin() = 0;

  // Register the tap. The source calls `cb` as audio flows through.
  virtual void setTapCallback(TapCallback cb, void* user) = 0;

  // Service the front-end; call from the main loop (or a dedicated task).
  virtual void loop() = 0;

  virtual const char* name() const = 0;
};

}  // namespace muninn
