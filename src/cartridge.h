/*
    Copyright 2019-2021 Hydr8gon

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

        void loadNdsRom(std::string path);
        void loadGbaRom(std::string path);
        void directBoot();
        void writeSave();

        void trimNdsRom() { trimRom(&ndsRom, &ndsRomSize, &ndsRomName); }
        void trimGbaRom() { trimRom(&gbaRom, &gbaRomSize, &gbaRomName); }

        void resizeNdsSave(int newSize) { resizeSave(newSize, &ndsSave, &ndsSaveSize, &ndsSaveDirty); }
        void resizeGbaSave(int newSize) { resizeSave(newSize, &gbaSave, &gbaSaveSize, &gbaSaveDirty); }

        int getNdsRomSize()  { return ndsRomSize;  }
        int getNdsSaveSize() { return ndsSaveSize; }

        int getGbaRomSize()  { return gbaRomSize;  }
        int getGbaSaveSize() { return gbaSaveSize; }
        uint8_t *getGbaRom() { return gbaRom;      }

        bool isGbaEeprom(uint32_t address)
        {
            return (gbaSaveSize == -1 || gbaSaveSize == 0x200 || gbaSaveSize == 0x2000) &&
                (gbaRomSize <= 0x1000000 || address >= 0x0DFFFF00);
        }
        uint8_t gbaEepromRead();
        void gbaEepromWrite(uint8_t value);

        uint8_t gbaSramRead(uint32_t address);
        void gbaSramWrite(uint32_t address, uint8_t value);

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

        std::string ndsRomName, ndsSaveName;
        uint8_t *ndsRom = nullptr, *ndsSave = nullptr;
        int ndsRomSize = 0, ndsSaveSize = 0;
        bool ndsSaveDirty = false;

        FILE *ndsRomFile = nullptr;
        uint32_t ndsRomCode = 0;
        bool ndsRomEncrypted = false;
        NdsCmdMode ndsCmdMode = CMD_NONE;

        int gbaEepromCount = 0;
        uint16_t gbaEepromCmd = 0;
        uint64_t gbaEepromData = 0;
        bool gbaEepromDone = false;

        uint8_t gbaFlashCmd = 0;
        bool gbaBankSwap = false;
        bool gbaFlashErase = false;

        uint32_t encTable[0x412] = {};
        uint32_t encCode[3] = {};

        uint32_t romAddrReal[2] = {}, romAddrVirt[2] = {};
        uint16_t blockSize[2] = {}, readCount[2] = {};
        bool encrypted[2] = {};

        uint8_t auxCommand[2] = {};
        uint32_t auxAddress[2] = {};
        int auxWriteCount[2] = {};

        uint16_t auxSpiCnt[2] = {};
        uint8_t auxSpiData[2] = {};
        uint32_t romCtrl[2] = {};
        uint64_t romCmdOut[2] = {};

        static void trimRom(uint8_t **rom, int *romSize, std::string *romName);
        static void resizeSave(int newSize, uint8_t **save, int *saveSize, bool *saveDirty);

        void loadRomSection(uint32_t offset, uint32_t size);

        uint64_t encrypt64(uint64_t value);
        uint64_t decrypt64(uint64_t value);
        void initKeycode(int level);
        void applyKeycode();
};

#endif // CARTRIDGE_H
