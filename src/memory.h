/*
    Copyright 2019 Hydr8gon

    This file is part of NoiDS.

    NoiDS is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    NoiDS is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with NoiDS. If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef MEMORY_H
#define MEMORY_H

#include "interpreter.h"

namespace memory
{

extern uint8_t vramA[0x20000];
extern uint8_t vramB[0x20000];
extern uint8_t vramC[0x20000];
extern uint8_t vramD[0x20000];

extern uint16_t ime9, ime7;
extern uint32_t ie9,  ie7;
extern uint32_t if9,  if7;

extern uint32_t dispcntA;
extern uint16_t dispstat;
extern uint16_t vcount;
extern uint16_t powcnt1;
extern uint32_t dispcntB;

uint32_t ioReadMap9(uint32_t address);
void ioWriteMap9(uint32_t address, uint32_t value);
void *memoryMap9(uint32_t address);

uint32_t ioReadMap7(uint32_t address);
void ioWriteMap7(uint32_t address, uint32_t value);
void *memoryMap7(uint32_t address);

template <typename T> T read(interpreter::Cpu *cpu, uint32_t address);
template <typename T> void write(interpreter::Cpu *cpu, uint32_t address, T value);

}

#endif // MEMORY_H
