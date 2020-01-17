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
#include <exception>

#include "memory.h"
#include "defines.h"
#include "cartridge.h"
#include "cp15.h"
#include "dma.h"
#include "gpu.h"
#include "gpu_2d.h"
#include "gpu_3d.h"
#include "gpu_3d_renderer.h"
#include "input.h"
#include "interpreter.h"
#include "ipc.h"
#include "math.h"
#include "rtc.h"
#include "spi.h"
#include "spu.h"
#include "timers.h"

Memory::Memory(Cartridge *cart9, Cartridge *cart7, Cp15 *cp15, Dma *dma9, Dma *dma7, Gpu *gpu, Gpu2D *engineA,
               Gpu2D *engineB, Gpu3D *gpu3D, Gpu3DRenderer *gpu3DRenderer, Input *input, Interpreter *arm9,
               Interpreter *arm7, Ipc *ipc, Math *math, Rtc *rtc, Spi *spi, Spu *spu, Timers *timers9, Timers *timers7):
               cart9(cart9), cart7(cart7), cp15(cp15), dma9(dma9), dma7(dma7), gpu(gpu), engineA(engineA),
               engineB(engineB), gpu3D(gpu3D), gpu3DRenderer(gpu3DRenderer), input(input), arm9(arm9),
               arm7(arm7), ipc(ipc), math(math), rtc(rtc), spi(spi), spu(spu), timers9(timers9), timers7(timers7)
{
    // Attempt to load the ARM9 BIOS
    FILE *bios9File = fopen("bios9.bin", "rb");
    if (!bios9File) throw new std::exception;
    fread(bios9, sizeof(uint8_t), 0x1000, bios9File);
    fclose(bios9File);

    // Attempt to load the ARM7 BIOS
    FILE *bios7File = fopen("bios7.bin", "rb");
    if (!bios7File) throw new std::exception;
    fread(bios7, sizeof(uint8_t), 0x4000, bios7File);
    fclose(bios7File);
}

template int8_t   Memory::read(bool arm9, uint32_t address);
template int16_t  Memory::read(bool arm9, uint32_t address);
template uint8_t  Memory::read(bool arm9, uint32_t address);
template uint16_t Memory::read(bool arm9, uint32_t address);
template uint32_t Memory::read(bool arm9, uint32_t address);
template <typename T> T Memory::read(bool arm9, uint32_t address)
{
    uint8_t *data = nullptr;

    if (arm9)
    {
        // Get a pointer to the ARM9 memory mapped to the given address
        if (cp15->getItcmEnabled() && address < cp15->getItcmSize()) // Instruction TCM
        {
            data = &instrTcm[address % 0x8000];
        }
        else if (cp15->getDtcmEnabled() && address >= cp15->getDtcmAddr() && address < cp15->getDtcmAddr() + cp15->getDtcmSize()) // Data TCM
        {
            data = &dataTcm[(address - cp15->getDtcmAddr()) % 0x4000];
        }
        else
        {
            switch (address & 0xFF000000)
            {
                case 0x02000000: // Main RAM
                {
                    data = &ram[address % 0x400000];
                    break;
                }

                case 0x03000000: // Shared WRAM
                {
                    switch (wramStat)
                    {
                        case 0: data = &wram[address % 0x8000];
                        case 1: data = &wram[address % 0x4000 + 0x4000];
                        case 2: data = &wram[address % 0x4000];
                    }
                    break;
                }

                case 0x04000000: // I/O registers
                {
                    return ioRead9<T>(address);
                }

                case 0x05000000: // Palettes
                {
                    data = &palette[address % 0x800];
                    break;
                }

                case 0x06000000: // VRAM
                {
                    data = getMappedVram(address);
                    break;
                }

                case 0x07000000: // OAM
                {
                    data = &oam[address % 0x800];
                    break;
                }

                case 0x08000000: case 0x0A000000: // GBA slot
                {
                    // Return endless 0xFFs as if nothing is inserted in the GBA slot
                    return (T)0xFFFFFFFF;
                }

                case 0xFF000000: // ARM9 BIOS
                {
                    if ((address & 0xFFFF8000) == 0xFFFF0000)
                        data = &bios9[address & 0xFFFF];
                    break;
                }
            }
        }
    }
    else
    {
        // Get a pointer to the ARM7 memory mapped to the given address
        switch (address & 0xFF000000)
        {
            case 0x00000000: // ARM7 BIOS
            {
                if (address < 0x4000)
                    data = &bios7[address];
                break;
            }

            case 0x02000000: // Main RAM
            {
                data = &ram[address % 0x400000];
                break;
            }

            case 0x03000000: // WRAM
            {
                if (!(address & 0x00800000)) // Shared WRAM
                {
                    switch (wramStat)
                    {
                        case 1: data = &wram[address % 0x4000];
                        case 2: data = &wram[address % 0x4000 + 0x4000];
                        case 3: data = &wram[address % 0x8000];
                    }
                }
                if (!data) data = &wram7[address % 0x10000]; // ARM7 WRAM
                break;
            }

            case 0x04000000: // I/O registers
            {
                return ioRead7<T>(address);
            }

            case 0x06000000: // VRAMs
            {
                if (address >= vramBases[2] && address < vramBases[2] + 0x20000 && (vramStat & BIT(0))) // VRAM block C
                    data = &vramC[address - vramBases[2]];
                else if (address >= vramBases[3] && address < vramBases[3] + 0x20000 && (vramStat & BIT(1))) // VRAM block D
                    data = &vramD[address - vramBases[3]];
                break;
            }

            case 0x08000000: case 0x0A000000: // GBA slot
            {
                // Return endless 0xFFs as if nothing is inserted in the GBA slot
                return (T)0xFFFFFFFF;
            }
        }
    }

    T value = 0;

    if (data)
    {
        // Form an LSB-first value from the data at the pointer
        for (unsigned int i = 0; i < sizeof(T); i++)
            value |= data[i] << (i * 8);
    }
    else
    {
        printf("Unmapped ARM%d memory read: 0x%X\n", (arm9 ? 9 : 7), address);
    }

    return value;
}

template void Memory::write(bool arm9, uint32_t address, uint8_t  value);
template void Memory::write(bool arm9, uint32_t address, uint16_t value);
template void Memory::write(bool arm9, uint32_t address, uint32_t value);
template <typename T> void Memory::write(bool arm9, uint32_t address, T value)
{
    uint8_t *data = nullptr;

    if (arm9)
    {
        // Get a pointer to the ARM9 memory mapped to the given address
        if (cp15->getItcmEnabled() && address < cp15->getItcmSize()) // Instruction TCM
        {
            data = &instrTcm[address % 0x8000];
        }
        else if (cp15->getDtcmEnabled() && address >= cp15->getDtcmAddr() && address < cp15->getDtcmAddr() + cp15->getDtcmSize()) // Data TCM
        {
            data = &dataTcm[(address - cp15->getDtcmAddr()) % 0x4000];
        }
        else
        {
            switch (address & 0xFF000000)
            {
                case 0x02000000: // Main RAM
                {
                    data = &ram[address % 0x400000];
                    break;
                }

                case 0x03000000: // Shared WRAM
                {
                    switch (wramStat)
                    {
                        case 0: data = &wram[address % 0x8000];
                        case 1: data = &wram[address % 0x4000 + 0x4000];
                        case 2: data = &wram[address % 0x4000];
                    }
                    break;
                }

                case 0x04000000: // I/O registers
                {
                    ioWrite9<T>(address, value);
                    return;
                }

                case 0x05000000: // Palettes
                {
                    data = &palette[address % 0x800];
                    break;
                }

                case 0x06000000: // VRAM
                {
                    data = getMappedVram(address);
                    break;
                }

                case 0x07000000: // OAM
                {
                    data = &oam[address % 0x800];
                    break;
                }
            }
        }
    }
    else
    {
        // Get a pointer to the ARM7 memory mapped to the given address
        switch (address & 0xFF000000)
        {
            case 0x02000000: // Main RAM
            {
                data = &ram[address % 0x400000];
                break;
            }

            case 0x03000000: // WRAM
            {
                if (!(address & 0x00800000)) // Shared WRAM
                {
                    switch (wramStat)
                    {
                        case 1: data = &wram[address % 0x4000];
                        case 2: data = &wram[address % 0x4000 + 0x4000];
                        case 3: data = &wram[address % 0x8000];
                    }
                }
                if (!data) data = &wram7[address % 0x10000]; // ARM7 WRAM
                break;
            }

            case 0x04000000: // I/O registers
            {
                ioWrite7<T>(address, value);
                return;
            }

            case 0x06000000: // VRAM
            {
                if (address >= vramBases[2] && address < vramBases[2] + 0x20000 && (vramStat & BIT(0))) // VRAM block C
                    data = &vramC[address - vramBases[2]];
                else if (address >= vramBases[3] && address < vramBases[3] + 0x20000 && (vramStat & BIT(1))) // VRAM block D
                    data = &vramD[address - vramBases[3]];
                break;
            }
        }
    }

    if (data)
    {
        // Write an LSB-first value to the data at the pointer
        for (unsigned int i = 0; i < sizeof(T); i++)
            data[i] = value >> (i * 8);
    }
    else
    {
        printf("Unmapped ARM%d memory write: 0x%X\n", (arm9 ? 9 : 7), address);
    }
}

uint8_t *Memory::getMappedVram(uint32_t address)
{
    // Get a pointer to the VRAM mapped to the ARM9 at the given address
    if (address >= vramBases[0] && address < vramBases[0] + 0x20000) // Block A
        return &vramA[address - vramBases[0]];
    if (address >= vramBases[1] && address < vramBases[1] + 0x20000) // Block B
        return &vramB[address - vramBases[1]];
    if (address >= vramBases[2] && address < vramBases[2] + 0x20000 && !(vramStat & BIT(0))) // Block C
        return &vramC[address - vramBases[2]];
    if (address >= vramBases[3] && address < vramBases[3] + 0x20000 && !(vramStat & BIT(1))) // Block D
        return &vramD[address - vramBases[3]];
    if (address >= vramBases[4] && address < vramBases[4] + 0x10000) // Block E
        return &vramE[address - vramBases[4]];
    if (address >= vramBases[5] && address < vramBases[5] + 0x4000) // Block F
        return &vramF[address - vramBases[5]];
    if (address >= vramBases[6] && address < vramBases[6] + 0x4000) // Block G
        return &vramG[address - vramBases[6]];
    if (address >= vramBases[7] && address < vramBases[7] + 0x8000) // Block H
        return &vramH[address - vramBases[7]];
    if (address >= vramBases[8] && address < vramBases[8] + 0x4000) // Block I
        return &vramI[address - vramBases[8]];
    return nullptr;
}

uint8_t *Memory::getVramBlock(int block)
{
    // Get a pointer to a VRAM block
    // Only blocks A through D are ever directly needed externally
    switch (block)
    {
        case 0:  return vramA;
        case 1:  return vramB;
        case 2:  return vramC;
        case 3:  return vramD;
        default: return nullptr;
    }
}

template <typename T> T Memory::ioRead9(uint32_t address)
{
    T value = 0;
    unsigned int i = 0;

    // Read a value from one or more ARM9 I/O registers
    while (i < sizeof(T))
    {
        uint32_t base = address + i;
        unsigned int size;
        uint32_t data;

        switch (base)
        {
            case 0x4000000:
            case 0x4000001:
            case 0x4000002:
            case 0x4000003: base -= 0x4000000; size = 4; data = engineA->readDispCnt();       break; // DISPCNT (engine A)
            case 0x4000004:
            case 0x4000005: base -= 0x4000004; size = 2; data = gpu->readDispStat9();         break; // DISPSTAT (ARM9)
            case 0x4000006:
            case 0x4000007: base -= 0x4000006; size = 2; data = gpu->readVCount();            break; // VCOUNT
            case 0x4000008:
            case 0x4000009: base -= 0x4000008; size = 2; data = engineA->readBgCnt(0);        break; // BG0CNT (engine A)
            case 0x400000A:
            case 0x400000B: base -= 0x400000A; size = 2; data = engineA->readBgCnt(1);        break; // BG1CNT (engine A)
            case 0x400000C:
            case 0x400000D: base -= 0x400000C; size = 2; data = engineA->readBgCnt(2);        break; // BG2CNT (engine A)
            case 0x400000E:
            case 0x400000F: base -= 0x400000E; size = 2; data = engineA->readBgCnt(3);        break; // BG3CNT (engine A)
            case 0x4000060:
            case 0x4000061: base -= 0x4000060; size = 2; gpu3DRenderer->readDisp3DCnt();      break; // DISP3DCNT
            case 0x4000064:
            case 0x4000065:
            case 0x4000066:
            case 0x4000067: base -= 0x4000064; size = 4; gpu->readDispCapCnt();               break; // DISPCAPCNT
            case 0x400006C:
            case 0x400006D: base -= 0x400006C; size = 2; data = engineA->readMasterBright();  break; // MASTER_BRIGHT (engine A)
            case 0x40000B0:
            case 0x40000B1:
            case 0x40000B2:
            case 0x40000B3: base -= 0x40000B0; size = 4; data = dma9->readDmaSad(0);          break; // DMA0SAD (ARM9)
            case 0x40000B4:
            case 0x40000B5:
            case 0x40000B6:
            case 0x40000B7: base -= 0x40000B4; size = 4; data = dma9->readDmaDad(0);          break; // DMA0DAD (ARM9)
            case 0x40000B8:
            case 0x40000B9:
            case 0x40000BA:
            case 0x40000BB: base -= 0x40000B8; size = 4; data = dma9->readDmaCnt(0);          break; // DMA0CNT (ARM9)
            case 0x40000BC:
            case 0x40000BD:
            case 0x40000BE:
            case 0x40000BF: base -= 0x40000BC; size = 4; data = dma9->readDmaSad(1);          break; // DMA1SAD (ARM9)
            case 0x40000C0:
            case 0x40000C1:
            case 0x40000C2:
            case 0x40000C3: base -= 0x40000C0; size = 4; data = dma9->readDmaDad(1);          break; // DMA1DAD (ARM9)
            case 0x40000C4:
            case 0x40000C5:
            case 0x40000C6:
            case 0x40000C7: base -= 0x40000C4; size = 4; data = dma9->readDmaCnt(1);          break; // DMA1CNT (ARM9)
            case 0x40000C8:
            case 0x40000C9:
            case 0x40000CA:
            case 0x40000CB: base -= 0x40000C8; size = 4; data = dma9->readDmaSad(2);          break; // DMA2SAD (ARM9)
            case 0x40000CC:
            case 0x40000CD:
            case 0x40000CE:
            case 0x40000CF: base -= 0x40000CC; size = 4; data = dma9->readDmaDad(2);          break; // DMA2DAD (ARM9)
            case 0x40000D0:
            case 0x40000D1:
            case 0x40000D2:
            case 0x40000D3: base -= 0x40000D0; size = 4; data = dma9->readDmaCnt(2);          break; // DMA2CNT (ARM9)
            case 0x40000D4:
            case 0x40000D5:
            case 0x40000D6:
            case 0x40000D7: base -= 0x40000D4; size = 4; data = dma9->readDmaSad(3);          break; // DMA3SAD (ARM9)
            case 0x40000D8:
            case 0x40000D9:
            case 0x40000DA:
            case 0x40000DB: base -= 0x40000D8; size = 4; data = dma9->readDmaDad(3);          break; // DMA3DAD (ARM9)
            case 0x40000DC:
            case 0x40000DD:
            case 0x40000DE:
            case 0x40000DF: base -= 0x40000DC; size = 4; data = dma9->readDmaCnt(3);          break; // DMA3CNT (ARM9)
            case 0x40000E0:
            case 0x40000E1:
            case 0x40000E2:
            case 0x40000E3: base -= 0x40000E0; size = 4; data = readDmaFill(0);               break; // DMA0FILL
            case 0x40000E4:
            case 0x40000E5:
            case 0x40000E6:
            case 0x40000E7: base -= 0x40000E4; size = 4; data = readDmaFill(1);               break; // DMA1FILL
            case 0x40000E8:
            case 0x40000E9:
            case 0x40000EA:
            case 0x40000EB: base -= 0x40000E8; size = 4; data = readDmaFill(2);               break; // DMA2FILL
            case 0x40000EC:
            case 0x40000ED:
            case 0x40000EE:
            case 0x40000EF: base -= 0x40000EC; size = 4; data = readDmaFill(3);               break; // DMA3FILL
            case 0x4000100:
            case 0x4000101: base -= 0x4000100; size = 2; data = timers9->readTmCntL(0);       break; // TM0CNT_L (ARM9)
            case 0x4000102:
            case 0x4000103: base -= 0x4000102; size = 2; data = timers9->readTmCntH(0);       break; // TM0CNT_H (ARM9)
            case 0x4000104:
            case 0x4000105: base -= 0x4000104; size = 2; data = timers9->readTmCntL(1);       break; // TM1CNT_L (ARM9)
            case 0x4000106:
            case 0x4000107: base -= 0x4000106; size = 2; data = timers9->readTmCntH(1);       break; // TM1CNT_H (ARM9)
            case 0x4000108:
            case 0x4000109: base -= 0x4000108; size = 2; data = timers9->readTmCntL(2);       break; // TM2CNT_L (ARM9)
            case 0x400010A:
            case 0x400010B: base -= 0x400010A; size = 2; data = timers9->readTmCntH(2);       break; // TM2CNT_H (ARM9)
            case 0x400010C:
            case 0x400010D: base -= 0x400010C; size = 2; data = timers9->readTmCntL(3);       break; // TM3CNT_L (ARM9)
            case 0x400010E:
            case 0x400010F: base -= 0x400010E; size = 2; data = timers9->readTmCntH(3);       break; // TM3CNT_H (ARM9)
            case 0x4000130:
            case 0x4000131: base -= 0x4000130; size = 2; data = input->readKeyInput();        break; // KEYINPUT
            case 0x4000180:
            case 0x4000181: base -= 0x4000180; size = 2; data = ipc->readIpcSync9();          break; // IPCSYNC (ARM9)
            case 0x4000184:
            case 0x4000185: base -= 0x4000184; size = 2; data = ipc->readIpcFifoCnt9();       break; // IPCFIFOCNT (ARM9)
            case 0x40001A0:
            case 0x40001A1: base -= 0x40001A0; size = 2; data = cart9->readAuxSpiCnt();       break; // AUXSPICNT (ARM9)
            case 0x40001A2: base -= 0x40001A2; size = 1; data = cart9->readAuxSpiData();      break; // AUXSPIDATA (ARM9)
            case 0x40001A4:
            case 0x40001A5:
            case 0x40001A6:
            case 0x40001A7: base -= 0x40001A4; size = 4; data = cart9->readRomCtrl();         break; // ROMCTRL (ARM9)
            case 0x40001A8:
            case 0x40001A9:
            case 0x40001AA:
            case 0x40001AB: base -= 0x40001A8; size = 4; data = cart9->readRomCmdOutL();      break; // ROMCMDOUT_L (ARM9)
            case 0x40001AC:
            case 0x40001AD:
            case 0x40001AE:
            case 0x40001AF: base -= 0x40001AC; size = 4; data = cart9->readRomCmdOutH();      break; // ROMCMDOUT_H (ARM9)
            case 0x4000208: base -= 0x4000208; size = 1; data = arm9->readIme();              break; // IME (ARM9)
            case 0x4000210:
            case 0x4000211:
            case 0x4000212:
            case 0x4000213: base -= 0x4000210; size = 4; data = arm9->readIe();               break; // IE (ARM9)
            case 0x4000214:
            case 0x4000215:
            case 0x4000216:
            case 0x4000217: base -= 0x4000214; size = 4; data = arm9->readIrf();              break; // IF (ARM9)
            case 0x4000280:
            case 0x4000281: base -= 0x4000280; size = 2; data = math->readDivCnt();           break; // DIVCNT
            case 0x4000290:
            case 0x4000291:
            case 0x4000292:
            case 0x4000293: base -= 0x4000290; size = 4; data = math->readDivNumerL();        break; // DIVNUMER_L
            case 0x4000294:
            case 0x4000295:
            case 0x4000296:
            case 0x4000297: base -= 0x4000294; size = 4; data = math->readDivNumerH();        break; // DIVNUMER_H
            case 0x4000298:
            case 0x4000299:
            case 0x400029A:
            case 0x400029B: base -= 0x4000298; size = 4; data = math->readDivDenomL();        break; // DIVDENOM_L
            case 0x400029C:
            case 0x400029D:
            case 0x400029E:
            case 0x400029F: base -= 0x400029C; size = 4; data = math->readDivDenomH();        break; // DIVDENOM_H
            case 0x40002A0:
            case 0x40002A1:
            case 0x40002A2:
            case 0x40002A3: base -= 0x40002A0; size = 4; data = math->readDivResultL();       break; // DIVRESULT_L
            case 0x40002A4:
            case 0x40002A5:
            case 0x40002A6:
            case 0x40002A7: base -= 0x40002A4; size = 4; data = math->readDivResultH();       break; // DIVRESULT_H
            case 0x40002A8:
            case 0x40002A9:
            case 0x40002AA:
            case 0x40002AB: base -= 0x40002A8; size = 4; data = math->readDivRemResultL();    break; // DIVREMRESULT_L
            case 0x40002AC:
            case 0x40002AD:
            case 0x40002AE:
            case 0x40002AF: base -= 0x40002AC; size = 4; data = math->readDivRemResultH();    break; // DIVREMRESULT_H
            case 0x40002B0:
            case 0x40002B1: base -= 0x40002B0; size = 2; data = math->readSqrtCnt();          break; // SQRTCNT
            case 0x40002B4:
            case 0x40002B5:
            case 0x40002B6:
            case 0x40002B7: base -= 0x40002B4; size = 4; data = math->readSqrtResult();       break; // SQRTRESULT
            case 0x40002B8:
            case 0x40002B9:
            case 0x40002BA:
            case 0x40002BB: base -= 0x40002B8; size = 4; data = math->readSqrtParamL();       break; // SQRTPARAM_L
            case 0x40002BC:
            case 0x40002BD:
            case 0x40002BE:
            case 0x40002BF: base -= 0x40002BC; size = 4; data = math->readSqrtParamH();       break; // SQRTPARAM_H
            case 0x4000300: base -= 0x4000300; size = 1; data = arm9->readPostFlg();          break; // POSTFLG (ARM9)
            case 0x4000304:
            case 0x4000305: base -= 0x4000304; size = 2; data = gpu->readPowCnt1();           break; // POWCNT1
            case 0x4000600:
            case 0x4000601:
            case 0x4000602:
            case 0x4000603: base -= 0x4000600; size = 4; data = gpu3D->readGxStat();          break; // GXSTAT
            case 0x4000604:
            case 0x4000605:
            case 0x4000606:
            case 0x4000607: base -= 0x4000604; size = 4; data = gpu3D->readRamCount();        break; // RAM_COUNT
            case 0x4000640:
            case 0x4000641:
            case 0x4000642:
            case 0x4000643: base -= 0x4000640; size = 4; data = gpu3D->readClipMtxResult(0);  break; // CLIPMTX_RESULT
            case 0x4000644:
            case 0x4000645:
            case 0x4000646:
            case 0x4000647: base -= 0x4000644; size = 4; data = gpu3D->readClipMtxResult(1);  break; // CLIPMTX_RESULT
            case 0x4000648:
            case 0x4000649:
            case 0x400064A:
            case 0x400064B: base -= 0x4000648; size = 4; data = gpu3D->readClipMtxResult(2);  break; // CLIPMTX_RESULT
            case 0x400064C:
            case 0x400064D:
            case 0x400064E:
            case 0x400064F: base -= 0x400064C; size = 4; data = gpu3D->readClipMtxResult(3);  break; // CLIPMTX_RESULT
            case 0x4000650:
            case 0x4000651:
            case 0x4000652:
            case 0x4000653: base -= 0x4000650; size = 4; data = gpu3D->readClipMtxResult(4);  break; // CLIPMTX_RESULT
            case 0x4000654:
            case 0x4000655:
            case 0x4000656:
            case 0x4000657: base -= 0x4000654; size = 4; data = gpu3D->readClipMtxResult(5);  break; // CLIPMTX_RESULT
            case 0x4000658:
            case 0x4000659:
            case 0x400065A:
            case 0x400065B: base -= 0x4000658; size = 4; data = gpu3D->readClipMtxResult(6);  break; // CLIPMTX_RESULT
            case 0x400065C:
            case 0x400065D:
            case 0x400065E:
            case 0x400065F: base -= 0x400065C; size = 4; data = gpu3D->readClipMtxResult(7);  break; // CLIPMTX_RESULT
            case 0x4000660:
            case 0x4000661:
            case 0x4000662:
            case 0x4000663: base -= 0x4000660; size = 4; data = gpu3D->readClipMtxResult(8);  break; // CLIPMTX_RESULT
            case 0x4000664:
            case 0x4000665:
            case 0x4000666:
            case 0x4000667: base -= 0x4000664; size = 4; data = gpu3D->readClipMtxResult(9);  break; // CLIPMTX_RESULT
            case 0x4000668:
            case 0x4000669:
            case 0x400066A:
            case 0x400066B: base -= 0x4000668; size = 4; data = gpu3D->readClipMtxResult(10); break; // CLIPMTX_RESULT
            case 0x400066C:
            case 0x400066D:
            case 0x400066E:
            case 0x400066F: base -= 0x400066C; size = 4; data = gpu3D->readClipMtxResult(11); break; // CLIPMTX_RESULT
            case 0x4000670:
            case 0x4000671:
            case 0x4000672:
            case 0x4000673: base -= 0x4000670; size = 4; data = gpu3D->readClipMtxResult(12); break; // CLIPMTX_RESULT
            case 0x4000674:
            case 0x4000675:
            case 0x4000676:
            case 0x4000677: base -= 0x4000674; size = 4; data = gpu3D->readClipMtxResult(13); break; // CLIPMTX_RESULT
            case 0x4000678:
            case 0x4000679:
            case 0x400067A:
            case 0x400067B: base -= 0x4000678; size = 4; data = gpu3D->readClipMtxResult(14); break; // CLIPMTX_RESULT
            case 0x400067C:
            case 0x400067D:
            case 0x400067E:
            case 0x400067F: base -= 0x400067C; size = 4; data = gpu3D->readClipMtxResult(15); break; // CLIPMTX_RESULT
            case 0x4000680:
            case 0x4000681:
            case 0x4000682:
            case 0x4000683: base -= 0x4000680; size = 4; data = gpu3D->readVecMtxResult(0);   break; // VECMTX_RESULT
            case 0x4000684:
            case 0x4000685:
            case 0x4000686:
            case 0x4000687: base -= 0x4000684; size = 4; data = gpu3D->readVecMtxResult(1);   break; // VECMTX_RESULT
            case 0x4000688:
            case 0x4000689:
            case 0x400068A:
            case 0x400068B: base -= 0x4000688; size = 4; data = gpu3D->readVecMtxResult(2);   break; // VECMTX_RESULT
            case 0x400068C:
            case 0x400068D:
            case 0x400068E:
            case 0x400068F: base -= 0x400068C; size = 4; data = gpu3D->readVecMtxResult(3);   break; // VECMTX_RESULT
            case 0x4000690:
            case 0x4000691:
            case 0x4000692:
            case 0x4000693: base -= 0x4000690; size = 4; data = gpu3D->readVecMtxResult(4);   break; // VECMTX_RESULT
            case 0x4000694:
            case 0x4000695:
            case 0x4000696:
            case 0x4000697: base -= 0x4000694; size = 4; data = gpu3D->readVecMtxResult(5);   break; // VECMTX_RESULT
            case 0x4000698:
            case 0x4000699:
            case 0x400069A:
            case 0x400069B: base -= 0x4000698; size = 4; data = gpu3D->readVecMtxResult(6);   break; // VECMTX_RESULT
            case 0x400069C:
            case 0x400069D:
            case 0x400069E:
            case 0x400069F: base -= 0x400069C; size = 4; data = gpu3D->readVecMtxResult(7);   break; // VECMTX_RESULT
            case 0x40006A0:
            case 0x40006A1:
            case 0x40006A2:
            case 0x40006A3: base -= 0x40006A0; size = 4; data = gpu3D->readVecMtxResult(8);   break; // VECMTX_RESULT
            case 0x4001000:
            case 0x4001001:
            case 0x4001002:
            case 0x4001003: base -= 0x4001000; size = 4; data = engineB->readDispCnt();       break; // DISPCNT (engine B)
            case 0x4001008:
            case 0x4001009: base -= 0x4001008; size = 2; data = engineB->readBgCnt(0);        break; // BG0CNT (engine B)
            case 0x400100A:
            case 0x400100B: base -= 0x400100A; size = 2; data = engineB->readBgCnt(1);        break; // BG1CNT (engine B)
            case 0x400100C:
            case 0x400100D: base -= 0x400100C; size = 2; data = engineB->readBgCnt(2);        break; // BG2CNT (engine B)
            case 0x400100E:
            case 0x400100F: base -= 0x400100E; size = 2; data = engineB->readBgCnt(3);        break; // BG3CNT (engine B)
            case 0x400106C:
            case 0x400106D: base -= 0x400106C; size = 2; data = engineB->readMasterBright();  break; // MASTER_BRIGHT (engine B)
            case 0x4100000:
            case 0x4100001:
            case 0x4100002:
            case 0x4100003: base -= 0x4100000; size = 4; data = ipc->readIpcFifoRecv9();      break; // IPCFIFORECV (ARM9)
            case 0x4100010:
            case 0x4100011:
            case 0x4100012:
            case 0x4100013: base -= 0x4100010; size = 4; data = cart9->readRomDataIn();       break; // ROMDATAIN (ARM9)

            default:
            {
                // Handle unknown reads by returning 0
                if (i == 0)
                {
                    printf("Unknown ARM9 I/O register read: 0x%X\n", address);
                    return 0;
                }

                // Ignore unknown reads if they occur after the first byte
                // This is in case, for example, a 16-bit register is accessed with a 32-bit read
                i++;
                continue;
            }
        }

        value |= (data >> (base * 8)) << (i * 8);
        i += size - base;
    }

    return value;
}

template <typename T> T Memory::ioRead7(uint32_t address)
{
    T value = 0;
    unsigned int i = 0;

    // Read a value from one or more ARM7 I/O registers
    while (i < sizeof(T))
    {
        uint32_t base = address + i;
        unsigned int size;
        uint32_t data;

        switch (base)
        {
            case 0x4000004:
            case 0x4000005: base -= 0x4000004; size = 2; data = gpu->readDispStat7();    break; // DISPSTAT (ARM7)
            case 0x4000006:
            case 0x4000007: base -= 0x4000006; size = 2; data = gpu->readVCount();       break; // VCOUNT
            case 0x40000B0:
            case 0x40000B1:
            case 0x40000B2:
            case 0x40000B3: base -= 0x40000B0; size = 4; data = dma7->readDmaSad(0);     break; // DMA0SAD (ARM7)
            case 0x40000B4:
            case 0x40000B5:
            case 0x40000B6:
            case 0x40000B7: base -= 0x40000B4; size = 4; data = dma7->readDmaDad(0);     break; // DMA0DAD (ARM7)
            case 0x40000B8:
            case 0x40000B9:
            case 0x40000BA:
            case 0x40000BB: base -= 0x40000B8; size = 4; data = dma7->readDmaCnt(0);     break; // DMA0CNT (ARM7)
            case 0x40000BC:
            case 0x40000BD:
            case 0x40000BE:
            case 0x40000BF: base -= 0x40000BC; size = 4; data = dma7->readDmaSad(1);     break; // DMA1SAD (ARM7)
            case 0x40000C0:
            case 0x40000C1:
            case 0x40000C2:
            case 0x40000C3: base -= 0x40000C0; size = 4; data = dma7->readDmaDad(1);     break; // DMA1DAD (ARM7)
            case 0x40000C4:
            case 0x40000C5:
            case 0x40000C6:
            case 0x40000C7: base -= 0x40000C4; size = 4; data = dma7->readDmaCnt(1);     break; // DMA1CNT (ARM7)
            case 0x40000C8:
            case 0x40000C9:
            case 0x40000CA:
            case 0x40000CB: base -= 0x40000C8; size = 4; data = dma7->readDmaSad(2);     break; // DMA2SAD (ARM7)
            case 0x40000CC:
            case 0x40000CD:
            case 0x40000CE:
            case 0x40000CF: base -= 0x40000CC; size = 4; data = dma7->readDmaDad(2);     break; // DMA2DAD (ARM7)
            case 0x40000D0:
            case 0x40000D1:
            case 0x40000D2:
            case 0x40000D3: base -= 0x40000D0; size = 4; data = dma7->readDmaCnt(2);     break; // DMA2CNT (ARM7)
            case 0x40000D4:
            case 0x40000D5:
            case 0x40000D6:
            case 0x40000D7: base -= 0x40000D4; size = 4; data = dma7->readDmaSad(3);     break; // DMA3SAD (ARM7)
            case 0x40000D8:
            case 0x40000D9:
            case 0x40000DA:
            case 0x40000DB: base -= 0x40000D8; size = 4; data = dma7->readDmaDad(3);     break; // DMA3DAD (ARM7)
            case 0x40000DC:
            case 0x40000DD:
            case 0x40000DE:
            case 0x40000DF: base -= 0x40000DC; size = 4; data = dma7->readDmaCnt(3);     break; // DMA3CNT (ARM7)
            case 0x4000100:
            case 0x4000101: base -= 0x4000100; size = 2; data = timers7->readTmCntL(0);  break; // TM0CNT_L (ARM7)
            case 0x4000102:
            case 0x4000103: base -= 0x4000102; size = 2; data = timers7->readTmCntH(0);  break; // TM0CNT_H (ARM7)
            case 0x4000104:
            case 0x4000105: base -= 0x4000104; size = 2; data = timers7->readTmCntL(1);  break; // TM1CNT_L (ARM7)
            case 0x4000106:
            case 0x4000107: base -= 0x4000106; size = 2; data = timers7->readTmCntH(1);  break; // TM1CNT_H (ARM7)
            case 0x4000108:
            case 0x4000109: base -= 0x4000108; size = 2; data = timers7->readTmCntL(2);  break; // TM2CNT_L (ARM7)
            case 0x400010A:
            case 0x400010B: base -= 0x400010A; size = 2; data = timers7->readTmCntH(2);  break; // TM2CNT_H (ARM7)
            case 0x400010C:
            case 0x400010D: base -= 0x400010C; size = 2; data = timers7->readTmCntL(3);  break; // TM3CNT_L (ARM7)
            case 0x400010E:
            case 0x400010F: base -= 0x400010E; size = 2; data = timers7->readTmCntH(3);  break; // TM3CNT_H (ARM7)
            case 0x4000130:
            case 0x4000131: base -= 0x4000130; size = 2; data = input->readKeyInput();   break; // KEYINPUT
            case 0x4000136:
            case 0x4000137: base -= 0x4000136; size = 2; data = input->readExtKeyIn();   break; // EXTKEYIN
            case 0x4000138: base -= 0x4000138; size = 1; data = rtc->readRtc();          break; // RTC
            case 0x4000180:
            case 0x4000181: base -= 0x4000180; size = 2; data = ipc->readIpcSync7();     break; // IPCSYNC (ARM7)
            case 0x4000184:
            case 0x4000185: base -= 0x4000184; size = 2; data = ipc->readIpcFifoCnt7();  break; // IPCFIFOCNT (ARM7)
            case 0x40001A0:
            case 0x40001A1: base -= 0x40001A0; size = 2; data = cart7->readAuxSpiCnt();  break; // AUXSPICNT (ARM7)
            case 0x40001A2: base -= 0x40001A2; size = 1; data = cart7->readAuxSpiData(); break; // AUXSPIDATA (ARM7)
            case 0x40001A4:
            case 0x40001A5:
            case 0x40001A6:
            case 0x40001A7: base -= 0x40001A4; size = 4; data = cart7->readRomCtrl();    break; // ROMCTRL (ARM7)
            case 0x40001A8:
            case 0x40001A9:
            case 0x40001AA:
            case 0x40001AB: base -= 0x40001A8; size = 4; data = cart7->readRomCmdOutL(); break; // ROMCMDOUT_L (ARM7)
            case 0x40001AC:
            case 0x40001AD:
            case 0x40001AE:
            case 0x40001AF: base -= 0x40001AC; size = 4; data = cart7->readRomCmdOutH(); break; // ROMCMDOUT_H (ARM7)
            case 0x40001C0:
            case 0x40001C1: base -= 0x40001C0; size = 2; data = spi->readSpiCnt();       break; // SPICNT
            case 0x40001C2: base -= 0x40001C2; size = 1; data = spi->readSpiData();      break; // SPIDATA
            case 0x4000208: base -= 0x4000208; size = 1; data = arm7->readIme();         break; // IME (ARM7)
            case 0x4000210:
            case 0x4000211:
            case 0x4000212:
            case 0x4000213: base -= 0x4000210; size = 4; data = arm7->readIe();          break; // IE (ARM7)
            case 0x4000214:
            case 0x4000215:
            case 0x4000216:
            case 0x4000217: base -= 0x4000214; size = 4; data = arm7->readIrf();         break; // IF (ARM7)
            case 0x4000241: base -= 0x4000241; size = 1; data = readWramStat();          break; // WRAMSTAT
            case 0x4000300: base -= 0x4000300; size = 1; data = arm7->readPostFlg();     break; // POSTFLG (ARM7)
            case 0x4000301: base -= 0x4000301; size = 1; data = readHaltCnt();           break; // HALTCNT
            case 0x4000400:
            case 0x4000401:
            case 0x4000402:
            case 0x4000403: base -= 0x4000400; size = 4; data = spu->readSoundCnt(0);    break; // SOUND0CNT
            case 0x4000410:
            case 0x4000411:
            case 0x4000412:
            case 0x4000413: base -= 0x4000410; size = 4; data = spu->readSoundCnt(1);    break; // SOUND1CNT
            case 0x4000420:
            case 0x4000421:
            case 0x4000422:
            case 0x4000423: base -= 0x4000420; size = 4; data = spu->readSoundCnt(2);    break; // SOUND2CNT
            case 0x4000430:
            case 0x4000431:
            case 0x4000432:
            case 0x4000433: base -= 0x4000430; size = 4; data = spu->readSoundCnt(3);    break; // SOUND3CNT
            case 0x4000440:
            case 0x4000441:
            case 0x4000442:
            case 0x4000443: base -= 0x4000440; size = 4; data = spu->readSoundCnt(4);    break; // SOUND4CNT
            case 0x4000450:
            case 0x4000451:
            case 0x4000452:
            case 0x4000453: base -= 0x4000450; size = 4; data = spu->readSoundCnt(5);    break; // SOUND5CNT
            case 0x4000460:
            case 0x4000461:
            case 0x4000462:
            case 0x4000463: base -= 0x4000460; size = 4; data = spu->readSoundCnt(6);    break; // SOUND6CNT
            case 0x4000470:
            case 0x4000471:
            case 0x4000472:
            case 0x4000473: base -= 0x4000470; size = 4; data = spu->readSoundCnt(7);    break; // SOUND7CNT
            case 0x4000480:
            case 0x4000481:
            case 0x4000482:
            case 0x4000483: base -= 0x4000480; size = 4; data = spu->readSoundCnt(8);    break; // SOUND8CNT
            case 0x4000490:
            case 0x4000491:
            case 0x4000492:
            case 0x4000493: base -= 0x4000490; size = 4; data = spu->readSoundCnt(9);    break; // SOUND9CNT
            case 0x40004A0:
            case 0x40004A1:
            case 0x40004A2:
            case 0x40004A3: base -= 0x40004A0; size = 4; data = spu->readSoundCnt(10);   break; // SOUND10CNT
            case 0x40004B0:
            case 0x40004B1:
            case 0x40004B2:
            case 0x40004B3: base -= 0x40004B0; size = 4; data = spu->readSoundCnt(11);   break; // SOUND11CNT
            case 0x40004C0:
            case 0x40004C1:
            case 0x40004C2:
            case 0x40004C3: base -= 0x40004C0; size = 4; data = spu->readSoundCnt(12);   break; // SOUND12CNT
            case 0x40004D0:
            case 0x40004D1:
            case 0x40004D2:
            case 0x40004D3: base -= 0x40004D0; size = 4; data = spu->readSoundCnt(13);   break; // SOUND13CNT
            case 0x40004E0:
            case 0x40004E1:
            case 0x40004E2:
            case 0x40004E3: base -= 0x40004E0; size = 4; data = spu->readSoundCnt(14);   break; // SOUND14CNT
            case 0x40004F0:
            case 0x40004F1:
            case 0x40004F2:
            case 0x40004F3: base -= 0x40004F0; size = 4; data = spu->readSoundCnt(15);   break; // SOUND15CNT
            case 0x4000500:
            case 0x4000501: base -= 0x4000500; size = 2; data = spu->readMainSoundCnt(); break; // SOUNDCNT
            case 0x4000504:
            case 0x4000505: base -= 0x4000504; size = 2; data = spu->readSoundBias();    break; // SOUNDBIAS
            case 0x4100000:
            case 0x4100001:
            case 0x4100002:
            case 0x4100003: base -= 0x4100000; size = 4; data = ipc->readIpcFifoRecv7(); break; // IPCFIFORECV (ARM7)
            case 0x4100010:
            case 0x4100011:
            case 0x4100012:
            case 0x4100013: base -= 0x4100010; size = 4; data = cart7->readRomDataIn();  break; // ROMDATAIN (ARM7)

            default:
            {
                // Handle unknown reads by returning 0
                if (i == 0)
                {
                    printf("Unknown ARM7 I/O register read: 0x%X\n", address);
                    return 0;
                }

                // Ignore unknown reads if they occur after the first byte
                // This is in case, for example, a 16-bit register is accessed with a 32-bit read
                i++;
                continue;
            }
        }

        value |= (data >> (base * 8)) << (i * 8);
        i += size - base;
    }

    return value;
}

template <typename T> void Memory::ioWrite9(uint32_t address, T value)
{
    unsigned int i = 0;

    // Write a value to one or more ARM9 I/O registers
    while (i < sizeof(T))
    {
        uint32_t base = address + i;
        unsigned int size;
        uint32_t mask = ((uint64_t)1 << ((sizeof(T) - i) * 8)) - 1;
        uint32_t data = value >> (i * 8);

        switch (base)
        {
            case 0x4000000:
            case 0x4000001:
            case 0x4000002:
            case 0x4000003: base -= 0x4000000; size = 4; engineA->writeDispCnt(mask << (base * 8), data << (base * 8));             break; // DISPCNT (engine A)
            case 0x4000004:
            case 0x4000005: base -= 0x4000004; size = 2; gpu->writeDispStat9(mask << (base * 8), data << (base * 8));               break; // DISPSTAT (ARM9)
            case 0x4000008:
            case 0x4000009: base -= 0x4000008; size = 2; engineA->writeBgCnt(0, mask << (base * 8), data << (base * 8));            break; // BG0CNT (engine A)
            case 0x400000A:
            case 0x400000B: base -= 0x400000A; size = 2; engineA->writeBgCnt(1, mask << (base * 8), data << (base * 8));            break; // BG1CNT (engine A)
            case 0x400000C:
            case 0x400000D: base -= 0x400000C; size = 2; engineA->writeBgCnt(2, mask << (base * 8), data << (base * 8));            break; // BG2CNT (engine A)
            case 0x400000E:
            case 0x400000F: base -= 0x400000E; size = 2; engineA->writeBgCnt(3, mask << (base * 8), data << (base * 8));            break; // BG3CNT (engine A)
            case 0x4000010:
            case 0x4000011: base -= 0x4000010; size = 2; engineA->writeBgHOfs(0, mask << (base * 8), data << (base * 8));           break; // BG0HOFS (engine A)
            case 0x4000012:
            case 0x4000013: base -= 0x4000012; size = 2; engineA->writeBgVOfs(0, mask << (base * 8), data << (base * 8));           break; // BG0VOFS (engine A)
            case 0x4000014:
            case 0x4000015: base -= 0x4000014; size = 2; engineA->writeBgHOfs(1, mask << (base * 8), data << (base * 8));           break; // BG1HOFS (engine A)
            case 0x4000016:
            case 0x4000017: base -= 0x4000016; size = 2; engineA->writeBgVOfs(1, mask << (base * 8), data << (base * 8));           break; // BG1VOFS (engine A)
            case 0x4000018:
            case 0x4000019: base -= 0x4000018; size = 2; engineA->writeBgHOfs(2, mask << (base * 8), data << (base * 8));           break; // BG2HOFS (engine A)
            case 0x400001A:
            case 0x400001B: base -= 0x400001A; size = 2; engineA->writeBgVOfs(2, mask << (base * 8), data << (base * 8));           break; // BG2VOFS (engine A)
            case 0x400001C:
            case 0x400001D: base -= 0x400001C; size = 2; engineA->writeBgHOfs(3, mask << (base * 8), data << (base * 8));           break; // BG3HOFS (engine A)
            case 0x400001E:
            case 0x400001F: base -= 0x400001E; size = 2; engineA->writeBgVOfs(3, mask << (base * 8), data << (base * 8));           break; // BG3VOFS (engine A)
            case 0x4000020:
            case 0x4000021: base -= 0x4000020; size = 2; engineA->writeBgPA(2, mask << (base * 8), data << (base * 8));             break; // BG2PA (engine A)
            case 0x4000022:
            case 0x4000023: base -= 0x4000022; size = 2; engineA->writeBgPB(2, mask << (base * 8), data << (base * 8));             break; // BG2PB (engine A)
            case 0x4000024:
            case 0x4000025: base -= 0x4000024; size = 2; engineA->writeBgPC(2, mask << (base * 8), data << (base * 8));             break; // BG2PC (engine A)
            case 0x4000026:
            case 0x4000027: base -= 0x4000026; size = 2; engineA->writeBgPD(2, mask << (base * 8), data << (base * 8));             break; // BG2PD (engine A)
            case 0x4000028:
            case 0x4000029:
            case 0x400002A:
            case 0x400002B: base -= 0x4000028; size = 4; engineA->writeBgX(2, mask << (base * 8), data << (base * 8));              break; // BG2X (engine A)
            case 0x400002C:
            case 0x400002D:
            case 0x400002E:
            case 0x400002F: base -= 0x400002C; size = 4; engineA->writeBgY(2, mask << (base * 8), data << (base * 8));              break; // BG2Y (engine A)
            case 0x4000030:
            case 0x4000031: base -= 0x4000030; size = 2; engineA->writeBgPA(3, mask << (base * 8), data << (base * 8));             break; // BG3PA (engine A)
            case 0x4000032:
            case 0x4000033: base -= 0x4000032; size = 2; engineA->writeBgPB(3, mask << (base * 8), data << (base * 8));             break; // BG3PB (engine A)
            case 0x4000034:
            case 0x4000035: base -= 0x4000034; size = 2; engineA->writeBgPC(3, mask << (base * 8), data << (base * 8));             break; // BG3PC (engine A)
            case 0x4000036:
            case 0x4000037: base -= 0x4000036; size = 2; engineA->writeBgPD(3, mask << (base * 8), data << (base * 8));             break; // BG3PD (engine A)
            case 0x4000038:
            case 0x4000039:
            case 0x400003A:
            case 0x400003B: base -= 0x4000038; size = 4; engineA->writeBgX(3, mask << (base * 8), data << (base * 8));              break; // BG3X (engine A)
            case 0x400003C:
            case 0x400003D:
            case 0x400003E:
            case 0x400003F: base -= 0x400003C; size = 4; engineA->writeBgY(3, mask << (base * 8), data << (base * 8));              break; // BG3Y (engine A)
            case 0x4000060:
            case 0x4000061: base -= 0x4000060; size = 2; gpu3DRenderer->writeDisp3DCnt(mask << (base * 8), data << (base * 8));     break; // DISP3DCNT
            case 0x4000064:
            case 0x4000065:
            case 0x4000066:
            case 0x4000067: base -= 0x4000064; size = 4; gpu->writeDispCapCnt(mask << (base * 8), data << (base * 8));              break; // DISPCAPCNT
            case 0x400006C:
            case 0x400006D: base -= 0x400006C; size = 2; engineA->writeMasterBright(mask << (base * 8), data << (base * 8));        break; // MASTER_BRIGHT (engine A)
            case 0x40000B0:
            case 0x40000B1:
            case 0x40000B2:
            case 0x40000B3: base -= 0x40000B0; size = 4; dma9->writeDmaSad(0, mask << (base * 8), data << (base * 8));              break; // DMA0SAD (ARM9)
            case 0x40000B4:
            case 0x40000B5:
            case 0x40000B6:
            case 0x40000B7: base -= 0x40000B4; size = 4; dma9->writeDmaDad(0, mask << (base * 8), data << (base * 8));              break; // DMA0DAD (ARM9)
            case 0x40000B8:
            case 0x40000B9:
            case 0x40000BA:
            case 0x40000BB: base -= 0x40000B8; size = 4; dma9->writeDmaCnt(0, mask << (base * 8), data << (base * 8));              break; // DMA0CNT (ARM9)
            case 0x40000BC:
            case 0x40000BD:
            case 0x40000BE:
            case 0x40000BF: base -= 0x40000BC; size = 4; dma9->writeDmaSad(1, mask << (base * 8), data << (base * 8));              break; // DMA1SAD (ARM9)
            case 0x40000C0:
            case 0x40000C1:
            case 0x40000C2:
            case 0x40000C3: base -= 0x40000C0; size = 4; dma9->writeDmaDad(1, mask << (base * 8), data << (base * 8));              break; // DMA1DAD (ARM9)
            case 0x40000C4:
            case 0x40000C5:
            case 0x40000C6:
            case 0x40000C7: base -= 0x40000C4; size = 4; dma9->writeDmaCnt(1, mask << (base * 8), data << (base * 8));              break; // DMA1CNT (ARM9)
            case 0x40000C8:
            case 0x40000C9:
            case 0x40000CA:
            case 0x40000CB: base -= 0x40000C8; size = 4; dma9->writeDmaSad(2, mask << (base * 8), data << (base * 8));              break; // DMA2SAD (ARM9)
            case 0x40000CC:
            case 0x40000CD:
            case 0x40000CE:
            case 0x40000CF: base -= 0x40000CC; size = 4; dma9->writeDmaDad(2, mask << (base * 8), data << (base * 8));              break; // DMA2DAD (ARM9)
            case 0x40000D0:
            case 0x40000D1:
            case 0x40000D2:
            case 0x40000D3: base -= 0x40000D0; size = 4; dma9->writeDmaCnt(2, mask << (base * 8), data << (base * 8));              break; // DMA2CNT (ARM9)
            case 0x40000D4:
            case 0x40000D5:
            case 0x40000D6:
            case 0x40000D7: base -= 0x40000D4; size = 4; dma9->writeDmaSad(3, mask << (base * 8), data << (base * 8));              break; // DMA3SAD (ARM9)
            case 0x40000D8:
            case 0x40000D9:
            case 0x40000DA:
            case 0x40000DB: base -= 0x40000D8; size = 4; dma9->writeDmaDad(3, mask << (base * 8), data << (base * 8));              break; // DMA3DAD (ARM9)
            case 0x40000DC:
            case 0x40000DD:
            case 0x40000DE:
            case 0x40000DF: base -= 0x40000DC; size = 4; dma9->writeDmaCnt(3, mask << (base * 8), data << (base * 8));              break; // DMA3CNT (ARM9)
            case 0x40000E0:
            case 0x40000E1:
            case 0x40000E2:
            case 0x40000E3: base -= 0x40000E0; size = 4; writeDmaFill(0, mask << (base * 8), data << (base * 8));                   break; // DMA0FILL
            case 0x40000E4:
            case 0x40000E5:
            case 0x40000E6:
            case 0x40000E7: base -= 0x40000E4; size = 4; writeDmaFill(1, mask << (base * 8), data << (base * 8));                   break; // DMA1FILL
            case 0x40000E8:
            case 0x40000E9:
            case 0x40000EA:
            case 0x40000EB: base -= 0x40000E8; size = 4; writeDmaFill(2, mask << (base * 8), data << (base * 8));                   break; // DMA2FILL
            case 0x40000EC:
            case 0x40000ED:
            case 0x40000EE:
            case 0x40000EF: base -= 0x40000EC; size = 4; writeDmaFill(3, mask << (base * 8), data << (base * 8));                   break; // DMA3FILL
            case 0x4000100:
            case 0x4000101: base -= 0x4000100; size = 2; timers9->writeTmCntL(0, mask << (base * 8), data << (base * 8));           break; // TM0CNT_L (ARM9)
            case 0x4000102:
            case 0x4000103: base -= 0x4000102; size = 2; timers9->writeTmCntH(0, mask << (base * 8), data << (base * 8));           break; // TM0CNT_H (ARM9)
            case 0x4000104:
            case 0x4000105: base -= 0x4000104; size = 2; timers9->writeTmCntL(1, mask << (base * 8), data << (base * 8));           break; // TM1CNT_L (ARM9)
            case 0x4000106:
            case 0x4000107: base -= 0x4000106; size = 2; timers9->writeTmCntH(1, mask << (base * 8), data << (base * 8));           break; // TM1CNT_H (ARM9)
            case 0x4000108:
            case 0x4000109: base -= 0x4000108; size = 2; timers9->writeTmCntL(2, mask << (base * 8), data << (base * 8));           break; // TM2CNT_L (ARM9)
            case 0x400010A:
            case 0x400010B: base -= 0x400010A; size = 2; timers9->writeTmCntH(2, mask << (base * 8), data << (base * 8));           break; // TM2CNT_H (ARM9)
            case 0x400010C:
            case 0x400010D: base -= 0x400010C; size = 2; timers9->writeTmCntL(3, mask << (base * 8), data << (base * 8));           break; // TM3CNT_L (ARM9)
            case 0x400010E:
            case 0x400010F: base -= 0x400010E; size = 2; timers9->writeTmCntH(3, mask << (base * 8), data << (base * 8));           break; // TM3CNT_H (ARM9)
            case 0x4000180:
            case 0x4000181: base -= 0x4000180; size = 2; ipc->writeIpcSync9(mask << (base * 8), data << (base * 8));                break; // IPCSYNC (ARM9)
            case 0x4000184:
            case 0x4000185: base -= 0x4000184; size = 2; ipc->writeIpcFifoCnt9(mask << (base * 8), data << (base * 8));             break; // IPCFIFOCNT (ARM9)
            case 0x4000188:
            case 0x4000189:
            case 0x400018A:
            case 0x400018B: base -= 0x4000188; size = 4; ipc->writeIpcFifoSend9(mask << (base * 8), data << (base * 8));            break; // IPCFIFOSEND (ARM9)
            case 0x40001A0:
            case 0x40001A1: base -= 0x40001A0; size = 2; cart9->writeAuxSpiCnt(mask << (base * 8), data << (base * 8));             break; // AUXSPICNT (ARM9)
            case 0x40001A2: base -= 0x40001A2; size = 1; cart9->writeAuxSpiData(data << (base * 8));                                break; // AUXSPIDATA (ARM9)
            case 0x40001A4:
            case 0x40001A5:
            case 0x40001A6:
            case 0x40001A7: base -= 0x40001A4; size = 4; cart9->writeRomCtrl(mask << (base * 8), data << (base * 8));               break; // ROMCTRL (ARM9)
            case 0x40001A8:
            case 0x40001A9:
            case 0x40001AA:
            case 0x40001AB: base -= 0x40001A8; size = 4; cart9->writeRomCmdOutL(mask << (base * 8), data << (base * 8));            break; // ROMCMDOUT_L (ARM9)
            case 0x40001AC:
            case 0x40001AD:
            case 0x40001AE:
            case 0x40001AF: base -= 0x40001AC; size = 4; cart9->writeRomCmdOutH(mask << (base * 8), data << (base * 8));            break; // ROMCMDOUT_H (ARM9)
            case 0x4000208: base -= 0x4000208; size = 1; arm9->writeIme(data << (base * 8));                                        break; // IME (ARM9)
            case 0x4000210:
            case 0x4000211:
            case 0x4000212:
            case 0x4000213: base -= 0x4000210; size = 4; arm9->writeIe(mask << (base * 8), data << (base * 8));                     break; // IE (ARM9)
            case 0x4000214:
            case 0x4000215:
            case 0x4000216:
            case 0x4000217: base -= 0x4000214; size = 4; arm9->writeIrf(mask << (base * 8), data << (base * 8));                    break; // IF (ARM9)
            case 0x4000240: base -= 0x4000240; size = 1; writeVramCntA(data << (base * 8));                                         break; // VRAMCNT_A
            case 0x4000241: base -= 0x4000241; size = 1; writeVramCntB(data << (base * 8));                                         break; // VRAMCNT_B
            case 0x4000242: base -= 0x4000242; size = 1; writeVramCntC(data << (base * 8));                                         break; // VRAMCNT_C
            case 0x4000243: base -= 0x4000243; size = 1; writeVramCntD(data << (base * 8));                                         break; // VRAMCNT_D
            case 0x4000244: base -= 0x4000244; size = 1; writeVramCntE(data << (base * 8));                                         break; // VRAMCNT_E
            case 0x4000245: base -= 0x4000245; size = 1; writeVramCntF(data << (base * 8));                                         break; // VRAMCNT_F
            case 0x4000246: base -= 0x4000246; size = 1; writeVramCntG(data << (base * 8));                                         break; // VRAMCNT_G
            case 0x4000247: base -= 0x4000247; size = 1; writeWramCnt(data << (base * 8));                                          break; // WRAMCNT
            case 0x4000248: base -= 0x4000248; size = 1; writeVramCntH(data << (base * 8));                                         break; // VRAMCNT_H
            case 0x4000249: base -= 0x4000249; size = 1; writeVramCntI(data << (base * 8));                                         break; // VRAMCNT_I
            case 0x4000280:
            case 0x4000281: base -= 0x4000280; size = 2; math->writeDivCnt(mask << (base * 8), data << (base * 8));                 break; // DIVCNT
            case 0x4000290:
            case 0x4000291:
            case 0x4000292:
            case 0x4000293: base -= 0x4000290; size = 4; math->writeDivNumerL(mask << (base * 8), data << (base * 8));              break; // DIVNUMER_L
            case 0x4000294:
            case 0x4000295:
            case 0x4000296:
            case 0x4000297: base -= 0x4000294; size = 4; math->writeDivNumerH(mask << (base * 8), data << (base * 8));              break; // DIVNUMER_H
            case 0x4000298:
            case 0x4000299:
            case 0x400029A:
            case 0x400029B: base -= 0x4000298; size = 4; math->writeDivDenomL(mask << (base * 8), data << (base * 8));              break; // DIVDENOM_L
            case 0x400029C:
            case 0x400029D:
            case 0x400029E:
            case 0x400029F: base -= 0x400029C; size = 4; math->writeDivDenomH(mask << (base * 8), data << (base * 8));              break; // DIVDENOM_H
            case 0x40002B0:
            case 0x40002B1: base -= 0x40002B0; size = 2; math->writeSqrtCnt(mask << (base * 8), data << (base * 8));                break; // SQRTCNT
            case 0x40002B8:
            case 0x40002B9:
            case 0x40002BA:
            case 0x40002BB: base -= 0x40002B8; size = 4; math->writeSqrtParamL(mask << (base * 8), data << (base * 8));             break; // SQRTPARAM_L
            case 0x40002BC:
            case 0x40002BD:
            case 0x40002BE:
            case 0x40002BF: base -= 0x40002BC; size = 4; math->writeSqrtParamH(mask << (base * 8), data << (base * 8));             break; // SQRTPARAM_H
            case 0x4000300: base -= 0x4000300; size = 1; arm9->writePostFlg(data << (base * 8));                                    break; // POSTFLG (ARM9)
            case 0x4000304:
            case 0x4000305: base -= 0x4000304; size = 2; gpu->writePowCnt1(mask << (base * 8), data << (base * 8));                 break; // POWCNT1
            case 0x4000350:
            case 0x4000351:
            case 0x4000352:
            case 0x4000353: base -= 0x4000350; size = 4; gpu3DRenderer->writeClearColor(mask << (base * 8), data << (base * 8));    break; // CLEAR_COLOR
            case 0x4000354:
            case 0x4000355: base -= 0x4000354; size = 2; gpu3DRenderer->writeClearDepth(mask << (base * 8), data << (base * 8));    break; // CLEAR_DEPTH
            case 0x4000380:
            case 0x4000381: base -= 0x4000380; size = 2; gpu3DRenderer->writeToonTable(0,  mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x4000382:
            case 0x4000383: base -= 0x4000382; size = 2; gpu3DRenderer->writeToonTable(1,  mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x4000384:
            case 0x4000385: base -= 0x4000384; size = 2; gpu3DRenderer->writeToonTable(2,  mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x4000386:
            case 0x4000387: base -= 0x4000386; size = 2; gpu3DRenderer->writeToonTable(3,  mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x4000388:
            case 0x4000389: base -= 0x4000388; size = 2; gpu3DRenderer->writeToonTable(4,  mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x400038A:
            case 0x400038B: base -= 0x400038A; size = 2; gpu3DRenderer->writeToonTable(5,  mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x400038C:
            case 0x400038D: base -= 0x400038C; size = 2; gpu3DRenderer->writeToonTable(6,  mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x400038E:
            case 0x400038F: base -= 0x400038E; size = 2; gpu3DRenderer->writeToonTable(7,  mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x4000390:
            case 0x4000391: base -= 0x4000390; size = 2; gpu3DRenderer->writeToonTable(8,  mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x4000392:
            case 0x4000393: base -= 0x4000392; size = 2; gpu3DRenderer->writeToonTable(9,  mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x4000394:
            case 0x4000395: base -= 0x4000394; size = 2; gpu3DRenderer->writeToonTable(10, mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x4000396:
            case 0x4000397: base -= 0x4000396; size = 2; gpu3DRenderer->writeToonTable(11, mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x4000398:
            case 0x4000399: base -= 0x4000398; size = 2; gpu3DRenderer->writeToonTable(12, mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x400039A:
            case 0x400039B: base -= 0x400039A; size = 2; gpu3DRenderer->writeToonTable(13, mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x400039C:
            case 0x400039D: base -= 0x400039C; size = 2; gpu3DRenderer->writeToonTable(14, mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x400039E:
            case 0x400039F: base -= 0x400039E; size = 2; gpu3DRenderer->writeToonTable(15, mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x40003A0:
            case 0x40003A1: base -= 0x40003A0; size = 2; gpu3DRenderer->writeToonTable(16, mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x40003A2:
            case 0x40003A3: base -= 0x40003A2; size = 2; gpu3DRenderer->writeToonTable(17, mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x40003A4:
            case 0x40003A5: base -= 0x40003A4; size = 2; gpu3DRenderer->writeToonTable(18, mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x40003A6:
            case 0x40003A7: base -= 0x40003A6; size = 2; gpu3DRenderer->writeToonTable(19, mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x40003A8:
            case 0x40003A9: base -= 0x40003A8; size = 2; gpu3DRenderer->writeToonTable(20, mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x40003AA:
            case 0x40003AB: base -= 0x40003AA; size = 2; gpu3DRenderer->writeToonTable(21, mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x40003AC:
            case 0x40003AD: base -= 0x40003AC; size = 2; gpu3DRenderer->writeToonTable(22, mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x40003AE:
            case 0x40003AF: base -= 0x40003AE; size = 2; gpu3DRenderer->writeToonTable(23, mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x40003B0:
            case 0x40003B1: base -= 0x40003B0; size = 2; gpu3DRenderer->writeToonTable(24, mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x40003B2:
            case 0x40003B3: base -= 0x40003B2; size = 2; gpu3DRenderer->writeToonTable(25, mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x40003B4:
            case 0x40003B5: base -= 0x40003B4; size = 2; gpu3DRenderer->writeToonTable(26, mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x40003B6:
            case 0x40003B7: base -= 0x40003B6; size = 2; gpu3DRenderer->writeToonTable(27, mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x40003B8:
            case 0x40003B9: base -= 0x40003B8; size = 2; gpu3DRenderer->writeToonTable(28, mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x40003BA:
            case 0x40003BB: base -= 0x40003BA; size = 2; gpu3DRenderer->writeToonTable(29, mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x40003BC:
            case 0x40003BD: base -= 0x40003BC; size = 2; gpu3DRenderer->writeToonTable(30, mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x40003BE:
            case 0x40003BF: base -= 0x40003BE; size = 2; gpu3DRenderer->writeToonTable(31, mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x4000400:
            case 0x4000401:
            case 0x4000402:
            case 0x4000403: base -= 0x4000400; size = 4; gpu3D->writeGxFifo(mask << (base * 8), data << (base * 8));                break; // GXFIFO
            case 0x4000404:
            case 0x4000405:
            case 0x4000406:
            case 0x4000407: base -= 0x4000404; size = 4; gpu3D->writeGxFifo(mask << (base * 8), data << (base * 8));                break; // GXFIFO
            case 0x4000408:
            case 0x4000409:
            case 0x400040A:
            case 0x400040B: base -= 0x4000408; size = 4; gpu3D->writeGxFifo(mask << (base * 8), data << (base * 8));                break; // GXFIFO
            case 0x400040C:
            case 0x400040D:
            case 0x400040E:
            case 0x400040F: base -= 0x400040C; size = 4; gpu3D->writeGxFifo(mask << (base * 8), data << (base * 8));                break; // GXFIFO
            case 0x4000410:
            case 0x4000411:
            case 0x4000412:
            case 0x4000413: base -= 0x4000410; size = 4; gpu3D->writeGxFifo(mask << (base * 8), data << (base * 8));                break; // GXFIFO
            case 0x4000414:
            case 0x4000415:
            case 0x4000416:
            case 0x4000417: base -= 0x4000414; size = 4; gpu3D->writeGxFifo(mask << (base * 8), data << (base * 8));                break; // GXFIFO
            case 0x4000418:
            case 0x4000419:
            case 0x400041A:
            case 0x400041B: base -= 0x4000418; size = 4; gpu3D->writeGxFifo(mask << (base * 8), data << (base * 8));                break; // GXFIFO
            case 0x400041C:
            case 0x400041D:
            case 0x400041E:
            case 0x400041F: base -= 0x400041C; size = 4; gpu3D->writeGxFifo(mask << (base * 8), data << (base * 8));                break; // GXFIFO
            case 0x4000420:
            case 0x4000421:
            case 0x4000422:
            case 0x4000423: base -= 0x4000420; size = 4; gpu3D->writeGxFifo(mask << (base * 8), data << (base * 8));                break; // GXFIFO
            case 0x4000424:
            case 0x4000425:
            case 0x4000426:
            case 0x4000427: base -= 0x4000424; size = 4; gpu3D->writeGxFifo(mask << (base * 8), data << (base * 8));                break; // GXFIFO
            case 0x4000428:
            case 0x4000429:
            case 0x400042A:
            case 0x400042B: base -= 0x4000428; size = 4; gpu3D->writeGxFifo(mask << (base * 8), data << (base * 8));                break; // GXFIFO
            case 0x400042C:
            case 0x400042D:
            case 0x400042E:
            case 0x400042F: base -= 0x400042C; size = 4; gpu3D->writeGxFifo(mask << (base * 8), data << (base * 8));                break; // GXFIFO
            case 0x4000430:
            case 0x4000431:
            case 0x4000432:
            case 0x4000433: base -= 0x4000430; size = 4; gpu3D->writeGxFifo(mask << (base * 8), data << (base * 8));                break; // GXFIFO
            case 0x4000434:
            case 0x4000435:
            case 0x4000436:
            case 0x4000437: base -= 0x4000434; size = 4; gpu3D->writeGxFifo(mask << (base * 8), data << (base * 8));                break; // GXFIFO
            case 0x4000438:
            case 0x4000439:
            case 0x400043A:
            case 0x400043B: base -= 0x4000438; size = 4; gpu3D->writeGxFifo(mask << (base * 8), data << (base * 8));                break; // GXFIFO
            case 0x400043C:
            case 0x400043D:
            case 0x400043E:
            case 0x400043F: base -= 0x400043C; size = 4; gpu3D->writeGxFifo(mask << (base * 8), data << (base * 8));                break; // GXFIFO
            case 0x4000440:
            case 0x4000441:
            case 0x4000442:
            case 0x4000443: base -= 0x4000440; size = 4; gpu3D->writeMtxMode(mask << (base * 8), data << (base * 8));               break; // MTX_MODE
            case 0x4000444:
            case 0x4000445:
            case 0x4000446:
            case 0x4000447: base -= 0x4000444; size = 4; gpu3D->writeMtxPush(mask << (base * 8), data << (base * 8));               break; // MTX_PUSH
            case 0x4000448:
            case 0x4000449:
            case 0x400044A:
            case 0x400044B: base -= 0x4000448; size = 4; gpu3D->writeMtxPop(mask << (base * 8), data << (base * 8));                break; // MTX_POP
            case 0x400044C:
            case 0x400044D:
            case 0x400044E:
            case 0x400044F: base -= 0x400044C; size = 4; gpu3D->writeMtxStore(mask << (base * 8), data << (base * 8));              break; // MTX_STORE
            case 0x4000450:
            case 0x4000451:
            case 0x4000452:
            case 0x4000453: base -= 0x4000450; size = 4; gpu3D->writeMtxRestore(mask << (base * 8), data << (base * 8));            break; // MTX_RESTORE
            case 0x4000454:
            case 0x4000455:
            case 0x4000456:
            case 0x4000457: base -= 0x4000454; size = 4; gpu3D->writeMtxIdentity(mask << (base * 8), data << (base * 8));           break; // MTX_IDENTITY
            case 0x4000458:
            case 0x4000459:
            case 0x400045A:
            case 0x400045B: base -= 0x4000458; size = 4; gpu3D->writeMtxLoad44(mask << (base * 8), data << (base * 8));             break; // MTX_LOAD_4x4
            case 0x400045C:
            case 0x400045D:
            case 0x400045E:
            case 0x400045F: base -= 0x400045C; size = 4; gpu3D->writeMtxLoad43(mask << (base * 8), data << (base * 8));             break; // MTX_LOAD_4x3
            case 0x4000460:
            case 0x4000461:
            case 0x4000462:
            case 0x4000463: base -= 0x4000460; size = 4; gpu3D->writeMtxMult44(mask << (base * 8), data << (base * 8));             break; // MTX_MULT_4x4
            case 0x4000464:
            case 0x4000465:
            case 0x4000466:
            case 0x4000467: base -= 0x4000464; size = 4; gpu3D->writeMtxMult43(mask << (base * 8), data << (base * 8));             break; // MTX_MULT_4x3
            case 0x4000468:
            case 0x4000469:
            case 0x400046A:
            case 0x400046B: base -= 0x4000468; size = 4; gpu3D->writeMtxMult33(mask << (base * 8), data << (base * 8));             break; // MTX_MULT_3x3
            case 0x400046C:
            case 0x400046D:
            case 0x400046E:
            case 0x400046F: base -= 0x400046C; size = 4; gpu3D->writeMtxScale(mask << (base * 8), data << (base * 8));              break; // MTX_SCALE
            case 0x4000470:
            case 0x4000471:
            case 0x4000472:
            case 0x4000473: base -= 0x4000470; size = 4; gpu3D->writeMtxTrans(mask << (base * 8), data << (base * 8));              break; // MTX_TRANS
            case 0x4000480:
            case 0x4000481:
            case 0x4000482:
            case 0x4000483: base -= 0x4000480; size = 4; gpu3D->writeColor(mask << (base * 8), data << (base * 8));                 break; // COLOR
            case 0x4000484:
            case 0x4000485:
            case 0x4000486:
            case 0x4000487: base -= 0x4000484; size = 4; gpu3D->writeNormal(mask << (base * 8), data << (base * 8));                break; // NORMAL
            case 0x4000488:
            case 0x4000489:
            case 0x400048A:
            case 0x400048B: base -= 0x4000488; size = 4; gpu3D->writeTexCoord(mask << (base * 8), data << (base * 8));              break; // TEXCOORD
            case 0x400048C:
            case 0x400048D:
            case 0x400048E:
            case 0x400048F: base -= 0x400048C; size = 4; gpu3D->writeVtx16(mask << (base * 8), data << (base * 8));                 break; // VTX_16
            case 0x4000490:
            case 0x4000491:
            case 0x4000492:
            case 0x4000493: base -= 0x4000490; size = 4; gpu3D->writeVtx10(mask << (base * 8), data << (base * 8));                 break; // VTX_10
            case 0x4000494:
            case 0x4000495:
            case 0x4000496:
            case 0x4000497: base -= 0x4000494; size = 4; gpu3D->writeVtxXY(mask << (base * 8), data << (base * 8));                 break; // VTX_XY
            case 0x4000498:
            case 0x4000499:
            case 0x400049A:
            case 0x400049B: base -= 0x4000498; size = 4; gpu3D->writeVtxXZ(mask << (base * 8), data << (base * 8));                 break; // VTX_XZ
            case 0x400049C:
            case 0x400049D:
            case 0x400049E:
            case 0x400049F: base -= 0x400049C; size = 4; gpu3D->writeVtxYZ(mask << (base * 8), data << (base * 8));                 break; // VTX_YZ
            case 0x40004A0:
            case 0x40004A1:
            case 0x40004A2:
            case 0x40004A3: base -= 0x40004A0; size = 4; gpu3D->writeVtxDiff(mask << (base * 8), data << (base * 8));               break; // VTX_DIFF
            case 0x40004A4:
            case 0x40004A5:
            case 0x40004A6:
            case 0x40004A7: base -= 0x40004A4; size = 4; gpu3D->writePolygonAttr(mask << (base * 8), data << (base * 8));           break; // POLYGON_ATTR
            case 0x40004A8:
            case 0x40004A9:
            case 0x40004AA:
            case 0x40004AB: base -= 0x40004A8; size = 4; gpu3D->writeTexImageParam(mask << (base * 8), data << (base * 8));         break; // TEXIMAGE_PARAM
            case 0x40004AC:
            case 0x40004AD:
            case 0x40004AE:
            case 0x40004AF: base -= 0x40004AC; size = 4; gpu3D->writePlttBase(mask << (base * 8), data << (base * 8));              break; // PLTT_BASE
            case 0x40004C0:
            case 0x40004C1:
            case 0x40004C2:
            case 0x40004C3: base -= 0x40004C0; size = 4; gpu3D->writeDifAmb(mask << (base * 8), data << (base * 8));                break; // DIF_AMB
            case 0x40004C4:
            case 0x40004C5:
            case 0x40004C6:
            case 0x40004C7: base -= 0x40004C4; size = 4; gpu3D->writeSpeEmi(mask << (base * 8), data << (base * 8));                break; // SPE_EMI
            case 0x40004C8:
            case 0x40004C9:
            case 0x40004CA:
            case 0x40004CB: base -= 0x40004C8; size = 4; gpu3D->writeLightVector(mask << (base * 8), data << (base * 8));           break; // LIGHT_VECTOR
            case 0x40004CC:
            case 0x40004CD:
            case 0x40004CE:
            case 0x40004CF: base -= 0x40004CC; size = 4; gpu3D->writeLightColor(mask << (base * 8), data << (base * 8));            break; // LIGHT_COLOR
            case 0x40004D0:
            case 0x40004D1:
            case 0x40004D2:
            case 0x40004D3: base -= 0x40004D0; size = 4; gpu3D->writeShininess(mask << (base * 8), data << (base * 8));             break; // SHININESS
            case 0x4000500:
            case 0x4000501:
            case 0x4000502:
            case 0x4000503: base -= 0x4000500; size = 4; gpu3D->writeBeginVtxs(mask << (base * 8), data << (base * 8));             break; // BEGIN_VTXS
            case 0x4000504:
            case 0x4000505:
            case 0x4000506:
            case 0x4000507: base -= 0x4000504; size = 4; gpu3D->writeEndVtxs(mask << (base * 8), data << (base * 8));               break; // END_VTXS
            case 0x4000540:
            case 0x4000541:
            case 0x4000542:
            case 0x4000543: base -= 0x4000540; size = 4; gpu3D->writeSwapBuffers(mask << (base * 8), data << (base * 8));           break; // SWAP_BUFFERS
            case 0x4000580:
            case 0x4000581:
            case 0x4000582:
            case 0x4000583: base -= 0x4000580; size = 4; gpu3D->writeViewport(mask << (base * 8), data << (base * 8));              break; // VIEWPORT
            case 0x40005C0:
            case 0x40005C1:
            case 0x40005C2:
            case 0x40005C3: base -= 0x40005C0; size = 4; gpu3D->writeBoxTest(mask << (base * 8), data << (base * 8));               break; // BOX_TEST
            case 0x40005C4:
            case 0x40005C5:
            case 0x40005C6:
            case 0x40005C7: base -= 0x40005C4; size = 4; gpu3D->writePosTest(mask << (base * 8), data << (base * 8));               break; // POS_TEST
            case 0x40005C8:
            case 0x40005C9:
            case 0x40005CA:
            case 0x40005CB: base -= 0x40005C8; size = 4; gpu3D->writeVecTest(mask << (base * 8), data << (base * 8));               break; // VEC_TEST
            case 0x4000600:
            case 0x4000601:
            case 0x4000602:
            case 0x4000603: base -= 0x4000600; size = 4; gpu3D->writeGxStat(mask << (base * 8), data << (base * 8));                break; // GXSTAT
            case 0x4001000:
            case 0x4001001:
            case 0x4001002:
            case 0x4001003: base -= 0x4001000; size = 4; engineB->writeDispCnt(mask << (base * 8), data << (base * 8));             break; // DISPCNT (engine B)
            case 0x4001008:
            case 0x4001009: base -= 0x4001008; size = 2; engineB->writeBgCnt(0, mask << (base * 8), data << (base * 8));            break; // BG0CNT (engine B)
            case 0x400100A:
            case 0x400100B: base -= 0x400100A; size = 2; engineB->writeBgCnt(1, mask << (base * 8), data << (base * 8));            break; // BG1CNT (engine B)
            case 0x400100C:
            case 0x400100D: base -= 0x400100C; size = 2; engineB->writeBgCnt(2, mask << (base * 8), data << (base * 8));            break; // BG2CNT (engine B)
            case 0x400100E:
            case 0x400100F: base -= 0x400100E; size = 2; engineB->writeBgCnt(3, mask << (base * 8), data << (base * 8));            break; // BG3CNT (engine B)
            case 0x4001010:
            case 0x4001011: base -= 0x4001010; size = 2; engineB->writeBgHOfs(0, mask << (base * 8), data << (base * 8));           break; // BG0HOFS (engine B)
            case 0x4001012:
            case 0x4001013: base -= 0x4001012; size = 2; engineB->writeBgVOfs(0, mask << (base * 8), data << (base * 8));           break; // BG0VOFS (engine B)
            case 0x4001014:
            case 0x4001015: base -= 0x4001014; size = 2; engineB->writeBgHOfs(1, mask << (base * 8), data << (base * 8));           break; // BG1HOFS (engine B)
            case 0x4001016:
            case 0x4001017: base -= 0x4001016; size = 2; engineB->writeBgVOfs(1, mask << (base * 8), data << (base * 8));           break; // BG1VOFS (engine B)
            case 0x4001018:
            case 0x4001019: base -= 0x4001018; size = 2; engineB->writeBgHOfs(2, mask << (base * 8), data << (base * 8));           break; // BG2HOFS (engine B)
            case 0x400101A:
            case 0x400101B: base -= 0x400101A; size = 2; engineB->writeBgVOfs(2, mask << (base * 8), data << (base * 8));           break; // BG2VOFS (engine B)
            case 0x400101C:
            case 0x400101D: base -= 0x400101C; size = 2; engineB->writeBgHOfs(3, mask << (base * 8), data << (base * 8));           break; // BG3HOFS (engine B)
            case 0x400101E:
            case 0x400101F: base -= 0x400101E; size = 2; engineB->writeBgVOfs(3, mask << (base * 8), data << (base * 8));           break; // BG3VOFS (engine B)
            case 0x4001020:
            case 0x4001021: base -= 0x4001020; size = 2; engineB->writeBgPA(2, mask << (base * 8), data << (base * 8));             break; // BG2PA (engine B)
            case 0x4001022:
            case 0x4001023: base -= 0x4001022; size = 2; engineB->writeBgPB(2, mask << (base * 8), data << (base * 8));             break; // BG2PB (engine B)
            case 0x4001024:
            case 0x4001025: base -= 0x4001024; size = 2; engineB->writeBgPC(2, mask << (base * 8), data << (base * 8));             break; // BG2PC (engine B)
            case 0x4001026:
            case 0x4001027: base -= 0x4001026; size = 2; engineB->writeBgPD(2, mask << (base * 8), data << (base * 8));             break; // BG2PD (engine B)
            case 0x4001028:
            case 0x4001029:
            case 0x400102A:
            case 0x400102B: base -= 0x4001028; size = 4; engineB->writeBgX(2, mask << (base * 8), data << (base * 8));              break; // BG2X (engine B)
            case 0x400102C:
            case 0x400102D:
            case 0x400102E:
            case 0x400102F: base -= 0x400102C; size = 4; engineB->writeBgY(2, mask << (base * 8), data << (base * 8));              break; // BG2Y (engine B)
            case 0x4001030:
            case 0x4001031: base -= 0x4001030; size = 2; engineB->writeBgPA(3, mask << (base * 8), data << (base * 8));             break; // BG3PA (engine B)
            case 0x4001032:
            case 0x4001033: base -= 0x4001032; size = 2; engineB->writeBgPB(3, mask << (base * 8), data << (base * 8));             break; // BG3PB (engine B)
            case 0x4001034:
            case 0x4001035: base -= 0x4001034; size = 2; engineB->writeBgPC(3, mask << (base * 8), data << (base * 8));             break; // BG3PC (engine B)
            case 0x4001036:
            case 0x4001037: base -= 0x4001036; size = 2; engineB->writeBgPD(3, mask << (base * 8), data << (base * 8));             break; // BG3PD (engine B)
            case 0x4001038:
            case 0x4001039:
            case 0x400103A:
            case 0x400103B: base -= 0x4001038; size = 4; engineB->writeBgX(3, mask << (base * 8), data << (base * 8));              break; // BG3X (engine B)
            case 0x400103C:
            case 0x400103D:
            case 0x400103E:
            case 0x400103F: base -= 0x400103C; size = 4; engineB->writeBgY(3, mask << (base * 8), data << (base * 8));              break; // BG3Y (engine B)
            case 0x400106C:
            case 0x400106D: base -= 0x400106C; size = 2; engineB->writeMasterBright(mask << (base * 8), data << (base * 8));        break; // MASTER_BRIGHT (engine B)

            default:
            {
                // Catch unknown writes
                if (i == 0)
                {
                    printf("Unknown ARM9 I/O register write: 0x%X\n", address);
                    return;
                }

                // Ignore unknown writes if they occur after the first byte
                // This is in case, for example, a 16-bit register is accessed with a 32-bit write
                i++;
                continue;
            }
        }

        i += size - base;
    }
}

template <typename T> void Memory::ioWrite7(uint32_t address, T value)
{
    unsigned int i = 0;

    // Write a value to one or more ARM7 I/O registers
    while (i < sizeof(T))
    {
        uint32_t base = address + i;
        unsigned int size;
        uint32_t mask = ((uint64_t)1 << ((sizeof(T) - i) * 8)) - 1;
        uint32_t data = value >> (i * 8);

        switch (base)
        {
            case 0x4000004:
            case 0x4000005: base -= 0x4000004; size = 2; gpu->writeDispStat7(mask << (base * 8), data << (base * 8));     break; // DISPSTAT (ARM7)
            case 0x40000B0:
            case 0x40000B1:
            case 0x40000B2:
            case 0x40000B3: base -= 0x40000B0; size = 4; dma7->writeDmaSad(0, mask << (base * 8), data << (base * 8));    break; // DMA0SAD (ARM7)
            case 0x40000B4:
            case 0x40000B5:
            case 0x40000B6:
            case 0x40000B7: base -= 0x40000B4; size = 4; dma7->writeDmaDad(0, mask << (base * 8), data << (base * 8));    break; // DMA0DAD (ARM7)
            case 0x40000B8:
            case 0x40000B9:
            case 0x40000BA:
            case 0x40000BB: base -= 0x40000B8; size = 4; dma7->writeDmaCnt(0, mask << (base * 8), data << (base * 8));    break; // DMA0CNT (ARM7)
            case 0x40000BC:
            case 0x40000BD:
            case 0x40000BE:
            case 0x40000BF: base -= 0x40000BC; size = 4; dma7->writeDmaSad(1, mask << (base * 8), data << (base * 8));    break; // DMA1SAD (ARM7)
            case 0x40000C0:
            case 0x40000C1:
            case 0x40000C2:
            case 0x40000C3: base -= 0x40000C0; size = 4; dma7->writeDmaDad(1, mask << (base * 8), data << (base * 8));    break; // DMA1DAD (ARM7)
            case 0x40000C4:
            case 0x40000C5:
            case 0x40000C6:
            case 0x40000C7: base -= 0x40000C4; size = 4; dma7->writeDmaCnt(1, mask << (base * 8), data << (base * 8));    break; // DMA1CNT (ARM7)
            case 0x40000C8:
            case 0x40000C9:
            case 0x40000CA:
            case 0x40000CB: base -= 0x40000C8; size = 4; dma7->writeDmaSad(2, mask << (base * 8), data << (base * 8));    break; // DMA2SAD (ARM7)
            case 0x40000CC:
            case 0x40000CD:
            case 0x40000CE:
            case 0x40000CF: base -= 0x40000CC; size = 4; dma7->writeDmaDad(2, mask << (base * 8), data << (base * 8));    break; // DMA2DAD (ARM7)
            case 0x40000D0:
            case 0x40000D1:
            case 0x40000D2:
            case 0x40000D3: base -= 0x40000D0; size = 4; dma7->writeDmaCnt(2, mask << (base * 8), data << (base * 8));    break; // DMA2CNT (ARM7)
            case 0x40000D4:
            case 0x40000D5:
            case 0x40000D6:
            case 0x40000D7: base -= 0x40000D4; size = 4; dma7->writeDmaSad(3, mask << (base * 8), data << (base * 8));    break; // DMA3SAD (ARM7)
            case 0x40000D8:
            case 0x40000D9:
            case 0x40000DA:
            case 0x40000DB: base -= 0x40000D8; size = 4; dma7->writeDmaDad(3, mask << (base * 8), data << (base * 8));    break; // DMA3DAD (ARM7)
            case 0x40000DC:
            case 0x40000DD:
            case 0x40000DE:
            case 0x40000DF: base -= 0x40000DC; size = 4; dma7->writeDmaCnt(3, mask << (base * 8), data << (base * 8));    break; // DMA3CNT (ARM7)
            case 0x4000100:
            case 0x4000101: base -= 0x4000100; size = 2; timers7->writeTmCntL(0, mask << (base * 8), data << (base * 8)); break; // TM0CNT_L (ARM7)
            case 0x4000102:
            case 0x4000103: base -= 0x4000102; size = 2; timers7->writeTmCntH(0, mask << (base * 8), data << (base * 8)); break; // TM0CNT_H (ARM7)
            case 0x4000104:
            case 0x4000105: base -= 0x4000104; size = 2; timers7->writeTmCntL(1, mask << (base * 8), data << (base * 8)); break; // TM1CNT_L (ARM7)
            case 0x4000106:
            case 0x4000107: base -= 0x4000106; size = 2; timers7->writeTmCntH(1, mask << (base * 8), data << (base * 8)); break; // TM1CNT_H (ARM7)
            case 0x4000108:
            case 0x4000109: base -= 0x4000108; size = 2; timers7->writeTmCntL(2, mask << (base * 8), data << (base * 8)); break; // TM2CNT_L (ARM7)
            case 0x400010A:
            case 0x400010B: base -= 0x400010A; size = 2; timers7->writeTmCntH(2, mask << (base * 8), data << (base * 8)); break; // TM2CNT_H (ARM7)
            case 0x400010C:
            case 0x400010D: base -= 0x400010C; size = 2; timers7->writeTmCntL(3, mask << (base * 8), data << (base * 8)); break; // TM3CNT_L (ARM7)
            case 0x400010E:
            case 0x400010F: base -= 0x400010E; size = 2; timers7->writeTmCntH(3, mask << (base * 8), data << (base * 8)); break; // TM3CNT_H (ARM7)
            case 0x4000138: base -= 0x4000138; size = 1; rtc->writeRtc(data << (base * 8));                               break; // RTC
            case 0x4000180:
            case 0x4000181: base -= 0x4000180; size = 2; ipc->writeIpcSync7(mask << (base * 8), data << (base * 8));      break; // IPCSYNC (ARM7)
            case 0x4000184:
            case 0x4000185: base -= 0x4000184; size = 2; ipc->writeIpcFifoCnt7(mask << (base * 8), data << (base * 8));   break; // IPCFIFOCNT (ARM7)
            case 0x4000188:
            case 0x4000189:
            case 0x400018A:
            case 0x400018B: base -= 0x4000188; size = 4; ipc->writeIpcFifoSend7(mask << (base * 8), data << (base * 8));  break; // IPCFIFOSEND (ARM7)
            case 0x40001A0:
            case 0x40001A1: base -= 0x40001A0; size = 2; cart7->writeAuxSpiCnt(mask << (base * 8), data << (base * 8));   break; // AUXSPICNT (ARM7)
            case 0x40001A2: base -= 0x40001A2; size = 1; cart7->writeAuxSpiData(data << (base * 8));                      break; // AUXSPIDATA (ARM7)
            case 0x40001A4:
            case 0x40001A5:
            case 0x40001A6:
            case 0x40001A7: base -= 0x40001A4; size = 4; cart7->writeRomCtrl(mask << (base * 8), data << (base * 8));     break; // ROMCTRL (ARM7)
            case 0x40001A8:
            case 0x40001A9:
            case 0x40001AA:
            case 0x40001AB: base -= 0x40001A8; size = 4; cart7->writeRomCmdOutL(mask << (base * 8), data << (base * 8));  break; // ROMCMDOUT_L (ARM7)
            case 0x40001AC:
            case 0x40001AD:
            case 0x40001AE:
            case 0x40001AF: base -= 0x40001AC; size = 4; cart7->writeRomCmdOutH(mask << (base * 8), data << (base * 8));  break; // ROMCMDOUT_H (ARM7)
            case 0x40001C0:
            case 0x40001C1: base -= 0x40001C0; size = 2; spi->writeSpiCnt(mask << (base * 8), data << (base * 8));        break; // SPICNT
            case 0x40001C2: base -= 0x40001C2; size = 1; spi->writeSpiData(data << (base * 8));                           break; // SPIDATA
            case 0x4000208: base -= 0x4000208; size = 1; arm7->writeIme(data << (base * 8));                              break; // IME (ARM7)
            case 0x4000210:
            case 0x4000211:
            case 0x4000212:
            case 0x4000213: base -= 0x4000210; size = 4; arm7->writeIe(mask << (base * 8), data << (base * 8));           break; // IE (ARM7)
            case 0x4000214:
            case 0x4000215:
            case 0x4000216:
            case 0x4000217: base -= 0x4000214; size = 4; arm7->writeIrf(mask << (base * 8), data << (base * 8));          break; // IF (ARM7)
            case 0x4000300: base -= 0x4000300; size = 1; arm7->writePostFlg(data << (base * 8));                          break; // POSTFLG (ARM7)
            case 0x4000301: base -= 0x4000301; size = 1; writeHaltCnt(data << (base * 8));                                break; // HALTCNT
            case 0x4000400:
            case 0x4000401:
            case 0x4000402:
            case 0x4000403: base -= 0x4000400; size = 4; spu->writeSoundCnt(0, mask << (base * 8), data << (base * 8));   break; // SOUND0CNT
            case 0x4000404:
            case 0x4000405:
            case 0x4000406:
            case 0x4000407: base -= 0x4000404; size = 4; spu->writeSoundSad(0, mask << (base * 8), data << (base * 8));   break; // SOUND0SAD
            case 0x4000408:
            case 0x4000409: base -= 0x4000408; size = 2; spu->writeSoundTmr(0, mask << (base * 8), data << (base * 8));   break; // SOUND0TMR
            case 0x400040A:
            case 0x400040B: base -= 0x400040A; size = 2; spu->writeSoundPnt(0, mask << (base * 8), data << (base * 8));   break; // SOUND0PNT
            case 0x400040C:
            case 0x400040D:
            case 0x400040E:
            case 0x400040F: base -= 0x400040C; size = 4; spu->writeSoundLen(0, mask << (base * 8), data << (base * 8));   break; // SOUND0LEN
            case 0x4000410:
            case 0x4000411:
            case 0x4000412:
            case 0x4000413: base -= 0x4000410; size = 4; spu->writeSoundCnt(1, mask << (base * 8), data << (base * 8));   break; // SOUND1CNT
            case 0x4000414:
            case 0x4000415:
            case 0x4000416:
            case 0x4000417: base -= 0x4000414; size = 4; spu->writeSoundSad(1, mask << (base * 8), data << (base * 8));   break; // SOUND1SAD
            case 0x4000418:
            case 0x4000419: base -= 0x4000418; size = 2; spu->writeSoundTmr(1, mask << (base * 8), data << (base * 8));   break; // SOUND1TMR
            case 0x400041A:
            case 0x400041B: base -= 0x400041A; size = 2; spu->writeSoundPnt(1, mask << (base * 8), data << (base * 8));   break; // SOUND1PNT
            case 0x400041C:
            case 0x400041D:
            case 0x400041E:
            case 0x400041F: base -= 0x400041C; size = 4; spu->writeSoundLen(1, mask << (base * 8), data << (base * 8));   break; // SOUND1LEN
            case 0x4000420:
            case 0x4000421:
            case 0x4000422:
            case 0x4000423: base -= 0x4000420; size = 4; spu->writeSoundCnt(2, mask << (base * 8), data << (base * 8));   break; // SOUND2CNT
            case 0x4000424:
            case 0x4000425:
            case 0x4000426:
            case 0x4000427: base -= 0x4000424; size = 4; spu->writeSoundSad(2, mask << (base * 8), data << (base * 8));   break; // SOUND2SAD
            case 0x4000428:
            case 0x4000429: base -= 0x4000428; size = 2; spu->writeSoundTmr(2, mask << (base * 8), data << (base * 8));   break; // SOUND2TMR
            case 0x400042A:
            case 0x400042B: base -= 0x400042A; size = 2; spu->writeSoundPnt(2, mask << (base * 8), data << (base * 8));   break; // SOUND2PNT
            case 0x400042C:
            case 0x400042D:
            case 0x400042E:
            case 0x400042F: base -= 0x400042C; size = 4; spu->writeSoundLen(2, mask << (base * 8), data << (base * 8));   break; // SOUND2LEN
            case 0x4000430:
            case 0x4000431:
            case 0x4000432:
            case 0x4000433: base -= 0x4000430; size = 4; spu->writeSoundCnt(3, mask << (base * 8), data << (base * 8));   break; // SOUND3CNT
            case 0x4000434:
            case 0x4000435:
            case 0x4000436:
            case 0x4000437: base -= 0x4000434; size = 4; spu->writeSoundSad(3, mask << (base * 8), data << (base * 8));   break; // SOUND3SAD
            case 0x4000438:
            case 0x4000439: base -= 0x4000438; size = 2; spu->writeSoundTmr(3, mask << (base * 8), data << (base * 8));   break; // SOUND3TMR
            case 0x400043A:
            case 0x400043B: base -= 0x400043A; size = 2; spu->writeSoundPnt(3, mask << (base * 8), data << (base * 8));   break; // SOUND3PNT
            case 0x400043C:
            case 0x400043D:
            case 0x400043E:
            case 0x400043F: base -= 0x400043C; size = 4; spu->writeSoundLen(3, mask << (base * 8), data << (base * 8));   break; // SOUND3LEN
            case 0x4000440:
            case 0x4000441:
            case 0x4000442:
            case 0x4000443: base -= 0x4000440; size = 4; spu->writeSoundCnt(4, mask << (base * 8), data << (base * 8));   break; // SOUND4CNT
            case 0x4000444:
            case 0x4000445:
            case 0x4000446:
            case 0x4000447: base -= 0x4000444; size = 4; spu->writeSoundSad(4, mask << (base * 8), data << (base * 8));   break; // SOUND4SAD
            case 0x4000448:
            case 0x4000449: base -= 0x4000448; size = 2; spu->writeSoundTmr(4, mask << (base * 8), data << (base * 8));   break; // SOUND4TMR
            case 0x400044A:
            case 0x400044B: base -= 0x400044A; size = 2; spu->writeSoundPnt(4, mask << (base * 8), data << (base * 8));   break; // SOUND4PNT
            case 0x400044C:
            case 0x400044D:
            case 0x400044E:
            case 0x400044F: base -= 0x400044C; size = 4; spu->writeSoundLen(4, mask << (base * 8), data << (base * 8));   break; // SOUND4LEN
            case 0x4000450:
            case 0x4000451:
            case 0x4000452:
            case 0x4000453: base -= 0x4000450; size = 4; spu->writeSoundCnt(5, mask << (base * 8), data << (base * 8));   break; // SOUND5CNT
            case 0x4000454:
            case 0x4000455:
            case 0x4000456:
            case 0x4000457: base -= 0x4000454; size = 4; spu->writeSoundSad(5, mask << (base * 8), data << (base * 8));   break; // SOUND5SAD
            case 0x4000458:
            case 0x4000459: base -= 0x4000458; size = 2; spu->writeSoundTmr(5, mask << (base * 8), data << (base * 8));   break; // SOUND5TMR
            case 0x400045A:
            case 0x400045B: base -= 0x400045A; size = 2; spu->writeSoundPnt(5, mask << (base * 8), data << (base * 8));   break; // SOUND5PNT
            case 0x400045C:
            case 0x400045D:
            case 0x400045E:
            case 0x400045F: base -= 0x400045C; size = 4; spu->writeSoundLen(5, mask << (base * 8), data << (base * 8));   break; // SOUND5LEN
            case 0x4000460:
            case 0x4000461:
            case 0x4000462:
            case 0x4000463: base -= 0x4000460; size = 4; spu->writeSoundCnt(6, mask << (base * 8), data << (base * 8));   break; // SOUND6CNT
            case 0x4000464:
            case 0x4000465:
            case 0x4000466:
            case 0x4000467: base -= 0x4000464; size = 4; spu->writeSoundSad(6, mask << (base * 8), data << (base * 8));   break; // SOUND6SAD
            case 0x4000468:
            case 0x4000469: base -= 0x4000468; size = 2; spu->writeSoundTmr(6, mask << (base * 8), data << (base * 8));   break; // SOUND6TMR
            case 0x400046A:
            case 0x400046B: base -= 0x400046A; size = 2; spu->writeSoundPnt(6, mask << (base * 8), data << (base * 8));   break; // SOUND6PNT
            case 0x400046C:
            case 0x400046D:
            case 0x400046E:
            case 0x400046F: base -= 0x400046C; size = 4; spu->writeSoundLen(6, mask << (base * 8), data << (base * 8));   break; // SOUND6LEN
            case 0x4000470:
            case 0x4000471:
            case 0x4000472:
            case 0x4000473: base -= 0x4000470; size = 4; spu->writeSoundCnt(7, mask << (base * 8), data << (base * 8));   break; // SOUND7CNT
            case 0x4000474:
            case 0x4000475:
            case 0x4000476:
            case 0x4000477: base -= 0x4000474; size = 4; spu->writeSoundSad(7, mask << (base * 8), data << (base * 8));   break; // SOUND7SAD
            case 0x4000478:
            case 0x4000479: base -= 0x4000478; size = 2; spu->writeSoundTmr(7, mask << (base * 8), data << (base * 8));   break; // SOUND7TMR
            case 0x400047A:
            case 0x400047B: base -= 0x400047A; size = 2; spu->writeSoundPnt(7, mask << (base * 8), data << (base * 8));   break; // SOUND7PNT
            case 0x400047C:
            case 0x400047D:
            case 0x400047E:
            case 0x400047F: base -= 0x400047C; size = 4; spu->writeSoundLen(7, mask << (base * 8), data << (base * 8));   break; // SOUND7LEN
            case 0x4000480:
            case 0x4000481:
            case 0x4000482:
            case 0x4000483: base -= 0x4000480; size = 4; spu->writeSoundCnt(8, mask << (base * 8), data << (base * 8));   break; // SOUND8CNT
            case 0x4000484:
            case 0x4000485:
            case 0x4000486:
            case 0x4000487: base -= 0x4000484; size = 4; spu->writeSoundSad(8, mask << (base * 8), data << (base * 8));   break; // SOUND8SAD
            case 0x4000488:
            case 0x4000489: base -= 0x4000488; size = 2; spu->writeSoundTmr(8, mask << (base * 8), data << (base * 8));   break; // SOUND8TMR
            case 0x400048A:
            case 0x400048B: base -= 0x400048A; size = 2; spu->writeSoundPnt(8, mask << (base * 8), data << (base * 8));   break; // SOUND8PNT
            case 0x400048C:
            case 0x400048D:
            case 0x400048E:
            case 0x400048F: base -= 0x400048C; size = 4; spu->writeSoundLen(8, mask << (base * 8), data << (base * 8));   break; // SOUND8LEN
            case 0x4000490:
            case 0x4000491:
            case 0x4000492:
            case 0x4000493: base -= 0x4000490; size = 4; spu->writeSoundCnt(9, mask << (base * 8), data << (base * 8));   break; // SOUND9CNT
            case 0x4000494:
            case 0x4000495:
            case 0x4000496:
            case 0x4000497: base -= 0x4000494; size = 4; spu->writeSoundSad(9, mask << (base * 8), data << (base * 8));   break; // SOUND9SAD
            case 0x4000498:
            case 0x4000499: base -= 0x4000498; size = 2; spu->writeSoundTmr(9, mask << (base * 8), data << (base * 8));   break; // SOUND9TMR
            case 0x400049A:
            case 0x400049B: base -= 0x400049A; size = 2; spu->writeSoundPnt(9, mask << (base * 8), data << (base * 8));   break; // SOUND9PNT
            case 0x400049C:
            case 0x400049D:
            case 0x400049E:
            case 0x400049F: base -= 0x400049C; size = 4; spu->writeSoundLen(9, mask << (base * 8), data << (base * 8));   break; // SOUND9LEN
            case 0x40004A0:
            case 0x40004A1:
            case 0x40004A2:
            case 0x40004A3: base -= 0x40004A0; size = 4; spu->writeSoundCnt(10, mask << (base * 8), data << (base * 8));  break; // SOUND10CNT
            case 0x40004A4:
            case 0x40004A5:
            case 0x40004A6:
            case 0x40004A7: base -= 0x40004A4; size = 4; spu->writeSoundSad(10, mask << (base * 8), data << (base * 8));  break; // SOUND10SAD
            case 0x40004A8:
            case 0x40004A9: base -= 0x40004A8; size = 2; spu->writeSoundTmr(10, mask << (base * 8), data << (base * 8));  break; // SOUND10TMR
            case 0x40004AA:
            case 0x40004AB: base -= 0x40004AA; size = 2; spu->writeSoundPnt(10, mask << (base * 8), data << (base * 8));  break; // SOUND10PNT
            case 0x40004AC:
            case 0x40004AD:
            case 0x40004AE:
            case 0x40004AF: base -= 0x40004AC; size = 4; spu->writeSoundLen(10, mask << (base * 8), data << (base * 8));  break; // SOUND10LEN
            case 0x40004B0:
            case 0x40004B1:
            case 0x40004B2:
            case 0x40004B3: base -= 0x40004B0; size = 4; spu->writeSoundCnt(11, mask << (base * 8), data << (base * 8));  break; // SOUND11CNT
            case 0x40004B4:
            case 0x40004B5:
            case 0x40004B6:
            case 0x40004B7: base -= 0x40004B4; size = 4; spu->writeSoundSad(11, mask << (base * 8), data << (base * 8));  break; // SOUND11SAD
            case 0x40004B8:
            case 0x40004B9: base -= 0x40004B8; size = 2; spu->writeSoundTmr(11, mask << (base * 8), data << (base * 8));  break; // SOUND11TMR
            case 0x40004BA:
            case 0x40004BB: base -= 0x40004BA; size = 2; spu->writeSoundPnt(11, mask << (base * 8), data << (base * 8));  break; // SOUND11PNT
            case 0x40004BC:
            case 0x40004BD:
            case 0x40004BE:
            case 0x40004BF: base -= 0x40004BC; size = 4; spu->writeSoundLen(11, mask << (base * 8), data << (base * 8));  break; // SOUND11LEN
            case 0x40004C0:
            case 0x40004C1:
            case 0x40004C2:
            case 0x40004C3: base -= 0x40004C0; size = 4; spu->writeSoundCnt(12, mask << (base * 8), data << (base * 8));  break; // SOUND12CNT
            case 0x40004C4:
            case 0x40004C5:
            case 0x40004C6:
            case 0x40004C7: base -= 0x40004C4; size = 4; spu->writeSoundSad(12, mask << (base * 8), data << (base * 8));  break; // SOUND12SAD
            case 0x40004C8:
            case 0x40004C9: base -= 0x40004C8; size = 2; spu->writeSoundTmr(12, mask << (base * 8), data << (base * 8));  break; // SOUND12TMR
            case 0x40004CA:
            case 0x40004CB: base -= 0x40004CA; size = 2; spu->writeSoundPnt(12, mask << (base * 8), data << (base * 8));  break; // SOUND12PNT
            case 0x40004CC:
            case 0x40004CD:
            case 0x40004CE:
            case 0x40004CF: base -= 0x40004CC; size = 4; spu->writeSoundLen(12, mask << (base * 8), data << (base * 8));  break; // SOUND12LEN
            case 0x40004D0:
            case 0x40004D1:
            case 0x40004D2:
            case 0x40004D3: base -= 0x40004D0; size = 4; spu->writeSoundCnt(13, mask << (base * 8), data << (base * 8));  break; // SOUND13CNT
            case 0x40004D4:
            case 0x40004D5:
            case 0x40004D6:
            case 0x40004D7: base -= 0x40004D4; size = 4; spu->writeSoundSad(13, mask << (base * 8), data << (base * 8));  break; // SOUND13SAD
            case 0x40004D8:
            case 0x40004D9: base -= 0x40004D8; size = 2; spu->writeSoundTmr(13, mask << (base * 8), data << (base * 8));  break; // SOUND13TMR
            case 0x40004DA:
            case 0x40004DB: base -= 0x40004DA; size = 2; spu->writeSoundPnt(13, mask << (base * 8), data << (base * 8));  break; // SOUND13PNT
            case 0x40004DC:
            case 0x40004DD:
            case 0x40004DE:
            case 0x40004DF: base -= 0x40004DC; size = 4; spu->writeSoundLen(13, mask << (base * 8), data << (base * 8));  break; // SOUND13LEN
            case 0x40004E0:
            case 0x40004E1:
            case 0x40004E2:
            case 0x40004E3: base -= 0x40004E0; size = 4; spu->writeSoundCnt(14, mask << (base * 8), data << (base * 8));  break; // SOUND14CNT
            case 0x40004E4:
            case 0x40004E5:
            case 0x40004E6:
            case 0x40004E7: base -= 0x40004E4; size = 4; spu->writeSoundSad(14, mask << (base * 8), data << (base * 8));  break; // SOUND14SAD
            case 0x40004E8:
            case 0x40004E9: base -= 0x40004E8; size = 2; spu->writeSoundTmr(14, mask << (base * 8), data << (base * 8));  break; // SOUND14TMR
            case 0x40004EA:
            case 0x40004EB: base -= 0x40004EA; size = 2; spu->writeSoundPnt(14, mask << (base * 8), data << (base * 8));  break; // SOUND14PNT
            case 0x40004EC:
            case 0x40004ED:
            case 0x40004EE:
            case 0x40004EF: base -= 0x40004EC; size = 4; spu->writeSoundLen(14, mask << (base * 8), data << (base * 8));  break; // SOUND14LEN
            case 0x40004F0:
            case 0x40004F1:
            case 0x40004F2:
            case 0x40004F3: base -= 0x40004F0; size = 4; spu->writeSoundCnt(15, mask << (base * 8), data << (base * 8));  break; // SOUND15CNT
            case 0x40004F4:
            case 0x40004F5:
            case 0x40004F6:
            case 0x40004F7: base -= 0x40004F4; size = 4; spu->writeSoundSad(15, mask << (base * 8), data << (base * 8));  break; // SOUND15SAD
            case 0x40004F8:
            case 0x40004F9: base -= 0x40004F8; size = 2; spu->writeSoundTmr(15, mask << (base * 8), data << (base * 8));  break; // SOUND15TMR
            case 0x40004FA:
            case 0x40004FB: base -= 0x40004FA; size = 2; spu->writeSoundPnt(15, mask << (base * 8), data << (base * 8));  break; // SOUND15PNT
            case 0x40004FC:
            case 0x40004FD:
            case 0x40004FE:
            case 0x40004FF: base -= 0x40004FC; size = 4; spu->writeSoundLen(15, mask << (base * 8), data << (base * 8));  break; // SOUND15LEN
            case 0x4000500:
            case 0x4000501: base -= 0x4000500; size = 2; spu->writeMainSoundCnt(mask << (base * 8), data << (base * 8));  break; // SOUNDCNT
            case 0x4000504:
            case 0x4000505: base -= 0x4000504; size = 2; spu->writeSoundBias(mask << (base * 8), data << (base * 8));     break; // SOUNDBIAS

            default:
            {
                // Catch unknown writes
                if (i == 0)
                {
                    printf("Unknown ARM7 I/O register write: 0x%X\n", address);
                    return;
                }

                // Ignore unknown writes if they occur after the first byte
                // This is in case, for example, a 16-bit register is accessed with a 32-bit write
                i++;
                continue;
            }
        }

        i += size - base;
    }
}

void Memory::writeVramCntA(uint8_t value)
{
    // Remap VRAM block A
    if (value & BIT(7)) // VRAM enabled
    {
        uint8_t mst = (value & 0x03);
        uint8_t ofs = (value & 0x18) >> 3;
        switch (mst)
        {
            case 0: vramBases[0] = 0x6800000;                            return; // Plain ARM9 access
            case 1: vramBases[0] = 0x6000000 + 0x20000 * ofs;            return; // Engine A BG VRAM
            case 2: vramBases[0] = 0x6400000 + 0x20000 * (ofs & BIT(0)); return; // Engine A OBJ VRAM
            case 3: gpu3DRenderer->setTexture(ofs, vramA);               break;  // 3D texture data

            default:
            {
                printf("Unknown VRAM A MST: %d\n", mst);
                break;
            }
        }
    }

    vramBases[0] = 0;
}

void Memory::writeVramCntB(uint8_t value)
{
    // Remap VRAM block B
    if (value & BIT(7)) // VRAM enabled
    {
        uint8_t mst = (value & 0x03);
        uint8_t ofs = (value & 0x18) >> 3;
        switch (mst)
        {
            case 0: vramBases[1] = 0x6820000;                            return; // Plain ARM9 access
            case 1: vramBases[1] = 0x6000000 + 0x20000 * ofs;            return; // Engine A BG VRAM
            case 2: vramBases[1] = 0x6400000 + 0x20000 * (ofs & BIT(0)); return; // Engine A OBJ VRAM
            case 3: gpu3DRenderer->setTexture(ofs, vramB);               break;  // 3D texture data

            default:
            {
                printf("Unknown VRAM B MST: %d\n", mst);
                break;
            }
        }
    }

    vramBases[1] = 0;
}

void Memory::writeVramCntC(uint8_t value)
{
    // Remap VRAM block C
    if (value & BIT(7)) // VRAM enabled
    {
        uint8_t mst = (value & 0x07);
        uint8_t ofs = (value & 0x18) >> 3;
        switch (mst)
        {
            case 0: vramBases[2] = 0x6840000;                            vramStat &= ~BIT(0); return; // Plain ARM9 access
            case 1: vramBases[2] = 0x6000000 + 0x20000 * ofs;            vramStat &= ~BIT(0); return; // Engine A BG VRAM
            case 2: vramBases[2] = 0x6000000 + 0x20000 * (ofs & BIT(0)); vramStat |=  BIT(0); return; // Plain ARM7 access
            case 3: gpu3DRenderer->setTexture(ofs, vramC);               vramStat &= ~BIT(0); break;  // 3D texture data
            case 4: vramBases[2] = 0x6200000;                            vramStat &= ~BIT(0); return; // Engine B BG VRAM

            default:
            {
                printf("Unknown VRAM C MST: %d\n", mst);
                break;
            }
        }
    }

    vramBases[2] = 0;
}

void Memory::writeVramCntD(uint8_t value)
{
    // Remap VRAM block D
    if (value & BIT(7)) // VRAM enabled
    {
        uint8_t mst = (value & 0x07);
        uint8_t ofs = (value & 0x18) >> 3;
        switch (mst)
        {
            case 0: vramBases[3] = 0x6860000;                            vramStat &= ~BIT(1); return; // Plain ARM9 access
            case 1: vramBases[3] = 0x6000000 + 0x20000 * ofs;            vramStat &= ~BIT(1); return; // Engine A BG VRAM
            case 2: vramBases[3] = 0x6000000 + 0x20000 * (ofs & BIT(0)); vramStat |=  BIT(1); return; // Plain ARM7 access
            case 3: gpu3DRenderer->setTexture(ofs, vramD);               vramStat &= ~BIT(0); break;  // 3D texture data
            case 4: vramBases[3] = 0x6600000;                            vramStat &= ~BIT(1); return; // Engine B OBJ VRAM

            default:
            {
                printf("Unknown VRAM D MST: %d\n", mst);
                break;
            }
        }
    }

    vramBases[3] = 0;
}

void Memory::writeVramCntE(uint8_t value)
{
    // Remap VRAM block E
    if (value & BIT(7)) // VRAM enabled
    {
        uint8_t mst = (value & 0x07);
        switch (mst)
        {
            case 0: vramBases[4] = 0x6880000; return; // Plain ARM9 access
            case 1: vramBases[4] = 0x6000000; return; // Engine A BG VRAM
            case 2: vramBases[4] = 0x6400000; return; // Engine A OBJ VRAM

            case 3: // 3D texture palette
            {
                for (int i = 0; i < 4; i++)
                    gpu3DRenderer->setPalette(i, &vramE[0x4000 * i]);
                break;
            }

            case 4: // Engine A BG extended palette
            {
                for (int i = 0; i < 4; i++)
                    engineA->setExtPalette(i, &vramE[0x2000 * i]);
                break;
            }

            default:
            {
                printf("Unknown VRAM E MST: %d\n", mst);
                break;
            }
        }
    }

    vramBases[4] = 0;
}

void Memory::writeVramCntF(uint8_t value)
{
    // Remap VRAM block F
    if (value & BIT(7)) // VRAM enabled
    {
        uint8_t mst = (value & 0x07);
        uint8_t ofs = (value & 0x18) >> 3;
        switch (mst)
        {
            case 0: vramBases[5] = 0x6890000;                                                     return; // Plain ARM9 access
            case 1: vramBases[5] = 0x6000000 + 0x8000 * (ofs & BIT(1)) + 0x4000 * (ofs & BIT(0)); return; // Engine A BG VRAM
            case 2: vramBases[5] = 0x6400000 + 0x8000 * (ofs & BIT(1)) + 0x4000 * (ofs & BIT(0)); return; // Engine A OBJ VRAM
            case 3: gpu3DRenderer->setPalette((ofs & BIT(1)) * 2 + (ofs & BIT(0)), vramF);        break;  // 3D texture palette
            case 5: engineA->setExtPalette(4, vramF);                                             break;  // Engine A OBJ extended palette

            case 4: // Engine A BG extended palette
            {
                for (int i = 0; i < 2; i++)
                    engineA->setExtPalette((ofs & BIT(0)) * 2 + i, &vramF[0x2000 * i]);
                break;
            }

            default:
            {
                printf("Unknown VRAM F MST: %d\n", mst);
                break;
            }
        }
    }

    vramBases[5] = 0;
}

void Memory::writeVramCntG(uint8_t value)
{
    // Remap VRAM block G
    if (value & BIT(7)) // VRAM enabled
    {
        uint8_t mst = (value & 0x07);
        uint8_t ofs = (value & 0x18) >> 3;
        switch (mst)
        {
            case 0: vramBases[6] = 0x6894000;                                                     return; // Plain ARM9 access
            case 1: vramBases[6] = 0x6000000 + 0x8000 * (ofs & BIT(1)) + 0x4000 * (ofs & BIT(0)); return; // Engine A BG VRAM
            case 2: vramBases[6] = 0x6400000 + 0x8000 * (ofs & BIT(1)) + 0x4000 * (ofs & BIT(0)); return; // Engine A OBJ VRAM
            case 3: gpu3DRenderer->setPalette((ofs & BIT(1)) * 2 + (ofs & BIT(0)), vramG);        break;  // 3D texture palette
            case 5: engineA->setExtPalette(4, vramG);                                             break;  // Engine A OBJ extended palette

            case 4: // Engine A BG extended palette
            {
                for (int i = 0; i < 2; i++)
                    engineA->setExtPalette((ofs & BIT(0)) * 2 + i, &vramG[0x2000 * i]);
                break;
            }

            default:
            {
                printf("Unknown VRAM G MST: %d\n", mst);
                break;
            }
        }
    }

    vramBases[6] = 0;
}

void Memory::writeWramCnt(uint8_t value)
{
    wramStat = value & 0x03;
}

void Memory::writeVramCntH(uint8_t value)
{
    // Remap VRAM block H
    if (value & BIT(7)) // VRAM enabled
    {
        uint8_t mst = (value & 0x03);
        switch (mst)
        {
            case 0: vramBases[7] = 0x6898000; return; // Plain ARM9 access
            case 1: vramBases[7] = 0x6200000; return; // Engine B BG VRAM

            case 2: // Engine B BG extended palette
            {
                for (int i = 0; i < 4; i++)
                    engineB->setExtPalette(i, &vramH[0x2000 * i]);
                break;
            }
 
            default:
            {
                printf("Unknown VRAM H MST: %d\n", mst);
                break;
            }
        }
    }

    vramBases[7] = 0;
}

void Memory::writeVramCntI(uint8_t value)
{
    // Remap VRAM block I
    if (value & BIT(7)) // VRAM enabled
    {
        uint8_t mst = (value & 0x03);
        switch (mst)
        {
            case 0: vramBases[8] = 0x68A0000;         return; // Plain ARM9 access
            case 1: vramBases[8] = 0x6208000;         return; // Engine B BG VRAM
            case 2: vramBases[8] = 0x6600000;         return; // Engine B OBJ VRAM
            case 3: engineB->setExtPalette(4, vramI); break;  // Engine B OBJ extended palette
 
            default:
            {
                printf("Unknown VRAM I MST: %d\n", mst);
                break;
            }
        }
    }

    vramBases[8] = 0;
}

void Memory::writeDmaFill(int channel, uint32_t mask, uint32_t value)
{
    // Write to one of the DMAFILL registers
    dmaFill[channel] = (dmaFill[channel] & ~mask) | (value & mask);
}

void Memory::writeHaltCnt(uint8_t value)
{
    // Write to the HALTCNT register
    haltCnt = value & 0xC0;

    // Change the ARM7's power mode
    switch (haltCnt >> 6)
    {
        case 0:               break; // None
        case 2: arm7->halt(); break; // Halt

        default:
        {
            printf("Unknown ARM7 power mode: %d\n", haltCnt >> 6);
            break;
        }
    }
}
