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

#include <cstring>
#include <thread>

#include "core.h"
#include "settings.h"

Core::Core(std::string filename, bool gba): cartridge(this), cp15(this), dma { Dma(this, 0), Dma(this, 1) }, gpu(this), gpu2D { Gpu2D(this, 0), Gpu2D(this, 1) },
                                            gpu3D(this), gpu3DRenderer(this), input(this), interpreter { Interpreter(this, 0), Interpreter(this, 1) }, ipc(this),
                                            math(this), memory(this), rtc(this), spi(this), spu(this), timers { Timers(this, 0), Timers(this, 1) }, wifi(this)
{
    // Load the NDS BIOS and firmware if not directly booting a GBA ROM
    if (filename == "" || !gba || !Settings::getDirectBoot())
    {
        memory.loadBios();
        spi.loadFirmware();
    }

    // Skip ROM loading if one wasn't specified
    if (filename == "") return;

    if (gba) // GBA
    {
        // Load the GBA BIOS and ROM
        memory.loadGbaBios();
        cartridge.loadGbaRom(filename);

        // Enable GBA mode right away if direct boot is enabled
        if (Settings::getDirectBoot())
        {
            memory.write<uint16_t>(0, 0x4000304, 0x8003); // POWCNT1
            enterGbaMode();
        }
    }
    else // NDS
    {
        // Load the NDS ROM
        cartridge.loadRom(filename);

        // Prepare to boot the NDS ROM directly if direct boot is enabled
        if (Settings::getDirectBoot())
        {
            // Set some registers as the BIOS/firmware would
            cp15.write(9, 1, 0, 0x027C0005); // Data TCM base/size
            cp15.write(9, 1, 1, 0x00000010); // Instruction TCM size
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
}

void Core::createSave(std::string filename, int type)
{
    // Determine the save size
    int saveSize;
    switch (type)
    {
        case 0:  saveSize =    0x200; break; // EEPROM 0.5KB
        case 1:  saveSize =   0x2000; break; // EEPROM   8KB
        case 2:  saveSize =   0x8000; break; // FRAM    32KB
        case 3:  saveSize =  0x10000; break; // EEPROM  64KB
        case 4:  saveSize =  0x40000; break; // FLASH  256KB
        case 5:  saveSize =  0x80000; break; // FLASH  512KB
        case 6:  saveSize = 0x100000; break; // FLASH 1024KB
        default: saveSize = 0x800000; break; // FLASH 8192KB
    }

    // Create an empty save
    uint8_t *save = new uint8_t[saveSize];
    memset(save, 0xFF, saveSize * sizeof(uint8_t));

    // Write the save to a file
    std::string saveName = filename.substr(0, filename.rfind(".")) + ".sav";
    FILE *saveFile = fopen(saveName.c_str(), "wb");
    if (saveFile)
    {
        fwrite(save, sizeof(uint8_t), saveSize, saveFile);
        fclose(saveFile);
    }

    delete[] save;
}

void Core::runGbaFrame()
{
    // Run a frame in GBA mode
    for (int i = 0; i < 228; i++) // 228 scanlines
    {
        for (int j = 0; j < 308 * 2; j++) // 308 dots per scanline
        {
            // Run the ARM7
            if (interpreter[1].shouldInterrupt()) interpreter[1].interrupt();
            if (interpreter[1].shouldRun())       interpreter[1].runCycle();
            if (dma[1].shouldTransfer())          dma[1].transfer();
            if (timers[1].shouldTick())           timers[1].tick(true);

            // Run the SPU every 256 cycles
            if (++spuTimer >= 256)
            {
                spu.runGbaSample();
                spuTimer = 0;
            }

            // The end of the visible scanline
            if (j == 240 * 2) gpu.gbaScanline240();
        }

        // The end of the scanline
        gpu.gbaScanline308();
    }

    // Count the FPS
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
    for (int i = 0; i < 263; i++) // 263 scanlines
    {
        for (int j = 0; j < 355 * 3; j++) // 355 dots per scanline
        {
            // Run the ARM9 at twice the speed of the ARM7
            for (int k = 0; k < 2; k++)
            {
                if (interpreter[0].shouldInterrupt()) interpreter[0].interrupt();
                if (interpreter[0].shouldRun())       interpreter[0].runCycle();
                if (dma[0].shouldTransfer())          dma[0].transfer();
                if (timers[0].shouldTick())           timers[0].tick(false);
            }

            // Run the ARM7
            if (interpreter[1].shouldInterrupt()) interpreter[1].interrupt();
            if (interpreter[1].shouldRun())       interpreter[1].runCycle();
            if (dma[1].shouldTransfer())          dma[1].transfer();
            if (timers[1].shouldTick())           timers[1].tick(true);
    
            // Run the 3D engine
            if (gpu3D.shouldRun()) gpu3D.runCycle();

            // Run the SPU every 512 cycles
            if (++spuTimer >= 512)
            {
                spu.runSample();
                spuTimer = 0;
            }

            // The end of the visible scanline
            if (j == 256 * 3) gpu.scanline256();
        }

        // The end of the scanline
        gpu.scanline355();
    }

    // Count the FPS
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

void Core::enterGbaMode()
{
    // Switch to GBA mode if enabled
    interpreter[1].enterGbaMode();
    runFunc = &Core::runGbaFrame;
    gbaMode = true;
}
