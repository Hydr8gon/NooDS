/*
    Copyright 2019 Hydr8gon

    This file is part of NoiDS.

    NoiDS is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    NoiDS is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with NoiDS. If not, see <https://www.gnu.org/licenses/>.
*/

#include <cstdio>

#include "memory.h"
#include "cp15.h"

#define BIT(i) (1 << i)

namespace memory
{

uint8_t ram[0x400000];    //  4MB main RAM
uint8_t wram[0x8000];     // 32KB shared WRAM
uint8_t instrTcm[0x8000]; // 32KB instruction TCM
uint8_t dataTcm[0x4000];  // 16KB data TCM
uint8_t bios9[0x8000];    // 32KB ARM9 BIOS
uint8_t bios7[0x4000];    // 16KB ARM7 BIOS
uint8_t wram7[0x10000];   // 64KB ARM7 WRAM

uint8_t palettes[0x800]; //   2KB palettes
uint8_t vramA[0x20000];  // 128KB VRAM block A
uint8_t vramB[0x20000];  // 128KB VRAM block B
uint8_t vramC[0x20000];  // 128KB VRAM block C
uint8_t vramD[0x20000];  // 128KB VRAM block D
uint8_t vramE[0x10000];  //  64KB VRAM block E
uint8_t vramF[0x4000];   //  16KB VRAM block F
uint8_t vramG[0x4000];   //  16KB VRAM block G
uint8_t vramH[0x8000];   //  32KB VRAM block H
uint8_t vramI[0x4000];   //  16KB VRAM block I
uint8_t oam[0x800];      //   2KB OAM

uint16_t ipcsync9; // ARM9 IPC synchronize
uint16_t ipcsync7; // ARM7 IPC synchronize

uint32_t dispcntA; // 2D engine A LCD control
uint16_t dispstat; // General LCD status
uint16_t vcount;   // Vertical counter
uint16_t powcnt1;  // Graphics power control
uint32_t dispcntB; // 2D engine B LCD control

void *vramMap(uint32_t address)
{
    if (address >= 0x5000000 && address < 0x6000000) // 2KB palettes
        return &palettes[(address - 0x5000000) % 0x800];
    else if (address >= 0x6800000 && address < 0x6820000) // 128KB VRAM block A
        return &vramA[(address - 0x6800000) % 0x20000];
    else if (address >= 0x6820000 && address < 0x6840000) // 128KB VRAM block B
        return &vramB[(address - 0x6820000) % 0x20000];
    else if (address >= 0x6840000 && address < 0x6860000) // 128KB VRAM block C
        return &vramC[(address - 0x6840000) % 0x20000];
    else if (address >= 0x6860000 && address < 0x6880000) // 128KB VRAM block D
        return &vramD[(address - 0x6860000) % 0x20000];
    else if (address >= 0x6880000 && address < 0x6890000) // 64KB VRAM block E
        return &vramE[(address - 0x6880000) % 0x10000];
    else if (address >= 0x6890000 && address < 0x6894000) // 16KB VRAM block F
        return &vramF[(address - 0x6890000) % 0x4000];
    else if (address >= 0x6894000 && address < 0x6898000) // 16KB VRAM block G
        return &vramG[(address - 0x6894000) % 0x4000];
    else if (address >= 0x6898000 && address < 0x68A0000) // 32KB VRAM block H
        return &vramH[(address - 0x6898000) % 0x8000];
    else if (address >= 0x68A0000 && address < 0x68A4000) // 16KB VRAM block I
        return &vramI[(address - 0x68A0000) % 0x4000];
    else if (address >= 0x68A4000 && address < 0x7000000) // Mirror
        return vramMap(address - 0x8A4000);
    else if (address >= 0x7000000 && address < 0x8000000) // 2KB OAM
        return &oam[(address - 0x7000000) % 0x800];
    else
        return nullptr;
}

void *memoryMap9(uint32_t address)
{
    if (cp15::itcmEnable && address < cp15::itcmSize) // 32KB instruction TCM
        return &instrTcm[address % 0x8000];
    else if (cp15::dtcmEnable && address >= cp15::dtcmBase && address < cp15::dtcmBase + cp15::dtcmSize) // 16KB data TCM
        return &dataTcm[(address - cp15::dtcmBase) % 0x4000];
    else if (address >= 0x2000000 && address < 0x3000000) // 4MB main RAM
        return &ram[(address - 0x2000000) % 0x400000];
    else if (address >= 0x3000000 && address < 0x4000000) // 32KB shared WRAM
        return &wram[(address - 0x3000000) % 0x8000];
    else if (address >= 0x5000000 && address < 0x8000000) // VRAM
        return vramMap(address);
    else if (address >= 0xFFFF0000 && address < 0xFFFF8000) // 32KB ARM9 BIOS
        return &bios9[address - 0xFFFF0000];
    else
        return nullptr;
}

void *memoryMap7(uint32_t address)
{
    if (address < 0x4000) // 16KB ARM7 BIOS
        return &bios7[address];
    else if (address >= 0x2000000 && address < 0x3000000) // 4MB main RAM
        return &ram[(address - 0x2000000) % 0x400000];
    else if (address >= 0x3000000 && address < 0x3800000) // 32KB shared WRAM
        return &wram[(address - 0x3000000) % 0x8000];
    else if (address >= 0x3800000 && address < 0x4000000) // 64KB ARM7 WRAM
        return &wram7[(address - 0x3800000) % 0x10000];
    else
        return nullptr;
}

uint32_t ioReadMap9(uint32_t address)
{
    switch (address)
    {
        case 0x4000000: // DISPCNT_A
            return dispcntA;

        case 0x4000004: // DISPSTAT
            return dispstat;

        case 0x4000006: // VCOUNT
            return vcount;

        case 0x4000180: // IPCSYNC_9
            return ipcsync9;

        case 0x4000304: // POWCNT1
            return powcnt1;

        case 0x4001000: // DISPCNT_B
            return dispcntB;

        default:
            printf("Unknown ARM9 I/O read: 0x%X\n", address);
            return 0;
    }
}

void ioWriteMap9(uint32_t address, uint32_t value)
{
    switch (address)
    {
        case 0x4000000: // DISPCNT_A
            dispcntA = value;
            break;

        case 0x4000004: // DISPSTAT
            dispstat = (value & 0xFFB8) | (dispstat & 0x0007);
            break;

        case 0x4000180: // IPCSYNC_9
            ipcsync9 = (ipcsync9 & 0x000F) | (value & 0x4F00);
            ipcsync7 = (ipcsync7 & 0x4F00) | ((value & 0x0F00) >> 8);
            if ((value & BIT(13)) && (ipcsync7 & BIT(14))) // Remote IRQ
                printf("Unhandled IPCSYNC_9 IRQ\n");
            break;

        case 0x4000304: // POWCNT1
            powcnt1 = (value & 0x820F);
            break;

        case 0x4001000: // DISPCNT_B
            dispcntB = (value & 0xC0B3FFF7);
            break;

        default:
            printf("Unknown ARM9 I/O write: 0x%X\n", address);
            break;
    }
}

uint32_t ioReadMap7(uint32_t address)
{
    switch (address)
    {
        case 0x4000004: // DISPSTAT
            return dispstat;

        case 0x4000006: // VCOUNT
            return vcount;

        case 0x4000180: // IPCSYNC_7
            return ipcsync7;

        default:
            printf("Unknown ARM7 I/O read: 0x%X\n", address);
            return 0;
    }
}

void ioWriteMap7(uint32_t address, uint32_t value)
{
    switch (address)
    {
        case 0x4000004: // DISPSTAT
            dispstat = (value & 0xFFB8) | (dispstat & 0x0007);
            break;

        case 0x4000180: // IPCSYNC_7
            ipcsync7 = (ipcsync7 & 0x000F) | (value & 0x4F00);
            ipcsync9 = (ipcsync9 & 0x4F00) | ((value & 0x0F00) >> 8);
            if ((value & BIT(13)) && (ipcsync9 & BIT(14))) // Remote IRQ
                printf("Unhandled IPCSYNC_7 IRQ\n");
            break;

        default:
            printf("Unknown ARM7 I/O write: 0x%X\n", address);
            break;
    }
}

template uint8_t  read(interpreter::Cpu *cpu, uint32_t address);
template uint16_t read(interpreter::Cpu *cpu, uint32_t address);
template uint32_t read(interpreter::Cpu *cpu, uint32_t address);
template <typename T> T read(interpreter::Cpu *cpu, uint32_t address)
{
    if (address >= 0x4000000 && address < 0x5000000)
    {
        return cpu->ioReadMap(address);
    }
    else
    {
        T *src = (T*)cpu->memoryMap(address);
        if (src)
            return *src;
        else
            printf("Unknown ARM%d memory read: 0x%X\n", cpu->type, address);
    }

    return 0;
}

template void write(interpreter::Cpu *cpu, uint32_t address, uint8_t  value);
template void write(interpreter::Cpu *cpu, uint32_t address, uint16_t value);
template void write(interpreter::Cpu *cpu, uint32_t address, uint32_t value);
template <typename T> void write(interpreter::Cpu *cpu, uint32_t address, T value)
{
    if (address >= 0x4000000 && address < 0x5000000)
    {
        cpu->ioWriteMap(address, value);
    }
    else
    {
        T *dst = (T*)cpu->memoryMap(address);
        if (dst)
            *dst = value;
        else
            printf("Unknown ARM%d memory write: 0x%X\n", cpu->type, address);
    }
}

}
