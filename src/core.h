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

#ifndef CORE_H
#define CORE_H

#include <chrono>
#include <cstdint>
#include <string>

#include "cartridge.h"
#include "cp15.h"
#include "dma.h"
#include "gpu.h"
#include "gpu_2d.h"
#include "gpu_3d.h"
#include "gpu_3d_renderer.h"
#include "input.h"
#include "interpreter.h"
#include "ipc.h"
#include "math.h"
#include "memory.h"
#include "rtc.h"
#include "spi.h"
#include "spu.h"
#include "timers.h"
#include "wifi.h"

class Core
{
    public:
        Core(std::string filename = "");
        ~Core();

        static void createSave(std::string filename, int type);

        void runFrame();

        void pressKey(int key)         { input.pressKey(key);                       }
        void releaseKey(int key)       { input.releaseKey(key);                     }
        void pressScreen(int x, int y) { input.pressScreen();   spi.setTouch(x, y); }
        void releaseScreen()           { input.releaseScreen(); spi.clearTouch();   }

        uint32_t *getFramebuffer()      { return gpu.getFramebuffer();  }
        uint32_t *getSamples(int count) { return spu.getSamples(count); }
        int       getFps()              { return fps;                   }

    private:
        uint8_t bios9[0x8000];
        uint8_t bios7[0x4000];
        uint8_t firmware[0x40000];
        uint8_t gbaBios[0x4000];

        uint8_t *rom, *save;
        uint32_t saveSize = 0;
        std::string saveName;

        uint8_t *gbaRom, *gbaSave;
        uint32_t gbaSaveSize = 0;
        std::string gbaSaveName;

        int fps = 0, fpsCount = 0;
        std::chrono::steady_clock::time_point lastFpsTime;
        int spuTimer = 0;

        Cartridge cart9, cart7;
        Cp15 cp15;
        Dma dma9, dma7;
        Gpu gpu;
        Gpu2D engineA, engineB;
        Gpu3D gpu3D;
        Gpu3DRenderer gpu3DRenderer;
        Input input;
        Interpreter arm9, arm7;
        Ipc ipc;
        Math math;
        Memory memory;
        Rtc rtc;
        Spi spi;
        Spu spu;
        Timers timers9, timers7;
        Wifi wifi;
};

#endif // CORE_H
