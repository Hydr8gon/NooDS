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

#ifndef INTERPRETER_H
#define INTERPRETER_H

#include <cstdint>
#include <queue>

namespace interpreter
{

typedef struct
{
    uint32_t *registers[16];
    uint32_t registersUsr[16];
    uint32_t registersFiq[7];
    uint32_t registersSvc[2];
    uint32_t registersAbt[2];
    uint32_t registersIrq[2];
    uint32_t registersUnd[2];

    uint32_t cpsr, *spsr;
    uint32_t spsrFiq, spsrSvc, spsrAbt, spsrIrq, spsrUnd;

    uint32_t *dmasad[4], *dmadad[4];
    uint32_t *dmacnt[4];
    uint32_t dmaDstAddrs[4], dmaSrcAddrs[4];

    uint16_t *tmcntL[4], *tmcntH[4];
    uint16_t timerReloads[4], timerScalers[4];

    uint16_t *ipcfifocnt;
    uint32_t *ipcfifosend, ipcfiforecv;
    std::queue<uint32_t> *fifo;

    uint16_t *auxspicnt;
    uint32_t *romctrl;
    uint64_t *romcmdout;

    uint16_t *ime;
    uint32_t *ie, *irf;

    bool halt;

    uint8_t type;
} Cpu;

extern Cpu arm9, arm7;

void execute(Cpu *cpu);
void setMode(Cpu *cpu, uint8_t mode);

void init();

}

#endif // INTERPRETER_H
