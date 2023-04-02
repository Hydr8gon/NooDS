/*
    Copyright 2019-2023 Hydr8gon

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
#include <functional>
#include <mutex>
#include <string>

#include "defines.h"

class Core;

enum NdsCmdMode
{
    CMD_NONE = 0,
    CMD_HEADER,
    CMD_CHIP,
    CMD_SECURE,
    CMD_DATA
};

class Cartridge
{
    public:
        Cartridge(Core *core): core(core) {}
        ~Cartridge();

        virtual bool loadRom(std::string path);
        void writeSave();

        void trimRom();
        void resizeSave(int newSize, bool dirty = true);

        int getRomSize()  { return romSize;  }
        int getSaveSize() { return saveSize; }

    protected:
        Core *core;

        FILE *romFile = nullptr;
        uint8_t *rom = nullptr, *save = nullptr;
        int romSize = 0, saveSize = 0;
        bool saveDirty = false;
        std::mutex mutex;

        uint32_t romMask = 0;

        void loadRomSection(size_t offset, size_t size);

    private:
        std::string romName, saveName;
};

class CartridgeNds: public Cartridge
{
    public:
        CartridgeNds(Core *core);

        bool loadRom(std::string path);
        void directBoot();

        uint16_t readAuxSpiCnt(bool cpu)  { return auxSpiCnt[cpu];  }
        uint8_t  readAuxSpiData(bool cpu) { return auxSpiData[cpu]; }
        uint32_t readRomCtrl(bool cpu)    { return romCtrl[cpu];    }
        uint32_t readRomDataIn(bool cpu);

        void writeAuxSpiCnt(bool cpu, uint16_t mask, uint16_t value);
        void writeAuxSpiData(bool cpu, uint8_t value);
        void writeRomCtrl(bool cpu, uint32_t mask, uint32_t value);
        void writeRomCmdOutL(bool cpu, uint32_t mask, uint32_t value);
        void writeRomCmdOutH(bool cpu, uint32_t mask, uint32_t value);

    private:
        uint32_t romCode = 0;
        bool romEncrypted = false;
        NdsCmdMode cmdMode = CMD_NONE;

        uint32_t encTable[0x412] = {};
        uint32_t encCode[3] = {};

        uint32_t romAddrReal[2] = {}, romAddrVirt[2] = {};
        uint16_t blockSize[2] = {}, readCount[2] = {};
        uint32_t wordCycles[2] = {};
        bool encrypted[2] = {};

        uint8_t auxCommand[2] = {};
        uint32_t auxAddress[2] = {};
        int auxWriteCount[2] = {};

        uint16_t auxSpiCnt[2] = {};
        uint8_t auxSpiData[2] = {};
        uint32_t romCtrl[2] = {};
        uint64_t romCmdOut[2] = {};

        std::function<void()> wordReadyTasks[2];

        uint64_t encrypt64(uint64_t value);
        uint64_t decrypt64(uint64_t value);
        void initKeycode(int level);
        void applyKeycode();

        void wordReady(bool cpu);
};

class CartridgeGba: public Cartridge
{
    public:
        CartridgeGba(Core *core): Cartridge(core) {}

        bool loadRom(std::string path);

        uint8_t *getRom(uint32_t address);
        bool isEeprom(uint32_t address);

        uint8_t eepromRead();
        void eepromWrite(uint8_t value);

        uint8_t sramRead(uint32_t address);
        void sramWrite(uint32_t address, uint8_t value);

    private:
        int eepromCount = 0;
        uint16_t eepromCmd = 0;
        uint64_t eepromData = 0;
        bool eepromDone = false;

        uint8_t flashCmd = 0;
        bool bankSwap = false;
        bool flashErase = false;

        bool findString(std::string string);
};

FORCE_INLINE uint8_t *CartridgeGba::getRom(uint32_t address)
{
    return ((address &= romMask) < romSize) ? &rom[address] : nullptr;
}

FORCE_INLINE bool CartridgeGba::isEeprom(uint32_t address)
{
    return (saveSize == -1 || saveSize == 0x200 || saveSize == 0x2000) && (romSize <= 0x1000000 || address >= 0x0DFFFF00);
}

#endif // CARTRIDGE_H
