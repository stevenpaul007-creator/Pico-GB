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

#define AUDIO_BUF_SIZE 2048
BYTE snd_buf[AUDIO_BUF_SIZE]={0};
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
  return 2048;
}

void InfoNES_SoundOutput(int samples, BYTE* wave1, BYTE* wave2, BYTE* wave3, BYTE* wave4, BYTE* wave5) {
  // Serial.printf("I InfoNES_SoundOutput samples=%d\r\n", samples);
  /*
  int i;

  for (i = 0; i < samples; i++) {
    final_wave[fw_wr][i] = (unsigned char)wave1[i] + (unsigned char)wave2[i] + (unsigned char)wave3[i] + (unsigned char)wave4[i] + (unsigned char)wave5[i];
  }
  final_wave[fw_wr][i] = -1;
  fw_wr = 1 - fw_wr;
  */

  
    static int test_i=0;

    SoundOutputBuilding = true;
    while (samples)
    {
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
  speed_control();
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

  if (!parseROM(psram_rom)) {
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
  // if (line == 100) {
  //   // display pixel_buffer to serial monitor for debugging
  //   for (int i = 0; i < 320; i++) {
  //     Serial.printf("%04X ", pixels_buffer[i]);
  //   }
  //   Serial.printf("\n");
  // }
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

/*BGR565*/
// // #define CC(BGR) (BGR & 32767)
// #define CC(rgb) ((((rgb) & 0xFF00) >> 8) | (((rgb) & 0x00FF) << 8))
// const WORD __not_in_flash_func(NesPalette)[64] = {
// CC(0xAE73),CC(0xD120),CC(0x1500),CC(0x1340),CC(0x0E88),CC(0x02A8),CC(0x00A0),CC(0x4078),
// CC(0x6041),CC(0x2002),CC(0x8002),CC(0xE201),CC(0xEB19),CC(0x0000),CC(0x0000),CC(0x0000),
// CC(0xF7BD),CC(0x9D03),CC(0xDD21),CC(0x1E80),CC(0x17B8),CC(0x0BE0),CC(0x40D9),CC(0x61CA),
// CC(0x808B),CC(0xA004),CC(0x4005),CC(0x8704),CC(0x1104),CC(0x0000),CC(0x0000),CC(0x0000),
// CC(0xFFFF),CC(0xFF3D),CC(0xBF5C),CC(0x5FA4),CC(0xDFF3),CC(0xB6FB),CC(0xACFB),CC(0xC7FC),
// CC(0xE7F5),CC(0x8286),CC(0xE94E),CC(0xD35F),CC(0x5B07),CC(0x0000),CC(0x0000),CC(0x0000),
// CC(0xFFFF),CC(0x3FAF),CC(0xBFC6),CC(0x5FD6),CC(0x3FFE),CC(0x3BFE),CC(0xF6FD),CC(0xD5FE),
// CC(0x34FF),CC(0xF4E7),CC(0x97AF),CC(0xF9B7),CC(0xFE9F),CC(0x0000),CC(0x0000),CC(0x0000)
// };    

#define CC(rgb) ((((rgb) & 0xFF00) >> 8) | (((rgb) & 0x00FF) << 8))

const WORD __not_in_flash_func(NesPalette)[64] = {
// Original values: 0xAE73, 0xD120, 0x1500, ... (reds were 1st, 2nd, 12th) red to black
// Blacks: 0x0000
CC(0x0000),CC(0x0000),CC(0x1500),CC(0x1340),CC(0x0E88),CC(0x02A8),CC(0x00A0),CC(0x4078),
CC(0x6041),CC(0x2002),CC(0x8002),CC(0x0000),CC(0xEB19),CC(0x0000),CC(0x0000),CC(0x0000),
// Original values: 0xF7BD, 0x9D03, 0xDD21, ...
CC(0x0000),CC(0x0000),CC(0xDD21),CC(0x1E80),CC(0x17B8),CC(0x0BE0),CC(0x40D9),CC(0x61CA),
CC(0x808B),CC(0xA004),CC(0x4005),CC(0x0000),CC(0x1104),CC(0x0000),CC(0x0000),CC(0x0000),
// Original values: 0xFFFF, 0xFF3D, 0xBF5C, ... (These are white/pinks, not typically the main reds, but included for completeness)
CC(0xFFFF),CC(0x0000),CC(0xBF5C),CC(0x5FA4),CC(0xDFF3),CC(0xB6FB),CC(0xACFB),CC(0xC7FC),
CC(0xE7F5),CC(0x8286),CC(0xE94E),CC(0x0000),CC(0x5B07),CC(0x0000),CC(0x0000),CC(0x0000),
// Original values: 0xFFFF, 0x3FAF, 0xBFC6, ...
CC(0xFFFF),CC(0x0000),CC(0xBFC6),CC(0x5FD6),CC(0x3FFE),CC(0x3BFE),CC(0xF6FD),CC(0xD5FE),
CC(0x34FF),CC(0xF4E7),CC(0x97AF),CC(0x0000),CC(0xFE9F),CC(0x0000),CC(0x0000),CC(0x0000)
};

// #define CC(rgb) ((((rgb) & 0xFF00) >> 8) | (((rgb) & 0x00FF) << 8))
// const WORD __not_in_flash_func(NesPalette)[] = {
//     CC(0x0000), CC(0x1071), CC(0x0015), CC(0x2013), CC(0x440e), CC(0x5402), CC(0x5000), CC(0x3c20),
//     CC(0x20a0), CC(0x0100), CC(0x0140), CC(0x00e2), CC(0x0ceb), CC(0x0000), CC(0x0000), CC(0x0000),
//     CC(0x5ef7), CC(0x01dd), CC(0x10fd), CC(0x401e), CC(0x5c17), CC(0x700b), CC(0x6ca0), CC(0x6521),
//     CC(0x45c0), CC(0x0240), CC(0x02a0), CC(0x0247), CC(0x0211), CC(0x0000), CC(0x0000), CC(0x0000),
//     CC(0x7fff), CC(0x1eff), CC(0x2e5f), CC(0x223f), CC(0x79ff), CC(0x7dd6), CC(0x7dcc), CC(0x7e67),
//     CC(0x7ae7), CC(0x4342), CC(0x2769), CC(0x2ff3), CC(0x03bb), CC(0x0000), CC(0x0000), CC(0x0000),
//     CC(0x7fff), CC(0x579f), CC(0x635f), CC(0x6b3f), CC(0x7f1f), CC(0x7f1b), CC(0x7ef6), CC(0x7f75),
//     CC(0x7f94), CC(0x73f4), CC(0x57d7), CC(0x5bf9), CC(0x4ffe), CC(0x0000), CC(0x0000), CC(0x0000)
// };


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

  // moved variables outside function body because prevButtons gets initialized to 0 everytime the function is called.
  // This is strange because a static variable inside a function is only initialsed once and retains it's value
  // throughout different function calls.
  // Am i missing something?
  // static DWORD prevButtons = 0;
  // static int rapidFireMask = 0;
  // static int rapidFireCounter = 0;

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
