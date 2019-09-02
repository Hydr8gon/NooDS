/*
    Copyright 2019 Hydr8gon

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

#include <cstdio>
#include <string>

#include "core.h"
#include "cartridge.h"
#include "cp15.h"
#include "dma.h"
#include "interpreter.h"
#include "fifo.h"
#include "gpu.h"
#include "memory.h"
#include "rtc.h"
#include "spi.h"
#include "timer.h"

namespace core
{

std::string saveName;

bool init()
{
    // Initialize everything
    cartridge::init();
    cp15::init();
    interpreter::init();
    fifo::init();
    memory::init();
    spi::init();
    rtc::init();

    // Load the ARM9 BIOS
    FILE *bios9File = fopen("bios9.bin", "rb");
    if (!bios9File) return false;
    fread(memory::bios9, sizeof(uint8_t), 0x1000, bios9File);
    fclose(bios9File);

    // Load the ARM7 BIOS
    FILE *bios7File = fopen("bios7.bin", "rb");
    if (!bios7File) return false;
    fread(memory::bios7, sizeof(uint8_t), 0x4000, bios7File);
    fclose(bios7File);

    // Load the firmware
    FILE *firmwareFile = fopen("firmware.bin", "rb");
    if (!firmwareFile) return false;
    fread(spi::firmware, sizeof(uint8_t), 0x40000, firmwareFile);
    fclose(firmwareFile);

    return true;
}

bool loadRom(char *filename)
{
    // Load the ROM
    FILE *romFile = fopen(filename, "rb");
    if (!romFile) return false;
    fseek(romFile, 0, SEEK_END);
    int romSize = ftell(romFile);
    fseek(romFile, 0, SEEK_SET);
    if (cartridge::rom) delete[] cartridge::rom;
    cartridge::rom = new uint8_t[romSize];
    fread(cartridge::rom, sizeof(uint8_t), romSize, romFile);
    fclose(romFile);

    // Load the save file
    saveName = ((std::string)filename).substr(0, ((std::string)filename).rfind(".")) + ".sav";
    FILE *saveFile = fopen(saveName.c_str(), "rb");
    if (saveFile)
    {
        // If the file exists, load it and store the size
        fseek(saveFile, 0, SEEK_END);
        spi::saveSize = ftell(saveFile);
        fseek(saveFile, 0, SEEK_SET);
        if (spi::save) delete[] spi::save;
        spi::save = new uint8_t[spi::saveSize];
        fread(spi::save, sizeof(uint8_t), spi::saveSize, saveFile);
        fclose(saveFile);
    }
    else
    {
        // If the file doesn't exist, saving won't be supported
        // Eventually there will be some way to detect or set the save type in this case
        spi::saveSize = 0;
    }

    // Load the initial ROM code into memory using values from the header
    for (unsigned int i = 0; i < *(uint32_t*)&cartridge::rom[0x2C]; i++)
    {
        memory::write<uint8_t>(&interpreter::arm9, *(uint32_t*)&cartridge::rom[0x28] + i,
                               cartridge::rom[*(uint32_t*)&cartridge::rom[0x20] + i]);
    }
    for (unsigned int i = 0; i < *(uint32_t*)&cartridge::rom[0x3C]; i++)
    {
        memory::write<uint8_t>(&interpreter::arm7, *(uint32_t*)&cartridge::rom[0x38] + i,
                               cartridge::rom[*(uint32_t*)&cartridge::rom[0x30] + i]);
    }

    // Load the ROM header into memory
    for (unsigned int i = 0; i < 0x170; i++)
        memory::write<uint8_t>(&interpreter::arm9, 0x27FFE00 + i, cartridge::rom[i]);

    // Load the user settings into memory
    for (unsigned int i = 0; i < 0x70; i++)
        memory::write<uint8_t>(&interpreter::arm9, 0x27FFC80 + i, spi::firmware[0x3FF00 + i]);

    // Write some memory values as the BIOS/firmware would
    memory::write<uint32_t>(&interpreter::arm9, 0x27FF800, 0x00001FC2); // Chip ID 1
    memory::write<uint32_t>(&interpreter::arm9, 0x27FF804, 0x00001FC2); // Chip ID 2
    memory::write<uint16_t>(&interpreter::arm9, 0x27FF850,     0x5835); // ARM7 BIOS CRC
    memory::write<uint16_t>(&interpreter::arm9, 0x27FF880,     0x0007); // Message from ARM9 to ARM7
    memory::write<uint16_t>(&interpreter::arm9, 0x27FF884,     0x0006); // ARM7 boot task
    memory::write<uint32_t>(&interpreter::arm9, 0x27FFC00, 0x00001FC2); // Copy of chip ID 1
    memory::write<uint32_t>(&interpreter::arm9, 0x27FFC04, 0x00001FC2); // Copy of chip ID 2
    memory::write<uint16_t>(&interpreter::arm9, 0x27FFC10,     0x5835); // Copy of ARM7 BIOS CRC
    memory::write<uint16_t>(&interpreter::arm9, 0x27FFC40,     0x0001); // Boot indicator

    // Write some register values as the BIOS/firmware would
    memory::write<uint8_t>(&interpreter::arm9, 0x4000247, 0x03); // WRAMCNT
    memory::write<uint8_t>(&interpreter::arm9, 0x4000300, 0x01); // POSTFLG_9
    memory::write<uint8_t>(&interpreter::arm7, 0x4000300, 0x01); // POSTFLG_7
    cp15::writeRegister(9, 1, 0, 0x027C0005); // Data TCM base/size
    cp15::writeRegister(9, 1, 1, 0x00000010); // Instruction TCM base/size

    // Point the ARM9 to its starting execution point and initialize the stack pointers
    interpreter::arm9.registersUsr[12] = *(uint32_t*)&cartridge::rom[0x24];
    interpreter::arm9.registersUsr[13] = 0x03002F7C;
    interpreter::arm9.registersUsr[14] = *(uint32_t*)&cartridge::rom[0x24];
    interpreter::arm9.registersUsr[15] = *(uint32_t*)&cartridge::rom[0x24] + 8;
    interpreter::arm9.registersIrq[0]  = 0x03003F80;
    interpreter::arm9.registersSvc[0]  = 0x03003FC0;
    interpreter::setMode(&interpreter::arm9, 0x1F);

    // Point the ARM7 to its starting execution point and initialize the stack pointers
    interpreter::arm7.registersUsr[12] = *(uint32_t*)&cartridge::rom[0x34];
    interpreter::arm7.registersUsr[13] = 0x0380FD80;
    interpreter::arm7.registersUsr[14] = *(uint32_t*)&cartridge::rom[0x34];
    interpreter::arm7.registersUsr[15] = *(uint32_t*)&cartridge::rom[0x34] + 8;
    interpreter::arm7.registersIrq[0]  = 0x0380FF80;
    interpreter::arm7.registersSvc[0]  = 0x0380FFC0;
    interpreter::setMode(&interpreter::arm7, 0x1F);

    return true;
}

void writeSave()
{
    // Don't write a save file if the file size is unknown
    if (spi::saveSize == 0)
        return;

    // Write the save to a file
    FILE *saveFile = fopen(saveName.c_str(), "wb");
    if (saveFile)
        fwrite(spi::save, sizeof(uint8_t), spi::saveSize, saveFile);
}

void runScanline()
{
    for (int i = 0; i < 355 * 6; i++)
    {
        // Run the CPUs, with the ARM7 running at half the frequency of the ARM9
        if (*interpreter::arm9.ie & *interpreter::arm9.irf)
            interpreter::interrupt(&interpreter::arm9);
        if (!interpreter::arm9.halt)
            interpreter::execute(&interpreter::arm9);
        if (i % 2 == 0)
        {
            if (*interpreter::arm7.ie & *interpreter::arm7.irf)
                interpreter::interrupt(&interpreter::arm7);
            if (!interpreter::arm7.halt)
                interpreter::execute(&interpreter::arm7);
        }

        // The end of the visible scanline, and the start of H-blank
        if (i == 256 * 6)
            gpu::scanline256();

        for (int j = 0; j < 4; j++)
        {
            // Tick the timers if they're enabled
            if (*interpreter::arm9.tmcntH[j] & BIT(7))
                timer::tick(&interpreter::arm9, j);
            if (*interpreter::arm7.tmcntH[j] & BIT(7))
                timer::tick(&interpreter::arm7, j);

            // Perform DMA transfers if they're enabled
            if (*interpreter::arm9.dmacnt[j] & BIT(31))
                dma::transfer(&interpreter::arm9, j);
            if (*interpreter::arm7.dmacnt[j] & BIT(31))
                dma::transfer(&interpreter::arm7, j);
        }
    }

    // The end of the scanline
    gpu::scanline355();
}

void pressKey(uint8_t key)
{
    // Clear key bits to indicate presses
    if (key < 10) // A, B, select, start, right, left, up, down, R, L
    {
        *interpreter::arm9.keyinput &= ~BIT(key);
        *interpreter::arm7.keyinput &= ~BIT(key);
    }
    else if (key < 12) // X, Y
    {
        *memory::extkeyin &= ~BIT(key - 10);
    }
}

void releaseKey(uint8_t key)
{
    // Set key bits to indicate releases
    if (key < 10) // A, B, select, start, right, left, up, down, R, L
    {
        *interpreter::arm9.keyinput |= BIT(key);
        *interpreter::arm7.keyinput |= BIT(key);
    }
    else if (key < 12) // X, Y
    {
        *memory::extkeyin |= BIT(key - 10);
    }
}

void pressScreen(uint8_t x, uint8_t y)
{
    // Read calibration points from the firmware
    uint16_t adcX1 = *(uint16_t*)&spi::firmware[0x3FF58];
    uint16_t adcY1 = *(uint16_t*)&spi::firmware[0x3FF5A];
    uint8_t  scrX1 =              spi::firmware[0x3FF5C];
    uint8_t  scrY1 =              spi::firmware[0x3FF5D];
    uint16_t adcX2 = *(uint16_t*)&spi::firmware[0x3FF5E];
    uint16_t adcY2 = *(uint16_t*)&spi::firmware[0x3FF60];
    uint8_t  scrX2 =              spi::firmware[0x3FF62];
    uint8_t  scrY2 =              spi::firmware[0x3FF63];

    // Convert the screen coordinates to ADC values and send them to the SPI
    if (scrX2 - scrX1 != 0)
        spi::touchX = (x - (scrX1 - 1)) * (adcX2 - adcX1) / (scrX2 - scrX1) + adcX1;
    if (scrY2 - scrY1 != 0)
        spi::touchY = (y - (scrY1 - 1)) * (adcY2 - adcY1) / (scrY2 - scrY1) + adcY1;

    // Set the pen down bit
    *memory::extkeyin &= ~BIT(6);
}

void releaseScreen()
{
    // Set the SPI values to their released state
    spi::touchX = 0x000;
    spi::touchY = 0xFFF;

    // Clear the pen down bit
    *memory::extkeyin |= BIT(6);
    
}

}
