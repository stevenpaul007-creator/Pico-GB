#pragma once

#include "InfoNES.h"
#include "InfoNES_Mapper.h"
#include "InfoNES_System.h"
#include "InfoNES_pAPU.h"
#include "baseservice.h"

/*-------------------------------------------------------------------*/
/*  NES resources                                                    */
/*-------------------------------------------------------------------*/

/* RAM 8K */
extern BYTE* RAM;
/* SRAM 8K */
extern BYTE* SRAM;

/* Character Buffer 32KB */
extern BYTE* ChrBuf;

/* PPU RAM 16K */
extern BYTE* PPURAM;
/* PPU BANK ( 1Kb * 16 ) */
extern BYTE* PPUBANK[];
/* Sprite RAM 256B */
extern BYTE SPRRAM[SPRRAM_SIZE];
/* Scanline Table */
extern BYTE PPU_ScanTable[263];
