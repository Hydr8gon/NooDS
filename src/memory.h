/*
    Copyright 2019-2025 Hydr8gon

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

#pragma once

#include <cstdint>
#include "defines.h"

class Core;

struct VramMapping {
    uint8_t *mappings[7] = {};
    uint8_t count = 0;

    void add(uint8_t *mapping);
    template <typename T> T read(uint32_t address);
    template <typename T> void write(uint32_t address, T value);
};

class Memory {
public:
    // 32-bit address space, split into 4KB blocks
    uint8_t *readMap9A[0x100000] = {};
    uint8_t *readMap9B[0x100000] = {};
    uint8_t *readMap7[0x100000] = {};
    uint8_t *writeMap9A[0x100000] = {};
    uint8_t *writeMap9B[0x100000] = {};
    uint8_t *writeMap7[0x100000] = {};

    uint8_t palette[0x800] = {}; // 2KB palette
    uint8_t oam[0x800] = {}; // 2KB OAM
    uint8_t *engAExtPal[5] = {};
    uint8_t *engBExtPal[5] = {};
    uint8_t *tex3D[4] = {};
    uint8_t *pal3D[6] = {};

    Memory(Core *core): core(core) {};
    void saveState(FILE *file);
    void loadState(FILE *file);

    bool loadBios9();
    bool loadBios7();
    bool loadGbaBios();
    void copyBiosLogo(uint8_t *logo);

    void updateMap9(uint32_t start, uint32_t end, bool tcm = false);
    void updateMap7(uint32_t start, uint32_t end);
    void updateVram();

    template <typename T> T read(bool arm7, uint32_t address, bool tcm = true);
    template <typename T> void write(bool arm7, uint32_t address, T value, bool tcm = true);

private:
    Core *core;
    uint32_t gbaBiosAddr = 0;

    uint8_t bios9[0x8000] = {}; // 32KB ARM9 BIOS
    uint8_t bios7[0x4000] = {}; // 16KB ARM7 BIOS
    uint8_t gbaBios[0x4000] = {}; // 16KB GBA BIOS

    uint8_t ram[0x1000000] = {}; // 16MB main RAM
    uint8_t wram[0x8000] = {}; // 32KB shared WRAM
    uint8_t instrTcm[0x8000] = {}; // 32KB instruction TCM
    uint8_t dataTcm[0x4000] = {}; // 16KB data TCM
    uint8_t wram7[0x10000] = {}; // 64KB ARM7 WRAM
    uint8_t wifiRam[0x2000] {}; // 8KB WiFi RAM

    uint8_t vramA[0x20000] = {}; // 128KB VRAM block A
    uint8_t vramB[0x20000] = {}; // 128KB VRAM block B
    uint8_t vramC[0x20000] = {}; // 128KB VRAM block C
    uint8_t vramD[0x20000] = {}; // 128KB VRAM block D
    uint8_t vramE[0x10000] = {}; // 64KB VRAM block E
    uint8_t vramF[0x4000] = {}; // 16KB VRAM block F
    uint8_t vramG[0x4000] = {}; // 16KB VRAM block G
    uint8_t vramH[0x8000] = {}; // 32KB VRAM block H
    uint8_t vramI[0x4000] = {}; // 16KB VRAM block I

    VramMapping engABg[32];
    VramMapping engBBg[8];
    VramMapping engAObj[16];
    VramMapping engBObj[8];
    VramMapping lcdc[64];
    VramMapping vram7[2];

    uint32_t dmaFill[4] = {};
    uint8_t vramCnt[9] = {};
    uint8_t vramStat = 0;
    uint8_t wramCnt = 0;
    uint8_t haltCnt = 0;

    template <typename T> T readFallback(bool arm7, uint32_t address);
    template <typename T> void writeFallback(bool arm7, uint32_t address, T value);

    template <typename T> T ioRead9(uint32_t address);
    template <typename T> T ioRead7(uint32_t address);
    template <typename T> T ioReadGba(uint32_t address);
    template <typename T> void ioWrite9(uint32_t address, T value);
    template <typename T> void ioWrite7(uint32_t address, T value);
    template <typename T> void ioWriteGba(uint32_t address, T value);

    uint32_t readDmaFill(int channel) { return dmaFill[channel]; }
    uint8_t readVramCnt(int block) { return vramCnt[block]; }
    uint8_t readVramStat() { return vramStat; }
    uint8_t readWramCnt() { return wramCnt; }
    uint8_t readHaltCnt() { return haltCnt; }

    void writeDmaFill(int channel, uint32_t mask, uint32_t value);
    void writeVramCnt(int index, uint8_t value);
    void writeWramCnt(uint8_t value);
    void writeHaltCnt(uint8_t value);
    void writeGbaHaltCnt(uint8_t value);
};

template uint8_t Memory::read(bool arm7, uint32_t address, bool tcm);
template uint16_t Memory::read(bool arm7, uint32_t address, bool tcm);
template uint32_t Memory::read(bool arm7, uint32_t address, bool tcm);
template <typename T> FORCE_INLINE T Memory::read(bool arm7, uint32_t address, bool tcm) {
    // Look up a pointer to readable memory and read a value from it LSB-first
    uint8_t **readMap = arm7 ? readMap7 : (tcm ? readMap9A : readMap9B);
    if (uint8_t *data = readMap[address >> 12]) {
        T value = 0;
        data += address & (0x1000 - sizeof(T));
        for (uint32_t i = 0; i < sizeof(T); i++)
            value |= data[i] << (i * 8);
        return value;
    }

    // Handle special read cases that can't be mapped
    return readFallback<T>(arm7, address);
}

template void Memory::write(bool arm7, uint32_t address, uint8_t value, bool tcm);
template void Memory::write(bool arm7, uint32_t address, uint16_t value, bool tcm);
template void Memory::write(bool arm7, uint32_t address, uint32_t value, bool tcm);
template <typename T> FORCE_INLINE void Memory::write(bool arm7, uint32_t address, T value, bool tcm) {
    // Look up a pointer to writable memory and write a value to it LSB-first
    uint8_t **writeMap = arm7 ? writeMap7 : (tcm ? writeMap9A : writeMap9B);
    if (uint8_t *data = writeMap[address >> 12]) {
        data += address & (0x1000 - sizeof(T));
        for (uint32_t i = 0; i < sizeof(T); i++)
            data[i] = value >> (i * 8);
        return;
    }

    // Handle special write cases that can't be mapped
    return writeFallback<T>(arm7, address, value);
}
