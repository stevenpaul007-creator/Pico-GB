/**
 * Copyright (C) 2022 by Mahyar Koshkouei <mk@deltabeard.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#if ENABLE_SDCARD

#include "card_loader.h"

#include "SdFat.h"
#include "common.h"
#include "filelistmenu.h"
#include "gb.h"
#include "hardware/flash.h"
#include "input.h"

#include <Arduino.h>

#if ENABLE_EXT_PSRAM
#include "psram.h"
#endif

#define RAM_SAVENAME_LENGTH 16 + 7

SdFs sd;
struct gb_save_state_s save
#ifdef ENABLE_RP2040_PSRAM
    PSRAM
#endif
    ;
bool is_real_time_savestate_loaded = false;
FileListMenu fileListMenu;
uint16_t num_page = 0;
uint16_t num_files;
uint16_t total_pages = 1;

bool init_sdcard_hardware() {
  SD_SPI.setMISO(SD_MISO_PIN);
  SD_SPI.setMOSI(SD_MOSI_PIN);
  SD_SPI.setSCK(SD_SCK_PIN);
  bool success = sd.begin(SdSpiConfig(SD_CS_PIN, SHARED_SPI, SD_SCK_MHZ(50), &SD_SPI));
  return success;
}

void init_sdcard() {
  Serial.println("Initialize SD-Card ...");

  if (!init_sdcard_hardware()) {
    tft.setCursor(0, ERROR_TEXT_OFFSET, FONT_ID);
    tft.setTextColor(TFT_RED);
    sd.printSdError(&tft);
    sd.printSdError(&Serial);
    reset(5000);
  }
  if (sd.vol()->fatType() == 0) { // vol() and fatType() do not access the sd-card
    error("Can't find a valid FAT16/FAT32/exFAT partition");
  }

  Serial.printf("SD-Card initialized: FAT-Type=%d\r\n", sd.vol()->fatType());
}

/**
 * Load a save file from the SD card
 */
void read_cart_ram_file(struct gb_s* gb) {
  char filename[RAM_SAVENAME_LENGTH];
  uint_fast32_t save_size;
  FsFile file;

  gb_get_rom_name(gb, filename);
  save_size = gb_get_save_size(gb);

  String f = String("SAVES/");
  f.concat(filename);
  f.toCharArray(filename, RAM_SAVENAME_LENGTH);

  if (save_size > 0) {
    if (!file.open(filename, O_RDONLY)) {
      Serial.printf("E f_open(%s) error\r\n", filename);
    } else {
      file.read(ram, file.size());
    }

    if (!file.close()) {
      Serial.printf("E f_close error\r\n");
    }
  }

  Serial.printf("I read_cart_ram_file(%s) COMPLETE (%lu bytes)\r\n", filename, save_size);
}

/**
 * Write a save file to the SD card
 */
void write_cart_ram_file(struct gb_s* gb) {
  char filename[RAM_SAVENAME_LENGTH];
  uint_fast32_t save_size;
  FsFile file;

  gb_get_rom_name(gb, filename);
  save_size = gb_get_save_size(gb);

  String f = String("SAVES/");
  f.concat(filename);
  f.toCharArray(filename, RAM_SAVENAME_LENGTH);

  if (save_size > 0) {
    if (!file.open(filename, O_WRONLY | O_CREAT)) {
      Serial.printf("E f_open(%s) error\r\n", filename);
      return;
    }

    file.write(ram, save_size);
    if (!file.close()) {
      Serial.printf("E f_close error\r\n");
    }
  }

  Serial.printf("I write_cart_ram_file(%s) COMPLETE (%lu bytes)\r\n", filename, save_size);
}

bool write_rom_sector_to_flash(FsFile& file, uint8_t* buffer, uint32_t offset) {
  int nread = file.read(buffer, FLASH_SECTOR_SIZE);
  if (nread < 0) {
    error("Failed to read file!");
  }

  if (nread == 0) {
    return false;
  }

  uint32_t flash_offset = ((uint32_t)&rom[offset]) - XIP_BASE;

  uint32_t ints = save_and_disable_interrupts();
  flash_range_erase(flash_offset, FLASH_SECTOR_SIZE);
  flash_range_program(flash_offset, buffer, FLASH_SECTOR_SIZE);
  restore_interrupts(ints);

  /* Read back target region and check programming */
  if (memcmp(&rom[offset], buffer, FLASH_SECTOR_SIZE) != 0) {
    //@REEMscope.close();
    error("Programming failed - Flash mismatch");
  }

  return true;
}

static void open_rom_file(FsFile& file, char* filename) {
  if (!file.open(filename, O_RDONLY)) {
    //@REEMscope.close();
    error("Failed to open ROM: " + String(filename));
  }
}

static void close_rom_file(FsFile& file) {
  if (!file.close()) {
    Serial.printf("E f_close error\r\n");
  }
}
#if ENABLE_RP2040_PSRAM
void load_cart_rom_file_to_PSRAM(char* filename) {
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0, FONT_ID);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.println("Loading ROM: ");

  uint8_t buffer[FLASH_SECTOR_SIZE];

  FsFile file;
  Serial.printf("psram: opening file '%s'\r\n", filename);
  open_rom_file(file, filename);

  uint32_t fileSize = file.size();
  Serial.printf("psram: file opened, size = %lu\r\n", fileSize);

  uint32_t offset = 0;

  if (rp2040.getPSRAMSize() == 0) {
    Serial.println("PSRAM not found or not initialized! Check platformio.ini board settings.");
  }

  psram_rom = (uint8_t*)pmalloc(fileSize);
  while (true) {
    tft.print("#");
    if (offset >= fileSize) {
      Serial.println("\nReached end of file");
      break;
    }

    int nread = file.read(buffer, FLASH_SECTOR_SIZE);
    if (nread < 0) {
      error("Failed to read file!");
    }
    if (nread == 0) {
      break;
    }

    memcpy(psram_rom + offset, buffer, (size_t)nread);

    uint8_t retry = 0;
    while (0 != memcmp(psram_rom + offset, buffer, (size_t)nread)) {
      retry++;
      if (retry >= 5) {
        Serial.printf("E copy psram error @ %07x\r\n", offset);

        for (int i = 0; i < nread; i++) {
          if (i % 16 == 0) {
            Serial.printf("\r\n%07x ", (offset + i));
          }
          Serial.printf("%02x", buffer[i]);
          Serial.printf(buffer[i] == psram_rom[offset + i] ? " " : "-");
          Serial.printf("%02x   ", psram_rom[offset + i]);
        }

        error("E copy psram error");
      }
      Serial.printf("I redo copy psram @ %07x\r\n", offset);
      memcpy(psram_rom + offset, buffer, (size_t)nread);
    }

    /* Next sector */
    offset += FLASH_SECTOR_SIZE;
  }

  Serial.printf("I load_cart_rom_file_to_PSRAM(%s) COMPLETE\r\n", filename);
  close_rom_file(file);
}

#endif

#if ENABLE_EXT_PSRAM
void load_cart_rom_file_to_rom_and_PSRAM(char* filename) {
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0, FONT_ID);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.println("Loading ROM: ");

  uint8_t buffer[FLASH_SECTOR_SIZE];

  FsFile file;
  Serial.printf("psram: opening file '%s'\r\n", filename);
  open_rom_file(file, filename);

  uint32_t fileSize = file.size();
  Serial.printf("psram: file opened, size = %lu\r\n", fileSize);

  Serial.printf("I Program target region...\r\n");

  uint32_t offset = 0;
  bool in_psram_section = false;

  while (true) {
    tft.print("#");

    // Check if we've crossed into PSRAM section
    if (!in_psram_section && offset >= MAX_ROM_SIZE_MB) {
      Serial.println("\nSwitching to PSRAM section");
      in_psram_section = true;
    }
    if (offset >= fileSize) {
      Serial.println("\nReached end of file");
      break;
    }

    // write to flash
    if (!in_psram_section) {
      // Use the working write_rom_sector_to_flash function
      if (!write_rom_sector_to_flash(file, buffer, offset)) {
        error("Flash programming failed");
      }
    } else {
      // write to PSRAM
      if (!psram_init()) {
        error("PSRAM init failed");
      } else {
        int nread = file.read(buffer, FLASH_SECTOR_SIZE);
        if (nread < 0) {
          error("Failed to read file!");
        }
        if (nread == 0) {
          break;
        }

        if (!psram_write(offset, buffer, (size_t)nread)) {
          Serial.println("psram: psram_write failed");
          return;
        }
      }
    }
    /* Next sector */
    offset += FLASH_SECTOR_SIZE;
  }

  Serial.printf("I load_cart_rom_file(%s) COMPLETE\r\n", filename);
  close_rom_file(file);
}
#else
/**
 * Load a .gb rom file in flash from the SD card
 */
void load_cart_rom_file(char* filename) {
  uint8_t buffer[FLASH_SECTOR_SIZE];

  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0, FONT_ID);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.println("Loading ROM: ");

  FsFile file;
  open_rom_file(file, filename);

  Serial.printf("I Program target region...\r\n");

  uint32_t offset = 0;
  while (write_rom_sector_to_flash(file, buffer, offset)) {
    tft.print("#");

    /* Next sector */
    offset += FLASH_SECTOR_SIZE;
    if (offset >= MAX_ROM_SIZE) {
      Serial.printf("I MAX_ROM_SIZE = (%d)\r\n", MAX_ROM_SIZE);
      Serial.printf("I offset = (%d)\r\n", offset);
      break;
    }
  }

  close_rom_file(file);

  Serial.printf("I load_cart_rom_file(%s) COMPLETE\r\n", filename);
}

#endif

static uint16_t read_file_page_from_card(char filename[FILES_PER_PAGE][MAX_PATH_LENGTH], uint16_t num_page) {
  FsFile dir;
  FsFile file;

  /* clear the filenames array */
  for (uint8_t ifile = 0; ifile < FILES_PER_PAGE; ifile++) {
    strcpy(filename[ifile], "");
  }

  if (!dir.open("/")) {
    error("Failed to open root dir");
  }

  /* search *.gb files */
  uint16_t num_files = 0;
  uint16_t num_file_offset = num_page * FILES_PER_PAGE;
  char currentFilename[MAX_PATH_LENGTH];
  while (file.openNext(&dir, O_RDONLY)) {
    file.getName(currentFilename, sizeof(currentFilename));

    auto currentFilenameStr = String(currentFilename);
    currentFilenameStr.toLowerCase();
    if (!currentFilenameStr.endsWith(".gb") && !currentFilenameStr.endsWith(".gbc")) {
      continue;
    }

    if (num_files < num_file_offset) {
      // skip the first N pages
    } else {
      // store the filenames of this page
      strcpy(filename[num_files % FILES_PER_PAGE], currentFilename);
    }

    num_files++;
    file.close();

    if (num_files >= num_file_offset + FILES_PER_PAGE) {
      break;
    }
  }

  dir.close();
  if (num_files == 0) {
    return 0;
  }
  if (num_files % FILES_PER_PAGE == 0) {
    return FILES_PER_PAGE;
  }
  return num_files % FILES_PER_PAGE;
}

/**
 * Function used by the rom file selector to display one page of .gb rom files
 */
static uint16_t rom_file_selector_display_page(uint16_t num_page) {
  char filename[FILES_PER_PAGE][MAX_PATH_LENGTH];
  uint16_t num_files = read_file_page_from_card(filename, num_page);

  for (uint8_t ifile = 0; ifile < num_files; ifile++) {
    fileListMenu.setTextAtIndex(filename[ifile], ifile);
  }
  return num_files;
}

/**
 * The ROM selector displays pages of up to FILES_PER_PAGE rom files
 * allowing the user to select which rom file to start
 * Copy your *.gb rom files to the root directory of the SD card
 */
void rom_file_selector() {
  /* display the first page with up to FILES_PER_PAGE rom files */
  num_files = rom_file_selector_display_page(num_page);

  fileListMenu.setOnNextPage(&onNextPage);
  fileListMenu.setOnPrevPage(&onPrevPage);
  fileListMenu.setAfterFileSelected(&afterFileSelected);
  fileListMenu.openMenu();
}

bool onNextPage() {
  /* select the next page */
  if (num_files < FILES_PER_PAGE) {
    /* no more pages */
    return false;
  }
  fileListMenu.clearMenu();
  num_page++;
  num_files = rom_file_selector_display_page(num_page);
  if (num_files == 0) {
    num_page--;
    num_files = rom_file_selector_display_page(num_page);
  }
  return true;
}
bool onPrevPage() {
  if (num_page <= 0) {
    return false;
  }
  fileListMenu.clearMenu();
  /* select the previous page */
  num_page--;
  num_files = rom_file_selector_display_page(num_page);
  return true;
}
void afterFileSelected() {
  /* copy the rom from the SD card to flash or PSRAM and start the game */
#if ENABLE_RP2040_PSRAM
  load_cart_rom_file_to_PSRAM(fileListMenu.getSelectedText());
#elif ENABLE_EXT_PSRAM
  load_cart_rom_file_to_rom_and_PSRAM(fileListMenu.getSelectedText());
#else
  load_cart_rom_file(fileListMenu.getSelectedText());
#endif
}

void save_state(struct gb_s* gb) {
  Serial.println("I save_state ...");
  save.mbc = gb->mbc;
  save.cart_ram = gb->cart_ram;
  save.num_rom_banks_mask = gb->num_rom_banks_mask;
  save.num_ram_banks = gb->num_ram_banks;
  save.selected_rom_bank = gb->selected_rom_bank;
  save.cart_ram_bank = gb->cart_ram_bank;
  save.enable_cart_ram = gb->enable_cart_ram;
  save.cart_mode_select = gb->cart_mode_select;
  save.rtc_latched = gb->rtc_latched;
  save.rtc_real = gb->rtc_real;
  save.cpu_reg = gb->cpu_reg;

  save.counter = gb->counter;
  memcpy(save.wram, gb->wram, WRAM_SIZE);
  memcpy(save.vram, gb->vram, VRAM_SIZE);
  memcpy(save.oam, gb->oam, OAM_SIZE);
  memcpy(save.hram_io, gb->hram_io, HRAM_IO_SIZE);

#if PEANUT_FULL_GBC_SUPPORT
  save.cgb.cgbMode = gb->cgb.cgbMode;
  save.cgb.doubleSpeed = gb->cgb.doubleSpeed;
  save.cgb.doubleSpeedPrep = gb->cgb.doubleSpeedPrep;
  save.cgb.wramBank = gb->cgb.wramBank;
  save.cgb.wramBankOffset = gb->cgb.wramBankOffset;
  save.cgb.vramBank = gb->cgb.vramBank;
  save.cgb.vramBankOffset = gb->cgb.vramBankOffset;
  save.cgb.OAMPaletteID = gb->cgb.OAMPaletteID;
  save.cgb.BGPaletteID = gb->cgb.BGPaletteID;
  save.cgb.OAMPaletteInc = gb->cgb.OAMPaletteInc;
  save.cgb.BGPaletteInc = gb->cgb.BGPaletteInc;
  save.cgb.dmaActive = gb->cgb.dmaActive;
  save.cgb.dmaMode = gb->cgb.dmaMode;
  save.cgb.dmaSize = gb->cgb.dmaSize;
  save.cgb.dmaSource = gb->cgb.dmaSource;
  save.cgb.dmaDest = gb->cgb.dmaDest;
  memcpy(save.cgb.fixPalette, gb->cgb.fixPalette, sizeof(gb->cgb.fixPalette));
  memcpy(save.cgb.OAMPalette, gb->cgb.OAMPalette, sizeof(gb->cgb.OAMPalette));
  memcpy(save.cgb.BGPalette, gb->cgb.BGPalette, sizeof(gb->cgb.BGPalette));

#endif

  Serial.println("I save_state done");
  is_real_time_savestate_loaded = true;

  char filename[RAM_SAVENAME_LENGTH];
  uint_fast32_t save_size;
  FsFile file;

  gb_get_rom_name(gb, filename);

  String f = String("rtsav/");
  f.concat(filename);
  f.toCharArray(filename, RAM_SAVENAME_LENGTH);
  Serial.printf("I f_open(%s) \r\n", filename);

  save_size = sizeof(save);
  if (save_size > 0) {
    if (!file.open(filename, O_WRONLY | O_CREAT)) {
      Serial.printf("E f_open(%s) error\r\n", filename);
      return;
    }

    file.write((uint8_t*)&save, sizeof(save));

    if (!file.close()) {
      Serial.printf("E f_close error\r\n");
    }
    Serial.println("I save_state written");
  }
}

void load_state(struct gb_s* gb) {
  Serial.println("I load_state ...");
  if (!is_real_time_savestate_loaded) {
#ifdef ENABLE_SDCARD
    Serial.println("I no savestate in ram, try loading file ...");
    char filename[RAM_SAVENAME_LENGTH];
    FsFile file;

    gb_get_rom_name(gb, filename);

    String f = String("rtsav/");
    f.concat(filename);
    f.toCharArray(filename, RAM_SAVENAME_LENGTH);
    if (!file.open(filename, O_RDONLY)) {
      Serial.printf("E f_open(%s) error\r\n", filename);
      return;
    } else {

      file.read((uint8_t*)&save, sizeof(save));
      is_real_time_savestate_loaded = true;

      if (!file.close()) {
        Serial.printf("E f_close error\r\n");
      }
    }
#else
    Serial.println("I no savestate in ram");
    return;
#endif
  }
  gb->mbc = save.mbc;
  gb->cart_ram = save.cart_ram;
  gb->num_rom_banks_mask = save.num_rom_banks_mask;
  gb->num_ram_banks = save.num_ram_banks;
  gb->selected_rom_bank = save.selected_rom_bank;
  gb->cart_ram_bank = save.cart_ram_bank;
  gb->enable_cart_ram = save.enable_cart_ram;
  gb->cart_mode_select = save.cart_mode_select;
  gb->rtc_latched = save.rtc_latched;
  gb->rtc_real = save.rtc_real;
  gb->cpu_reg = save.cpu_reg;

  gb->counter = save.counter;
  memcpy(gb->wram, save.wram, WRAM_SIZE);
  memcpy(gb->vram, save.vram, VRAM_SIZE);
  memcpy(gb->oam, save.oam, OAM_SIZE);
  memcpy(gb->hram_io, save.hram_io, HRAM_IO_SIZE);
#if PEANUT_FULL_GBC_SUPPORT
  gb->cgb.cgbMode = save.cgb.cgbMode;
  gb->cgb.doubleSpeed = save.cgb.doubleSpeed;
  gb->cgb.doubleSpeedPrep = save.cgb.doubleSpeedPrep;
  gb->cgb.wramBank = save.cgb.wramBank;
  gb->cgb.wramBankOffset = save.cgb.wramBankOffset;
  gb->cgb.vramBank = save.cgb.vramBank;
  gb->cgb.vramBankOffset = save.cgb.vramBankOffset;
  gb->cgb.OAMPaletteID = save.cgb.OAMPaletteID;
  gb->cgb.BGPaletteID = save.cgb.BGPaletteID;
  gb->cgb.OAMPaletteInc = save.cgb.OAMPaletteInc;
  gb->cgb.BGPaletteInc = save.cgb.BGPaletteInc;
  gb->cgb.dmaActive = save.cgb.dmaActive;
  gb->cgb.dmaMode = save.cgb.dmaMode;
  gb->cgb.dmaSize = save.cgb.dmaSize;
  gb->cgb.dmaSource = save.cgb.dmaSource;
  gb->cgb.dmaDest = save.cgb.dmaDest;
  memcpy(gb->cgb.fixPalette, save.cgb.fixPalette, sizeof(save.cgb.fixPalette));
  memcpy(gb->cgb.OAMPalette, save.cgb.OAMPalette, sizeof(save.cgb.OAMPalette));
  memcpy(gb->cgb.BGPalette, save.cgb.BGPalette, sizeof(save.cgb.BGPalette));

#endif
  Serial.println("I load_state loaded");
}

#endif
