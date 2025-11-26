#include "nesinput.h"

#include "allservices.h"
#include "hardware/divider.h"
#include "lcd_core.h"
/*-------------------------------------------------------------------*/
/*  NES resources                                                    */
/*-------------------------------------------------------------------*/

#pragma region buffers
/* RAM 8K */
BYTE* RAM = RS_ram;
// BYTE RAM[RAM_SIZE] ;
// Share with romselect.cpp
void* InfoNes_GetRAM(size_t* size) {
  printf("Acquired RAM Buffer from emulator: %d bytes\n", RAM_SIZE);
  *size = RAM_SIZE;
  return SRAM;
}
/* SRAM 8K */
// BYTE SRAM[SRAM_SIZE] ;
BYTE* SRAM = RS_ram + RAM_SIZE;

/* Character Buffer 512B */
BYTE ChrBuf[CHRBUF_SIZE];

// Share with romselect.cpp
void* InfoNes_GetChrBuf(size_t* size) {
  printf("Acquired ChrBuf Buffer from emulator: %d bytes\n", CHRBUF_SIZE);
  *size = CHRBUF_SIZE;
  return ChrBuf;
}
/* PPU RAM 16K */
// BYTE PPURAM[PPURAM_SIZE];
BYTE* PPURAM = RS_ram + (RAM_SIZE + SRAM_SIZE);
// Share with romselect.cpp
void* InfoNes_GetPPURAM(size_t* size) {
  printf("Acquired PPURAM Buffer from emulator: %d bytes\n", PPURAM_SIZE);
  *size = PPURAM_SIZE;
  return PPURAM;
}
/* PPU BANK ( 1Kb * 16 ) */
BYTE* PPUBANK[16];
/* Sprite RAM 256B */
BYTE SPRRAM[SPRRAM_SIZE];
// Share with romselect.cpp
void* InfoNes_GetSPRRAM(size_t* size) {
  printf("Acquired SPRRAM Buffer from emulator: %d bytes\n", SPRRAM_SIZE);
  *size = SPRRAM_SIZE;
  return SPRRAM;
}
/* Scanline Table */
BYTE PPU_ScanTable[263];
#pragma endregion

static constexpr uintptr_t NES_FILE_ADDR = 0x10110000; // Location of .nes rom or tar archive with .nes roms
static constexpr uintptr_t NES_BATTERY_SAVE_ADDR = 0x100D0000; // 256K
                                                               //  = 8K   D0000 - 0D1FFF for persisting some variables after reboot
                                                               //  = 248K D2000 - 10FFFF for save games (=31 savegames MAX)
                                                               // grows towards NES_FILE_ADDR

bool fps_enabled = false;
static auto frame = 0;
static uint32_t start_tick_us = 0;
static uint32_t fps = 0;

// Positions in SRAM for storing state variables
#define STATUSINDICATORPOS 0
#define GAMEINDEXPOS 3
#define ADVANCEPOS 4
#define VOLUMEINDICATORPOS 5
#define MODEPOS 8
#define VOLUMEPOS 9
#define BACKLIGHTPOS 10
#define SCANLINEBUFFERLINES 24 // Max 40
#define SCANLINEPIXELS 240
#define SCANLINEBYTESIZE (SCANLINEPIXELS * sizeof(WORD))
// WORD scanlinebuffer0[SCANLINEPIXELS * SCANLINEBUFFERLINES];
// WORD* scanlinesbuffers[] = {scanlinebuffer0};

#define AUDIO_BUF_SIZE AUDIO_BUFFER_SIZE_BYTES
static int16_t snd_buf[AUDIO_BUF_SIZE] = {0};
int buf_residue_size=AUDIO_BUF_SIZE;
volatile bool SoundOutputBuilding = true;

// final wave buffer
int fw_wr, fw_rd;
int final_wave[2][735 + 1]; /* 44100 (just in case)/ 60 = 735 samples per sync */

// micomenu
bool micromenu;

bool saveSettingsAndReboot = false;
void InfoNES_ReleaseRom() {
  ROM = nullptr;
  VROM = nullptr;
}

void InfoNES_SoundInit() {
}

int InfoNES_SoundOpen(int samples_per_sync, int sample_rate) {
  return 0;
}

void InfoNES_SoundClose() {
}

int InfoNES_GetSoundBufferSize() {
  return AUDIO_BUF_SIZE;
}

/*
 *  call from InfoNES_pAPUHsync
 */
void __not_in_flash_func(InfoNES_SoundOutput)(int samples, BYTE *wave1, BYTE *wave2, BYTE *wave3, BYTE *wave4, BYTE *wave5){
  // Producer: write 8-bit mixed samples into circular buffer
  /*
  int i;

  for (i = 0; i < samples; i++) {
    final_wave[fw_wr][i] = (unsigned char)wave1[i] + (unsigned char)wave2[i] + (unsigned char)wave3[i] + (unsigned char)wave4[i] + (unsigned char)wave5[i];
  }
  final_wave[fw_wr][i] = -1;
  fw_wr = 1 - fw_wr;
  */

  static int test_i = 0;

  SoundOutputBuilding = true;

  while (samples) {
        // auto &ring = dvi_->getAudioRingBuffer();
        // auto n = std::min<int>(samples, ring.getWritableSize());
        auto n = std::min<int>(samples, buf_residue_size);
        // auto n = samples;
        if (!n)
        {
            return;
        }
        // auto p = ring.getWritePointer();
        auto p = &snd_buf[AUDIO_BUF_SIZE-buf_residue_size];
        // auto p = snd_buf;

        int ct = n;
        while (ct--)
        {
            uint8_t w1 = *wave1++;
            uint8_t w2 = *wave2++;
            uint8_t w3 = *wave3++; // triangle
            uint8_t w4 = *wave4++; // noise
            uint8_t w5 = *wave5++; // DPCM
             *p++ =  (((w1 * 2 + w2 * 2)/2)  + w3 * 1  + w4 * 1 * 4 + w5 * 2 * 1) / 4;
        }

        // ring.advanceWritePointer(n);
        samples -= n;
        buf_residue_size -= n;
        // snd_buf should not be full, just for case
        if(buf_residue_size <= 0) buf_residue_size = AUDIO_BUF_SIZE;
  }
  SoundOutputBuilding = false;
  // if(samples)
}
static void __not_in_flash_func(speed_control)(void) {
  static uint64_t last_blink = 0;

  // frame timing control
  uint64_t cur_time = time_us_64();
  uint64_t diff_time = cur_time - last_blink;
  // 1/60 = 16666 us
  while (last_blink + (16666) > cur_time) {
    cur_time = time_us_64();
  }

  last_blink = cur_time;
}

int InfoNES_LoadFrame() {
  //speed_control();
  if(!SoundOutputBuilding){
    srv.soundService.handleSoundLoop();
  }
  auto count = frame++;
#ifdef LED_ENABLED
  auto onOff = hw_divider_s32_quotient_inlined(count, 60) & 1;
  gpio_put(LED_PIN, onOff);
#endif
  uint32_t tick_us = time_us_64() - start_tick_us;
  // calculate fps and round to nearest value (instead of truncating/floor)
  fps = (1000000 - 1) / tick_us + 1;
  start_tick_us = time_us_64();
  return count;
}
uint32_t getCurrentNVRAMAddr() {
  // Save Games are stored towards address stored roms.
  // calculate address of save game slot
  // slot 0 is reserved. (Some state variables are stored at this location)
  uint32_t saveLocation = NES_BATTERY_SAVE_ADDR + SRAM_SIZE * (1 + 1);
  if (saveLocation >= NES_FILE_ADDR) {
    printf("No more save slots available, (Requested slot = 1)");
    return {};
  }
  return saveLocation;
}
bool loadNVRAM() {
  if (auto addr = getCurrentNVRAMAddr()) {
    printf("load SRAM %x\n", addr);
    memcpy(SRAM, reinterpret_cast<void*>(addr), SRAM_SIZE);
  }
  SRAMwritten = false;
  return true;
}

void loadState() {
  memcpy(SRAM, reinterpret_cast<void*>(NES_BATTERY_SAVE_ADDR), SRAM_SIZE);
}
void InfoNES_MessageBox(const char* pszMsg, ...) {
  Serial.printf("[MSG]");
  va_list args;
  va_start(args, pszMsg);
  vprintf(pszMsg, args);
  Serial.println(pszMsg);
  va_end(args);
  Serial.printf("\n");
}
char* ErrorMessage;
int ERRORMESSAGESIZE = 100;
void InfoNES_Error(const char* pszMsg, ...) {
  Serial.printf("[Error]");
  va_list args;
  va_start(args, pszMsg);
  vsnprintf(ErrorMessage, ERRORMESSAGESIZE, pszMsg, args);
  Serial.printf("%s", ErrorMessage);
  va_end(args);
  Serial.printf("\n");
}
inline bool checkNESMagic(const uint8_t* data) {
  bool ok = false;
  int nIdx;
  if (memcmp(data, "NES\x1a", 4) == 0) {
    uint8_t MapperNo = *(data + 6) >> 4;
    Serial.printf("I MapperNo = %d \r\n", MapperNo);
    for (nIdx = 0; MapperTable[nIdx].nMapperNo != -1; ++nIdx) {
      if (MapperTable[nIdx].nMapperNo == MapperNo) {
        ok = true;
        break;
      }
    }
  }
  return ok;
}
bool parseROM(const uint8_t* nesFile) {
  memcpy(&NesHeader, nesFile, sizeof(NesHeader));
  if (!checkNESMagic(NesHeader.byID)) {
    return false;
  }

  nesFile += sizeof(NesHeader);

  memset(SRAM, 0, SRAM_SIZE);

  if (NesHeader.byInfo1 & 4) {
    memcpy(&SRAM[0x1000], nesFile, 512);
    nesFile += 512;
  }

  auto romSize = NesHeader.byRomSize * 0x4000;
  ROM = (BYTE*)nesFile;
  nesFile += romSize;

  if (NesHeader.byVRomSize > 0) {
    auto vromSize = NesHeader.byVRomSize * 0x2000;
    VROM = (BYTE*)nesFile;
    nesFile += vromSize;
  }

  return true;
}

bool loadAndReset() {
  Serial.printf("loadAndReset\n");
  uint8_t* romPtr;

#if ENABLE_RP2040_PSRAM
  romPtr = psram_rom;
#else
  romPtr = RS_rom;
#endif

  if (!parseROM(romPtr)) {
    Serial.printf("NES file parse error.\n");
    Serial.flush();
    return false;
  }
  // if (loadNVRAM() == false) {
  //   return false;
  // }

  if (InfoNES_Reset() < 0) {
    Serial.printf("NES reset error.\n");
    Serial.flush();
    return false;
  }

  Serial.printf("NES file parse OK\n");
  Serial.flush();
  return true;
}
int InfoNES_Menu() {

  // InfoNES_Main() のループで最初に呼ばれる is called first in the loop of
  return loadAndReset() ? 0 : -1;
  // return 0;
}
void __not_in_flash_func(InfoNES_PostDrawLine)(int line, bool frommenu) {
  lcd_draw_line(nullptr, pixels_buffer, line);
}

void __not_in_flash_func(InfoNES_PreDrawLine)(int line) {
  InfoNES_SetLineBuffer(pixels_buffer, NES_DISP_WIDTH);
}

void initJoypad() {
}

int nesMain() {
  max_lcd_width = NES_DISP_WIDTH;
  max_lcd_height = NES_DISP_HEIGHT;

  micromenu = false;
  char errorMessage[30];
  saveSettingsAndReboot = false;
  strcpy(errorMessage, "");

  final_wave[0][0] = final_wave[1][0] = -1; // click fix
  fw_wr = fw_rd = 0;
  Serial.printf("Start program\n");
  Serial.flush();

  loadAndReset();

  // When system is rebooted after flashing SRAM, load the saved state and volume from flash and proceed.
  // loadState();
  return 0;
}
// color table in aaaarrrrggggbbbb format
//  a = alpha - 4 bit
//  r = red - 4 bit
//  g = green - 4 bit
//  b = blue - 4 bit
// Colors are stored as ggggbbbbaaaarrrr
// converted from http://wiki.picosystem.com/en/tools/image-converter

#define CC(x) (x & 32767)
const WORD __not_in_flash_func(NesPalette)[64] = {
    CC(0xAE73), CC(0xD120), CC(0x1500), CC(0x1340), CC(0x0E88), CC(0x02A8), CC(0x00A0), CC(0x4078),
    CC(0x6041), CC(0x2002), CC(0x8002), CC(0xE201), CC(0xEB19), CC(0x0000), CC(0x0000), CC(0x0000),
    CC(0xF7BD), CC(0x9D03), CC(0xDD21), CC(0x1E80), CC(0x17B8), CC(0x0BE0), CC(0x40D9), CC(0x61CA),
    CC(0x808B), CC(0xA004), CC(0x4005), CC(0x8704), CC(0x1104), CC(0x0000), CC(0x0000), CC(0x0000),
    CC(0xFFFF), CC(0xFF3D), CC(0xBF5C), CC(0x5FA4), CC(0xDFF3), CC(0xB6FB), CC(0xACFB), CC(0xC7FC),
    CC(0xE7F5), CC(0x8286), CC(0xE94E), CC(0xD35F), CC(0x5B07), CC(0x0000), CC(0x0000), CC(0x0000),
    CC(0xFFFF), CC(0x3FAF), CC(0xBFC6), CC(0x5FD6), CC(0x3FFE), CC(0x3BFE), CC(0xF6FD), CC(0xD5FE),
    CC(0x34FF), CC(0xF4E7), CC(0x97AF), CC(0xF9B7), CC(0xFE9F), CC(0x0000), CC(0x0000), CC(0x0000)};

int getbuttons() {
  srv.inputService.handleJoypad();
  if ((PRESSED_KEY(ButtonID::BTN_SELECT) ? GPSELECT : 0)
      && (PRESSED_KEY(ButtonID::BTN_START) ? GPSTART : 0)) {
    reset();
  }
  int key = (PRESSED_KEY(ButtonID::BTN_LEFT) ? GPLEFT : 0)
      | (PRESSED_KEY(ButtonID::BTN_RIGHT) ? GPRIGHT : 0)
      | (PRESSED_KEY(ButtonID::BTN_UP) ? GPUP : 0)
      | (PRESSED_KEY(ButtonID::BTN_DOWN) ? GPDOWN : 0)
      | (PRESSED_KEY(ButtonID::BTN_A) ? GPA : 0)
      | (PRESSED_KEY(ButtonID::BTN_B) ? GPB : 0)
      | (PRESSED_KEY(ButtonID::BTN_SELECT) ? GPSELECT : 0)
      | (PRESSED_KEY(ButtonID::BTN_START) ? GPSTART : 0)
      | 0;
  return key;
}
static DWORD prevButtons = 0;
static int rapidFireMask = 0;
static int rapidFireCounter = 0;
static bool jumptomenu = false;

/**
 * check buttons
 */
void InfoNES_PadState(DWORD* pdwPad1, DWORD* pdwPad2, DWORD* pdwSystem) {

  int v = getbuttons();

  ++rapidFireCounter;
  bool reset = jumptomenu = false;

  auto& dst = *pdwPad1;

  int rv = v;
  if (rapidFireCounter & 2) {
    // 15 fire/sec
    rv &= ~rapidFireMask;
  }

  dst = rv;

  auto p1 = v;

  auto pushed = v & ~prevButtons;

  prevButtons = v;

  *pdwSystem = reset ? PAD_SYS_QUIT : 0;
}


void nesAudioCallback(void* userdata, int16_t* stream, size_t len) {
  // Zero output buffer first
  memset(stream, 0, len);

  // How many 8-bit samples are available in snd_buf
  int available_bytes = AUDIO_BUF_SIZE - buf_residue_size;
  if (available_bytes <= 0) {
    return;
  }

  // Stream expects 16-bit stereo interleaved samples (2 channels), len is bytes.
  // Each input byte will be expanded to two int16_t (left/right), consuming 4 bytes in stream.
  int max_output_samples = len / 4; // number of input bytes we can convert
  int samples_to_copy = (int)std::min<int>(available_bytes, max_output_samples);

  for (int i = 0; i < samples_to_copy; ++i) {
    int16_t in = snd_buf[i];
    // Convert unsigned 8-bit [0..255] -> signed 16-bit roughly centered: [-32768..32767]
    int16_t s = (in - 128) << 8;
    // write stereo interleaved
    stream[2 * i] = s;
    stream[2 * i + 2] = s;
  }

  // Remove consumed bytes from snd_buf by memmoving remaining data to start
  int remaining = available_bytes - samples_to_copy;
  if (remaining > 0) {
    memmove(snd_buf, &snd_buf[samples_to_copy], remaining);
  }

  // update buffer residue: we've freed samples_to_copy bytes
  buf_residue_size += samples_to_copy;
}
