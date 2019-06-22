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

uint8_t paletteA[0x400]; //   1KB engine A palette
uint8_t paletteB[0x400]; //   1KB engine B palette
uint8_t vramA[0x20000];  // 128KB VRAM block A
uint8_t vramB[0x20000];  // 128KB VRAM block B
uint8_t vramC[0x20000];  // 128KB VRAM block C
uint8_t vramD[0x20000];  // 128KB VRAM block D
uint8_t vramE[0x10000];  //  64KB VRAM block E
uint8_t vramF[0x4000];   //  16KB VRAM block F
uint8_t vramG[0x4000];   //  16KB VRAM block G
uint8_t vramH[0x8000];   //  32KB VRAM block H
uint8_t vramI[0x4000];   //  16KB VRAM block I
uint8_t oamA[0x400];     //   1KB engine A OAM
uint8_t oamB[0x400];     //   1KB engine B OAM

uint16_t wramOffset9 = 0x0000; uint16_t wramSize9 = 0x0000;
uint16_t wramOffset7 = 0x0000; uint16_t wramSize7 = 0x8000;

bool     vramEnables[9];
uint32_t vramBases[9];

uint32_t dmasad9[4], dmasad7[4]; // DMA source addresss
uint32_t dmadad9[4], dmadad7[4]; // DMA destination address
uint32_t dmacnt9[4], dmacnt7[4]; // DMA control
uint32_t dmafill[4];             // DMA fill data

uint32_t keyinput = 0x03FF; // Key status
uint32_t extkeyin = 0x007F; // Key X/Y input

uint32_t ipcsync9, ipcsync7; // IPC synchronize
uint32_t ime9,     ime7;     // Interrupt master enable
uint32_t ie9,      ie7;      // Interrupt enable
uint32_t if9,      if7;      // Interrupt request flags

uint32_t wramcnt = 0x03; // WRAM bank control

uint32_t dispcntA;  // Engine A display control
uint32_t dispstat;  // General LCD status
uint32_t bgcntA[4]; // Engine A background control
uint32_t vcount;    // Vertical counter
uint32_t powcnt1;   // Graphics power control
uint32_t dispcntB;  // Engine B display control
uint32_t bgcntB[4]; // Engine B background control

void dmaTransfer(interpreter::Cpu *cpu, uint32_t dmasad, uint32_t dmadad, uint32_t *dmacnt)
{
    uint8_t mode = (*dmacnt & 0x38000000) >> 27;

    if (mode == 0) // Start immediately
    {
        uint8_t dstAddrCnt = (*dmacnt & 0x00600000) >> 21;
        uint8_t srcAddrCnt = (*dmacnt & 0x01800000) >> 23;
        uint32_t size = (*dmacnt & 0x001FFFFF);

        if (*dmacnt & BIT(26)) // 32-bit
        {
            for (int i = 0; i < size; i++)
            {
                write<uint32_t>(cpu, dmadad, read<uint32_t>(cpu, dmasad));

                if (dstAddrCnt == 0 || dstAddrCnt == 3) // Destination increment
                    dmadad += 4;
                else if (dstAddrCnt == 1) // Destination decrement
                    dmadad -= 4;

                if (srcAddrCnt == 0) // Source increment
                    dmasad += 4;
                else if (srcAddrCnt == 1) // Source decrement
                    dmasad -= 4;
            }
        }
        else // 16-bit
        {
            for (int i = 0; i < size; i++)
            {
                write<uint16_t>(cpu, dmadad, read<uint16_t>(cpu, dmasad));

                if (dstAddrCnt == 0 || dstAddrCnt == 3) // Destination increment
                    dmadad += 2;
                else if (dstAddrCnt == 1) // Destination decrement
                    dmadad -= 2;

                if (srcAddrCnt == 0) // Source increment
                    dmasad += 2;
                else if (srcAddrCnt == 1) // Source decrement
                    dmasad -= 2;
            }
        }
    }
    else
    {
        printf("Unknown ARM%d DMA transfer mode: %d\n", cpu->type, mode);
    }

    *dmacnt &= ~BIT(31);
}

void *vramMap(uint32_t address)
{
    if (address >= 0x5000000 && address < 0x5000400) // 1KB engine A palette
        return &paletteA[address - 0x5000000];
    else if (address >= 0x5000400 && address < 0x5000800) // 1KB engine B palette
        return &paletteB[address - 0x5000400];
    else if (address >= 0x5000800 && address < 0x6000000) // Palette mirror
        return vramMap(0x5000000 + address % 0x800);
    else if (vramEnables[0] && address >= vramBases[0] && address < vramBases[0] + 0x20000) // 128KB VRAM block A
        return &vramA[address - vramBases[0]];
    else if (vramEnables[1] && address >= vramBases[1] && address < vramBases[1] + 0x20000) // 128KB VRAM block B
        return &vramB[address - vramBases[1]];
    else if (vramEnables[2] && address >= vramBases[2] && address < vramBases[2] + 0x20000) // 128KB VRAM block C
        return &vramC[address - vramBases[2]];
    else if (vramEnables[3] && address >= vramBases[3] && address < vramBases[3] + 0x20000) // 128KB VRAM block D
        return &vramD[address - vramBases[3]];
    else if (vramEnables[4] && address >= vramBases[4] && address < vramBases[4] + 0x10000) // 64KB VRAM block E
        return &vramE[address - vramBases[4]];
    else if (vramEnables[5] && address >= vramBases[5] && address < vramBases[5] + 0x4000) // 16KB VRAM block F
        return &vramF[address - vramBases[5]];
    else if (vramEnables[6] && address >= vramBases[6] && address < vramBases[6] + 0x4000) // 16KB VRAM block G
        return &vramG[address - vramBases[6]];
    else if (vramEnables[7] && address >= vramBases[7] && address < vramBases[7] + 0x8000) // 32KB VRAM block H
        return &vramH[address - vramBases[7]];
    else if (vramEnables[8] && address >= vramBases[8] && address < vramBases[8] + 0x4000) // 16KB VRAM block I
        return &vramI[address - vramBases[8]];
    else if (address >= 0x7000000 && address < 0x7000400) // 1KB engine A OAM
        return &oamA[address - 0x7000000];
    else if (address >= 0x7000400 && address < 0x7000800) // 1KB engine B OAM
        return &oamB[address - 0x7000400];
    else if (address >= 0x7000800 && address < 0x8000000) // OAM mirror
        return vramMap(0x7000000 + address % 0x800);
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
        return &ram[address % 0x400000];
    else if (address >= 0x3000000 && address < 0x4000000 && wramSize9 != 0) // 32KB shared WRAM
        return &wram[wramOffset9 + address % wramSize9];
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
        return &ram[address % 0x400000];
    else if (address >= 0x3000000 && address < 0x3800000 && wramSize7 != 0) // 32KB shared WRAM
        return &wram[wramOffset7 + address % wramSize7];
    else if (address >= 0x3000000 && address < 0x4000000) // 64KB ARM7 WRAM
        return &wram7[address % 0x10000];
    else
        return nullptr;
}

uint32_t ioReadMap9(uint32_t address)
{
    switch (address)
    {
        case 0x4000000: return dispcntA;   // DISPCNT_A
        case 0x4000004: return dispstat;   // DISPSTAT
        case 0x4000008: return bgcntA[0];  // BG0CNT_A
        case 0x400000A: return bgcntA[1];  // BG1CNT_A
        case 0x400000C: return bgcntA[2];  // BG2CNT_A
        case 0x400000E: return bgcntA[3];  // BG3CNT_A
        case 0x4000006: return vcount;     // VCOUNT
        case 0x40000B0: return dmasad9[0]; // DMA0SAD_9
        case 0x40000B4: return dmadad9[0]; // DMA0DAD_9
        case 0x40000B8: return dmacnt9[0]; // DMA0CNT_9
        case 0x40000BC: return dmasad9[1]; // DMA1SAD_9
        case 0x40000C0: return dmadad9[1]; // DMA1DAD_9
        case 0x40000C4: return dmacnt9[1]; // DMA1CNT_9
        case 0x40000C8: return dmasad9[2]; // DMA2SAD_9
        case 0x40000CC: return dmadad9[2]; // DMA2DAD_9
        case 0x40000D0: return dmacnt9[2]; // DMA2CNT_9
        case 0x40000D4: return dmasad9[3]; // DMA3SAD_9
        case 0x40000D8: return dmadad9[3]; // DMA3DAD_9
        case 0x40000DC: return dmacnt9[3]; // DMA3CNT_9
        case 0x40000E0: return dmafill[0]; // DMA0FILL
        case 0x40000E4: return dmafill[1]; // DMA1FILL
        case 0x40000E8: return dmafill[2]; // DMA2FILL
        case 0x40000EC: return dmafill[3]; // DMA3FILL
        case 0x4000130: return keyinput;   // KEYINPUT
        case 0x4000180: return ipcsync9;   // IPCSYNC_9
        case 0x4000208: return ime9;       // IME_9
        case 0x4000210: return ie9;        // IE_9
        case 0x4000214: return if9;        // IF_9
        case 0x4000247: return wramcnt;    // WRAMCNT
        case 0x4000304: return powcnt1;    // POWCNT1
        case 0x4001000: return dispcntB;   // DISPCNT_B
        case 0x4001008: return bgcntB[0];  // BG0CNT_B
        case 0x400100A: return bgcntB[1];  // BG1CNT_B
        case 0x400100C: return bgcntB[2];  // BG2CNT_B
        case 0x400100E: return bgcntB[3];  // BG3CNT_B
        default: printf("Unknown ARM9 I/O read: 0x%X\n", address); return 0;
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

        case 0x4000008: // BG0CNT_A
            *(T*)&bgcntA[0] = value & 0xFFFF;
            break;

        case 0x400000A: // BG1CNT_A
            *(T*)&bgcntA[1] = value & 0xFFFF;
            break;

        case 0x400000C: // BG2CNT_A
            *(T*)&bgcntA[2] = value & 0xFFFF;
            break;

        case 0x400000E: // BG3CNT_A
            *(T*)&bgcntA[3] = value & 0xFFFF;
            break;

        case 0x40000B0: // DMA0SAD_9
            *(T*)&dmasad9[0] = value & 0x0FFFFFFF;
            break;

        case 0x40000B4: // DMA0DAD_9
            *(T*)&dmadad9[0] = value & 0x0FFFFFFF;
            break;

        case 0x40000B8: // DMA0CNT_9
            *(T*)&dmacnt9[0] = value;
            if (dmacnt9[0] & BIT(31))
                dmaTransfer(&core::arm9, dmasad9[0], dmadad9[0], &dmacnt9[0]);
            break;

        case 0x40000BC: // DMA1SAD_9
            *(T*)&dmasad9[1] = value & 0x0FFFFFFF;
            break;

        case 0x40000C0: // DMA1DAD_9
            *(T*)&dmadad9[1] = value & 0x0FFFFFFF;
            break;

        case 0x40000C4: // DMA1CNT_9
            *(T*)&dmacnt9[1] = value;
            if (dmacnt9[1] & BIT(31))
                dmaTransfer(&core::arm9, dmasad9[1], dmadad9[1], &dmacnt9[1]);
            break;

        case 0x40000C8: // DMA2SAD_9
            *(T*)&dmasad9[2] = value & 0x0FFFFFFF;
            break;

        case 0x40000CC: // DMA2DAD_9
            *(T*)&dmadad9[2] = value & 0x0FFFFFFF;
            break;

        case 0x40000D0: // DMA2CNT_9
            *(T*)&dmacnt9[2] = value;
            if (dmacnt9[2] & BIT(31))
                dmaTransfer(&core::arm9, dmasad9[2], dmadad9[2], &dmacnt9[2]);
            break;

        case 0x40000D4: // DMA3SAD_9
            *(T*)&dmasad9[3] = value & 0x0FFFFFFF;
            break;

        case 0x40000D8: // DMA3DAD_9
            *(T*)&dmadad9[3] = value & 0x0FFFFFFF;
            break;

        case 0x40000DC: // DMA3CNT_9
            *(T*)&dmacnt9[3] = value;
            if (dmacnt9[3] & BIT(31))
                dmaTransfer(&core::arm9, dmasad9[3], dmadad9[3], &dmacnt9[3]);
            break;

        case 0x40000E0: // DMA0FILL
            *(T*)&dmafill[0] = value;
            break;

        case 0x40000E4: // DMA1FILL
            *(T*)&dmafill[1] = value;
            break;

        case 0x40000E8: // DMA2FILL
            *(T*)&dmafill[2] = value;
            break;

        case 0x40000EC: // DMA3FILL
            *(T*)&dmafill[3] = value;
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
            vramEnables[0] = (value & BIT(7));
            if (vramEnables[0])
            {
                uint8_t mst = (value & 0x03);
                uint8_t offset = (value & 0x18) >> 3;
                switch (mst)
                {
                    case 0x0: vramBases[0] = 0x6800000;                               break;
                    case 0x1: vramBases[0] = 0x6000000 + 0x20000 * offset;            break;
                    case 0x2: vramBases[0] = 0x6400000 + 0x20000 * (offset & BIT(0)); break;
                    default: vramEnables[0] = false; printf("Unknown VRAM A MST: %d\n", mst); break;
                }
            }
            break;

        case 0x4000241: // VRAMCNT_B
            vramEnables[1] = (value & BIT(7));
            if (vramEnables[1])
            {
                uint8_t mst = (value & 0x03);
                uint8_t offset = (value & 0x18) >> 3;
                switch (mst)
                {
                    case 0x0: vramBases[1] = 0x6820000;                               break;
                    case 0x1: vramBases[1] = 0x6000000 + 0x20000 * offset;            break;
                    case 0x2: vramBases[1] = 0x6400000 + 0x20000 * (offset & BIT(0)); break;
                    default: vramEnables[1] = false; printf("Unknown VRAM B MST: %d\n", mst); break;
                }
            }
            break;

        case 0x4000242: // VRAMCNT_C
            vramEnables[2] = (value & BIT(7));
            if (vramEnables[2])
            {
                uint8_t mst = (value & 0x07);
                uint8_t offset = (value & 0x18) >> 3;
                switch (mst)
                {
                    case 0x0: vramBases[2] = 0x6840000;                    break;
                    case 0x1: vramBases[2] = 0x6000000 + 0x20000 * offset; break;
                    case 0x4: vramBases[2] = 0x6200000;                    break;
                    default: vramEnables[2] = false; printf("Unknown VRAM C MST: %d\n", mst); break;
                }
            }
            break;

        case 0x4000243: // VRAMCNT_D
            vramEnables[3] = (value & BIT(7));
            if (vramEnables[3])
            {
                uint8_t mst = (value & 0x07);
                uint8_t offset = (value & 0x18) >> 3;
                switch (mst)
                {
                    case 0x0: vramBases[3] = 0x6860000;                    break;
                    case 0x1: vramBases[3] = 0x6000000 + 0x20000 * offset; break;
                    case 0x4: vramBases[3] = 0x6600000;                    break;
                    default: vramEnables[3] = false; printf("Unknown VRAM D MST: %d\n", mst); break;
                }
            }
            break;

        case 0x4000244: // VRAMCNT_E
            vramEnables[4] = (value & BIT(7));
            if (vramEnables[4])
            {
                uint8_t mst = (value & 0x07);
                switch (mst)
                {
                    case 0x0: vramBases[4] = 0x6880000;                    break;
                    case 0x1: vramBases[4] = 0x6000000;                    break;
                    case 0x2: vramBases[4] = 0x6400000;                    break;
                    default: vramEnables[4] = false; printf("Unknown VRAM E MST: %d\n", mst); break;
                }
            }
            break;

        case 0x4000245: // VRAMCNT_F
            vramEnables[5] = (value & BIT(7));
            if (vramEnables[5])
            {
                uint8_t mst = (value & 0x07);
                uint8_t offset = (value & 0x18) >> 3;
                switch (mst)
                {
                    case 0x0: vramBases[5] = 0x6890000;                                                           break;
                    case 0x1: vramBases[5] = 0x6000000 + 0x8000 * (offset & BIT(1)) + 0x4000 * (offset & BIT(0)); break;
                    case 0x2: vramBases[5] = 0x6400000 + 0x8000 * (offset & BIT(1)) + 0x4000 * (offset & BIT(0)); break;
                    default: vramEnables[5] = false; printf("Unknown VRAM F MST: %d\n", mst); break;
                }
            }
            break;

        case 0x4000246: // VRAMCNT_G
            vramEnables[6] = (value & BIT(7));
            if (vramEnables[6])
            {
                uint8_t mst = (value & 0x07);
                uint8_t offset = (value & 0x18) >> 3;
                switch (mst)
                {
                    case 0x0: vramBases[6] = 0x6894000;                                                           break;
                    case 0x1: vramBases[6] = 0x6000000 + 0x8000 * (offset & BIT(1)) + 0x4000 * (offset & BIT(0)); break;
                    case 0x2: vramBases[6] = 0x6400000 + 0x8000 * (offset & BIT(1)) + 0x4000 * (offset & BIT(0)); break;
                    default: vramEnables[6] = false; printf("Unknown VRAM G MST: %d\n", mst); break;
                }
            }
            break;

        case 0x4000247: // WRAMCNT
            *(T*)&wramcnt = value & 0x03;
            switch (wramcnt)
            {
                case 0x0: wramOffset9 = 0x0000; wramSize9 = 0x8000; wramOffset7 = 0x0000; wramSize7 = 0x0000; break;
                case 0x1: wramOffset9 = 0x4000; wramSize9 = 0x4000; wramOffset7 = 0x0000; wramSize7 = 0x4000; break;
                case 0x2: wramOffset9 = 0x0000; wramSize9 = 0x4000; wramOffset7 = 0x4000; wramSize7 = 0x4000; break;
                case 0x3: wramOffset9 = 0x0000; wramSize9 = 0x0000; wramOffset7 = 0x0000; wramSize7 = 0x8000; break;
            }
            break;

        case 0x4000248: // VRAMCNT_H
            vramEnables[7] = (value & BIT(7));
            if (vramEnables[7])
            {
                uint8_t mst = (value & 0x03);
                switch (mst)
                {
                    case 0x0: vramBases[7] = 0x6898000; break;
                    case 0x1: vramBases[7] = 0x6200000; break;
                    default: vramEnables[7] = false; printf("Unknown VRAM H MST: %d\n", mst); break;
                }
            }
            break;

        case 0x4000249: // VRAMCNT_I
            vramEnables[8] = (value & BIT(7));
            if (vramEnables[8])
            {
                uint8_t mst = (value & 0x03);
                switch (mst)
                {
                    case 0x0: vramBases[8] = 0x68A0000; break;
                    case 0x1: vramBases[8] = 0x6208000; break;
                    case 0x2: vramBases[8] = 0x6600000; break;
                    default: vramEnables[8] = false; printf("Unknown VRAM I MST: %d\n", mst); break;
                }
            }
            break;

        case 0x4000304: // POWCNT1
            *(T*)&powcnt1 = (value & 0x820F);
            break;

        case 0x4001000: // DISPCNT_B
            *(T*)&dispcntB = (value & 0xC0B1FFF7);
            break;

        case 0x4001008: // BG0CNT_B
            *(T*)&bgcntB[0] = value & 0xFFFF;
            break;

        case 0x400100A: // BG1CNT_B
            *(T*)&bgcntB[1] = value & 0xFFFF;
            break;

        case 0x400100C: // BG2CNT_B
            *(T*)&bgcntB[2] = value & 0xFFFF;
            break;

        case 0x400100E: // BG3CNT_B
            *(T*)&bgcntB[3] = value & 0xFFFF;
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
        case 0x4000004: return dispstat;   // DISPSTAT
        case 0x4000006: return vcount;     // VCOUNT
        case 0x40000B0: return dmasad7[0]; // DMA0SAD_7
        case 0x40000B4: return dmadad7[0]; // DMA0DAD_7
        case 0x40000B8: return dmacnt7[0]; // DMA0CNT_7
        case 0x40000BC: return dmasad7[1]; // DMA1SAD_7
        case 0x40000C0: return dmadad7[1]; // DMA1DAD_7
        case 0x40000C4: return dmacnt7[1]; // DMA1CNT_7
        case 0x40000C8: return dmasad7[2]; // DMA2SAD_7
        case 0x40000CC: return dmadad7[2]; // DMA2DAD_7
        case 0x40000D0: return dmacnt7[2]; // DMA2CNT_7
        case 0x40000D4: return dmasad7[3]; // DMA3SAD_7
        case 0x40000D8: return dmadad7[3]; // DMA3DAD_7
        case 0x40000DC: return dmacnt7[3]; // DMA3CNT_7
        case 0x4000130: return keyinput;   // KEYINPUT
        case 0x4000136: return extkeyin;   // EXTKEYIN
        case 0x4000180: return ipcsync7;   // IPCSYNC_7
        case 0x4000208: return ime7;       // IME_7
        case 0x4000210: return ie7;        // IE_7
        case 0x4000214: return if7;        // IF_7
        case 0x4000241: return wramcnt;    // WRAMSTAT
        default: printf("Unknown ARM7 I/O read: 0x%X\n", address); return 0;
    }
}

template <typename T> void ioWriteMap7(uint32_t address, T value)
{
    switch (address)
    {
        case 0x4000004: // DISPSTAT
            *(T*)&dispstat = (value & 0xFFB8) | (dispstat & 0x0007);
            break;

        case 0x40000B0: // DMA0SAD_7
            *(T*)&dmasad7[0] = value & 0x07FFFFFF;
            break;

        case 0x40000B4: // DMA0DAD_7
            *(T*)&dmadad7[0] = value & 0x07FFFFFF;
            break;

        case 0x40000B8: // DMA0CNT_7
            *(T*)&dmacnt7[0] = value & 0xF7E03FFF;
            if (dmacnt7[0] & BIT(31))
                dmaTransfer(&core::arm7, dmasad7[0], dmadad7[0], &dmacnt7[0]);
            break;

        case 0x40000BC: // DMA1SAD_7
            *(T*)&dmasad7[1] = value & 0x07FFFFFF;
            break;

        case 0x40000C0: // DMA1DAD_7
            *(T*)&dmadad7[1] = value & 0x07FFFFFF;
            break;

        case 0x40000C4: // DMA1CNT_7
            *(T*)&dmacnt7[1] = value & 0xF7E03FFF;
            if (dmacnt7[1] & BIT(31))
                dmaTransfer(&core::arm7, dmasad7[1], dmadad7[1], &dmacnt7[1]);
            break;

        case 0x40000C8: // DMA2SAD_7
            *(T*)&dmasad7[2] = value & 0x07FFFFFF;
            break;

        case 0x40000CC: // DMA2DAD_7
            *(T*)&dmadad7[2] = value & 0x07FFFFFF;
            break;

        case 0x40000D0: // DMA2CNT_7
            *(T*)&dmacnt7[2] = value & 0xF7E03FFF;
            if (dmacnt7[2] & BIT(31))
                dmaTransfer(&core::arm7, dmasad7[2], dmadad7[2], &dmacnt7[2]);
            break;

        case 0x40000D4: // DMA3SAD_7
            *(T*)&dmasad7[3] = value & 0x07FFFFFF;
            break;

        case 0x40000D8: // DMA3DAD_7
            *(T*)&dmadad7[3] = value & 0x07FFFFFF;
            break;

        case 0x40000DC: // DMA3CNT_7
            *(T*)&dmacnt7[3] = value & 0xF7E0FFFF;
            if (dmacnt7[3] & BIT(31))
                dmaTransfer(&core::arm7, dmasad7[3], dmadad7[3], &dmacnt7[3]);
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
                case 0x1: printf("Unhandled request: GBA mode\n"); break;
                case 0x2: core::arm7.halt = true; break;
                case 0x3: printf("Unhandled request: Sleep mode\n"); break;
            }
            break;

        default:
            printf("Unknown ARM7 I/O write: 0x%X\n", address);
            break;
    }
}

template int8_t   read(interpreter::Cpu *cpu, uint32_t address);
template int16_t  read(interpreter::Cpu *cpu, uint32_t address);
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
