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
#include "gpu.h"
#include "memory.h"

namespace core
{

interpreter::Cpu arm9, arm7;

void init(uint32_t entry9, uint32_t entry7)
{
    memset(&arm9, 0, sizeof(arm9));
    for (int i = 0; i < 16; i++)
        arm9.registers[i] = &arm9.registersUsr[i];
    arm9.registersUsr[12] = entry9;
    arm9.registersUsr[14] = entry9;
    arm9.registersUsr[15] = entry9 + 8;
    arm9.registersUsr[13] = 0x03002F7C;
    arm9.registersIrq[0]  = 0x03003F80;
    arm9.registersSvc[0]  = 0x03003FC0;
    arm9.cpsr             = 0x000000DF;
    arm9.type             = 9;

    memset(&arm7, 0, sizeof(arm9));
    for (int i = 0; i < 16; i++)
        arm7.registers[i] = &arm7.registersUsr[i];
    arm7.registersUsr[12] = entry7;
    arm7.registersUsr[14] = entry7;
    arm7.registersUsr[15] = entry7 + 8;
    arm7.registersUsr[13] = 0x0380FD80;
    arm7.registersIrq[0]  = 0x0380FF80;
    arm7.registersSvc[0]  = 0x0380FFC0;
    arm7.cpsr             = 0x000000DF;
    arm7.type             = 7;
}

bool loadRom(char *filename)
{
    // Load the ROM
    FILE *romFile = fopen(filename, "rb");
    if (!romFile) return false;
    fseek(romFile, 0, SEEK_END);
    int size = ftell(romFile);
    uint8_t *rom = new uint8_t[size];
    fseek(romFile, 0, SEEK_SET);
    fread(rom, sizeof(uint8_t), size, romFile);
    fclose(romFile);

    // Load the ARM9 BIOS
    FILE *bios9File = fopen("bios9.bin", "rb");
    if (!bios9File) return false;
    uint8_t bios9[0x1000];
    fread(bios9, sizeof(uint8_t), 0x1000, bios9File);
    fclose(bios9File);

    // Load the ARM7 BIOS
    FILE *bios7File = fopen("bios7.bin", "rb");
    if (!bios7File) return false;
    uint8_t bios7[0x4000];
    fread(bios7, sizeof(uint8_t), 0x4000, bios7File);
    fclose(bios7File);

    // Initialize the system
    init(*(uint32_t*)&rom[0x24], *(uint32_t*)&rom[0x34]);
    interpreter::init();
    gpu::init();
    memory::init();

    // Write the program to memory
    for (int i = 0; i < *(uint32_t*)&rom[0x2C]; i++)
        memory::write<uint8_t>(&arm9, *(uint32_t*)&rom[0x28] + i, rom[*(uint32_t*)&rom[0x20] + i]);
    for (int i = 0; i < *(uint32_t*)&rom[0x3C]; i++)
        memory::write<uint8_t>(&arm7, *(uint32_t*)&rom[0x38] + i, rom[*(uint32_t*)&rom[0x30] + i]);
    for (int i = 0; i < 0x1000; i++)
        memory::write<uint8_t>(&arm9, 0xFFFF0000 + i, bios9[i]);
    for (int i = 0; i < 0x4000; i++)
        memory::write<uint8_t>(&arm7, 0x00000000 + i, bios7[i]);
    delete[] rom;

    return true;
}

void runScanline()
{
    for (int i = 0; i < 355 * 6; i++)
    {
        if (!arm9.halt)
            interpreter::execute(&arm9);
        if (i % 2 == 0 && !arm7.halt)
            interpreter::execute(&arm7);
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
