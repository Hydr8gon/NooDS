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

#ifndef GPU_2D_H
#define GPU_2D_H

#include <cstdint>

class Core;

class Gpu2D
{
    public:
        Gpu2D(Core *core, bool engine);

        void drawGbaScanline(int line);
        void drawScanline(int line);

        uint32_t *getFramebuffer() { return framebuffer; }
        uint32_t *getRawLine()     { return layers[0];   }

        uint32_t readDispCnt()      { return dispCnt;      }
        uint16_t readBgCnt(int bg)  { return bgCnt[bg];    }
        uint16_t readWinIn()        { return winIn;        }
        uint16_t readWinOut()       { return winOut;       }
        uint16_t readBldCnt()       { return bldCnt;       }
        uint16_t readBldAlpha()     { return bldAlpha;     }
        uint16_t readMasterBright() { return masterBright; }

        void writeDispCnt(uint32_t mask, uint32_t value);
        void writeBgCnt(int bg, uint16_t mask, uint16_t value);
        void writeBgHOfs(int bg, uint16_t mask, uint16_t value);
        void writeBgVOfs(int bg, uint16_t mask, uint16_t value);
        void writeBgPA(int bg, uint16_t mask, uint16_t value);
        void writeBgPB(int bg, uint16_t mask, uint16_t value);
        void writeBgPC(int bg, uint16_t mask, uint16_t value);
        void writeBgPD(int bg, uint16_t mask, uint16_t value);
        void writeBgX(int bg, uint32_t mask, uint32_t value);
        void writeBgY(int bg, uint32_t mask, uint32_t value);
        void writeWinH(int win, uint16_t mask, uint16_t value);
        void writeWinV(int win, uint16_t mask, uint16_t value);
        void writeWinIn(uint16_t mask, uint16_t value);
        void writeWinOut(uint16_t mask, uint16_t value);
        void writeMosaic(uint16_t mask, uint16_t value);
        void writeBldCnt(uint16_t mask, uint16_t value);
        void writeBldAlpha(uint16_t mask, uint16_t value);
        void writeBldY(uint8_t value);
        void writeMasterBright(uint16_t mask, uint16_t value);

    private:
        Core *core;
        bool engine;

        uint32_t bgVramAddr, objVramAddr;
        uint8_t *palette, *oam;
        uint8_t **extPalettes;

        uint32_t framebuffer[256 * 192] = {};
        uint32_t layers[2][256] = {};
        int8_t priorities[2][256] = {};
        int8_t blendBits[2][256] = {};

        int internalX[2] = {};
        int internalY[2] = {};
        bool winHFlip[2] = {};
        bool winVFlip[2] = {};

        uint32_t dispCnt = 0;
        uint16_t bgCnt[4] = {};
        uint16_t bgHOfs[4] = {};
        uint16_t bgVOfs[4] = {};
        int16_t bgPA[2] = {};
        int16_t bgPB[2] = {};
        int16_t bgPC[2] = {};
        int16_t bgPD[2] = {};
        int32_t bgX[2] = {};
        int32_t bgY[2] = {};
        uint16_t winX1[2] = {};
        uint16_t winX2[2] = {};
        uint16_t winY1[2] = {};
        uint16_t winY2[2] = {};
        uint16_t winIn = 0;
        uint16_t winOut = 0;
        uint16_t bldCnt = 0;
        uint16_t mosaic = 0;
        uint16_t bldAlpha = 0;
        uint8_t bldY = 0;
        uint16_t masterBright = 0;

        static uint32_t rgb5ToRgb6(uint32_t color);

        void drawBgPixel(int bg, int line, int x, uint32_t pixel);
        void drawObjPixel(int line, int x, uint32_t pixel, int8_t priority);

        template <bool gbaMode> void drawText(int bg, int line);
        template <bool gbaMode> void drawAffine(int bg, int line);
        void drawExtended(int bg, int line);
        void drawExtendedGba(int bg, int line);
        void drawLarge(int bg, int line);
        template <bool gbaMode> void drawObjects(int line, bool window);
};

#endif // GPU_2D_H
