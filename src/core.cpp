/*
    Copyright 2019-2024 Hydr8gon

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

Core::Core(std::string ndsRom, std::string gbaRom, std::string ndsSave, std::string gbaSave,
    int id, int ndsRomFd, int gbaRomFd, int ndsSaveFd, int gbaSaveFd):
    id(id), bios { Bios(this, 0, Bios::swiTable9), Bios(this, 1, Bios::swiTable7), Bios(this, 1, Bios::swiTableGba) },
    cartridgeNds(this), cartridgeGba(this), cp15(this), divSqrt(this), dldi(this), dma { Dma(this, 0),
    Dma(this, 1) }, gpu(this), gpu2D { Gpu2D(this, 0), Gpu2D(this, 1) }, gpu3D(this), gpu3DRenderer(this),
    input(this), interpreter { Interpreter(this, 0), Interpreter(this, 1) }, ipc(this), memory(this),
    rtc(this), spi(this), spu(this), timers { Timers(this, 0), Timers(this, 1) }, wifi(this)
{
    // Try to load BIOS and firmware; require DS files when not direct booting
    bool required = !Settings::directBoot || (ndsRom == "" && gbaRom == "" && ndsRomFd == -1 && gbaRomFd == -1);
    if (!memory.loadBios9() && required) throw ERROR_BIOS;
    if (!memory.loadBios7() && required) throw ERROR_BIOS;
    if (!spi.loadFirmware() && required) throw ERROR_FIRM;
    realGbaBios = memory.loadGbaBios();

    // Define the tasks that can be scheduled
    tasks[RESET_CYCLES] = std::bind(&Core::resetCycles, this);
    tasks[CART9_WORD_READY] = std::bind(&CartridgeNds::wordReady, &cartridgeNds, 0);
    tasks[CART7_WORD_READY] = std::bind(&CartridgeNds::wordReady, &cartridgeNds, 1);
    tasks[DMA9_TRANSFER0] = std::bind(&Dma::transfer, &dma[0], 0);
    tasks[DMA9_TRANSFER1] = std::bind(&Dma::transfer, &dma[0], 1);
    tasks[DMA9_TRANSFER2] = std::bind(&Dma::transfer, &dma[0], 2);
    tasks[DMA9_TRANSFER3] = std::bind(&Dma::transfer, &dma[0], 3);
    tasks[DMA7_TRANSFER0] = std::bind(&Dma::transfer, &dma[1], 0);
    tasks[DMA7_TRANSFER1] = std::bind(&Dma::transfer, &dma[1], 1);
    tasks[DMA7_TRANSFER2] = std::bind(&Dma::transfer, &dma[1], 2);
    tasks[DMA7_TRANSFER3] = std::bind(&Dma::transfer, &dma[1], 3);
    tasks[NDS_SCANLINE256] = std::bind(&Gpu::scanline256, &gpu);
    tasks[NDS_SCANLINE355] = std::bind(&Gpu::scanline355, &gpu);
    tasks[GBA_SCANLINE240] = std::bind(&Gpu::gbaScanline240, &gpu);
    tasks[GBA_SCANLINE308] = std::bind(&Gpu::gbaScanline308, &gpu);
    tasks[GPU3D_COMMAND] = std::bind(&Gpu3D::runCommand, &gpu3D);
    tasks[ARM9_INTERRUPT] = std::bind(&Interpreter::interrupt, &interpreter[0]);
    tasks[ARM7_INTERRUPT] = std::bind(&Interpreter::interrupt, &interpreter[1]);
    tasks[NDS_SPU_SAMPLE] = std::bind(&Spu::runSample, &spu);
    tasks[GBA_SPU_SAMPLE] = std::bind(&Spu::runGbaSample, &spu);
    tasks[TIMER9_OVERFLOW0] = std::bind(&Timers::overflow, &timers[0], 0);
    tasks[TIMER9_OVERFLOW1] = std::bind(&Timers::overflow, &timers[0], 1);
    tasks[TIMER9_OVERFLOW2] = std::bind(&Timers::overflow, &timers[0], 2);
    tasks[TIMER9_OVERFLOW3] = std::bind(&Timers::overflow, &timers[0], 3);
    tasks[TIMER7_OVERFLOW0] = std::bind(&Timers::overflow, &timers[1], 0);
    tasks[TIMER7_OVERFLOW1] = std::bind(&Timers::overflow, &timers[1], 1);
    tasks[TIMER7_OVERFLOW2] = std::bind(&Timers::overflow, &timers[1], 2);
    tasks[TIMER7_OVERFLOW3] = std::bind(&Timers::overflow, &timers[1], 3);
    tasks[WIFI_COUNT_MS] = std::bind(&Wifi::countMs, &wifi);

    // Schedule initial tasks for NDS mode
    schedule(RESET_CYCLES, 0x7FFFFFFF);
    schedule(NDS_SCANLINE256, 256 * 6);
    schedule(NDS_SCANLINE355, 355 * 6);
    schedule(NDS_SPU_SAMPLE, 512 * 2);

    // Initialize the memory and CPUs
    memory.updateMap9<false>(0x00000000, 0xFFFFFFFF);
    memory.updateMap7(0x00000000, 0xFFFFFFFF);
    interpreter[0].init();
    interpreter[1].init();

    if (gbaRom != "" || gbaRomFd != -1)
    {
        // Load a GBA ROM
        if (!cartridgeGba.setRom(gbaRom, gbaSave) && !cartridgeGba.setRom(gbaRomFd, gbaSaveFd))
            throw ERROR_ROM;

        // Enable GBA mode right away if direct boot is enabled
        if (Settings::directBoot && ndsRom == "" && ndsRomFd == -1)
        {
            memory.write<uint16_t>(0, 0x4000304, 0x8003); // POWCNT1
            enterGbaMode();
        }
    }

    if (ndsRom != "" || ndsRomFd != -1)
    {
        // Load an NDS ROM
        if (!cartridgeNds.setRom(ndsRom, ndsSave) && !cartridgeNds.setRom(ndsRomFd, ndsSaveFd))
            throw ERROR_ROM;

        // Prepare to boot the NDS ROM directly if direct boot is enabled
        if (Settings::directBoot)
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

            cartridgeNds.directBoot();
            interpreter[0].directBoot();
            interpreter[1].directBoot();
            spi.directBoot();
        }
    }

    // Let the core run
    running.store(true);
}

void Core::resetCycles()
{
    // Reset the global cycle count periodically to prevent overflow
    for (size_t i = 0; i < events.size(); i++)
        events[i].cycles -= globalCycles;
    for (int i = 0; i < 2; i++)
        interpreter[i].resetCycles(), timers[i].resetCycles();
    globalCycles -= globalCycles;
    schedule(RESET_CYCLES, 0x7FFFFFFF);
}

void Core::schedule(SchedTask task, uint32_t cycles)
{
    // Add a task to the scheduler, sorted by least to most cycles until execution
    SchedEvent event(&tasks[task], globalCycles + cycles);
    auto it = std::upper_bound(events.cbegin(), events.cend(), event);
    events.insert(it, event);
}

void Core::enterGbaMode()
{
    // Switch to GBA mode
    gbaMode = true;
    runFunc = &Interpreter::runGbaFrame;
    running.store(false);

    // Reset the scheduler and schedule initial tasks for GBA mode
    events.clear();
    schedule(RESET_CYCLES, 1);
    schedule(GBA_SCANLINE240, 240 * 4);
    schedule(GBA_SCANLINE308, 308 * 4);
    schedule(GBA_SPU_SAMPLE, 512);

    // Reset the system for GBA mode
    memory.updateMap7(0x00000000, 0xFFFFFFFF);
    interpreter[1].init();
    rtc.reset();

    // Set VRAM blocks A and B to plain access mode
    // This is used by the GPU to access the VRAM borders
    memory.write<uint8_t>(0, 0x4000240, 0x80); // VRAMCNT_A
    memory.write<uint8_t>(0, 0x4000241, 0x80); // VRAMCNT_B

    // Disable HLE BIOS if a real one was loaded
    if (realGbaBios)
        return interpreter[1].setBios(nullptr);

    // Enable HLE BIOS and boot the GBA ROM directly
    interpreter[1].setBios(&bios[2]);
    interpreter[1].directBoot();
    memory.write<uint16_t>(1, 0x4000088, 0x200); // SOUNDBIAS
}

void Core::endFrame()
{
    // Break execution at the end of a frame and count it
    running.store(false);
    fpsCount++;

    // Update the FPS and reset the counter every second
    std::chrono::duration<double> fpsTime = std::chrono::steady_clock::now() - lastFpsTime;
    if (fpsTime.count() >= 1.0f)
    {
        fps = fpsCount;
        fpsCount = 0;
        lastFpsTime = std::chrono::steady_clock::now();
    }

    // Schedule WiFi updates only when needed
    if (wifi.shouldSchedule())
        wifi.scheduleInit();
}
