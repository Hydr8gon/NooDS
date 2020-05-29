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

#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include <cstdint>

#include "defines.h"

class Dma;
class Interpreter;
class Memory;

class Cartridge
{
    public:
        Cartridge(Dma *dma, Interpreter *cpu, Memory *memory): dma(dma), cpu(cpu), memory(memory) {}

        void setRom(uint8_t *rom, uint32_t romSize, uint8_t *save, uint32_t saveSize);
        void setGbaRom(uint8_t *gbaRom, uint32_t gbaRomSize, uint8_t *gbaSave, uint32_t gbaSaveSize);

        template <typename T> T gbaRomRead(uint32_t address);
        template <typename T> void gbaRomWrite(uint32_t address, T value);

        uint16_t readAuxSpiCnt()  { return auxSpiCnt;       }
        uint8_t  readAuxSpiData() { return auxSpiData;      }
        uint32_t readRomCtrl()    { return romCtrl;         }
        uint32_t readRomCmdOutL() { return romCmdOut;       }
        uint32_t readRomCmdOutH() { return romCmdOut >> 32; }
        uint32_t readRomDataIn();

        void writeAuxSpiCnt(uint16_t mask, uint16_t value);
        void writeAuxSpiData(uint8_t value);
        void writeRomCtrl(uint32_t mask, uint32_t value);
        void writeRomCmdOutL(uint32_t mask, uint32_t value);
        void writeRomCmdOutH(uint32_t mask, uint32_t value);

    private:
        uint64_t command = 0;
        int blockSize = 0, readCount = 0;
        bool encrypted = false;

        uint32_t encTable[0x412] = {};
        uint32_t encCode[3] = {};

        uint8_t auxCommand = 0;
        uint32_t auxAddress = 0;
        int auxWriteCount = 0;

        uint16_t auxSpiCnt = 0;
        uint8_t auxSpiData = 0;
        uint32_t romCtrl = 0;
        uint64_t romCmdOut = 0;

        int gbaEepromCount = 0;
        uint16_t gbaEepromCmd = 0;
        uint64_t gbaEepromData = 0;
        bool gbaEepromDone = false;

        uint8_t gbaFlashCmd = 0;
        bool gbaBankSwap = false;
        bool gbaFlashErase = false;

        uint8_t *rom = nullptr, *save = nullptr;
        int romSize = 0, saveSize = 0;

        uint8_t *gbaRom = nullptr, *gbaSave = nullptr;
        int gbaRomSize = 0, gbaSaveSize = 0;

        Dma *dma;
        Interpreter *cpu;
        Memory *memory;

        uint64_t encrypt64(uint64_t value);
        uint64_t decrypt64(uint64_t value);
        void initKeycode(int level);
        void applyKeycode();
};

#endif // CARTRIDGE_H
