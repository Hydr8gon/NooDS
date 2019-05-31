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
#include "core.h"
#include "cp15.h"

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

bool     vramEnables[9];
uint32_t vramOffsets[9];

uint32_t ipcsync9, ipcsync7; // IPC synchronize
uint32_t ime9,     ime7;     // Interrupt master enable
uint32_t ie9,      ie7;      // Interrupt enable
uint32_t if9,      if7;      // Interrupt request flags

uint32_t dispcntA; // 2D engine A LCD control
uint32_t dispstat; // General LCD status
uint32_t vcount;   // Vertical counter
uint32_t powcnt1;  // Graphics power control
uint32_t dispcntB; // 2D engine B LCD control

void *vramMap(uint32_t address)
{
    if (address >= 0x5000000 && address < 0x6000000) // 2KB palettes
        return &palettes[(address - 0x5000000) % 0x800];
    else if (vramEnables[0] && address >= vramOffsets[0] && address < vramOffsets[0] + 0x20000) // 128KB VRAM block A
        return &vramA[(address - vramOffsets[0]) % 0x20000];
    else if (vramEnables[1] && address >= vramOffsets[1] && address < vramOffsets[1] + 0x20000) // 128KB VRAM block B
        return &vramB[(address - vramOffsets[1]) % 0x20000];
    else if (vramEnables[2] && address >= vramOffsets[2] && address < vramOffsets[2] + 0x20000) // 128KB VRAM block C
        return &vramC[(address - vramOffsets[2]) % 0x20000];
    else if (vramEnables[3] && address >= vramOffsets[3] && address < vramOffsets[3] + 0x20000) // 128KB VRAM block D
        return &vramD[(address - vramOffsets[3]) % 0x20000];
    else if (vramEnables[4] && address >= vramOffsets[4] && address < vramOffsets[4] + 0x10000) // 64KB VRAM block E
        return &vramE[(address - vramOffsets[4]) % 0x10000];
    else if (vramEnables[5] && address >= vramOffsets[5] && address < vramOffsets[5] + 0x4000) // 16KB VRAM block F
        return &vramF[(address - vramOffsets[5]) % 0x4000];
    else if (vramEnables[6] && address >= vramOffsets[6] && address < vramOffsets[6] + 0x4000) // 16KB VRAM block G
        return &vramG[(address - vramOffsets[6]) % 0x4000];
    else if (vramEnables[7] && address >= vramOffsets[7] && address < vramOffsets[7] + 0x8000) // 32KB VRAM block H
        return &vramH[(address - vramOffsets[7]) % 0x8000];
    else if (vramEnables[8] && address >= vramOffsets[8] && address < vramOffsets[8] + 0x4000) // 16KB VRAM block I
        return &vramI[(address - vramOffsets[8]) % 0x4000];
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

        case 0x4000208: // IME_9
            return ime9;

        case 0x4000210: // IE_9
            return ie9;

        case 0x4000214: // IF_9
            return if9;

        case 0x4000304: // POWCNT1
            return powcnt1;

        case 0x4001000: // DISPCNT_B
            return dispcntB;

        default:
            printf("Unknown ARM9 I/O read: 0x%X\n", address);
            return 0;
    }
}

template <typename T> void ioWriteMap9(uint32_t address, T value)
{
    switch (address)
    {
        case 0x4000000: // DISPCNT_A
            *(T*)&dispcntA = value;
            break;

        case 0x4000004: // DISPSTAT
            *(T*)&dispstat = (value & 0xFFB8) | (dispstat & 0x0007);
            break;

        case 0x4000180: // IPCSYNC_9
            ipcsync9 = (ipcsync9 & 0x000F) | (value & 0x4F00);
            ipcsync7 = (ipcsync7 & 0x4F00) | ((value & 0x0F00) >> 8);
            if ((value & BIT(13)) && (ipcsync7 & BIT(14))) // Remote IRQ
                interpreter::irq7(16);
            break;

        case 0x4000208: // IME_9
            ime9 = value & BIT(0);
            break;

        case 0x4000210: // IE_9
            *(T*)&ie9 = value & 0xFFFFFF7F;
            break;

        case 0x4000214: // IF_9
            if9 &= ~((uint32_t)value);
            break;

        case 0x4000240: // VRAMCNT_A
            if (value & BIT(7))
            {
                vramEnables[0] = true;
                uint8_t mst = (value & 0x03);
                uint8_t offset = (value & 0x18) >> 3;
                switch (mst) // MST
                {
                    case 0x0: vramOffsets[0] = 0x6800000;                               break;
                    case 0x1: vramOffsets[0] = 0x6000000 + 0x20000 * offset;            break;
                    case 0x2: vramOffsets[0] = 0x6400000 + 0x20000 * (offset & BIT(0)); break;
                    default: vramEnables[0] = false; printf("Unknown VRAM A MST: %d\n", mst); break;
                }
            }
            else
            {
                vramEnables[0] = false;
            }
            break;

        case 0x4000241: // VRAMCNT_B
            if (value & BIT(7))
            {
                vramEnables[1] = true;
                uint8_t mst = (value & 0x03);
                uint8_t offset = (value & 0x18) >> 3;
                switch (mst)
                {
                    case 0x0: vramOffsets[1] = 0x6820000;                               break;
                    case 0x1: vramOffsets[1] = 0x6000000 + 0x20000 * offset;            break;
                    case 0x2: vramOffsets[1] = 0x6400000 + 0x20000 * (offset & BIT(0)); break;
                    default: vramEnables[1] = false; printf("Unknown VRAM B MST: %d\n", mst); break;
                }
            }
            else
            {
                vramEnables[1] = false;
            }
            break;

        case 0x4000242: // VRAMCNT_C
            if (value & BIT(7))
            {
                vramEnables[2] = true;
                uint8_t mst = (value & 0x07);
                uint8_t offset = (value & 0x18) >> 3;
                switch (mst)
                {
                    case 0x0: vramOffsets[2] = 0x6840000;                    break;
                    case 0x1: vramOffsets[2] = 0x6000000 + 0x20000 * offset; break;
                    case 0x4: vramOffsets[2] = 0x6200000;                    break;
                    default: vramEnables[2] = false; printf("Unknown VRAM C MST: %d\n", mst); break;
                }
            }
            else
            {
                vramEnables[2] = false;
            }
            break;

        case 0x4000243: // VRAMCNT_D
            if (value & BIT(7))
            {
                vramEnables[3] = true;
                uint8_t mst = (value & 0x07);
                uint8_t offset = (value & 0x18) >> 3;
                switch (mst)
                {
                    case 0x0: vramOffsets[3] = 0x6860000;                    break;
                    case 0x1: vramOffsets[3] = 0x6000000 + 0x20000 * offset; break;
                    case 0x4: vramOffsets[3] = 0x6600000;                    break;
                    default: vramEnables[3] = false; printf("Unknown VRAM D MST: %d\n", mst); break;
                }
            }
            else
            {
                vramEnables[3] = false;
            }
            break;

        case 0x4000244: // VRAMCNT_E
            if (value & BIT(7))
            {
                vramEnables[4] = true;
                uint8_t mst = (value & 0x07);
                uint8_t offset = (value & 0x18) >> 3;
                switch (mst)
                {
                    case 0x0: vramOffsets[4] = 0x6880000;                    break;
                    case 0x1: vramOffsets[4] = 0x6000000;                    break;
                    case 0x2: vramOffsets[4] = 0x6400000;                    break;
                    default: vramEnables[4] = false; printf("Unknown VRAM E MST: %d\n", mst); break;
                }
            }
            else
            {
                vramEnables[4] = false;
            }
            break;

        case 0x4000245: // VRAMCNT_F
            if (value & BIT(7))
            {
                vramEnables[5] = true;
                uint8_t mst = (value & 0x07);
                uint8_t offset = (value & 0x18) >> 3;
                switch (mst)
                {
                    case 0x0: vramOffsets[5] = 0x6890000;                                                           break;
                    case 0x1: vramOffsets[5] = 0x6000000 + 0x8000 * (offset & BIT(1)) + 0x4000 * (offset & BIT(0)); break;
                    case 0x2: vramOffsets[5] = 0x6400000 + 0x8000 * (offset & BIT(1)) + 0x4000 * (offset & BIT(0)); break;
                    default: vramEnables[5] = false; printf("Unknown VRAM F MST: %d\n", mst); break;
                }
            }
            else
            {
                vramEnables[5] = false;
            }
            break;

        case 0x4000246: // VRAMCNT_G
            if (value & BIT(7))
            {
                vramEnables[6] = true;
                uint8_t mst = (value & 0x07);
                uint8_t offset = (value & 0x18) >> 3;
                switch (mst)
                {
                    case 0x0: vramOffsets[6] = 0x6894000;                                                           break;
                    case 0x1: vramOffsets[6] = 0x6000000 + 0x8000 * (offset & BIT(1)) + 0x4000 * (offset & BIT(0)); break;
                    case 0x2: vramOffsets[6] = 0x6400000 + 0x8000 * (offset & BIT(1)) + 0x4000 * (offset & BIT(0)); break;
                    default: vramEnables[6] = false; printf("Unknown VRAM G MST: %d\n", mst); break;
                }
            }
            else
            {
                vramEnables[6] = false;
            }
            break;

        case 0x4000248: // VRAMCNT_H
            if (value & BIT(7))
            {
                vramEnables[7] = true;
                uint8_t mst = (value & 0x03);
                switch (mst)
                {
                    case 0x0: vramOffsets[7] = 0x6898000; break;
                    case 0x1: vramOffsets[7] = 0x6200000; break;
                    default: vramEnables[7] = false; printf("Unknown VRAM H MST: %d\n", mst); break;
                }
            }
            else
            {
                vramEnables[7] = false;
            }
            break;

        case 0x4000249: // VRAMCNT_I
            if (value & BIT(7))
            {
                vramEnables[8] = true;
                uint8_t mst = (value & 0x03);
                switch (mst)
                {
                    case 0x0: vramOffsets[8] = 0x68A0000; break;
                    case 0x1: vramOffsets[8] = 0x6208000; break;
                    case 0x2: vramOffsets[8] = 0x6600000; break;
                    default: vramEnables[8] = false; printf("Unknown VRAM I MST: %d\n", mst); break;
                }
            }
            else
            {
                vramEnables[8] = false;
            }
            break;

        case 0x4000304: // POWCNT1
            *(T*)&powcnt1 = (value & 0x820F);
            break;

        case 0x4001000: // DISPCNT_B
            *(T*)&dispcntB = (value & 0xC0B3FFF7);
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

        case 0x4000208: // IME_7
            return ime7;

        case 0x4000210: // IE_7
            return ie7;

        case 0x4000214: // IF_7
            return if7;

        default:
            printf("Unknown ARM7 I/O read: 0x%X\n", address);
            return 0;
    }
}

template <typename T> void ioWriteMap7(uint32_t address, T value)
{
    switch (address)
    {
        case 0x4000004: // DISPSTAT
            *(T*)&dispstat = (value & 0xFFB8) | (dispstat & 0x0007);
            break;

        case 0x4000180: // IPCSYNC_7
            ipcsync7 = (ipcsync7 & 0x000F) | (value & 0x4F00);
            ipcsync9 = (ipcsync9 & 0x4F00) | ((value & 0x0F00) >> 8);
            if ((value & BIT(13)) && (ipcsync9 & BIT(14))) // Remote IRQ
                interpreter::irq9(16);
            break;

        case 0x4000208: // IME_7
            ime7 = value & BIT(0);
            break;

        case 0x4000210: // IE_7
            *(T*)&ie7 = value;
            break;

        case 0x4000214: // IF_7
            if7 &= ~((uint32_t)value);
            break;

        case 0x4000301: // HALTCNT
            switch ((value & 0xC0) >> 6)
            {
                case 0x1:
                    printf("Unhandled request: GBA mode\n");
                    break;

                case 0x2:
                    core::arm7.halt = true;
                    break;

                case 0x3:
                    printf("Unhandled request: Sleep mode\n");
                    break;
            }
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
    if (cpu->type == 9)
    {
        if (address >= 0x4000000 && address < 0x5000000)
        {
            return ioReadMap9(address);
        }
        else
        {
            T *src = (T*)memoryMap9(address);
            if (src)
                return *src;
            else
                printf("Unknown ARM9 memory read: 0x%X\n", address);
        }
    }
    else
    {
        if (address >= 0x4000000 && address < 0x5000000)
        {
            return ioReadMap7(address);
        }
        else
        {
            T *src = (T*)memoryMap7(address);
            if (src)
                return *src;
            else
                printf("Unknown ARM7 memory read: 0x%X\n", address);
        }
    }

    return 0;
}

template void write(interpreter::Cpu *cpu, uint32_t address, uint8_t  value);
template void write(interpreter::Cpu *cpu, uint32_t address, uint16_t value);
template void write(interpreter::Cpu *cpu, uint32_t address, uint32_t value);
template <typename T> void write(interpreter::Cpu *cpu, uint32_t address, T value)
{
    if (cpu->type == 9)
    {
        if (address >= 0x4000000 && address < 0x5000000)
        {
            ioWriteMap9<T>(address, value);
        }
        else
        {
            T *dst = (T*)memoryMap9(address);
            if (dst)
                *dst = value;
            else
                printf("Unknown ARM9 memory write: 0x%X\n", address);
        }
    }
    else
    {
        if (address >= 0x4000000 && address < 0x5000000)
        {
            ioWriteMap7<T>(address, value);
        }
        else
        {
            T *dst = (T*)memoryMap7(address);
            if (dst)
                *dst = value;
            else
                printf("Unknown ARM7 memory write: 0x%X\n", address);
        }
    }
}

}
