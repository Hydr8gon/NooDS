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
#include <cstring>

#include "core.h"
#include "cp15.h"
#include "interpreter.h"
#include "gpu.h"
#include "memory.h"

namespace core
{

bool init()
{
    // Initialize everything else
    cp15::init();
    interpreter::init();
    gpu::init();
    memory::init();

    // Reset the ARM9
    memset(&interpreter::arm9, 0, sizeof(interpreter::arm9));
    for (int i = 0; i < 16; i++)
        interpreter::arm9.registers[i] = &interpreter::arm9.registersUsr[i];
    interpreter::arm9.registersUsr[15] = 0xFFFF0000 + 8;
    interpreter::arm9.cpsr             = 0x000000C0;
    interpreter::arm9.type             = 9;
    interpreter::setMode(&interpreter::arm9, 0x13);

    // Reset the ARM7
    memset(&interpreter::arm7, 0, sizeof(interpreter::arm7));
    for (int i = 0; i < 16; i++)
        interpreter::arm7.registers[i] = &interpreter::arm7.registersUsr[i];
    interpreter::arm7.registersUsr[15] = 0x00000000 + 8;
    interpreter::arm7.cpsr             = 0x000000C0;
    interpreter::arm7.type             = 7;
    interpreter::setMode(&interpreter::arm7, 0x13);

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
    fread(memory::firmware, sizeof(uint8_t), 0x40000, firmwareFile);
    fclose(firmwareFile);

    return true;
}

bool loadRom(char *filename)
{
    // Load the ROM
    FILE *romFile = fopen(filename, "rb");
    if (!romFile) return false;
    fseek(romFile, 0, SEEK_END);
    int size = ftell(romFile);
    if (memory::rom) delete[] memory::rom;
    memory::rom = new uint8_t[size];
    fseek(romFile, 0, SEEK_SET);
    fread(memory::rom, sizeof(uint8_t), size, romFile);
    fclose(romFile);

    // Set memory values
    memory::write<uint8_t>(&interpreter::arm9, 0x4000247, 0x03);
    cp15::writeRegister(1, 0, 0, 0x00000078);
    cp15::writeRegister(9, 1, 0, 0x027C0005);
    cp15::writeRegister(9, 1, 1, 0x00000010);

    // Prepare the ARM9
    interpreter::arm9.registersUsr[12] = *(uint32_t*)&memory::rom[0x24];
    interpreter::arm9.registersUsr[13] = 0x03002F7C;
    interpreter::arm9.registersUsr[14] = *(uint32_t*)&memory::rom[0x24];
    interpreter::arm9.registersUsr[15] = *(uint32_t*)&memory::rom[0x24] + 8;
    interpreter::arm9.registersIrq[0]  = 0x03003F80;
    interpreter::arm9.registersSvc[0]  = 0x03003FC0;
    interpreter::setMode(&interpreter::arm9, 0x1F);
    for (int i = 0; i < *(uint32_t*)&memory::rom[0x2C]; i++)
        memory::write<uint8_t>(&interpreter::arm9, *(uint32_t*)&memory::rom[0x28] + i, memory::rom[*(uint32_t*)&memory::rom[0x20] + i]);

    // Prepare the ARM7
    interpreter::arm7.registersUsr[12] = *(uint32_t*)&memory::rom[0x34];
    interpreter::arm7.registersUsr[13] = 0x0380FD80;
    interpreter::arm7.registersUsr[14] = *(uint32_t*)&memory::rom[0x34];
    interpreter::arm7.registersUsr[15] = *(uint32_t*)&memory::rom[0x34] + 8;
    interpreter::arm7.registersIrq[0]  = 0x0380FF80;
    interpreter::arm7.registersSvc[0]  = 0x0380FFC0;
    interpreter::setMode(&interpreter::arm7, 0x1F);
    for (int i = 0; i < *(uint32_t*)&memory::rom[0x3C]; i++)
        memory::write<uint8_t>(&interpreter::arm7, *(uint32_t*)&memory::rom[0x38] + i, memory::rom[*(uint32_t*)&memory::rom[0x30] + i]);

    return true;
}

void runScanline()
{
    for (int i = 0; i < 355 * 6; i++)
    {
        if (!interpreter::arm9.halt)
            interpreter::execute(&interpreter::arm9);
        if (i % 2 == 0 && !interpreter::arm7.halt)
            interpreter::execute(&interpreter::arm7);
        if (i == 256 * 6)
            gpu::scanline256();
    }
    gpu::scanline355();
}

void pressKey(uint8_t key)
{
    if (key < 10)
        memory::keyinput &= ~BIT(key);
    else if (key < 12)
        memory::extkeyin &= ~BIT(key - 10);
}

void releaseKey(uint8_t key)
{
    if (key < 10)
        memory::keyinput |= BIT(key);
    else if (key < 12)
        memory::extkeyin |= BIT(key - 10);
}

}
