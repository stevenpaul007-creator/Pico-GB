#include "psram.h"

#if ENABLE_PSRAM

// Opcodes
static const uint8_t PSRAM_CMD_READ = 0x03;      // standard read
static const uint8_t PSRAM_CMD_FAST_READ = 0x0B; // fast read with 1 dummy byte
static const uint8_t PSRAM_CMD_WRITE = 0x02;     // write

// Reasonable defaults
static const SPISettings psramSPISettings(80000000, MSBFIRST, SPI_MODE0);
static const size_t TRANSFER_CHUNK = 4096;
static const uint32_t DEFAULT_PSRAM_SIZE = 8 * 1024 * 1024; // 8MB for APS6404

uint32_t psram_size_bytes() {
  return DEFAULT_PSRAM_SIZE;
}

static inline void ensure_cs_pins() {
  pinMode(PSRAM_CS_PIN, OUTPUT);
  digitalWrite(PSRAM_CS_PIN, HIGH);
#if ENABLE_SDCARD
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
#endif
}

bool psram_init() {
  ensure_cs_pins();

  // Do NOT reconfigure or call begin() on the shared SPI object here.
  // The SD card driver already initializes SPI and SdFs expects to manage it.
  // Reinitializing SPI pins here can break open FsFile operations.

  return true;
}

static inline void deselect_sd_if_needed() {
#if ENABLE_SDCARD
  digitalWrite(SD_CS_PIN, HIGH);
#endif
}

bool psram_read(uint32_t addr, void* buf, size_t len) {
  if (!buf) return false;
  if (addr + len > psram_size_bytes()) return false;

  uint8_t *p = (uint8_t*)buf;
  size_t remaining = len;

  while (remaining) {
    size_t cur = remaining > TRANSFER_CHUNK ? TRANSFER_CHUNK : remaining;

    deselect_sd_if_needed();

    PSRAM_SPI.beginTransaction(psramSPISettings);
    digitalWrite(PSRAM_CS_PIN, LOW);

    // Use fast-read (0x0B) with a single dummy byte; many PSRAM chips accept it.
    PSRAM_SPI.transfer(PSRAM_CMD_FAST_READ);
    PSRAM_SPI.transfer((addr >> 16) & 0xFF);
    PSRAM_SPI.transfer((addr >> 8) & 0xFF);
    PSRAM_SPI.transfer((addr) & 0xFF);
    // dummy
    PSRAM_SPI.transfer(0x00);

    for (size_t i = 0; i < cur; ++i) {
      p[i] = PSRAM_SPI.transfer(0x00);
    }

    digitalWrite(PSRAM_CS_PIN, HIGH);
    PSRAM_SPI.endTransaction();

    addr += cur;
    p += cur;
    remaining -= cur;
  }

  return true;
}

bool psram_write(uint32_t addr, const void* buf, size_t len) {
  if (!buf) return false;
  if (addr + len > psram_size_bytes()) return false;

  const uint8_t *p = (const uint8_t*)buf;
  size_t remaining = len;

  while (remaining) {
    size_t cur = remaining > TRANSFER_CHUNK ? TRANSFER_CHUNK : remaining;

    deselect_sd_if_needed();

    PSRAM_SPI.beginTransaction(psramSPISettings);
    digitalWrite(PSRAM_CS_PIN, LOW);

    PSRAM_SPI.transfer(PSRAM_CMD_WRITE);
    PSRAM_SPI.transfer((addr >> 16) & 0xFF);
    PSRAM_SPI.transfer((addr >> 8) & 0xFF);
    PSRAM_SPI.transfer((addr) & 0xFF);

    for (size_t i = 0; i < cur; ++i) {
      PSRAM_SPI.transfer(p[i]);
    }

    digitalWrite(PSRAM_CS_PIN, HIGH);
    PSRAM_SPI.endTransaction();

    addr += cur;
    p += cur;
    remaining -= cur;
  }

  return true;
}

bool load_rom_to_psram(FsFile &f, uint32_t &out_size) {
  if (!f.isOpen()) return false;

  // Rewind
  if (!f.seekSet(0)) {
    Serial.println("psram: seekSet(0) failed");
    return false;
  }

  // Debug: report file size if available
  uint32_t fileSize = 0;
  #if defined(FsFile_size)
  fileSize = f.size();
  #else
  fileSize = f.size();
  #endif
  Serial.printf("psram: loading file, reported size=%lu\r\n", fileSize);

  uint8_t buf[TRANSFER_CHUNK];
  uint32_t addr = 0;
  int32_t n;
  while ((n = f.read(buf, sizeof(buf))) > 0) {
    // Debug: report the first successful read size
    if (addr == 0) {
      Serial.printf("psram: first read returned %d bytes\r\n", n);
    }

    if ((uint32_t)n + addr > psram_size_bytes()) {
      Serial.println("psram: file too big for PSRAM");
      return false; // too big
    }
    if (!psram_write(addr, buf, (size_t)n)) {
      Serial.println("psram: psram_write failed");
      return false;
    }
    addr += (uint32_t)n;
  }

  if (n < 0) {
    Serial.printf("psram: file read error (code=%d)\r\n", n);
  }

  out_size = addr;
  return true;
}

#else

// Stubs when PSRAM disabled so code can link
bool psram_init() { return false; }
bool psram_read(uint32_t addr, void* buf, size_t len) { (void)addr; (void)buf; (void)len; return false; }
bool psram_write(uint32_t addr, const void* buf, size_t len) { (void)addr; (void)buf; (void)len; return false; }
bool load_rom_to_psram(FsFile &f, uint32_t &out_size) { (void)f; out_size = 0; return false; }
uint32_t psram_size_bytes() { return 0; }

#endif
