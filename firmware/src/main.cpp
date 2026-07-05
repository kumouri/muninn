// Muninn firmware entry point.
//
// Flow: I2S line-in tap (program + your voice, stereo) -> DSP mix to mono (per-channel gain) ->
// downsample to 16 kHz -> pack into 20 ms AUDIO frames (CAPTURING flag reflects the button) ->
// Transport to the PC listener. A debounced button toggles capture and emits CAPTURE_START/STOP
// control frames; the status LED shows state. The device is never in your monitoring path.
#include <Arduino.h>
#include <FastLED.h>

#include "audio/i2s_line_source.h"
#include "capture/trigger.h"
#include "config.h"
#include "muninn_dsp.h"
#include "muninn_proto.h"
#include "transport/usb_cdc.h"

namespace {
using namespace muninn;

I2sLineSource g_source;
UsbCdcTransport g_transport;  // v1 default; swap for WifiTcp/SdStore per config
CaptureTrigger g_trigger;

CRGB g_led[1];

// Tap pipeline state.
volatile bool g_capturing = false;
uint32_t g_seq = 0;
int16_t g_frame[proto::FRAME_SAMPLES];  // 16 kHz mono accumulator (320 samples)
size_t g_frame_fill = 0;
uint8_t g_out[proto::HEADER_SIZE + proto::FRAME_SAMPLES * sizeof(int16_t)];

void flushAudioFrame() {
  const uint8_t flags = g_capturing ? proto::CAPTURING : 0;
  size_t n = proto::encode(g_out, sizeof(g_out), proto::AUDIO, flags, g_seq++, millis(),
                           reinterpret_cast<const uint8_t*>(g_frame),
                           static_cast<uint16_t>(g_frame_fill * sizeof(int16_t)));
  if (n) g_transport.sendFrame(g_out, n);
  g_frame_fill = 0;
}

// Runs from the I2S read in loop() — keep it lean.
void onTap(const int16_t* interleaved, size_t frames, int channels, void*) {
  static int16_t mono48k[1024];
  static int16_t mono16k[proto::FRAME_SAMPLES * 4];

  // 1) Mix program (ch0) + your voice (ch1) to mono with per-channel gain.
  size_t mixed = dsp::mix_to_mono_q8(interleaved, frames, channels, MUNINN_GAIN_PROGRAM_Q8,
                                     MUNINN_GAIN_VOICE_Q8, mono48k,
                                     sizeof(mono48k) / sizeof(mono48k[0]));
  // 2) Decimate 48 kHz -> 16 kHz mono.
  size_t got = dsp::downsample_48k_to_16k(mono48k, mixed, 1, mono16k,
                                          sizeof(mono16k) / sizeof(mono16k[0]));
  // 3) Pack into 20 ms frames and ship.
  for (size_t i = 0; i < got; ++i) {
    g_frame[g_frame_fill++] = mono16k[i];
    if (g_frame_fill == proto::FRAME_SAMPLES) flushAudioFrame();
  }
}

void sendControl(uint8_t code) {
  uint8_t buf[proto::HEADER_SIZE + 1];
  size_t n = proto::encode(buf, sizeof(buf), proto::CONTROL, 0, g_seq++, millis(), &code, 1);
  if (n) g_transport.sendFrame(buf, n);
}

void setLed(uint32_t rgb) {
  g_led[0] = CRGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
  FastLED.show();
}
}  // namespace

void setup() {
  pinMode(PIN_CAPTURE_BUTTON, INPUT_PULLUP);
  FastLED.addLeds<WS2812, PIN_STATUS_LED, GRB>(g_led, 1);
  setLed(LED_COLOR_IDLE);

  g_transport.begin();
  g_source.setTapCallback(onTap, nullptr);
  g_source.begin();
}

void loop() {
  g_source.loop();  // reads the I2S ADC and drives onTap()

  // Button: active-low with INPUT_PULLUP, so pressed == LOW.
  const bool pressed = digitalRead(PIN_CAPTURE_BUTTON) == LOW;
  switch (g_trigger.update(pressed, millis())) {
    case CaptureEvent::Started:
      g_capturing = true;
      sendControl(proto::CTRL_CAPTURE_START);
      setLed(LED_COLOR_CAPTURING);
      break;
    case CaptureEvent::Stopped:
      g_capturing = false;
      sendControl(proto::CTRL_CAPTURE_STOP);
      setLed(LED_COLOR_IDLE);
      break;
    case CaptureEvent::None:
      break;
  }

  // Drain any inbound TEXT_ACK (transcript echo) — v1 just discards it.
  uint8_t in[128];
  g_transport.poll(in, sizeof(in));
}
