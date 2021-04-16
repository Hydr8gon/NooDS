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

#include <algorithm>
#include <cstring>
#include <thread>

#include "core.h"
#include "settings.h"

Core::Core(std::string ndsPath, std::string gbaPath):
    cartridge(this), cp15(this), divSqrt(this), dldi(this), dma { Dma(this, 0), Dma(this, 1) }, gpu(this), gpu2D { Gpu2D(this, 0),
    Gpu2D(this, 1) }, gpu3D(this), gpu3DRenderer(this), input(this), interpreter { Interpreter(this, 0), Interpreter(this, 1) },
    ipc(this), memory(this), rtc(this), spi(this), spu(this), timers { Timers(this, 0), Timers(this, 1) }, wifi(this)
{
    // Schedule initial tasks for NDS mode
    resetCyclesTask = std::bind(&Core::resetCycles, this);
    schedule(Task(&resetCyclesTask, 0x7FFFFFFF));
    gpu.scheduleInit();
    spu.scheduleInit();

    // Load the NDS BIOS and firmware unless directly booting a GBA ROM
    if (ndsPath != "" || gbaPath == "" || !Settings::getDirectBoot())
    {
        memory.loadBios();
        spi.loadFirmware();
    }

    if (ndsPath != "")
    {
        // Load an NDS ROM
        cartridge.loadNdsRom(ndsPath);

        // Prepare to boot the NDS ROM directly if direct boot is enabled
        if (Settings::getDirectBoot())
        {
            // Set some registers as the BIOS/firmware would
            cp15.write(1, 0, 0, 0x0005707D); // CP15 Control
            cp15.write(9, 1, 0, 0x0300000A); // Data TCM base/size
            cp15.write(9, 1, 1, 0x00000020); // Instruction TCM size
            memory.write<uint8_t>(0,  0x4000247,   0x03); // WRAMCNT
            memory.write<uint8_t>(0,  0x4000300,   0x01); // POSTFLG (ARM9)
            memory.write<uint8_t>(1,  0x4000300,   0x01); // POSTFLG (ARM7)
            memory.write<uint16_t>(0, 0x4000304, 0x0001); // POWCNT1
            memory.write<uint16_t>(1, 0x4000504, 0x0200); // SOUNDBIAS

            // Set some memory values as the BIOS/firmware would
            memory.write<uint32_t>(0, 0x27FF800, 0x00001FC2); // Chip ID 1
            memory.write<uint32_t>(0, 0x27FF804, 0x00001FC2); // Chip ID 2
            memory.write<uint16_t>(0, 0x27FF850,     0x5835); // ARM7 BIOS CRC
            memory.write<uint16_t>(0, 0x27FF880,     0x0007); // Message from ARM9 to ARM7
            memory.write<uint16_t>(0, 0x27FF884,     0x0006); // ARM7 boot task
            memory.write<uint32_t>(0, 0x27FFC00, 0x00001FC2); // Copy of chip ID 1
            memory.write<uint32_t>(0, 0x27FFC04, 0x00001FC2); // Copy of chip ID 2
            memory.write<uint16_t>(0, 0x27FFC10,     0x5835); // Copy of ARM7 BIOS CRC
            memory.write<uint16_t>(0, 0x27FFC40,     0x0001); // Boot indicator

            cartridge.directBoot();
            interpreter[0].directBoot();
            interpreter[1].directBoot();
            spi.directBoot();
        }
    }

    if (gbaPath != "")
    {
        // Load the GBA BIOS unless directly booting an NDS ROM
        if (ndsPath == "" || !Settings::getDirectBoot())
            memory.loadGbaBios();

        // Load a GBA ROM
        cartridge.loadGbaRom(gbaPath);

        // Enable GBA mode right away if direct boot is enabled
        if (ndsPath == "" && Settings::getDirectBoot())
        {
            memory.write<uint16_t>(0, 0x4000304, 0x8003); // POWCNT1
            enterGbaMode();
        }
    }
}

void Core::resetCycles()
{
    // Reset the global cycle count periodically to prevent overflow
    for (unsigned int i = 0; i < tasks.size(); i++)
        tasks[i].cycles -= globalCycles;
    timers[0].resetCycles();
    timers[1].resetCycles();
    globalCycles -= globalCycles;
    schedule(Task(&resetCyclesTask, 0x7FFFFFFF));
}

void Core::runGbaFrame()
{
    // Run a frame in GBA mode
    while (++frameCycles < 228 * 308 * 4) // 228 scanlines, 308 dots, 4 ARM7 cycles
    {
        // Run the ARM7
        if ((frameCycles & 1) && interpreter[1].shouldRun())
            interpreter[1].runOpcode();

        if (interpreter[1].shouldRun())
        {
            // Count cycles normally
            globalCycles++;
        }
        else
        {
            // Jump to the next scheduled task
            frameCycles += tasks[0].cycles - globalCycles - 1;
            globalCycles = tasks[0].cycles;
        }

        // Run any tasks that are scheduled now
        while (tasks[0].cycles <= globalCycles)
        {
            (*tasks[0].task)();
            tasks.erase(tasks.begin());
        }
    }

    // Count a frame
    frameCycles -= 228 * 308 * 4;
    fpsCount++;

    // Update the FPS and reset the counter every second
    std::chrono::duration<double> fpsTime = std::chrono::steady_clock::now() - lastFpsTime;
    if (fpsTime.count() >= 1.0f)
    {
        fps = fpsCount;
        fpsCount = 0;
        lastFpsTime = std::chrono::steady_clock::now();
    }
}

void Core::runNdsFrame()
{
    // Run a frame in NDS mode
    while (++frameCycles < 263 * 355 * 6) // 263 scanlines, 355 dots, 6 ARM9 cycles
    {
        // Run the ARM9
        if (interpreter[0].shouldRun())
            interpreter[0].runOpcode();

        // Run the ARM7 at half the speed of the ARM9
        if ((frameCycles & 1) && interpreter[1].shouldRun())
            interpreter[1].runOpcode();

        if (interpreter[0].shouldRun() || interpreter[1].shouldRun())
        {
            // Count cycles normally
            globalCycles++;
        }
        else
        {
            // Jump to the next scheduled task
            frameCycles += tasks[0].cycles - globalCycles - 1;
            globalCycles = tasks[0].cycles;
        }

        // Run any tasks that are scheduled now
        while (tasks[0].cycles <= globalCycles)
        {
            (*tasks[0].task)();
            tasks.erase(tasks.begin());
        }
    }

    // Count a frame
    frameCycles -= 263 * 355 * 6;
    fpsCount++;

    // Update the FPS and reset the counter every second
    std::chrono::duration<double> fpsTime = std::chrono::steady_clock::now() - lastFpsTime;
    if (fpsTime.count() >= 1.0f)
    {
        fps = fpsCount;
        fpsCount = 0;
        lastFpsTime = std::chrono::steady_clock::now();
    }
}

void Core::schedule(Task task)
{
    // Add a task to the scheduler, sorted by least to most cycles until execution
    task.cycles += globalCycles;
    auto it = std::upper_bound(tasks.cbegin(), tasks.cend(), task);
    tasks.insert(it, task);
}

void Core::enterGbaMode()
{
    // Switch to GBA mode
    interpreter[1].enterGbaMode();
    runFunc = &Core::runGbaFrame;
    gbaMode = true;

    // Reset the scheduler and schedule initial tasks for GBA mode
    frameCycles = globalCycles = 0;
    tasks.clear();
    schedule(Task(&resetCyclesTask, 0x7FFFFFFF));
    gpu.gbaScheduleInit();
    spu.gbaScheduleInit();

    // Set VRAM blocks A and B to plain access mode
    // This is used by the GPU to access the VRAM borders
    memory.write<uint8_t>(0, 0x4000240, 0x80); // VRAMCNT_A
    memory.write<uint8_t>(0, 0x4000241, 0x80); // VRAMCNT_B
}
