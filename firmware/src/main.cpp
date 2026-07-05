// Muninn firmware entry point.
//
// Flow: I2S line-in tap (program + your voice, stereo) -> DSP mix to mono (per-channel gain) ->
// downsample to 16 kHz -> pack into 20 ms AUDIO frames (CAPTURING flag reflects capture state) ->
// Transport to the PC listener. Capture toggles from the local button OR an inbound CONTROL frame
// (the PC hotkey). The status LED shows state. The device is never in your monitoring path.
#include <Arduino.h>
#include <FastLED.h>

#include <cstring>

#include "audio/i2s_line_source.h"
#include "capture/trigger.h"
#include "config.h"
#include "muninn_dsp.h"
#include "muninn_proto.h"
#include "transport/sd_store.h"
#include "transport/store_and_forward.h"
#include "transport/usb_cdc.h"
#include "transport/wifi_tcp.h"

namespace {
using namespace muninn;

// ── Transport selection (config.h: MUNINN_TRANSPORT) ────────────────────────────
#if MUNINN_TRANSPORT == MUNINN_TRANSPORT_WIFI_TCP
WifiTcpTransport g_wifi;
SdStore g_sd;
StoreAndForwardTransport g_transport(g_wifi, g_sd);  // wireless + offline buffering
#elif MUNINN_TRANSPORT == MUNINN_TRANSPORT_SD
SdStore g_transport;
#else
UsbCdcTransport g_transport;  // v1 default
#endif

I2sLineSource g_source;
CaptureTrigger g_trigger;
CRGB g_led[1];

// Tap pipeline state.
volatile bool g_capturing = false;
uint32_t g_seq = 0;
int16_t g_frame[proto::FRAME_SAMPLES];  // 16 kHz mono accumulator (320 samples)
size_t g_frame_fill = 0;
uint8_t g_out[proto::HEADER_SIZE + proto::FRAME_SAMPLES * sizeof(int16_t)];

// Inbound (listener -> device) frame reassembly, for the PC hotkey's remote control.
uint8_t g_inbuf[256];
size_t g_inlen = 0;

void setLed(uint32_t rgb) {
  g_led[0] = CRGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
  FastLED.show();
}

void sendControl(uint8_t code) {
  uint8_t buf[proto::HEADER_SIZE + 1];
  size_t n = proto::encode(buf, sizeof(buf), proto::CONTROL, 0, g_seq++, millis(), &code, 1);
  if (n) g_transport.sendFrame(buf, n);
}

// Single place that flips capture state. `announce` = tell the listener (button-initiated); false
// when the change came FROM the listener (a remote/hotkey control frame), to avoid echo loops.
void applyCapture(bool on, bool announce) {
  g_capturing = on;
  setLed(on ? LED_COLOR_CAPTURING : LED_COLOR_IDLE);
  if (announce) sendControl(on ? proto::CTRL_CAPTURE_START : proto::CTRL_CAPTURE_STOP);
}

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

  size_t mixed = dsp::mix_to_mono_q8(interleaved, frames, channels, MUNINN_GAIN_PROGRAM_Q8,
                                     MUNINN_GAIN_VOICE_Q8, mono48k,
                                     sizeof(mono48k) / sizeof(mono48k[0]));
  size_t got = dsp::downsample_48k_to_16k(mono48k, mixed, 1, mono16k,
                                          sizeof(mono16k) / sizeof(mono16k[0]));
  for (size_t i = 0; i < got; ++i) {
    g_frame[g_frame_fill++] = mono16k[i];
    if (g_frame_fill == proto::FRAME_SAMPLES) flushAudioFrame();
  }
}

// Decode inbound CONTROL frames from the listener (PC hotkey) and apply remote capture toggles.
void handleInbound() {
  uint8_t tmp[128];
  size_t got = g_transport.poll(tmp, sizeof(tmp));
  if (got) {
    if (g_inlen + got > sizeof(g_inbuf)) g_inlen = 0;  // overflow guard — drop stale partial
    memcpy(g_inbuf + g_inlen, tmp, got);
    g_inlen += got;
  }

  size_t off = 0;
  while (off < g_inlen) {
    proto::Frame f;
    size_t consumed = 0;
    proto::Decode d = proto::decode(g_inbuf + off, g_inlen - off, &f, &consumed);
    if (d == proto::Decode::INCOMPLETE) break;
    if (d == proto::Decode::OK) {
      if (f.hdr.type == proto::CONTROL && f.hdr.payload_len >= 1) {
        if (f.payload[0] == proto::CTRL_CAPTURE_START) applyCapture(true, false);
        else if (f.payload[0] == proto::CTRL_CAPTURE_STOP) applyCapture(false, false);
      }
      off += consumed;
    } else {  // BAD_MAGIC — skip forward to resync
      off += consumed ? consumed : 1;
    }
  }
  if (off > 0) {
    memmove(g_inbuf, g_inbuf + off, g_inlen - off);
    g_inlen -= off;
  }
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

  // Local button: active-low with INPUT_PULLUP, so pressed == LOW.
  const bool pressed = digitalRead(PIN_CAPTURE_BUTTON) == LOW;
  switch (g_trigger.update(pressed, millis())) {
    case CaptureEvent::Started:
      applyCapture(true, /*announce=*/true);
      break;
    case CaptureEvent::Stopped:
      applyCapture(false, /*announce=*/true);
      break;
    case CaptureEvent::None:
      break;
  }

  handleInbound();  // remote capture control from the PC hotkey
}
