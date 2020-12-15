/*
    Copyright 2019-2020 Hydr8gon

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

#include <cstring>

#include "memory.h"
#include "core.h"
#include "settings.h"

void Memory::loadBios()
{
    // Attempt to load the ARM9 BIOS
    FILE *bios9File = fopen(Settings::getBios9Path().c_str(), "rb");
    if (!bios9File) throw 1;
    fread(bios9, sizeof(uint8_t), 0x1000, bios9File);
    fclose(bios9File);

    // Attempt to load the ARM7 BIOS
    FILE *bios7File = fopen(Settings::getBios7Path().c_str(), "rb");
    if (!bios7File) throw 1;
    fread(bios7, sizeof(uint8_t), 0x4000, bios7File);
    fclose(bios7File);
}

void Memory::loadGbaBios()
{
    // Attempt to load the GBA BIOS
    FILE *gbaBiosFile = fopen(Settings::getGbaBiosPath().c_str(), "rb");
    if (!gbaBiosFile) throw 1;
    fread(gbaBios, sizeof(uint8_t), 0x4000, gbaBiosFile);
    fclose(gbaBiosFile);
}

template int8_t   Memory::read(bool cpu, uint32_t address);
template int16_t  Memory::read(bool cpu, uint32_t address);
template uint8_t  Memory::read(bool cpu, uint32_t address);
template uint16_t Memory::read(bool cpu, uint32_t address);
template uint32_t Memory::read(bool cpu, uint32_t address);
template <typename T> T Memory::read(bool cpu, uint32_t address)
{
    // Align the address
    address &= ~(sizeof(T) - 1);

    uint8_t *data = nullptr;

    if (cpu == 0) // ARM9
    {
        // Get a pointer to the ARM9 memory mapped to the given address
        if (core->cp15.getItcmEnabled() && address < core->cp15.getItcmSize()) // Instruction TCM
        {
            data = &instrTcm[address & 0x7FFF];
        }
        else if (core->cp15.getDtcmEnabled() && address >= core->cp15.getDtcmAddr() &&
            address < core->cp15.getDtcmAddr() + core->cp15.getDtcmSize()) // Data TCM
        {
            data = &dataTcm[(address - core->cp15.getDtcmAddr()) & 0x3FFF];
        }
        else
        {
            switch (address & 0xFF000000)
            {
                case 0x02000000: // Main RAM
                {
                    data = &ram[address & 0x3FFFFF];
                    break;
                }

                case 0x03000000: // Shared WRAM
                {
                    switch (wramCnt)
                    {
                        case 0: data = &wram[(address & 0x7FFF)];          break;
                        case 1: data = &wram[(address & 0x3FFF) + 0x4000]; break;
                        case 2: data = &wram[(address & 0x3FFF)];          break;
                    }
                    break;
                }

                case 0x04000000: // I/O registers
                {
                    return ioRead9<T>(address);
                }

                case 0x05000000: // Palettes
                {
                    data = &palette[address & 0x7FF];
                    break;
                }

                case 0x06000000: // VRAM
                {
                    switch (address & 0xFFE00000)
                    {
                        case 0x06000000: data =  engABg[(address & 0x7FFFF) >> 14]; break;
                        case 0x06200000: data =  engBBg[(address & 0x1FFFF) >> 14]; break;
                        case 0x06400000: data = engAObj[(address & 0x3FFFF) >> 14]; break;
                        case 0x06600000: data = engBObj[(address & 0x1FFFF) >> 14]; break;
                        default:         data =    lcdc[(address & 0xFFFFF) >> 14]; break;
                    }
                    if (data) data += (address & 0x3FFF);
                    break;
                }

                case 0x07000000: // OAM
                {
                    data = &oam[address & 0x7FF];
                    break;
                }

                case 0x08000000: case 0x09000000: // GBA ROM
                {
                    if ((address & 0x01FFFFFF) < core->cartridge.getGbaRomSize())
                    {
                        data = &core->cartridge.getGbaRom()[address & 0x01FFFFFF];
                        break;
                    }
                    return (T)0xFFFFFFFF;
                }

                case 0x0A000000: // GBA SRAM
                {
                    return core->cartridge.gbaSramRead(address + 0x4000000);
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
    else if (core->isGbaMode()) // GBA
    {
        // Get a pointer to the GBA memory mapped to the given address
        switch (address & 0xFF000000)
        {
            case 0x00000000: // GBA BIOS
            {
                data = &gbaBios[address & 0x3FFF];
                break;
            }

            case 0x02000000: // On-board WRAM
            {
                data = &ram[address & 0x3FFFF];
                break;
            }

            case 0x03000000: // On-chip WRAM
            {
                data = &wram[address & 0x7FFF];
                break;
            }

            case 0x04000000: // I/O registers
            {
                return ioReadGba<T>(address);
            }

            case 0x05000000: // Palettes
            {
                data = &palette[address & 0x3FF];
                break;
            }

            case 0x06000000: // VRAM
            {
                data = &vramC[address & ((address & 0x10000) ? 0x17FFF : 0xFFFF)];
                break;
            }

            case 0x07000000: // OAM
            {
                data = &oam[address & 0x3FF];
                break;
            }

            case 0x0D000000: // EEPROM/ROM
            {
                if (core->cartridge.isGbaEeprom(address))
                    return core->cartridge.gbaEepromRead();
            }

            case 0x08000000: case 0x09000000:
            case 0x0A000000: case 0x0B000000:
            case 0x0C000000: // ROM
            {
                if ((address & 0x01FFFFFF) < core->cartridge.getGbaRomSize())
                {
                    data = &core->cartridge.getGbaRom()[address & 0x01FFFFFF];
                    break;
                }
                return (T)0xFFFFFFFF;
            }

            case 0x0E000000: // SRAM
            {
                return core->cartridge.gbaSramRead(address);
            }
        }
    }
    else // ARM7
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
                data = &ram[address & 0x3FFFFF];
                break;
            }

            case 0x03000000: // WRAM
            {
                if (!(address & 0x00800000)) // Shared WRAM
                {
                    switch (wramCnt)
                    {
                        case 1: data = &wram[(address & 0x3FFF)];          break;
                        case 2: data = &wram[(address & 0x3FFF) + 0x4000]; break;
                        case 3: data = &wram[(address & 0x7FFF)];          break;
                    }
                }
                if (!data) data = &wram7[address & 0xFFFF]; // ARM7 WRAM
                break;
            }

            case 0x04000000: // I/O registers
            {
                if (address & 0x00800000) // WiFi regions
                {
                    address &= ~0x00008000; // Mirror the regions
                    if (address >= 0x04804000 && address < 0x04806000) // WiFi RAM
                        data = &wifiRam[address & 0x1FFF];
                    else if (address < 0x04808000) // WiFi I/O registers
                        return ioRead7<T>(address);
                    break;
                }
                else
                {
                    return ioRead7<T>(address);
                }
            }

            case 0x06000000: // VRAM
            {
                data = vram7[(address & 0x3FFFF) >> 17];
                if (data) data += (address & 0x1FFFF);
                break;
            }

            case 0x08000000: case 0x09000000: // GBA ROM
            {
                if ((address & 0x01FFFFFF) < core->cartridge.getGbaRomSize())
                {
                    data = &core->cartridge.getGbaRom()[address & 0x01FFFFFF];
                    break;
                }
                return (T)0xFFFFFFFF;
            }

            case 0x0A000000: // GBA SRAM
            {
                return core->cartridge.gbaSramRead(address + 0x4000000);
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
    else if (core->isGbaMode())
    {
        printf("Unmapped GBA memory read: 0x%X\n", address);
    }
    else
    {
        printf("Unmapped ARM%d memory read: 0x%X\n", ((cpu == 0) ? 9 : 7), address);
    }

    return value;
}

template void Memory::write(bool cpu, uint32_t address, uint8_t  value);
template void Memory::write(bool cpu, uint32_t address, uint16_t value);
template void Memory::write(bool cpu, uint32_t address, uint32_t value);
template <typename T> void Memory::write(bool cpu, uint32_t address, T value)
{
    // Align the address
    address &= ~(sizeof(T) - 1);

    uint8_t *data = nullptr;

    if (cpu == 0) // ARM9
    {
        // Get a pointer to the ARM9 memory mapped to the given address
        if (core->cp15.getItcmEnabled() && address < core->cp15.getItcmSize()) // Instruction TCM
        {
            data = &instrTcm[address & 0x7FFF];
        }
        else if (core->cp15.getDtcmEnabled() && address >= core->cp15.getDtcmAddr() &&
            address < core->cp15.getDtcmAddr() + core->cp15.getDtcmSize()) // Data TCM
        {
            data = &dataTcm[(address - core->cp15.getDtcmAddr()) & 0x3FFF];
        }
        else
        {
            switch (address & 0xFF000000)
            {
                case 0x02000000: // Main RAM
                {
                    data = &ram[address & 0x3FFFFF];
                    break;
                }

                case 0x03000000: // Shared WRAM
                {
                    switch (wramCnt)
                    {
                        case 0: data = &wram[(address & 0x7FFF)];          break;
                        case 1: data = &wram[(address & 0x3FFF) + 0x4000]; break;
                        case 2: data = &wram[(address & 0x3FFF)];          break;
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
                    data = &palette[address & 0x7FF];
                    break;
                }

                case 0x06000000: // VRAM
                {
                    switch (address & 0xFFE00000)
                    {
                        case 0x06000000: data =  engABg[(address & 0x7FFFF) >> 14]; break;
                        case 0x06200000: data =  engBBg[(address & 0x1FFFF) >> 14]; break;
                        case 0x06400000: data = engAObj[(address & 0x3FFFF) >> 14]; break;
                        case 0x06600000: data = engBObj[(address & 0x1FFFF) >> 14]; break;
                        default:         data =    lcdc[(address & 0xFFFFF) >> 14]; break;
                    }
                    if (data) data += (address & 0x3FFF);
                    break;
                }

                case 0x07000000: // OAM
                {
                    data = &oam[address & 0x7FF];
                    break;
                }

                case 0x0A000000: // GBA SRAM
                {
                    core->cartridge.gbaSramWrite(address + 0x4000000, value);
                    return;
                }
            }
        }
    }
    else if (core->isGbaMode()) // GBA
    {
        // Get a pointer to the GBA memory mapped to the given address
        switch (address & 0xFF000000)
        {
            case 0x02000000: // On-board WRAM
            {
                data = &ram[address & 0x3FFFF];
                break;
            }

            case 0x03000000: // On-chip WRAM
            {
                data = &wram[address & 0x7FFF];
                break;
            }

            case 0x04000000: // I/O registers
            {
                ioWriteGba<T>(address, value);
                return;
            }

            case 0x05000000: // Palettes
            {
                data = &palette[address & 0x3FF];
                break;
            }

            case 0x06000000: // VRAM
            {
                data = &vramC[address & ((address & 0x10000) ? 0x17FFF : 0xFFFF)];
                break;
            }

            case 0x07000000: // OAM
            {
                data = &oam[address & 0x3FF];
                break;
            }

            case 0x0D000000: // EEPROM
            {
                if (core->cartridge.isGbaEeprom(address))
                    core->cartridge.gbaEepromWrite(value);
                return;
            }

            case 0x0E000000: // SRAM
            {
                core->cartridge.gbaSramWrite(address, value);
                return;
            }
        }
    }
    else // ARM7
    {
        // Get a pointer to the ARM7 memory mapped to the given address
        switch (address & 0xFF000000)
        {
            case 0x02000000: // Main RAM
            {
                data = &ram[address & 0x3FFFFF];
                break;
            }

            case 0x03000000: // WRAM
            {
                if (!(address & 0x00800000)) // Shared WRAM
                {
                    switch (wramCnt)
                    {
                        case 1: data = &wram[(address & 0x3FFF)];          break;
                        case 2: data = &wram[(address & 0x3FFF) + 0x4000]; break;
                        case 3: data = &wram[(address & 0x7FFF)];          break;
                    }
                }
                if (!data) data = &wram7[address & 0xFFFF]; // ARM7 WRAM
                break;
            }

            case 0x04000000: // I/O registers
            {
                if (address & 0x00800000) // WiFi regions
                {
                    if (sizeof(T) == 1) return; // Ignore single-byte writes
                    address &= ~0x00008000; // Mirror the regions
                    if (address >= 0x04804000 && address < 0x04806000) // WiFi RAM
                    {
                        data = &wifiRam[address & 0x1FFF];
                    }
                    else if (address < 0x04808000) // WiFi I/O registers
                    {
                        ioWrite7<T>(address, value);
                        return;
                    }
                    break;
                }
                else
                {
                    ioWrite7<T>(address, value);
                    return;
                }
            }

            case 0x06000000: // VRAM
            {
                data = vram7[(address & 0x3FFFF) >> 17];
                if (data) data += (address & 0x1FFFF);
                break;
            }

            case 0x0A000000: // GBA SRAM
            {
                core->cartridge.gbaSramWrite(address + 0x4000000, value);
                return;
            }
        }
    }

    if (data)
    {
        // Write an LSB-first value to the data at the pointer
        for (unsigned int i = 0; i < sizeof(T); i++)
            data[i] = value >> (i * 8);
    }
    else if (core->isGbaMode())
    {
        printf("Unmapped GBA memory write: 0x%X\n", address);
    }
    else
    {
        printf("Unmapped ARM%d memory write: 0x%X\n", ((cpu == 0) ? 9 : 7), address);
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
            case 0x4000003: base -= 0x4000000; size = 4; data = core->gpu2D[0].readDispCnt();        break; // DISPCNT (engine A)
            case 0x4000004:
            case 0x4000005: base -= 0x4000004; size = 2; data = core->gpu.readDispStat(0);           break; // DISPSTAT (ARM9)
            case 0x4000006:
            case 0x4000007: base -= 0x4000006; size = 2; data = core->gpu.readVCount();              break; // VCOUNT
            case 0x4000008:
            case 0x4000009: base -= 0x4000008; size = 2; data = core->gpu2D[0].readBgCnt(0);         break; // BG0CNT (engine A)
            case 0x400000A:
            case 0x400000B: base -= 0x400000A; size = 2; data = core->gpu2D[0].readBgCnt(1);         break; // BG1CNT (engine A)
            case 0x400000C:
            case 0x400000D: base -= 0x400000C; size = 2; data = core->gpu2D[0].readBgCnt(2);         break; // BG2CNT (engine A)
            case 0x400000E:
            case 0x400000F: base -= 0x400000E; size = 2; data = core->gpu2D[0].readBgCnt(3);         break; // BG3CNT (engine A)
            case 0x4000048:
            case 0x4000049: base -= 0x4000048; size = 2; data = core->gpu2D[0].readWinIn();          break; // WININ (engine A)
            case 0x400004A:
            case 0x400004B: base -= 0x400004A; size = 2; data = core->gpu2D[0].readWinOut();         break; // WINOUT (engine A)
            case 0x4000050:
            case 0x4000051: base -= 0x4000050; size = 2; data = core->gpu2D[0].readBldCnt();         break; // BLDCNT (engine A)
            case 0x4000052:
            case 0x4000053: base -= 0x4000052; size = 2; data = core->gpu2D[0].readBldAlpha();       break; // BLDALPHA (engine A)
            case 0x4000060:
            case 0x4000061: base -= 0x4000060; size = 2; data = core->gpu3DRenderer.readDisp3DCnt(); break; // DISP3DCNT
            case 0x4000064:
            case 0x4000065:
            case 0x4000066:
            case 0x4000067: base -= 0x4000064; size = 4; data = core->gpu.readDispCapCnt();          break; // DISPCAPCNT
            case 0x400006C:
            case 0x400006D: base -= 0x400006C; size = 2; data = core->gpu2D[0].readMasterBright();   break; // MASTER_BRIGHT (engine A)
            case 0x40000B0:
            case 0x40000B1:
            case 0x40000B2:
            case 0x40000B3: base -= 0x40000B0; size = 4; data = core->dma[0].readDmaSad(0);          break; // DMA0SAD (ARM9)
            case 0x40000B4:
            case 0x40000B5:
            case 0x40000B6:
            case 0x40000B7: base -= 0x40000B4; size = 4; data = core->dma[0].readDmaDad(0);          break; // DMA0DAD (ARM9)
            case 0x40000B8:
            case 0x40000B9:
            case 0x40000BA:
            case 0x40000BB: base -= 0x40000B8; size = 4; data = core->dma[0].readDmaCnt(0);          break; // DMA0CNT (ARM9)
            case 0x40000BC:
            case 0x40000BD:
            case 0x40000BE:
            case 0x40000BF: base -= 0x40000BC; size = 4; data = core->dma[0].readDmaSad(1);          break; // DMA1SAD (ARM9)
            case 0x40000C0:
            case 0x40000C1:
            case 0x40000C2:
            case 0x40000C3: base -= 0x40000C0; size = 4; data = core->dma[0].readDmaDad(1);          break; // DMA1DAD (ARM9)
            case 0x40000C4:
            case 0x40000C5:
            case 0x40000C6:
            case 0x40000C7: base -= 0x40000C4; size = 4; data = core->dma[0].readDmaCnt(1);          break; // DMA1CNT (ARM9)
            case 0x40000C8:
            case 0x40000C9:
            case 0x40000CA:
            case 0x40000CB: base -= 0x40000C8; size = 4; data = core->dma[0].readDmaSad(2);          break; // DMA2SAD (ARM9)
            case 0x40000CC:
            case 0x40000CD:
            case 0x40000CE:
            case 0x40000CF: base -= 0x40000CC; size = 4; data = core->dma[0].readDmaDad(2);          break; // DMA2DAD (ARM9)
            case 0x40000D0:
            case 0x40000D1:
            case 0x40000D2:
            case 0x40000D3: base -= 0x40000D0; size = 4; data = core->dma[0].readDmaCnt(2);          break; // DMA2CNT (ARM9)
            case 0x40000D4:
            case 0x40000D5:
            case 0x40000D6:
            case 0x40000D7: base -= 0x40000D4; size = 4; data = core->dma[0].readDmaSad(3);          break; // DMA3SAD (ARM9)
            case 0x40000D8:
            case 0x40000D9:
            case 0x40000DA:
            case 0x40000DB: base -= 0x40000D8; size = 4; data = core->dma[0].readDmaDad(3);          break; // DMA3DAD (ARM9)
            case 0x40000DC:
            case 0x40000DD:
            case 0x40000DE:
            case 0x40000DF: base -= 0x40000DC; size = 4; data = core->dma[0].readDmaCnt(3);          break; // DMA3CNT (ARM9)
            case 0x40000E0:
            case 0x40000E1:
            case 0x40000E2:
            case 0x40000E3: base -= 0x40000E0; size = 4; data = readDmaFill(0);                      break; // DMA0FILL
            case 0x40000E4:
            case 0x40000E5:
            case 0x40000E6:
            case 0x40000E7: base -= 0x40000E4; size = 4; data = readDmaFill(1);                      break; // DMA1FILL
            case 0x40000E8:
            case 0x40000E9:
            case 0x40000EA:
            case 0x40000EB: base -= 0x40000E8; size = 4; data = readDmaFill(2);                      break; // DMA2FILL
            case 0x40000EC:
            case 0x40000ED:
            case 0x40000EE:
            case 0x40000EF: base -= 0x40000EC; size = 4; data = readDmaFill(3);                      break; // DMA3FILL
            case 0x4000100:
            case 0x4000101: base -= 0x4000100; size = 2; data = core->timers[0].readTmCntL(0);       break; // TM0CNT_L (ARM9)
            case 0x4000102:
            case 0x4000103: base -= 0x4000102; size = 2; data = core->timers[0].readTmCntH(0);       break; // TM0CNT_H (ARM9)
            case 0x4000104:
            case 0x4000105: base -= 0x4000104; size = 2; data = core->timers[0].readTmCntL(1);       break; // TM1CNT_L (ARM9)
            case 0x4000106:
            case 0x4000107: base -= 0x4000106; size = 2; data = core->timers[0].readTmCntH(1);       break; // TM1CNT_H (ARM9)
            case 0x4000108:
            case 0x4000109: base -= 0x4000108; size = 2; data = core->timers[0].readTmCntL(2);       break; // TM2CNT_L (ARM9)
            case 0x400010A:
            case 0x400010B: base -= 0x400010A; size = 2; data = core->timers[0].readTmCntH(2);       break; // TM2CNT_H (ARM9)
            case 0x400010C:
            case 0x400010D: base -= 0x400010C; size = 2; data = core->timers[0].readTmCntL(3);       break; // TM3CNT_L (ARM9)
            case 0x400010E:
            case 0x400010F: base -= 0x400010E; size = 2; data = core->timers[0].readTmCntH(3);       break; // TM3CNT_H (ARM9)
            case 0x4000130:
            case 0x4000131: base -= 0x4000130; size = 2; data = core->input.readKeyInput();          break; // KEYINPUT
            case 0x4000180:
            case 0x4000181: base -= 0x4000180; size = 2; data = core->ipc.readIpcSync(0);            break; // IPCSYNC (ARM9)
            case 0x4000184:
            case 0x4000185: base -= 0x4000184; size = 2; data = core->ipc.readIpcFifoCnt(0);         break; // IPCFIFOCNT (ARM9)
            case 0x40001A0:
            case 0x40001A1: base -= 0x40001A0; size = 2; data = core->cartridge.readAuxSpiCnt(0);    break; // AUXSPICNT (ARM9)
            case 0x40001A2: base -= 0x40001A2; size = 1; data = core->cartridge.readAuxSpiData(0);   break; // AUXSPIDATA (ARM9)
            case 0x40001A4:
            case 0x40001A5:
            case 0x40001A6:
            case 0x40001A7: base -= 0x40001A4; size = 4; data = core->cartridge.readRomCtrl(0);      break; // ROMCTRL (ARM9)
            case 0x4000208: base -= 0x4000208; size = 1; data = core->interpreter[0].readIme();      break; // IME (ARM9)
            case 0x4000210:
            case 0x4000211:
            case 0x4000212:
            case 0x4000213: base -= 0x4000210; size = 4; data = core->interpreter[0].readIe();       break; // IE (ARM9)
            case 0x4000214:
            case 0x4000215:
            case 0x4000216:
            case 0x4000217: base -= 0x4000214; size = 4; data = core->interpreter[0].readIrf();      break; // IF (ARM9)
            case 0x4000240: base -= 0x4000240; size = 1; data = readVramCnt(0);                      break; // VRAMCNT_A
            case 0x4000241: base -= 0x4000241; size = 1; data = readVramCnt(1);                      break; // VRAMCNT_B
            case 0x4000242: base -= 0x4000242; size = 1; data = readVramCnt(2);                      break; // VRAMCNT_C
            case 0x4000243: base -= 0x4000243; size = 1; data = readVramCnt(3);                      break; // VRAMCNT_D
            case 0x4000244: base -= 0x4000244; size = 1; data = readVramCnt(4);                      break; // VRAMCNT_E
            case 0x4000245: base -= 0x4000245; size = 1; data = readVramCnt(5);                      break; // VRAMCNT_F
            case 0x4000246: base -= 0x4000246; size = 1; data = readVramCnt(6);                      break; // VRAMCNT_G
            case 0x4000247: base -= 0x4000247; size = 1; data = readWramCnt();                       break; // WRAMCNT
            case 0x4000248: base -= 0x4000248; size = 1; data = readVramCnt(7);                      break; // VRAMCNT_H
            case 0x4000249: base -= 0x4000249; size = 1; data = readVramCnt(8);                      break; // VRAMCNT_I
            case 0x4000280:
            case 0x4000281: base -= 0x4000280; size = 2; data = core->divSqrt.readDivCnt();          break; // DIVCNT
            case 0x4000290:
            case 0x4000291:
            case 0x4000292:
            case 0x4000293: base -= 0x4000290; size = 4; data = core->divSqrt.readDivNumerL();       break; // DIVNUMER_L
            case 0x4000294:
            case 0x4000295:
            case 0x4000296:
            case 0x4000297: base -= 0x4000294; size = 4; data = core->divSqrt.readDivNumerH();       break; // DIVNUMER_H
            case 0x4000298:
            case 0x4000299:
            case 0x400029A:
            case 0x400029B: base -= 0x4000298; size = 4; data = core->divSqrt.readDivDenomL();       break; // DIVDENOM_L
            case 0x400029C:
            case 0x400029D:
            case 0x400029E:
            case 0x400029F: base -= 0x400029C; size = 4; data = core->divSqrt.readDivDenomH();       break; // DIVDENOM_H
            case 0x40002A0:
            case 0x40002A1:
            case 0x40002A2:
            case 0x40002A3: base -= 0x40002A0; size = 4; data = core->divSqrt.readDivResultL();      break; // DIVRESULT_L
            case 0x40002A4:
            case 0x40002A5:
            case 0x40002A6:
            case 0x40002A7: base -= 0x40002A4; size = 4; data = core->divSqrt.readDivResultH();      break; // DIVRESULT_H
            case 0x40002A8:
            case 0x40002A9:
            case 0x40002AA:
            case 0x40002AB: base -= 0x40002A8; size = 4; data = core->divSqrt.readDivRemResultL();   break; // DIVREMRESULT_L
            case 0x40002AC:
            case 0x40002AD:
            case 0x40002AE:
            case 0x40002AF: base -= 0x40002AC; size = 4; data = core->divSqrt.readDivRemResultH();   break; // DIVREMRESULT_H
            case 0x40002B0:
            case 0x40002B1: base -= 0x40002B0; size = 2; data = core->divSqrt.readSqrtCnt();         break; // SQRTCNT
            case 0x40002B4:
            case 0x40002B5:
            case 0x40002B6:
            case 0x40002B7: base -= 0x40002B4; size = 4; data = core->divSqrt.readSqrtResult();      break; // SQRTRESULT
            case 0x40002B8:
            case 0x40002B9:
            case 0x40002BA:
            case 0x40002BB: base -= 0x40002B8; size = 4; data = core->divSqrt.readSqrtParamL();      break; // SQRTPARAM_L
            case 0x40002BC:
            case 0x40002BD:
            case 0x40002BE:
            case 0x40002BF: base -= 0x40002BC; size = 4; data = core->divSqrt.readSqrtParamH();      break; // SQRTPARAM_H
            case 0x4000300: base -= 0x4000300; size = 1; data = core->interpreter[0].readPostFlg();  break; // POSTFLG (ARM9)
            case 0x4000304:
            case 0x4000305: base -= 0x4000304; size = 2; data = core->gpu.readPowCnt1();             break; // POWCNT1
            case 0x4000600:
            case 0x4000601:
            case 0x4000602:
            case 0x4000603: base -= 0x4000600; size = 4; data = core->gpu3D.readGxStat();            break; // GXSTAT
            case 0x4000604:
            case 0x4000605:
            case 0x4000606:
            case 0x4000607: base -= 0x4000604; size = 4; data = core->gpu3D.readRamCount();          break; // RAM_COUNT
            case 0x4000620:
            case 0x4000621:
            case 0x4000622:
            case 0x4000623: base -= 0x4000620; size = 4; data = core->gpu3D.readPosResult(0);        break; // POS_RESULT
            case 0x4000624:
            case 0x4000625:
            case 0x4000626:
            case 0x4000627: base -= 0x4000624; size = 4; data = core->gpu3D.readPosResult(1);        break; // POS_RESULT
            case 0x4000628:
            case 0x4000629:
            case 0x400062A:
            case 0x400062B: base -= 0x4000628; size = 4; data = core->gpu3D.readPosResult(2);        break; // POS_RESULT
            case 0x400062C:
            case 0x400062D:
            case 0x400062E:
            case 0x400062F: base -= 0x400062C; size = 4; data = core->gpu3D.readPosResult(3);        break; // POS_RESULT
            case 0x4000630:
            case 0x4000631: base -= 0x4000630; size = 2; data = core->gpu3D.readVecResult(0);        break; // VEC_RESULT
            case 0x4000632:
            case 0x4000633: base -= 0x4000632; size = 2; data = core->gpu3D.readVecResult(1);        break; // VEC_RESULT
            case 0x4000634:
            case 0x4000635: base -= 0x4000634; size = 2; data = core->gpu3D.readVecResult(2);        break; // VEC_RESULT
            case 0x4000640:
            case 0x4000641:
            case 0x4000642:
            case 0x4000643: base -= 0x4000640; size = 4; data = core->gpu3D.readClipMtxResult(0);    break; // CLIPMTX_RESULT
            case 0x4000644:
            case 0x4000645:
            case 0x4000646:
            case 0x4000647: base -= 0x4000644; size = 4; data = core->gpu3D.readClipMtxResult(1);    break; // CLIPMTX_RESULT
            case 0x4000648:
            case 0x4000649:
            case 0x400064A:
            case 0x400064B: base -= 0x4000648; size = 4; data = core->gpu3D.readClipMtxResult(2);    break; // CLIPMTX_RESULT
            case 0x400064C:
            case 0x400064D:
            case 0x400064E:
            case 0x400064F: base -= 0x400064C; size = 4; data = core->gpu3D.readClipMtxResult(3);    break; // CLIPMTX_RESULT
            case 0x4000650:
            case 0x4000651:
            case 0x4000652:
            case 0x4000653: base -= 0x4000650; size = 4; data = core->gpu3D.readClipMtxResult(4);    break; // CLIPMTX_RESULT
            case 0x4000654:
            case 0x4000655:
            case 0x4000656:
            case 0x4000657: base -= 0x4000654; size = 4; data = core->gpu3D.readClipMtxResult(5);    break; // CLIPMTX_RESULT
            case 0x4000658:
            case 0x4000659:
            case 0x400065A:
            case 0x400065B: base -= 0x4000658; size = 4; data = core->gpu3D.readClipMtxResult(6);    break; // CLIPMTX_RESULT
            case 0x400065C:
            case 0x400065D:
            case 0x400065E:
            case 0x400065F: base -= 0x400065C; size = 4; data = core->gpu3D.readClipMtxResult(7);    break; // CLIPMTX_RESULT
            case 0x4000660:
            case 0x4000661:
            case 0x4000662:
            case 0x4000663: base -= 0x4000660; size = 4; data = core->gpu3D.readClipMtxResult(8);    break; // CLIPMTX_RESULT
            case 0x4000664:
            case 0x4000665:
            case 0x4000666:
            case 0x4000667: base -= 0x4000664; size = 4; data = core->gpu3D.readClipMtxResult(9);    break; // CLIPMTX_RESULT
            case 0x4000668:
            case 0x4000669:
            case 0x400066A:
            case 0x400066B: base -= 0x4000668; size = 4; data = core->gpu3D.readClipMtxResult(10);   break; // CLIPMTX_RESULT
            case 0x400066C:
            case 0x400066D:
            case 0x400066E:
            case 0x400066F: base -= 0x400066C; size = 4; data = core->gpu3D.readClipMtxResult(11);   break; // CLIPMTX_RESULT
            case 0x4000670:
            case 0x4000671:
            case 0x4000672:
            case 0x4000673: base -= 0x4000670; size = 4; data = core->gpu3D.readClipMtxResult(12);   break; // CLIPMTX_RESULT
            case 0x4000674:
            case 0x4000675:
            case 0x4000676:
            case 0x4000677: base -= 0x4000674; size = 4; data = core->gpu3D.readClipMtxResult(13);   break; // CLIPMTX_RESULT
            case 0x4000678:
            case 0x4000679:
            case 0x400067A:
            case 0x400067B: base -= 0x4000678; size = 4; data = core->gpu3D.readClipMtxResult(14);   break; // CLIPMTX_RESULT
            case 0x400067C:
            case 0x400067D:
            case 0x400067E:
            case 0x400067F: base -= 0x400067C; size = 4; data = core->gpu3D.readClipMtxResult(15);   break; // CLIPMTX_RESULT
            case 0x4000680:
            case 0x4000681:
            case 0x4000682:
            case 0x4000683: base -= 0x4000680; size = 4; data = core->gpu3D.readVecMtxResult(0);     break; // VECMTX_RESULT
            case 0x4000684:
            case 0x4000685:
            case 0x4000686:
            case 0x4000687: base -= 0x4000684; size = 4; data = core->gpu3D.readVecMtxResult(1);     break; // VECMTX_RESULT
            case 0x4000688:
            case 0x4000689:
            case 0x400068A:
            case 0x400068B: base -= 0x4000688; size = 4; data = core->gpu3D.readVecMtxResult(2);     break; // VECMTX_RESULT
            case 0x400068C:
            case 0x400068D:
            case 0x400068E:
            case 0x400068F: base -= 0x400068C; size = 4; data = core->gpu3D.readVecMtxResult(3);     break; // VECMTX_RESULT
            case 0x4000690:
            case 0x4000691:
            case 0x4000692:
            case 0x4000693: base -= 0x4000690; size = 4; data = core->gpu3D.readVecMtxResult(4);     break; // VECMTX_RESULT
            case 0x4000694:
            case 0x4000695:
            case 0x4000696:
            case 0x4000697: base -= 0x4000694; size = 4; data = core->gpu3D.readVecMtxResult(5);     break; // VECMTX_RESULT
            case 0x4000698:
            case 0x4000699:
            case 0x400069A:
            case 0x400069B: base -= 0x4000698; size = 4; data = core->gpu3D.readVecMtxResult(6);     break; // VECMTX_RESULT
            case 0x400069C:
            case 0x400069D:
            case 0x400069E:
            case 0x400069F: base -= 0x400069C; size = 4; data = core->gpu3D.readVecMtxResult(7);     break; // VECMTX_RESULT
            case 0x40006A0:
            case 0x40006A1:
            case 0x40006A2:
            case 0x40006A3: base -= 0x40006A0; size = 4; data = core->gpu3D.readVecMtxResult(8);     break; // VECMTX_RESULT
            case 0x4001000:
            case 0x4001001:
            case 0x4001002:
            case 0x4001003: base -= 0x4001000; size = 4; data = core->gpu2D[1].readDispCnt();        break; // DISPCNT (engine B)
            case 0x4001008:
            case 0x4001009: base -= 0x4001008; size = 2; data = core->gpu2D[1].readBgCnt(0);         break; // BG0CNT (engine B)
            case 0x400100A:
            case 0x400100B: base -= 0x400100A; size = 2; data = core->gpu2D[1].readBgCnt(1);         break; // BG1CNT (engine B)
            case 0x400100C:
            case 0x400100D: base -= 0x400100C; size = 2; data = core->gpu2D[1].readBgCnt(2);         break; // BG2CNT (engine B)
            case 0x400100E:
            case 0x400100F: base -= 0x400100E; size = 2; data = core->gpu2D[1].readBgCnt(3);         break; // BG3CNT (engine B)
            case 0x4001048:
            case 0x4001049: base -= 0x4001048; size = 2; data = core->gpu2D[1].readWinIn();          break; // WININ (engine B)
            case 0x400104A:
            case 0x400104B: base -= 0x400104A; size = 2; data = core->gpu2D[1].readWinOut();         break; // WINOUT (engine B)
            case 0x4001050:
            case 0x4001051: base -= 0x4001050; size = 2; data = core->gpu2D[1].readBldCnt();         break; // BLDCNT (engine B)
            case 0x4001052:
            case 0x4001053: base -= 0x4001052; size = 2; data = core->gpu2D[1].readBldAlpha();       break; // BLDALPHA (engine B)
            case 0x400106C:
            case 0x400106D: base -= 0x400106C; size = 2; data = core->gpu2D[1].readMasterBright();   break; // MASTER_BRIGHT (engine B)
            case 0x4100000:
            case 0x4100001:
            case 0x4100002:
            case 0x4100003: base -= 0x4100000; size = 4; data = core->ipc.readIpcFifoRecv(0);        break; // IPCFIFORECV (ARM9)
            case 0x4100010:
            case 0x4100011:
            case 0x4100012:
            case 0x4100013: base -= 0x4100010; size = 4; data = core->cartridge.readRomDataIn(0);    break; // ROMDATAIN (ARM9)

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
            case 0x4000005: base -= 0x4000004; size = 2; data = core->gpu.readDispStat(1);          break; // DISPSTAT (ARM7)
            case 0x4000006:
            case 0x4000007: base -= 0x4000006; size = 2; data = core->gpu.readVCount();             break; // VCOUNT
            case 0x40000B0:
            case 0x40000B1:
            case 0x40000B2:
            case 0x40000B3: base -= 0x40000B0; size = 4; data = core->dma[1].readDmaSad(0);         break; // DMA0SAD (ARM7)
            case 0x40000B4:
            case 0x40000B5:
            case 0x40000B6:
            case 0x40000B7: base -= 0x40000B4; size = 4; data = core->dma[1].readDmaDad(0);         break; // DMA0DAD (ARM7)
            case 0x40000B8:
            case 0x40000B9:
            case 0x40000BA:
            case 0x40000BB: base -= 0x40000B8; size = 4; data = core->dma[1].readDmaCnt(0);         break; // DMA0CNT (ARM7)
            case 0x40000BC:
            case 0x40000BD:
            case 0x40000BE:
            case 0x40000BF: base -= 0x40000BC; size = 4; data = core->dma[1].readDmaSad(1);         break; // DMA1SAD (ARM7)
            case 0x40000C0:
            case 0x40000C1:
            case 0x40000C2:
            case 0x40000C3: base -= 0x40000C0; size = 4; data = core->dma[1].readDmaDad(1);         break; // DMA1DAD (ARM7)
            case 0x40000C4:
            case 0x40000C5:
            case 0x40000C6:
            case 0x40000C7: base -= 0x40000C4; size = 4; data = core->dma[1].readDmaCnt(1);         break; // DMA1CNT (ARM7)
            case 0x40000C8:
            case 0x40000C9:
            case 0x40000CA:
            case 0x40000CB: base -= 0x40000C8; size = 4; data = core->dma[1].readDmaSad(2);         break; // DMA2SAD (ARM7)
            case 0x40000CC:
            case 0x40000CD:
            case 0x40000CE:
            case 0x40000CF: base -= 0x40000CC; size = 4; data = core->dma[1].readDmaDad(2);         break; // DMA2DAD (ARM7)
            case 0x40000D0:
            case 0x40000D1:
            case 0x40000D2:
            case 0x40000D3: base -= 0x40000D0; size = 4; data = core->dma[1].readDmaCnt(2);         break; // DMA2CNT (ARM7)
            case 0x40000D4:
            case 0x40000D5:
            case 0x40000D6:
            case 0x40000D7: base -= 0x40000D4; size = 4; data = core->dma[1].readDmaSad(3);         break; // DMA3SAD (ARM7)
            case 0x40000D8:
            case 0x40000D9:
            case 0x40000DA:
            case 0x40000DB: base -= 0x40000D8; size = 4; data = core->dma[1].readDmaDad(3);         break; // DMA3DAD (ARM7)
            case 0x40000DC:
            case 0x40000DD:
            case 0x40000DE:
            case 0x40000DF: base -= 0x40000DC; size = 4; data = core->dma[1].readDmaCnt(3);         break; // DMA3CNT (ARM7)
            case 0x4000100:
            case 0x4000101: base -= 0x4000100; size = 2; data = core->timers[1].readTmCntL(0);      break; // TM0CNT_L (ARM7)
            case 0x4000102:
            case 0x4000103: base -= 0x4000102; size = 2; data = core->timers[1].readTmCntH(0);      break; // TM0CNT_H (ARM7)
            case 0x4000104:
            case 0x4000105: base -= 0x4000104; size = 2; data = core->timers[1].readTmCntL(1);      break; // TM1CNT_L (ARM7)
            case 0x4000106:
            case 0x4000107: base -= 0x4000106; size = 2; data = core->timers[1].readTmCntH(1);      break; // TM1CNT_H (ARM7)
            case 0x4000108:
            case 0x4000109: base -= 0x4000108; size = 2; data = core->timers[1].readTmCntL(2);      break; // TM2CNT_L (ARM7)
            case 0x400010A:
            case 0x400010B: base -= 0x400010A; size = 2; data = core->timers[1].readTmCntH(2);      break; // TM2CNT_H (ARM7)
            case 0x400010C:
            case 0x400010D: base -= 0x400010C; size = 2; data = core->timers[1].readTmCntL(3);      break; // TM3CNT_L (ARM7)
            case 0x400010E:
            case 0x400010F: base -= 0x400010E; size = 2; data = core->timers[1].readTmCntH(3);      break; // TM3CNT_H (ARM7)
            case 0x4000130:
            case 0x4000131: base -= 0x4000130; size = 2; data = core->input.readKeyInput();         break; // KEYINPUT
            case 0x4000136:
            case 0x4000137: base -= 0x4000136; size = 2; data = core->input.readExtKeyIn();         break; // EXTKEYIN
            case 0x4000138: base -= 0x4000138; size = 1; data = core->rtc.readRtc();                break; // RTC
            case 0x4000180:
            case 0x4000181: base -= 0x4000180; size = 2; data = core->ipc.readIpcSync(1);           break; // IPCSYNC (ARM7)
            case 0x4000184:
            case 0x4000185: base -= 0x4000184; size = 2; data = core->ipc.readIpcFifoCnt(1);        break; // IPCFIFOCNT (ARM7)
            case 0x40001A0:
            case 0x40001A1: base -= 0x40001A0; size = 2; data = core->cartridge.readAuxSpiCnt(1);   break; // AUXSPICNT (ARM7)
            case 0x40001A2: base -= 0x40001A2; size = 1; data = core->cartridge.readAuxSpiData(1);  break; // AUXSPIDATA (ARM7)
            case 0x40001A4:
            case 0x40001A5:
            case 0x40001A6:
            case 0x40001A7: base -= 0x40001A4; size = 4; data = core->cartridge.readRomCtrl(1);     break; // ROMCTRL (ARM7)
            case 0x40001C0:
            case 0x40001C1: base -= 0x40001C0; size = 2; data = core->spi.readSpiCnt();             break; // SPICNT
            case 0x40001C2: base -= 0x40001C2; size = 1; data = core->spi.readSpiData();            break; // SPIDATA
            case 0x4000208: base -= 0x4000208; size = 1; data = core->interpreter[1].readIme();     break; // IME (ARM7)
            case 0x4000210:
            case 0x4000211:
            case 0x4000212:
            case 0x4000213: base -= 0x4000210; size = 4; data = core->interpreter[1].readIe();      break; // IE (ARM7)
            case 0x4000214:
            case 0x4000215:
            case 0x4000216:
            case 0x4000217: base -= 0x4000214; size = 4; data = core->interpreter[1].readIrf();     break; // IF (ARM7)
            case 0x4000240: base -= 0x4000240; size = 1; data = readVramStat();                     break; // VRAMSTAT
            case 0x4000241: base -= 0x4000241; size = 1; data = readWramCnt();                      break; // WRAMSTAT
            case 0x4000300: base -= 0x4000300; size = 1; data = core->interpreter[1].readPostFlg(); break; // POSTFLG (ARM7)
            case 0x4000301: base -= 0x4000301; size = 1; data = readHaltCnt();                      break; // HALTCNT
            case 0x4000400:
            case 0x4000401:
            case 0x4000402:
            case 0x4000403: base -= 0x4000400; size = 4; data = core->spu.readSoundCnt(0);          break; // SOUND0CNT
            case 0x4000410:
            case 0x4000411:
            case 0x4000412:
            case 0x4000413: base -= 0x4000410; size = 4; data = core->spu.readSoundCnt(1);          break; // SOUND1CNT
            case 0x4000420:
            case 0x4000421:
            case 0x4000422:
            case 0x4000423: base -= 0x4000420; size = 4; data = core->spu.readSoundCnt(2);          break; // SOUND2CNT
            case 0x4000430:
            case 0x4000431:
            case 0x4000432:
            case 0x4000433: base -= 0x4000430; size = 4; data = core->spu.readSoundCnt(3);          break; // SOUND3CNT
            case 0x4000440:
            case 0x4000441:
            case 0x4000442:
            case 0x4000443: base -= 0x4000440; size = 4; data = core->spu.readSoundCnt(4);          break; // SOUND4CNT
            case 0x4000450:
            case 0x4000451:
            case 0x4000452:
            case 0x4000453: base -= 0x4000450; size = 4; data = core->spu.readSoundCnt(5);          break; // SOUND5CNT
            case 0x4000460:
            case 0x4000461:
            case 0x4000462:
            case 0x4000463: base -= 0x4000460; size = 4; data = core->spu.readSoundCnt(6);          break; // SOUND6CNT
            case 0x4000470:
            case 0x4000471:
            case 0x4000472:
            case 0x4000473: base -= 0x4000470; size = 4; data = core->spu.readSoundCnt(7);          break; // SOUND7CNT
            case 0x4000480:
            case 0x4000481:
            case 0x4000482:
            case 0x4000483: base -= 0x4000480; size = 4; data = core->spu.readSoundCnt(8);          break; // SOUND8CNT
            case 0x4000490:
            case 0x4000491:
            case 0x4000492:
            case 0x4000493: base -= 0x4000490; size = 4; data = core->spu.readSoundCnt(9);          break; // SOUND9CNT
            case 0x40004A0:
            case 0x40004A1:
            case 0x40004A2:
            case 0x40004A3: base -= 0x40004A0; size = 4; data = core->spu.readSoundCnt(10);         break; // SOUND10CNT
            case 0x40004B0:
            case 0x40004B1:
            case 0x40004B2:
            case 0x40004B3: base -= 0x40004B0; size = 4; data = core->spu.readSoundCnt(11);         break; // SOUND11CNT
            case 0x40004C0:
            case 0x40004C1:
            case 0x40004C2:
            case 0x40004C3: base -= 0x40004C0; size = 4; data = core->spu.readSoundCnt(12);         break; // SOUND12CNT
            case 0x40004D0:
            case 0x40004D1:
            case 0x40004D2:
            case 0x40004D3: base -= 0x40004D0; size = 4; data = core->spu.readSoundCnt(13);         break; // SOUND13CNT
            case 0x40004E0:
            case 0x40004E1:
            case 0x40004E2:
            case 0x40004E3: base -= 0x40004E0; size = 4; data = core->spu.readSoundCnt(14);         break; // SOUND14CNT
            case 0x40004F0:
            case 0x40004F1:
            case 0x40004F2:
            case 0x40004F3: base -= 0x40004F0; size = 4; data = core->spu.readSoundCnt(15);         break; // SOUND15CNT
            case 0x4000500:
            case 0x4000501: base -= 0x4000500; size = 2; data = core->spu.readMainSoundCnt();       break; // SOUNDCNT
            case 0x4000504:
            case 0x4000505: base -= 0x4000504; size = 2; data = core->spu.readSoundBias();          break; // SOUNDBIAS
            case 0x4000508: base -= 0x4000508; size = 1; data = core->spu.readSndCapCnt(0);         break; // SNDCAP0CNT
            case 0x4000509: base -= 0x4000509; size = 1; data = core->spu.readSndCapCnt(1);         break; // SNDCAP1CNT
            case 0x4000510:
            case 0x4000511:
            case 0x4000512:
            case 0x4000513: base -= 0x4000510; size = 4; data = core->spu.readSndCapDad(0);         break; // SNDCAP0DAD
            case 0x4000518:
            case 0x4000519:
            case 0x400051A:
            case 0x400051B: base -= 0x4000518; size = 4; data = core->spu.readSndCapDad(1);         break; // SNDCAP1DAD
            case 0x4100000:
            case 0x4100001:
            case 0x4100002:
            case 0x4100003: base -= 0x4100000; size = 4; data = core->ipc.readIpcFifoRecv(1);       break; // IPCFIFORECV (ARM7)
            case 0x4100010:
            case 0x4100011:
            case 0x4100012:
            case 0x4100013: base -= 0x4100010; size = 4; data = core->cartridge.readRomDataIn(1);   break; // ROMDATAIN (ARM7)
            case 0x4800006:
            case 0x4800007: base -= 0x4800006; size = 2; data = core->wifi.readWModeWep();          break; // W_MODE_WEP
            case 0x4800010:
            case 0x4800011: base -= 0x4800010; size = 2; data = core->wifi.readWIrf();              break; // W_IF
            case 0x4800012:
            case 0x4800013: base -= 0x4800012; size = 2; data = core->wifi.readWIe();               break; // W_IE
            case 0x4800018:
            case 0x4800019: base -= 0x4800018; size = 2; data = core->wifi.readWMacaddr(0);         break; // W_MACADDR_0
            case 0x480001A:
            case 0x480001B: base -= 0x480001A; size = 2; data = core->wifi.readWMacaddr(1);         break; // W_MACADDR_1
            case 0x480001C:
            case 0x480001D: base -= 0x480001C; size = 2; data = core->wifi.readWMacaddr(2);         break; // W_MACADDR_2
            case 0x4800020:
            case 0x4800021: base -= 0x4800020; size = 2; data = core->wifi.readWBssid(0);           break; // W_BSSID_0
            case 0x4800022:
            case 0x4800023: base -= 0x4800022; size = 2; data = core->wifi.readWBssid(1);           break; // W_BSSID_1
            case 0x4800024:
            case 0x4800025: base -= 0x4800024; size = 2; data = core->wifi.readWBssid(2);           break; // W_BSSID_2
            case 0x480002A:
            case 0x480002B: base -= 0x480002A; size = 2; data = core->wifi.readWAidFull();          break; // W_AID_FULL
            case 0x480003C:
            case 0x480003D: base -= 0x480003C; size = 2; data = core->wifi.readWPowerstate();       break; // W_POWERSTATE
            case 0x4800040:
            case 0x4800041: base -= 0x4800040; size = 2; data = core->wifi.readWPowerforce();       break; // W_POWERFORCE
            case 0x4800050:
            case 0x4800051: base -= 0x4800050; size = 2; data = core->wifi.readWRxbufBegin();       break; // W_RXBUF_BEGIN
            case 0x4800052:
            case 0x4800053: base -= 0x4800052; size = 2; data = core->wifi.readWRxbufEnd();         break; // W_RXBUF_END
            case 0x4800056:
            case 0x4800057: base -= 0x4800056; size = 2; data = core->wifi.readWRxbufWrAddr();      break; // W_RXBUF_WR_ADDR
            case 0x4800058:
            case 0x4800059: base -= 0x4800058; size = 2; data = core->wifi.readWRxbufRdAddr();      break; // W_RXBUF_RD_ADDR
            case 0x480005A:
            case 0x480005B: base -= 0x480005A; size = 2; data = core->wifi.readWRxbufReadcsr();     break; // W_RXBUF_READCSR
            case 0x480005C:
            case 0x480005D: base -= 0x480005C; size = 2; data = core->wifi.readWRxbufCount();       break; // W_RXBUF_COUNT
            case 0x4800060:
            case 0x4800061: base -= 0x4800060; size = 2; data = core->wifi.readWRxbufRdData();      break; // W_RXBUF_RD_DATA
            case 0x4800062:
            case 0x4800063: base -= 0x4800062; size = 2; data = core->wifi.readWRxbufGap();         break; // W_RXBUF_GAP
            case 0x4800064:
            case 0x4800065: base -= 0x4800064; size = 2; data = core->wifi.readWRxbufGapdisp();     break; // W_RXBUF_GAPDISP
            case 0x4800068:
            case 0x4800069: base -= 0x4800068; size = 2; data = core->wifi.readWTxbufWrAddr();      break; // W_TXBUF_WR_ADDR
            case 0x480006C:
            case 0x480006D: base -= 0x480006C; size = 2; data = core->wifi.readWTxbufCount();       break; // W_TXBUF_COUNT
            case 0x4800074:
            case 0x4800075: base -= 0x4800074; size = 2; data = core->wifi.readWTxbufGap();         break; // W_TXBUF_GAP
            case 0x4800076:
            case 0x4800077: base -= 0x4800076; size = 2; data = core->wifi.readWTxbufGapdisp();     break; // W_TXBUF_GAPDISP
            case 0x4800120:
            case 0x4800121: base -= 0x4800120; size = 2; data = core->wifi.readWConfig(0);          break; // W_CONFIG_120
            case 0x4800122:
            case 0x4800123: base -= 0x4800122; size = 2; data = core->wifi.readWConfig(1);          break; // W_CONFIG_122
            case 0x4800124:
            case 0x4800125: base -= 0x4800124; size = 2; data = core->wifi.readWConfig(2);          break; // W_CONFIG_124
            case 0x4800128:
            case 0x4800129: base -= 0x4800128; size = 2; data = core->wifi.readWConfig(3);          break; // W_CONFIG_128
            case 0x4800130:
            case 0x4800131: base -= 0x4800130; size = 2; data = core->wifi.readWConfig(4);          break; // W_CONFIG_130
            case 0x4800132:
            case 0x4800133: base -= 0x4800132; size = 2; data = core->wifi.readWConfig(5);          break; // W_CONFIG_132
            case 0x4800134:
            case 0x4800135: base -= 0x4800134; size = 2; data = core->wifi.readWBeaconcount2();     break; // W_BEACONCOUNT2
            case 0x4800140:
            case 0x4800141: base -= 0x4800140; size = 2; data = core->wifi.readWConfig(6);          break; // W_CONFIG_140
            case 0x4800142:
            case 0x4800143: base -= 0x4800142; size = 2; data = core->wifi.readWConfig(7);          break; // W_CONFIG_142
            case 0x4800144:
            case 0x4800145: base -= 0x4800144; size = 2; data = core->wifi.readWConfig(8);          break; // W_CONFIG_144
            case 0x4800146:
            case 0x4800147: base -= 0x4800146; size = 2; data = core->wifi.readWConfig(9);          break; // W_CONFIG_146
            case 0x4800148:
            case 0x4800149: base -= 0x4800148; size = 2; data = core->wifi.readWConfig(10);         break; // W_CONFIG_148
            case 0x480014A:
            case 0x480014B: base -= 0x480014A; size = 2; data = core->wifi.readWConfig(11);         break; // W_CONFIG_14A
            case 0x480014C:
            case 0x480014D: base -= 0x480014C; size = 2; data = core->wifi.readWConfig(12);         break; // W_CONFIG_14C
            case 0x4800150:
            case 0x4800151: base -= 0x4800150; size = 2; data = core->wifi.readWConfig(13);         break; // W_CONFIG_150
            case 0x4800154:
            case 0x4800155: base -= 0x4800154; size = 2; data = core->wifi.readWConfig(14);         break; // W_CONFIG_154
            case 0x480015C:
            case 0x480015D: base -= 0x480015C; size = 2; data = core->wifi.readWBbRead();           break; // W_BB_READ

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

template <typename T> T Memory::ioReadGba(uint32_t address)
{
    T value = 0;
    unsigned int i = 0;

    // Read a value from one or more GBA I/O registers
    while (i < sizeof(T))
    {
        uint32_t base = address + i;
        unsigned int size;
        uint32_t data;

        switch (base)
        {
            case 0x4000000:
            case 0x4000001: base -= 0x4000000; size = 2; data = core->gpu2D[0].readDispCnt();       break; // DISPCNT
            case 0x4000004:
            case 0x4000005: base -= 0x4000004; size = 2; data = core->gpu.readDispStat(1);          break; // DISPSTAT
            case 0x4000006:
            case 0x4000007: base -= 0x4000006; size = 2; data = core->gpu.readVCount();             break; // VCOUNT
            case 0x4000008:
            case 0x4000009: base -= 0x4000008; size = 2; data = core->gpu2D[0].readBgCnt(0);        break; // BG0CNT
            case 0x400000A:
            case 0x400000B: base -= 0x400000A; size = 2; data = core->gpu2D[0].readBgCnt(1);        break; // BG1CNT
            case 0x400000C:
            case 0x400000D: base -= 0x400000C; size = 2; data = core->gpu2D[0].readBgCnt(2);        break; // BG2CNT
            case 0x400000E:
            case 0x400000F: base -= 0x400000E; size = 2; data = core->gpu2D[0].readBgCnt(3);        break; // BG3CNT
            case 0x4000048:
            case 0x4000049: base -= 0x4000048; size = 2; data = core->gpu2D[0].readWinIn();         break; // WININ
            case 0x400004A:
            case 0x400004B: base -= 0x400004A; size = 2; data = core->gpu2D[0].readWinOut();        break; // WINOUT
            case 0x4000050:
            case 0x4000051: base -= 0x4000050; size = 2; data = core->gpu2D[0].readBldCnt();        break; // BLDCNT
            case 0x4000052:
            case 0x4000053: base -= 0x4000052; size = 2; data = core->gpu2D[0].readBldAlpha();      break; // BLDALPHA
            case 0x4000060:
            case 0x4000061: base -= 0x4000060; size = 2; data = core->spu.readGbaSoundCntL(0);      break; // SOUND0CNT_L
            case 0x4000062:
            case 0x4000063: base -= 0x4000062; size = 2; data = core->spu.readGbaSoundCntH(0);      break; // SOUND0CNT_H
            case 0x4000064:
            case 0x4000065: base -= 0x4000064; size = 2; data = core->spu.readGbaSoundCntX(0);      break; // SOUND0CNT_X
            case 0x4000068:
            case 0x4000069: base -= 0x4000068; size = 2; data = core->spu.readGbaSoundCntL(1);      break; // SOUND1CNT_L
            case 0x400006C:
            case 0x400006D: base -= 0x400006C; size = 2; data = core->spu.readGbaSoundCntX(1);      break; // SOUND1CNT_X
            case 0x4000070:
            case 0x4000071: base -= 0x4000070; size = 2; data = core->spu.readGbaSoundCntL(2);      break; // SOUND2CNT_L
            case 0x4000072:
            case 0x4000073: base -= 0x4000072; size = 2; data = core->spu.readGbaSoundCntH(2);      break; // SOUND2CNT_H
            case 0x4000074:
            case 0x4000075: base -= 0x4000074; size = 2; data = core->spu.readGbaSoundCntX(2);      break; // SOUND2CNT_X
            case 0x4000078:
            case 0x4000079: base -= 0x4000078; size = 2; data = core->spu.readGbaSoundCntL(3);      break; // SOUND3CNT_L
            case 0x400007C:
            case 0x400007D: base -= 0x400007C; size = 2; data = core->spu.readGbaSoundCntX(3);      break; // SOUND3CNT_X
            case 0x4000080:
            case 0x4000081: base -= 0x4000080; size = 2; data = core->spu.readGbaMainSoundCntL();   break; // SOUNDCNT_L
            case 0x4000082:
            case 0x4000083: base -= 0x4000082; size = 2; data = core->spu.readGbaMainSoundCntH();   break; // SOUNDCNT_H
            case 0x4000084:
            case 0x4000085: base -= 0x4000084; size = 2; data = core->spu.readGbaMainSoundCntX();   break; // SOUNDCNT_X
            case 0x4000088:
            case 0x4000089: base -= 0x4000088; size = 2; data = core->spu.readGbaSoundBias();       break; // SOUNDBIAS
            case 0x4000090: base -= 0x4000090; size = 1; data = core->spu.readGbaWaveRam(0);        break; // WAVE_RAM
            case 0x4000091: base -= 0x4000091; size = 1; data = core->spu.readGbaWaveRam(1);        break; // WAVE_RAM
            case 0x4000092: base -= 0x4000092; size = 1; data = core->spu.readGbaWaveRam(2);        break; // WAVE_RAM
            case 0x4000093: base -= 0x4000093; size = 1; data = core->spu.readGbaWaveRam(3);        break; // WAVE_RAM
            case 0x4000094: base -= 0x4000094; size = 1; data = core->spu.readGbaWaveRam(4);        break; // WAVE_RAM
            case 0x4000095: base -= 0x4000095; size = 1; data = core->spu.readGbaWaveRam(5);        break; // WAVE_RAM
            case 0x4000096: base -= 0x4000096; size = 1; data = core->spu.readGbaWaveRam(6);        break; // WAVE_RAM
            case 0x4000097: base -= 0x4000097; size = 1; data = core->spu.readGbaWaveRam(7);        break; // WAVE_RAM
            case 0x4000098: base -= 0x4000098; size = 1; data = core->spu.readGbaWaveRam(8);        break; // WAVE_RAM
            case 0x4000099: base -= 0x4000099; size = 1; data = core->spu.readGbaWaveRam(9);        break; // WAVE_RAM
            case 0x400009A: base -= 0x400009A; size = 1; data = core->spu.readGbaWaveRam(10);       break; // WAVE_RAM
            case 0x400009B: base -= 0x400009B; size = 1; data = core->spu.readGbaWaveRam(11);       break; // WAVE_RAM
            case 0x400009C: base -= 0x400009C; size = 1; data = core->spu.readGbaWaveRam(12);       break; // WAVE_RAM
            case 0x400009D: base -= 0x400009D; size = 1; data = core->spu.readGbaWaveRam(13);       break; // WAVE_RAM
            case 0x400009E: base -= 0x400009E; size = 1; data = core->spu.readGbaWaveRam(14);       break; // WAVE_RAM
            case 0x400009F: base -= 0x400009F; size = 1; data = core->spu.readGbaWaveRam(15);       break; // WAVE_RAM
            case 0x40000B0:
            case 0x40000B1:
            case 0x40000B2:
            case 0x40000B3: base -= 0x40000B0; size = 4; data = core->dma[1].readDmaSad(0);         break; // DMA0SAD
            case 0x40000B4:
            case 0x40000B5:
            case 0x40000B6:
            case 0x40000B7: base -= 0x40000B4; size = 4; data = core->dma[1].readDmaDad(0);         break; // DMA0DAD
            case 0x40000B8:
            case 0x40000B9:
            case 0x40000BA:
            case 0x40000BB: base -= 0x40000B8; size = 4; data = core->dma[1].readDmaCnt(0);         break; // DMA0CNT
            case 0x40000BC:
            case 0x40000BD:
            case 0x40000BE:
            case 0x40000BF: base -= 0x40000BC; size = 4; data = core->dma[1].readDmaSad(1);         break; // DMA1SAD
            case 0x40000C0:
            case 0x40000C1:
            case 0x40000C2:
            case 0x40000C3: base -= 0x40000C0; size = 4; data = core->dma[1].readDmaDad(1);         break; // DMA1DAD
            case 0x40000C4:
            case 0x40000C5:
            case 0x40000C6:
            case 0x40000C7: base -= 0x40000C4; size = 4; data = core->dma[1].readDmaCnt(1);         break; // DMA1CNT
            case 0x40000C8:
            case 0x40000C9:
            case 0x40000CA:
            case 0x40000CB: base -= 0x40000C8; size = 4; data = core->dma[1].readDmaSad(2);         break; // DMA2SAD
            case 0x40000CC:
            case 0x40000CD:
            case 0x40000CE:
            case 0x40000CF: base -= 0x40000CC; size = 4; data = core->dma[1].readDmaDad(2);         break; // DMA2DAD
            case 0x40000D0:
            case 0x40000D1:
            case 0x40000D2:
            case 0x40000D3: base -= 0x40000D0; size = 4; data = core->dma[1].readDmaCnt(2);         break; // DMA2CNT
            case 0x40000D4:
            case 0x40000D5:
            case 0x40000D6:
            case 0x40000D7: base -= 0x40000D4; size = 4; data = core->dma[1].readDmaSad(3);         break; // DMA3SAD
            case 0x40000D8:
            case 0x40000D9:
            case 0x40000DA:
            case 0x40000DB: base -= 0x40000D8; size = 4; data = core->dma[1].readDmaDad(3);         break; // DMA3DAD
            case 0x40000DC:
            case 0x40000DD:
            case 0x40000DE:
            case 0x40000DF: base -= 0x40000DC; size = 4; data = core->dma[1].readDmaCnt(3);         break; // DMA3CNT
            case 0x4000100:
            case 0x4000101: base -= 0x4000100; size = 2; data = core->timers[1].readTmCntL(0);      break; // TM0CNT_L
            case 0x4000102:
            case 0x4000103: base -= 0x4000102; size = 2; data = core->timers[1].readTmCntH(0);      break; // TM0CNT_H
            case 0x4000104:
            case 0x4000105: base -= 0x4000104; size = 2; data = core->timers[1].readTmCntL(1);      break; // TM1CNT_L
            case 0x4000106:
            case 0x4000107: base -= 0x4000106; size = 2; data = core->timers[1].readTmCntH(1);      break; // TM1CNT_H
            case 0x4000108:
            case 0x4000109: base -= 0x4000108; size = 2; data = core->timers[1].readTmCntL(2);      break; // TM2CNT_L
            case 0x400010A:
            case 0x400010B: base -= 0x400010A; size = 2; data = core->timers[1].readTmCntH(2);      break; // TM2CNT_H
            case 0x400010C:
            case 0x400010D: base -= 0x400010C; size = 2; data = core->timers[1].readTmCntL(3);      break; // TM3CNT_L
            case 0x400010E:
            case 0x400010F: base -= 0x400010E; size = 2; data = core->timers[1].readTmCntH(3);      break; // TM3CNT_H
            case 0x4000130:
            case 0x4000131: base -= 0x4000130; size = 2; data = core->input.readKeyInput();         break; // KEYINPUT
            case 0x4000200:
            case 0x4000201: base -= 0x4000200; size = 2; data = core->interpreter[1].readIe();      break; // IE
            case 0x4000202:
            case 0x4000203: base -= 0x4000202; size = 2; data = core->interpreter[1].readIrf();     break; // IF
            case 0x4000208: base -= 0x4000208; size = 1; data = core->interpreter[1].readIme();     break; // IME
            case 0x4000300: base -= 0x4000300; size = 1; data = core->interpreter[1].readPostFlg(); break; // POSTFLG

            default:
            {
                // Handle unknown reads by returning 0
                if (i == 0)
                {
                    printf("Unknown GBA I/O register read: 0x%X\n", address);
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
            case 0x4000003: base -= 0x4000000; size = 4; core->gpu2D[0].writeDispCnt(mask << (base * 8), data << (base * 8));            break; // DISPCNT (engine A)
            case 0x4000004:
            case 0x4000005: base -= 0x4000004; size = 2; core->gpu.writeDispStat(0, mask << (base * 8), data << (base * 8));             break; // DISPSTAT (ARM9)
            case 0x4000008:
            case 0x4000009: base -= 0x4000008; size = 2; core->gpu2D[0].writeBgCnt(0, mask << (base * 8), data << (base * 8));           break; // BG0CNT (engine A)
            case 0x400000A:
            case 0x400000B: base -= 0x400000A; size = 2; core->gpu2D[0].writeBgCnt(1, mask << (base * 8), data << (base * 8));           break; // BG1CNT (engine A)
            case 0x400000C:
            case 0x400000D: base -= 0x400000C; size = 2; core->gpu2D[0].writeBgCnt(2, mask << (base * 8), data << (base * 8));           break; // BG2CNT (engine A)
            case 0x400000E:
            case 0x400000F: base -= 0x400000E; size = 2; core->gpu2D[0].writeBgCnt(3, mask << (base * 8), data << (base * 8));           break; // BG3CNT (engine A)
            case 0x4000010:
            case 0x4000011: base -= 0x4000010; size = 2; core->gpu2D[0].writeBgHOfs(0, mask << (base * 8), data << (base * 8));          break; // BG0HOFS (engine A)
            case 0x4000012:
            case 0x4000013: base -= 0x4000012; size = 2; core->gpu2D[0].writeBgVOfs(0, mask << (base * 8), data << (base * 8));          break; // BG0VOFS (engine A)
            case 0x4000014:
            case 0x4000015: base -= 0x4000014; size = 2; core->gpu2D[0].writeBgHOfs(1, mask << (base * 8), data << (base * 8));          break; // BG1HOFS (engine A)
            case 0x4000016:
            case 0x4000017: base -= 0x4000016; size = 2; core->gpu2D[0].writeBgVOfs(1, mask << (base * 8), data << (base * 8));          break; // BG1VOFS (engine A)
            case 0x4000018:
            case 0x4000019: base -= 0x4000018; size = 2; core->gpu2D[0].writeBgHOfs(2, mask << (base * 8), data << (base * 8));          break; // BG2HOFS (engine A)
            case 0x400001A:
            case 0x400001B: base -= 0x400001A; size = 2; core->gpu2D[0].writeBgVOfs(2, mask << (base * 8), data << (base * 8));          break; // BG2VOFS (engine A)
            case 0x400001C:
            case 0x400001D: base -= 0x400001C; size = 2; core->gpu2D[0].writeBgHOfs(3, mask << (base * 8), data << (base * 8));          break; // BG3HOFS (engine A)
            case 0x400001E:
            case 0x400001F: base -= 0x400001E; size = 2; core->gpu2D[0].writeBgVOfs(3, mask << (base * 8), data << (base * 8));          break; // BG3VOFS (engine A)
            case 0x4000020:
            case 0x4000021: base -= 0x4000020; size = 2; core->gpu2D[0].writeBgPA(2, mask << (base * 8), data << (base * 8));            break; // BG2PA (engine A)
            case 0x4000022:
            case 0x4000023: base -= 0x4000022; size = 2; core->gpu2D[0].writeBgPB(2, mask << (base * 8), data << (base * 8));            break; // BG2PB (engine A)
            case 0x4000024:
            case 0x4000025: base -= 0x4000024; size = 2; core->gpu2D[0].writeBgPC(2, mask << (base * 8), data << (base * 8));            break; // BG2PC (engine A)
            case 0x4000026:
            case 0x4000027: base -= 0x4000026; size = 2; core->gpu2D[0].writeBgPD(2, mask << (base * 8), data << (base * 8));            break; // BG2PD (engine A)
            case 0x4000028:
            case 0x4000029:
            case 0x400002A:
            case 0x400002B: base -= 0x4000028; size = 4; core->gpu2D[0].writeBgX(2, mask << (base * 8), data << (base * 8));             break; // BG2X (engine A)
            case 0x400002C:
            case 0x400002D:
            case 0x400002E:
            case 0x400002F: base -= 0x400002C; size = 4; core->gpu2D[0].writeBgY(2, mask << (base * 8), data << (base * 8));             break; // BG2Y (engine A)
            case 0x4000030:
            case 0x4000031: base -= 0x4000030; size = 2; core->gpu2D[0].writeBgPA(3, mask << (base * 8), data << (base * 8));            break; // BG3PA (engine A)
            case 0x4000032:
            case 0x4000033: base -= 0x4000032; size = 2; core->gpu2D[0].writeBgPB(3, mask << (base * 8), data << (base * 8));            break; // BG3PB (engine A)
            case 0x4000034:
            case 0x4000035: base -= 0x4000034; size = 2; core->gpu2D[0].writeBgPC(3, mask << (base * 8), data << (base * 8));            break; // BG3PC (engine A)
            case 0x4000036:
            case 0x4000037: base -= 0x4000036; size = 2; core->gpu2D[0].writeBgPD(3, mask << (base * 8), data << (base * 8));            break; // BG3PD (engine A)
            case 0x4000038:
            case 0x4000039:
            case 0x400003A:
            case 0x400003B: base -= 0x4000038; size = 4; core->gpu2D[0].writeBgX(3, mask << (base * 8), data << (base * 8));             break; // BG3X (engine A)
            case 0x400003C:
            case 0x400003D:
            case 0x400003E:
            case 0x400003F: base -= 0x400003C; size = 4; core->gpu2D[0].writeBgY(3, mask << (base * 8), data << (base * 8));             break; // BG3Y (engine A)
            case 0x4000040:
            case 0x4000041: base -= 0x4000040; size = 2; core->gpu2D[0].writeWinH(0, mask << (base * 8), data << (base * 8));            break; // WIN0H (engine A)
            case 0x4000042:
            case 0x4000043: base -= 0x4000042; size = 2; core->gpu2D[0].writeWinH(1, mask << (base * 8), data << (base * 8));            break; // WIN1H (engine A)
            case 0x4000044:
            case 0x4000045: base -= 0x4000044; size = 2; core->gpu2D[0].writeWinV(0, mask << (base * 8), data << (base * 8));            break; // WIN0V (engine A)
            case 0x4000046:
            case 0x4000047: base -= 0x4000046; size = 2; core->gpu2D[0].writeWinV(1, mask << (base * 8), data << (base * 8));            break; // WIN1V (engine A)
            case 0x4000048:
            case 0x4000049: base -= 0x4000048; size = 2; core->gpu2D[0].writeWinIn(mask << (base * 8), data << (base * 8));              break; // WININ (engine A)
            case 0x400004A:
            case 0x400004B: base -= 0x400004A; size = 2; core->gpu2D[0].writeWinOut(mask << (base * 8), data << (base * 8));             break; // WINOUT (engine A)
            case 0x4000050:
            case 0x4000051: base -= 0x4000050; size = 2; core->gpu2D[0].writeBldCnt(mask << (base * 8), data << (base * 8));             break; // BLDCNT (engine A)
            case 0x4000052:
            case 0x4000053: base -= 0x4000052; size = 2; core->gpu2D[0].writeBldAlpha(mask << (base * 8), data << (base * 8));           break; // BLDALPHA (engine A)
            case 0x4000054: base -= 0x4000054; size = 1; core->gpu2D[0].writeBldY(data << (base * 8));                                   break; // BLDY (engine A)
            case 0x4000060:
            case 0x4000061: base -= 0x4000060; size = 2; core->gpu3DRenderer.writeDisp3DCnt(mask << (base * 8), data << (base * 8));     break; // DISP3DCNT
            case 0x4000064:
            case 0x4000065:
            case 0x4000066:
            case 0x4000067: base -= 0x4000064; size = 4; core->gpu.writeDispCapCnt(mask << (base * 8), data << (base * 8));              break; // DISPCAPCNT
            case 0x400006C:
            case 0x400006D: base -= 0x400006C; size = 2; core->gpu2D[0].writeMasterBright(mask << (base * 8), data << (base * 8));       break; // MASTER_BRIGHT (engine A)
            case 0x40000B0:
            case 0x40000B1:
            case 0x40000B2:
            case 0x40000B3: base -= 0x40000B0; size = 4; core->dma[0].writeDmaSad(0, mask << (base * 8), data << (base * 8));            break; // DMA0SAD (ARM9)
            case 0x40000B4:
            case 0x40000B5:
            case 0x40000B6:
            case 0x40000B7: base -= 0x40000B4; size = 4; core->dma[0].writeDmaDad(0, mask << (base * 8), data << (base * 8));            break; // DMA0DAD (ARM9)
            case 0x40000B8:
            case 0x40000B9:
            case 0x40000BA:
            case 0x40000BB: base -= 0x40000B8; size = 4; core->dma[0].writeDmaCnt(0, mask << (base * 8), data << (base * 8));            break; // DMA0CNT (ARM9)
            case 0x40000BC:
            case 0x40000BD:
            case 0x40000BE:
            case 0x40000BF: base -= 0x40000BC; size = 4; core->dma[0].writeDmaSad(1, mask << (base * 8), data << (base * 8));            break; // DMA1SAD (ARM9)
            case 0x40000C0:
            case 0x40000C1:
            case 0x40000C2:
            case 0x40000C3: base -= 0x40000C0; size = 4; core->dma[0].writeDmaDad(1, mask << (base * 8), data << (base * 8));            break; // DMA1DAD (ARM9)
            case 0x40000C4:
            case 0x40000C5:
            case 0x40000C6:
            case 0x40000C7: base -= 0x40000C4; size = 4; core->dma[0].writeDmaCnt(1, mask << (base * 8), data << (base * 8));            break; // DMA1CNT (ARM9)
            case 0x40000C8:
            case 0x40000C9:
            case 0x40000CA:
            case 0x40000CB: base -= 0x40000C8; size = 4; core->dma[0].writeDmaSad(2, mask << (base * 8), data << (base * 8));            break; // DMA2SAD (ARM9)
            case 0x40000CC:
            case 0x40000CD:
            case 0x40000CE:
            case 0x40000CF: base -= 0x40000CC; size = 4; core->dma[0].writeDmaDad(2, mask << (base * 8), data << (base * 8));            break; // DMA2DAD (ARM9)
            case 0x40000D0:
            case 0x40000D1:
            case 0x40000D2:
            case 0x40000D3: base -= 0x40000D0; size = 4; core->dma[0].writeDmaCnt(2, mask << (base * 8), data << (base * 8));            break; // DMA2CNT (ARM9)
            case 0x40000D4:
            case 0x40000D5:
            case 0x40000D6:
            case 0x40000D7: base -= 0x40000D4; size = 4; core->dma[0].writeDmaSad(3, mask << (base * 8), data << (base * 8));            break; // DMA3SAD (ARM9)
            case 0x40000D8:
            case 0x40000D9:
            case 0x40000DA:
            case 0x40000DB: base -= 0x40000D8; size = 4; core->dma[0].writeDmaDad(3, mask << (base * 8), data << (base * 8));            break; // DMA3DAD (ARM9)
            case 0x40000DC:
            case 0x40000DD:
            case 0x40000DE:
            case 0x40000DF: base -= 0x40000DC; size = 4; core->dma[0].writeDmaCnt(3, mask << (base * 8), data << (base * 8));            break; // DMA3CNT (ARM9)
            case 0x40000E0:
            case 0x40000E1:
            case 0x40000E2:
            case 0x40000E3: base -= 0x40000E0; size = 4; writeDmaFill(0, mask << (base * 8), data << (base * 8));                        break; // DMA0FILL
            case 0x40000E4:
            case 0x40000E5:
            case 0x40000E6:
            case 0x40000E7: base -= 0x40000E4; size = 4; writeDmaFill(1, mask << (base * 8), data << (base * 8));                        break; // DMA1FILL
            case 0x40000E8:
            case 0x40000E9:
            case 0x40000EA:
            case 0x40000EB: base -= 0x40000E8; size = 4; writeDmaFill(2, mask << (base * 8), data << (base * 8));                        break; // DMA2FILL
            case 0x40000EC:
            case 0x40000ED:
            case 0x40000EE:
            case 0x40000EF: base -= 0x40000EC; size = 4; writeDmaFill(3, mask << (base * 8), data << (base * 8));                        break; // DMA3FILL
            case 0x4000100:
            case 0x4000101: base -= 0x4000100; size = 2; core->timers[0].writeTmCntL(0, mask << (base * 8), data << (base * 8));         break; // TM0CNT_L (ARM9)
            case 0x4000102:
            case 0x4000103: base -= 0x4000102; size = 2; core->timers[0].writeTmCntH(0, mask << (base * 8), data << (base * 8));         break; // TM0CNT_H (ARM9)
            case 0x4000104:
            case 0x4000105: base -= 0x4000104; size = 2; core->timers[0].writeTmCntL(1, mask << (base * 8), data << (base * 8));         break; // TM1CNT_L (ARM9)
            case 0x4000106:
            case 0x4000107: base -= 0x4000106; size = 2; core->timers[0].writeTmCntH(1, mask << (base * 8), data << (base * 8));         break; // TM1CNT_H (ARM9)
            case 0x4000108:
            case 0x4000109: base -= 0x4000108; size = 2; core->timers[0].writeTmCntL(2, mask << (base * 8), data << (base * 8));         break; // TM2CNT_L (ARM9)
            case 0x400010A:
            case 0x400010B: base -= 0x400010A; size = 2; core->timers[0].writeTmCntH(2, mask << (base * 8), data << (base * 8));         break; // TM2CNT_H (ARM9)
            case 0x400010C:
            case 0x400010D: base -= 0x400010C; size = 2; core->timers[0].writeTmCntL(3, mask << (base * 8), data << (base * 8));         break; // TM3CNT_L (ARM9)
            case 0x400010E:
            case 0x400010F: base -= 0x400010E; size = 2; core->timers[0].writeTmCntH(3, mask << (base * 8), data << (base * 8));         break; // TM3CNT_H (ARM9)
            case 0x4000180:
            case 0x4000181: base -= 0x4000180; size = 2; core->ipc.writeIpcSync(0, mask << (base * 8), data << (base * 8));              break; // IPCSYNC (ARM9)
            case 0x4000184:
            case 0x4000185: base -= 0x4000184; size = 2; core->ipc.writeIpcFifoCnt(0, mask << (base * 8), data << (base * 8));           break; // IPCFIFOCNT (ARM9)
            case 0x4000188:
            case 0x4000189:
            case 0x400018A:
            case 0x400018B: base -= 0x4000188; size = 4; core->ipc.writeIpcFifoSend(0, mask << (base * 8), data << (base * 8));          break; // IPCFIFOSEND (ARM9)
            case 0x40001A0:
            case 0x40001A1: base -= 0x40001A0; size = 2; core->cartridge.writeAuxSpiCnt(0, mask << (base * 8), data << (base * 8));      break; // AUXSPICNT (ARM9)
            case 0x40001A2: base -= 0x40001A2; size = 1; core->cartridge.writeAuxSpiData(0, data << (base * 8));                         break; // AUXSPIDATA (ARM9)
            case 0x40001A4:
            case 0x40001A5:
            case 0x40001A6:
            case 0x40001A7: base -= 0x40001A4; size = 4; core->cartridge.writeRomCtrl(0, mask << (base * 8), data << (base * 8));        break; // ROMCTRL (ARM9)
            case 0x40001A8:
            case 0x40001A9:
            case 0x40001AA:
            case 0x40001AB: base -= 0x40001A8; size = 4; core->cartridge.writeRomCmdOutL(0, mask << (base * 8), data << (base * 8));     break; // ROMCMDOUT_L (ARM9)
            case 0x40001AC:
            case 0x40001AD:
            case 0x40001AE:
            case 0x40001AF: base -= 0x40001AC; size = 4; core->cartridge.writeRomCmdOutH(0, mask << (base * 8), data << (base * 8));     break; // ROMCMDOUT_H (ARM9)
            case 0x4000208: base -= 0x4000208; size = 1; core->interpreter[0].writeIme(data << (base * 8));                              break; // IME (ARM9)
            case 0x4000210:
            case 0x4000211:
            case 0x4000212:
            case 0x4000213: base -= 0x4000210; size = 4; core->interpreter[0].writeIe(mask << (base * 8), data << (base * 8));           break; // IE (ARM9)
            case 0x4000214:
            case 0x4000215:
            case 0x4000216:
            case 0x4000217: base -= 0x4000214; size = 4; core->interpreter[0].writeIrf(mask << (base * 8), data << (base * 8));          break; // IF (ARM9)
            case 0x4000240: base -= 0x4000240; size = 1; writeVramCnt(0, data << (base * 8));                                            break; // VRAMCNT_A
            case 0x4000241: base -= 0x4000241; size = 1; writeVramCnt(1, data << (base * 8));                                            break; // VRAMCNT_B
            case 0x4000242: base -= 0x4000242; size = 1; writeVramCnt(2, data << (base * 8));                                            break; // VRAMCNT_C
            case 0x4000243: base -= 0x4000243; size = 1; writeVramCnt(3, data << (base * 8));                                            break; // VRAMCNT_D
            case 0x4000244: base -= 0x4000244; size = 1; writeVramCnt(4, data << (base * 8));                                            break; // VRAMCNT_E
            case 0x4000245: base -= 0x4000245; size = 1; writeVramCnt(5, data << (base * 8));                                            break; // VRAMCNT_F
            case 0x4000246: base -= 0x4000246; size = 1; writeVramCnt(6, data << (base * 8));                                            break; // VRAMCNT_G
            case 0x4000247: base -= 0x4000247; size = 1; writeWramCnt(data << (base * 8));                                               break; // WRAMCNT
            case 0x4000248: base -= 0x4000248; size = 1; writeVramCnt(7, data << (base * 8));                                            break; // VRAMCNT_H
            case 0x4000249: base -= 0x4000249; size = 1; writeVramCnt(8, data << (base * 8));                                            break; // VRAMCNT_I
            case 0x4000280:
            case 0x4000281: base -= 0x4000280; size = 2; core->divSqrt.writeDivCnt(mask << (base * 8), data << (base * 8));              break; // DIVCNT
            case 0x4000290:
            case 0x4000291:
            case 0x4000292:
            case 0x4000293: base -= 0x4000290; size = 4; core->divSqrt.writeDivNumerL(mask << (base * 8), data << (base * 8));           break; // DIVNUMER_L
            case 0x4000294:
            case 0x4000295:
            case 0x4000296:
            case 0x4000297: base -= 0x4000294; size = 4; core->divSqrt.writeDivNumerH(mask << (base * 8), data << (base * 8));           break; // DIVNUMER_H
            case 0x4000298:
            case 0x4000299:
            case 0x400029A:
            case 0x400029B: base -= 0x4000298; size = 4; core->divSqrt.writeDivDenomL(mask << (base * 8), data << (base * 8));           break; // DIVDENOM_L
            case 0x400029C:
            case 0x400029D:
            case 0x400029E:
            case 0x400029F: base -= 0x400029C; size = 4; core->divSqrt.writeDivDenomH(mask << (base * 8), data << (base * 8));           break; // DIVDENOM_H
            case 0x40002B0:
            case 0x40002B1: base -= 0x40002B0; size = 2; core->divSqrt.writeSqrtCnt(mask << (base * 8), data << (base * 8));             break; // SQRTCNT
            case 0x40002B8:
            case 0x40002B9:
            case 0x40002BA:
            case 0x40002BB: base -= 0x40002B8; size = 4; core->divSqrt.writeSqrtParamL(mask << (base * 8), data << (base * 8));          break; // SQRTPARAM_L
            case 0x40002BC:
            case 0x40002BD:
            case 0x40002BE:
            case 0x40002BF: base -= 0x40002BC; size = 4; core->divSqrt.writeSqrtParamH(mask << (base * 8), data << (base * 8));          break; // SQRTPARAM_H
            case 0x4000300: base -= 0x4000300; size = 1; core->interpreter[0].writePostFlg(data << (base * 8));                          break; // POSTFLG (ARM9)
            case 0x4000304:
            case 0x4000305: base -= 0x4000304; size = 2; core->gpu.writePowCnt1(mask << (base * 8), data << (base * 8));                 break; // POWCNT1
            case 0x4000350:
            case 0x4000351:
            case 0x4000352:
            case 0x4000353: base -= 0x4000350; size = 4; core->gpu3DRenderer.writeClearColor(mask << (base * 8), data << (base * 8));    break; // CLEAR_COLOR
            case 0x4000354:
            case 0x4000355: base -= 0x4000354; size = 2; core->gpu3DRenderer.writeClearDepth(mask << (base * 8), data << (base * 8));    break; // CLEAR_DEPTH
            case 0x4000358:
            case 0x4000359:
            case 0x400035A:
            case 0x400035B: base -= 0x4000358; size = 4; core->gpu3DRenderer.writeFogColor(mask << (base * 8), data << (base * 8));      break; // FOG_COLOR
            case 0x400035C:
            case 0x400035D: base -= 0x400035C; size = 2; core->gpu3DRenderer.writeFogOffset(mask << (base * 8), data << (base * 8));     break; // FOG_OFFSET
            case 0x4000360: base -= 0x4000360; size = 1; core->gpu3DRenderer.writeFogTable(0,  data << (base * 8));                      break; // FOG_TABLE
            case 0x4000361: base -= 0x4000361; size = 1; core->gpu3DRenderer.writeFogTable(1,  data << (base * 8));                      break; // FOG_TABLE
            case 0x4000362: base -= 0x4000362; size = 1; core->gpu3DRenderer.writeFogTable(2,  data << (base * 8));                      break; // FOG_TABLE
            case 0x4000363: base -= 0x4000363; size = 1; core->gpu3DRenderer.writeFogTable(3,  data << (base * 8));                      break; // FOG_TABLE
            case 0x4000364: base -= 0x4000364; size = 1; core->gpu3DRenderer.writeFogTable(4,  data << (base * 8));                      break; // FOG_TABLE
            case 0x4000365: base -= 0x4000365; size = 1; core->gpu3DRenderer.writeFogTable(5,  data << (base * 8));                      break; // FOG_TABLE
            case 0x4000366: base -= 0x4000366; size = 1; core->gpu3DRenderer.writeFogTable(6,  data << (base * 8));                      break; // FOG_TABLE
            case 0x4000367: base -= 0x4000367; size = 1; core->gpu3DRenderer.writeFogTable(7,  data << (base * 8));                      break; // FOG_TABLE
            case 0x4000368: base -= 0x4000368; size = 1; core->gpu3DRenderer.writeFogTable(8,  data << (base * 8));                      break; // FOG_TABLE
            case 0x4000369: base -= 0x4000369; size = 1; core->gpu3DRenderer.writeFogTable(9,  data << (base * 8));                      break; // FOG_TABLE
            case 0x400036A: base -= 0x400036A; size = 1; core->gpu3DRenderer.writeFogTable(10, data << (base * 8));                      break; // FOG_TABLE
            case 0x400036B: base -= 0x400036B; size = 1; core->gpu3DRenderer.writeFogTable(11, data << (base * 8));                      break; // FOG_TABLE
            case 0x400036C: base -= 0x400036C; size = 1; core->gpu3DRenderer.writeFogTable(12, data << (base * 8));                      break; // FOG_TABLE
            case 0x400036D: base -= 0x400036D; size = 1; core->gpu3DRenderer.writeFogTable(13, data << (base * 8));                      break; // FOG_TABLE
            case 0x400036E: base -= 0x400036E; size = 1; core->gpu3DRenderer.writeFogTable(14, data << (base * 8));                      break; // FOG_TABLE
            case 0x400036F: base -= 0x400036F; size = 1; core->gpu3DRenderer.writeFogTable(15, data << (base * 8));                      break; // FOG_TABLE
            case 0x4000370: base -= 0x4000370; size = 1; core->gpu3DRenderer.writeFogTable(16, data << (base * 8));                      break; // FOG_TABLE
            case 0x4000371: base -= 0x4000371; size = 1; core->gpu3DRenderer.writeFogTable(17, data << (base * 8));                      break; // FOG_TABLE
            case 0x4000372: base -= 0x4000372; size = 1; core->gpu3DRenderer.writeFogTable(18, data << (base * 8));                      break; // FOG_TABLE
            case 0x4000373: base -= 0x4000373; size = 1; core->gpu3DRenderer.writeFogTable(19, data << (base * 8));                      break; // FOG_TABLE
            case 0x4000374: base -= 0x4000374; size = 1; core->gpu3DRenderer.writeFogTable(20, data << (base * 8));                      break; // FOG_TABLE
            case 0x4000375: base -= 0x4000375; size = 1; core->gpu3DRenderer.writeFogTable(21, data << (base * 8));                      break; // FOG_TABLE
            case 0x4000376: base -= 0x4000376; size = 1; core->gpu3DRenderer.writeFogTable(22, data << (base * 8));                      break; // FOG_TABLE
            case 0x4000377: base -= 0x4000377; size = 1; core->gpu3DRenderer.writeFogTable(23, data << (base * 8));                      break; // FOG_TABLE
            case 0x4000378: base -= 0x4000378; size = 1; core->gpu3DRenderer.writeFogTable(24, data << (base * 8));                      break; // FOG_TABLE
            case 0x4000379: base -= 0x4000379; size = 1; core->gpu3DRenderer.writeFogTable(25, data << (base * 8));                      break; // FOG_TABLE
            case 0x400037A: base -= 0x400037A; size = 1; core->gpu3DRenderer.writeFogTable(26, data << (base * 8));                      break; // FOG_TABLE
            case 0x400037B: base -= 0x400037B; size = 1; core->gpu3DRenderer.writeFogTable(27, data << (base * 8));                      break; // FOG_TABLE
            case 0x400037C: base -= 0x400037C; size = 1; core->gpu3DRenderer.writeFogTable(28, data << (base * 8));                      break; // FOG_TABLE
            case 0x400037D: base -= 0x400037D; size = 1; core->gpu3DRenderer.writeFogTable(29, data << (base * 8));                      break; // FOG_TABLE
            case 0x400037E: base -= 0x400037E; size = 1; core->gpu3DRenderer.writeFogTable(30, data << (base * 8));                      break; // FOG_TABLE
            case 0x400037F: base -= 0x400037F; size = 1; core->gpu3DRenderer.writeFogTable(31, data << (base * 8));                      break; // FOG_TABLE
            case 0x4000380:
            case 0x4000381: base -= 0x4000380; size = 2; core->gpu3DRenderer.writeToonTable(0,  mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x4000382:
            case 0x4000383: base -= 0x4000382; size = 2; core->gpu3DRenderer.writeToonTable(1,  mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x4000384:
            case 0x4000385: base -= 0x4000384; size = 2; core->gpu3DRenderer.writeToonTable(2,  mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x4000386:
            case 0x4000387: base -= 0x4000386; size = 2; core->gpu3DRenderer.writeToonTable(3,  mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x4000388:
            case 0x4000389: base -= 0x4000388; size = 2; core->gpu3DRenderer.writeToonTable(4,  mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x400038A:
            case 0x400038B: base -= 0x400038A; size = 2; core->gpu3DRenderer.writeToonTable(5,  mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x400038C:
            case 0x400038D: base -= 0x400038C; size = 2; core->gpu3DRenderer.writeToonTable(6,  mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x400038E:
            case 0x400038F: base -= 0x400038E; size = 2; core->gpu3DRenderer.writeToonTable(7,  mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x4000390:
            case 0x4000391: base -= 0x4000390; size = 2; core->gpu3DRenderer.writeToonTable(8,  mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x4000392:
            case 0x4000393: base -= 0x4000392; size = 2; core->gpu3DRenderer.writeToonTable(9,  mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x4000394:
            case 0x4000395: base -= 0x4000394; size = 2; core->gpu3DRenderer.writeToonTable(10, mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x4000396:
            case 0x4000397: base -= 0x4000396; size = 2; core->gpu3DRenderer.writeToonTable(11, mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x4000398:
            case 0x4000399: base -= 0x4000398; size = 2; core->gpu3DRenderer.writeToonTable(12, mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x400039A:
            case 0x400039B: base -= 0x400039A; size = 2; core->gpu3DRenderer.writeToonTable(13, mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x400039C:
            case 0x400039D: base -= 0x400039C; size = 2; core->gpu3DRenderer.writeToonTable(14, mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x400039E:
            case 0x400039F: base -= 0x400039E; size = 2; core->gpu3DRenderer.writeToonTable(15, mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x40003A0:
            case 0x40003A1: base -= 0x40003A0; size = 2; core->gpu3DRenderer.writeToonTable(16, mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x40003A2:
            case 0x40003A3: base -= 0x40003A2; size = 2; core->gpu3DRenderer.writeToonTable(17, mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x40003A4:
            case 0x40003A5: base -= 0x40003A4; size = 2; core->gpu3DRenderer.writeToonTable(18, mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x40003A6:
            case 0x40003A7: base -= 0x40003A6; size = 2; core->gpu3DRenderer.writeToonTable(19, mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x40003A8:
            case 0x40003A9: base -= 0x40003A8; size = 2; core->gpu3DRenderer.writeToonTable(20, mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x40003AA:
            case 0x40003AB: base -= 0x40003AA; size = 2; core->gpu3DRenderer.writeToonTable(21, mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x40003AC:
            case 0x40003AD: base -= 0x40003AC; size = 2; core->gpu3DRenderer.writeToonTable(22, mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x40003AE:
            case 0x40003AF: base -= 0x40003AE; size = 2; core->gpu3DRenderer.writeToonTable(23, mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x40003B0:
            case 0x40003B1: base -= 0x40003B0; size = 2; core->gpu3DRenderer.writeToonTable(24, mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x40003B2:
            case 0x40003B3: base -= 0x40003B2; size = 2; core->gpu3DRenderer.writeToonTable(25, mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x40003B4:
            case 0x40003B5: base -= 0x40003B4; size = 2; core->gpu3DRenderer.writeToonTable(26, mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x40003B6:
            case 0x40003B7: base -= 0x40003B6; size = 2; core->gpu3DRenderer.writeToonTable(27, mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x40003B8:
            case 0x40003B9: base -= 0x40003B8; size = 2; core->gpu3DRenderer.writeToonTable(28, mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x40003BA:
            case 0x40003BB: base -= 0x40003BA; size = 2; core->gpu3DRenderer.writeToonTable(29, mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x40003BC:
            case 0x40003BD: base -= 0x40003BC; size = 2; core->gpu3DRenderer.writeToonTable(30, mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x40003BE:
            case 0x40003BF: base -= 0x40003BE; size = 2; core->gpu3DRenderer.writeToonTable(31, mask << (base * 8), data << (base * 8)); break; // TOON_TABLE
            case 0x4000400:
            case 0x4000401:
            case 0x4000402:
            case 0x4000403: base -= 0x4000400; size = 4; core->gpu3D.writeGxFifo(mask << (base * 8), data << (base * 8));                break; // GXFIFO
            case 0x4000404:
            case 0x4000405:
            case 0x4000406:
            case 0x4000407: base -= 0x4000404; size = 4; core->gpu3D.writeGxFifo(mask << (base * 8), data << (base * 8));                break; // GXFIFO
            case 0x4000408:
            case 0x4000409:
            case 0x400040A:
            case 0x400040B: base -= 0x4000408; size = 4; core->gpu3D.writeGxFifo(mask << (base * 8), data << (base * 8));                break; // GXFIFO
            case 0x400040C:
            case 0x400040D:
            case 0x400040E:
            case 0x400040F: base -= 0x400040C; size = 4; core->gpu3D.writeGxFifo(mask << (base * 8), data << (base * 8));                break; // GXFIFO
            case 0x4000410:
            case 0x4000411:
            case 0x4000412:
            case 0x4000413: base -= 0x4000410; size = 4; core->gpu3D.writeGxFifo(mask << (base * 8), data << (base * 8));                break; // GXFIFO
            case 0x4000414:
            case 0x4000415:
            case 0x4000416:
            case 0x4000417: base -= 0x4000414; size = 4; core->gpu3D.writeGxFifo(mask << (base * 8), data << (base * 8));                break; // GXFIFO
            case 0x4000418:
            case 0x4000419:
            case 0x400041A:
            case 0x400041B: base -= 0x4000418; size = 4; core->gpu3D.writeGxFifo(mask << (base * 8), data << (base * 8));                break; // GXFIFO
            case 0x400041C:
            case 0x400041D:
            case 0x400041E:
            case 0x400041F: base -= 0x400041C; size = 4; core->gpu3D.writeGxFifo(mask << (base * 8), data << (base * 8));                break; // GXFIFO
            case 0x4000420:
            case 0x4000421:
            case 0x4000422:
            case 0x4000423: base -= 0x4000420; size = 4; core->gpu3D.writeGxFifo(mask << (base * 8), data << (base * 8));                break; // GXFIFO
            case 0x4000424:
            case 0x4000425:
            case 0x4000426:
            case 0x4000427: base -= 0x4000424; size = 4; core->gpu3D.writeGxFifo(mask << (base * 8), data << (base * 8));                break; // GXFIFO
            case 0x4000428:
            case 0x4000429:
            case 0x400042A:
            case 0x400042B: base -= 0x4000428; size = 4; core->gpu3D.writeGxFifo(mask << (base * 8), data << (base * 8));                break; // GXFIFO
            case 0x400042C:
            case 0x400042D:
            case 0x400042E:
            case 0x400042F: base -= 0x400042C; size = 4; core->gpu3D.writeGxFifo(mask << (base * 8), data << (base * 8));                break; // GXFIFO
            case 0x4000430:
            case 0x4000431:
            case 0x4000432:
            case 0x4000433: base -= 0x4000430; size = 4; core->gpu3D.writeGxFifo(mask << (base * 8), data << (base * 8));                break; // GXFIFO
            case 0x4000434:
            case 0x4000435:
            case 0x4000436:
            case 0x4000437: base -= 0x4000434; size = 4; core->gpu3D.writeGxFifo(mask << (base * 8), data << (base * 8));                break; // GXFIFO
            case 0x4000438:
            case 0x4000439:
            case 0x400043A:
            case 0x400043B: base -= 0x4000438; size = 4; core->gpu3D.writeGxFifo(mask << (base * 8), data << (base * 8));                break; // GXFIFO
            case 0x400043C:
            case 0x400043D:
            case 0x400043E:
            case 0x400043F: base -= 0x400043C; size = 4; core->gpu3D.writeGxFifo(mask << (base * 8), data << (base * 8));                break; // GXFIFO
            case 0x4000440:
            case 0x4000441:
            case 0x4000442:
            case 0x4000443: base -= 0x4000440; size = 4; core->gpu3D.writeMtxMode(mask << (base * 8), data << (base * 8));               break; // MTX_MODE
            case 0x4000444:
            case 0x4000445:
            case 0x4000446:
            case 0x4000447: base -= 0x4000444; size = 4; core->gpu3D.writeMtxPush(mask << (base * 8), data << (base * 8));               break; // MTX_PUSH
            case 0x4000448:
            case 0x4000449:
            case 0x400044A:
            case 0x400044B: base -= 0x4000448; size = 4; core->gpu3D.writeMtxPop(mask << (base * 8), data << (base * 8));                break; // MTX_POP
            case 0x400044C:
            case 0x400044D:
            case 0x400044E:
            case 0x400044F: base -= 0x400044C; size = 4; core->gpu3D.writeMtxStore(mask << (base * 8), data << (base * 8));              break; // MTX_STORE
            case 0x4000450:
            case 0x4000451:
            case 0x4000452:
            case 0x4000453: base -= 0x4000450; size = 4; core->gpu3D.writeMtxRestore(mask << (base * 8), data << (base * 8));            break; // MTX_RESTORE
            case 0x4000454:
            case 0x4000455:
            case 0x4000456:
            case 0x4000457: base -= 0x4000454; size = 4; core->gpu3D.writeMtxIdentity(mask << (base * 8), data << (base * 8));           break; // MTX_IDENTITY
            case 0x4000458:
            case 0x4000459:
            case 0x400045A:
            case 0x400045B: base -= 0x4000458; size = 4; core->gpu3D.writeMtxLoad44(mask << (base * 8), data << (base * 8));             break; // MTX_LOAD_4x4
            case 0x400045C:
            case 0x400045D:
            case 0x400045E:
            case 0x400045F: base -= 0x400045C; size = 4; core->gpu3D.writeMtxLoad43(mask << (base * 8), data << (base * 8));             break; // MTX_LOAD_4x3
            case 0x4000460:
            case 0x4000461:
            case 0x4000462:
            case 0x4000463: base -= 0x4000460; size = 4; core->gpu3D.writeMtxMult44(mask << (base * 8), data << (base * 8));             break; // MTX_MULT_4x4
            case 0x4000464:
            case 0x4000465:
            case 0x4000466:
            case 0x4000467: base -= 0x4000464; size = 4; core->gpu3D.writeMtxMult43(mask << (base * 8), data << (base * 8));             break; // MTX_MULT_4x3
            case 0x4000468:
            case 0x4000469:
            case 0x400046A:
            case 0x400046B: base -= 0x4000468; size = 4; core->gpu3D.writeMtxMult33(mask << (base * 8), data << (base * 8));             break; // MTX_MULT_3x3
            case 0x400046C:
            case 0x400046D:
            case 0x400046E:
            case 0x400046F: base -= 0x400046C; size = 4; core->gpu3D.writeMtxScale(mask << (base * 8), data << (base * 8));              break; // MTX_SCALE
            case 0x4000470:
            case 0x4000471:
            case 0x4000472:
            case 0x4000473: base -= 0x4000470; size = 4; core->gpu3D.writeMtxTrans(mask << (base * 8), data << (base * 8));              break; // MTX_TRANS
            case 0x4000480:
            case 0x4000481:
            case 0x4000482:
            case 0x4000483: base -= 0x4000480; size = 4; core->gpu3D.writeColor(mask << (base * 8), data << (base * 8));                 break; // COLOR
            case 0x4000484:
            case 0x4000485:
            case 0x4000486:
            case 0x4000487: base -= 0x4000484; size = 4; core->gpu3D.writeNormal(mask << (base * 8), data << (base * 8));                break; // NORMAL
            case 0x4000488:
            case 0x4000489:
            case 0x400048A:
            case 0x400048B: base -= 0x4000488; size = 4; core->gpu3D.writeTexCoord(mask << (base * 8), data << (base * 8));              break; // TEXCOORD
            case 0x400048C:
            case 0x400048D:
            case 0x400048E:
            case 0x400048F: base -= 0x400048C; size = 4; core->gpu3D.writeVtx16(mask << (base * 8), data << (base * 8));                 break; // VTX_16
            case 0x4000490:
            case 0x4000491:
            case 0x4000492:
            case 0x4000493: base -= 0x4000490; size = 4; core->gpu3D.writeVtx10(mask << (base * 8), data << (base * 8));                 break; // VTX_10
            case 0x4000494:
            case 0x4000495:
            case 0x4000496:
            case 0x4000497: base -= 0x4000494; size = 4; core->gpu3D.writeVtxXY(mask << (base * 8), data << (base * 8));                 break; // VTX_XY
            case 0x4000498:
            case 0x4000499:
            case 0x400049A:
            case 0x400049B: base -= 0x4000498; size = 4; core->gpu3D.writeVtxXZ(mask << (base * 8), data << (base * 8));                 break; // VTX_XZ
            case 0x400049C:
            case 0x400049D:
            case 0x400049E:
            case 0x400049F: base -= 0x400049C; size = 4; core->gpu3D.writeVtxYZ(mask << (base * 8), data << (base * 8));                 break; // VTX_YZ
            case 0x40004A0:
            case 0x40004A1:
            case 0x40004A2:
            case 0x40004A3: base -= 0x40004A0; size = 4; core->gpu3D.writeVtxDiff(mask << (base * 8), data << (base * 8));               break; // VTX_DIFF
            case 0x40004A4:
            case 0x40004A5:
            case 0x40004A6:
            case 0x40004A7: base -= 0x40004A4; size = 4; core->gpu3D.writePolygonAttr(mask << (base * 8), data << (base * 8));           break; // POLYGON_ATTR
            case 0x40004A8:
            case 0x40004A9:
            case 0x40004AA:
            case 0x40004AB: base -= 0x40004A8; size = 4; core->gpu3D.writeTexImageParam(mask << (base * 8), data << (base * 8));         break; // TEXIMAGE_PARAM
            case 0x40004AC:
            case 0x40004AD:
            case 0x40004AE:
            case 0x40004AF: base -= 0x40004AC; size = 4; core->gpu3D.writePlttBase(mask << (base * 8), data << (base * 8));              break; // PLTT_BASE
            case 0x40004C0:
            case 0x40004C1:
            case 0x40004C2:
            case 0x40004C3: base -= 0x40004C0; size = 4; core->gpu3D.writeDifAmb(mask << (base * 8), data << (base * 8));                break; // DIF_AMB
            case 0x40004C4:
            case 0x40004C5:
            case 0x40004C6:
            case 0x40004C7: base -= 0x40004C4; size = 4; core->gpu3D.writeSpeEmi(mask << (base * 8), data << (base * 8));                break; // SPE_EMI
            case 0x40004C8:
            case 0x40004C9:
            case 0x40004CA:
            case 0x40004CB: base -= 0x40004C8; size = 4; core->gpu3D.writeLightVector(mask << (base * 8), data << (base * 8));           break; // LIGHT_VECTOR
            case 0x40004CC:
            case 0x40004CD:
            case 0x40004CE:
            case 0x40004CF: base -= 0x40004CC; size = 4; core->gpu3D.writeLightColor(mask << (base * 8), data << (base * 8));            break; // LIGHT_COLOR
            case 0x40004D0:
            case 0x40004D1:
            case 0x40004D2:
            case 0x40004D3: base -= 0x40004D0; size = 4; core->gpu3D.writeShininess(mask << (base * 8), data << (base * 8));             break; // SHININESS
            case 0x4000500:
            case 0x4000501:
            case 0x4000502:
            case 0x4000503: base -= 0x4000500; size = 4; core->gpu3D.writeBeginVtxs(mask << (base * 8), data << (base * 8));             break; // BEGIN_VTXS
            case 0x4000504:
            case 0x4000505:
            case 0x4000506:
            case 0x4000507: base -= 0x4000504; size = 4; core->gpu3D.writeEndVtxs(mask << (base * 8), data << (base * 8));               break; // END_VTXS
            case 0x4000540:
            case 0x4000541:
            case 0x4000542:
            case 0x4000543: base -= 0x4000540; size = 4; core->gpu3D.writeSwapBuffers(mask << (base * 8), data << (base * 8));           break; // SWAP_BUFFERS
            case 0x4000580:
            case 0x4000581:
            case 0x4000582:
            case 0x4000583: base -= 0x4000580; size = 4; core->gpu3D.writeViewport(mask << (base * 8), data << (base * 8));              break; // VIEWPORT
            case 0x40005C0:
            case 0x40005C1:
            case 0x40005C2:
            case 0x40005C3: base -= 0x40005C0; size = 4; core->gpu3D.writeBoxTest(mask << (base * 8), data << (base * 8));               break; // BOX_TEST
            case 0x40005C4:
            case 0x40005C5:
            case 0x40005C6:
            case 0x40005C7: base -= 0x40005C4; size = 4; core->gpu3D.writePosTest(mask << (base * 8), data << (base * 8));               break; // POS_TEST
            case 0x40005C8:
            case 0x40005C9:
            case 0x40005CA:
            case 0x40005CB: base -= 0x40005C8; size = 4; core->gpu3D.writeVecTest(mask << (base * 8), data << (base * 8));               break; // VEC_TEST
            case 0x4000600:
            case 0x4000601:
            case 0x4000602:
            case 0x4000603: base -= 0x4000600; size = 4; core->gpu3D.writeGxStat(mask << (base * 8), data << (base * 8));                break; // GXSTAT
            case 0x4001000:
            case 0x4001001:
            case 0x4001002:
            case 0x4001003: base -= 0x4001000; size = 4; core->gpu2D[1].writeDispCnt(mask << (base * 8), data << (base * 8));            break; // DISPCNT (engine B)
            case 0x4001008:
            case 0x4001009: base -= 0x4001008; size = 2; core->gpu2D[1].writeBgCnt(0, mask << (base * 8), data << (base * 8));           break; // BG0CNT (engine B)
            case 0x400100A:
            case 0x400100B: base -= 0x400100A; size = 2; core->gpu2D[1].writeBgCnt(1, mask << (base * 8), data << (base * 8));           break; // BG1CNT (engine B)
            case 0x400100C:
            case 0x400100D: base -= 0x400100C; size = 2; core->gpu2D[1].writeBgCnt(2, mask << (base * 8), data << (base * 8));           break; // BG2CNT (engine B)
            case 0x400100E:
            case 0x400100F: base -= 0x400100E; size = 2; core->gpu2D[1].writeBgCnt(3, mask << (base * 8), data << (base * 8));           break; // BG3CNT (engine B)
            case 0x4001010:
            case 0x4001011: base -= 0x4001010; size = 2; core->gpu2D[1].writeBgHOfs(0, mask << (base * 8), data << (base * 8));          break; // BG0HOFS (engine B)
            case 0x4001012:
            case 0x4001013: base -= 0x4001012; size = 2; core->gpu2D[1].writeBgVOfs(0, mask << (base * 8), data << (base * 8));          break; // BG0VOFS (engine B)
            case 0x4001014:
            case 0x4001015: base -= 0x4001014; size = 2; core->gpu2D[1].writeBgHOfs(1, mask << (base * 8), data << (base * 8));          break; // BG1HOFS (engine B)
            case 0x4001016:
            case 0x4001017: base -= 0x4001016; size = 2; core->gpu2D[1].writeBgVOfs(1, mask << (base * 8), data << (base * 8));          break; // BG1VOFS (engine B)
            case 0x4001018:
            case 0x4001019: base -= 0x4001018; size = 2; core->gpu2D[1].writeBgHOfs(2, mask << (base * 8), data << (base * 8));          break; // BG2HOFS (engine B)
            case 0x400101A:
            case 0x400101B: base -= 0x400101A; size = 2; core->gpu2D[1].writeBgVOfs(2, mask << (base * 8), data << (base * 8));          break; // BG2VOFS (engine B)
            case 0x400101C:
            case 0x400101D: base -= 0x400101C; size = 2; core->gpu2D[1].writeBgHOfs(3, mask << (base * 8), data << (base * 8));          break; // BG3HOFS (engine B)
            case 0x400101E:
            case 0x400101F: base -= 0x400101E; size = 2; core->gpu2D[1].writeBgVOfs(3, mask << (base * 8), data << (base * 8));          break; // BG3VOFS (engine B)
            case 0x4001020:
            case 0x4001021: base -= 0x4001020; size = 2; core->gpu2D[1].writeBgPA(2, mask << (base * 8), data << (base * 8));            break; // BG2PA (engine B)
            case 0x4001022:
            case 0x4001023: base -= 0x4001022; size = 2; core->gpu2D[1].writeBgPB(2, mask << (base * 8), data << (base * 8));            break; // BG2PB (engine B)
            case 0x4001024:
            case 0x4001025: base -= 0x4001024; size = 2; core->gpu2D[1].writeBgPC(2, mask << (base * 8), data << (base * 8));            break; // BG2PC (engine B)
            case 0x4001026:
            case 0x4001027: base -= 0x4001026; size = 2; core->gpu2D[1].writeBgPD(2, mask << (base * 8), data << (base * 8));            break; // BG2PD (engine B)
            case 0x4001028:
            case 0x4001029:
            case 0x400102A:
            case 0x400102B: base -= 0x4001028; size = 4; core->gpu2D[1].writeBgX(2, mask << (base * 8), data << (base * 8));             break; // BG2X (engine B)
            case 0x400102C:
            case 0x400102D:
            case 0x400102E:
            case 0x400102F: base -= 0x400102C; size = 4; core->gpu2D[1].writeBgY(2, mask << (base * 8), data << (base * 8));             break; // BG2Y (engine B)
            case 0x4001030:
            case 0x4001031: base -= 0x4001030; size = 2; core->gpu2D[1].writeBgPA(3, mask << (base * 8), data << (base * 8));            break; // BG3PA (engine B)
            case 0x4001032:
            case 0x4001033: base -= 0x4001032; size = 2; core->gpu2D[1].writeBgPB(3, mask << (base * 8), data << (base * 8));            break; // BG3PB (engine B)
            case 0x4001034:
            case 0x4001035: base -= 0x4001034; size = 2; core->gpu2D[1].writeBgPC(3, mask << (base * 8), data << (base * 8));            break; // BG3PC (engine B)
            case 0x4001036:
            case 0x4001037: base -= 0x4001036; size = 2; core->gpu2D[1].writeBgPD(3, mask << (base * 8), data << (base * 8));            break; // BG3PD (engine B)
            case 0x4001038:
            case 0x4001039:
            case 0x400103A:
            case 0x400103B: base -= 0x4001038; size = 4; core->gpu2D[1].writeBgX(3, mask << (base * 8), data << (base * 8));             break; // BG3X (engine B)
            case 0x400103C:
            case 0x400103D:
            case 0x400103E:
            case 0x400103F: base -= 0x400103C; size = 4; core->gpu2D[1].writeBgY(3, mask << (base * 8), data << (base * 8));             break; // BG3Y (engine B)
            case 0x4001040:
            case 0x4001041: base -= 0x4001040; size = 2; core->gpu2D[1].writeWinH(0, mask << (base * 8), data << (base * 8));            break; // WIN0H (engine B)
            case 0x4001042:
            case 0x4001043: base -= 0x4001042; size = 2; core->gpu2D[1].writeWinH(1, mask << (base * 8), data << (base * 8));            break; // WIN1H (engine B)
            case 0x4001044:
            case 0x4001045: base -= 0x4001044; size = 2; core->gpu2D[1].writeWinV(0, mask << (base * 8), data << (base * 8));            break; // WIN0V (engine B)
            case 0x4001046:
            case 0x4001047: base -= 0x4001046; size = 2; core->gpu2D[1].writeWinV(1, mask << (base * 8), data << (base * 8));            break; // WIN1V (engine B)
            case 0x4001048:
            case 0x4001049: base -= 0x4001048; size = 2; core->gpu2D[1].writeWinIn(mask << (base * 8), data << (base * 8));              break; // WININ (engine B)
            case 0x400104A:
            case 0x400104B: base -= 0x400104A; size = 2; core->gpu2D[1].writeWinOut(mask << (base * 8), data << (base * 8));             break; // WINOUT (engine B)
            case 0x4001050:
            case 0x4001051: base -= 0x4001050; size = 2; core->gpu2D[1].writeBldCnt(mask << (base * 8), data << (base * 8));             break; // BLDCNT (engine B)
            case 0x4001052:
            case 0x4001053: base -= 0x4001052; size = 2; core->gpu2D[1].writeBldAlpha(mask << (base * 8), data << (base * 8));           break; // BLDALPHA (engine B)
            case 0x4001054: base -= 0x4001054; size = 1; core->gpu2D[1].writeBldY(data << (base * 8));                                   break; // BLDY (engine B)
            case 0x400106C:
            case 0x400106D: base -= 0x400106C; size = 2; core->gpu2D[1].writeMasterBright(mask << (base * 8), data << (base * 8));       break; // MASTER_BRIGHT (engine B)

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
            case 0x4000005: base -= 0x4000004; size = 2; core->gpu.writeDispStat(1, mask << (base * 8), data << (base * 8));         break; // DISPSTAT (ARM7)
            case 0x40000B0:
            case 0x40000B1:
            case 0x40000B2:
            case 0x40000B3: base -= 0x40000B0; size = 4; core->dma[1].writeDmaSad(0, mask << (base * 8), data << (base * 8));        break; // DMA0SAD (ARM7)
            case 0x40000B4:
            case 0x40000B5:
            case 0x40000B6:
            case 0x40000B7: base -= 0x40000B4; size = 4; core->dma[1].writeDmaDad(0, mask << (base * 8), data << (base * 8));        break; // DMA0DAD (ARM7)
            case 0x40000B8:
            case 0x40000B9:
            case 0x40000BA:
            case 0x40000BB: base -= 0x40000B8; size = 4; core->dma[1].writeDmaCnt(0, mask << (base * 8), data << (base * 8));        break; // DMA0CNT (ARM7)
            case 0x40000BC:
            case 0x40000BD:
            case 0x40000BE:
            case 0x40000BF: base -= 0x40000BC; size = 4; core->dma[1].writeDmaSad(1, mask << (base * 8), data << (base * 8));        break; // DMA1SAD (ARM7)
            case 0x40000C0:
            case 0x40000C1:
            case 0x40000C2:
            case 0x40000C3: base -= 0x40000C0; size = 4; core->dma[1].writeDmaDad(1, mask << (base * 8), data << (base * 8));        break; // DMA1DAD (ARM7)
            case 0x40000C4:
            case 0x40000C5:
            case 0x40000C6:
            case 0x40000C7: base -= 0x40000C4; size = 4; core->dma[1].writeDmaCnt(1, mask << (base * 8), data << (base * 8));        break; // DMA1CNT (ARM7)
            case 0x40000C8:
            case 0x40000C9:
            case 0x40000CA:
            case 0x40000CB: base -= 0x40000C8; size = 4; core->dma[1].writeDmaSad(2, mask << (base * 8), data << (base * 8));        break; // DMA2SAD (ARM7)
            case 0x40000CC:
            case 0x40000CD:
            case 0x40000CE:
            case 0x40000CF: base -= 0x40000CC; size = 4; core->dma[1].writeDmaDad(2, mask << (base * 8), data << (base * 8));        break; // DMA2DAD (ARM7)
            case 0x40000D0:
            case 0x40000D1:
            case 0x40000D2:
            case 0x40000D3: base -= 0x40000D0; size = 4; core->dma[1].writeDmaCnt(2, mask << (base * 8), data << (base * 8));        break; // DMA2CNT (ARM7)
            case 0x40000D4:
            case 0x40000D5:
            case 0x40000D6:
            case 0x40000D7: base -= 0x40000D4; size = 4; core->dma[1].writeDmaSad(3, mask << (base * 8), data << (base * 8));        break; // DMA3SAD (ARM7)
            case 0x40000D8:
            case 0x40000D9:
            case 0x40000DA:
            case 0x40000DB: base -= 0x40000D8; size = 4; core->dma[1].writeDmaDad(3, mask << (base * 8), data << (base * 8));        break; // DMA3DAD (ARM7)
            case 0x40000DC:
            case 0x40000DD:
            case 0x40000DE:
            case 0x40000DF: base -= 0x40000DC; size = 4; core->dma[1].writeDmaCnt(3, mask << (base * 8), data << (base * 8));        break; // DMA3CNT (ARM7)
            case 0x4000100:
            case 0x4000101: base -= 0x4000100; size = 2; core->timers[1].writeTmCntL(0, mask << (base * 8), data << (base * 8));     break; // TM0CNT_L (ARM7)
            case 0x4000102:
            case 0x4000103: base -= 0x4000102; size = 2; core->timers[1].writeTmCntH(0, mask << (base * 8), data << (base * 8));     break; // TM0CNT_H (ARM7)
            case 0x4000104:
            case 0x4000105: base -= 0x4000104; size = 2; core->timers[1].writeTmCntL(1, mask << (base * 8), data << (base * 8));     break; // TM1CNT_L (ARM7)
            case 0x4000106:
            case 0x4000107: base -= 0x4000106; size = 2; core->timers[1].writeTmCntH(1, mask << (base * 8), data << (base * 8));     break; // TM1CNT_H (ARM7)
            case 0x4000108:
            case 0x4000109: base -= 0x4000108; size = 2; core->timers[1].writeTmCntL(2, mask << (base * 8), data << (base * 8));     break; // TM2CNT_L (ARM7)
            case 0x400010A:
            case 0x400010B: base -= 0x400010A; size = 2; core->timers[1].writeTmCntH(2, mask << (base * 8), data << (base * 8));     break; // TM2CNT_H (ARM7)
            case 0x400010C:
            case 0x400010D: base -= 0x400010C; size = 2; core->timers[1].writeTmCntL(3, mask << (base * 8), data << (base * 8));     break; // TM3CNT_L (ARM7)
            case 0x400010E:
            case 0x400010F: base -= 0x400010E; size = 2; core->timers[1].writeTmCntH(3, mask << (base * 8), data << (base * 8));     break; // TM3CNT_H (ARM7)
            case 0x4000138: base -= 0x4000138; size = 1; core->rtc.writeRtc(data << (base * 8));                                     break; // RTC
            case 0x4000180:
            case 0x4000181: base -= 0x4000180; size = 2; core->ipc.writeIpcSync(1, mask << (base * 8), data << (base * 8));          break; // IPCSYNC (ARM7)
            case 0x4000184:
            case 0x4000185: base -= 0x4000184; size = 2; core->ipc.writeIpcFifoCnt(1, mask << (base * 8), data << (base * 8));       break; // IPCFIFOCNT (ARM7)
            case 0x4000188:
            case 0x4000189:
            case 0x400018A:
            case 0x400018B: base -= 0x4000188; size = 4; core->ipc.writeIpcFifoSend(1, mask << (base * 8), data << (base * 8));      break; // IPCFIFOSEND (ARM7)
            case 0x40001A0:
            case 0x40001A1: base -= 0x40001A0; size = 2; core->cartridge.writeAuxSpiCnt(1, mask << (base * 8), data << (base * 8));  break; // AUXSPICNT (ARM7)
            case 0x40001A2: base -= 0x40001A2; size = 1; core->cartridge.writeAuxSpiData(1, data << (base * 8));                     break; // AUXSPIDATA (ARM7)
            case 0x40001A4:
            case 0x40001A5:
            case 0x40001A6:
            case 0x40001A7: base -= 0x40001A4; size = 4; core->cartridge.writeRomCtrl(1, mask << (base * 8), data << (base * 8));    break; // ROMCTRL (ARM7)
            case 0x40001A8:
            case 0x40001A9:
            case 0x40001AA:
            case 0x40001AB: base -= 0x40001A8; size = 4; core->cartridge.writeRomCmdOutL(1, mask << (base * 8), data << (base * 8)); break; // ROMCMDOUT_L (ARM7)
            case 0x40001AC:
            case 0x40001AD:
            case 0x40001AE:
            case 0x40001AF: base -= 0x40001AC; size = 4; core->cartridge.writeRomCmdOutH(1, mask << (base * 8), data << (base * 8)); break; // ROMCMDOUT_H (ARM7)
            case 0x40001C0:
            case 0x40001C1: base -= 0x40001C0; size = 2; core->spi.writeSpiCnt(mask << (base * 8), data << (base * 8));              break; // SPICNT
            case 0x40001C2: base -= 0x40001C2; size = 1; core->spi.writeSpiData(data << (base * 8));                                 break; // SPIDATA
            case 0x4000208: base -= 0x4000208; size = 1; core->interpreter[1].writeIme(data << (base * 8));                          break; // IME (ARM7)
            case 0x4000210:
            case 0x4000211:
            case 0x4000212:
            case 0x4000213: base -= 0x4000210; size = 4; core->interpreter[1].writeIe(mask << (base * 8), data << (base * 8));       break; // IE (ARM7)
            case 0x4000214:
            case 0x4000215:
            case 0x4000216:
            case 0x4000217: base -= 0x4000214; size = 4; core->interpreter[1].writeIrf(mask << (base * 8), data << (base * 8));      break; // IF (ARM7)
            case 0x4000300: base -= 0x4000300; size = 1; core->interpreter[1].writePostFlg(data << (base * 8));                      break; // POSTFLG (ARM7)
            case 0x4000301: base -= 0x4000301; size = 1; writeHaltCnt(data << (base * 8));                                           break; // HALTCNT
            case 0x4000400:
            case 0x4000401:
            case 0x4000402:
            case 0x4000403: base -= 0x4000400; size = 4; core->spu.writeSoundCnt(0, mask << (base * 8), data << (base * 8));         break; // SOUND0CNT
            case 0x4000404:
            case 0x4000405:
            case 0x4000406:
            case 0x4000407: base -= 0x4000404; size = 4; core->spu.writeSoundSad(0, mask << (base * 8), data << (base * 8));         break; // SOUND0SAD
            case 0x4000408:
            case 0x4000409: base -= 0x4000408; size = 2; core->spu.writeSoundTmr(0, mask << (base * 8), data << (base * 8));         break; // SOUND0TMR
            case 0x400040A:
            case 0x400040B: base -= 0x400040A; size = 2; core->spu.writeSoundPnt(0, mask << (base * 8), data << (base * 8));         break; // SOUND0PNT
            case 0x400040C:
            case 0x400040D:
            case 0x400040E:
            case 0x400040F: base -= 0x400040C; size = 4; core->spu.writeSoundLen(0, mask << (base * 8), data << (base * 8));         break; // SOUND0LEN
            case 0x4000410:
            case 0x4000411:
            case 0x4000412:
            case 0x4000413: base -= 0x4000410; size = 4; core->spu.writeSoundCnt(1, mask << (base * 8), data << (base * 8));         break; // SOUND1CNT
            case 0x4000414:
            case 0x4000415:
            case 0x4000416:
            case 0x4000417: base -= 0x4000414; size = 4; core->spu.writeSoundSad(1, mask << (base * 8), data << (base * 8));         break; // SOUND1SAD
            case 0x4000418:
            case 0x4000419: base -= 0x4000418; size = 2; core->spu.writeSoundTmr(1, mask << (base * 8), data << (base * 8));         break; // SOUND1TMR
            case 0x400041A:
            case 0x400041B: base -= 0x400041A; size = 2; core->spu.writeSoundPnt(1, mask << (base * 8), data << (base * 8));         break; // SOUND1PNT
            case 0x400041C:
            case 0x400041D:
            case 0x400041E:
            case 0x400041F: base -= 0x400041C; size = 4; core->spu.writeSoundLen(1, mask << (base * 8), data << (base * 8));         break; // SOUND1LEN
            case 0x4000420:
            case 0x4000421:
            case 0x4000422:
            case 0x4000423: base -= 0x4000420; size = 4; core->spu.writeSoundCnt(2, mask << (base * 8), data << (base * 8));         break; // SOUND2CNT
            case 0x4000424:
            case 0x4000425:
            case 0x4000426:
            case 0x4000427: base -= 0x4000424; size = 4; core->spu.writeSoundSad(2, mask << (base * 8), data << (base * 8));         break; // SOUND2SAD
            case 0x4000428:
            case 0x4000429: base -= 0x4000428; size = 2; core->spu.writeSoundTmr(2, mask << (base * 8), data << (base * 8));         break; // SOUND2TMR
            case 0x400042A:
            case 0x400042B: base -= 0x400042A; size = 2; core->spu.writeSoundPnt(2, mask << (base * 8), data << (base * 8));         break; // SOUND2PNT
            case 0x400042C:
            case 0x400042D:
            case 0x400042E:
            case 0x400042F: base -= 0x400042C; size = 4; core->spu.writeSoundLen(2, mask << (base * 8), data << (base * 8));         break; // SOUND2LEN
            case 0x4000430:
            case 0x4000431:
            case 0x4000432:
            case 0x4000433: base -= 0x4000430; size = 4; core->spu.writeSoundCnt(3, mask << (base * 8), data << (base * 8));         break; // SOUND3CNT
            case 0x4000434:
            case 0x4000435:
            case 0x4000436:
            case 0x4000437: base -= 0x4000434; size = 4; core->spu.writeSoundSad(3, mask << (base * 8), data << (base * 8));         break; // SOUND3SAD
            case 0x4000438:
            case 0x4000439: base -= 0x4000438; size = 2; core->spu.writeSoundTmr(3, mask << (base * 8), data << (base * 8));         break; // SOUND3TMR
            case 0x400043A:
            case 0x400043B: base -= 0x400043A; size = 2; core->spu.writeSoundPnt(3, mask << (base * 8), data << (base * 8));         break; // SOUND3PNT
            case 0x400043C:
            case 0x400043D:
            case 0x400043E:
            case 0x400043F: base -= 0x400043C; size = 4; core->spu.writeSoundLen(3, mask << (base * 8), data << (base * 8));         break; // SOUND3LEN
            case 0x4000440:
            case 0x4000441:
            case 0x4000442:
            case 0x4000443: base -= 0x4000440; size = 4; core->spu.writeSoundCnt(4, mask << (base * 8), data << (base * 8));         break; // SOUND4CNT
            case 0x4000444:
            case 0x4000445:
            case 0x4000446:
            case 0x4000447: base -= 0x4000444; size = 4; core->spu.writeSoundSad(4, mask << (base * 8), data << (base * 8));         break; // SOUND4SAD
            case 0x4000448:
            case 0x4000449: base -= 0x4000448; size = 2; core->spu.writeSoundTmr(4, mask << (base * 8), data << (base * 8));         break; // SOUND4TMR
            case 0x400044A:
            case 0x400044B: base -= 0x400044A; size = 2; core->spu.writeSoundPnt(4, mask << (base * 8), data << (base * 8));         break; // SOUND4PNT
            case 0x400044C:
            case 0x400044D:
            case 0x400044E:
            case 0x400044F: base -= 0x400044C; size = 4; core->spu.writeSoundLen(4, mask << (base * 8), data << (base * 8));         break; // SOUND4LEN
            case 0x4000450:
            case 0x4000451:
            case 0x4000452:
            case 0x4000453: base -= 0x4000450; size = 4; core->spu.writeSoundCnt(5, mask << (base * 8), data << (base * 8));         break; // SOUND5CNT
            case 0x4000454:
            case 0x4000455:
            case 0x4000456:
            case 0x4000457: base -= 0x4000454; size = 4; core->spu.writeSoundSad(5, mask << (base * 8), data << (base * 8));         break; // SOUND5SAD
            case 0x4000458:
            case 0x4000459: base -= 0x4000458; size = 2; core->spu.writeSoundTmr(5, mask << (base * 8), data << (base * 8));         break; // SOUND5TMR
            case 0x400045A:
            case 0x400045B: base -= 0x400045A; size = 2; core->spu.writeSoundPnt(5, mask << (base * 8), data << (base * 8));         break; // SOUND5PNT
            case 0x400045C:
            case 0x400045D:
            case 0x400045E:
            case 0x400045F: base -= 0x400045C; size = 4; core->spu.writeSoundLen(5, mask << (base * 8), data << (base * 8));         break; // SOUND5LEN
            case 0x4000460:
            case 0x4000461:
            case 0x4000462:
            case 0x4000463: base -= 0x4000460; size = 4; core->spu.writeSoundCnt(6, mask << (base * 8), data << (base * 8));         break; // SOUND6CNT
            case 0x4000464:
            case 0x4000465:
            case 0x4000466:
            case 0x4000467: base -= 0x4000464; size = 4; core->spu.writeSoundSad(6, mask << (base * 8), data << (base * 8));         break; // SOUND6SAD
            case 0x4000468:
            case 0x4000469: base -= 0x4000468; size = 2; core->spu.writeSoundTmr(6, mask << (base * 8), data << (base * 8));         break; // SOUND6TMR
            case 0x400046A:
            case 0x400046B: base -= 0x400046A; size = 2; core->spu.writeSoundPnt(6, mask << (base * 8), data << (base * 8));         break; // SOUND6PNT
            case 0x400046C:
            case 0x400046D:
            case 0x400046E:
            case 0x400046F: base -= 0x400046C; size = 4; core->spu.writeSoundLen(6, mask << (base * 8), data << (base * 8));         break; // SOUND6LEN
            case 0x4000470:
            case 0x4000471:
            case 0x4000472:
            case 0x4000473: base -= 0x4000470; size = 4; core->spu.writeSoundCnt(7, mask << (base * 8), data << (base * 8));         break; // SOUND7CNT
            case 0x4000474:
            case 0x4000475:
            case 0x4000476:
            case 0x4000477: base -= 0x4000474; size = 4; core->spu.writeSoundSad(7, mask << (base * 8), data << (base * 8));         break; // SOUND7SAD
            case 0x4000478:
            case 0x4000479: base -= 0x4000478; size = 2; core->spu.writeSoundTmr(7, mask << (base * 8), data << (base * 8));         break; // SOUND7TMR
            case 0x400047A:
            case 0x400047B: base -= 0x400047A; size = 2; core->spu.writeSoundPnt(7, mask << (base * 8), data << (base * 8));         break; // SOUND7PNT
            case 0x400047C:
            case 0x400047D:
            case 0x400047E:
            case 0x400047F: base -= 0x400047C; size = 4; core->spu.writeSoundLen(7, mask << (base * 8), data << (base * 8));         break; // SOUND7LEN
            case 0x4000480:
            case 0x4000481:
            case 0x4000482:
            case 0x4000483: base -= 0x4000480; size = 4; core->spu.writeSoundCnt(8, mask << (base * 8), data << (base * 8));         break; // SOUND8CNT
            case 0x4000484:
            case 0x4000485:
            case 0x4000486:
            case 0x4000487: base -= 0x4000484; size = 4; core->spu.writeSoundSad(8, mask << (base * 8), data << (base * 8));         break; // SOUND8SAD
            case 0x4000488:
            case 0x4000489: base -= 0x4000488; size = 2; core->spu.writeSoundTmr(8, mask << (base * 8), data << (base * 8));         break; // SOUND8TMR
            case 0x400048A:
            case 0x400048B: base -= 0x400048A; size = 2; core->spu.writeSoundPnt(8, mask << (base * 8), data << (base * 8));         break; // SOUND8PNT
            case 0x400048C:
            case 0x400048D:
            case 0x400048E:
            case 0x400048F: base -= 0x400048C; size = 4; core->spu.writeSoundLen(8, mask << (base * 8), data << (base * 8));         break; // SOUND8LEN
            case 0x4000490:
            case 0x4000491:
            case 0x4000492:
            case 0x4000493: base -= 0x4000490; size = 4; core->spu.writeSoundCnt(9, mask << (base * 8), data << (base * 8));         break; // SOUND9CNT
            case 0x4000494:
            case 0x4000495:
            case 0x4000496:
            case 0x4000497: base -= 0x4000494; size = 4; core->spu.writeSoundSad(9, mask << (base * 8), data << (base * 8));         break; // SOUND9SAD
            case 0x4000498:
            case 0x4000499: base -= 0x4000498; size = 2; core->spu.writeSoundTmr(9, mask << (base * 8), data << (base * 8));         break; // SOUND9TMR
            case 0x400049A:
            case 0x400049B: base -= 0x400049A; size = 2; core->spu.writeSoundPnt(9, mask << (base * 8), data << (base * 8));         break; // SOUND9PNT
            case 0x400049C:
            case 0x400049D:
            case 0x400049E:
            case 0x400049F: base -= 0x400049C; size = 4; core->spu.writeSoundLen(9, mask << (base * 8), data << (base * 8));         break; // SOUND9LEN
            case 0x40004A0:
            case 0x40004A1:
            case 0x40004A2:
            case 0x40004A3: base -= 0x40004A0; size = 4; core->spu.writeSoundCnt(10, mask << (base * 8), data << (base * 8));        break; // SOUND10CNT
            case 0x40004A4:
            case 0x40004A5:
            case 0x40004A6:
            case 0x40004A7: base -= 0x40004A4; size = 4; core->spu.writeSoundSad(10, mask << (base * 8), data << (base * 8));        break; // SOUND10SAD
            case 0x40004A8:
            case 0x40004A9: base -= 0x40004A8; size = 2; core->spu.writeSoundTmr(10, mask << (base * 8), data << (base * 8));        break; // SOUND10TMR
            case 0x40004AA:
            case 0x40004AB: base -= 0x40004AA; size = 2; core->spu.writeSoundPnt(10, mask << (base * 8), data << (base * 8));        break; // SOUND10PNT
            case 0x40004AC:
            case 0x40004AD:
            case 0x40004AE:
            case 0x40004AF: base -= 0x40004AC; size = 4; core->spu.writeSoundLen(10, mask << (base * 8), data << (base * 8));        break; // SOUND10LEN
            case 0x40004B0:
            case 0x40004B1:
            case 0x40004B2:
            case 0x40004B3: base -= 0x40004B0; size = 4; core->spu.writeSoundCnt(11, mask << (base * 8), data << (base * 8));        break; // SOUND11CNT
            case 0x40004B4:
            case 0x40004B5:
            case 0x40004B6:
            case 0x40004B7: base -= 0x40004B4; size = 4; core->spu.writeSoundSad(11, mask << (base * 8), data << (base * 8));        break; // SOUND11SAD
            case 0x40004B8:
            case 0x40004B9: base -= 0x40004B8; size = 2; core->spu.writeSoundTmr(11, mask << (base * 8), data << (base * 8));        break; // SOUND11TMR
            case 0x40004BA:
            case 0x40004BB: base -= 0x40004BA; size = 2; core->spu.writeSoundPnt(11, mask << (base * 8), data << (base * 8));        break; // SOUND11PNT
            case 0x40004BC:
            case 0x40004BD:
            case 0x40004BE:
            case 0x40004BF: base -= 0x40004BC; size = 4; core->spu.writeSoundLen(11, mask << (base * 8), data << (base * 8));        break; // SOUND11LEN
            case 0x40004C0:
            case 0x40004C1:
            case 0x40004C2:
            case 0x40004C3: base -= 0x40004C0; size = 4; core->spu.writeSoundCnt(12, mask << (base * 8), data << (base * 8));        break; // SOUND12CNT
            case 0x40004C4:
            case 0x40004C5:
            case 0x40004C6:
            case 0x40004C7: base -= 0x40004C4; size = 4; core->spu.writeSoundSad(12, mask << (base * 8), data << (base * 8));        break; // SOUND12SAD
            case 0x40004C8:
            case 0x40004C9: base -= 0x40004C8; size = 2; core->spu.writeSoundTmr(12, mask << (base * 8), data << (base * 8));        break; // SOUND12TMR
            case 0x40004CA:
            case 0x40004CB: base -= 0x40004CA; size = 2; core->spu.writeSoundPnt(12, mask << (base * 8), data << (base * 8));        break; // SOUND12PNT
            case 0x40004CC:
            case 0x40004CD:
            case 0x40004CE:
            case 0x40004CF: base -= 0x40004CC; size = 4; core->spu.writeSoundLen(12, mask << (base * 8), data << (base * 8));        break; // SOUND12LEN
            case 0x40004D0:
            case 0x40004D1:
            case 0x40004D2:
            case 0x40004D3: base -= 0x40004D0; size = 4; core->spu.writeSoundCnt(13, mask << (base * 8), data << (base * 8));        break; // SOUND13CNT
            case 0x40004D4:
            case 0x40004D5:
            case 0x40004D6:
            case 0x40004D7: base -= 0x40004D4; size = 4; core->spu.writeSoundSad(13, mask << (base * 8), data << (base * 8));        break; // SOUND13SAD
            case 0x40004D8:
            case 0x40004D9: base -= 0x40004D8; size = 2; core->spu.writeSoundTmr(13, mask << (base * 8), data << (base * 8));        break; // SOUND13TMR
            case 0x40004DA:
            case 0x40004DB: base -= 0x40004DA; size = 2; core->spu.writeSoundPnt(13, mask << (base * 8), data << (base * 8));        break; // SOUND13PNT
            case 0x40004DC:
            case 0x40004DD:
            case 0x40004DE:
            case 0x40004DF: base -= 0x40004DC; size = 4; core->spu.writeSoundLen(13, mask << (base * 8), data << (base * 8));        break; // SOUND13LEN
            case 0x40004E0:
            case 0x40004E1:
            case 0x40004E2:
            case 0x40004E3: base -= 0x40004E0; size = 4; core->spu.writeSoundCnt(14, mask << (base * 8), data << (base * 8));        break; // SOUND14CNT
            case 0x40004E4:
            case 0x40004E5:
            case 0x40004E6:
            case 0x40004E7: base -= 0x40004E4; size = 4; core->spu.writeSoundSad(14, mask << (base * 8), data << (base * 8));        break; // SOUND14SAD
            case 0x40004E8:
            case 0x40004E9: base -= 0x40004E8; size = 2; core->spu.writeSoundTmr(14, mask << (base * 8), data << (base * 8));        break; // SOUND14TMR
            case 0x40004EA:
            case 0x40004EB: base -= 0x40004EA; size = 2; core->spu.writeSoundPnt(14, mask << (base * 8), data << (base * 8));        break; // SOUND14PNT
            case 0x40004EC:
            case 0x40004ED:
            case 0x40004EE:
            case 0x40004EF: base -= 0x40004EC; size = 4; core->spu.writeSoundLen(14, mask << (base * 8), data << (base * 8));        break; // SOUND14LEN
            case 0x40004F0:
            case 0x40004F1:
            case 0x40004F2:
            case 0x40004F3: base -= 0x40004F0; size = 4; core->spu.writeSoundCnt(15, mask << (base * 8), data << (base * 8));        break; // SOUND15CNT
            case 0x40004F4:
            case 0x40004F5:
            case 0x40004F6:
            case 0x40004F7: base -= 0x40004F4; size = 4; core->spu.writeSoundSad(15, mask << (base * 8), data << (base * 8));        break; // SOUND15SAD
            case 0x40004F8:
            case 0x40004F9: base -= 0x40004F8; size = 2; core->spu.writeSoundTmr(15, mask << (base * 8), data << (base * 8));        break; // SOUND15TMR
            case 0x40004FA:
            case 0x40004FB: base -= 0x40004FA; size = 2; core->spu.writeSoundPnt(15, mask << (base * 8), data << (base * 8));        break; // SOUND15PNT
            case 0x40004FC:
            case 0x40004FD:
            case 0x40004FE:
            case 0x40004FF: base -= 0x40004FC; size = 4; core->spu.writeSoundLen(15, mask << (base * 8), data << (base * 8));        break; // SOUND15LEN
            case 0x4000500:
            case 0x4000501: base -= 0x4000500; size = 2; core->spu.writeMainSoundCnt(mask << (base * 8), data << (base * 8));        break; // SOUNDCNT
            case 0x4000504:
            case 0x4000505: base -= 0x4000504; size = 2; core->spu.writeSoundBias(mask << (base * 8), data << (base * 8));           break; // SOUNDBIAS
            case 0x4000508: base -= 0x4000508; size = 1; core->spu.writeSndCapCnt(0, data << (base * 8));                            break; // SNDCAP0CNT
            case 0x4000509: base -= 0x4000509; size = 1; core->spu.writeSndCapCnt(1, data << (base * 8));                            break; // SNDCAP1CNT
            case 0x4000510:
            case 0x4000511:
            case 0x4000512:
            case 0x4000513: base -= 0x4000510; size = 4; core->spu.writeSndCapDad(0, mask << (base * 8), data << (base * 8));        break; // SNDCAP0DAD
            case 0x4000514:
            case 0x4000515: base -= 0x4000514; size = 2; core->spu.writeSndCapLen(0, mask << (base * 8), data << (base * 8));        break; // SNDCAP0LEN
            case 0x4000518:
            case 0x4000519:
            case 0x400051A:
            case 0x400051B: base -= 0x4000518; size = 4; core->spu.writeSndCapDad(1, mask << (base * 8), data << (base * 8));        break; // SNDCAP1DAD
            case 0x400051C:
            case 0x400051D: base -= 0x400051C; size = 2; core->spu.writeSndCapLen(1, mask << (base * 8), data << (base * 8));        break; // SNDCAP1LEN
            case 0x4800006:
            case 0x4800007: base -= 0x4800006; size = 2; core->wifi.writeWModeWep(mask << (base * 8), data << (base * 8));           break; // W_MODE_WEP
            case 0x4800010:
            case 0x4800011: base -= 0x4800010; size = 2; core->wifi.writeWIrf(mask << (base * 8), data << (base * 8));               break; // W_IF
            case 0x4800012:
            case 0x4800013: base -= 0x4800012; size = 2; core->wifi.writeWIe(mask << (base * 8), data << (base * 8));                break; // W_IE
            case 0x4800018:
            case 0x4800019: base -= 0x4800018; size = 2; core->wifi.writeWMacaddr(0, mask << (base * 8), data << (base * 8));        break; // W_MACADDR_0
            case 0x480001A:
            case 0x480001B: base -= 0x480001A; size = 2; core->wifi.writeWMacaddr(1, mask << (base * 8), data << (base * 8));        break; // W_MACADDR_1
            case 0x480001C:
            case 0x480001D: base -= 0x480001C; size = 2; core->wifi.writeWMacaddr(2, mask << (base * 8), data << (base * 8));        break; // W_MACADDR_2
            case 0x4800020:
            case 0x4800021: base -= 0x4800020; size = 2; core->wifi.writeWBssid(0, mask << (base * 8), data << (base * 8));          break; // W_BSSID_0
            case 0x4800022:
            case 0x4800023: base -= 0x4800022; size = 2; core->wifi.writeWBssid(1, mask << (base * 8), data << (base * 8));          break; // W_BSSID_1
            case 0x4800024:
            case 0x4800025: base -= 0x4800024; size = 2; core->wifi.writeWBssid(2, mask << (base * 8), data << (base * 8));          break; // W_BSSID_2
            case 0x480002A:
            case 0x480002B: base -= 0x480002A; size = 2; core->wifi.writeWAidFull(mask << (base * 8), data << (base * 8));           break; // W_AID_FULL
            case 0x480003C:
            case 0x480003D: base -= 0x480003C; size = 2; core->wifi.writeWPowerstate(mask << (base * 8), data << (base * 8));        break; // W_POWERSTATE
            case 0x4800040:
            case 0x4800041: base -= 0x4800040; size = 2; core->wifi.writeWPowerforce(mask << (base * 8), data << (base * 8));        break; // W_POWERFORCE
            case 0x4800050:
            case 0x4800051: base -= 0x4800050; size = 2; core->wifi.writeWRxbufBegin(mask << (base * 8), data << (base * 8));        break; // W_RXBUF_BEGIN
            case 0x4800052:
            case 0x4800053: base -= 0x4800052; size = 2; core->wifi.writeWRxbufEnd(mask << (base * 8), data << (base * 8));          break; // W_RXBUF_END
            case 0x4800056:
            case 0x4800057: base -= 0x4800056; size = 2; core->wifi.writeWRxbufWrAddr(mask << (base * 8), data << (base * 8));       break; // W_RXBUF_WR_ADDR
            case 0x4800058:
            case 0x4800059: base -= 0x4800058; size = 2; core->wifi.writeWRxbufRdAddr(mask << (base * 8), data << (base * 8));       break; // W_RXBUF_RD_ADDR
            case 0x480005A:
            case 0x480005B: base -= 0x480005A; size = 2; core->wifi.writeWRxbufReadcsr(mask << (base * 8), data << (base * 8));      break; // W_RXBUF_READCSR
            case 0x480005C:
            case 0x480005D: base -= 0x480005C; size = 2; core->wifi.writeWRxbufCount(mask << (base * 8), data << (base * 8));        break; // W_RXBUF_COUNT
            case 0x4800062:
            case 0x4800063: base -= 0x4800062; size = 2; core->wifi.writeWRxbufGap(mask << (base * 8), data << (base * 8));          break; // W_RXBUF_GAP
            case 0x4800064:
            case 0x4800065: base -= 0x4800064; size = 2; core->wifi.writeWRxbufGapdisp(mask << (base * 8), data << (base * 8));      break; // W_RXBUF_GAPDISP
            case 0x4800068:
            case 0x4800069: base -= 0x4800068; size = 2; core->wifi.writeWTxbufWrAddr(mask << (base * 8), data << (base * 8));       break; // W_TXBUF_WR_ADDR
            case 0x480006C:
            case 0x480006D: base -= 0x480006C; size = 2; core->wifi.writeWTxbufCount(mask << (base * 8), data << (base * 8));        break; // W_TXBUF_COUNT
            case 0x4800070:
            case 0x4800071: base -= 0x4800070; size = 2; core->wifi.writeWTxbufWrData(mask << (base * 8), data << (base * 8));       break; // W_TXBUF_WR_DATA
            case 0x4800074:
            case 0x4800075: base -= 0x4800074; size = 2; core->wifi.writeWTxbufGap(mask << (base * 8), data << (base * 8));          break; // W_TXBUF_GAP
            case 0x4800076:
            case 0x4800077: base -= 0x4800076; size = 2; core->wifi.writeWTxbufGapdisp(mask << (base * 8), data << (base * 8));      break; // W_TXBUF_GAPDISP
            case 0x4800120:
            case 0x4800121: base -= 0x4800120; size = 2; core->wifi.writeWConfig(0,  mask << (base * 8), data << (base * 8));        break; // W_CONFIG_120
            case 0x4800122:
            case 0x4800123: base -= 0x4800122; size = 2; core->wifi.writeWConfig(1,  mask << (base * 8), data << (base * 8));        break; // W_CONFIG_122
            case 0x4800124:
            case 0x4800125: base -= 0x4800124; size = 2; core->wifi.writeWConfig(2,  mask << (base * 8), data << (base * 8));        break; // W_CONFIG_124
            case 0x4800128:
            case 0x4800129: base -= 0x4800128; size = 2; core->wifi.writeWConfig(3,  mask << (base * 8), data << (base * 8));        break; // W_CONFIG_128
            case 0x4800130:
            case 0x4800131: base -= 0x4800130; size = 2; core->wifi.writeWConfig(4,  mask << (base * 8), data << (base * 8));        break; // W_CONFIG_130
            case 0x4800132:
            case 0x4800133: base -= 0x4800132; size = 2; core->wifi.writeWConfig(5,  mask << (base * 8), data << (base * 8));        break; // W_CONFIG_132
            case 0x4800134:
            case 0x4800135: base -= 0x4800134; size = 2; core->wifi.writeWBeaconcount2(mask << (base * 8), data << (base * 8));      break; // W_BEACONCOUNT2
            case 0x4800140:
            case 0x4800141: base -= 0x4800140; size = 2; core->wifi.writeWConfig(6,  mask << (base * 8), data << (base * 8));        break; // W_CONFIG_140
            case 0x4800142:
            case 0x4800143: base -= 0x4800142; size = 2; core->wifi.writeWConfig(7,  mask << (base * 8), data << (base * 8));        break; // W_CONFIG_142
            case 0x4800144:
            case 0x4800145: base -= 0x4800144; size = 2; core->wifi.writeWConfig(8,  mask << (base * 8), data << (base * 8));        break; // W_CONFIG_144
            case 0x4800146:
            case 0x4800147: base -= 0x4800146; size = 2; core->wifi.writeWConfig(9,  mask << (base * 8), data << (base * 8));        break; // W_CONFIG_146
            case 0x4800148:
            case 0x4800149: base -= 0x4800148; size = 2; core->wifi.writeWConfig(10, mask << (base * 8), data << (base * 8));        break; // W_CONFIG_148
            case 0x480014A:
            case 0x480014B: base -= 0x480014A; size = 2; core->wifi.writeWConfig(11, mask << (base * 8), data << (base * 8));        break; // W_CONFIG_14A
            case 0x480014C:
            case 0x480014D: base -= 0x480014C; size = 2; core->wifi.writeWConfig(12, mask << (base * 8), data << (base * 8));        break; // W_CONFIG_14C
            case 0x4800150:
            case 0x4800151: base -= 0x4800150; size = 2; core->wifi.writeWConfig(13, mask << (base * 8), data << (base * 8));        break; // W_CONFIG_150
            case 0x4800154:
            case 0x4800155: base -= 0x4800154; size = 2; core->wifi.writeWConfig(14, mask << (base * 8), data << (base * 8));        break; // W_CONFIG_154
            case 0x4800158:
            case 0x4800159: base -= 0x4800158; size = 2; core->wifi.writeWBbCnt(mask << (base * 8), data << (base * 8));             break; // W_BB_CNT
            case 0x480015A:
            case 0x480015B: base -= 0x480015A; size = 2; core->wifi.writeWBbWrite(mask << (base * 8), data << (base * 8));           break; // W_BB_WRITE
            case 0x480021C:
            case 0x480021D: base -= 0x480021C; size = 2; core->wifi.writeWIrfSet(mask << (base * 8), data << (base * 8));            break; // W_IF_SET

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

template <typename T> void Memory::ioWriteGba(uint32_t address, T value)
{
    unsigned int i = 0;

    // Write a value to one or more GBA I/O registers
    while (i < sizeof(T))
    {
        uint32_t base = address + i;
        unsigned int size;
        uint32_t mask = ((uint64_t)1 << ((sizeof(T) - i) * 8)) - 1;
        uint32_t data = value >> (i * 8);

        switch (base)
        {
            case 0x4000000:
            case 0x4000001: base -= 0x4000000; size = 2; core->gpu2D[0].writeDispCnt(mask << (base * 8), data << (base * 8));     break; // DISPCNT
            case 0x4000004:
            case 0x4000005: base -= 0x4000004; size = 2; core->gpu.writeDispStat(1, mask << (base * 8), data << (base * 8));      break; // DISPSTAT
            case 0x4000008:
            case 0x4000009: base -= 0x4000008; size = 2; core->gpu2D[0].writeBgCnt(0, mask << (base * 8), data << (base * 8));    break; // BG0CNT
            case 0x400000A:
            case 0x400000B: base -= 0x400000A; size = 2; core->gpu2D[0].writeBgCnt(1, mask << (base * 8), data << (base * 8));    break; // BG1CNT
            case 0x400000C:
            case 0x400000D: base -= 0x400000C; size = 2; core->gpu2D[0].writeBgCnt(2, mask << (base * 8), data << (base * 8));    break; // BG2CNT
            case 0x400000E:
            case 0x400000F: base -= 0x400000E; size = 2; core->gpu2D[0].writeBgCnt(3, mask << (base * 8), data << (base * 8));    break; // BG3CNT
            case 0x4000010:
            case 0x4000011: base -= 0x4000010; size = 2; core->gpu2D[0].writeBgHOfs(0, mask << (base * 8), data << (base * 8));   break; // BG0HOFS
            case 0x4000012:
            case 0x4000013: base -= 0x4000012; size = 2; core->gpu2D[0].writeBgVOfs(0, mask << (base * 8), data << (base * 8));   break; // BG0VOFS
            case 0x4000014:
            case 0x4000015: base -= 0x4000014; size = 2; core->gpu2D[0].writeBgHOfs(1, mask << (base * 8), data << (base * 8));   break; // BG1HOFS
            case 0x4000016:
            case 0x4000017: base -= 0x4000016; size = 2; core->gpu2D[0].writeBgVOfs(1, mask << (base * 8), data << (base * 8));   break; // BG1VOFS
            case 0x4000018:
            case 0x4000019: base -= 0x4000018; size = 2; core->gpu2D[0].writeBgHOfs(2, mask << (base * 8), data << (base * 8));   break; // BG2HOFS
            case 0x400001A:
            case 0x400001B: base -= 0x400001A; size = 2; core->gpu2D[0].writeBgVOfs(2, mask << (base * 8), data << (base * 8));   break; // BG2VOFS
            case 0x400001C:
            case 0x400001D: base -= 0x400001C; size = 2; core->gpu2D[0].writeBgHOfs(3, mask << (base * 8), data << (base * 8));   break; // BG3HOFS
            case 0x400001E:
            case 0x400001F: base -= 0x400001E; size = 2; core->gpu2D[0].writeBgVOfs(3, mask << (base * 8), data << (base * 8));   break; // BG3VOFS
            case 0x4000020:
            case 0x4000021: base -= 0x4000020; size = 2; core->gpu2D[0].writeBgPA(2, mask << (base * 8), data << (base * 8));     break; // BG2PA
            case 0x4000022:
            case 0x4000023: base -= 0x4000022; size = 2; core->gpu2D[0].writeBgPB(2, mask << (base * 8), data << (base * 8));     break; // BG2PB
            case 0x4000024:
            case 0x4000025: base -= 0x4000024; size = 2; core->gpu2D[0].writeBgPC(2, mask << (base * 8), data << (base * 8));     break; // BG2PC
            case 0x4000026:
            case 0x4000027: base -= 0x4000026; size = 2; core->gpu2D[0].writeBgPD(2, mask << (base * 8), data << (base * 8));     break; // BG2PD
            case 0x4000028:
            case 0x4000029:
            case 0x400002A:
            case 0x400002B: base -= 0x4000028; size = 4; core->gpu2D[0].writeBgX(2, mask << (base * 8), data << (base * 8));      break; // BG2X
            case 0x400002C:
            case 0x400002D:
            case 0x400002E:
            case 0x400002F: base -= 0x400002C; size = 4; core->gpu2D[0].writeBgY(2, mask << (base * 8), data << (base * 8));      break; // BG2Y
            case 0x4000030:
            case 0x4000031: base -= 0x4000030; size = 2; core->gpu2D[0].writeBgPA(3, mask << (base * 8), data << (base * 8));     break; // BG3PA
            case 0x4000032:
            case 0x4000033: base -= 0x4000032; size = 2; core->gpu2D[0].writeBgPB(3, mask << (base * 8), data << (base * 8));     break; // BG3PB
            case 0x4000034:
            case 0x4000035: base -= 0x4000034; size = 2; core->gpu2D[0].writeBgPC(3, mask << (base * 8), data << (base * 8));     break; // BG3PC
            case 0x4000036:
            case 0x4000037: base -= 0x4000036; size = 2; core->gpu2D[0].writeBgPD(3, mask << (base * 8), data << (base * 8));     break; // BG3PD
            case 0x4000038:
            case 0x4000039:
            case 0x400003A:
            case 0x400003B: base -= 0x4000038; size = 4; core->gpu2D[0].writeBgX(3, mask << (base * 8), data << (base * 8));      break; // BG3X
            case 0x400003C:
            case 0x400003D:
            case 0x400003E:
            case 0x400003F: base -= 0x400003C; size = 4; core->gpu2D[0].writeBgY(3, mask << (base * 8), data << (base * 8));      break; // BG3Y
            case 0x4000040:
            case 0x4000041: base -= 0x4000040; size = 2; core->gpu2D[0].writeWinH(0, mask << (base * 8), data << (base * 8));     break; // WIN0H
            case 0x4000042:
            case 0x4000043: base -= 0x4000042; size = 2; core->gpu2D[0].writeWinH(1, mask << (base * 8), data << (base * 8));     break; // WIN1H
            case 0x4000044:
            case 0x4000045: base -= 0x4000044; size = 2; core->gpu2D[0].writeWinV(0, mask << (base * 8), data << (base * 8));     break; // WIN0V
            case 0x4000046:
            case 0x4000047: base -= 0x4000046; size = 2; core->gpu2D[0].writeWinV(1, mask << (base * 8), data << (base * 8));     break; // WIN1V
            case 0x4000048:
            case 0x4000049: base -= 0x4000048; size = 2; core->gpu2D[0].writeWinIn(mask << (base * 8), data << (base * 8));       break; // WININ
            case 0x400004A:
            case 0x400004B: base -= 0x400004A; size = 2; core->gpu2D[0].writeWinOut(mask << (base * 8), data << (base * 8));      break; // WINOUT
            case 0x4000050:
            case 0x4000051: base -= 0x4000050; size = 2; core->gpu2D[0].writeBldCnt(mask << (base * 8), data << (base * 8));      break; // BLDCNT
            case 0x4000052:
            case 0x4000053: base -= 0x4000052; size = 2; core->gpu2D[0].writeBldAlpha(mask << (base * 8), data << (base * 8));    break; // BLDALPHA
            case 0x4000054: base -= 0x4000054; size = 1; core->gpu2D[0].writeBldY(data << (base * 8));                            break; // BLDY
            case 0x4000060:
            case 0x4000061: base -= 0x4000060; size = 2; core->spu.writeGbaSoundCntL(0, data << (base * 8));                      break; // SOUND0CNT_L
            case 0x4000062:
            case 0x4000063: base -= 0x4000062; size = 2; core->spu.writeGbaSoundCntH(0, mask << (base * 8), data << (base * 8));  break; // SOUND0CNT_H
            case 0x4000064:
            case 0x4000065: base -= 0x4000064; size = 2; core->spu.writeGbaSoundCntX(0, mask << (base * 8), data << (base * 8));  break; // SOUND0CNT_X
            case 0x4000068:
            case 0x4000069: base -= 0x4000068; size = 2; core->spu.writeGbaSoundCntH(1, mask << (base * 8), data << (base * 8));  break; // SOUND1CNT_H
            case 0x400006C:
            case 0x400006D: base -= 0x400006C; size = 2; core->spu.writeGbaSoundCntX(1, mask << (base * 8), data << (base * 8));  break; // SOUND1CNT_X
            case 0x4000070:
            case 0x4000071: base -= 0x4000070; size = 2; core->spu.writeGbaSoundCntL(2, data << (base * 8));                      break; // SOUND2CNT_L
            case 0x4000072:
            case 0x4000073: base -= 0x4000072; size = 2; core->spu.writeGbaSoundCntH(2, mask << (base * 8), data << (base * 8));  break; // SOUND2CNT_H
            case 0x4000074:
            case 0x4000075: base -= 0x4000074; size = 2; core->spu.writeGbaSoundCntX(2, mask << (base * 8), data << (base * 8));  break; // SOUND2CNT_X
            case 0x4000078:
            case 0x4000079: base -= 0x4000078; size = 2; core->spu.writeGbaSoundCntH(3, mask << (base * 8), data << (base * 8));  break; // SOUND3CNT_H
            case 0x400007C:
            case 0x400007D: base -= 0x400007C; size = 2; core->spu.writeGbaSoundCntX(3, mask << (base * 8), data << (base * 8));  break; // SOUND3CNT_X
            case 0x4000080:
            case 0x4000081: base -= 0x4000080; size = 2; core->spu.writeGbaMainSoundCntL(mask << (base * 8), data << (base * 8)); break; // SOUNDCNT_L
            case 0x4000082:
            case 0x4000083: base -= 0x4000082; size = 2; core->spu.writeGbaMainSoundCntH(mask << (base * 8), data << (base * 8)); break; // SOUNDCNT_H
            case 0x4000084:
            case 0x4000085: base -= 0x4000084; size = 2; core->spu.writeGbaMainSoundCntX(data << (base * 8));                     break; // SOUNDCNT_X
            case 0x4000088:
            case 0x4000089: base -= 0x4000088; size = 2; core->spu.writeGbaSoundBias(mask << (base * 8), data << (base * 8));     break; // SOUNDBIAS
            case 0x4000090: base -= 0x4000090; size = 1; core->spu.writeGbaWaveRam(0,  data << (base * 8));                       break; // WAVE_RAM
            case 0x4000091: base -= 0x4000091; size = 1; core->spu.writeGbaWaveRam(1,  data << (base * 8));                       break; // WAVE_RAM
            case 0x4000092: base -= 0x4000092; size = 1; core->spu.writeGbaWaveRam(2,  data << (base * 8));                       break; // WAVE_RAM
            case 0x4000093: base -= 0x4000093; size = 1; core->spu.writeGbaWaveRam(3,  data << (base * 8));                       break; // WAVE_RAM
            case 0x4000094: base -= 0x4000094; size = 1; core->spu.writeGbaWaveRam(4,  data << (base * 8));                       break; // WAVE_RAM
            case 0x4000095: base -= 0x4000095; size = 1; core->spu.writeGbaWaveRam(5,  data << (base * 8));                       break; // WAVE_RAM
            case 0x4000096: base -= 0x4000096; size = 1; core->spu.writeGbaWaveRam(6,  data << (base * 8));                       break; // WAVE_RAM
            case 0x4000097: base -= 0x4000097; size = 1; core->spu.writeGbaWaveRam(7,  data << (base * 8));                       break; // WAVE_RAM
            case 0x4000098: base -= 0x4000098; size = 1; core->spu.writeGbaWaveRam(8,  data << (base * 8));                       break; // WAVE_RAM
            case 0x4000099: base -= 0x4000099; size = 1; core->spu.writeGbaWaveRam(9,  data << (base * 8));                       break; // WAVE_RAM
            case 0x400009A: base -= 0x400009A; size = 1; core->spu.writeGbaWaveRam(10, data << (base * 8));                       break; // WAVE_RAM
            case 0x400009B: base -= 0x400009B; size = 1; core->spu.writeGbaWaveRam(11, data << (base * 8));                       break; // WAVE_RAM
            case 0x400009C: base -= 0x400009C; size = 1; core->spu.writeGbaWaveRam(12, data << (base * 8));                       break; // WAVE_RAM
            case 0x400009D: base -= 0x400009D; size = 1; core->spu.writeGbaWaveRam(13, data << (base * 8));                       break; // WAVE_RAM
            case 0x400009E: base -= 0x400009E; size = 1; core->spu.writeGbaWaveRam(14, data << (base * 8));                       break; // WAVE_RAM
            case 0x400009F: base -= 0x400009F; size = 1; core->spu.writeGbaWaveRam(15, data << (base * 8));                       break; // WAVE_RAM
            case 0x40000A0:
            case 0x40000A1:
            case 0x40000A2:
            case 0x40000A3: base -= 0x40000A0; size = 4; core->spu.writeGbaFifoA(mask << (base * 8), data << (base * 8));         break; // FIFO_A
            case 0x40000A4:
            case 0x40000A5:
            case 0x40000A6:
            case 0x40000A7: base -= 0x40000A4; size = 4; core->spu.writeGbaFifoB(mask << (base * 8), data << (base * 8));         break; // FIFO_B
            case 0x40000B0:
            case 0x40000B1:
            case 0x40000B2:
            case 0x40000B3: base -= 0x40000B0; size = 4; core->dma[1].writeDmaSad(0, mask << (base * 8), data << (base * 8));     break; // DMA0SAD
            case 0x40000B4:
            case 0x40000B5:
            case 0x40000B6:
            case 0x40000B7: base -= 0x40000B4; size = 4; core->dma[1].writeDmaDad(0, mask << (base * 8), data << (base * 8));     break; // DMA0DAD
            case 0x40000B8:
            case 0x40000B9:
            case 0x40000BA:
            case 0x40000BB: base -= 0x40000B8; size = 4; core->dma[1].writeDmaCnt(0, mask << (base * 8), data << (base * 8));     break; // DMA0CNT
            case 0x40000BC:
            case 0x40000BD:
            case 0x40000BE:
            case 0x40000BF: base -= 0x40000BC; size = 4; core->dma[1].writeDmaSad(1, mask << (base * 8), data << (base * 8));     break; // DMA1SAD
            case 0x40000C0:
            case 0x40000C1:
            case 0x40000C2:
            case 0x40000C3: base -= 0x40000C0; size = 4; core->dma[1].writeDmaDad(1, mask << (base * 8), data << (base * 8));     break; // DMA1DAD
            case 0x40000C4:
            case 0x40000C5:
            case 0x40000C6:
            case 0x40000C7: base -= 0x40000C4; size = 4; core->dma[1].writeDmaCnt(1, mask << (base * 8), data << (base * 8));     break; // DMA1CNT
            case 0x40000C8:
            case 0x40000C9:
            case 0x40000CA:
            case 0x40000CB: base -= 0x40000C8; size = 4; core->dma[1].writeDmaSad(2, mask << (base * 8), data << (base * 8));     break; // DMA2SAD
            case 0x40000CC:
            case 0x40000CD:
            case 0x40000CE:
            case 0x40000CF: base -= 0x40000CC; size = 4; core->dma[1].writeDmaDad(2, mask << (base * 8), data << (base * 8));     break; // DMA2DAD
            case 0x40000D0:
            case 0x40000D1:
            case 0x40000D2:
            case 0x40000D3: base -= 0x40000D0; size = 4; core->dma[1].writeDmaCnt(2, mask << (base * 8), data << (base * 8));     break; // DMA2CNT
            case 0x40000D4:
            case 0x40000D5:
            case 0x40000D6:
            case 0x40000D7: base -= 0x40000D4; size = 4; core->dma[1].writeDmaSad(3, mask << (base * 8), data << (base * 8));     break; // DMA3SAD
            case 0x40000D8:
            case 0x40000D9:
            case 0x40000DA:
            case 0x40000DB: base -= 0x40000D8; size = 4; core->dma[1].writeDmaDad(3, mask << (base * 8), data << (base * 8));     break; // DMA3DAD
            case 0x40000DC:
            case 0x40000DD:
            case 0x40000DE:
            case 0x40000DF: base -= 0x40000DC; size = 4; core->dma[1].writeDmaCnt(3, mask << (base * 8), data << (base * 8));     break; // DMA3CNT
            case 0x4000100:
            case 0x4000101: base -= 0x4000100; size = 2; core->timers[1].writeTmCntL(0, mask << (base * 8), data << (base * 8));  break; // TM0CNT_L
            case 0x4000102:
            case 0x4000103: base -= 0x4000102; size = 2; core->timers[1].writeTmCntH(0, mask << (base * 8), data << (base * 8));  break; // TM0CNT_H
            case 0x4000104:
            case 0x4000105: base -= 0x4000104; size = 2; core->timers[1].writeTmCntL(1, mask << (base * 8), data << (base * 8));  break; // TM1CNT_L
            case 0x4000106:
            case 0x4000107: base -= 0x4000106; size = 2; core->timers[1].writeTmCntH(1, mask << (base * 8), data << (base * 8));  break; // TM1CNT_H
            case 0x4000108:
            case 0x4000109: base -= 0x4000108; size = 2; core->timers[1].writeTmCntL(2, mask << (base * 8), data << (base * 8));  break; // TM2CNT_L
            case 0x400010A:
            case 0x400010B: base -= 0x400010A; size = 2; core->timers[1].writeTmCntH(2, mask << (base * 8), data << (base * 8));  break; // TM2CNT_H
            case 0x400010C:
            case 0x400010D: base -= 0x400010C; size = 2; core->timers[1].writeTmCntL(3, mask << (base * 8), data << (base * 8));  break; // TM3CNT_L
            case 0x400010E:
            case 0x400010F: base -= 0x400010E; size = 2; core->timers[1].writeTmCntH(3, mask << (base * 8), data << (base * 8));  break; // TM3CNT_H
            case 0x4000200:
            case 0x4000201: base -= 0x4000200; size = 2; core->interpreter[1].writeIe(mask << (base * 8), data << (base * 8));    break; // IE
            case 0x4000202:
            case 0x4000203: base -= 0x4000202; size = 2; core->interpreter[1].writeIrf(mask << (base * 8), data << (base * 8));   break; // IF
            case 0x4000208: base -= 0x4000208; size = 1; core->interpreter[1].writeIme(data << (base * 8));                       break; // IME
            case 0x4000300: base -= 0x4000300; size = 1; core->interpreter[1].writePostFlg(data << (base * 8));                   break; // POSTFLG
            case 0x4000301: base -= 0x4000301; size = 1; writeGbaHaltCnt(data << (base * 8));                                     break; // HALTCNT

            default:
            {
                // Catch unknown writes
                if (i == 0)
                {
                    printf("Unknown GBA I/O register write: 0x%X\n", address);
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

void Memory::writeDmaFill(int channel, uint32_t mask, uint32_t value)
{
    // Write to one of the DMAFILL registers
    dmaFill[channel] = (dmaFill[channel] & ~mask) | (value & mask);
}

void Memory::writeVramCnt(int index, uint8_t value)
{
    // Write to one of the VRAMCNT registers and invalidate the 3D if a parameter changed
    const uint8_t masks[] = { 0x9B, 0x9B, 0x9F, 0x9F, 0x87, 0x9F, 0x9F, 0x83, 0x83 };
    if ((value & masks[index]) != (vramCnt[index] & masks[index]))
    {
        vramCnt[index] = value & masks[index];
        core->gpu.invalidate3D();
    }

    // Clear the previous mappings
    memset(lcdc,       0, 64 * sizeof(uint8_t*));
    memset(engABg,     0, 32 * sizeof(uint8_t*));
    memset(engAObj,    0, 16 * sizeof(uint8_t*));
    memset(engBBg,     0,  8 * sizeof(uint8_t*));
    memset(engBObj,    0,  8 * sizeof(uint8_t*));
    memset(vram7,      0,  2 * sizeof(uint8_t*));
    memset(engAExtPal, 0,  5 * sizeof(uint8_t*));
    memset(engBExtPal, 0,  5 * sizeof(uint8_t*));
    memset(tex3D,      0,  4 * sizeof(uint8_t*));
    memset(pal3D,      0,  6 * sizeof(uint8_t*));
    vramStat = 0;

    // Remap VRAM block I
    if (vramCnt[8] & BIT(7)) // Enabled
    {
        switch (vramCnt[8] & 0x07) // MST
        {
            case 0:                             lcdc[40]                      = &vramI[0]; break; // LCDC
            case 1: for (int i = 0; i < 2; i++) engBBg[2 + i] = engBBg[6 + i] = &vramI[0]; break; // Engine B BG
            case 2: for (int i = 0; i < 8; i++) engBObj[i]                    = &vramI[0]; break; // Engine B OBJ
            case 3:                             engBExtPal[4]                 = &vramI[0]; break; // Engine B OBJ ext pal
        }
    }

    // Remap VRAM block H
    if (vramCnt[7] & BIT(7)) // Enabled
    {
        switch (vramCnt[7] & 0x07) // MST
        {
            case 0: for (int i = 0; i < 2; i++) lcdc[38 + i]              = &vramH[i << 14]; break; // LCDC
            case 1: for (int i = 0; i < 2; i++) engBBg[i] = engBBg[4 + i] = &vramH[i << 14]; break; // Engine B BG
            case 2: for (int i = 0; i < 4; i++) engBExtPal[i]             = &vramH[i << 13]; break; // Engine B BG ext pal
        }
    }

    // Map VRAM block G
    if (vramCnt[6] & BIT(7)) // Enabled
    {
        uint8_t ofs = (vramCnt[6] & 0x18) >> 3;
        switch (vramCnt[6] & 0x07) // MST
        {
            case 0:                             lcdc[37]                                         = &vramG[0];       break; // LCDC
            case 1: for (int i = 0; i < 2; i++) engABg[((ofs & 2) << 1) + (ofs & 1) + (i << 1)]  = &vramG[0];       break; // Engine A BG
            case 2: for (int i = 0; i < 2; i++) engAObj[((ofs & 2) << 1) + (ofs & 1) + (i << 1)] = &vramG[0];       break; // Engine A OBJ
            case 3:                             pal3D[((ofs & 2) << 1) + (ofs & 1)]              = &vramG[0];       break; // 3D palette
            case 4: for (int i = 0; i < 2; i++) engAExtPal[((ofs & 1) << 1) + i]                 = &vramG[i << 13]; break; // Engine A BG ext pal
            case 5:                             engAExtPal[4]                                    = &vramG[0];       break; // Engine A OBJ ext pal
        }
    }

    // Remap VRAM block F
    if (vramCnt[5] & BIT(7)) // Enabled
    {
        uint8_t ofs = (vramCnt[5] & 0x18) >> 3;
        switch (vramCnt[5] & 0x07) // MST
        {
            case 0:                             lcdc[36]                                         = &vramF[0];       break; // LCDC
            case 1: for (int i = 0; i < 2; i++) engABg[((ofs & 2) << 1) + (ofs & 1) + (i << 1)]  = &vramF[0];       break; // Engine A BG
            case 2: for (int i = 0; i < 2; i++) engAObj[((ofs & 2) << 1) + (ofs & 1) + (i << 1)] = &vramF[0];       break; // Engine A OBJ
            case 3:                             pal3D[((ofs & 2) << 1) + (ofs & 1)]              = &vramF[0];       break; // 3D palette
            case 4: for (int i = 0; i < 2; i++) engAExtPal[((ofs & 1) << 1) + i]                 = &vramF[i << 13]; break; // Engine A BG ext pal
            case 5:                             engAExtPal[4]                                    = &vramF[0];       break; // Engine A OBJ ext pal
        }
    }

    // Remap VRAM block E
    if (vramCnt[4] & BIT(7)) // Enabled
    {
        switch (vramCnt[4] & 0x07) // MST
        {
            case 0: for (int i = 0; i < 4; i++) lcdc[32 + i]  = &vramE[i << 14]; break; // LCDC
            case 1: for (int i = 0; i < 4; i++) engABg[i]     = &vramE[i << 14]; break; // Engine A BG
            case 2: for (int i = 0; i < 4; i++) engAObj[i]    = &vramE[i << 14]; break; // Engine A OBJ
            case 3: for (int i = 0; i < 4; i++) pal3D[i]      = &vramE[i << 14]; break; // 3D palette
            case 4: for (int i = 0; i < 4; i++) engAExtPal[i] = &vramE[i << 13]; break; // Engine A BG ext pal
        }
    }

    // Remap VRAM block D
    if (vramCnt[3] & BIT(7)) // Enabled
    {
        uint8_t ofs = (vramCnt[3] & 0x18) >> 3;
        switch (vramCnt[3] & 0x07) // MST
        {
            case 0: for (int i = 0; i < 8; i++) lcdc[24 + i]           = &vramD[i << 14]; break; // LCDC
            case 1: for (int i = 0; i < 8; i++) engABg[(ofs << 3) + i] = &vramD[i << 14]; break; // Engine A BG
            case 2: vramStat |= BIT(1);         vram7[ofs & BIT(0)]    = &vramD[0];       break; // ARM7
            case 3:                             tex3D[ofs]             = &vramD[0];       break; // 3D texture
            case 4: for (int i = 0; i < 8; i++) engBObj[i]             = &vramD[i << 14]; break; // Engine B OBJ
        }
    }

    // Remap VRAM block C
    if (vramCnt[2] & BIT(7)) // Enabled
    {
        uint8_t ofs = (vramCnt[2] & 0x18) >> 3;
        switch (vramCnt[2] & 0x07) // MST
        {
            case 0: for (int i = 0; i < 8; i++) lcdc[16 + i]           = &vramC[i << 14]; break; // LCDC
            case 1: for (int i = 0; i < 8; i++) engABg[(ofs << 3) + i] = &vramC[i << 14]; break; // Engine A BG
            case 2: vramStat |= BIT(0);         vram7[ofs & BIT(0)]    = &vramC[0];       break; // ARM7
            case 3:                             tex3D[ofs]             = &vramC[0];       break; // 3D texture
            case 4: for (int i = 0; i < 8; i++) engBBg[i]              = &vramC[i << 14]; break; // Engine B BG
        }
    }

    // Remap VRAM block B
    if (vramCnt[1] & BIT(7)) // Enabled
    {
        uint8_t ofs = (vramCnt[1] & 0x18) >> 3;
        switch (vramCnt[1] & 0x07) // MST
        {
            case 0: for (int i = 0; i < 8; i++) lcdc[8 + i]             = &vramB[i << 14]; break; // LCDC
            case 1: for (int i = 0; i < 8; i++) engABg[(ofs << 3) + i]  = &vramB[i << 14]; break; // Engine A BG
            case 2: for (int i = 0; i < 8; i++) engAObj[(ofs << 3) + i] = &vramB[i << 14]; break; // Engine A OBJ
            case 3:                             tex3D[ofs]              = &vramB[0];       break; // 3D texture
        }
    }

    // Remap VRAM block A
    if (vramCnt[0] & BIT(7)) // Enabled
    {
        uint8_t ofs = (vramCnt[0] & 0x18) >> 3;
        switch (vramCnt[0] & 0x07) // MST
        {
            case 0: for (int i = 0; i < 8; i++) lcdc[i]                 = &vramA[i << 14]; break; // LCDC
            case 1: for (int i = 0; i < 8; i++) engABg[(ofs << 3) + i]  = &vramA[i << 14]; break; // Engine A BG
            case 2: for (int i = 0; i < 8; i++) engAObj[(ofs << 3) + i] = &vramA[i << 14]; break; // Engine A OBJ
            case 3:                             tex3D[ofs]              = &vramA[0];       break; // 3D texture
        }
    }
}

void Memory::writeWramCnt(uint8_t value)
{
    // Write to the WRAMCNT register
    wramCnt = value & 0x03;
}

void Memory::writeHaltCnt(uint8_t value)
{
    // Write to the HALTCNT register
    haltCnt = value & 0xC0;

    // Change the ARM7's power mode
    switch (haltCnt >> 6)
    {
        case 1: // GBA
        {
            core->enterGbaMode(false);
            break;
        }

        case 2: // Halt
        {
            core->interpreter[1].halt();
            break;
        }

        case 3: // Sleep
        {
            printf("Unhandled request for sleep mode\n");
            break;
        }
    }
}

void Memory::writeGbaHaltCnt(uint8_t value)
{
    // Halt the CPU
    core->interpreter[1].halt();

    if (value & BIT(7)) // Stop
        printf("Unhandled request for stop mode\n");
}
