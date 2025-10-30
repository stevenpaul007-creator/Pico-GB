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

#include <Arduino.h>
#include "SdFat.h"
#include "hardware/flash.h"

#include "common.h"
#include "input.h"
#include "gb.h"
#include "card_loader.h"

#if ENABLE_PSRAM
#include "psram.h"
#endif

#define RAM_SAVENAME_LENGTH 16 + 7

SdFs sd;

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
  f.toCharArray(filename,RAM_SAVENAME_LENGTH);

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
  f.toCharArray(filename,RAM_SAVENAME_LENGTH);

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

  uint32_t flash_offset = ((uint32_t) &rom[offset]) - XIP_BASE;

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
#if ENABLE_PSRAM
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
    if (!in_psram_section)
    { 
      // Use the working write_rom_sector_to_flash function
      if (!write_rom_sector_to_flash(file, buffer, offset)) {
        error("Flash programming failed");
      }
    }else{
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
    if (offset >= MAX_ROM_SIZE)
    {
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
    Serial.printf("I Found file: (%s)\r\n", currentFilenameStr);

    if (num_files < num_file_offset) {
      // skip the first N pages
    } else {
      // store the filenames of this page
      strcpy(filename[num_files%FILES_PER_PAGE], currentFilename);
      Serial.printf("I Copy file %02d Name: (%s)\r\n", num_files%FILES_PER_PAGE, currentFilenameStr);
    }

    num_files++;
    file.close();

    if (num_files >= num_file_offset + FILES_PER_PAGE) {
      break;
    }
  }

  dir.close();
  if (num_files == 0){
    return 0;
  }
  if (num_files % FILES_PER_PAGE == 0){
    return FILES_PER_PAGE;
  }
  return num_files%FILES_PER_PAGE;
}


void print_file_entry(char* s, uint8_t index, uint8_t num_files, bool selected = false) {
  if (num_files == 0) {
    const char* no_files = "<No files on card>";
    tft.drawString(no_files, ERROR_TEXT_OFFSET, index * FONT_HEIGHT, FONT_ID);
    return;
  }

  tft.setTextColor(TFT_WHITE, selected ? TFT_RED : TFT_BLACK);
  tft.drawString(s, 0, index * FONT_HEIGHT, FONT_ID);
}

/**
 * Function used by the rom file selector to display one page of .gb rom files
 */
static uint16_t rom_file_selector_display_page(char filename[FILES_PER_PAGE][MAX_PATH_LENGTH], uint16_t num_page) {
  uint16_t num_files = read_file_page_from_card(filename, num_page);

  /* display *.gb rom files on screen */
  tft.fillScreen(TFT_BLACK);
  for (uint8_t ifile = 0; ifile < num_files; ifile++) {
    print_file_entry(filename[ifile], ifile, num_files);
  }

  return num_files;
}

/**
 * The ROM selector displays pages of up to FILES_PER_PAGE rom files
 * allowing the user to select which rom file to start
 * Copy your *.gb rom files to the root directory of the SD card
 */
void rom_file_selector() {
  uint16_t num_page = 0;
  char filename[FILES_PER_PAGE][MAX_PATH_LENGTH];
  uint16_t num_files;
  uint16_t total_pages = 1;

  /* display the first page with up to FILES_PER_PAGE rom files */
  num_files = rom_file_selector_display_page(filename, num_page);

  /* select the first rom */
  uint8_t selected = 0;
  print_file_entry(filename[selected], selected, num_files, true);

  /* get user's input */
  bool up, down, left, right, a, b, select, start;
  while (true) {
    up = readJoypad(PIN_UP);
    down = readJoypad(PIN_DOWN);
    left = readJoypad(PIN_LEFT);
    right = readJoypad(PIN_RIGHT);
    a = readJoypad(PIN_A);
    b = readJoypad(PIN_B);
    select = readJoypad(PIN_SELECT);
    start = readJoypad(PIN_START);
    
    if (!start && !select) {
      reset();
    }
    if (!start) {
      /* re-start the last game (no need to reprogram flash) */
#if ENABLE_PSRAM
      load_cart_rom_file_to_rom_and_PSRAM(filename[selected]);
#endif
      break;
    }
    if (!a | !b) {
      /* copy the rom from the SD card to flash or PSRAM and start the game */
#if ENABLE_PSRAM
      load_cart_rom_file_to_rom_and_PSRAM(filename[selected]);
#else
      load_cart_rom_file(filename[selected]);
#endif
      break;
    }
    if (!down) {
      /* select the next rom */
      print_file_entry(filename[selected], selected, num_files);
      selected++;
      if (selected >= num_files)
        selected = 0;
      print_file_entry(filename[selected], selected, num_files, true);
      sleep_ms(200);
    }
    if (!up) {
      /* select the previous rom */
      print_file_entry(filename[selected], selected, num_files);
      if (selected == 0) {
        selected = num_files - 1;
      } else {
        selected--;
      }
      print_file_entry(filename[selected], selected, num_files, true);
      sleep_ms(200);
    }
    if (!right) {
      /* select the next page */
      if(num_files < FILES_PER_PAGE) {
        /* no more pages */
        continue;
      }
      num_page++;
      num_files = rom_file_selector_display_page(filename, num_page);
      if (num_files == 0) {
        /* no files in this page, go to the previous page */
        num_page--;
        num_files = rom_file_selector_display_page(filename, num_page);
      }
      /* select the first file */
      selected = 0;
      print_file_entry(filename[selected], selected, num_files, true);
      sleep_ms(200);
    }
    if ((!left) && num_page > 0) {
      /* select the previous page */
      num_page--;
      num_files = rom_file_selector_display_page(filename, num_page);
      /* select the first file */
      selected = 0;
      print_file_entry(filename[selected], selected, num_files, true);
      sleep_ms(200);
    }
    tight_loop_contents();
  }
}

#endif
