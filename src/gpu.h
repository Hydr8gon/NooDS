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
#include <mutex>

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
            dmas { dma9, dma7 }, engineA(engineA), engineB(engineB), gpu3D(gpu3D),
            gpu3DRenderer(gpu3DRenderer), cpus { arm9, arm7 }, memory(memory) {}

        uint32_t *getFrame(bool gbaCrop);

        void gbaScanline240();
        void gbaScanline308();
        void scanline256();
        void scanline355();

        uint16_t readDispStat(bool cpu) { return dispStat[cpu]; }
        uint16_t readVCount()           { return vCount;        }
        uint32_t readDispCapCnt()       { return dispCapCnt;    }
        uint16_t readPowCnt1()          { return powCnt1;       }

        void writeDispStat(bool cpu, uint16_t mask, uint16_t value);
        void writeDispCapCnt(uint32_t mask, uint32_t value);
        void writePowCnt1(uint16_t mask, uint16_t value);

    private:
        uint32_t framebuffer[256 * 192 * 2] = {};
        bool ready = true;
        std::mutex mutex;

        bool displayCapture = false;

        uint16_t dispStat[2] = {};
        uint16_t vCount = 0;
        uint32_t dispCapCnt = 0;
        uint16_t powCnt1 = 0;

        Dma *dmas[2];
        Gpu2D *engineA, *engineB;
        Gpu3D *gpu3D;
        Gpu3DRenderer *gpu3DRenderer;
        Interpreter *cpus[2];
        Memory *memory;

        uint16_t rgb6ToRgb5(uint32_t color);
};

#endif // GPU_H
