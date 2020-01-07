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

#ifndef GPU_2D_H
#define GPU_2D_H

#include <cstdint>

#include "defines.h"

class Memory;
class Gpu3DRenderer;

class Gpu2D
{
    public:
        Gpu2D(Memory *memory);
        Gpu2D(Gpu3DRenderer *gpu3DRenderer, Memory *memory);

        void drawScanline(int line);

        bool is3DEnabled() { return dispCnt & BIT(3); }

        uint32_t *getFramebuffer() { return framebuffer; }

        void setExtPalette(int slot, uint8_t *data) { extPalettes[slot] = data; }

        uint32_t readDispCnt()      { return dispCnt;      }
        uint16_t readBgCnt(int bg)  { return bgCnt[bg];    }
        uint16_t readMasterBright() { return masterBright; }

        void writeDispCnt(uint32_t mask, uint32_t value);
        void writeBgCnt(int bg, uint16_t mask, uint16_t value);
        void writeBgHOfs(int bg, uint16_t mask, uint16_t value);
        void writeBgVOfs(int bg, uint16_t mask, uint16_t value);
        void writeBgX(int bg, uint32_t mask, uint32_t value);
        void writeBgY(int bg, uint32_t mask, uint32_t value);
        void writeBgPA(int bg, uint16_t mask, uint16_t value);
        void writeBgPB(int bg, uint16_t mask, uint16_t value);
        void writeBgPC(int bg, uint16_t mask, uint16_t value);
        void writeBgPD(int bg, uint16_t mask, uint16_t value);
        void writeMasterBright(uint16_t mask, uint16_t value);

    private:
        uint32_t framebuffer[256 * 192] = {};
        uint32_t layers[8][256 * 192] = {};

        uint32_t dispCnt = 0;
        uint16_t bgCnt[4] = {};
        uint16_t bgHOfs[4] = {};
        uint16_t bgVOfs[4] = {};
        int32_t bgX[2] = {};
        int32_t bgY[2] = {};
        int16_t bgPA[2] = {};
        int16_t bgPB[2] = {};
        int16_t bgPC[2] = {};
        int16_t bgPD[2] = {};
        uint16_t masterBright = 0;

        uint8_t *palette, *oam;
        uint32_t bgVramAddr, objVramAddr;
        uint8_t *extPalettes[5] = {};

        Gpu3DRenderer *gpu3DRenderer;
        Memory *memory;

        uint32_t rgb5ToRgb6(uint16_t color);

        void drawText(int bg, int line);
        void drawAffine(int bg, int line);
        void drawExtended(int bg, int line);
        void drawObjects(int line);
};

#endif // GPU_2D_H
