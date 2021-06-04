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

#ifndef MEMORY_H
#define MEMORY_H

#include <cstdint>

class Core;

class Memory
{
    public:
        Memory(Core *core): core(core) {};

        void loadBios();
        void loadGbaBios();

        void updateMap7(uint32_t start, uint32_t end);
        void updateMap9(uint32_t start, uint32_t end);

        template <typename T> T read(bool cpu, uint32_t address);
        template <typename T> void write(bool cpu, uint32_t address, T value);

        uint8_t  *getPalette()    { return palette;    }
        uint8_t  *getOam()        { return oam;        }
        uint8_t **getEngAExtPal() { return engAExtPal; }
        uint8_t **getEngBExtPal() { return engBExtPal; }
        uint8_t **getTex3D()      { return tex3D;      }
        uint8_t **getPal3D()      { return pal3D;      }

    private:
        Core *core;

        // 32-bit address space, split into 4KB blocks
        uint8_t *readMap9[0x100000]  = {};
        uint8_t *readMap7[0x100000]  = {};
        uint8_t *writeMap9[0x100000] = {};
        uint8_t *writeMap7[0x100000] = {};

        uint8_t bios9[0x8000]   = {}; // 32KB ARM9 BIOS
        uint8_t bios7[0x4000]   = {}; // 16KB ARM7 BIOS
        uint8_t gbaBios[0x4000] = {}; // 16KB GBA BIOS

        uint8_t ram[0x400000]    = {}; //  4MB main RAM
        uint8_t wram[0x8000]     = {}; // 32KB shared WRAM
        uint8_t instrTcm[0x8000] = {}; // 32KB instruction TCM
        uint8_t dataTcm[0x4000]  = {}; // 16KB data TCM
        uint8_t wram7[0x10000]   = {}; // 64KB ARM7 WRAM
        uint8_t wifiRam[0x2000]  = {}; //  8KB WiFi RAM

        uint8_t palette[0x800] = {}; //   2KB palette
        uint8_t vramA[0x20000] = {}; // 128KB VRAM block A
        uint8_t vramB[0x20000] = {}; // 128KB VRAM block B
        uint8_t vramC[0x20000] = {}; // 128KB VRAM block C
        uint8_t vramD[0x20000] = {}; // 128KB VRAM block D
        uint8_t vramE[0x10000] = {}; //  64KB VRAM block E
        uint8_t vramF[0x4000]  = {}; //  16KB VRAM block F
        uint8_t vramG[0x4000]  = {}; //  16KB VRAM block G
        uint8_t vramH[0x8000]  = {}; //  32KB VRAM block H
        uint8_t vramI[0x4000]  = {}; //  16KB VRAM block I
        uint8_t oam[0x800]     = {}; //   2KB OAM

        uint8_t *lcdc[64]      = {};
        uint8_t *engABg[32]    = {};
        uint8_t *engAObj[16]   = {};
        uint8_t *engAExtPal[5] = {};
        uint8_t *engBBg[8]     = {};
        uint8_t *engBObj[8]    = {};
        uint8_t *engBExtPal[5] = {};
        uint8_t *vram7[2]      = {};
        uint8_t *tex3D[4]      = {};
        uint8_t *pal3D[6]      = {};

        uint32_t dmaFill[4] = {};
        uint8_t vramCnt[9] = {};
        uint8_t vramStat = 0;
        uint8_t wramCnt = 0;
        uint8_t haltCnt = 0;

        template <typename T> T ioRead9(uint32_t address);
        template <typename T> T ioRead7(uint32_t address);
        template <typename T> T ioReadGba(uint32_t address);
        template <typename T> void ioWrite9(uint32_t address, T value);
        template <typename T> void ioWrite7(uint32_t address, T value);
        template <typename T> void ioWriteGba(uint32_t address, T value);

        uint32_t readDmaFill(int channel) { return dmaFill[channel]; }
        uint8_t  readVramCnt(int block)   { return vramCnt[block];   }
        uint8_t  readVramStat()           { return vramStat;         }
        uint8_t  readWramCnt()            { return wramCnt;          }
        uint8_t  readHaltCnt()            { return haltCnt;          }

        void writeDmaFill(int channel, uint32_t mask, uint32_t value);
        void writeVramCnt(int index, uint8_t value);
        void writeWramCnt(uint8_t value);
        void writeHaltCnt(uint8_t value);
        void writeGbaHaltCnt(uint8_t value);
};

#endif // MEMORY_H
