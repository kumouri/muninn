#include "config.h"

// Compiled only for the A2DP build (original ESP32 with Classic Bluetooth). On the S3 line-in build
// this file is empty — BluetoothA2DPSink/Classic BT don't exist there.
#if MUNINN_FRONTEND == MUNINN_FRONTEND_A2DP_SINK

#include <BluetoothA2DPSink.h>

#include "audio/a2dp_sink_source.h"

namespace muninn {

static BluetoothA2DPSink s_sink;
static A2dpSinkSource* s_self = nullptr;

// Free-function trampoline the library calls with each PCM block.
static void a2dp_read_cb(const uint8_t* data, uint32_t len) {
  if (s_self) s_self->onA2dpData(data, len);
}

bool A2dpSinkSource::begin() {
  s_self = this;
  // false = do NOT forward to I2S: we are a silent sink (you monitor on wired headphones).
  s_sink.set_stream_reader(a2dp_read_cb, false);
  s_sink.start(MUNINN_A2DP_NAME);
  return true;
}

void A2dpSinkSource::setTapCallback(TapCallback cb, void* user) {
  tap_ = cb;
  tap_user_ = user;
}

void A2dpSinkSource::onA2dpData(const uint8_t* data, uint32_t len) {
  if (!tap_) return;
  // A2DP delivers interleaved 16-bit stereo PCM at MUNINN_A2DP_SAMPLE_RATE.
  const int16_t* pcm = reinterpret_cast<const int16_t*>(data);
  const size_t frames = len / (sizeof(int16_t) * 2);
  if (frames) tap_(pcm, frames, 2, tap_user_);
}

void A2dpSinkSource::loop() {
  // A2DP is serviced by the Bluetooth stack's own task; nothing to pump here.
}

}  // namespace muninn

#endif  // MUNINN_FRONTEND == MUNINN_FRONTEND_A2DP_SINK
