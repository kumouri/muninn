#include "muninn_dsp.h"

namespace muninn::dsp {

namespace {
// Average all channels of one interleaved frame down to a single mono sample.
inline int32_t frame_to_mono(const int16_t* frame, int channels) {
  int32_t acc = 0;
  for (int c = 0; c < channels; ++c) acc += frame[c];
  return acc / channels;
}
}  // namespace

size_t downmix_to_mono(const int16_t* in, size_t in_frames, int channels, int16_t* out,
                       size_t out_cap) {
  if (channels < 1) channels = 1;
  size_t n = in_frames < out_cap ? in_frames : out_cap;
  for (size_t i = 0; i < n; ++i) {
    out[i] = static_cast<int16_t>(frame_to_mono(in + i * channels, channels));
  }
  return n;
}

size_t mix_to_mono_q8(const int16_t* in, size_t in_frames, int channels, int g0_q8, int g1_q8,
                      int16_t* out, size_t out_cap) {
  if (channels < 1) channels = 1;
  const size_t n = in_frames < out_cap ? in_frames : out_cap;
  for (size_t i = 0; i < n; ++i) {
    const int16_t* frame = in + i * channels;
    int32_t acc = static_cast<int32_t>(frame[0]) * g0_q8;
    if (channels >= 2) acc += static_cast<int32_t>(frame[1]) * g1_q8;
    acc >>= 8;  // undo Q8
    if (acc > 32767) acc = 32767;
    if (acc < -32768) acc = -32768;
    out[i] = static_cast<int16_t>(acc);
  }
  return n;
}

size_t downsample_48k_to_16k(const int16_t* in, size_t in_frames, int channels,
                             int16_t* out, size_t out_cap) {
  if (channels < 1) channels = 1;
  const size_t groups = in_frames / 3;  // 48000 / 16000 = 3
  size_t written = 0;
  for (size_t g = 0; g < groups && written < out_cap; ++g) {
    const int16_t* f0 = in + (g * 3 + 0) * channels;
    const int16_t* f1 = in + (g * 3 + 1) * channels;
    const int16_t* f2 = in + (g * 3 + 2) * channels;
    // 3-tap moving average of the mono-downmixed frames (simple anti-alias).
    const int32_t avg =
        (frame_to_mono(f0, channels) + frame_to_mono(f1, channels) + frame_to_mono(f2, channels)) /
        3;
    out[written++] = static_cast<int16_t>(avg);
  }
  return written;
}

}  // namespace muninn::dsp
