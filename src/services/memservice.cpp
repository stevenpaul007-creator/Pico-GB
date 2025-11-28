#include "memservice.h"

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

/* Character Buffer 32KB */
BYTE* ChrBuf = gb.wram;

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