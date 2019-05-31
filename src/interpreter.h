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

#ifndef INTERPRETER_H
#define INTERPRETER_H

#include <cstdint>

namespace interpreter
{

typedef struct
{
    uint32_t *registers[16];
    uint32_t *spsr;
    uint32_t cpsr;
    uint32_t registersUsr[16];
    uint32_t registersFiq[7];
    uint32_t registersSvc[2];
    uint32_t registersAbt[2];
    uint32_t registersIrq[2];
    uint32_t registersUnd[2];
    uint32_t spsrFiq, spsrSvc, spsrAbt, spsrIrq, spsrUnd;
    bool halt;
    uint8_t type;
} Cpu;

void execute(Cpu *cpu);
void setMode(Cpu *cpu, uint8_t mode);
void irq9(uint8_t type);
void irq7(uint8_t type);

uint32_t lsl(Cpu *cpu, uint32_t value, uint8_t amount, bool carry);
uint32_t lsr(Cpu *cpu, uint32_t value, uint8_t amount, bool carry);
uint32_t asr(Cpu *cpu, uint32_t value, uint8_t amount, bool carry);
uint32_t ror(Cpu *cpu, uint32_t value, uint8_t amount, bool carry);

void init();

}

#endif // INTERPRETER_H
