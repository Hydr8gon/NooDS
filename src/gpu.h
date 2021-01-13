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

#ifndef GPU_H
#define GPU_H

#include <atomic>
#include <cstdint>
#include <thread>
#include <mutex>

#include "defines.h"

class Core;

class Gpu
{
    public:
        Gpu(Core *core);
        ~Gpu();

        uint32_t *getFrame(bool gbaCrop);

        void gbaScanline240();
        void gbaScanline308();
        void scanline256();
        void scanline355();

        void invalidate3D() { dirty3D |= BIT(0); }

        uint16_t readDispStat(bool cpu) { return dispStat[cpu]; }
        uint16_t readVCount()           { return vCount;        }
        uint32_t readDispCapCnt()       { return dispCapCnt;    }
        uint16_t readPowCnt1()          { return powCnt1;       }

        void writeDispStat(bool cpu, uint16_t mask, uint16_t value);
        void writeDispCapCnt(uint32_t mask, uint32_t value);
        void writePowCnt1(uint16_t mask, uint16_t value);

    private:
        Core *core;

        uint32_t framebuffer[256 * 192 * 2] = {};
        bool ready = true;
        std::mutex mutex;

        bool running = false;
        std::atomic<bool> drawing;
        std::thread *thread = nullptr;

        bool displayCapture = false;
        uint8_t dirty3D = 0;

        uint16_t dispStat[2] = {};
        uint16_t vCount = 0;
        uint32_t dispCapCnt = 0;
        uint16_t powCnt1 = 0;

        static uint32_t rgb6ToRgb8(uint32_t color);
        static uint16_t rgb6ToRgb5(uint32_t color);

        void drawGbaThreaded();
        void drawThreaded();
};

#endif // GPU_H
