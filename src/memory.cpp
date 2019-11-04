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
#include "input.h"
#include "interpreter.h"
#include "ipc.h"
#include "math.h"
#include "rtc.h"
#include "spi.h"
#include "timers.h"

Memory::Memory(Cartridge *cart9, Cartridge *cart7, Cp15 *cp15, Dma *dma9, Dma *dma7, Gpu *gpu,
               Gpu2D *engineA, Gpu2D *engineB, Input *input, Interpreter *arm9, Interpreter *arm7,
               Ipc *ipc, Math *math, Rtc *rtc, Spi *spi, Timers *timers9, Timers *timers7):
               cart9(cart9), cart7(cart7), cp15(cp15), dma9(dma9), dma7(dma7), gpu(gpu),
               engineA(engineA), engineB(engineB), input(input), arm9(arm9), arm7(arm7),
               ipc(ipc), math(math), rtc(rtc), spi(spi), timers9(timers9), timers7(timers7)
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
                    data = &ram[address % 0x400000];
                    break;

                case 0x03000000: // Shared WRAM
                    switch (wramStat)
                    {
                        case 0: data = &wram[address % 0x8000];
                        case 1: data = &wram[address % 0x4000 + 0x4000];
                        case 2: data = &wram[address % 0x4000];
                    }
                    break;

                case 0x04000000: // I/O registers
                    return ioRead9<T>(address);

                case 0x05000000: // Palettes
                    data = &palette[address % 0x800];
                    break;

                case 0x06000000: // VRAM
                    data = getMappedVram(address);
                    break;

                case 0x07000000: // OAM
                    data = &oam[address % 0x800];
                    break;

                case 0x08000000: case 0x0A000000: // GBA slot
                    return (T)0xFFFFFFFF; // As if nothing is inserted in the GBA slot

                case 0xFF000000: // ARM9 BIOS
                    if ((address & 0xFFFF8000) == 0xFFFF0000)
                        data = &bios9[address & 0xFFFF];
                    break;
            }
        }
    }
    else
    {
        // Get a pointer to the ARM7 memory mapped to the given address
        switch (address & 0xFF000000)
        {
            case 0x00000000: // ARM7 BIOS
                if (address < 0x4000)
                    data = &bios7[address];
                break;

            case 0x02000000: // Main RAM
                data = &ram[address % 0x400000];
                break;

            case 0x03000000: // WRAM
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

            case 0x04000000: // I/O registers
                return ioRead7<T>(address);

            case 0x06000000: // VRAM
                if (address >= vramBases[2] && address < vramBases[2] + 0x20000 && (vramStat & BIT(0))) // VRAM block C
                    data = &vramC[address - vramBases[2]];
                else if (address >= vramBases[3] && address < vramBases[3] + 0x20000 && (vramStat & BIT(1))) // VRAM block D
                    data = &vramD[address - vramBases[3]];
                break;

            case 0x08000000: case 0x0A000000: // GBA slot
                return (T)0xFFFFFFFF; // As if nothing is inserted in the GBA slot
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
                    data = &ram[address % 0x400000];
                    break;

                case 0x03000000: // Shared WRAM
                    switch (wramStat)
                    {
                        case 0: data = &wram[address % 0x8000];
                        case 1: data = &wram[address % 0x4000 + 0x4000];
                        case 2: data = &wram[address % 0x4000];
                    }
                    break;

                case 0x04000000: // I/O registers
                    ioWrite9<T>(address, value);
                    return;

                case 0x05000000: // Palettes
                    data = &palette[address % 0x800];
                    break;

                case 0x06000000: // VRAM
                    data = getMappedVram(address);
                    break;

                case 0x07000000: // OAM
                    data = &oam[address % 0x800];
                    break;
            }
        }
    }
    else
    {
        // Get a pointer to the ARM7 memory mapped to the given address
        switch (address & 0xFF000000)
        {
            case 0x02000000: // Main RAM
                data = &ram[address % 0x400000];
                break;

            case 0x03000000: // WRAM
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

            case 0x04000000: // I/O registers
                ioWrite7<T>(address, value);
                return;

            case 0x06000000: // VRAM
                if (address >= vramBases[2] && address < vramBases[2] + 0x20000 && (vramStat & BIT(0))) // VRAM block C
                    data = &vramC[address - vramBases[2]];
                else if (address >= vramBases[3] && address < vramBases[3] + 0x20000 && (vramStat & BIT(1))) // VRAM block D
                    data = &vramD[address - vramBases[3]];
                break;
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

uint8_t *Memory::getVramBlock(unsigned int block)
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

    // Form an LSB-first value from the data of the ARM9 I/O registers
    for (unsigned int i = 0; i < sizeof(T); i++)
    {
        uint8_t data = 0;

        switch (address + i)
        {
            case 0x4000000:
            case 0x4000001:
            case 0x4000002:
            case 0x4000003: data = engineA->readDispCnt(address + i - 0x4000000);   break; // DISPCNT (engine A)
            case 0x4000004:
            case 0x4000005: data = gpu->readDispStat9(address + i - 0x4000004);     break; // DISPSTAT (ARM9)
            case 0x4000006:
            case 0x4000007: data = gpu->readVCount(address + i - 0x4000006);        break; // VCOUNT
            case 0x4000008:
            case 0x4000009: data = engineA->readBgCnt(0, address + i - 0x4000008);  break; // BG0CNT (engine A)
            case 0x400000A:
            case 0x400000B: data = engineA->readBgCnt(1, address + i - 0x400000A);  break; // BG1CNT (engine A)
            case 0x400000C:
            case 0x400000D: data = engineA->readBgCnt(2, address + i - 0x400000C);  break; // BG2CNT (engine A)
            case 0x400000E:
            case 0x400000F: data = engineA->readBgCnt(3, address + i - 0x400000E);  break; // BG3CNT (engine A)
            case 0x40000B0:
            case 0x40000B1:
            case 0x40000B2:
            case 0x40000B3: data = dma9->readDmaSad(0, address + i - 0x40000B0);    break; // DMA0SAD (ARM9)
            case 0x40000B4:
            case 0x40000B5:
            case 0x40000B6:
            case 0x40000B7: data = dma9->readDmaDad(0, address + i - 0x40000B4);    break; // DMA0DAD (ARM9)
            case 0x40000B8:
            case 0x40000B9:
            case 0x40000BA:
            case 0x40000BB: data = dma9->readDmaCnt(0, address + i - 0x40000B8);    break; // DMA0CNT (ARM9)
            case 0x40000BC:
            case 0x40000BD:
            case 0x40000BE:
            case 0x40000BF: data = dma9->readDmaSad(1, address + i - 0x40000BC);    break; // DMA1SAD (ARM9)
            case 0x40000C0:
            case 0x40000C1:
            case 0x40000C2:
            case 0x40000C3: data = dma9->readDmaDad(1, address + i - 0x40000C0);    break; // DMA1DAD (ARM9)
            case 0x40000C4:
            case 0x40000C5:
            case 0x40000C6:
            case 0x40000C7: data = dma9->readDmaCnt(1, address + i - 0x40000C4);    break; // DMA1CNT (ARM9)
            case 0x40000C8:
            case 0x40000C9:
            case 0x40000CA:
            case 0x40000CB: data = dma9->readDmaSad(2, address + i - 0x40000C8);    break; // DMA2SAD (ARM9)
            case 0x40000CC:
            case 0x40000CD:
            case 0x40000CE:
            case 0x40000CF: data = dma9->readDmaDad(2, address + i - 0x40000CC);    break; // DMA2DAD (ARM9)
            case 0x40000D0:
            case 0x40000D1:
            case 0x40000D2:
            case 0x40000D3: data = dma9->readDmaCnt(2, address + i - 0x40000D0);    break; // DMA2CNT (ARM9)
            case 0x40000D4:
            case 0x40000D5:
            case 0x40000D6:
            case 0x40000D7: data = dma9->readDmaSad(3, address + i - 0x40000D4);    break; // DMA3SAD (ARM9)
            case 0x40000D8:
            case 0x40000D9:
            case 0x40000DA:
            case 0x40000DB: data = dma9->readDmaDad(3, address + i - 0x40000D8);    break; // DMA3DAD (ARM9)
            case 0x40000DC:
            case 0x40000DD:
            case 0x40000DE:
            case 0x40000DF: data = dma9->readDmaCnt(3, address + i - 0x40000DC);    break; // DMA3CNT (ARM9)
            case 0x40000E0:
            case 0x40000E1:
            case 0x40000E2:
            case 0x40000E3: data = readDmaFill(0, address + i - 0x40000E0);         break; // DMA0FILL
            case 0x40000E4:
            case 0x40000E5:
            case 0x40000E6:
            case 0x40000E7: data = readDmaFill(1, address + i - 0x40000E4);         break; // DMA1FILL
            case 0x40000E8:
            case 0x40000E9:
            case 0x40000EA:
            case 0x40000EB: data = readDmaFill(2, address + i - 0x40000E8);         break; // DMA2FILL
            case 0x40000EC:
            case 0x40000ED:
            case 0x40000EE:
            case 0x40000EF: data = readDmaFill(3, address + i - 0x40000EC);         break; // DMA3FILL
            case 0x4000100:
            case 0x4000101: data = timers9->readTmCntL(0, address + i - 0x4000100); break; // TM0CNT_L (ARM9)
            case 0x4000102: data = timers9->readTmCntH(0);                          break; // TM0CNT_H (ARM9)
            case 0x4000104:
            case 0x4000105: data = timers9->readTmCntL(1, address + i - 0x4000104); break; // TM1CNT_L (ARM9)
            case 0x4000106: data = timers9->readTmCntH(1);                          break; // TM1CNT_H (ARM9)
            case 0x4000108:
            case 0x4000109: data = timers9->readTmCntL(2, address + i - 0x4000108); break; // TM2CNT_L (ARM9)
            case 0x400010A: data = timers9->readTmCntH(2);                          break; // TM2CNT_H (ARM9)
            case 0x400010C:
            case 0x400010D: data = timers9->readTmCntL(3, address + i - 0x400010C); break; // TM3CNT_L (ARM9)
            case 0x400010E: data = timers9->readTmCntH(3);                          break; // TM3CNT_H (ARM9)
            case 0x4000130:
            case 0x4000131: data = input->readKeyInput(address + i - 0x4000130);    break; // KEYINPUT
            case 0x4000180:
            case 0x4000181: data = ipc->readIpcSync9(address + i - 0x4000180);      break; // IPCSYNC (ARM9)
            case 0x4000184:
            case 0x4000185: data = ipc->readIpcFifoCnt9(address + i - 0x4000184);   break; // IPCFIFOCNT (ARM9)
            case 0x40001A0:
            case 0x40001A1: data = cart9->readAuxSpiCnt(address + i - 0x40001A0);   break; // AUXSPICNT (ARM9)
            case 0x40001A2: data = cart9->readAuxSpiData();                         break; // AUXSPIDATA (ARM9)
            case 0x40001A4:
            case 0x40001A5:
            case 0x40001A6:
            case 0x40001A7: data = cart9->readRomCtrl(address + i - 0x40001A4);     break; // ROMCTRL (ARM9)
            case 0x40001A8:
            case 0x40001A9:
            case 0x40001AA:
            case 0x40001AB:
            case 0x40001AC:
            case 0x40001AD:
            case 0x40001AE:
            case 0x40001AF: data = cart9->readRomCmdOut(address + i - 0x40001A8);   break; // ROMCMDOUT (ARM9)
            case 0x4000208: data = arm9->readIme();                                 break; // IME (ARM9)
            case 0x4000210:
            case 0x4000211:
            case 0x4000212:
            case 0x4000213: data = arm9->readIe(address + i - 0x4000210);           break; // IE (ARM9)
            case 0x4000214:
            case 0x4000215:
            case 0x4000216:
            case 0x4000217: data = arm9->readIrf(address + i - 0x4000214);          break; // IF (ARM9)
            case 0x4000280:
            case 0x4000281: data = math->readDivCnt(address + i - 0x4000280);       break; // DIVCNT
            case 0x4000290:
            case 0x4000291:
            case 0x4000292:
            case 0x4000293:
            case 0x4000294:
            case 0x4000295:
            case 0x4000296:
            case 0x4000297: data = math->readDivNumer(address + i - 0x4000290);     break; // DIVNUMER
            case 0x4000298:
            case 0x4000299:
            case 0x400029A:
            case 0x400029B:
            case 0x400029C:
            case 0x400029D:
            case 0x400029E:
            case 0x400029F: data = math->readDivDenom(address + i - 0x4000298);     break; // DIVDENOM
            case 0x40002A0:
            case 0x40002A1:
            case 0x40002A2:
            case 0x40002A3:
            case 0x40002A4:
            case 0x40002A5:
            case 0x40002A6:
            case 0x40002A7: data = math->readDivResult(address + i - 0x40002A0);    break; // DIVRESULT
            case 0x40002A8:
            case 0x40002A9:
            case 0x40002AA:
            case 0x40002AB:
            case 0x40002AC:
            case 0x40002AD:
            case 0x40002AE:
            case 0x40002AF: data = math->readDivRemResult(address + i - 0x40002A8); break; // DIVREMRESULT
            case 0x40002B0:
            case 0x40002B1: data = math->readSqrtCnt(address + i - 0x40002B0);      break; // SQRTCNT
            case 0x40002B4:
            case 0x40002B5:
            case 0x40002B6:
            case 0x40002B7: data = math->readSqrtResult(address + i - 0x40002B4);   break; // SQRTRESULT
            case 0x40002B8:
            case 0x40002B9:
            case 0x40002BA:
            case 0x40002BB:
            case 0x40002BC:
            case 0x40002BD:
            case 0x40002BE:
            case 0x40002BF: data = math->readSqrtParam(address + i - 0x40002B8);    break; // SQRTPARAM
            case 0x4000300: data = arm9->readPostFlg();                             break; // POSTFLG (ARM9)
            case 0x4000304:
            case 0x4000305: data = gpu->readPowCnt1(address + i - 0x4000214);       break; // POWCNT1
            case 0x4001000:
            case 0x4001001:
            case 0x4001002:
            case 0x4001003: data = engineB->readDispCnt(address + i - 0x4001000);   break; // DISPCNT (engine B)
            case 0x4001008:
            case 0x4001009: data = engineB->readBgCnt(0, address + i - 0x4001008);  break; // BG0CNT (engine B)
            case 0x400100A:
            case 0x400100B: data = engineB->readBgCnt(1, address + i - 0x400100A);  break; // BG1CNT (engine B)
            case 0x400100C:
            case 0x400100D: data = engineB->readBgCnt(2, address + i - 0x400100C);  break; // BG2CNT (engine B)
            case 0x400100E:
            case 0x400100F: data = engineB->readBgCnt(3, address + i - 0x400100E);  break; // BG3CNT (engine B)
            case 0x4100000:
            case 0x4100001:
            case 0x4100002:
            case 0x4100003: data = ipc->readIpcFifoRecv9(address + i - 0x4100000);  break; // IPCFIFORECV (ARM9)
            case 0x4100010:
            case 0x4100011:
            case 0x4100012:
            case 0x4100013: data = cart9->readRomDataIn(address + i - 0x4100010);   break; // ROMDATAIN (ARM9)

            default:
                if (i == 0)
                {
                    printf("Unknown ARM9 I/O register read: 0x%X\n", address);
                    return 0;
                }
        }

        value |= data << (i * 8);
    }

    return value;
}

template <typename T> T Memory::ioRead7(uint32_t address)
{
    T value = 0;

    // Form an LSB-first value from the data of the ARM7 I/O registers
    for (unsigned int i = 0; i < sizeof(T); i++)
    {
        uint8_t data = 0;

        switch (address + i)
        {
            case 0x4000004:
            case 0x4000005: data = gpu->readDispStat7(address + i - 0x4000004);     break; // DISPSTAT (ARM7)
            case 0x4000006:
            case 0x4000007: data = gpu->readVCount(address + i - 0x4000006);        break; // VCOUNT
            case 0x40000B0:
            case 0x40000B1:
            case 0x40000B2:
            case 0x40000B3: data = dma7->readDmaSad(0, address + i - 0x40000B0);    break; // DMA0SAD (ARM7)
            case 0x40000B4:
            case 0x40000B5:
            case 0x40000B6:
            case 0x40000B7: data = dma7->readDmaDad(0, address + i - 0x40000B4);    break; // DMA0DAD (ARM7)
            case 0x40000B8:
            case 0x40000B9:
            case 0x40000BA:
            case 0x40000BB: data = dma7->readDmaCnt(0, address + i - 0x40000B8);    break; // DMA0CNT (ARM7)
            case 0x40000BC:
            case 0x40000BD:
            case 0x40000BE:
            case 0x40000BF: data = dma7->readDmaSad(1, address + i - 0x40000BC);    break; // DMA1SAD (ARM7)
            case 0x40000C0:
            case 0x40000C1:
            case 0x40000C2:
            case 0x40000C3: data = dma7->readDmaDad(1, address + i - 0x40000C0);    break; // DMA1DAD (ARM7)
            case 0x40000C4:
            case 0x40000C5:
            case 0x40000C6:
            case 0x40000C7: data = dma7->readDmaCnt(1, address + i - 0x40000C4);    break; // DMA1CNT (ARM7)
            case 0x40000C8:
            case 0x40000C9:
            case 0x40000CA:
            case 0x40000CB: data = dma7->readDmaSad(2, address + i - 0x40000C8);    break; // DMA2SAD (ARM7)
            case 0x40000CC:
            case 0x40000CD:
            case 0x40000CE:
            case 0x40000CF: data = dma7->readDmaDad(2, address + i - 0x40000CC);    break; // DMA2DAD (ARM7)
            case 0x40000D0:
            case 0x40000D1:
            case 0x40000D2:
            case 0x40000D3: data = dma7->readDmaCnt(2, address + i - 0x40000D0);    break; // DMA2CNT (ARM7)
            case 0x40000D4:
            case 0x40000D5:
            case 0x40000D6:
            case 0x40000D7: data = dma7->readDmaSad(3, address + i - 0x40000D4);    break; // DMA3SAD (ARM7)
            case 0x40000D8:
            case 0x40000D9:
            case 0x40000DA:
            case 0x40000DB: data = dma7->readDmaDad(3, address + i - 0x40000D8);    break; // DMA3DAD (ARM7)
            case 0x40000DC:
            case 0x40000DD:
            case 0x40000DE:
            case 0x40000DF: data = dma7->readDmaCnt(3, address + i - 0x40000DC);    break; // DMA3CNT (ARM7)
            case 0x4000100:
            case 0x4000101: data = timers7->readTmCntL(0, address + i - 0x4000100); break; // TM0CNT_L (ARM7)
            case 0x4000102: data = timers7->readTmCntH(0);                          break; // TM0CNT_H (ARM7)
            case 0x4000104:
            case 0x4000105: data = timers7->readTmCntL(1, address + i - 0x4000104); break; // TM1CNT_L (ARM7)
            case 0x4000106: data = timers7->readTmCntH(1);                          break; // TM1CNT_H (ARM7)
            case 0x4000108:
            case 0x4000109: data = timers7->readTmCntL(2, address + i - 0x4000108); break; // TM2CNT_L (ARM7)
            case 0x400010A: data = timers7->readTmCntH(2);                          break; // TM2CNT_H (ARM7)
            case 0x400010C:
            case 0x400010D: data = timers7->readTmCntL(3, address + i - 0x400010C); break; // TM3CNT_L (ARM7)
            case 0x400010E: data = timers7->readTmCntH(3);                          break; // TM3CNT_H (ARM7)
            case 0x4000130:
            case 0x4000131: data = input->readKeyInput(address + i - 0x4000130);    break; // KEYINPUT
            case 0x4000136:
            case 0x4000137: data = input->readExtKeyIn(address + i - 0x4000136);    break; // EXTKEYIN
            case 0x4000138: data = rtc->readRtc();                                  break; // RTC
            case 0x4000180:
            case 0x4000181: data = ipc->readIpcSync7(address + i - 0x4000180);      break; // IPCSYNC (ARM7)
            case 0x4000184:
            case 0x4000185: data = ipc->readIpcFifoCnt7(address + i - 0x4000184);   break; // IPCFIFOCNT (ARM7)
            case 0x40001A0:
            case 0x40001A1: data = cart7->readAuxSpiCnt(address + i - 0x40001A0);   break; // AUXSPICNT (ARM7)
            case 0x40001A2: data = cart7->readAuxSpiData();                         break; // AUXSPIDATA (ARM7)
            case 0x40001A4:
            case 0x40001A5:
            case 0x40001A6:
            case 0x40001A7: data = cart7->readRomCtrl(address + i - 0x40001A4);     break; // ROMCTRL (ARM7)
            case 0x40001A8:
            case 0x40001A9:
            case 0x40001AA:
            case 0x40001AB:
            case 0x40001AC:
            case 0x40001AD:
            case 0x40001AE:
            case 0x40001AF: data = cart7->readRomCmdOut(address + i - 0x40001A8);   break; // ROMCMDOUT (ARM7)
            case 0x40001C0:
            case 0x40001C1: data = spi->readSpiCnt(address + i - 0x40001C0);        break; // SPICNT
            case 0x40001C2: data = spi->readSpiData();                              break; // SPIDATA
            case 0x4000208: data = arm7->readIme();                                 break; // IME (ARM7)
            case 0x4000210:
            case 0x4000211:
            case 0x4000212:
            case 0x4000213: data = arm7->readIe(address + i - 0x4000210);           break; // IE (ARM7)
            case 0x4000214:
            case 0x4000215:
            case 0x4000216:
            case 0x4000217: data = arm7->readIrf(address + i - 0x4000214);          break; // IF (ARM7)
            case 0x4000241: data = readWramStat();                                  break; // WRAMSTAT
            case 0x4000300: data = arm7->readPostFlg();                             break; // POSTFLG (ARM7)
            case 0x4000301: data = readHaltCnt();                                   break; // HALTCNT
            case 0x4000504:
            case 0x4000505: data = readSoundBias(address + i - 0x4000504);          break; // SOUNDBIAS
            case 0x4100000:
            case 0x4100001:
            case 0x4100002:
            case 0x4100003: data = ipc->readIpcFifoRecv7(address + i - 0x4100000);  break; // IPCFIFORECV (ARM7)
            case 0x4100010:
            case 0x4100011:
            case 0x4100012:
            case 0x4100013: data = cart7->readRomDataIn(address + i - 0x4100010);   break; // ROMDATAIN (ARM7)

            default:
                if (i == 0)
                {
                    printf("Unknown ARM7 I/O register read: 0x%X\n", address);
                    return 0;
                }
        }

        value |= data << (i * 8);
    }

    return value;
}

template <typename T> void Memory::ioWrite9(uint32_t address, T value)
{
    // Write an LSB-first value to the data of the ARM9 I/O registers
    for (unsigned int i = 0; i < sizeof(T); i++)
    {
        uint8_t data = value >> (i * 8);

        switch (address + i)
        {
            case 0x4000000:
            case 0x4000001:
            case 0x4000002:
            case 0x4000003: engineA->writeDispCnt(address + i - 0x4000000, data);      break; // DISPCNT (engine A)
            case 0x4000004:
            case 0x4000005: gpu->writeDispStat9(address + i - 0x4000004, data);        break; // DISPSTAT (ARM9)
            case 0x4000008:
            case 0x4000009: engineA->writeBgCnt(0, address + i - 0x4000008, data);     break; // BG0CNT (engine A)
            case 0x400000A:
            case 0x400000B: engineA->writeBgCnt(1, address + i - 0x400000A, data);     break; // BG1CNT (engine A)
            case 0x400000C:
            case 0x400000D: engineA->writeBgCnt(2, address + i - 0x400000C, data);     break; // BG2CNT (engine A)
            case 0x400000E:
            case 0x400000F: engineA->writeBgCnt(3, address + i - 0x400000E, data);     break; // BG3CNT (engine A)
            case 0x4000010:
            case 0x4000011: engineA->writeBgHOfs(0, address + i - 0x4000010, data);    break; // BG0HOFS (engine A)
            case 0x4000012:
            case 0x4000013: engineA->writeBgVOfs(0, address + i - 0x4000012, data);    break; // BG0VOFS (engine A)
            case 0x4000014:
            case 0x4000015: engineA->writeBgHOfs(1, address + i - 0x4000014, data);    break; // BG1HOFS (engine A)
            case 0x4000016:
            case 0x4000017: engineA->writeBgVOfs(1, address + i - 0x4000016, data);    break; // BG1VOFS (engine A)
            case 0x4000018:
            case 0x4000019: engineA->writeBgHOfs(2, address + i - 0x4000018, data);    break; // BG2HOFS (engine A)
            case 0x400001A:
            case 0x400001B: engineA->writeBgVOfs(2, address + i - 0x400001A, data);    break; // BG2VOFS (engine A)
            case 0x400001C:
            case 0x400001D: engineA->writeBgHOfs(3, address + i - 0x400001C, data);    break; // BG3HOFS (engine A)
            case 0x400001E:
            case 0x400001F: engineA->writeBgVOfs(3, address + i - 0x400001E, data);    break; // BG3VOFS (engine A)
            case 0x400006C:
            case 0x400006D: engineA->writeMasterBright(address + i - 0x400006C, data); break; // MASTER_BRIGHT (engine A)
            case 0x40000B0:
            case 0x40000B1:
            case 0x40000B2:
            case 0x40000B3: dma9->writeDmaSad(0, address + i - 0x40000B0, data);       break; // DMA0SAD (ARM9)
            case 0x40000B4:
            case 0x40000B5:
            case 0x40000B6:
            case 0x40000B7: dma9->writeDmaDad(0, address + i - 0x40000B4, data);       break; // DMA0DAD (ARM9)
            case 0x40000B8:
            case 0x40000B9:
            case 0x40000BA:
            case 0x40000BB: dma9->writeDmaCnt(0, address + i - 0x40000B8, data);       break; // DMA0CNT (ARM9)
            case 0x40000BC:
            case 0x40000BD:
            case 0x40000BE:
            case 0x40000BF: dma9->writeDmaSad(1, address + i - 0x40000BC, data);       break; // DMA1SAD (ARM9)
            case 0x40000C0:
            case 0x40000C1:
            case 0x40000C2:
            case 0x40000C3: dma9->writeDmaDad(1, address + i - 0x40000C0, data);       break; // DMA1DAD (ARM9)
            case 0x40000C4:
            case 0x40000C5:
            case 0x40000C6:
            case 0x40000C7: dma9->writeDmaCnt(1, address + i - 0x40000C4, data);       break; // DMA1CNT (ARM9)
            case 0x40000C8:
            case 0x40000C9:
            case 0x40000CA:
            case 0x40000CB: dma9->writeDmaSad(2, address + i - 0x40000C8, data);       break; // DMA2SAD (ARM9)
            case 0x40000CC:
            case 0x40000CD:
            case 0x40000CE:
            case 0x40000CF: dma9->writeDmaDad(2, address + i - 0x40000CC, data);       break; // DMA2DAD (ARM9)
            case 0x40000D0:
            case 0x40000D1:
            case 0x40000D2:
            case 0x40000D3: dma9->writeDmaCnt(2, address + i - 0x40000D0, data);       break; // DMA2CNT (ARM9)
            case 0x40000D4:
            case 0x40000D5:
            case 0x40000D6:
            case 0x40000D7: dma9->writeDmaSad(3, address + i - 0x40000D4, data);       break; // DMA3SAD (ARM9)
            case 0x40000D8:
            case 0x40000D9:
            case 0x40000DA:
            case 0x40000DB: dma9->writeDmaDad(3, address + i - 0x40000D8, data);       break; // DMA3DAD (ARM9)
            case 0x40000DC:
            case 0x40000DD:
            case 0x40000DE:
            case 0x40000DF: dma9->writeDmaCnt(3, address + i - 0x40000DC, data);       break; // DMA3CNT (ARM9)
            case 0x40000E0:
            case 0x40000E1:
            case 0x40000E2:
            case 0x40000E3: writeDmaFill(0, address + i - 0x40000E0, data);            break; // DMA0FILL
            case 0x40000E4:
            case 0x40000E5:
            case 0x40000E6:
            case 0x40000E7: writeDmaFill(1, address + i - 0x40000E4, data);            break; // DMA1FILL
            case 0x40000E8:
            case 0x40000E9:
            case 0x40000EA:
            case 0x40000EB: writeDmaFill(2, address + i - 0x40000E8, data);            break; // DMA2FILL
            case 0x40000EC:
            case 0x40000ED:
            case 0x40000EE:
            case 0x40000EF: writeDmaFill(3, address + i - 0x40000EC, data);            break; // DMA3FILL
            case 0x4000100:
            case 0x4000101: timers9->writeTmCntL(0, address + i - 0x4000100, data);    break; // TM0CNT_L (ARM9)
            case 0x4000102: timers9->writeTmCntH(0, data);                             break; // TM0CNT_H (ARM9)
            case 0x4000104:
            case 0x4000105: timers9->writeTmCntL(1, address + i - 0x4000104, data);    break; // TM1CNT_L (ARM9)
            case 0x4000106: timers9->writeTmCntH(1, data);                             break; // TM1CNT_H (ARM9)
            case 0x4000108:
            case 0x4000109: timers9->writeTmCntL(2, address + i - 0x4000108, data);    break; // TM2CNT_L (ARM9)
            case 0x400010A: timers9->writeTmCntH(2, data);                             break; // TM2CNT_H (ARM9)
            case 0x400010C:
            case 0x400010D: timers9->writeTmCntL(3, address + i - 0x400010C, data);    break; // TM3CNT_L (ARM9)
            case 0x400010E: timers9->writeTmCntH(3, data);                             break; // TM3CNT_H (ARM9)
            case 0x4000180:
            case 0x4000181: ipc->writeIpcSync9(address + i - 0x4000180, data);         break; // IPCSYNC (ARM9)
            case 0x4000184:
            case 0x4000185: ipc->writeIpcFifoCnt9(address + i - 0x4000184, data);      break; // IPCFIFOCNT (ARM9)
            case 0x4000188:
            case 0x4000189:
            case 0x400018A:
            case 0x400018B: ipc->writeIpcFifoSend9(address + i - 0x4000188, data);     break; // IPCFIFOSEND (ARM9)
            case 0x40001A0:
            case 0x40001A1: cart9->writeAuxSpiCnt(address + i - 0x40001A0, data);      break; // AUXSPICNT (ARM9)
            case 0x40001A2: cart9->writeAuxSpiData(data);                              break; // AUXSPIDATA (ARM9)
            case 0x40001A4:
            case 0x40001A5:
            case 0x40001A6:
            case 0x40001A7: cart9->writeRomCtrl(address + i - 0x40001A4, data);        break; // ROMCTRL (ARM9)
            case 0x40001A8:
            case 0x40001A9:
            case 0x40001AA:
            case 0x40001AB:
            case 0x40001AC:
            case 0x40001AD:
            case 0x40001AE:
            case 0x40001AF: cart9->writeRomCmdOut(address + i - 0x40001A8, data);      break; // ROMCMDOUT (ARM9)
            case 0x4000208: arm9->writeIme(data);                                      break; // IME (ARM9)
            case 0x4000210:
            case 0x4000211:
            case 0x4000212:
            case 0x4000213: arm9->writeIe(address + i - 0x4000210, data);              break; // IE (ARM9)
            case 0x4000214:
            case 0x4000215:
            case 0x4000216:
            case 0x4000217: arm9->writeIrf(address + i - 0x4000214, data);             break; // IF (ARM9)
            case 0x4000240: writeVramCntA(value);                                      break; // VRAMCNT_A
            case 0x4000241: writeVramCntB(value);                                      break; // VRAMCNT_B
            case 0x4000242: writeVramCntC(value);                                      break; // VRAMCNT_C
            case 0x4000243: writeVramCntD(value);                                      break; // VRAMCNT_D
            case 0x4000244: writeVramCntE(value);                                      break; // VRAMCNT_E
            case 0x4000245: writeVramCntF(value);                                      break; // VRAMCNT_F
            case 0x4000246: writeVramCntG(value);                                      break; // VRAMCNT_G
            case 0x4000247: writeWramCnt(value);                                       break; // WRAMCNT
            case 0x4000248: writeVramCntH(value);                                      break; // VRAMCNT_H
            case 0x4000249: writeVramCntI(value);                                      break; // VRAMCNT_I
            case 0x4000280: math->writeDivCnt(data);                                   break; // DIVCNT
            case 0x4000290:
            case 0x4000291:
            case 0x4000292:
            case 0x4000293:
            case 0x4000294:
            case 0x4000295:
            case 0x4000296:
            case 0x4000297: math->writeDivNumer(address + i - 0x4000290, data);        break; // DIVNUMER
            case 0x4000298:
            case 0x4000299:
            case 0x400029A:
            case 0x400029B:
            case 0x400029C:
            case 0x400029D:
            case 0x400029E:
            case 0x400029F: math->writeDivDenom(address + i - 0x4000298, data);        break; // DIVDENOM
            case 0x40002B0: math->writeSqrtCnt(data);                                  break; // SQRTCNT
            case 0x40002B8:
            case 0x40002B9:
            case 0x40002BA:
            case 0x40002BB:
            case 0x40002BC:
            case 0x40002BD:
            case 0x40002BE:
            case 0x40002BF: math->writeSqrtParam(address + i - 0x40002B8, data);       break; // SQRTPARAM
            case 0x4000300: arm9->writePostFlg(data);                                  break; // POSTFLG (ARM9)
            case 0x4000304:
            case 0x4000305: gpu->writePowCnt1(address + i - 0x4000214, data);          break; // POWCNT1
            case 0x4001000:
            case 0x4001001:
            case 0x4001002:
            case 0x4001003: engineB->writeDispCnt(address + i - 0x4001000, data);      break; // DISPCNT (engine B)
            case 0x4001008:
            case 0x4001009: engineB->writeBgCnt(0, address + i - 0x4001008, data);     break; // BG0CNT (engine B)
            case 0x400100A:
            case 0x400100B: engineB->writeBgCnt(1, address + i - 0x400100A, data);     break; // BG1CNT (engine B)
            case 0x400100C:
            case 0x400100D: engineB->writeBgCnt(2, address + i - 0x400100C, data);     break; // BG2CNT (engine B)
            case 0x400100E:
            case 0x400100F: engineB->writeBgCnt(3, address + i - 0x400100E, data);     break; // BG3CNT (engine B)
            case 0x4001010:
            case 0x4001011: engineB->writeBgHOfs(0, address + i - 0x4001010, data);    break; // BG0HOFS (engine B)
            case 0x4001012:
            case 0x4001013: engineB->writeBgVOfs(0, address + i - 0x4001012, data);    break; // BG0VOFS (engine B)
            case 0x4001014:
            case 0x4001015: engineB->writeBgHOfs(1, address + i - 0x4001014, data);    break; // BG1HOFS (engine B)
            case 0x4001016:
            case 0x4001017: engineB->writeBgVOfs(1, address + i - 0x4001016, data);    break; // BG1VOFS (engine B)
            case 0x4001018:
            case 0x4001019: engineB->writeBgHOfs(2, address + i - 0x4001018, data);    break; // BG2HOFS (engine B)
            case 0x400101A:
            case 0x400101B: engineB->writeBgVOfs(2, address + i - 0x400101A, data);    break; // BG2VOFS (engine B)
            case 0x400101C:
            case 0x400101D: engineB->writeBgHOfs(3, address + i - 0x400101C, data);    break; // BG3HOFS (engine B)
            case 0x400101E:
            case 0x400101F: engineB->writeBgVOfs(3, address + i - 0x400101E, data);    break; // BG3VOFS (engine B)
            case 0x400106C:
            case 0x400106D: engineB->writeMasterBright(address + i - 0x400106C, data); break; // MASTER_BRIGHT (engine B)

            default:
                if (i == 0)
                {
                    printf("Unknown ARM9 I/O register write: 0x%X\n", address);
                    return;
                }
        }
    }
}

template <typename T> void Memory::ioWrite7(uint32_t address, T value)
{
    // Write an LSB-first value to the data of the ARM7 I/O registers
    for (unsigned int i = 0; i < sizeof(T); i++)
    {
        uint8_t data = value >> (i * 8);

        switch (address + i)
        {
            case 0x4000004:
            case 0x4000005: gpu->writeDispStat7(address + i - 0x4000004, data);     break; // DISPSTAT (ARM7)
            case 0x40000B0:
            case 0x40000B1:
            case 0x40000B2:
            case 0x40000B3: dma7->writeDmaSad(0, address + i - 0x40000B0, data);    break; // DMA0SAD (ARM7)
            case 0x40000B4:
            case 0x40000B5:
            case 0x40000B6:
            case 0x40000B7: dma7->writeDmaDad(0, address + i - 0x40000B4, data);    break; // DMA0DAD (ARM7)
            case 0x40000B8:
            case 0x40000B9:
            case 0x40000BA:
            case 0x40000BB: dma7->writeDmaCnt(0, address + i - 0x40000B8, data);    break; // DMA0CNT (ARM7)
            case 0x40000BC:
            case 0x40000BD:
            case 0x40000BE:
            case 0x40000BF: dma7->writeDmaSad(1, address + i - 0x40000BC, data);    break; // DMA1SAD (ARM7)
            case 0x40000C0:
            case 0x40000C1:
            case 0x40000C2:
            case 0x40000C3: dma7->writeDmaDad(1, address + i - 0x40000C0, data);    break; // DMA1DAD (ARM7)
            case 0x40000C4:
            case 0x40000C5:
            case 0x40000C6:
            case 0x40000C7: dma7->writeDmaCnt(1, address + i - 0x40000C4, data);    break; // DMA1CNT (ARM7)
            case 0x40000C8:
            case 0x40000C9:
            case 0x40000CA:
            case 0x40000CB: dma7->writeDmaSad(2, address + i - 0x40000C8, data);    break; // DMA2SAD (ARM7)
            case 0x40000CC:
            case 0x40000CD:
            case 0x40000CE:
            case 0x40000CF: dma7->writeDmaDad(2, address + i - 0x40000CC, data);    break; // DMA2DAD (ARM7)
            case 0x40000D0:
            case 0x40000D1:
            case 0x40000D2:
            case 0x40000D3: dma7->writeDmaCnt(2, address + i - 0x40000D0, data);    break; // DMA2CNT (ARM7)
            case 0x40000D4:
            case 0x40000D5:
            case 0x40000D6:
            case 0x40000D7: dma7->writeDmaSad(3, address + i - 0x40000D4, data);    break; // DMA3SAD (ARM7)
            case 0x40000D8:
            case 0x40000D9:
            case 0x40000DA:
            case 0x40000DB: dma7->writeDmaDad(3, address + i - 0x40000D8, data);    break; // DMA3DAD (ARM7)
            case 0x40000DC:
            case 0x40000DD:
            case 0x40000DE:
            case 0x40000DF: dma7->writeDmaCnt(3, address + i - 0x40000DC, data);    break; // DMA3CNT (ARM7)
            case 0x4000100:
            case 0x4000101: timers7->writeTmCntL(0, address + i - 0x4000100, data); break; // TM0CNT_L (ARM7)
            case 0x4000102: timers7->writeTmCntH(0, data);                          break; // TM0CNT_H (ARM7)
            case 0x4000104:
            case 0x4000105: timers7->writeTmCntL(1, address + i - 0x4000104, data); break; // TM1CNT_L (ARM7)
            case 0x4000106: timers7->writeTmCntH(1, data);                          break; // TM1CNT_H (ARM7)
            case 0x4000108:
            case 0x4000109: timers7->writeTmCntL(2, address + i - 0x4000108, data); break; // TM2CNT_L (ARM7)
            case 0x400010A: timers7->writeTmCntH(2, data);                          break; // TM2CNT_H (ARM7)
            case 0x400010C:
            case 0x400010D: timers7->writeTmCntL(3, address + i - 0x400010C, data); break; // TM3CNT_L (ARM7)
            case 0x400010E: timers7->writeTmCntH(3, data);                          break; // TM3CNT_H (ARM7)
            case 0x4000138: rtc->writeRtc(value);                                   break; // RTC
            case 0x4000180:
            case 0x4000181: ipc->writeIpcSync7(address + i - 0x4000180, data);      break; // IPCSYNC (ARM7)
            case 0x4000184:
            case 0x4000185: ipc->writeIpcFifoCnt7(address + i - 0x4000184, data);   break; // IPCFIFOCNT (ARM7)
            case 0x4000188:
            case 0x4000189:
            case 0x400018A:
            case 0x400018B: ipc->writeIpcFifoSend7(address + i - 0x4000188, data);  break; // IPCFIFOSEND (ARM7)
            case 0x40001A0:
            case 0x40001A1: cart7->writeAuxSpiCnt(address + i - 0x40001A0, data);   break; // AUXSPICNT (ARM7)
            case 0x40001A2: cart7->writeAuxSpiData(data);                           break; // AUXSPIDATA (ARM7)
            case 0x40001A4:
            case 0x40001A5:
            case 0x40001A6:
            case 0x40001A7: cart7->writeRomCtrl(address + i - 0x40001A4, data);     break; // ROMCTRL (ARM7)
            case 0x40001A8:
            case 0x40001A9:
            case 0x40001AA:
            case 0x40001AB:
            case 0x40001AC:
            case 0x40001AD:
            case 0x40001AE:
            case 0x40001AF: cart7->writeRomCmdOut(address + i - 0x40001A8, data);   break; // ROMCMDOUT (ARM7)
            case 0x40001C0:
            case 0x40001C1: spi->writeSpiCnt(address + i - 0x40001C0, data);        break; // SPICNT
            case 0x40001C2: spi->writeSpiData(data);                                break; // SPIDATA
            case 0x4000208: arm7->writeIme(data);                                   break; // IME (ARM7)
            case 0x4000210:
            case 0x4000211:
            case 0x4000212:
            case 0x4000213: arm7->writeIe(address + i - 0x4000210, data);           break; // IE (ARM7)
            case 0x4000214:
            case 0x4000215:
            case 0x4000216:
            case 0x4000217: arm7->writeIrf(address + i - 0x4000214, data);          break; // IF (ARM7)
            case 0x4000300: arm7->writePostFlg(data);                               break; // POSTFLG (ARM7)
            case 0x4000301: writeHaltCnt(data);                                     break; // HALTCNT
            case 0x4000504:
            case 0x4000505: writeSoundBias(address + i - 0x4000504, data);          break; // SOUNDBIAS

            default:
                if (i == 0)
                {
                    printf("Unknown ARM7 I/O register write: 0x%X\n", address);
                    return;
                }
        }
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

            default:
                printf("Unknown VRAM A MST: %d\n", mst);
                break;
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

            default:
                printf("Unknown VRAM B MST: %d\n", mst);
                break;
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
            case 4: vramBases[2] = 0x6200000;                            vramStat &= ~BIT(0); return; // Engine B BG VRAM

            default:
                printf("Unknown VRAM C MST: %d\n", mst);
                break;
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
            case 4: vramBases[3] = 0x6600000;                            vramStat &= ~BIT(1); return; // Engine B OBJ VRAM

            default:
                printf("Unknown VRAM D MST: %d\n", mst);
                break;
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

            case 4: // Engine A BG extended palette
                for (int i = 0; i < 4; i++)
                    engineA->setExtPalette(i, &vramE[0x2000 * i]);
                break;

            default:
                printf("Unknown VRAM E MST: %d\n", mst);
                break;
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
            case 5: engineA->setExtPalette(4, vramF);                                             break;  // Engine A OBJ extended palette

            case 4: // Engine A BG extended palette
                for (int i = 0; i < 2; i++)
                    engineA->setExtPalette((ofs & BIT(0)) * 2 + i, &vramF[0x2000 * i]);
                break;

            default:
                printf("Unknown VRAM F MST: %d\n", mst);
                break;
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
            case 5: engineA->setExtPalette(4, vramG);                                             break;  // Engine A OBJ extended palette

            case 4: // Engine A BG extended palette
                for (int i = 0; i < 2; i++)
                    engineA->setExtPalette((ofs & BIT(0)) * 2 + i, &vramG[0x2000 * i]);
                break;

            default:
                printf("Unknown VRAM G MST: %d\n", mst);
                break;
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
                for (int i = 0; i < 4; i++)
                    engineB->setExtPalette(i, &vramH[0x2000 * i]);
                break;
 
            default:
                printf("Unknown VRAM H MST: %d\n", mst);
                break;
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
                printf("Unknown VRAM I MST: %d\n", mst);
                break;
        }
    }

    vramBases[8] = 0;
}

void Memory::writeDmaFill(unsigned int channel, unsigned int byte, uint8_t value)
{
    // Write to one of the DMAFILL registers
    dmaFill[channel] = (dmaFill[channel] & ~(0xFF << (byte * 8))) | (value << (byte * 8));
}

void Memory::writeHaltCnt(uint8_t value)
{
    // Write to the HALTCNT register
    haltCnt = value & 0xC0;

    // Change the ARM7's power mode
    switch (haltCnt >> 6)
    {
        case 0: // None
            break;

        case 2: // Halt
            arm7->halt();
            break;

        default:
            printf("Unknown ARM7 power mode: %d\n", haltCnt >> 6);
            break;
    }
}

void Memory::writeSoundBias(unsigned int byte, uint8_t value)
{
    // Write to the SOUNDBIAS register
    // This doesn't do anything yet, but some programs expect it to be writable
    uint16_t mask = 0x03FF & (0xFF << (byte * 8));
    soundBias = (soundBias & ~mask) | ((value << (byte * 8)) & mask);
}
