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
#include <string>

class Core;

class Cartridge
{
    public:
        Cartridge(Core *core): core(core) {}
        ~Cartridge();

        void loadRom(std::string path);
        void loadGbaRom(std::string path);
        void directBoot();
        void writeSave();

        int getRomSize(bool gba);
        void trimRom(bool gba);

        int getSaveSize(bool gba);
        void setSaveSize(bool gba, int size);

        template <typename T> T gbaRomRead(uint32_t address);
        template <typename T> void gbaRomWrite(uint32_t address, T value);

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
        Core *core;

        std::string gbaRomName, gbaSaveName;
        uint8_t *gbaRom = nullptr, *gbaSave = nullptr;
        int gbaRomSize = 0, gbaSaveSize = 0;
        bool gbaSaveDirty = false;

        std::string romName, saveName;
        uint8_t *rom = nullptr, *save = nullptr;
        int romSize = 0, saveSize = 0;
        bool saveDirty = false;

        int gbaEepromCount = 0;
        uint16_t gbaEepromCmd = 0;
        uint64_t gbaEepromData = 0;
        bool gbaEepromDone = false;

        uint8_t gbaFlashCmd = 0;
        bool gbaBankSwap = false;
        bool gbaFlashErase = false;

        uint32_t encTable[0x412] = {};
        uint32_t encCode[3] = {};

        uint64_t command[2] = {};
        int blockSize[2] = {}, readCount[2] = {};
        bool encrypted[2] = {};

        uint8_t auxCommand[2] = {};
        uint32_t auxAddress[2] = {};
        int auxWriteCount[2] = {};

        uint16_t auxSpiCnt[2] = {};
        uint8_t auxSpiData[2] = {};
        uint32_t romCtrl[2] = {};
        uint64_t romCmdOut[2] = {};

        uint64_t encrypt64(uint64_t value);
        uint64_t decrypt64(uint64_t value);
        void initKeycode(int level);
        void applyKeycode();
};

#endif // CARTRIDGE_H
