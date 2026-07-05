// i2s_line_source — v1 front-end. Reads a stereo line-level tap from the mixer via an I2S ADC
// (PCM1808): channel L = program/monitor tap, channel R = your voice/mic bus. The block is handed
// to the TapCallback, which mixes to mono (with per-channel gain) and downsamples. The device is
// never in the monitoring path — you keep hearing audio through your own headphones/1Mii B03.
#pragma once
#include "audio/audio_source.h"

namespace muninn {

class I2sLineSource : public AudioSource {
 public:
  bool begin() override;
  void setTapCallback(TapCallback cb, void* user) override;
  void loop() override;  // polls the I2S ADC and fires the tap
  const char* name() const override { return "i2s-line"; }

 private:
  TapCallback tap_ = nullptr;
  void* tap_user_ = nullptr;
  bool ready_ = false;
};

}  // namespace muninn
