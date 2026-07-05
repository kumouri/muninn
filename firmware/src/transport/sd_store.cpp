#include "transport/sd_store.h"

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

#include "config.h"

// M2 store-and-forward. Appends frames to /muninn/capture.mnn; a companion replay session streams
// the file to the listener. Frames are already self-delimiting (see docs/protocol.md).

namespace muninn {

static File s_file;

bool SdStoreTransport::begin() {
  SPIClass spi(HSPI);
  spi.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
  if (!SD.begin(PIN_SD_CS, spi)) return false;
  SD.mkdir("/muninn");
  s_file = SD.open("/muninn/capture.mnn", FILE_APPEND);
  ready_ = static_cast<bool>(s_file);
  return ready_;
}

size_t SdStoreTransport::sendFrame(const uint8_t* frame, size_t len) {
  if (!ready_) return 0;
  size_t n = s_file.write(frame, len);
  s_file.flush();
  return n;
}

size_t SdStoreTransport::poll(uint8_t*, size_t) {
  return 0;  // one-way sink
}

}  // namespace muninn
