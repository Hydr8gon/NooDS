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

#ifndef CORE_H
#define CORE_H

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "bios.h"
#include "cartridge.h"
#include "cp15.h"
#include "defines.h"
#include "div_sqrt.h"
#include "dldi.h"
#include "dma.h"
#include "gpu.h"
#include "gpu_2d.h"
#include "gpu_3d.h"
#include "gpu_3d_renderer.h"
#include "input.h"
#include "interpreter.h"
#include "ipc.h"
#include "memory.h"
#include "rtc.h"
#include "spi.h"
#include "spu.h"
#include "timers.h"
#include "wifi.h"

enum CoreError
{
    ERROR_BIOS = 1,
    ERROR_FIRM,
    ERROR_ROM
};

struct Task
{
    std::function<void()> *task;
    uint32_t cycles;

    Task(std::function<void()> *task, uint32_t cycles):
        task(task), cycles(cycles) {}

    bool operator<(const Task &task) const
        { return cycles < task.cycles; }
};

class Core
{
    public:
        int id = 0;
        bool gbaMode = false;
        int fps = 0;

        Bios9 bios9;
        Bios7 bios7;
        CartridgeNds cartridgeNds;
        CartridgeGba cartridgeGba;
        Cp15 cp15;
        DivSqrt divSqrt;
        Dldi dldi;
        Dma dma[2];
        Gpu gpu;
        Gpu2D gpu2D[2];
        Gpu3D gpu3D;
        Gpu3DRenderer gpu3DRenderer;
        Input input;
        Interpreter interpreter[2];
        Ipc ipc;
        Memory memory;
        Rtc rtc;
        Spi spi;
        Spu spu;
        Timers timers[2];
        Wifi wifi;

        std::atomic<bool> running;
        std::vector<Task> tasks;
        uint32_t globalCycles = 0;

        Core(std::string ndsPath = "", std::string gbaPath = "", int id = 0);

        void runFrame() { (*runFunc)(*this); }

        void schedule(Task task);
        void enterGbaMode();
        void endFrame();

    private:
        void (*runFunc)(Core&) = &Interpreter::runNdsFrame;
        std::chrono::steady_clock::time_point lastFpsTime;
        int fpsCount = 0;

        std::function<void()> resetCyclesTask;

        void resetCycles();
};

#endif // CORE_H
