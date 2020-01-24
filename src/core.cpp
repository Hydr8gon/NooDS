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

#include <exception>
#include <thread>

#include "core.h"
#include "defines.h"
#include "settings.h"

Core::Core(): cart9(&arm9, &memory), cart7(&arm7, &memory), cp15(&arm9), dma9(&cart9, &gpu3D, &arm9, &memory),
              dma7(&cart7, nullptr, &arm7, &memory), gpu(&engineA, &engineB, &gpu3D, &gpu3DRenderer, &arm9,
              &arm7, &memory), engineA(&gpu3DRenderer, &memory), engineB(&memory), gpu3D(&arm9), gpu3DRenderer(&gpu3D),
              arm9(&cp15, &memory), arm7(&memory), ipc(&arm9, &arm7), memory(&cart9, &cart7, &cp15, &dma9,
              &dma7, &gpu, &engineA, &engineB, &gpu3D, &gpu3DRenderer, &input, &arm9, &arm7, &ipc, &math, &rtc,
              &spi, &spu, &timers9, &timers7), spi(&arm7, firmware), spu(&memory), timers9(&arm9), timers7(&arm7)
{
    // Attempt to load the firmware
    FILE *firmwareFile = fopen(Settings::getFirmwarePath().c_str(), "rb");
    if (!firmwareFile) throw new std::exception;
    fread(firmware, sizeof(uint8_t), 0x40000, firmwareFile);
    fclose(firmwareFile);
}

Core::Core(std::string filename): Core()
{
    // Attempt to load a ROM
    FILE *romFile = fopen(filename.c_str(), "rb");
    if (!romFile) throw new std::exception;
    fseek(romFile, 0, SEEK_END);
    uint32_t romSize = ftell(romFile);
    fseek(romFile, 0, SEEK_SET);
    rom = new uint8_t[romSize];
    fread(rom, sizeof(uint8_t), romSize, romFile);
    fclose(romFile);

    // Attempt to load a save file
    saveName = filename.substr(0, filename.rfind(".")) + ".sav";
    FILE *saveFile = fopen(saveName.c_str(), "rb");
    if (saveFile)
    {
        fseek(saveFile, 0, SEEK_END);
        saveSize = ftell(saveFile);
        fseek(saveFile, 0, SEEK_SET);
        save = new uint8_t[saveSize];
        fread(save, sizeof(uint8_t), saveSize, saveFile);
        fclose(saveFile);
    }

    // "Insert" the cartridge
    cart9.setRom(rom, romSize, save, saveSize);
    cart7.setRom(rom, romSize, save, saveSize);

    // Don't run the direct boot setup if it's disabled
    // Games will boot from the firmware in this case
    if (!Settings::getDirectBoot()) return;

    // Set some registers as the BIOS/firmware would
    memory.write<uint8_t>(true,   0x4000247,   0x03); // WRAMCNT
    memory.write<uint8_t>(true,   0x4000300,   0x01); // POSTFLG (ARM9)
    memory.write<uint8_t>(false,  0x4000300,   0x01); // POSTFLG (ARM7)
    memory.write<uint16_t>(true,  0x4000304, 0x0001); // POWCNT1
    memory.write<uint16_t>(false, 0x4000504, 0x0200); // SOUNDBIAS
    cp15.write(9, 1, 0, 0x027C0005); // Data TCM base/size
    cp15.write(9, 1, 1, 0x00000010); // Instruction TCM size

    // Extract some information about the initial ARM9 code from the header
    uint32_t offset9    = U8TO32(rom, 0x20);
    uint32_t entryAddr9 = U8TO32(rom, 0x24);
    uint32_t ramAddr9   = U8TO32(rom, 0x28);
    uint32_t size9      = U8TO32(rom, 0x2C);
    printf("ARM9 code ROM offset:    0x%X\n", offset9);
    printf("ARM9 code entry address: 0x%X\n", entryAddr9);
    printf("ARM9 RAM address:        0x%X\n", ramAddr9);
    printf("ARM9 code size:          0x%X\n", size9);

    // Extract some information about the initial ARM7 code from the header
    uint32_t offset7    = U8TO32(rom, 0x30);
    uint32_t entryAddr7 = U8TO32(rom, 0x34);
    uint32_t ramAddr7   = U8TO32(rom, 0x38);
    uint32_t size7      = U8TO32(rom, 0x3C);
    printf("ARM7 code ROM offset:    0x%X\n", offset7);
    printf("ARM7 code entry address: 0x%X\n", entryAddr7);
    printf("ARM7 RAM address:        0x%X\n", ramAddr7);
    printf("ARM7 code size:          0x%X\n", size7);

    // Load the ROM header into memory
    for (uint32_t i = 0; i < 0x170; i++)
        memory.write<uint8_t>(true, 0x27FFE00 + i, rom[i]);

    // Load the initial ARM9 code into memory
    for (uint32_t i = 0; i < size9; i++)
        memory.write<uint8_t>(true, ramAddr9 + i, rom[offset9 + i]);

    // Load the initial ARM7 code into memory
    for (uint32_t i = 0; i < size7; i++)
        memory.write<uint8_t>(false, ramAddr7 + i, rom[offset7 + i]);

    // Load the user settings into memory
    for (uint32_t i = 0; i < 0x70; i++)
        memory.write<uint8_t>(true, 0x27FFC80 + i, firmware[0x3FF00 + i]);

    // Set some memory values as the BIOS/firmware would
    memory.write<uint32_t>(true, 0x27FF800, 0x00001FC2); // Chip ID 1
    memory.write<uint32_t>(true, 0x27FF804, 0x00001FC2); // Chip ID 2
    memory.write<uint16_t>(true, 0x27FF850,     0x5835); // ARM7 BIOS CRC
    memory.write<uint16_t>(true, 0x27FF880,     0x0007); // Message from ARM9 to ARM7
    memory.write<uint16_t>(true, 0x27FF884,     0x0006); // ARM7 boot task
    memory.write<uint32_t>(true, 0x27FFC00, 0x00001FC2); // Copy of chip ID 1
    memory.write<uint32_t>(true, 0x27FFC04, 0x00001FC2); // Copy of chip ID 2
    memory.write<uint16_t>(true, 0x27FFC10,     0x5835); // Copy of ARM7 BIOS CRC
    memory.write<uint16_t>(true, 0x27FFC40,     0x0001); // Boot indicator

    arm9.directBoot(entryAddr9);
    arm7.directBoot(entryAddr7);
}

Core::~Core()
{
    // Update the save file
    if (saveSize != 0)
    {
        FILE *saveFile = fopen(saveName.c_str(), "wb");
        if (saveFile)
        {
            fwrite(save, sizeof(uint8_t), saveSize, saveFile);
            fclose(saveFile);
        }
    }

    if (rom)  delete[] rom;
    if (save) delete[] save;
}

void Core::runFrame()
{
    // Run a frame
    for (int i = 0; i < 262; i++) // 262 scanlines
    {
        for (int j = 0; j < 355 * 3; j++) // 355 dots per scanline, 3 ARM7 cycles each
        {
            for (int k = 0; k < 2; k++)
            {
                // Run the ARM9 at twice the speed of the ARM7
                if (arm9.shouldInterrupt()) arm9.interrupt();
                if (arm9.shouldRun())       arm9.runCycle();
                if (dma9.shouldTransfer())  dma9.transfer();
                if (gpu3D.shouldRun())      gpu3D.runCycle();

                // Run both the ARM9 and ARM7 timers at ARM9 speed
                if (timers9.shouldTick())   timers9.tick();
                if (timers7.shouldTick())   timers7.tick();
            }

            // Run the ARM7
            if (arm7.shouldInterrupt()) arm7.interrupt();
            if (arm7.shouldRun())       arm7.runCycle();
            if (dma7.shouldTransfer())  dma7.transfer();

            // The end of the visible scanline
            if (j == 256 * 3)
                gpu.scanline256();
        }

        // The end of the scanline
        gpu.scanline355();
    }

    // Limit the FPS to 60
    std::chrono::duration<double> frameTime = std::chrono::steady_clock::now() - lastFrameTime;
#ifdef _WIN32
    // Sleeping on Windows is unreliable; a while loop is wasteful and not entirely accurate either, but it works
    while (Settings::getLimitFps() && frameTime.count() < 1.0f / 60)
        frameTime = std::chrono::steady_clock::now() - lastFrameTime;
#else
    if (Settings::getLimitFps() && frameTime.count() < 1.0f / 60)
        std::this_thread::sleep_for(std::chrono::microseconds((int)((1.0f / 60 - frameTime.count()) * 1000000)));
#endif
    lastFrameTime = std::chrono::steady_clock::now();

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
