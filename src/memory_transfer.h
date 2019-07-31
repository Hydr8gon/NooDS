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

#ifndef MEMORY_TRANSFER_H
#define MEMORY_TRANSFER_H

#include "interpreter.h"

namespace memory_transfer
{

extern uint8_t firmware[0x40000];
extern uint8_t *rom;

void dmaTransfer(interpreter::Cpu *cpu, uint8_t channel);

void rtcWrite(uint8_t *value);

void fifoClear(interpreter::Cpu *cpuSend, interpreter::Cpu *cpuRecv);
void fifoSend(interpreter::Cpu *cpuSend, interpreter::Cpu *cpuRecv);
uint32_t fifoReceive(interpreter::Cpu *cpuSend, interpreter::Cpu *cpuRecv);

void romTransferStart(interpreter::Cpu *cpu);
uint32_t romTransfer(interpreter::Cpu *cpu);

void spiWrite(uint8_t value);

void init();

}

#endif // MEMORY_TRANSFER_H
