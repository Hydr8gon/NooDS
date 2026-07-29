/*
    Copyright 2019-2026 Hydr8gon

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

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "defines.h"
#include "save_states.h"
#include "settings.h"
#include "arm/cp15.h"
#include "arm/interpreter.h"
#include "arm/timers.h"
#include "gpu/gpu.h"
#include "gpu/gpu_2d.h"
#include "gpu/gpu_3d.h"
#include "gpu/gpu_3d_renderer.h"
#include "hle/action_replay.h"
#include "hle/dldi.h"
#include "hle/hle_arm7.h"
#include "hle/hle_bios.h"
#include "io/div_sqrt.h"
#include "io/input.h"
#include "io/ipc.h"
#include "io/rtc.h"
#include "io/spi.h"
#include "io/spu.h"
#include "io/wifi.h"
#include "memory/cartridge.h"
#include "memory/dma.h"
#include "memory/memory.h"

enum CoreError {
    ERROR_NDS_BIOS = 1,
    ERROR_NDS_FIRM,
    ERROR_DSI_BIOS,
    ERROR_DSI_NAND,
    ERROR_ROM
};

enum SchedTask {
    UPDATE_RUN,
    RESET_CYCLES,
    CART9_WORD_READY,
    CART7_WORD_READY,
    DMA9_TRANSFER0,
    DMA9_TRANSFER1,
    DMA9_TRANSFER2,
    DMA9_TRANSFER3,
    DMA7_TRANSFER0,
    DMA7_TRANSFER1,
    DMA7_TRANSFER2,
    DMA7_TRANSFER3,
    NDS_SCANLINE256,
    NDS_SCANLINE355,
    GBA_SCANLINE240,
    GBA_SCANLINE308,
    GPU3D_COMMANDS,
    ARM9_INTERRUPT,
    ARM7_INTERRUPT,
    NDS_SPU_SAMPLE,
    GBA_SPU_SAMPLE,
    TIMER9_OVERFLOW0,
    TIMER9_OVERFLOW1,
    TIMER9_OVERFLOW2,
    TIMER9_OVERFLOW3,
    TIMER7_OVERFLOW0,
    TIMER7_OVERFLOW1,
    TIMER7_OVERFLOW2,
    TIMER7_OVERFLOW3,
    WIFI_COUNT_MS,
    WIFI_TRANS_REPLY,
    WIFI_TRANS_ACK,
    MAX_TASKS
};

struct SchedEvent {
    SchedTask task;
    uint32_t cycles;

    SchedEvent(SchedTask task, uint32_t cycles): task(task), cycles(cycles) {}
    bool operator<(const SchedEvent &event) const { return cycles < event.cycles; }
};

class Core {
public:
    int id = 0;
    int fps = 0;
    bool arm7Hle = false;
    bool dsiMode = false;
    bool gbaMode = false;

    ActionReplay actionReplay;
    CartridgeGba cartridgeGba;
    CartridgeNds cartridgeNds;
    Cp15 cp15;
    DivSqrt divSqrt;
    Dldi dldi;
    Dma dma[2];
    Gpu gpu;
    Gpu2D gpu2D[2];
    Gpu3D gpu3D;
    Gpu3DRenderer gpu3DRenderer;
    HleArm7 hleArm7;
    HleBios hleBios[3];
    Input input;
    Interpreter interpreter[2];
    Ipc ipc;
    Memory memory;
    Rtc rtc;
    SaveStates saveStates;
    Spi spi;
    Spu spu;
    Timers timers[2];
    Wifi wifi;

    std::atomic<bool> running;
    std::vector<SchedEvent> events;
    std::function<void()> tasks[MAX_TASKS];
    uint32_t globalCycles = 0;

    Core(std::string ndsRom = "", std::string gbaRom = "", int id = 0, int ndsRomFd = -1, int gbaRomFd = -1,
        int ndsSaveFd = -1, int gbaSaveFd = -1, int ndsStateFd = -1, int gbaStateFd = -1, int ndsCheatFd = -1);
    void saveState(FILE *file);
    void loadState(FILE *file);

    void runCore() { (*runFunc)(*this); }
    void schedule(SchedTask task, uint32_t cycles);
    void enterGbaMode();
    void endFrame();

private:
    bool realGbaBios;
    void (*runFunc)(Core&) = &Interpreter::runCoreNds;
    std::chrono::steady_clock::time_point lastFpsTime;
    int fpsCount = 0;

    void updateRun();
    void resetCycles();
};
