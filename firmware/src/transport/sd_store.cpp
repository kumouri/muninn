#include "transport/sd_store.h"

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

#include "config.h"

// M2 store-and-forward. Frames are appended to /muninn/pending.mnn; they are self-delimiting (see
// docs/protocol.md), so drainTo() can stream the raw file straight to a live transport.

namespace muninn {

static const char* kPendingPath = "/muninn/pending.mnn";
static File s_file;

bool SdStore::begin() {
  static SPIClass spi(HSPI);
  spi.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
  if (!SD.begin(PIN_SD_CS, spi)) return false;
  SD.mkdir("/muninn");
  s_file = SD.open(kPendingPath, FILE_APPEND);
  ready_ = static_cast<bool>(s_file);
  return ready_;
}

size_t SdStore::sendFrame(const uint8_t* frame, size_t len) {
  if (!ready_) return 0;
  size_t n = s_file.write(frame, len);
  s_file.flush();
  return n;
}

size_t SdStore::poll(uint8_t*, size_t) { return 0; }

bool SdStore::hasPending() {
  if (!ready_) return false;
  File f = SD.open(kPendingPath, FILE_READ);
  bool pending = f && f.size() > 0;
  if (f) f.close();
  return pending;
}

size_t SdStore::drainTo(Transport& live) {
  if (!ready_) return 0;
  s_file.close();  // flush the append handle before reading

  File in = SD.open(kPendingPath, FILE_READ);
  if (!in) {
    s_file = SD.open(kPendingPath, FILE_APPEND);
    return 0;
  }

  uint8_t block[512];
  size_t forwarded = 0;
  bool ok = true;
  while (in.available() > 0) {
    int got = in.read(block, sizeof(block));
    if (got <= 0) break;
    if (live.sendFrame(block, got) != static_cast<size_t>(got)) {
      ok = false;  // live transport went down mid-drain; keep the file for next time
      break;
    }
    forwarded += got;
  }
  in.close();

  if (ok) {
    SD.remove(kPendingPath);  // fully forwarded — clear the backlog
    forwarded = forwarded ? forwarded : 0;
  }
  s_file = SD.open(kPendingPath, FILE_APPEND);  // reopen for continued buffering
  return ok ? forwarded : 0;
}

}  // namespace muninn
