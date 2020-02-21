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

#ifndef GPU_H
#define GPU_H

#include <cstdint>

class Dma;
class Gpu2D;
class Gpu3D;
class Gpu3DRenderer;
class Interpreter;
class Memory;

class Gpu
{
    public:
        Gpu(Dma *dma9, Dma *dma7, Gpu2D *engineA, Gpu2D *engineB, Gpu3D *gpu3D,
            Gpu3DRenderer *gpu3DRenderer, Interpreter *arm9, Interpreter *arm7, Memory *memory):
            dma9(dma9), dma7(dma7), engineA(engineA), engineB(engineB), gpu3D(gpu3D),
            gpu3DRenderer(gpu3DRenderer), arm9(arm9), arm7(arm7), memory(memory) {}

        void scanline256();
        void scanline355();

        uint32_t *getFramebuffer() { return framebuffer; }

        uint16_t readDispStat9()  { return dispStat9;  }
        uint16_t readDispStat7()  { return dispStat7;  }
        uint16_t readVCount()     { return vCount;     }
        uint32_t readDispCapCnt() { return dispCapCnt; }
        uint16_t readPowCnt1()    { return powCnt1;    }

        void writeDispStat9(uint16_t mask, uint16_t value);
        void writeDispStat7(uint16_t mask, uint16_t value);
        void writeDispCapCnt(uint32_t mask, uint32_t value);
        void writePowCnt1(uint16_t mask, uint16_t value);

    private:
        uint32_t framebuffer[256 * 192 * 2] = {};

        bool displayCapture = false;

        uint16_t dispStat9 = 0, dispStat7 = 0;
        uint16_t vCount = 0;
        uint32_t dispCapCnt = 0;
        uint16_t powCnt1 = 0;

        Dma *dma9, *dma7;
        Gpu2D *engineA, *engineB;
        Gpu3D *gpu3D;
        Gpu3DRenderer *gpu3DRenderer;
        Interpreter *arm9, *arm7;
        Memory *memory;

        uint16_t rgb6ToRgb5(uint32_t color);
};

#endif // GPU_H
