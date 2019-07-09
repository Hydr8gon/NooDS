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

extern uint16_t *keyinput;
extern uint16_t *extkeyin;

extern uint16_t *ime9, *ime7;
extern uint32_t *ie9,  *ie7;
extern uint32_t *if9,  *if7;

void *vramMap(uint32_t address);

template <typename T> T read(interpreter::Cpu *cpu, uint32_t address);
template <typename T> void write(interpreter::Cpu *cpu, uint32_t address, T value);

void init();

}

#endif // MEMORY_H
