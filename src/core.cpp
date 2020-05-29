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

#include <thread>

#include "core.h"
#include "defines.h"
#include "settings.h"

Core::Core(std::string filename): cart9(&dma9, &arm9, &memory), cart7(&dma7, &arm7, &memory), cp15(&arm9), dma9(true, &arm9, &memory),
                                  dma7(false, &arm7, &memory), gpu(&dma9, &dma7, &engineA, &engineB, &gpu3D, &gpu3DRenderer, &arm9, &arm7,
                                  &memory), engineA(&gpu3DRenderer, &memory), engineB(&memory), gpu3D(&dma9, &arm9), gpu3DRenderer(&gpu3D),
                                  arm9(&cp15, &memory), arm7(&memory), ipc(&arm9, &arm7), memory(bios9, bios7, gbaBios, &cart9, &cart7, &cp15,
                                  &dma9, &dma7, &gpu, &engineA, &engineB, &gpu3D, &gpu3DRenderer, &input, &arm9, &arm7, &ipc, &math, &rtc, &spi,
                                  &spu, &timers9, &timers7, &wifi), spi(&arm7, firmware), spu(&dma7, &memory), timers9(&arm9), timers7(&arm7, &spu)
{
    // Check the ROM type
    bool gba = (filename.length() > 0 && filename.substr(filename.rfind(".")) == ".gba");

    // Load the NDS BIOS and firmware if needed
    // They aren't needed if direct booting a GBA ROM
    if (!gba || !Settings::getDirectBoot())
    {
        // Attempt to load the ARM9 BIOS
        FILE *bios9File = fopen(Settings::getBios9Path().c_str(), "rb");
        if (!bios9File) throw 1;
        fread(bios9, sizeof(uint8_t), 0x1000, bios9File);
        fclose(bios9File);

        // Attempt to load the ARM7 BIOS
        FILE *bios7File = fopen(Settings::getBios7Path().c_str(), "rb");
        if (!bios7File) throw 1;
        fread(bios7, sizeof(uint8_t), 0x4000, bios7File);
        fclose(bios7File);

        // Attempt to load the firmware
        FILE *firmwareFile = fopen(Settings::getFirmwarePath().c_str(), "rb");
        if (!firmwareFile) throw 1;
        fread(firmware, sizeof(uint8_t), 0x40000, firmwareFile);
        fclose(firmwareFile);
    }

    // Skip ROM loading if one wasn't specified
    if (filename == "") return;

    if (gba) // GBA ROM
    {
        // Attempt to load the GBA BIOS
        FILE *gbaBiosFile = fopen(Settings::getGbaBiosPath().c_str(), "rb");
        if (!gbaBiosFile) throw 1;
        fread(gbaBios, sizeof(uint8_t), 0x4000, gbaBiosFile);
        fclose(gbaBiosFile);

        // Attempt to load a GBA ROM
        FILE *gbaRomFile = fopen(filename.c_str(), "rb");
        if (!gbaRomFile) throw 2;
        fseek(gbaRomFile, 0, SEEK_END);
        uint32_t gbaRomSize = ftell(gbaRomFile);
        fseek(gbaRomFile, 0, SEEK_SET);
        gbaRom = new uint8_t[gbaRomSize];
        fread(gbaRom, sizeof(uint8_t), gbaRomSize, gbaRomFile);
        fclose(gbaRomFile);

        // "Insert" the cartridge
        memory.setGbaRom(gbaRom, gbaRomSize);

        // Enable GBA mode right away if direct boot is enabled
        if (Settings::getDirectBoot())
        {
            memory.write<uint8_t>(false, 0x4000301,   0x40); // HALTCNT
            memory.write<uint16_t>(true, 0x4000304, 0x8003); // POWCNT1
        }
    }
    else // NDS ROM
    {
        // Attempt to load an NDS ROM
        FILE *romFile = fopen(filename.c_str(), "rb");
        if (!romFile) throw 2;
        fseek(romFile, 0, SEEK_END);
        uint32_t romSize = ftell(romFile);
        fseek(romFile, 0, SEEK_SET);
        rom = new uint8_t[romSize];
        fread(rom, sizeof(uint8_t), romSize, romFile);
        fclose(romFile);

        // Attempt to load the ROM's save file
        saveName = filename.substr(0, filename.rfind(".")) + ".sav";
        FILE *saveFile = fopen(saveName.c_str(), "rb");
        if (!saveFile) throw 3;
        fseek(saveFile, 0, SEEK_END);
        saveSize = ftell(saveFile);
        fseek(saveFile, 0, SEEK_SET);
        save = new uint8_t[saveSize];
        fread(save, sizeof(uint8_t), saveSize, saveFile);
        fclose(saveFile);

        // "Insert" the cartridge
        cart9.setRom(rom, romSize, save, saveSize);
        cart7.setRom(rom, romSize, save, saveSize);

        // Skip the direct boot setup if it's disabled
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
    for (int i = 0; i < saveSize; i++)
        save[i] = 0;

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

void Core::runFrame()
{
    if (memory.isGbaMode())
    {
        // Run a frame in GBA mode
        for (int i = 0; i < 228; i++) // 263 scanlines
        {
            for (int j = 0; j < 308 * 2; j++) // 308 dots per scanline
            {
                // Run the ARM7
                if (arm7.shouldInterrupt()) arm7.interrupt();
                if (arm7.shouldRun())       arm7.runCycle();
                if (dma7.shouldTransfer())  dma7.transfer();
                if (timers7.shouldTick())   timers7.tick(true);

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
    }
    else
    {
        // Run a frame in NDS mode
        for (int i = 0; i < 263; i++) // 263 scanlines
        {
            for (int j = 0; j < 355 * 3; j++) // 355 dots per scanline
            {
                // Run the ARM9 at twice the speed of the ARM7
                for (int k = 0; k < 2; k++)
                {
                    if (arm9.shouldInterrupt()) arm9.interrupt();
                    if (arm9.shouldRun())       arm9.runCycle();
                    if (dma9.shouldTransfer())  dma9.transfer();
                    if (timers9.shouldTick())   timers9.tick(false);
                }

                // Run the ARM7
                if (arm7.shouldInterrupt()) arm7.interrupt();
                if (arm7.shouldRun())       arm7.runCycle();
                if (dma7.shouldTransfer())  dma7.transfer();
                if (timers7.shouldTick())   timers7.tick(true);

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
