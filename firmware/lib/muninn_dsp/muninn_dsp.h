// Muninn DSP — downmix + downsample the tapped audio to 16 kHz mono (all Whisper needs).
// Portable C++17, no hardware dependencies (compiles on-target and on the host, env:native).
#pragma once
#include <cstddef>
#include <cstdint>

namespace muninn::dsp {

// Per-channel signal levels for metering / clip indication.
struct Levels {
  int16_t peak[2] = {0, 0};   // peak absolute amplitude, channel 0 and 1
  bool clip[2] = {false, false};  // true if that channel reached/exceeded clip_threshold
};

// Measure per-channel peak amplitude (and clip) over one interleaved block. Channels beyond the
// first two are ignored (Muninn's line-in is program + voice). Safe on INT16_MIN.
Levels measure_levels(const int16_t* in, size_t in_frames, int channels, int16_t clip_threshold);


// Downmix interleaved int16 PCM to mono, then decimate 48 kHz -> 16 kHz (factor 3) with a
// 3-tap moving-average anti-alias filter.
//
//   in         interleaved int16 samples, `in_frames` frames of `channels` samples each
//   in_frames  number of frames available (should be a multiple of 3; a partial trailing
//              group is dropped — feed multiples of 3, or carry the remainder yourself)
//   channels   1 (mono) or 2 (stereo); other values are treated as "average all channels"
//   out        destination for mono 16 kHz samples
//   out_cap    capacity of `out` in samples
//
// Returns the number of output samples written (= in_frames / 3, capped by out_cap).
size_t downsample_48k_to_16k(const int16_t* in, size_t in_frames, int channels,
                             int16_t* out, size_t out_cap);

// Downmix interleaved int16 PCM to mono in place-ish (no rate change). Returns frames written.
size_t downmix_to_mono(const int16_t* in, size_t in_frames, int channels, int16_t* out,
                       size_t out_cap);

// Mix interleaved int16 PCM to mono with per-channel gain, so a program tap and your voice bus can
// be balanced. Gains are Q8 fixed point (256 = unity, 128 = -6 dB). Channel 0 uses g0, channel 1
// uses g1; any further channels are ignored (line-in is stereo). Output is clamped to int16.
// This is how "add in my own voice" is realized: program on ch0, voice on ch1, push g1 up to taste.
size_t mix_to_mono_q8(const int16_t* in, size_t in_frames, int channels, int g0_q8, int g1_q8,
                      int16_t* out, size_t out_cap);

}  // namespace muninn::dsp
