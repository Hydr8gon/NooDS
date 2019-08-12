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

#include <queue>

#include "interpreter.h"

namespace memory
{

extern uint8_t bios9[0x8000];
extern uint8_t bios7[0x4000];

extern uint8_t vramA[0x20000];
extern uint8_t vramB[0x20000];
extern uint8_t vramC[0x20000];
extern uint8_t vramD[0x20000];

extern uint16_t *powcnt1;

extern uint16_t *extkeyin;

extern uint16_t *spicnt;
extern uint16_t *spidata;

void *vramMap(uint32_t address);

template <typename T> T read(interpreter::Cpu *cpu, uint32_t address);
template <typename T> void write(interpreter::Cpu *cpu, uint32_t address, T value);

void init();

}

#endif // MEMORY_H
