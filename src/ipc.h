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
        Ipc(Interpreter *arm9, Interpreter *arm7): arm9(arm9), arm7(arm7) {}

        uint16_t readIpcSync9()    { return ipcSync9;    }
        uint16_t readIpcSync7()    { return ipcSync7;    }
        uint16_t readIpcFifoCnt9() { return ipcFifoCnt9; }
        uint16_t readIpcFifoCnt7() { return ipcFifoCnt7; }
        uint32_t readIpcFifoRecv9();
        uint32_t readIpcFifoRecv7();

        void writeIpcSync9(uint16_t mask, uint16_t value);
        void writeIpcSync7(uint16_t mask, uint16_t value);
        void writeIpcFifoCnt9(uint16_t mask, uint16_t value);
        void writeIpcFifoCnt7(uint16_t mask, uint16_t value);
        void writeIpcFifoSend9(uint32_t mask, uint32_t value);
        void writeIpcFifoSend7(uint32_t mask, uint32_t value);

    private:
        std::queue<uint32_t> fifo9, fifo7;

        uint16_t ipcSync9 = 0, ipcSync7 = 0;
        uint16_t ipcFifoCnt9 = 0x0101, ipcFifoCnt7 = 0x0101;
        uint32_t ipcFifoRecv9 = 0, ipcFifoRecv7 = 0;

        Interpreter *arm9, *arm7;
};

#endif // IPC_H
