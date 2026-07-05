// Muninn firmware entry point.
//
// Flow: I2S line-in tap (program + your voice, stereo) -> level metering + VAD -> DSP mix to mono
// (per-channel gain) -> downsample to 16 kHz -> pack into 20 ms AUDIO frames (CAPTURING flag) ->
// Transport to the PC listener. Capture toggles from the local button, an inbound CONTROL frame
// (PC hotkey), or voice activity (VAD), per MUNINN_CAPTURE_MODE. The status LED is a one-pixel VU
// meter (brightness follows level; red on clip). The device is never in your monitoring path.
#include <Arduino.h>
#include <FastLED.h>

#include <cstring>

#include "audio/i2s_line_source.h"
#include "capture/trigger.h"
#include "config.h"
#include "muninn_dsp.h"
#include "muninn_proto.h"
#include "muninn_vad.h"
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
Vad g_vad(MUNINN_VAD_THRESHOLD, MUNINN_VAD_HANG_MS);
CRGB g_led[1];

// Tap pipeline state.
volatile bool g_capturing = false;
volatile int16_t g_peak = 0;      // latest input peak, for the LED VU meter
volatile bool g_clip = false;     // latest clip state
volatile uint8_t g_vad_req = 0;   // 0 none, 1 start, 2 stop (produced in onTap, applied in loop)
uint32_t g_seq = 0;
#if MUNINN_STEREO_TAP
constexpr size_t kOutChannels = 2;  // diarization tap: interleaved 16 kHz stereo
#else
constexpr size_t kOutChannels = 1;  // mono mix (default)
#endif
constexpr size_t kFlushSamples = proto::FRAME_SAMPLES * kOutChannels;  // 20 ms per frame
int16_t g_frame[kFlushSamples];  // 16 kHz accumulator
size_t g_frame_fill = 0;
uint8_t g_out[proto::HEADER_SIZE + sizeof(g_frame)];

// Inbound (listener -> device) frame reassembly, for the PC hotkey's remote control.
uint8_t g_inbuf[256];
size_t g_inlen = 0;

void sendControl(uint8_t code) {
  uint8_t buf[proto::HEADER_SIZE + 1];
  size_t n = proto::encode(buf, sizeof(buf), proto::CONTROL, 0, g_seq++, millis(), &code, 1);
  if (n) g_transport.sendFrame(buf, n);
}

// Single place that flips capture state. `announce` = tell the listener (locally-initiated); false
// when the change came FROM the listener (a remote control frame), to avoid echo loops.
void applyCapture(bool on, bool announce) {
  g_capturing = on;
  if (announce) sendControl(on ? proto::CTRL_CAPTURE_START : proto::CTRL_CAPTURE_STOP);
}

// One-pixel VU meter: base color by capture state, brightness by peak, red on clip.
void renderLed() {
  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last < 30) return;  // ~33 Hz refresh
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
#if MUNINN_STEREO_TAP
  flags |= proto::STEREO;
#endif
  size_t n = proto::encode(g_out, sizeof(g_out), proto::AUDIO, flags, g_seq++, millis(),
                           reinterpret_cast<const uint8_t*>(g_frame),
                           static_cast<uint16_t>(g_frame_fill * sizeof(int16_t)));
  if (n) g_transport.sendFrame(g_out, n);
  g_frame_fill = 0;
}

// Runs from the I2S read in loop() — keep it lean.
void onTap(const int16_t* interleaved, size_t frames, int channels, void*) {
  // Metering + VAD run on the raw stereo (program on ch0, your voice on ch1).
  dsp::Levels lv = dsp::measure_levels(interleaved, frames, channels, MUNINN_CLIP_THRESHOLD);
  g_peak = lv.peak[0] > lv.peak[1] ? lv.peak[0] : lv.peak[1];
  g_clip = lv.clip[0] || lv.clip[1];
  const int16_t voice = channels >= 2 ? lv.peak[1] : lv.peak[0];
  switch (g_vad.update(voice, millis())) {
    case VadEvent::Started: g_vad_req = 1; break;
    case VadEvent::Stopped: g_vad_req = 2; break;
    case VadEvent::None: break;
  }

#if MUNINN_STEREO_TAP
  // Diarization tap: keep program (L) + voice (R) separate at 16 kHz.
  static int16_t st16k[proto::FRAME_SAMPLES * 4 * 2];
  size_t got = dsp::downsample_48k_to_16k_stereo(interleaved, frames, st16k,
                                                 sizeof(st16k) / sizeof(st16k[0]));
#else
  // Default: mix program + voice to mono, decimate to 16 kHz.
  static int16_t mono48k[1024];
  static int16_t st16k[proto::FRAME_SAMPLES * 4];  // "st16k" == mono samples here
  size_t mixed = dsp::mix_to_mono_q8(interleaved, frames, channels, MUNINN_GAIN_PROGRAM_Q8,
                                     MUNINN_GAIN_VOICE_Q8, mono48k,
                                     sizeof(mono48k) / sizeof(mono48k[0]));
  size_t got = dsp::downsample_48k_to_16k(mono48k, mixed, 1, st16k,
                                          sizeof(st16k) / sizeof(st16k[0]));
#endif
  for (size_t i = 0; i < got; ++i) {
    g_frame[g_frame_fill++] = st16k[i];
    if (g_frame_fill == kFlushSamples) flushAudioFrame();
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
  g_led[0] = CRGB((LED_COLOR_IDLE >> 16) & 0xFF, (LED_COLOR_IDLE >> 8) & 0xFF, LED_COLOR_IDLE & 0xFF);
  FastLED.show();

  g_transport.begin();
  g_source.setTapCallback(onTap, nullptr);
  g_source.begin();
}

void loop() {
  g_source.loop();  // reads the I2S ADC and drives onTap()

  // Local button (unless we're in VAD-only mode). Active-low with INPUT_PULLUP.
  if (MUNINN_CAPTURE_MODE != MUNINN_CAPTURE_VAD) {
    const bool pressed = digitalRead(PIN_CAPTURE_BUTTON) == LOW;
    switch (g_trigger.update(pressed, millis())) {
      case CaptureEvent::Started: applyCapture(true, /*announce=*/true); break;
      case CaptureEvent::Stopped: applyCapture(false, /*announce=*/true); break;
      case CaptureEvent::None: break;
    }
  }

  // Voice-activated capture (unless we're in button-only mode).
  if (MUNINN_CAPTURE_MODE != MUNINN_CAPTURE_BUTTON) {
    const uint8_t req = g_vad_req;
    g_vad_req = 0;
    if (req == 1) applyCapture(true, /*announce=*/true);
    else if (req == 2) applyCapture(false, /*announce=*/true);
  }

  handleInbound();  // remote capture control from the PC hotkey
  renderLed();      // one-pixel VU meter
}
