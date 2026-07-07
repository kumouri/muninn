// a2dp_sink_source — M4 front-end for the ORIGINAL ESP32 (Classic Bluetooth required; the S3 is
// BLE-only). A silent A2DP sink: it pairs with the 1Mii B03+ transmitter, receives the mixer mix
// (SBC-decoded to PCM), and hands it to the TapCallback. No audio output — you monitor on your wired
// headphones off the splitter, so nothing needs to be played back. Bridges Bluetooth audio into the
// PC, which Windows cannot do natively (it won't act as an A2DP sink).
#pragma once
#include "audio/audio_source.h"

namespace muninn {

class A2dpSinkSource : public AudioSource {
 public:
  bool begin() override;
  void setTapCallback(TapCallback cb, void* user) override;
  void loop() override;
  const char* name() const override { return "a2dp-sink"; }

  // A2DP data callback entry point (host stream -> us): interleaved 16-bit stereo PCM.
  void onA2dpData(const uint8_t* data, uint32_t len);

 private:
  TapCallback tap_ = nullptr;
  void* tap_user_ = nullptr;
};

}  // namespace muninn
