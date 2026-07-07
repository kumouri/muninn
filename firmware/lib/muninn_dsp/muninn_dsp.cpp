#include "muninn_dsp.h"

namespace muninn::dsp {

namespace {
// Average all channels of one interleaved frame down to a single mono sample.
inline int32_t frame_to_mono(const int16_t* frame, int channels) {
  int32_t acc = 0;
  for (int c = 0; c < channels; ++c) acc += frame[c];
  return acc / channels;
}
// |x| computed in int32 so INT16_MIN doesn't overflow.
inline int32_t abs32(int16_t x) { return x < 0 ? -static_cast<int32_t>(x) : x; }
}  // namespace

Levels measure_levels(const int16_t* in, size_t in_frames, int channels, int16_t clip_threshold) {
  if (channels < 1) channels = 1;
  const int meter_ch = channels < 2 ? 1 : 2;  // meter at most 2 channels
  Levels lv;
  int32_t peak[2] = {0, 0};
  for (size_t i = 0; i < in_frames; ++i) {
    const int16_t* frame = in + i * channels;
    for (int c = 0; c < meter_ch; ++c) {
      int32_t a = abs32(frame[c]);
      if (a > peak[c]) peak[c] = a;
    }
  }
  for (int c = 0; c < 2; ++c) {
    if (peak[c] > 32767) peak[c] = 32767;
    lv.peak[c] = static_cast<int16_t>(peak[c]);
    lv.clip[c] = peak[c] >= clip_threshold;
  }
  return lv;
}

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

size_t resample_linear_mono(const int16_t* in, size_t in_len, int in_rate, int out_rate,
                            int16_t* out, size_t out_cap) {
  if (in_len == 0 || in_rate <= 0 || out_rate <= 0) return 0;
  size_t out_len = static_cast<size_t>(static_cast<uint64_t>(in_len) * out_rate / in_rate);
  if (out_len > out_cap) out_len = out_cap;
  const double step = static_cast<double>(in_rate) / out_rate;
  for (size_t i = 0; i < out_len; ++i) {
    const double src = i * step;
    const size_t j = static_cast<size_t>(src);
    const double frac = src - j;
    const int32_t a = in[j];
    const int32_t b = (j + 1 < in_len) ? in[j + 1] : in[j];
    out[i] = static_cast<int16_t>(a + (b - a) * frac);
  }
  return out_len;
}

size_t downsample_48k_to_16k_stereo(const int16_t* in, size_t in_frames, int16_t* out,
                                    size_t out_cap) {
  const size_t groups = in_frames / 3;  // 48000 / 16000 = 3
  size_t w = 0;
  for (size_t g = 0; g < groups; ++g) {
    if (w + 2 > out_cap) break;
    for (int ch = 0; ch < 2; ++ch) {
      const int32_t s0 = in[(g * 3 + 0) * 2 + ch];
      const int32_t s1 = in[(g * 3 + 1) * 2 + ch];
      const int32_t s2 = in[(g * 3 + 2) * 2 + ch];
      out[w++] = static_cast<int16_t>((s0 + s1 + s2) / 3);
    }
  }
  return w;
}

}  // namespace muninn::dsp
