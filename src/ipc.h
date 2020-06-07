/*
    Copyright 2019-2020 Hydr8gon

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

#ifndef IPC_H
#define IPC_H

#include <cstdint>
#include <queue>

class Interpreter;

class Ipc
{
    public:
        Ipc(Interpreter *arm9, Interpreter *arm7): cpus { arm9, arm7 } {}

        uint16_t readIpcSync(bool cpu)    { return ipcSync[cpu];    }
        uint16_t readIpcFifoCnt(bool cpu) { return ipcFifoCnt[cpu]; }
        uint32_t readIpcFifoRecv(bool cpu);

        void writeIpcSync(bool cpu, uint16_t mask, uint16_t value);
        void writeIpcFifoCnt(bool cpu, uint16_t mask, uint16_t value);
        void writeIpcFifoSend(bool cpu, uint32_t mask, uint32_t value);

    private:
        std::queue<uint32_t> fifos[2];

        uint16_t ipcSync[2] = {};
        uint16_t ipcFifoCnt[2] = { 0x0101, 0x0101 };
        uint32_t ipcFifoRecv[2] = {};

        Interpreter *cpus[2];
};

#endif // IPC_H
