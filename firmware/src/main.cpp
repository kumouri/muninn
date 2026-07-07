// Muninn firmware entry point.
//
// Two audio front-ends behind the same pipeline (MUNINN_FRONTEND in config.h):
//   I2S_LINE  (ESP32-S3): PCM1808 stereo line tap, mixed/diarized program + voice.
//   A2DP_SINK (orig ESP32): silent Bluetooth sink capturing the B03+ mixer mix; voice is added on
//                           the desktop from its webcam mic (see the listener).
// Either way: tap -> DSP to 16 kHz -> 20 ms AUDIO frames -> transport to the PC listener. Capture
// toggles from the button, an inbound CONTROL frame (PC hotkey), or VAD. The device never sits in
// your monitoring path.
#include <Arduino.h>
#include <FastLED.h>

#include <cstring>

#include "capture/trigger.h"
#include "config.h"
#include "muninn_dsp.h"
#include "muninn_proto.h"
#include "muninn_vad.h"
#include "transport/sd_store.h"
#include "transport/store_and_forward.h"
#include "transport/usb_cdc.h"
#include "transport/wifi_tcp.h"

#if MUNINN_FRONTEND == MUNINN_FRONTEND_A2DP_SINK
#include "audio/a2dp_sink_source.h"
#else
#include "audio/i2s_line_source.h"
#endif

// Stereo output frames only for the line-in diarization tap.
#if MUNINN_FRONTEND == MUNINN_FRONTEND_I2S_LINE && MUNINN_STEREO_TAP
#define MUNINN_OUT_STEREO 1
#else
#define MUNINN_OUT_STEREO 0
#endif

namespace {
using namespace muninn;

// ── Transport selection (config.h: MUNINN_TRANSPORT) ────────────────────────────
#if MUNINN_TRANSPORT == MUNINN_TRANSPORT_WIFI_TCP
WifiTcpTransport g_wifi;
SdStore g_sd;
StoreAndForwardTransport g_transport(g_wifi, g_sd);
#elif MUNINN_TRANSPORT == MUNINN_TRANSPORT_SD
SdStore g_transport;
#else
UsbCdcTransport g_transport;
#endif

#if MUNINN_FRONTEND == MUNINN_FRONTEND_A2DP_SINK
A2dpSinkSource g_source;
#else
I2sLineSource g_source;
#endif

CaptureTrigger g_trigger;
Vad g_vad(MUNINN_VAD_THRESHOLD, MUNINN_VAD_HANG_MS);
CRGB g_led[1];

// Tap pipeline state.
volatile bool g_capturing = false;
volatile int16_t g_peak = 0;
volatile bool g_clip = false;
volatile int16_t g_peak_l = 0;
volatile int16_t g_peak_r = 0;
volatile bool g_clip_l = false;
volatile bool g_clip_r = false;
volatile uint8_t g_vad_req = 0;  // 0 none, 1 start, 2 stop
uint32_t g_seq = 0;

#if MUNINN_OUT_STEREO
constexpr size_t kOutChannels = 2;
#else
constexpr size_t kOutChannels = 1;
#endif
constexpr size_t kFlushSamples = proto::FRAME_SAMPLES * kOutChannels;
int16_t g_frame[kFlushSamples];
size_t g_frame_fill = 0;
uint8_t g_out[proto::HEADER_SIZE + sizeof(g_frame)];

uint8_t g_inbuf[256];
size_t g_inlen = 0;

void sendControl(uint8_t code) {
  uint8_t buf[proto::HEADER_SIZE + 1];
  size_t n = proto::encode(buf, sizeof(buf), proto::CONTROL, 0, g_seq++, millis(), &code, 1);
  if (n) g_transport.sendFrame(buf, n);
}

void sendMeter() {
  const uint16_t pl = static_cast<uint16_t>(g_peak_l);
  const uint16_t pr = static_cast<uint16_t>(g_peak_r);
  uint8_t p[5] = {static_cast<uint8_t>(pl & 0xFF), static_cast<uint8_t>(pl >> 8),
                  static_cast<uint8_t>(pr & 0xFF), static_cast<uint8_t>(pr >> 8),
                  static_cast<uint8_t>((g_clip_l ? 0x01 : 0) | (g_clip_r ? 0x02 : 0))};
  uint8_t buf[proto::HEADER_SIZE + sizeof(p)];
  size_t n = proto::encode(buf, sizeof(buf), proto::METER, 0, g_seq++, millis(), p, sizeof(p));
  if (n) g_transport.sendFrame(buf, n);
}

void applyCapture(bool on, bool announce) {
  g_capturing = on;
  if (announce) sendControl(on ? proto::CTRL_CAPTURE_START : proto::CTRL_CAPTURE_STOP);
}

void renderLed() {
  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last < 30) return;
  last = now;
  const uint32_t base = g_capturing ? LED_COLOR_CAPTURING : LED_COLOR_IDLE;
  CRGB c((base >> 16) & 0xFF, (base >> 8) & 0xFF, base & 0xFF);
  if (g_clip) {
    c = CRGB(255, 0, 0);
  } else if (MUNINN_METER_ENABLE) {
    int p = g_peak;
    if (p > MUNINN_METER_FULL_SCALE) p = MUNINN_METER_FULL_SCALE;
    uint8_t b = static_cast<uint8_t>(25 + static_cast<uint32_t>(p) * (255 - 25) / MUNINN_METER_FULL_SCALE);
    c.nscale8_video(b);
  }
  g_led[0] = c;
  FastLED.show();
}

void flushAudioFrame() {
  uint8_t flags = g_capturing ? proto::CAPTURING : 0;
#if MUNINN_OUT_STEREO
  flags |= proto::STEREO;
#endif
  size_t n = proto::encode(g_out, sizeof(g_out), proto::AUDIO, flags, g_seq++, millis(),
                           reinterpret_cast<const uint8_t*>(g_frame),
                           static_cast<uint16_t>(g_frame_fill * sizeof(int16_t)));
  if (n) g_transport.sendFrame(g_out, n);
  g_frame_fill = 0;
}

// Runs from the audio path — keep it lean.
void onTap(const int16_t* interleaved, size_t frames, int channels, void*) {
  // Metering + VAD on the raw input.
  dsp::Levels lv = dsp::measure_levels(interleaved, frames, channels, MUNINN_CLIP_THRESHOLD);
  g_peak_l = lv.peak[0];
  g_peak_r = lv.peak[1];
  g_clip_l = lv.clip[0];
  g_clip_r = lv.clip[1];
  g_peak = lv.peak[0] > lv.peak[1] ? lv.peak[0] : lv.peak[1];
  g_clip = lv.clip[0] || lv.clip[1];
#if MUNINN_FRONTEND == MUNINN_FRONTEND_A2DP_SINK
  const int16_t vad_level = g_peak;  // no separate voice channel; use program level
#else
  const int16_t vad_level = channels >= 2 ? lv.peak[1] : lv.peak[0];  // voice channel
#endif
  switch (g_vad.update(vad_level, millis())) {
    case VadEvent::Started: g_vad_req = 1; break;
    case VadEvent::Stopped: g_vad_req = 2; break;
    case VadEvent::None: break;
  }

  // Convert this block to the 16 kHz payload the wire protocol carries.
  const int16_t* packed;
  size_t got;
#if MUNINN_FRONTEND == MUNINN_FRONTEND_A2DP_SINK
  // A2DP program: 44.1 kHz stereo -> mono -> 16 kHz (linear; SBC rate isn't an integer of 16k).
  static int16_t mono_in[4096];
  static int16_t out16k[2048];
  size_t m = dsp::downmix_to_mono(interleaved, frames, channels, mono_in,
                                  sizeof(mono_in) / sizeof(mono_in[0]));
  got = dsp::resample_linear_mono(mono_in, m, MUNINN_A2DP_SAMPLE_RATE, proto::SAMPLE_RATE_HZ,
                                  out16k, sizeof(out16k) / sizeof(out16k[0]));
  packed = out16k;
#elif MUNINN_OUT_STEREO
  // Line-in diarization tap: keep program (L) + voice (R) separate at 16 kHz.
  static int16_t st16k[proto::FRAME_SAMPLES * 4 * 2];
  got = dsp::downsample_48k_to_16k_stereo(interleaved, frames, st16k,
                                          sizeof(st16k) / sizeof(st16k[0]));
  packed = st16k;
#else
  // Line-in default: mix program + voice to mono, decimate to 16 kHz.
  static int16_t mono48k[1024];
  static int16_t out16k[proto::FRAME_SAMPLES * 4];
  size_t mixed = dsp::mix_to_mono_q8(interleaved, frames, channels, MUNINN_GAIN_PROGRAM_Q8,
                                     MUNINN_GAIN_VOICE_Q8, mono48k,
                                     sizeof(mono48k) / sizeof(mono48k[0]));
  got = dsp::downsample_48k_to_16k(mono48k, mixed, 1, out16k, sizeof(out16k) / sizeof(out16k[0]));
  packed = out16k;
#endif
  for (size_t i = 0; i < got; ++i) {
    g_frame[g_frame_fill++] = packed[i];
    if (g_frame_fill == kFlushSamples) flushAudioFrame();
  }
}

// Decode inbound CONTROL frames from the listener (PC hotkey) and apply remote capture toggles.
void handleInbound() {
  uint8_t tmp[128];
  size_t got = g_transport.poll(tmp, sizeof(tmp));
  if (got) {
    if (g_inlen + got > sizeof(g_inbuf)) g_inlen = 0;
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
    } else {
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
  g_led[0] = CRGB((LED_COLOR_IDLE >> 16) & 0xFF, (LED_COLOR_IDLE >> 8) & 0xFF, LED_COLOR_IDLE & 0xFF);
  FastLED.show();

  g_transport.begin();
  g_source.setTapCallback(onTap, nullptr);
  g_source.begin();
}

void loop() {
  g_source.loop();

  if (MUNINN_CAPTURE_MODE != MUNINN_CAPTURE_VAD) {
    const bool pressed = digitalRead(PIN_CAPTURE_BUTTON) == LOW;
    switch (g_trigger.update(pressed, millis())) {
      case CaptureEvent::Started: applyCapture(true, /*announce=*/true); break;
      case CaptureEvent::Stopped: applyCapture(false, /*announce=*/true); break;
      case CaptureEvent::None: break;
    }
  }

  if (MUNINN_CAPTURE_MODE != MUNINN_CAPTURE_BUTTON) {
    const uint8_t req = g_vad_req;
    g_vad_req = 0;
    if (req == 1) applyCapture(true, /*announce=*/true);
    else if (req == 2) applyCapture(false, /*announce=*/true);
  }

  handleInbound();
  renderLed();

#if MUNINN_METER_REPORT_MS > 0
  static uint32_t last_meter = 0;
  if (millis() - last_meter >= MUNINN_METER_REPORT_MS) {
    last_meter = millis();
    sendMeter();
  }
#endif
}
