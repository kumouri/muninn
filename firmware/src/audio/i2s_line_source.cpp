#include "audio/i2s_line_source.h"

#include <Arduino.h>
#include <driver/i2s.h>

#include "config.h"

// PCM1808 stereo line ADC on I2S. The ESP32-S3 is I2S master and provides MCLK (SCKI), BCK, and
// LRCK; the PCM1808 returns 24-bit-in-32 I2S data, which we read as 16-bit for the tap.

namespace muninn {

static int16_t s_block[512 * MUNINN_ADC_CHANNELS];  // ~5 ms @ 48k stereo per read

bool I2sLineSource::begin() {
  i2s_config_t cfg = {};
  cfg.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX);
  cfg.sample_rate = MUNINN_ADC_SAMPLE_RATE_HZ;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;  // stereo
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.dma_buf_count = 8;
  cfg.dma_buf_len = 256;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.use_apll = true;  // cleaner audio clock for the ADC

  i2s_pin_config_t pins = {};
  pins.mck_io_num = PIN_I2S_MCLK;  // PCM1808 SCKI (system clock)
  pins.bck_io_num = PIN_I2S_BCK;
  pins.ws_io_num = PIN_I2S_LRCK;
  pins.data_out_num = I2S_PIN_NO_CHANGE;
  pins.data_in_num = PIN_I2S_DIN;  // <- PCM1808 DOUT

  if (i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr) != ESP_OK) return false;
  if (i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK) return false;
  ready_ = true;
  return true;
}

void I2sLineSource::setTapCallback(TapCallback cb, void* user) {
  tap_ = cb;
  tap_user_ = user;
}

void I2sLineSource::loop() {
  if (!ready_ || !tap_) return;
  size_t bytes_read = 0;
  // Short timeout so the main loop still services the button and transport.
  if (i2s_read(I2S_NUM_0, s_block, sizeof(s_block), &bytes_read, pdMS_TO_TICKS(10)) != ESP_OK)
    return;
  const size_t frames = bytes_read / (sizeof(int16_t) * MUNINN_ADC_CHANNELS);
  if (frames) tap_(s_block, frames, MUNINN_ADC_CHANNELS, tap_user_);
}

}  // namespace muninn
