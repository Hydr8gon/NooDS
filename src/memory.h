/*
    Copyright 2019 Hydr8gon

    This file is part of NooDS.

    NooDS is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    NooDS is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with NooDS. If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef MEMORY_H
#define MEMORY_H

#include "interpreter.h"

namespace memory
{

extern uint8_t firmware[0x40000];
extern uint8_t *rom;

extern uint8_t bios9[0x8000];
extern uint8_t bios7[0x4000];

extern uint8_t paletteA[0x400];
extern uint8_t paletteB[0x400];
extern uint8_t vramA[0x20000];
extern uint8_t vramB[0x20000];
extern uint8_t vramC[0x20000];
extern uint8_t vramD[0x20000];

extern uint16_t *extPalettesA[6];
extern uint16_t *extPalettesB[4];

extern uint16_t *dispstat;
extern uint16_t *vcount;
extern uint16_t *powcnt1;

extern uint32_t *dispcntA, *dispcntB;
extern uint16_t *bg0cntA,  *bg0cntB;
extern uint16_t *bg1cntA,  *bg1cntB;
extern uint16_t *bg2cntA,  *bg2cntB;
extern uint16_t *bg3cntA,  *bg3cntB;
extern uint16_t *bg0hofsA, *bg0hofsB;
extern uint16_t *bg0vofsA, *bg0vofsB;
extern uint16_t *bg1hofsA, *bg1hofsB;
extern uint16_t *bg1vofsA, *bg1vofsB;
extern uint16_t *bg2hofsA, *bg2hofsB;
extern uint16_t *bg2vofsA, *bg2vofsB;
extern uint16_t *bg3hofsA, *bg3hofsB;
extern uint16_t *bg3vofsA, *bg3vofsB;

extern uint32_t *dma0sad9, *dma0sad7;
extern uint32_t *dma0dad9, *dma0dad7;
extern uint32_t *dma0cnt9, *dma0cnt7;
extern uint32_t *dma1sad9, *dma1sad7;
extern uint32_t *dma1dad9, *dma1dad7;
extern uint32_t *dma1cnt9, *dma1cnt7;
extern uint32_t *dma2sad9, *dma2sad7;
extern uint32_t *dma2dad9, *dma2dad7;
extern uint32_t *dma2cnt9, *dma2cnt7;
extern uint32_t *dma3sad9, *dma3sad7;
extern uint32_t *dma3dad9, *dma3dad7;
extern uint32_t *dma3cnt9, *dma3cnt7;

extern uint16_t *tm0count9, *tm0count7;
extern uint16_t *tm0cnt9,   *tm0cnt7;
extern uint16_t *tm1count9, *tm1count7;
extern uint16_t *tm1cnt9,   *tm1cnt7;
extern uint16_t *tm2count9, *tm2count7;
extern uint16_t *tm2cnt9,   *tm2cnt7;
extern uint16_t *tm3count9, *tm3count7;
extern uint16_t *tm3cnt9,   *tm3cnt7;

extern uint16_t *keyinput;
extern uint16_t *extkeyin;

extern uint16_t *auxspicnt9, *auxspicnt7;
extern uint32_t *romctrl9,   *romctrl7;
extern uint8_t  *romcmdout9, *romcmdout7;

extern uint16_t *spicnt;
extern uint16_t *spidata;

extern uint16_t *ime9, *ime7;
extern uint32_t *ie9,  *ie7;
extern uint32_t *irf9, *irf7;

void *vramMap(uint32_t address);

template <typename T> T read(interpreter::Cpu *cpu, uint32_t address);
template <typename T> void write(interpreter::Cpu *cpu, uint32_t address, T value);

void init();

}

#endif // MEMORY_H
