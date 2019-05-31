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

#include "core.h"
#include "gpu.h"
#include "memory.h"

namespace core
{

uint8_t *rom;
interpreter::Cpu arm9, arm7;

bool init9(uint32_t romOffset, uint32_t entryAddress, uint32_t ramAddress, uint32_t size)
{
    // Set default values
    for (int i = 0; i < 16; i++)
        arm9.registers[i] = &arm9.registersUsr[i];
    arm9.registersUsr[12] = entryAddress;
    arm9.registersUsr[14] = entryAddress;
    arm9.registersUsr[15] = entryAddress + 8;
    arm9.registersUsr[13] = 0x03002F7C;
    arm9.registersIrq[0]  = 0x03003F80;
    arm9.registersSvc[0]  = 0x03003FC0;
    arm9.cpsr             = 0x000000DF;
    arm9.type             = 9;

    // Load the BIOS into memory
    FILE *file = fopen("bios9.bin", "rb");
    if (!file) return false;
    uint8_t bios[0x1000];
    fread(bios, sizeof(uint8_t), 0x1000, file);
    fclose(file);
    for (int i = 0; i < 0x1000; i++)
        memory::write<uint8_t>(&arm9, 0xFFFF0000 + i, bios[i]);

    // Load the code into memory
    for (int i = 0; i < size; i++)
        memory::write<uint8_t>(&arm9, ramAddress + i, core::rom[romOffset + i]);

    printf("ARM9 ROM offset:    0x%X\n", romOffset);
    printf("ARM9 entry address: 0x%X\n", entryAddress);
    printf("ARM9 RAM address:   0x%X\n", ramAddress);
    printf("ARM9 code size:     0x%X\n", size);

    return true;
}

bool init7(uint32_t romOffset, uint32_t entryAddress, uint32_t ramAddress, uint32_t size)
{
    // Set default values
    for (int i = 0; i < 16; i++)
        arm7.registers[i] = &arm7.registersUsr[i];
    arm7.registersUsr[12] = entryAddress;
    arm7.registersUsr[14] = entryAddress;
    arm7.registersUsr[15] = entryAddress + 8;
    arm7.registersUsr[13] = 0x0380FD80;
    arm7.registersIrq[0]  = 0x0380FF80;
    arm7.registersSvc[0]  = 0x0380FFC0;
    arm7.cpsr             = 0x000000DF;
    arm7.type             = 7;

    // Load the BIOS into memory
    FILE *file = fopen("bios7.bin", "rb");
    if (!file) return false;
    uint8_t bios[0x4000];
    fread(bios, sizeof(uint8_t), 0x4000, file);
    fclose(file);
    for (int i = 0; i < 0x4000; i++)
        memory::write<uint8_t>(&arm7, 0x00000000 + i, bios[i]);

    // Load the code into memory
    for (int i = 0; i < size; i++)
        memory::write<uint8_t>(&arm7, ramAddress + i, core::rom[romOffset + i]);

    printf("ARM7 ROM offset:    0x%X\n", romOffset);
    printf("ARM7 entry address: 0x%X\n", entryAddress);
    printf("ARM7 RAM address:   0x%X\n", ramAddress);
    printf("ARM7 code size:     0x%X\n", size);

    return true;
}

bool loadRom(char *filename)
{
    FILE *file = fopen(filename, "rb");
    if (!file)
    {
        printf("Failed to load ROM!\n");
        return false;
    }

    // Load the ROM
    fseek(file, 0, SEEK_END);
    int size = ftell(file);
    rom = new uint8_t[size];
    fseek(file, 0, SEEK_SET);
    fread(rom, sizeof(uint8_t), size, file);
    fclose(file);

    if (!init9(*(uint32_t*)&rom[0x20], *(uint32_t*)&rom[0x24], *(uint32_t*)&rom[0x28], *(uint32_t*)&rom[0x2C]))
    {
        printf("Failed to load ARM9 BIOS (bios9.bin)!\n");
        return false;
    }

    if (!init7(*(uint32_t*)&rom[0x30], *(uint32_t*)&rom[0x34], *(uint32_t*)&rom[0x38], *(uint32_t*)&rom[0x3C]))
    {
        printf("Failed to load ARM7 BIOS (bios7.bin)!\n");
        return false;
    }

    interpreter::init();

    return true;
}

void runDot()
{
    for (int i = 0; i < 6; i++)
    {
        if (i % 1 == 0)
            interpreter::execute(&arm9);
        if (i % 2 == 0)
            interpreter::execute(&arm7);
        if (i % 6 == 0)
            gpu::drawDot();
    }
}

}
