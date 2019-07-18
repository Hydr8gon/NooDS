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

#include "memory.h"
#include "core.h"
#include "cp15.h"
#include "memory_transfer.h"

namespace memory
{

uint8_t firmware[0x40000];
uint8_t *rom;

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

std::queue<uint32_t> fifo9, fifo7;

uint16_t wramOffset9; uint16_t wramSize9;
uint16_t wramOffset7; uint16_t wramSize7;

bool     vramMapped[9];
uint32_t vramBases[9];

uint16_t *extPalettesA[6];
uint16_t *extPalettesB[4];

uint8_t ioData9[0x2000], ioMask9[0x2000], ioWriteMask9[0x2000];
uint8_t ioData7[0x2000], ioMask7[0x2000], ioWriteMask7[0x2000];

uint16_t *dispstat = (uint16_t*)&ioData9[0x004];
uint16_t *vcount   = (uint16_t*)&ioData9[0x006];
uint16_t *powcnt1  = (uint16_t*)&ioData9[0x304];

uint32_t *dispcntA = (uint32_t*)&ioData9[0x000], *dispcntB = (uint32_t*)&ioData9[0x1000];
uint16_t *bg0cntA  = (uint16_t*)&ioData9[0x008], *bg0cntB  = (uint16_t*)&ioData9[0x1008];
uint16_t *bg1cntA  = (uint16_t*)&ioData9[0x00A], *bg1cntB  = (uint16_t*)&ioData9[0x100A];
uint16_t *bg2cntA  = (uint16_t*)&ioData9[0x00C], *bg2cntB  = (uint16_t*)&ioData9[0x100C];
uint16_t *bg3cntA  = (uint16_t*)&ioData9[0x00E], *bg3cntB  = (uint16_t*)&ioData9[0x100E];
uint16_t *bg0hofsA = (uint16_t*)&ioData9[0x010], *bg0hofsB = (uint16_t*)&ioData9[0x1010];
uint16_t *bg0vofsA = (uint16_t*)&ioData9[0x012], *bg0vofsB = (uint16_t*)&ioData9[0x1012];
uint16_t *bg1hofsA = (uint16_t*)&ioData9[0x014], *bg1hofsB = (uint16_t*)&ioData9[0x1014];
uint16_t *bg1vofsA = (uint16_t*)&ioData9[0x016], *bg1vofsB = (uint16_t*)&ioData9[0x1016];
uint16_t *bg2hofsA = (uint16_t*)&ioData9[0x018], *bg2hofsB = (uint16_t*)&ioData9[0x1018];
uint16_t *bg2vofsA = (uint16_t*)&ioData9[0x01A], *bg2vofsB = (uint16_t*)&ioData9[0x101A];
uint16_t *bg3hofsA = (uint16_t*)&ioData9[0x01C], *bg3hofsB = (uint16_t*)&ioData9[0x101C];
uint16_t *bg3vofsA = (uint16_t*)&ioData9[0x01E], *bg3vofsB = (uint16_t*)&ioData9[0x101E];

uint32_t *dma0sad9 = (uint32_t*)&ioData9[0x0B0], *dma0sad7 = (uint32_t*)&ioData7[0x0B0];
uint32_t *dma0dad9 = (uint32_t*)&ioData9[0x0B4], *dma0dad7 = (uint32_t*)&ioData7[0x0B4];
uint32_t *dma0cnt9 = (uint32_t*)&ioData9[0x0B8], *dma0cnt7 = (uint32_t*)&ioData7[0x0B8];
uint32_t *dma1sad9 = (uint32_t*)&ioData9[0x0BC], *dma1sad7 = (uint32_t*)&ioData7[0x0BC];
uint32_t *dma1dad9 = (uint32_t*)&ioData9[0x0C0], *dma1dad7 = (uint32_t*)&ioData7[0x0C0];
uint32_t *dma1cnt9 = (uint32_t*)&ioData9[0x0C4], *dma1cnt7 = (uint32_t*)&ioData7[0x0C4];
uint32_t *dma2sad9 = (uint32_t*)&ioData9[0x0C8], *dma2sad7 = (uint32_t*)&ioData7[0x0C8];
uint32_t *dma2dad9 = (uint32_t*)&ioData9[0x0CC], *dma2dad7 = (uint32_t*)&ioData7[0x0CC];
uint32_t *dma2cnt9 = (uint32_t*)&ioData9[0x0D0], *dma2cnt7 = (uint32_t*)&ioData7[0x0D0];
uint32_t *dma3sad9 = (uint32_t*)&ioData9[0x0D4], *dma3sad7 = (uint32_t*)&ioData7[0x0D4];
uint32_t *dma3dad9 = (uint32_t*)&ioData9[0x0D8], *dma3dad7 = (uint32_t*)&ioData7[0x0D8];
uint32_t *dma3cnt9 = (uint32_t*)&ioData9[0x0DC], *dma3cnt7 = (uint32_t*)&ioData7[0x0DC];

uint16_t *tm0count9 = (uint16_t*)&ioData9[0x100], *tm0count7 = (uint16_t*)&ioData7[0x100];
uint16_t *tm0cnt9   = (uint16_t*)&ioData9[0x102], *tm0cnt7   = (uint16_t*)&ioData7[0x102];
uint16_t *tm1count9 = (uint16_t*)&ioData9[0x104], *tm1count7 = (uint16_t*)&ioData7[0x104];
uint16_t *tm1cnt9   = (uint16_t*)&ioData9[0x106], *tm1cnt7   = (uint16_t*)&ioData7[0x106];
uint16_t *tm2count9 = (uint16_t*)&ioData9[0x108], *tm2count7 = (uint16_t*)&ioData7[0x108];
uint16_t *tm2cnt9   = (uint16_t*)&ioData9[0x10A], *tm2cnt7   = (uint16_t*)&ioData7[0x10A];
uint16_t *tm3count9 = (uint16_t*)&ioData9[0x10C], *tm3count7 = (uint16_t*)&ioData7[0x10C];
uint16_t *tm3cnt9   = (uint16_t*)&ioData9[0x10E], *tm3cnt7   = (uint16_t*)&ioData7[0x10E];

uint16_t *keyinput = (uint16_t*)&ioData9[0x130];
uint16_t *extkeyin = (uint16_t*)&ioData7[0x136];

uint16_t *ipcsync9     = (uint16_t*)&ioData9[0x180], *ipcsync7     = (uint16_t*)&ioData7[0x180];
uint16_t *ipcfifocnt9  = (uint16_t*)&ioData9[0x184], *ipcfifocnt7  = (uint16_t*)&ioData7[0x184];
uint32_t *ipcfifosend9 = (uint32_t*)&ioData9[0x188], *ipcfifosend7 = (uint32_t*)&ioData7[0x188];

uint16_t *auxspicnt9 = (uint16_t*)&ioData9[0x1A0], *auxspicnt7 = (uint16_t*)&ioData7[0x1A0];
uint32_t *romctrl9   = (uint32_t*)&ioData9[0x1A4], *romctrl7   = (uint32_t*)&ioData7[0x1A4];
uint8_t  *romcmdout9 =            &ioData9[0x1A8], *romcmdout7 =            &ioData7[0x1A8];

uint16_t *spicnt  = (uint16_t*)&ioData7[0x1C0];
uint16_t *spidata = (uint16_t*)&ioData7[0x1C2];

uint16_t *ime9 = (uint16_t*)&ioData9[0x208], *ime7 = (uint16_t*)&ioData7[0x208];
uint32_t *ie9  = (uint32_t*)&ioData9[0x210], *ie7  = (uint32_t*)&ioData7[0x210];
uint32_t *irf9 = (uint32_t*)&ioData9[0x214], *irf7 = (uint32_t*)&ioData7[0x214];

void *vramMap(uint32_t address)
{
    if (address >= 0x5000000 && address < 0x5000400) // 1KB engine A palette
        return &paletteA[address - 0x5000000];
    else if (address >= 0x5000400 && address < 0x5000800) // 1KB engine B palette
        return &paletteB[address - 0x5000400];
    else if (address >= 0x5000800 && address < 0x6000000) // Palette mirror
        return vramMap(0x5000000 + address % 0x800);
    else if (vramMapped[0] && address >= vramBases[0] && address < vramBases[0] + 0x20000) // 128KB VRAM block A
        return &vramA[address - vramBases[0]];
    else if (vramMapped[1] && address >= vramBases[1] && address < vramBases[1] + 0x20000) // 128KB VRAM block B
        return &vramB[address - vramBases[1]];
    else if (vramMapped[2] && address >= vramBases[2] && address < vramBases[2] + 0x20000) // 128KB VRAM block C
        return &vramC[address - vramBases[2]];
    else if (vramMapped[3] && address >= vramBases[3] && address < vramBases[3] + 0x20000) // 128KB VRAM block D
        return &vramD[address - vramBases[3]];
    else if (vramMapped[4] && address >= vramBases[4] && address < vramBases[4] + 0x10000) // 64KB VRAM block E
        return &vramE[address - vramBases[4]];
    else if (vramMapped[5] && address >= vramBases[5] && address < vramBases[5] + 0x4000) // 16KB VRAM block F
        return &vramF[address - vramBases[5]];
    else if (vramMapped[6] && address >= vramBases[6] && address < vramBases[6] + 0x4000) // 16KB VRAM block G
        return &vramG[address - vramBases[6]];
    else if (vramMapped[7] && address >= vramBases[7] && address < vramBases[7] + 0x8000) // 32KB VRAM block H
        return &vramH[address - vramBases[7]];
    else if (vramMapped[8] && address >= vramBases[8] && address < vramBases[8] + 0x4000) // 16KB VRAM block I
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

template <typename T> T ioRead9(uint32_t address)
{
    uint32_t ioAddr = address - 0x4000000;

    if (ioAddr == 0x100000) // IPCFIFORECV
        return memory_transfer::fifoReceive(&interpreter::arm9, &interpreter::arm7);
    else if (ioAddr == 0x100010) // ROMDATAIN
        return memory_transfer::romTransfer(&interpreter::arm9);

    if (ioAddr >= sizeof(ioMask9) || !ioMask9[ioAddr])
    {
        printf("Unknown ARM9 I/O read: 0x%X\n", address);
        return 0;
    }

    return *(T*)&ioData9[ioAddr];
}

template <typename T> void ioWrite9(uint32_t address, T value)
{
    uint32_t ioAddr = address - 0x4000000;

    if (ioAddr >= sizeof(ioMask9) || !ioMask9[ioAddr])
    {
        printf("Unknown ARM9 I/O write: 0x%X\n", address);
        return;
    }

    *(T*)&ioData9[ioAddr] &= ~(*(T*)&ioWriteMask9[ioAddr]);
    *(T*)&ioData9[ioAddr] |= (value & *(T*)&ioWriteMask9[ioAddr]);

    for (int i = 0; i < sizeof(T); i++)
    {
        switch (ioAddr + i)
        {
            case 0x0BB: // DMA0CNT_9
                memory_transfer::dmaTransfer(&interpreter::arm9, 0);
                break;

            case 0x0C7: // DMA1CNT_9
                memory_transfer::dmaTransfer(&interpreter::arm9, 1);
                break;

            case 0x0D3: // DMA2CNT_9
                memory_transfer::dmaTransfer(&interpreter::arm9, 2);
                break;

            case 0x0DF: // DMA3CNT_9
                memory_transfer::dmaTransfer(&interpreter::arm9, 3);
                break;

            case 0x100: case 0x104: case 0x108: case 0x10C: // TMCOUNT_9
                ((uint8_t*)&interpreter::arm9.timerReloads[(ioAddr + i - 0x100) / 4])[0] = ((uint8_t*)&value)[i];
                break;

            case 0x101: case 0x105: case 0x109: case 0x10D: // TMCOUNT_9
                ((uint8_t*)&interpreter::arm9.timerReloads[(ioAddr + i - 0x101) / 4])[1] = ((uint8_t*)&value)[i];
                break;

            case 0x102: case 0x106: case 0x10A: case 0x10E: // TMCNT_9
                if (!(ioData9[ioAddr + i] & BIT(7)) && (((uint8_t*)&value)[i] & BIT(7)))
                    *interpreter::arm9.timerCounters[(ioAddr + i - 0x102) / 4] = interpreter::arm9.timerReloads[(ioAddr + i - 0x102) / 4];
                ioData9[ioAddr + i] &= ~BIT(7);
                ioData9[ioAddr + i] |= (((uint8_t*)&value)[i] & BIT(7));
                break;

            case 0x181: // IPCSYNC_9
                ((uint8_t*)ipcsync7)[0] = (((uint8_t*)&value)[i] & 0x0F);
                if ((((uint8_t*)&value)[i] & BIT(5)) && (*ipcsync7 & BIT(14))) // Remote IRQ
                    *interpreter::arm7.irf |= BIT(16);
                break;

            case 0x184: // IPCFIFOCNT_9
                if ((*ipcfifocnt9 & BIT(0)) && (((uint8_t*)&value)[i] & BIT(2)) && !(ioData9[0x184] & BIT(2)))
                    *interpreter::arm9.irf |= BIT(17); // Send FIFO empty IRQ
                ioData9[0x184] &= ~BIT(2);
                ioData9[0x184] |= (((uint8_t*)&value)[i] & BIT(2));
                if (((uint8_t*)&value)[i] & BIT(3))
                    memory_transfer::fifoClear(&interpreter::arm9, &interpreter::arm7);
                break;

            case 0x185: // IPCFIFOCNT_9
                if (!(*ipcfifocnt9 & BIT(8)) && (((uint8_t*)&value)[i] & BIT(2)) && !(ioData9[0x185] & BIT(2)))
                    *interpreter::arm9.irf |= BIT(18); // Receive FIFO not empty IRQ
                ioData9[0x185] &= ~BIT(2);
                ioData9[0x185] |= (((uint8_t*)&value)[i] & BIT(2));
                break;

            case 0x188: case 0x189: case 0x18A: case 0x18B: // IPCFIFOSEND_9
                memory_transfer::fifoSend(&interpreter::arm9, &interpreter::arm7);
                return;

            case 0x1A7: // ROMCTRL
                ioData9[0x1A7] |= (((uint8_t*)&value)[i] & BIT(5));
                memory_transfer::romTransferStart(&interpreter::arm9);
                break;

            case 0x214: case 0x215: case 0x216: case 0x217: // IF_9
                ioData9[ioAddr + i] &= ~((uint8_t*)&value)[i];
                break;

            case 0x240: // VRAMCNT_A
                if (((uint8_t*)&value)[i] & BIT(7))
                {
                    uint8_t mst = (((uint8_t*)&value)[i] & 0x03);
                    uint8_t ofs = (((uint8_t*)&value)[i] & 0x18) >> 3;
                    switch (mst)
                    {
                        case 0x0: vramMapped[0] = true; vramBases[0] = 0x6800000;                            break;
                        case 0x1: vramMapped[0] = true; vramBases[0] = 0x6000000 + 0x20000 * ofs;            break;
                        case 0x2: vramMapped[0] = true; vramBases[0] = 0x6400000 + 0x20000 * (ofs & BIT(0)); break;
                        case 0x3: vramMapped[0] = false; extPalettesA[ofs] = (uint16_t*)vramA;               break;
                    }
                }
                else
                {
                    vramMapped[0] = false;
                }
                break;

            case 0x241: // VRAMCNT_B
                if (((uint8_t*)&value)[i] & BIT(7))
                {
                    uint8_t mst = (((uint8_t*)&value)[i] & 0x03);
                    uint8_t ofs = (((uint8_t*)&value)[i] & 0x18) >> 3;
                    switch (mst)
                    {
                        case 0x0: vramMapped[1] = true; vramBases[1] = 0x6820000;                            break;
                        case 0x1: vramMapped[1] = true; vramBases[1] = 0x6000000 + 0x20000 * ofs;            break;
                        case 0x2: vramMapped[1] = true; vramBases[1] = 0x6400000 + 0x20000 * (ofs & BIT(0)); break;
                        case 0x3: vramMapped[1] = false; extPalettesA[ofs] = (uint16_t*)vramB;               break;
                    }
                }
                else
                {
                    vramMapped[1] = false;
                }
                break;

            case 0x242: // VRAMCNT_C
                if (((uint8_t*)&value)[i] & BIT(7))
                {
                    uint8_t mst = (((uint8_t*)&value)[i] & 0x07);
                    uint8_t ofs = (((uint8_t*)&value)[i] & 0x18) >> 3;
                    switch (mst)
                    {
                        case 0x0: vramMapped[2] = true; vramBases[2] = 0x6840000;                 break;
                        case 0x1: vramMapped[2] = true; vramBases[2] = 0x6000000 + 0x20000 * ofs; break;
                        case 0x3: vramMapped[2] = false; extPalettesA[ofs] = (uint16_t*)vramC;    break;
                        case 0x4: vramMapped[2] = true; vramBases[2] = 0x6200000;                 break;
                        default:  vramMapped[2] = false;                                          break;
                    }
                }
                else
                {
                    vramMapped[2] = false;
                }
                break;

            case 0x243: // VRAMCNT_D
                if (((uint8_t*)&value)[i] & BIT(7))
                {
                    uint8_t mst = (((uint8_t*)&value)[i] & 0x07);
                    uint8_t ofs = (((uint8_t*)&value)[i] & 0x18) >> 3;
                    switch (mst)
                    {
                        case 0x0: vramMapped[3] = true; vramBases[3] = 0x6860000;                 break;
                        case 0x1: vramMapped[3] = true; vramBases[3] = 0x6000000 + 0x20000 * ofs; break;
                        case 0x3: vramMapped[3] = false; extPalettesA[ofs] = (uint16_t*)vramD;    break;
                        case 0x4: vramMapped[3] = true; vramBases[3] = 0x6600000;                 break;
                        default:  vramMapped[3] = false;                                          break;
                    }
                }
                else
                {
                    vramMapped[3] = false;
                }
                break;

            case 0x244: // VRAMCNT_E
                if (((uint8_t*)&value)[i] & BIT(7))
                {
                    uint8_t mst = (((uint8_t*)&value)[i] & 0x07);
                    switch (mst)
                    {
                        case 0x0: vramMapped[4] = true; vramBases[4] = 0x6880000;                                                       break;
                        case 0x1: vramMapped[4] = true; vramBases[4] = 0x6000000;                                                       break;
                        case 0x2: vramMapped[4] = true; vramBases[4] = 0x6400000;                                                       break;
                        case 0x3: vramMapped[4] = false; for (int i = 0; i < 4; i++) extPalettesA[i] = (uint16_t*)vramE;                break;
                        case 0x4: vramMapped[4] = false; for (int i = 0; i < 4; i++) extPalettesA[i] = (uint16_t*)(vramE + 0x2000 * i); break;
                        default:  vramMapped[4] = false;                                                                                break;
                    }
                }
                else
                {
                    vramMapped[4] = false;
                }
                break;

            case 0x245: // VRAMCNT_F
                if (((uint8_t*)&value)[i] & BIT(7))
                {
                    uint8_t mst = (((uint8_t*)&value)[i] & 0x07);
                    uint8_t ofs = (((uint8_t*)&value)[i] & 0x18) >> 3;
                    switch (mst)
                    {
                        case 0x0: vramMapped[5] = true; vramBases[5] = 0x6890000;                                                                            break;
                        case 0x1: vramMapped[5] = true; vramBases[5] = 0x6000000 + 0x8000 * (ofs & BIT(1)) + 0x4000 * (ofs & BIT(0));                        break;
                        case 0x2: vramMapped[5] = true; vramBases[5] = 0x6400000 + 0x8000 * (ofs & BIT(1)) + 0x4000 * (ofs & BIT(0));                        break;
                        case 0x3: vramMapped[5] = false; extPalettesA[(ofs & BIT(0)) + (ofs & BIT(1)) * 2] = (uint16_t*)vramF;                               break;
                        case 0x4: vramMapped[5] = false; for (int i = 0; i < 2; i++) extPalettesA[(ofs & BIT(0)) * 2 + i] = (uint16_t*)(vramF + 0x4000 * i); break;
                        case 0x5: vramMapped[5] = false; extPalettesA[0] = (uint16_t*)vramF;                                                                 break;
                        default:  vramMapped[5] = false;                                                                                                     break;
                    }
                }
                else
                {
                    vramMapped[5] = false;
                }
                break;

            case 0x246: // VRAMCNT_G
                if (((uint8_t*)&value)[i] & BIT(7))
                {
                    uint8_t mst = (((uint8_t*)&value)[i] & 0x07);
                    uint8_t ofs = (((uint8_t*)&value)[i] & 0x18) >> 3;
                    switch (mst)
                    {
                        case 0x0: vramMapped[6] = true; vramBases[6] = 0x6894000;                                                                            break;
                        case 0x1: vramMapped[6] = true; vramBases[6] = 0x6000000 + 0x8000 * (ofs & BIT(1)) + 0x4000 * (ofs & BIT(0));                        break;
                        case 0x2: vramMapped[6] = true; vramBases[6] = 0x6400000 + 0x8000 * (ofs & BIT(1)) + 0x4000 * (ofs & BIT(0));                        break;
                        case 0x3: vramMapped[6] = false; extPalettesA[(ofs & BIT(0)) + (ofs & BIT(1)) * 2] = (uint16_t*)vramG;                               break;
                        case 0x4: vramMapped[6] = false; for (int i = 0; i < 2; i++) extPalettesA[(ofs & BIT(0)) * 2 + i] = (uint16_t*)(vramG + 0x4000 * i); break;
                        case 0x5: vramMapped[6] = false; extPalettesA[0] = (uint16_t*)vramG;                                                                 break;
                        default:  vramMapped[6] = false;                                                                                                     break;
                    }
                }
                else
                {
                    vramMapped[6] = false;
                }
                break;

            case 0x247: // WRAMCNT
                switch (((uint8_t*)&value)[i] & 0x03)
                {
                    case 0x0: wramOffset9 = 0x0000; wramSize9 = 0x8000; wramOffset7 = 0x0000; wramSize7 = 0x0000; break;
                    case 0x1: wramOffset9 = 0x4000; wramSize9 = 0x4000; wramOffset7 = 0x0000; wramSize7 = 0x4000; break;
                    case 0x2: wramOffset9 = 0x0000; wramSize9 = 0x4000; wramOffset7 = 0x4000; wramSize7 = 0x4000; break;
                    case 0x3: wramOffset9 = 0x0000; wramSize9 = 0x0000; wramOffset7 = 0x0000; wramSize7 = 0x8000; break;
                }
                break;

            case 0x248: // VRAMCNT_H
                if (((uint8_t*)&value)[i] & BIT(7))
                {
                    uint8_t mst = (((uint8_t*)&value)[i] & 0x03);
                    switch (mst)
                    {
                        case 0x0: vramMapped[7] = true; vramBases[7] = 0x6898000;                                                       break;
                        case 0x1: vramMapped[7] = true; vramBases[7] = 0x6200000;                                                       break;
                        case 0x2: vramMapped[7] = false; for (int i = 0; i < 4; i++) extPalettesB[i] = (uint16_t*)(vramH + 0x2000 * i); break;
                        default:  vramMapped[7] = false;                                                                                break;
                    }
                }
                else
                {
                    vramMapped[7] = false;
                }
                break;

            case 0x249: // VRAMCNT_I
                if (((uint8_t*)&value)[i] & BIT(7))
                {
                    uint8_t mst = (((uint8_t*)&value)[i] & 0x03);
                    switch (mst)
                    {
                        case 0x0: vramMapped[8] = true; vramBases[8] = 0x68A0000;            break;
                        case 0x1: vramMapped[8] = true; vramBases[8] = 0x6208000;            break;
                        case 0x2: vramMapped[8] = true; vramBases[8] = 0x6600000;            break;
                        case 0x3: vramMapped[8] = false; extPalettesB[0] = (uint16_t*)vramI; break;
                        default:  vramMapped[8] = false;                                     break;
                    }
                }
                else
                {
                    vramMapped[8] = false;
                }
                break;

            case 0x300: // POSTFLG
                ioData9[0x300] |= (((uint8_t*)&value)[i] & BIT(0));
                break;
        }
    }
}

template <typename T> T ioRead7(uint32_t address)
{
    uint32_t ioAddr = address - 0x4000000;

    if (ioAddr == 0x100000) // IPCFIFORECV
        return memory_transfer::fifoReceive(&interpreter::arm7, &interpreter::arm9);
    else if (ioAddr == 0x100010) // ROMDATAIN
        return memory_transfer::romTransfer(&interpreter::arm7);

    if (ioAddr >= sizeof(ioMask7) || !ioMask7[ioAddr])
    {
        printf("Unknown ARM7 I/O read: 0x%X\n", address);
        return 0;
    }

    for (int i = 0; i < sizeof(T); i++)
    {
        switch (ioAddr + i)
        {
            case 0x004: case 0x005: // DISPSTAT
            case 0x006: case 0x007: // VCOUNT
            case 0x130:             // KEYINPUT
                ioData7[ioAddr + i] = ioData9[ioAddr + i];
                break;
        }
    }

    return *(T*)&ioData7[ioAddr];
}


template <typename T> void ioWrite7(uint32_t address, T value)
{
    uint32_t ioAddr = address - 0x4000000;

    if (ioAddr >= sizeof(ioMask7) || !ioMask7[ioAddr])
    {
        printf("Unknown ARM7 I/O write: 0x%X\n", address);
        return;
    }

    *(T*)&ioData7[ioAddr] &= ~(*(T*)&ioWriteMask7[ioAddr]);
    *(T*)&ioData7[ioAddr] |= (value & *(T*)&ioWriteMask7[ioAddr]);

    for (int i = 0; i < sizeof(T); i++)
    {
        switch (ioAddr + i)
        {
            case 0x004: case 0x005: // DISPSTAT
                ioData9[ioAddr + i] &= ~ioWriteMask9[ioAddr + i];
                ioData9[ioAddr + i] |= (((uint8_t*)&value)[i] & ioWriteMask9[ioAddr + i]);
                break;

            case 0x0BB: // DMA0CNT_7
                memory_transfer::dmaTransfer(&interpreter::arm7, 0);
                break;

            case 0x0C7: // DMA1CNT_7
                memory_transfer::dmaTransfer(&interpreter::arm7, 1);
                break;

            case 0x0D3: // DMA2CNT_7
                memory_transfer::dmaTransfer(&interpreter::arm7, 2);
                break;

            case 0x0DF: // DMA3CNT_7
                memory_transfer::dmaTransfer(&interpreter::arm7, 3);
                break;

            case 0x100: case 0x104: case 0x108: case 0x10C: // TMCOUNT_7
                ((uint8_t*)&interpreter::arm7.timerReloads[(ioAddr + i - 0x100) / 4])[0] = ((uint8_t*)&value)[i];
                break;

            case 0x101: case 0x105: case 0x109: case 0x10D: // TMCOUNT_7
                ((uint8_t*)&interpreter::arm7.timerReloads[(ioAddr + i - 0x101) / 4])[1] = ((uint8_t*)&value)[i];
                break;

            case 0x102: case 0x106: case 0x10A: case 0x10E: // TMCNT_7
                if (!(ioData7[ioAddr + i] & BIT(7)) && (((uint8_t*)&value)[i] & BIT(7)))
                    *interpreter::arm7.timerCounters[(ioAddr + i - 0x102) / 4] = interpreter::arm7.timerReloads[(ioAddr + i - 0x102) / 4];
                ioData7[ioAddr + i] &= ~BIT(7);
                ioData7[ioAddr + i] |= (((uint8_t*)&value)[i] & BIT(7));
                break;

            case 0x181: // IPCSYNC_7
                ((uint8_t*)ipcsync9)[0] = (((uint8_t*)&value)[i] & 0x0F);
                if ((((uint8_t*)&value)[i] & BIT(5)) && (*ipcsync9 & BIT(14))) // Remote IRQ
                    *interpreter::arm9.irf |= BIT(16);
                break;

            case 0x184: // IPCFIFOCNT_7
                if ((*ipcfifocnt7 & BIT(0)) && (((uint8_t*)&value)[i] & BIT(2)) && !(ioData7[0x184] & BIT(2)))
                    *interpreter::arm7.irf |= BIT(17); // Send FIFO empty IRQ
                ioData7[0x184] &= ~BIT(2);
                ioData7[0x184] |= (((uint8_t*)&value)[i] & BIT(2));
                if (((uint8_t*)&value)[i] & BIT(3)) // Clear FIFO
                    memory_transfer::fifoClear(&interpreter::arm7, &interpreter::arm9);
                break;

            case 0x185: // IPCFIFOCNT_7
                if (!(*ipcfifocnt7 & BIT(8)) && (((uint8_t*)&value)[i] & BIT(2)) && !(ioData7[0x185] & BIT(2)))
                    *interpreter::arm7.irf |= BIT(18); // Receive FIFO not empty IRQ
                ioData7[0x185] &= ~BIT(2);
                ioData7[0x185] |= (((uint8_t*)&value)[i] & BIT(2));
                break;

            case 0x188: case 0x189: case 0x18A: case 0x18B: // IPCFIFOSEND_7
                memory_transfer::fifoSend(&interpreter::arm7, &interpreter::arm9);
                return;

            case 0x1A7: // ROMCTRL
                ioData7[0x1A7] |= (((uint8_t*)&value)[i] & BIT(5));
                memory_transfer::romTransferStart(&interpreter::arm7);
                break;

            case 0x1C2: // SPIDATA
                memory_transfer::spiWrite(((uint8_t*)&value)[i]);
                break;

            case 0x214: case 0x215: case 0x216: case 0x217: // IF_7
                ioData7[ioAddr + i] &= ~((uint8_t*)&value)[i];
                break;

            case 0x300: // POSTFLG
                ioData7[0x300] |= (((uint8_t*)&value)[i] & BIT(0));
                break;

            case 0x301: // HALTCNT
                if (((value & 0xC0) >> 6) == 0x2)
                    interpreter::arm7.halt = true;
                break;
        }
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
        if (address >= 0x4000000 && address < 0x5000000) // I/O registers
        {
            return ioRead9<T>(address);
        }
        else
        {
            T *src = (T*)memoryMap9(address);
            if (src)
                return *src;
            else
                printf("Unmapped ARM9 memory read: 0x%X\n", address);
        }
    }
    else
    {
        if (address >= 0x4000000 && address < 0x5000000) // I/O registers
        {
            return ioRead7<T>(address);
        }
        else
        {
            T *src = (T*)memoryMap7(address);
            if (src)
                return *src;
            else
                printf("Unmapped ARM7 memory read: 0x%X\n", address);
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
        if (address >= 0x4000000 && address < 0x5000000) // I/O registers
        {
            ioWrite9<T>(address, value);
        }
        else
        {
            T *dst = (T*)memoryMap9(address);
            if (dst)
                *dst = value;
            else
                printf("Unmapped ARM9 memory write: 0x%X\n", address);
        }
    }
    else
    {
        if (address >= 0x4000000 && address < 0x5000000) // I/O registers
        {
            ioWrite7<T>(address, value);
        }
        else
        {
            T *dst = (T*)memoryMap7(address);
            if (dst)
                *dst = value;
            else
                printf("Unmapped ARM7 memory write: 0x%X\n", address);
        }
    }
}

void init()
{
    memset(ram,      0, sizeof(ram));
    memset(wram,     0, sizeof(wram));
    memset(instrTcm, 0, sizeof(instrTcm));
    memset(dataTcm,  0, sizeof(dataTcm));
    memset(bios9,    0, sizeof(bios9));
    memset(bios7,    0, sizeof(bios7));
    memset(wram7,    0, sizeof(wram7));

    memset(paletteA, 0, sizeof(paletteA));
    memset(paletteB, 0, sizeof(paletteB));
    memset(vramA,    0, sizeof(vramA));
    memset(vramB,    0, sizeof(vramB));
    memset(vramC,    0, sizeof(vramC));
    memset(vramD,    0, sizeof(vramD));
    memset(vramE,    0, sizeof(vramE));
    memset(vramF,    0, sizeof(vramF));
    memset(vramG,    0, sizeof(vramG));
    memset(vramH,    0, sizeof(vramH));
    memset(vramI,    0, sizeof(vramI));
    memset(oamA,     0, sizeof(oamA));
    memset(oamB,     0, sizeof(oamB));

    memset(vramMapped,   0, sizeof(vramMapped));
    memset(extPalettesA, 0, sizeof(extPalettesA));
    memset(extPalettesB, 0, sizeof(extPalettesB));

    memset(ioData9, 0, sizeof(ioData9));
    memset(ioData7, 0, sizeof(ioData7));

    while (!fifo9.empty()) fifo9.pop();
    while (!fifo7.empty()) fifo7.pop();

    *keyinput = 0x03FF;
    *extkeyin = 0x007F;

    *ipcfifocnt9 = 0x0101;
    *ipcfifocnt7 = 0x0101;

    *(uint32_t*)&ioMask9[0x000]  = 0xFFFFFFFF; *(uint32_t*)&ioWriteMask9[0x000]  = 0xFFFFFFFF; // DISPCNT_A
    *(uint16_t*)&ioMask9[0x004]  =     0xFFBF; *(uint16_t*)&ioWriteMask9[0x004]  =     0xFFB8; // DISPSTAT
    *(uint16_t*)&ioMask9[0x006]  =     0x01FF; *(uint16_t*)&ioWriteMask9[0x006]  =     0x0000; // VCOUNT
    *(uint16_t*)&ioMask9[0x008]  =     0xFFFF; *(uint16_t*)&ioWriteMask9[0x008]  =     0xFFFF; // BG0CNT_A
    *(uint16_t*)&ioMask9[0x00A]  =     0xFFFF; *(uint16_t*)&ioWriteMask9[0x00A]  =     0xFFFF; // BG1CNT_A
    *(uint16_t*)&ioMask9[0x00C]  =     0xFFFF; *(uint16_t*)&ioWriteMask9[0x00C]  =     0xFFFF; // BG2CNT_A
    *(uint16_t*)&ioMask9[0x00E]  =     0xFFFF; *(uint16_t*)&ioWriteMask9[0x00E]  =     0xFFFF; // BG3CNT_A
    *(uint16_t*)&ioMask9[0x010]  =     0x01FF; *(uint16_t*)&ioWriteMask9[0x010]  =     0x01FF; // BG0HOFS_A
    *(uint16_t*)&ioMask9[0x012]  =     0x01FF; *(uint16_t*)&ioWriteMask9[0x012]  =     0x01FF; // BG0VOFS_A
    *(uint16_t*)&ioMask9[0x014]  =     0x01FF; *(uint16_t*)&ioWriteMask9[0x014]  =     0x01FF; // BG1HOFS_A
    *(uint16_t*)&ioMask9[0x016]  =     0x01FF; *(uint16_t*)&ioWriteMask9[0x016]  =     0x01FF; // BG1VOFS_A
    *(uint16_t*)&ioMask9[0x018]  =     0x01FF; *(uint16_t*)&ioWriteMask9[0x018]  =     0x01FF; // BG2HOFS_A
    *(uint16_t*)&ioMask9[0x01A]  =     0x01FF; *(uint16_t*)&ioWriteMask9[0x01A]  =     0x01FF; // BG2VOFS_A
    *(uint16_t*)&ioMask9[0x01C]  =     0x01FF; *(uint16_t*)&ioWriteMask9[0x01C]  =     0x01FF; // BG3HOFS_A
    *(uint16_t*)&ioMask9[0x01E]  =     0x01FF; *(uint16_t*)&ioWriteMask9[0x01E]  =     0x01FF; // BG3VOFS_A
    *(uint32_t*)&ioMask9[0x0B0]  = 0x0FFFFFFF; *(uint32_t*)&ioWriteMask9[0x0B0]  = 0x0FFFFFFF; // DMA0SAD_9
    *(uint32_t*)&ioMask9[0x0B4]  = 0x0FFFFFFF; *(uint32_t*)&ioWriteMask9[0x0B4]  = 0x0FFFFFFF; // DMA0DAD_9
    *(uint32_t*)&ioMask9[0x0B8]  = 0xFFFFFFFF; *(uint32_t*)&ioWriteMask9[0x0B8]  = 0xFFFFFFFF; // DMA0CNT_9
    *(uint32_t*)&ioMask9[0x0BC]  = 0x0FFFFFFF; *(uint32_t*)&ioWriteMask9[0x0BC]  = 0x0FFFFFFF; // DMA1SAD_9
    *(uint32_t*)&ioMask9[0x0C0]  = 0x0FFFFFFF; *(uint32_t*)&ioWriteMask9[0x0C0]  = 0x0FFFFFFF; // DMA1DAD_9
    *(uint32_t*)&ioMask9[0x0C4]  = 0xFFFFFFFF; *(uint32_t*)&ioWriteMask9[0x0C4]  = 0xFFFFFFFF; // DMA1CNT_9
    *(uint32_t*)&ioMask9[0x0C8]  = 0x0FFFFFFF; *(uint32_t*)&ioWriteMask9[0x0C8]  = 0x0FFFFFFF; // DMA2SAD_9
    *(uint32_t*)&ioMask9[0x0CC]  = 0x0FFFFFFF; *(uint32_t*)&ioWriteMask9[0x0CC]  = 0x0FFFFFFF; // DMA2DAD_9
    *(uint32_t*)&ioMask9[0x0D0]  = 0xFFFFFFFF; *(uint32_t*)&ioWriteMask9[0x0D0]  = 0xFFFFFFFF; // DMA2CNT_9
    *(uint32_t*)&ioMask9[0x0D4]  = 0x0FFFFFFF; *(uint32_t*)&ioWriteMask9[0x0D4]  = 0x0FFFFFFF; // DMA3SAD_9
    *(uint32_t*)&ioMask9[0x0D8]  = 0x0FFFFFFF; *(uint32_t*)&ioWriteMask9[0x0D8]  = 0x0FFFFFFF; // DMA3DAD_9
    *(uint32_t*)&ioMask9[0x0DC]  = 0xFFFFFFFF; *(uint32_t*)&ioWriteMask9[0x0DC]  = 0xFFFFFFFF; // DMA3CNT_9
    *(uint32_t*)&ioMask9[0x0E0]  = 0xFFFFFFFF; *(uint32_t*)&ioWriteMask9[0x0E0]  = 0xFFFFFFFF; // DMA0FILL
    *(uint32_t*)&ioMask9[0x0E4]  = 0xFFFFFFFF; *(uint32_t*)&ioWriteMask9[0x0E4]  = 0xFFFFFFFF; // DMA1FILL
    *(uint32_t*)&ioMask9[0x0E8]  = 0xFFFFFFFF; *(uint32_t*)&ioWriteMask9[0x0E8]  = 0xFFFFFFFF; // DMA2FILL
    *(uint32_t*)&ioMask9[0x0EC]  = 0xFFFFFFFF; *(uint32_t*)&ioWriteMask9[0x0EC]  = 0xFFFFFFFF; // DMA3FILL
    *(uint16_t*)&ioMask9[0x100]  =     0xFFFF; *(uint16_t*)&ioWriteMask9[0x100]  =     0x0000; // TM0COUNT_9
    *(uint16_t*)&ioMask9[0x102]  =     0x00C7; *(uint16_t*)&ioWriteMask9[0x102]  =     0x0047; // TM0CNT_9
    *(uint16_t*)&ioMask9[0x104]  =     0xFFFF; *(uint16_t*)&ioWriteMask9[0x104]  =     0x0000; // TM1COUNT_9
    *(uint16_t*)&ioMask9[0x106]  =     0x00C7; *(uint16_t*)&ioWriteMask9[0x106]  =     0x0047; // TM1CNT_9
    *(uint16_t*)&ioMask9[0x108]  =     0xFFFF; *(uint16_t*)&ioWriteMask9[0x108]  =     0x0000; // TM2COUNT_9
    *(uint16_t*)&ioMask9[0x10A]  =     0x00C7; *(uint16_t*)&ioWriteMask9[0x10A]  =     0x0047; // TM2CNT_9
    *(uint16_t*)&ioMask9[0x10C]  =     0xFFFF; *(uint16_t*)&ioWriteMask9[0x10C]  =     0x0000; // TM3COUNT_9
    *(uint16_t*)&ioMask9[0x10E]  =     0x00C7; *(uint16_t*)&ioWriteMask9[0x10E]  =     0x0047; // TM3CNT_9
    *(uint16_t*)&ioMask9[0x130]  =     0x03FF; *(uint16_t*)&ioWriteMask9[0x130]  =     0x0000; // KEYINPUT
    *(uint16_t*)&ioMask9[0x180]  =     0x6F0F; *(uint16_t*)&ioWriteMask9[0x180]  =     0x4F0F; // IPCSYNC_9
    *(uint16_t*)&ioMask9[0x184]  =     0xC70F; *(uint16_t*)&ioWriteMask9[0x184]  =     0xC000; // IPCFIFOCNT_9
    *(uint32_t*)&ioMask9[0x188]  = 0xFFFFFFFF; *(uint32_t*)&ioWriteMask9[0x188]  = 0xFFFFFFFF; // IPCFIFOSEND_9
    *(uint16_t*)&ioMask9[0x1A0]  =     0xE0C3; *(uint16_t*)&ioWriteMask9[0x1A0]  =     0xE043; // AUXSPICNT_9
    *(uint32_t*)&ioMask9[0x1A4]  = 0xFFFFFFFF; *(uint32_t*)&ioWriteMask9[0x1A4]  = 0xDF7F7FFF; // ROMCTRL_9
    *(uint32_t*)&ioMask9[0x1A8]  = 0xFFFFFFFF; *(uint32_t*)&ioWriteMask9[0x1A8]  = 0xFFFFFFFF; // ROMCMDOUT_9
    *(uint32_t*)&ioMask9[0x1AC]  = 0xFFFFFFFF; *(uint32_t*)&ioWriteMask9[0x1AC]  = 0xFFFFFFFF; // ROMCMDOUT_9
    *(uint16_t*)&ioMask9[0x208]  =     0x0001; *(uint16_t*)&ioWriteMask9[0x208]  =     0x0001; // IME_9
    *(uint32_t*)&ioMask9[0x210]  = 0x003F3F7F; *(uint32_t*)&ioWriteMask9[0x210]  = 0x003F3F7F; // IE_9
    *(uint32_t*)&ioMask9[0x214]  = 0x003F3F7F; *(uint32_t*)&ioWriteMask9[0x214]  = 0x00000000; // IF_9
                 ioMask9[0x240]  =       0x9B;              ioWriteMask9[0x240]  =       0x00; // VRAMCNT_A
                 ioMask9[0x241]  =       0x9B;              ioWriteMask9[0x241]  =       0x00; // VRAMCNT_B
                 ioMask9[0x242]  =       0x9F;              ioWriteMask9[0x242]  =       0x00; // VRAMCNT_C
                 ioMask9[0x243]  =       0x9F;              ioWriteMask9[0x243]  =       0x00; // VRAMCNT_D
                 ioMask9[0x244]  =       0x87;              ioWriteMask9[0x244]  =       0x00; // VRAMCNT_E
                 ioMask9[0x245]  =       0x9F;              ioWriteMask9[0x245]  =       0x00; // VRAMCNT_F
                 ioMask9[0x246]  =       0x9F;              ioWriteMask9[0x246]  =       0x00; // VRAMCNT_G
                 ioMask9[0x247]  =       0x03;              ioWriteMask9[0x247]  =       0x03; // WRAMCNT
                 ioMask9[0x248]  =       0x83;              ioWriteMask9[0x248]  =       0x00; // VRAMCNT_H
                 ioMask9[0x249]  =       0x83;              ioWriteMask9[0x249]  =       0x00; // VRAMCNT_I
                 ioMask9[0x300]  =       0x03;              ioWriteMask9[0x300]  =       0x02; // POSTFLG
    *(uint16_t*)&ioMask9[0x304]  =     0x820F; *(uint16_t*)&ioWriteMask9[0x304]  =     0x820F; // POWCNT1
    *(uint32_t*)&ioMask9[0x1000] = 0xC0B1FFF7; *(uint32_t*)&ioWriteMask9[0x1000] = 0xC0B1FFF7; // DISPCNT_B
    *(uint16_t*)&ioMask9[0x1008] =     0xFFFF; *(uint16_t*)&ioWriteMask9[0x1008] =     0xFFFF; // BG0CNT_B
    *(uint16_t*)&ioMask9[0x100A] =     0xFFFF; *(uint16_t*)&ioWriteMask9[0x100A] =     0xFFFF; // BG1CNT_B
    *(uint16_t*)&ioMask9[0x100C] =     0xFFFF; *(uint16_t*)&ioWriteMask9[0x100C] =     0xFFFF; // BG2CNT_B
    *(uint16_t*)&ioMask9[0x100E] =     0xFFFF; *(uint16_t*)&ioWriteMask9[0x100E] =     0xFFFF; // BG3CNT_B
    *(uint16_t*)&ioMask9[0x1010] =     0x01FF; *(uint16_t*)&ioWriteMask9[0x1010] =     0x01FF; // BG0HOFS_B
    *(uint16_t*)&ioMask9[0x1012] =     0x01FF; *(uint16_t*)&ioWriteMask9[0x1012] =     0x01FF; // BG0VOFS_B
    *(uint16_t*)&ioMask9[0x1014] =     0x01FF; *(uint16_t*)&ioWriteMask9[0x1014] =     0x01FF; // BG1HOFS_B
    *(uint16_t*)&ioMask9[0x1016] =     0x01FF; *(uint16_t*)&ioWriteMask9[0x1016] =     0x01FF; // BG1VOFS_B
    *(uint16_t*)&ioMask9[0x1018] =     0x01FF; *(uint16_t*)&ioWriteMask9[0x1018] =     0x01FF; // BG2HOFS_B
    *(uint16_t*)&ioMask9[0x101A] =     0x01FF; *(uint16_t*)&ioWriteMask9[0x101A] =     0x01FF; // BG2VOFS_B
    *(uint16_t*)&ioMask9[0x101C] =     0x01FF; *(uint16_t*)&ioWriteMask9[0x101C] =     0x01FF; // BG3HOFS_B
    *(uint16_t*)&ioMask9[0x101E] =     0x01FF; *(uint16_t*)&ioWriteMask9[0x101E] =     0x01FF; // BG3VOFS_B

    *(uint16_t*)&ioMask7[0x004] =     0xFFBF; *(uint16_t*)&ioWriteMask7[0x004] =     0x0000; // DISPSTAT
    *(uint16_t*)&ioMask7[0x006] =     0x01FF; *(uint16_t*)&ioWriteMask7[0x006] =     0x0000; // VCOUNT
    *(uint32_t*)&ioMask7[0x0B0] = 0x07FFFFFF; *(uint32_t*)&ioWriteMask7[0x0B0] = 0x07FFFFFF; // DMA0SAD_7
    *(uint32_t*)&ioMask7[0x0B4] = 0x07FFFFFF; *(uint32_t*)&ioWriteMask7[0x0B4] = 0x07FFFFFF; // DMA0DAD_7
    *(uint32_t*)&ioMask7[0x0B8] = 0xF7E03FFF; *(uint32_t*)&ioWriteMask7[0x0B8] = 0xF7E03FFF; // DMA0CNT_7
    *(uint32_t*)&ioMask7[0x0BC] = 0x07FFFFFF; *(uint32_t*)&ioWriteMask7[0x0BC] = 0x07FFFFFF; // DMA1SAD_7
    *(uint32_t*)&ioMask7[0x0C0] = 0x07FFFFFF; *(uint32_t*)&ioWriteMask7[0x0C0] = 0x07FFFFFF; // DMA1DAD_7
    *(uint32_t*)&ioMask7[0x0C4] = 0xF7E03FFF; *(uint32_t*)&ioWriteMask7[0x0C4] = 0xF7E03FFF; // DMA1CNT_7
    *(uint32_t*)&ioMask7[0x0C8] = 0x0FFFFFFF; *(uint32_t*)&ioWriteMask7[0x0C8] = 0x07FFFFFF; // DMA2SAD_7
    *(uint32_t*)&ioMask7[0x0CC] = 0x0FFFFFFF; *(uint32_t*)&ioWriteMask7[0x0CC] = 0x07FFFFFF; // DMA2DAD_7
    *(uint32_t*)&ioMask7[0x0D0] = 0xF7E03FFF; *(uint32_t*)&ioWriteMask7[0x0D0] = 0xF7E03FFF; // DMA2CNT_7
    *(uint32_t*)&ioMask7[0x0D4] = 0x07FFFFFF; *(uint32_t*)&ioWriteMask7[0x0D4] = 0x07FFFFFF; // DMA3SAD_7
    *(uint32_t*)&ioMask7[0x0D8] = 0x07FFFFFF; *(uint32_t*)&ioWriteMask7[0x0D8] = 0x07FFFFFF; // DMA3DAD_7
    *(uint32_t*)&ioMask7[0x0DC] = 0xF7E0FFFF; *(uint32_t*)&ioWriteMask7[0x0DC] = 0xF7E0FFFF; // DMA3CNT_7
    *(uint16_t*)&ioMask7[0x100] =     0xFFFF; *(uint16_t*)&ioWriteMask7[0x100] =     0x0000; // TM0COUNT_7
    *(uint16_t*)&ioMask7[0x102] =     0x00C7; *(uint16_t*)&ioWriteMask7[0x102] =     0x0047; // TM0CNT_7
    *(uint16_t*)&ioMask7[0x104] =     0xFFFF; *(uint16_t*)&ioWriteMask7[0x104] =     0x0000; // TM1COUNT_7
    *(uint16_t*)&ioMask7[0x106] =     0x00C7; *(uint16_t*)&ioWriteMask7[0x106] =     0x0047; // TM1CNT_7
    *(uint16_t*)&ioMask7[0x108] =     0xFFFF; *(uint16_t*)&ioWriteMask7[0x108] =     0x0000; // TM2COUNT_7
    *(uint16_t*)&ioMask7[0x10A] =     0x00C7; *(uint16_t*)&ioWriteMask7[0x10A] =     0x0047; // TM2CNT_7
    *(uint16_t*)&ioMask7[0x10C] =     0xFFFF; *(uint16_t*)&ioWriteMask7[0x10C] =     0x0000; // TM3COUNT_7
    *(uint16_t*)&ioMask7[0x10E] =     0x00C7; *(uint16_t*)&ioWriteMask7[0x10E] =     0x0047; // TM3CNT_7
    *(uint16_t*)&ioMask7[0x130] =     0x03FF; *(uint16_t*)&ioWriteMask7[0x130] =     0x0000; // KEYINPUT
    *(uint16_t*)&ioMask7[0x136] =     0x00FF; *(uint16_t*)&ioWriteMask7[0x136] =     0x0000; // EXTKEYIN
    *(uint16_t*)&ioMask7[0x180] =     0x6F0F; *(uint16_t*)&ioWriteMask7[0x180] =     0x4F0F; // IPCSYNC_7
    *(uint16_t*)&ioMask7[0x184] =     0xC70F; *(uint16_t*)&ioWriteMask7[0x184] =     0xC000; // IPCFIFOCNT_7
    *(uint32_t*)&ioMask7[0x188] = 0xFFFFFFFF; *(uint32_t*)&ioWriteMask7[0x188] = 0xFFFFFFFF; // IPCFIFOSEND_7
    *(uint16_t*)&ioMask7[0x1A0] =     0xE0C3; *(uint16_t*)&ioWriteMask7[0x1A0] =     0xE043; // AUXSPICNT_7
    *(uint32_t*)&ioMask7[0x1A4] = 0xFFFFFFFF; *(uint32_t*)&ioWriteMask7[0x1A4] = 0xDF7F7FFF; // ROMCTRL_7
    *(uint32_t*)&ioMask7[0x1A8] = 0xFFFFFFFF; *(uint32_t*)&ioWriteMask7[0x1A8] = 0xFFFFFFFF; // ROMCMDOUT_7
    *(uint32_t*)&ioMask7[0x1AC] = 0xFFFFFFFF; *(uint32_t*)&ioWriteMask7[0x1AC] = 0xFFFFFFFF; // ROMCMDOUT_7
    *(uint16_t*)&ioMask7[0x1C0] =     0xCF83; *(uint16_t*)&ioWriteMask7[0x1C0] =     0xCF03; // SPICNT
    *(uint16_t*)&ioMask7[0x1C2] =     0x00FF; *(uint16_t*)&ioWriteMask7[0x1C2] =     0x0000; // SPIDATA
    *(uint16_t*)&ioMask7[0x208] =     0x0001; *(uint16_t*)&ioWriteMask7[0x208] =     0x0001; // IME_7
    *(uint32_t*)&ioMask7[0x210] = 0x01FF3FFF; *(uint32_t*)&ioWriteMask7[0x210] = 0x01FF3FFF; // IE_7
    *(uint32_t*)&ioMask7[0x214] = 0x01FF3FFF; *(uint32_t*)&ioWriteMask7[0x214] = 0x00000000; // IF_7
                 ioMask7[0x241] =       0x03;              ioWriteMask7[0x241] =       0x00; // WRAMSTAT
                 ioMask7[0x300] =       0x01;              ioWriteMask7[0x300] =       0x00; // POSTFLG
                 ioMask7[0x301] =       0xC0;              ioWriteMask7[0x301] =       0xC0; // HALTCNT
    *(uint16_t*)&ioMask7[0x504] =     0x03FF; *(uint16_t*)&ioWriteMask7[0x504] =     0x03FF; // SOUNDBIAS
}

}
