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

#ifndef IPC_H
#define IPC_H

#include <cstdint>
#include <queue>

class Interpreter;

class Ipc
{
    public:
        Ipc(Interpreter *arm9, Interpreter *arm7): arm9(arm9), arm7(arm7) {}

        uint8_t readIpcSync9(unsigned int byte)    { return ipcSync9    >> (byte * 8); }
        uint8_t readIpcSync7(unsigned int byte)    { return ipcSync7    >> (byte * 8); }
        uint8_t readIpcFifoCnt9(unsigned int byte) { return ipcFifoCnt9 >> (byte * 8); }
        uint8_t readIpcFifoCnt7(unsigned int byte) { return ipcFifoCnt7 >> (byte * 8); }
        uint8_t readIpcFifoRecv9(unsigned int byte);
        uint8_t readIpcFifoRecv7(unsigned int byte);

        void writeIpcSync9(unsigned int byte, uint8_t value);
        void writeIpcSync7(unsigned int byte, uint8_t value);
        void writeIpcFifoCnt9(unsigned int byte, uint8_t value);
        void writeIpcFifoCnt7(unsigned int byte, uint8_t value);
        void writeIpcFifoSend9(unsigned int byte, uint8_t value);
        void writeIpcFifoSend7(unsigned int byte, uint8_t value);

    private:
        std::queue<uint32_t> fifo9, fifo7;

        uint16_t ipcSync9 = 0, ipcSync7 = 0;
        uint16_t ipcFifoCnt9 = 0x0101, ipcFifoCnt7 = 0x0101;
        uint32_t ipcFifoSend9 = 0, ipcFifoSend7 = 0;
        uint32_t ipcFifoRecv9 = 0, ipcFifoRecv7 = 0;

        Interpreter *arm9, *arm7;
};

#endif // IPC_H
