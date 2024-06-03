/*
    Copyright 2019-2024 Hydr8gon

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

// Defines an 8-bit register in an I/O switch statement
#define DEF_IO_8(addr, func) \
    case addr:               \
        base -= addr;        \
        size = 1;            \
        func;                \
        break;

// Defines a 16-bit register in an I/O switch statement
#define DEF_IO16(addr, func) \
    case addr:               \
    case addr + 1:           \
        base -= addr;        \
        size = 2;            \
        func;                \
        break;

// Defines a 32-bit register in an I/O switch statement
#define DEF_IO32(addr, func) \
    case addr:               \
    case addr + 1:           \
    case addr + 2:           \
    case addr + 3:           \
        base -= addr;        \
        size = 4;            \
        func;                \
        break;

// Defines shared parameters for I/O register writes
#define IOWR_PARAMS8 data << (base * 8)
#define IOWR_PARAMS  mask << (base * 8), data << (base * 8)

void VramMapping::add(uint8_t *mapping)
{
    // Add a VRAM mapping
    mappings[count++] = mapping;
}

template <typename T> T VramMapping::read(uint32_t address)
{
    T value = 0;

    // Read a value from all the VRAM mappings, ORed together
    for (int m = 0; m < count; m++)
    {
        for (unsigned int i = 0; i < sizeof(T); i++)
            value |= mappings[m][address + i] << (i * 8);
    }

    return value;
}

template <typename T> void VramMapping::write(uint32_t address, T value)
{
    // Write a value to all the VRAM mappings
    for (int m = 0; m < count; m++)
    {
        for (unsigned int i = 0; i < sizeof(T); i++)
            mappings[m][address + i] = value >> (i * 8);
    }
}

void Memory::saveState(FILE *file)
{
    // Write state data to the file
    fwrite(ram, 1, sizeof(ram), file);
    fwrite(wram, 1, sizeof(wram), file);
    fwrite(instrTcm, 1, sizeof(instrTcm), file);
    fwrite(dataTcm, 1, sizeof(dataTcm), file);
    fwrite(wram7, 1, sizeof(wram7), file);
    fwrite(wifiRam, 1, sizeof(wifiRam), file);
    fwrite(palette, 1, sizeof(palette), file);
    fwrite(vramA, 1, sizeof(vramA), file);
    fwrite(vramB, 1, sizeof(vramB), file);
    fwrite(vramC, 1, sizeof(vramC), file);
    fwrite(vramD, 1, sizeof(vramD), file);
    fwrite(vramE, 1, sizeof(vramE), file);
    fwrite(vramF, 1, sizeof(vramF), file);
    fwrite(vramG, 1, sizeof(vramG), file);
    fwrite(vramH, 1, sizeof(vramH), file);
    fwrite(vramI, 1, sizeof(vramI), file);
    fwrite(oam, 1, sizeof(oam), file);
    fwrite(&gbaBiosAddr, sizeof(gbaBiosAddr), 1, file);
    fwrite(dmaFill, 4, sizeof(dmaFill) / 4, file);
    fwrite(vramCnt, 1, sizeof(vramCnt), file);
    fwrite(&wramCnt, sizeof(wramCnt), 1, file);
    fwrite(&haltCnt, sizeof(haltCnt), 1, file);
}

void Memory::loadState(FILE *file)
{
    // Read state data from the file
    fread(ram, 1, sizeof(ram), file);
    fread(wram, 1, sizeof(wram), file);
    fread(instrTcm, 1, sizeof(instrTcm), file);
    fread(dataTcm, 1, sizeof(dataTcm), file);
    fread(wram7, 1, sizeof(wram7), file);
    fread(wifiRam, 1, sizeof(wifiRam), file);
    fread(palette, 1, sizeof(palette), file);
    fread(vramA, 1, sizeof(vramA), file);
    fread(vramB, 1, sizeof(vramB), file);
    fread(vramC, 1, sizeof(vramC), file);
    fread(vramD, 1, sizeof(vramD), file);
    fread(vramE, 1, sizeof(vramE), file);
    fread(vramF, 1, sizeof(vramF), file);
    fread(vramG, 1, sizeof(vramG), file);
    fread(vramH, 1, sizeof(vramH), file);
    fread(vramI, 1, sizeof(vramI), file);
    fread(oam, 1, sizeof(oam), file);
    fread(&gbaBiosAddr, sizeof(gbaBiosAddr), 1, file);
    fread(dmaFill, 4, sizeof(dmaFill) / 4, file);
    fread(vramCnt, 1, sizeof(vramCnt), file);
    fread(&wramCnt, sizeof(wramCnt), 1, file);
    fread(&haltCnt, sizeof(haltCnt), 1, file);

    // Update mapped memory
    updateMap9<false>(0x00000000, 0xFFFFFFFF);
    updateMap9<true>(0x00000000, 0xFFFFFFFF);
    updateMap7(0x00000000, 0xFFFFFFFF);
    updateVram();
}

bool Memory::loadBios9()
{
    // Load the ARM9 BIOS if the file is found
    if (FILE *file = fopen(Settings::bios9Path.c_str(), "rb"))
    {
        fread(bios9, sizeof(uint8_t), 0x1000, file);
        fclose(file);
        return true;
    }

    // Prepare HLE BIOS with a special opcode for interrupt return
    bios9[3] = 0xFF;
    core->interpreter[0].setBios(&core->bios[0]);
    return false;
}

bool Memory::loadBios7()
{
    // Load the ARM7 BIOS if the file is found
    if (FILE *file = fopen(Settings::bios7Path.c_str(), "rb"))
    {
        fread(bios7, sizeof(uint8_t), 0x4000, file);
        fclose(file);
        return true;
    }

    // Prepare HLE BIOS with a special opcode for interrupt return
    bios7[3] = 0xFF;
    core->interpreter[1].setBios(&core->bios[1]);
    return false;
}

bool Memory::loadGbaBios()
{
    // Load the GBA BIOS if the file is found
    if (FILE *file = fopen(Settings::gbaBiosPath.c_str(), "rb"))
    {
        fread(gbaBios, sizeof(uint8_t), 0x4000, file);
        fclose(file);
        return true;
    }

    // Prepare HLE BIOS with a special opcode for interrupt return
    gbaBios[3] = 0xFF;
    return false;
}

void Memory::copyBiosLogo(uint8_t *logo)
{
    // Copy logo data to HLE BIOS so GBA ROMs can be verified
    if (bios9[3] == 0xFF)
        memcpy(&bios9[0x20], logo, 0x9C);
}

template <bool tcm> void Memory::updateMap9(uint32_t start, uint32_t end)
{
    // Update the ARM9 read and write memory maps in the given range
    for (uint64_t address = start; address < end; address += 0x1000)
    {
        // Some components can't access TCM, so there are TCM and non-TCM maps
        uint8_t *&read = (tcm ? readMap9A : readMap9B)[address >> 12];
        uint8_t *&write = (tcm ? writeMap9A : writeMap9B)[address >> 12];
        read = write = nullptr;

        // Map a 4KB block to the corresponding ARM9 memory, excluding special cases
        switch (address & 0xFF000000)
        {
            case 0x02000000: // Main RAM
                read = write = &ram[address & 0x3FFFFF];
                break;

            case 0x03000000: // Shared WRAM
                switch (wramCnt)
                {
                    case 0: read = write = &wram[(address & 0x7FFF)];          break;
                    case 1: read = write = &wram[(address & 0x3FFF) + 0x4000]; break;
                    case 2: read = write = &wram[(address & 0x3FFF)];          break;
                }
                break;

            case 0x06000000: // VRAM
            {
                VramMapping *mapping;
                switch (address & 0xFFE00000)
                {
                    case 0x06000000: mapping =  &engABg[(address & 0x7FFFF) >> 14]; break;
                    case 0x06200000: mapping =  &engBBg[(address & 0x1FFFF) >> 14]; break;
                    case 0x06400000: mapping = &engAObj[(address & 0x3FFFF) >> 14]; break;
                    case 0x06600000: mapping = &engBObj[(address & 0x1FFFF) >> 14]; break;
                    default:         mapping =    &lcdc[(address & 0xFFFFF) >> 14]; break;
                }
                if (mapping->getCount() == 1)
                    read = write = &mapping->getBaseMapping()[address & 0x3FFF];
                break;
            }

            case 0x08000000: case 0x09000000: // GBA ROM
                read = core->cartridgeGba.getRom(address);
                break;

            case 0xFF000000: // ARM9 BIOS
                if ((address & 0xFFFF8000) == 0xFFFF0000)
                    read = &bios9[address & 0xFFFF];
                break;
        }

        if (tcm)
        {
            // Map TCM on top of the standard memory layout
            if (address < core->cp15.getItcmSize()) // Instruction TCM
            {
                if (core->cp15.getItcmReadEnabled())
                    read = &instrTcm[address & 0x7FFF];
                if (core->cp15.getItcmWriteEnabled())
                    write = &instrTcm[address & 0x7FFF];
            }
            else if (address >= core->cp15.getDtcmAddr() && // Data TCM
                address < core->cp15.getDtcmAddr() + core->cp15.getDtcmSize())
            {
                if (core->cp15.getDtcmReadEnabled())
                    read = &dataTcm[(address - core->cp15.getDtcmAddr()) & 0x3FFF];
                if (core->cp15.getDtcmWriteEnabled())
                    write = &dataTcm[(address - core->cp15.getDtcmAddr()) & 0x3FFF];
            }
        }
    }

    // For non-TCM updates, update the TCM map as well
    if (!tcm)
        updateMap9<true>(start, end);
}

void Memory::updateMap7(uint32_t start, uint32_t end)
{
    // Update the ARM7 read and write memory maps in the given range
    for (uint64_t address = start; address < end; address += 0x1000)
    {
        uint8_t *&read = readMap7[address >> 12];
        uint8_t *&write = writeMap7[address >> 12];
        read = write = nullptr;

        if (core->gbaMode) // GBA
        {
            // Map a 4KB block to the corresponding GBA memory, excluding special cases
            switch (address & 0xFF000000)
            {
                case 0x02000000: // On-board WRAM
                    read = write = &ram[address & 0x3FFFF];
                    break;

                case 0x03000000: // On-chip WRAM
                    read = write = &wram[address & 0x7FFF];
                    break;

                case 0x06000000: // VRAM
                    read = write = &vramC[address & ((address & 0x10000) ? 0x17FFF : 0xFFFF)];
                    break;

                case 0x08000000: case 0x09000000: case 0x0A000000:
                case 0x0B000000: case 0x0C000000: // ROM
                    if (address > 0x8000000 || !core->rtc.readGpControl()) // GPIO fallback
                        read = core->cartridgeGba.getRom(address);
                    break;
            }
        }
        else // ARM7
        {
            // Map a 4KB block to the corresponding ARM7 memory, excluding special cases
            switch (address & 0xFF000000)
            {
                case 0x00000000: // ARM7 BIOS
                    if (address < 0x4000)
                        read = &bios7[address];
                    break;

                case 0x02000000: // Main RAM
                    read = write = &ram[address & 0x3FFFFF];
                    break;

                case 0x03000000: // WRAM
                    if (!(address & 0x00800000)) // Shared WRAM
                    {
                        switch (wramCnt)
                        {
                            case 1: read = write = &wram[(address & 0x3FFF)];          break;
                            case 2: read = write = &wram[(address & 0x3FFF) + 0x4000]; break;
                            case 3: read = write = &wram[(address & 0x7FFF)];          break;
                        }
                    }
                    if (!read)
                        read = write = &wram7[address & 0xFFFF]; // ARM7 WRAM
                    break;

                case 0x04000000: // I/O registers
                    if (address & 0x00800000) // WiFi regions
                    {
                        uint32_t addr = address & ~0x00008000; // Mirror
                        if (addr >= 0x04804000 && addr < 0x04806000) // WiFi RAM
                            read = write = &wifiRam[addr & 0x1FFF];
                    }
                    break;

                case 0x06000000: // VRAM
                {
                    VramMapping *mapping = &vram7[(address & 0x3FFFF) >> 17];
                    if (mapping->getCount() == 1)
                        read = write = &mapping->getBaseMapping()[address & 0x1FFFF];
                    break;
                }

                case 0x08000000: case 0x09000000: // GBA ROM
                    read = core->cartridgeGba.getRom(address);
                    break;
            }
        }
    }
}

void Memory::updateVram()
{
    // Clear the previous VRAM mappings
    memset(engABg, 0, sizeof(engABg));
    memset(engBBg, 0, sizeof(engBBg));
    memset(engAObj, 0, sizeof(engAObj));
    memset(engBObj, 0, sizeof(engBObj));
    memset(lcdc, 0, sizeof(lcdc));
    memset(vram7, 0, sizeof(vram7));
    memset(engAExtPal, 0, sizeof(engAExtPal));
    memset(engBExtPal, 0, sizeof(engBExtPal));
    memset(tex3D, 0, sizeof(tex3D));
    memset(pal3D, 0, sizeof(pal3D));
    vramStat = 0;

    // Remap VRAM block A
    if (vramCnt[0] & BIT(7)) // Enabled
    {
        uint8_t ofs = (vramCnt[0] & 0x18) >> 3;
        switch (vramCnt[0] & 0x07) // MST
        {
            case 0: // LCDC
                for (int i = 0; i < 8; i++)
                    lcdc[i].add(&vramA[i << 14]);
                break;

            case 1: // Engine A BG
                for (int i = 0; i < 8; i++)
                    engABg[(ofs << 3) + i].add(&vramA[i << 14]);
                break;

            case 2: // Engine A OBJ
                for (int i = 0; i < 8; i++)
                    engAObj[(ofs << 3) + i].add(&vramA[i << 14]);
                break;

            case 3: // 3D texture
                tex3D[ofs] = &vramA[0];
                break;
        }
    }

    // Remap VRAM block B
    if (vramCnt[1] & BIT(7)) // Enabled
    {
        uint8_t ofs = (vramCnt[1] & 0x18) >> 3;
        switch (vramCnt[1] & 0x07) // MST
        {
            case 0: // LCDC
                for (int i = 0; i < 8; i++)
                    lcdc[8 + i].add(&vramB[i << 14]);
                break;

            case 1: // Engine A BG
                for (int i = 0; i < 8; i++)
                    engABg[(ofs << 3) + i].add(&vramB[i << 14]);
                break;

            case 2: // Engine A OBJ
                for (int i = 0; i < 8; i++)
                    engAObj[(ofs << 3) + i].add(&vramB[i << 14]);
                break;

            case 3: // 3D texture
                tex3D[ofs] = &vramB[0];
                break;
        }
    }

    // Remap VRAM block C
    if (vramCnt[2] & BIT(7)) // Enabled
    {
        uint8_t ofs = (vramCnt[2] & 0x18) >> 3;
        switch (vramCnt[2] & 0x07) // MST
        {
            case 0: // LCDC
                for (int i = 0; i < 8; i++)
                    lcdc[16 + i].add(&vramC[i << 14]);
                break;

            case 1: // Engine A BG
                for (int i = 0; i < 8; i++)
                    engABg[(ofs << 3) + i].add(&vramC[i << 14]);
                break;

            case 2: // ARM7
                vram7[ofs & BIT(0)].add(&vramC[0]);
                vramStat |= BIT(0);
                break;

            case 3: // 3D texture
                tex3D[ofs] = &vramC[0];
                break;

            case 4: // Engine B BG
                for (int i = 0; i < 8; i++)
                    engBBg[i].add(&vramC[i << 14]);
                break;
        }
    }

    // Remap VRAM block D
    if (vramCnt[3] & BIT(7)) // Enabled
    {
        uint8_t ofs = (vramCnt[3] & 0x18) >> 3;
        switch (vramCnt[3] & 0x07) // MST
        {
            case 0: // LCDC
                for (int i = 0; i < 8; i++)
                    lcdc[24 + i].add(&vramD[i << 14]);
                break;

            case 1: // Engine A BG
                for (int i = 0; i < 8; i++)
                    engABg[(ofs << 3) + i].add(&vramD[i << 14]);
                break;

            case 2: // ARM7
                vram7[ofs & BIT(0)].add(&vramD[0]);
                vramStat |= BIT(1);
                break;

            case 3: // 3D texture
                tex3D[ofs] = &vramD[0];
                break;

            case 4: // Engine B OBJ
                for (int i = 0; i < 8; i++)
                    engBObj[i].add(&vramD[i << 14]);
                break;
        }
    }

    // Remap VRAM block E
    if (vramCnt[4] & BIT(7)) // Enabled
    {
        switch (vramCnt[4] & 0x07) // MST
        {
            case 0: // LCDC
                for (int i = 0; i < 4; i++)
                    lcdc[32 + i].add(&vramE[i << 14]);
                break;

            case 1: // Engine A BG
                for (int i = 0; i < 4; i++)
                    engABg[i].add(&vramE[i << 14]);
                break;

            case 2: // Engine A OBJ
                for (int i = 0; i < 4; i++)
                    engAObj[i].add(&vramE[i << 14]);
                break;

            case 3: // 3D palette
                for (int i = 0; i < 4; i++)
                    pal3D[i] = &vramE[i << 14];
                break;

            case 4: // Engine A BG ext pal
                for (int i = 0; i < 4; i++)
                    engAExtPal[i] = &vramE[i << 13];
                break;
        }
    }

    // Remap VRAM block F
    if (vramCnt[5] & BIT(7)) // Enabled
    {
        uint8_t ofs = (vramCnt[5] & 0x18) >> 3;
        switch (vramCnt[5] & 0x07) // MST
        {
            case 0: // LCDC
                lcdc[36].add(&vramF[0]);
                break;

            case 1: // Engine A BG
                for (int i = 0; i < 2; i++)
                    engABg[((ofs & 2) << 1) + (ofs & 1) + (i << 1)].add(&vramF[0]);
                break;

            case 2: // Engine A OBJ
                for (int i = 0; i < 2; i++)
                    engAObj[((ofs & 2) << 1) + (ofs & 1) + (i << 1)].add(&vramF[0]);
                break;

            case 3: // 3D palette
                pal3D[((ofs & 2) << 1) + (ofs & 1)] = &vramF[0];
                break;

            case 4: // Engine A BG ext pal
                for (int i = 0; i < 2; i++)
                    engAExtPal[((ofs & 1) << 1) + i] = &vramF[i << 13];
                break;

            case 5: // Engine A OBJ ext pal
                engAExtPal[4] = &vramF[0];
                break;
        }
    }

    // Remap VRAM block G
    if (vramCnt[6] & BIT(7)) // Enabled
    {
        uint8_t ofs = (vramCnt[6] & 0x18) >> 3;
        switch (vramCnt[6] & 0x07) // MST
        {
            case 0: // LCDC
                lcdc[37].add(&vramG[0]);
                break;

            case 1: // Engine A BG
                for (int i = 0; i < 2; i++)
                    engABg[((ofs & 2) << 1) + (ofs & 1) + (i << 1)].add(&vramG[0]);
                break;

            case 2: // Engine A OBJ
                for (int i = 0; i < 2; i++)
                    engAObj[((ofs & 2) << 1) + (ofs & 1) + (i << 1)].add(&vramG[0]);
                break;

            case 3: // 3D palette
                pal3D[((ofs & 2) << 1) + (ofs & 1)] = &vramG[0];
                break;

            case 4: // Engine A BG ext pal
                for (int i = 0; i < 2; i++)
                    engAExtPal[((ofs & 1) << 1) + i] = &vramG[i << 13];
                break;

            case 5: // Engine A OBJ ext pal
                engAExtPal[4] = &vramG[0];
                break;
        }
    }

    // Remap VRAM block H
    if (vramCnt[7] & BIT(7)) // Enabled
    {
        switch (vramCnt[7] & 0x07) // MST
        {
            case 0: // LCDC
                for (int i = 0; i < 2; i++)
                    lcdc[38 + i].add(&vramH[i << 14]);
                break;

            case 1: // Engine B BG
                for (int i = 0; i < 2; i++)
                {
                    engBBg[0 + i].add(&vramH[i << 14]);
                    engBBg[4 + i].add(&vramH[i << 14]);
                }
                break;

            case 2: // Engine B BG ext pal
                for (int i = 0; i < 4; i++)
                    engBExtPal[i] = &vramH[i << 13];
                break;
        }
    }

    // Remap VRAM block I
    if (vramCnt[8] & BIT(7)) // Enabled
    {
        switch (vramCnt[8] & 0x07) // MST
        {
            case 0: // LCDC
                lcdc[40].add(&vramI[0]);
                break;

            case 1: // Engine B BG
                for (int i = 0; i < 2; i++)
                {
                    engBBg[2 + i].add(&vramI[0]);
                    engBBg[6 + i].add(&vramI[0]);
                }
                break;

            case 2: // Engine B OBJ
                for (int i = 0; i < 8; i++)
                    engBObj[i].add(&vramI[0]);
                break;

            case 3: // Engine B OBJ ext pal
                engBExtPal[4] = &vramI[0];
                break;
        }
    }

    // Update the memory maps at the VRAM locations
    updateMap9<false>(0x06000000, 0x07000000);
    updateMap7(0x06000000, 0x07000000);
    core->gpu.invalidate3D();
}

template <typename T> T Memory::readFallback(bool cpu, uint32_t address)
{
    uint8_t *data = nullptr;

    // Handle special memory reads that can't be done with the read map
    // This includes I/O registers, overlapping VRAM, and areas smaller than 4KB
    if (cpu == 0) // ARM9
    {
        switch (address & 0xFF000000)
        {
            case 0x04000000: // I/O registers
                return ioRead9<T>(address);

            case 0x05000000: // Palettes
                data = &palette[address & 0x7FF];
                break;

            case 0x06000000: // VRAM
            {
                VramMapping *mapping;
                switch (address & 0xFFE00000)
                {
                    case 0x06000000: mapping =  &engABg[(address & 0x7FFFF) >> 14]; break;
                    case 0x06200000: mapping =  &engBBg[(address & 0x1FFFF) >> 14]; break;
                    case 0x06400000: mapping = &engAObj[(address & 0x3FFFF) >> 14]; break;
                    case 0x06600000: mapping = &engBObj[(address & 0x1FFFF) >> 14]; break;
                    default:         mapping =    &lcdc[(address & 0xFFFFF) >> 14]; break;
                }
                if (mapping->getCount() == 0) break;
                return mapping->read<T>(address & 0x3FFF);
            }

            case 0x07000000: // OAM
                data = &oam[address & 0x7FF];
                break;

            case 0x08000000: case 0x09000000: // GBA ROM (empty)
                return (T)0xFFFFFFFF;

            case 0x0A000000: // GBA SRAM
                return core->cartridgeGba.sramRead(address + 0x4000000);
        }
    }
    else if (core->gbaMode) // GBA
    {
        switch (address & 0xFF000000)
        {
            case 0x00000000: // GBA BIOS (only readable when executing; otherwise returns last value)
                if (address < 0x4000)
                    data = &gbaBios[(core->interpreter[1].getPC() < 0x4000) ? (gbaBiosAddr = address) : gbaBiosAddr];
                break;

            case 0x04000000: // I/O registers
                return ioReadGba<T>(address);

            case 0x05000000: // Palettes
                data = &palette[address & 0x3FF];
                break;

            case 0x07000000: // OAM
                data = &oam[address & 0x3FF];
                break;

            case 0x0D000000: // EEPROM/ROM
                if (core->cartridgeGba.isEeprom(address))
                    return core->cartridgeGba.eepromRead();

            case 0x08000000: case 0x09000000: case 0x0A000000:
            case 0x0B000000: case 0x0C000000: // GPIO/ROM
                if (address >= 0x80000C4 && address < 0x80000CA)
                    return ioReadGba<T>(address);
                if (data = core->cartridgeGba.getRom(address))
                    break;
                return (T)0xFFFFFFFF;

            case 0x0E000000: // SRAM
                return core->cartridgeGba.sramRead(address);
        }
    }
    else // ARM7
    {
        switch (address & 0xFF000000)
        {
            case 0x04000000: // I/O registers
                return ioRead7<T>(address);

            case 0x06000000: // VRAM
            {
                VramMapping *mapping = &vram7[(address & 0x3FFFF) >> 17];
                if (mapping->getCount() == 0) break;
                return mapping->read<T>(address & 0x1FFFF);
            }

            case 0x08000000: case 0x09000000: // GBA ROM (empty)
                return (T)0xFFFFFFFF;

            case 0x0A000000: // GBA SRAM
                return core->cartridgeGba.sramRead(address + 0x4000000);
        }
    }

    if (data)
    {
        // Form an LSB-first value from the data at the pointer
        T value = 0;
        for (size_t i = 0; i < sizeof(T); i++)
            value |= data[i] << (i * 8);
        return value;
    }

    if (!core->gbaMode)
    {
        LOG("Unmapped ARM%d memory read: 0x%X\n", ((cpu == 0) ? 9 : 7), address);
        return 0;
    }
    else
    {
        LOG("Unmapped GBA memory read: 0x%X\n", address);
        if (address == core->interpreter[1].getPC()) return 0;
        return read<T>(cpu, core->interpreter[1].getPC()); // Open bus (last prefetched opcode)
    }
}

template <typename T> void Memory::writeFallback(bool cpu, uint32_t address, T value)
{
    uint8_t *data = nullptr;

    // Handle special memory writes that can't be done with the write map
    // This includes I/O registers, overlapping VRAM, and areas smaller than 4KB
    if (cpu == 0) // ARM9
    {
        switch (address & 0xFF000000)
        {
            case 0x04000000: // I/O registers
                ioWrite9<T>(address, value);
                return;

            case 0x05000000: // Palettes
                data = &palette[address & 0x7FF];
                break;

            case 0x06000000: // VRAM
            {
                VramMapping *mapping;
                switch (address & 0xFFE00000)
                {
                    case 0x06000000: mapping =  &engABg[(address & 0x7FFFF) >> 14]; break;
                    case 0x06200000: mapping =  &engBBg[(address & 0x1FFFF) >> 14]; break;
                    case 0x06400000: mapping = &engAObj[(address & 0x3FFFF) >> 14]; break;
                    case 0x06600000: mapping = &engBObj[(address & 0x1FFFF) >> 14]; break;
                    default:         mapping =    &lcdc[(address & 0xFFFFF) >> 14]; break;
                }
                if (mapping->getCount() == 0) break;
                mapping->write<T>(address & 0x3FFF, value);
                return;
            }

            case 0x07000000: // OAM
                data = &oam[address & 0x7FF];
                break;

            case 0x0A000000: // GBA SRAM
                core->cartridgeGba.sramWrite(address + 0x4000000, value);
                return;
        }
    }
    else if (core->gbaMode) // GBA
    {
        switch (address & 0xFF000000)
        {
            case 0x04000000: // I/O registers
                ioWriteGba<T>(address, value);
                return;

            case 0x05000000: // Palettes
                data = &palette[address & 0x3FF];
                break;

            case 0x07000000: // OAM
                data = &oam[address & 0x3FF];
                break;

            case 0x08000000: // GPIO
                if (address >= 0x80000C4 && address < 0x80000CA)
                    return ioWriteGba<T>(address, value);
                break;

            case 0x0D000000: // EEPROM
                if (core->cartridgeGba.isEeprom(address))
                    return core->cartridgeGba.eepromWrite(value);
                break;

            case 0x0E000000: // SRAM
                core->cartridgeGba.sramWrite(address, value);
                return;
        }
    }
    else // ARM7
    {
        switch (address & 0xFF000000)
        {
            case 0x04000000: // I/O registers
                ioWrite7<T>(address, value);
                return;

            case 0x06000000: // VRAM
            {
                VramMapping *mapping = &vram7[(address & 0x3FFFF) >> 17];
                if (mapping->getCount() == 0) break;
                mapping->write<T>(address & 0x1FFFF, value);
                return;
            }

            case 0x0A000000: // GBA SRAM
                core->cartridgeGba.sramWrite(address + 0x4000000, value);
                return;
        }
    }

    if (data)
    {
        // Write an LSB-first value to the data at the pointer
        for (size_t i = 0; i < sizeof(T); i++)
            data[i] = value >> (i * 8);
        return;
    }

    if (!core->gbaMode)
        LOG("Unmapped ARM%d memory write: 0x%X\n", ((cpu == 0) ? 9 : 7), address);
    else
        LOG("Unmapped GBA memory write: 0x%X\n", address);
}

template <typename T> T Memory::ioRead9(uint32_t address)
{
    T value = 0;
    size_t i = 0;

    // Read a value from one or more ARM9 I/O registers
    while (i < sizeof(T))
    {
        uint32_t base = address + i;
        size_t size;
        uint32_t data;

        switch (base)
        {
            DEF_IO32(0x4000000, data = core->gpu2D[0].readDispCnt())         // DISPCNT (engine A)
            DEF_IO16(0x4000004, data = core->gpu.readDispStat(0))            // DISPCNT (engine A)
            DEF_IO16(0x4000006, data = core->gpu.readVCount())               // VCOUNT
            DEF_IO16(0x4000008, data = core->gpu2D[0].readBgCnt(0))          // BG0CNT (engine A)
            DEF_IO16(0x400000A, data = core->gpu2D[0].readBgCnt(1))          // BG1CNT (engine A)
            DEF_IO16(0x400000C, data = core->gpu2D[0].readBgCnt(2))          // BG2CNT (engine A)
            DEF_IO16(0x400000E, data = core->gpu2D[0].readBgCnt(3))          // BG3CNT (engine A)
            DEF_IO16(0x4000048, data = core->gpu2D[0].readWinIn())           // WININ (engine A)
            DEF_IO16(0x400004A, data = core->gpu2D[0].readWinOut())          // WINOUT (engine A)
            DEF_IO16(0x4000050, data = core->gpu2D[0].readBldCnt())          // BLDCNT (engine A)
            DEF_IO16(0x4000052, data = core->gpu2D[0].readBldAlpha())        // BLDALPHA (engine A)
            DEF_IO16(0x4000060, data = core->gpu3DRenderer.readDisp3DCnt())  // DISP3DCNT
            DEF_IO32(0x4000064, data = core->gpu.readDispCapCnt())           // DISPCAPCNT
            DEF_IO16(0x400006C, data = core->gpu2D[0].readMasterBright())    // MASTER_BRIGHT (engine A)
            DEF_IO32(0x40000B0, data = core->dma[0].readDmaSad(0))           // DMA0SAD (ARM9)
            DEF_IO32(0x40000B4, data = core->dma[0].readDmaDad(0))           // DMA0DAD (ARM9)
            DEF_IO32(0x40000B8, data = core->dma[0].readDmaCnt(0))           // DMA0CNT (ARM9)
            DEF_IO32(0x40000BC, data = core->dma[0].readDmaSad(1))           // DMA1SAD (ARM9)
            DEF_IO32(0x40000C0, data = core->dma[0].readDmaDad(1))           // DMA1DAD (ARM9)
            DEF_IO32(0x40000C4, data = core->dma[0].readDmaCnt(1))           // DMA1CNT (ARM9)
            DEF_IO32(0x40000C8, data = core->dma[0].readDmaSad(2))           // DMA2SAD (ARM9)
            DEF_IO32(0x40000CC, data = core->dma[0].readDmaDad(2))           // DMA2DAD (ARM9)
            DEF_IO32(0x40000D0, data = core->dma[0].readDmaCnt(2))           // DMA2CNT (ARM9)
            DEF_IO32(0x40000D4, data = core->dma[0].readDmaSad(3))           // DMA3SAD (ARM9)
            DEF_IO32(0x40000D8, data = core->dma[0].readDmaDad(3))           // DMA3DAD (ARM9)
            DEF_IO32(0x40000DC, data = core->dma[0].readDmaCnt(3))           // DMA3CNT (ARM9)
            DEF_IO32(0x40000E0, data = readDmaFill(0))                       // DMA0FILL
            DEF_IO32(0x40000E4, data = readDmaFill(1))                       // DMA1FILL
            DEF_IO32(0x40000E8, data = readDmaFill(2))                       // DMA2FILL
            DEF_IO32(0x40000EC, data = readDmaFill(3))                       // DMA3FILL
            DEF_IO16(0x4000100, data = core->timers[0].readTmCntL(0))        // TM0CNT_L (ARM9)
            DEF_IO16(0x4000102, data = core->timers[0].readTmCntH(0))        // TM0CNT_H (ARM9)
            DEF_IO16(0x4000104, data = core->timers[0].readTmCntL(1))        // TM1CNT_L (ARM9)
            DEF_IO16(0x4000106, data = core->timers[0].readTmCntH(1))        // TM1CNT_H (ARM9)
            DEF_IO16(0x4000108, data = core->timers[0].readTmCntL(2))        // TM2CNT_L (ARM9)
            DEF_IO16(0x400010A, data = core->timers[0].readTmCntH(2))        // TM2CNT_H (ARM9)
            DEF_IO16(0x400010C, data = core->timers[0].readTmCntL(3))        // TM3CNT_L (ARM9)
            DEF_IO16(0x400010E, data = core->timers[0].readTmCntH(3))        // TM3CNT_H (ARM9)
            DEF_IO16(0x4000130, data = core->input.readKeyInput())           // KEYINPUT
            DEF_IO16(0x4000180, data = core->ipc.readIpcSync(0))             // IPCSYNC (ARM9)
            DEF_IO16(0x4000184, data = core->ipc.readIpcFifoCnt(0))          // IPCFIFOCNT (ARM9)
            DEF_IO16(0x40001A0, data = core->cartridgeNds.readAuxSpiCnt(0))  // AUXSPICNT (ARM9)
            DEF_IO_8(0x40001A2, data = core->cartridgeNds.readAuxSpiData(0)) // AUXSPIDATA (ARM9)
            DEF_IO32(0x40001A4, data = core->cartridgeNds.readRomCtrl(0))    // ROMCTRL (ARM9)
            DEF_IO_8(0x4000208, data = core->interpreter[0].readIme())       // IME (ARM9)
            DEF_IO32(0x4000210, data = core->interpreter[0].readIe())        // IE (ARM9)
            DEF_IO32(0x4000214, data = core->interpreter[0].readIrf())       // IF (ARM9)
            DEF_IO_8(0x4000240, data = readVramCnt(0))                       // VRAMCNT_A
            DEF_IO_8(0x4000241, data = readVramCnt(1))                       // VRAMCNT_B
            DEF_IO_8(0x4000242, data = readVramCnt(2))                       // VRAMCNT_C
            DEF_IO_8(0x4000243, data = readVramCnt(3))                       // VRAMCNT_D
            DEF_IO_8(0x4000244, data = readVramCnt(4))                       // VRAMCNT_E
            DEF_IO_8(0x4000245, data = readVramCnt(5))                       // VRAMCNT_F
            DEF_IO_8(0x4000246, data = readVramCnt(6))                       // VRAMCNT_G
            DEF_IO_8(0x4000247, data = readWramCnt())                        // WRAMCNT
            DEF_IO_8(0x4000248, data = readVramCnt(7))                       // VRAMCNT_H
            DEF_IO_8(0x4000249, data = readVramCnt(8))                       // VRAMCNT_I
            DEF_IO16(0x4000280, data = core->divSqrt.readDivCnt())           // DIVCNT
            DEF_IO32(0x4000290, data = core->divSqrt.readDivNumerL())        // DIVNUMER_L
            DEF_IO32(0x4000294, data = core->divSqrt.readDivNumerH())        // DIVNUMER_H
            DEF_IO32(0x4000298, data = core->divSqrt.readDivDenomL())        // DIVDENOM_L
            DEF_IO32(0x400029C, data = core->divSqrt.readDivDenomH())        // DIVDENOM_H
            DEF_IO32(0x40002A0, data = core->divSqrt.readDivResultL())       // DIVRESULT_L
            DEF_IO32(0x40002A4, data = core->divSqrt.readDivResultH())       // DIVRESULT_H
            DEF_IO32(0x40002A8, data = core->divSqrt.readDivRemResultL())    // DIVREMRESULT_L
            DEF_IO32(0x40002AC, data = core->divSqrt.readDivRemResultH())    // DIVREMRESULT_H
            DEF_IO16(0x40002B0, data = core->divSqrt.readSqrtCnt())          // SQRTCNT
            DEF_IO32(0x40002B4, data = core->divSqrt.readSqrtResult())       // SQRTRESULT
            DEF_IO32(0x40002B8, data = core->divSqrt.readSqrtParamL())       // SQRTPARAM_L
            DEF_IO32(0x40002BC, data = core->divSqrt.readSqrtParamH())       // SQRTPARAM_H
            DEF_IO_8(0x4000300, data = core->interpreter[0].readPostFlg())   // POSTFLG (ARM9)
            DEF_IO16(0x4000304, data = core->gpu.readPowCnt1())              // POWCNT1
            DEF_IO32(0x4000600, data = core->gpu3D.readGxStat())             // GXSTAT
            DEF_IO32(0x4000604, data = core->gpu3D.readRamCount())           // RAM_COUNT
            DEF_IO32(0x4000620, data = core->gpu3D.readPosResult(0))         // POS_RESULT
            DEF_IO32(0x4000624, data = core->gpu3D.readPosResult(1))         // POS_RESULT
            DEF_IO32(0x4000628, data = core->gpu3D.readPosResult(2))         // POS_RESULT
            DEF_IO32(0x400062C, data = core->gpu3D.readPosResult(3))         // POS_RESULT
            DEF_IO16(0x4000630, data = core->gpu3D.readVecResult(0))         // VEC_RESULT
            DEF_IO16(0x4000632, data = core->gpu3D.readVecResult(1))         // VEC_RESULT
            DEF_IO16(0x4000634, data = core->gpu3D.readVecResult(2))         // VEC_RESULT
            DEF_IO32(0x4000640, data = core->gpu3D.readClipMtxResult(0))     // CLIPMTX_RESULT
            DEF_IO32(0x4000644, data = core->gpu3D.readClipMtxResult(1))     // CLIPMTX_RESULT
            DEF_IO32(0x4000648, data = core->gpu3D.readClipMtxResult(2))     // CLIPMTX_RESULT
            DEF_IO32(0x400064C, data = core->gpu3D.readClipMtxResult(3))     // CLIPMTX_RESULT
            DEF_IO32(0x4000650, data = core->gpu3D.readClipMtxResult(4))     // CLIPMTX_RESULT
            DEF_IO32(0x4000654, data = core->gpu3D.readClipMtxResult(5))     // CLIPMTX_RESULT
            DEF_IO32(0x4000658, data = core->gpu3D.readClipMtxResult(6))     // CLIPMTX_RESULT
            DEF_IO32(0x400065C, data = core->gpu3D.readClipMtxResult(7))     // CLIPMTX_RESULT
            DEF_IO32(0x4000660, data = core->gpu3D.readClipMtxResult(8))     // CLIPMTX_RESULT
            DEF_IO32(0x4000664, data = core->gpu3D.readClipMtxResult(9))     // CLIPMTX_RESULT
            DEF_IO32(0x4000668, data = core->gpu3D.readClipMtxResult(10))    // CLIPMTX_RESULT
            DEF_IO32(0x400066C, data = core->gpu3D.readClipMtxResult(11))    // CLIPMTX_RESULT
            DEF_IO32(0x4000670, data = core->gpu3D.readClipMtxResult(12))    // CLIPMTX_RESULT
            DEF_IO32(0x4000674, data = core->gpu3D.readClipMtxResult(13))    // CLIPMTX_RESULT
            DEF_IO32(0x4000678, data = core->gpu3D.readClipMtxResult(14))    // CLIPMTX_RESULT
            DEF_IO32(0x400067C, data = core->gpu3D.readClipMtxResult(15))    // CLIPMTX_RESULT
            DEF_IO32(0x4000680, data = core->gpu3D.readVecMtxResult(0))      // VECMTX_RESULT
            DEF_IO32(0x4000684, data = core->gpu3D.readVecMtxResult(1))      // VECMTX_RESULT
            DEF_IO32(0x4000688, data = core->gpu3D.readVecMtxResult(2))      // VECMTX_RESULT
            DEF_IO32(0x400068C, data = core->gpu3D.readVecMtxResult(3))      // VECMTX_RESULT
            DEF_IO32(0x4000690, data = core->gpu3D.readVecMtxResult(4))      // VECMTX_RESULT
            DEF_IO32(0x4000694, data = core->gpu3D.readVecMtxResult(5))      // VECMTX_RESULT
            DEF_IO32(0x4000698, data = core->gpu3D.readVecMtxResult(6))      // VECMTX_RESULT
            DEF_IO32(0x400069C, data = core->gpu3D.readVecMtxResult(7))      // VECMTX_RESULT
            DEF_IO32(0x40006A0, data = core->gpu3D.readVecMtxResult(8))      // VECMTX_RESULT
            DEF_IO32(0x4001000, data = core->gpu2D[1].readDispCnt())         // DISPCNT (engine B)
            DEF_IO16(0x4001008, data = core->gpu2D[1].readBgCnt(0))          // BG0CNT (engine B)
            DEF_IO16(0x400100A, data = core->gpu2D[1].readBgCnt(1))          // BG1CNT (engine B)
            DEF_IO16(0x400100C, data = core->gpu2D[1].readBgCnt(2))          // BG2CNT (engine B)
            DEF_IO16(0x400100E, data = core->gpu2D[1].readBgCnt(3))          // BG3CNT (engine B)
            DEF_IO16(0x4001048, data = core->gpu2D[1].readWinIn())           // WININ (engine B)
            DEF_IO16(0x400104A, data = core->gpu2D[1].readWinOut())          // WINOUT (engine B)
            DEF_IO16(0x4001050, data = core->gpu2D[1].readBldCnt())          // BLDCNT (engine B)
            DEF_IO16(0x4001052, data = core->gpu2D[1].readBldAlpha())        // BLDALPHA (engine B)
            DEF_IO16(0x400106C, data = core->gpu2D[1].readMasterBright())    // MASTER_BRIGHT (engine B)
            DEF_IO32(0x4100000, data = core->ipc.readIpcFifoRecv(0))         // IPCFIFORECV (ARM9)
            DEF_IO32(0x4100010, data = core->cartridgeNds.readRomDataIn(0))  // ROMDATAIN (ARM9)

            default:
                // Handle unknown reads by returning 0
                if (i == 0)
                {
                    LOG("Unknown ARM9 I/O register read: 0x%X\n", address);
                    return 0;
                }

                // Ignore unknown reads if they occur after the first byte
                // This is in case, for example, a 16-bit register is accessed with a 32-bit read
                i++;
                continue;
        }

        value |= (data >> (base * 8)) << (i * 8);
        i += size - base;
    }

    return value;
}

template <typename T> T Memory::ioRead7(uint32_t address)
{
    // Mirror the WiFi regions
    if (address >= 0x04808000 && address < 0x04810000)
        address &= ~0x00008000;

    T value = 0;
    size_t i = 0;

    // Read a value from one or more ARM7 I/O registers
    while (i < sizeof(T))
    {
        uint32_t base = address + i;
        size_t size;
        uint32_t data;

        switch (base)
        {
            DEF_IO16(0x4000004, data = core->gpu.readDispStat(1))            // DISPSTAT (ARM7)
            DEF_IO16(0x4000006, data = core->gpu.readVCount())               // VCOUNT
            DEF_IO32(0x40000B0, data = core->dma[1].readDmaSad(0))           // DMA0SAD (ARM7)
            DEF_IO32(0x40000B4, data = core->dma[1].readDmaDad(0))           // DMA0DAD (ARM7)
            DEF_IO32(0x40000B8, data = core->dma[1].readDmaCnt(0))           // DMA0CNT (ARM7)
            DEF_IO32(0x40000BC, data = core->dma[1].readDmaSad(1))           // DMA1SAD (ARM7)
            DEF_IO32(0x40000C0, data = core->dma[1].readDmaDad(1))           // DMA1DAD (ARM7)
            DEF_IO32(0x40000C4, data = core->dma[1].readDmaCnt(1))           // DMA1CNT (ARM7)
            DEF_IO32(0x40000C8, data = core->dma[1].readDmaSad(2))           // DMA2SAD (ARM7)
            DEF_IO32(0x40000CC, data = core->dma[1].readDmaDad(2))           // DMA2DAD (ARM7)
            DEF_IO32(0x40000D0, data = core->dma[1].readDmaCnt(2))           // DMA2CNT (ARM7)
            DEF_IO32(0x40000D4, data = core->dma[1].readDmaSad(3))           // DMA3SAD (ARM7)
            DEF_IO32(0x40000D8, data = core->dma[1].readDmaDad(3))           // DMA3DAD (ARM7)
            DEF_IO32(0x40000DC, data = core->dma[1].readDmaCnt(3))           // DMA3CNT (ARM7)
            DEF_IO16(0x4000100, data = core->timers[1].readTmCntL(0))        // TM0CNT_L (ARM7)
            DEF_IO16(0x4000102, data = core->timers[1].readTmCntH(0))        // TM0CNT_H (ARM7)
            DEF_IO16(0x4000104, data = core->timers[1].readTmCntL(1))        // TM1CNT_L (ARM7)
            DEF_IO16(0x4000106, data = core->timers[1].readTmCntH(1))        // TM1CNT_H (ARM7)
            DEF_IO16(0x4000108, data = core->timers[1].readTmCntL(2))        // TM2CNT_L (ARM7)
            DEF_IO16(0x400010A, data = core->timers[1].readTmCntH(2))        // TM2CNT_H (ARM7)
            DEF_IO16(0x400010C, data = core->timers[1].readTmCntL(3))        // TM3CNT_L (ARM7)
            DEF_IO16(0x400010E, data = core->timers[1].readTmCntH(3))        // TM3CNT_H (ARM7)
            DEF_IO16(0x4000130, data = core->input.readKeyInput())           // KEYINPUT
            DEF_IO16(0x4000136, data = core->input.readExtKeyIn())           // EXTKEYIN
            DEF_IO_8(0x4000138, data = core->rtc.readRtc())                  // RTC
            DEF_IO16(0x4000180, data = core->ipc.readIpcSync(1))             // IPCSYNC (ARM7)
            DEF_IO16(0x4000184, data = core->ipc.readIpcFifoCnt(1))          // IPCFIFOCNT (ARM7)
            DEF_IO16(0x40001A0, data = core->cartridgeNds.readAuxSpiCnt(1))  // AUXSPICNT (ARM7)
            DEF_IO_8(0x40001A2, data = core->cartridgeNds.readAuxSpiData(1)) // AUXSPIDATA (ARM7)
            DEF_IO32(0x40001A4, data = core->cartridgeNds.readRomCtrl(1))    // ROMCTRL (ARM7)
            DEF_IO16(0x40001C0, data = core->spi.readSpiCnt())               // SPICNT
            DEF_IO_8(0x40001C2, data = core->spi.readSpiData())              // SPIDATA
            DEF_IO_8(0x4000208, data = core->interpreter[1].readIme())       // IME (ARM7)
            DEF_IO32(0x4000210, data = core->interpreter[1].readIe())        // IE (ARM7)
            DEF_IO32(0x4000214, data = core->interpreter[1].readIrf())       // IF (ARM7)
            DEF_IO_8(0x4000240, data = readVramStat())                       // VRAMSTAT
            DEF_IO_8(0x4000241, data = readWramCnt())                        // WRAMSTAT
            DEF_IO_8(0x4000300, data = core->interpreter[1].readPostFlg())   // POSTFLG (ARM7)
            DEF_IO_8(0x4000301, data = readHaltCnt())                        // HALTCNT
            DEF_IO32(0x4000400, data = core->spu.readSoundCnt(0))            // SOUND0CNT
            DEF_IO32(0x4000410, data = core->spu.readSoundCnt(1))            // SOUND1CNT
            DEF_IO32(0x4000420, data = core->spu.readSoundCnt(2))            // SOUND2CNT
            DEF_IO32(0x4000430, data = core->spu.readSoundCnt(3))            // SOUND3CNT
            DEF_IO32(0x4000440, data = core->spu.readSoundCnt(4))            // SOUND4CNT
            DEF_IO32(0x4000450, data = core->spu.readSoundCnt(5))            // SOUND5CNT
            DEF_IO32(0x4000460, data = core->spu.readSoundCnt(6))            // SOUND6CNT
            DEF_IO32(0x4000470, data = core->spu.readSoundCnt(7))            // SOUND7CNT
            DEF_IO32(0x4000480, data = core->spu.readSoundCnt(8))            // SOUND8CNT
            DEF_IO32(0x4000490, data = core->spu.readSoundCnt(9))            // SOUND9CNT
            DEF_IO32(0x40004A0, data = core->spu.readSoundCnt(10))           // SOUND10CNT
            DEF_IO32(0x40004B0, data = core->spu.readSoundCnt(11))           // SOUND11CNT
            DEF_IO32(0x40004C0, data = core->spu.readSoundCnt(12))           // SOUND12CNT
            DEF_IO32(0x40004D0, data = core->spu.readSoundCnt(13))           // SOUND13CNT
            DEF_IO32(0x40004E0, data = core->spu.readSoundCnt(14))           // SOUND14CNT
            DEF_IO32(0x40004F0, data = core->spu.readSoundCnt(15))           // SOUND15CNT
            DEF_IO16(0x4000500, data = core->spu.readMainSoundCnt())         // SOUNDCNT
            DEF_IO16(0x4000504, data = core->spu.readSoundBias())            // SOUNDBIAS
            DEF_IO_8(0x4000508, data = core->spu.readSndCapCnt(0))           // SNDCAP0CNT
            DEF_IO_8(0x4000509, data = core->spu.readSndCapCnt(1))           // SNDCAP1CNT
            DEF_IO32(0x4000510, data = core->spu.readSndCapDad(0))           // SNDCAP0DAD
            DEF_IO32(0x4000518, data = core->spu.readSndCapDad(1))           // SNDCAP1DAD
            DEF_IO32(0x4100000, data = core->ipc.readIpcFifoRecv(1))         // IPCFIFORECV (ARM7)
            DEF_IO32(0x4100010, data = core->cartridgeNds.readRomDataIn(1))  // ROMDATAIN (ARM7)
            DEF_IO16(0x4800006, data = core->wifi.readWModeWep())            // W_MODE_WEP
            DEF_IO16(0x4800010, data = core->wifi.readWIrf())                // W_IF
            DEF_IO16(0x4800012, data = core->wifi.readWIe())                 // W_IE
            DEF_IO16(0x4800018, data = core->wifi.readWMacaddr(0))           // W_MACADDR_0
            DEF_IO16(0x480001A, data = core->wifi.readWMacaddr(1))           // W_MACADDR_1
            DEF_IO16(0x480001C, data = core->wifi.readWMacaddr(2))           // W_MACADDR_2
            DEF_IO16(0x4800020, data = core->wifi.readWBssid(0))             // W_BSSID_0
            DEF_IO16(0x4800022, data = core->wifi.readWBssid(1))             // W_BSSID_1
            DEF_IO16(0x4800024, data = core->wifi.readWBssid(2))             // W_BSSID_2
            DEF_IO16(0x480002A, data = core->wifi.readWAidFull())            // W_AID_FULL
            DEF_IO16(0x4800030, data = core->wifi.readWRxcnt())              // W_RXCNT
            DEF_IO16(0x480003C, data = core->wifi.readWPowerstate())         // W_POWERSTATE
            DEF_IO16(0x4800040, data = core->wifi.readWPowerforce())         // W_POWERFORCE
            DEF_IO16(0x4800050, data = core->wifi.readWRxbufBegin())         // W_RXBUF_BEGIN
            DEF_IO16(0x4800052, data = core->wifi.readWRxbufEnd())           // W_RXBUF_END
            DEF_IO16(0x4800054, data = core->wifi.readWRxbufWrcsr())         // W_RXBUF_WRCSR
            DEF_IO16(0x4800056, data = core->wifi.readWRxbufWrAddr())        // W_RXBUF_WR_ADDR
            DEF_IO16(0x4800058, data = core->wifi.readWRxbufRdAddr())        // W_RXBUF_RD_ADDR
            DEF_IO16(0x480005A, data = core->wifi.readWRxbufReadcsr())       // W_RXBUF_READCSR
            DEF_IO16(0x480005C, data = core->wifi.readWRxbufCount())         // W_RXBUF_COUNT
            DEF_IO16(0x4800060, data = core->wifi.readWRxbufRdData())        // W_RXBUF_RD_DATA
            DEF_IO16(0x4800062, data = core->wifi.readWRxbufGap())           // W_RXBUF_GAP
            DEF_IO16(0x4800064, data = core->wifi.readWRxbufGapdisp())       // W_RXBUF_GAPDISP
            DEF_IO16(0x4800068, data = core->wifi.readWTxbufWrAddr())        // W_TXBUF_WR_ADDR
            DEF_IO16(0x480006C, data = core->wifi.readWTxbufCount())         // W_TXBUF_COUNT
            DEF_IO16(0x4800074, data = core->wifi.readWTxbufGap())           // W_TXBUF_GAP
            DEF_IO16(0x4800076, data = core->wifi.readWTxbufGapdisp())       // W_TXBUF_GAPDISP
            DEF_IO16(0x4800080, data = core->wifi.readWTxbufLoc(4))          // W_TXBUF_BEACON
            DEF_IO16(0x480008C, data = core->wifi.readWBeaconInt())          // W_BEACON_INT
            DEF_IO16(0x4800090, data = core->wifi.readWTxbufLoc(1))          // W_TXBUF_CMD
            DEF_IO16(0x48000A0, data = core->wifi.readWTxbufLoc(0))          // W_TXBUF_LOC1
            DEF_IO16(0x48000A4, data = core->wifi.readWTxbufLoc(2))          // W_TXBUF_LOC2
            DEF_IO16(0x48000A8, data = core->wifi.readWTxbufLoc(3))          // W_TXBUF_LOC3
            DEF_IO16(0x48000B0, data = core->wifi.readWTxreqRead())          // W_TXREQ_READ
            DEF_IO16(0x48000E8, data = core->wifi.readWUsCountcnt())         // W_US_COUNTCNT
            DEF_IO16(0x48000EA, data = core->wifi.readWUsComparecnt())       // W_US_COMPARECNT
            DEF_IO16(0x4800110, data = core->wifi.readWPreBeacon())          // W_PRE_BEACON
            DEF_IO16(0x480011C, data = core->wifi.readWBeaconCount())        // W_BEACON_COUNT
            DEF_IO16(0x4800120, data = core->wifi.readWConfig(0))            // W_CONFIG_120
            DEF_IO16(0x4800122, data = core->wifi.readWConfig(1))            // W_CONFIG_122
            DEF_IO16(0x4800124, data = core->wifi.readWConfig(2))            // W_CONFIG_124
            DEF_IO16(0x4800128, data = core->wifi.readWConfig(3))            // W_CONFIG_128
            DEF_IO16(0x4800130, data = core->wifi.readWConfig(4))            // W_CONFIG_130
            DEF_IO16(0x4800132, data = core->wifi.readWConfig(5))            // W_CONFIG_132
            DEF_IO16(0x4800134, data = core->wifi.readWPostBeacon())         // W_POST_BEACON
            DEF_IO16(0x4800140, data = core->wifi.readWConfig(6))            // W_CONFIG_140
            DEF_IO16(0x4800142, data = core->wifi.readWConfig(7))            // W_CONFIG_142
            DEF_IO16(0x4800144, data = core->wifi.readWConfig(8))            // W_CONFIG_144
            DEF_IO16(0x4800146, data = core->wifi.readWConfig(9))            // W_CONFIG_146
            DEF_IO16(0x4800148, data = core->wifi.readWConfig(10))           // W_CONFIG_148
            DEF_IO16(0x480014A, data = core->wifi.readWConfig(11))           // W_CONFIG_14A
            DEF_IO16(0x480014C, data = core->wifi.readWConfig(12))           // W_CONFIG_14C
            DEF_IO16(0x4800150, data = core->wifi.readWConfig(13))           // W_CONFIG_150
            DEF_IO16(0x4800154, data = core->wifi.readWConfig(14))           // W_CONFIG_154
            DEF_IO16(0x480015C, data = core->wifi.readWBbRead())             // W_BB_READ

            default:
                // Handle unknown reads by returning 0
                if (i == 0)
                {
                    LOG("Unknown ARM7 I/O register read: 0x%X\n", address);
                    return 0;
                }

                // Ignore unknown reads if they occur after the first byte
                // This is in case, for example, a 16-bit register is accessed with a 32-bit read
                i++;
                continue;
        }

        value |= (data >> (base * 8)) << (i * 8);
        i += size - base;
    }

    return value;
}

template <typename T> T Memory::ioReadGba(uint32_t address)
{
    T value = 0;
    size_t i = 0;

    // Read a value from one or more GBA I/O registers
    while (i < sizeof(T))
    {
        uint32_t base = address + i;
        size_t size;
        uint32_t data;

        switch (base)
        {
            DEF_IO16(0x4000000, data = core->gpu2D[0].readDispCnt())       // DISPCNT
            DEF_IO16(0x4000004, data = core->gpu.readDispStat(1))          // DISPSTAT
            DEF_IO16(0x4000006, data = core->gpu.readVCount())             // VCOUNT
            DEF_IO16(0x4000008, data = core->gpu2D[0].readBgCnt(0))        // BG0CNT
            DEF_IO16(0x400000A, data = core->gpu2D[0].readBgCnt(1))        // BG1CNT
            DEF_IO16(0x400000C, data = core->gpu2D[0].readBgCnt(2))        // BG2CNT
            DEF_IO16(0x400000E, data = core->gpu2D[0].readBgCnt(3))        // BG3CNT
            DEF_IO16(0x4000048, data = core->gpu2D[0].readWinIn())         // WININ
            DEF_IO16(0x400004A, data = core->gpu2D[0].readWinOut())        // WINOUT
            DEF_IO16(0x4000050, data = core->gpu2D[0].readBldCnt())        // BLDCNT
            DEF_IO16(0x4000052, data = core->gpu2D[0].readBldAlpha())      // BLDALPHA
            DEF_IO16(0x4000060, data = core->spu.readGbaSoundCntL(0))      // SOUND0CNT_L
            DEF_IO16(0x4000062, data = core->spu.readGbaSoundCntH(0))      // SOUND0CNT_H
            DEF_IO16(0x4000064, data = core->spu.readGbaSoundCntX(0))      // SOUND0CNT_X
            DEF_IO16(0x4000068, data = core->spu.readGbaSoundCntH(1))      // SOUND1CNT_H
            DEF_IO16(0x400006C, data = core->spu.readGbaSoundCntX(1))      // SOUND1CNT_X
            DEF_IO16(0x4000070, data = core->spu.readGbaSoundCntL(2))      // SOUND2CNT_L
            DEF_IO16(0x4000072, data = core->spu.readGbaSoundCntH(2))      // SOUND2CNT_H
            DEF_IO16(0x4000074, data = core->spu.readGbaSoundCntX(2))      // SOUND2CNT_X
            DEF_IO16(0x4000078, data = core->spu.readGbaSoundCntH(3))      // SOUND3CNT_H
            DEF_IO16(0x400007C, data = core->spu.readGbaSoundCntX(3))      // SOUND3CNT_X
            DEF_IO16(0x4000080, data = core->spu.readGbaMainSoundCntL())   // SOUNDCNT_L
            DEF_IO16(0x4000082, data = core->spu.readGbaMainSoundCntH())   // SOUNDCNT_H
            DEF_IO16(0x4000084, data = core->spu.readGbaMainSoundCntX())   // SOUNDCNT_X
            DEF_IO16(0x4000088, data = core->spu.readGbaSoundBias())       // SOUNDBIAS
            DEF_IO_8(0x4000090, data = core->spu.readGbaWaveRam(0))        // WAVE_RAM
            DEF_IO_8(0x4000091, data = core->spu.readGbaWaveRam(1))        // WAVE_RAM
            DEF_IO_8(0x4000092, data = core->spu.readGbaWaveRam(2))        // WAVE_RAM
            DEF_IO_8(0x4000093, data = core->spu.readGbaWaveRam(3))        // WAVE_RAM
            DEF_IO_8(0x4000094, data = core->spu.readGbaWaveRam(4))        // WAVE_RAM
            DEF_IO_8(0x4000095, data = core->spu.readGbaWaveRam(5))        // WAVE_RAM
            DEF_IO_8(0x4000096, data = core->spu.readGbaWaveRam(6))        // WAVE_RAM
            DEF_IO_8(0x4000097, data = core->spu.readGbaWaveRam(7))        // WAVE_RAM
            DEF_IO_8(0x4000098, data = core->spu.readGbaWaveRam(8))        // WAVE_RAM
            DEF_IO_8(0x4000099, data = core->spu.readGbaWaveRam(9))        // WAVE_RAM
            DEF_IO_8(0x400009A, data = core->spu.readGbaWaveRam(10))       // WAVE_RAM
            DEF_IO_8(0x400009B, data = core->spu.readGbaWaveRam(11))       // WAVE_RAM
            DEF_IO_8(0x400009C, data = core->spu.readGbaWaveRam(12))       // WAVE_RAM
            DEF_IO_8(0x400009D, data = core->spu.readGbaWaveRam(13))       // WAVE_RAM
            DEF_IO_8(0x400009E, data = core->spu.readGbaWaveRam(14))       // WAVE_RAM
            DEF_IO_8(0x400009F, data = core->spu.readGbaWaveRam(15))       // WAVE_RAM
            DEF_IO32(0x40000B8, data = core->dma[1].readDmaCnt(0))         // DMA0CNT
            DEF_IO32(0x40000C4, data = core->dma[1].readDmaCnt(1))         // DMA1CNT
            DEF_IO32(0x40000D0, data = core->dma[1].readDmaCnt(2))         // DMA2CNT
            DEF_IO32(0x40000DC, data = core->dma[1].readDmaCnt(3))         // DMA3CNT
            DEF_IO16(0x4000100, data = core->timers[1].readTmCntL(0))      // TM0CNT_L
            DEF_IO16(0x4000102, data = core->timers[1].readTmCntH(0))      // TM0CNT_H
            DEF_IO16(0x4000104, data = core->timers[1].readTmCntL(1))      // TM1CNT_L
            DEF_IO16(0x4000106, data = core->timers[1].readTmCntH(1))      // TM1CNT_H
            DEF_IO16(0x4000108, data = core->timers[1].readTmCntL(2))      // TM2CNT_L
            DEF_IO16(0x400010A, data = core->timers[1].readTmCntH(2))      // TM2CNT_H
            DEF_IO16(0x400010C, data = core->timers[1].readTmCntL(3))      // TM3CNT_L
            DEF_IO16(0x400010E, data = core->timers[1].readTmCntH(3))      // TM3CNT_H
            DEF_IO16(0x4000130, data = core->input.readKeyInput())         // KEYINPUT
            DEF_IO16(0x4000200, data = core->interpreter[1].readIe())      // IE
            DEF_IO16(0x4000202, data = core->interpreter[1].readIrf())     // IF
            DEF_IO_8(0x4000208, data = core->interpreter[1].readIme())     // IME
            DEF_IO_8(0x4000300, data = core->interpreter[1].readPostFlg()) // POSTFLG
            DEF_IO16(0x80000C4, data = core->rtc.readGpData())             // GP_DATA
            DEF_IO16(0x80000C6, data = core->rtc.readGpDirection())        // GP_DIRECTION
            DEF_IO16(0x80000C8, data = core->rtc.readGpControl())          // GP_CONTROL

            default:
                // Handle unknown reads by returning 0
                if (i == 0)
                {
                    LOG("Unknown GBA I/O register read: 0x%X\n", address);
                    return 0;
                }

                // Ignore unknown reads if they occur after the first byte
                // This is in case, for example, a 16-bit register is accessed with a 32-bit read
                i++;
                continue;
        }

        value |= (data >> (base * 8)) << (i * 8);
        i += size - base;
    }

    return value;
}

template <typename T> void Memory::ioWrite9(uint32_t address, T value)
{
    size_t i = 0;

    // Write a value to one or more ARM9 I/O registers
    while (i < sizeof(T))
    {
        uint32_t base = address + i;
        size_t size;
        uint32_t mask = ((uint64_t)1 << ((sizeof(T) - i) * 8)) - 1;
        uint32_t data = value >> (i * 8);

        switch (base)
        {
            DEF_IO32(0x4000000, core->gpu2D[0].writeDispCnt(IOWR_PARAMS))            // DISPCNT (engine A)
            DEF_IO16(0x4000004, core->gpu.writeDispStat(0, IOWR_PARAMS))             // DISPSTAT (ARM9)
            DEF_IO16(0x4000008, core->gpu2D[0].writeBgCnt(0, IOWR_PARAMS))           // BG0CNT (engine A)
            DEF_IO16(0x400000A, core->gpu2D[0].writeBgCnt(1, IOWR_PARAMS))           // BG1CNT (engine A)
            DEF_IO16(0x400000C, core->gpu2D[0].writeBgCnt(2, IOWR_PARAMS))           // BG2CNT (engine A)
            DEF_IO16(0x400000E, core->gpu2D[0].writeBgCnt(3, IOWR_PARAMS))           // BG3CNT (engine A)
            DEF_IO16(0x4000010, core->gpu2D[0].writeBgHOfs(0, IOWR_PARAMS))          // BG0HOFS (engine A)
            DEF_IO16(0x4000012, core->gpu2D[0].writeBgVOfs(0, IOWR_PARAMS))          // BG0VOFS (engine A)
            DEF_IO16(0x4000014, core->gpu2D[0].writeBgHOfs(1, IOWR_PARAMS))          // BG1HOFS (engine A)
            DEF_IO16(0x4000016, core->gpu2D[0].writeBgVOfs(1, IOWR_PARAMS))          // BG1VOFS (engine A)
            DEF_IO16(0x4000018, core->gpu2D[0].writeBgHOfs(2, IOWR_PARAMS))          // BG2HOFS (engine A)
            DEF_IO16(0x400001A, core->gpu2D[0].writeBgVOfs(2, IOWR_PARAMS))          // BG2VOFS (engine A)
            DEF_IO16(0x400001C, core->gpu2D[0].writeBgHOfs(3, IOWR_PARAMS))          // BG3HOFS (engine A)
            DEF_IO16(0x400001E, core->gpu2D[0].writeBgVOfs(3, IOWR_PARAMS))          // BG3VOFS (engine A)
            DEF_IO16(0x4000020, core->gpu2D[0].writeBgPA(2, IOWR_PARAMS))            // BG2PA (engine A)
            DEF_IO16(0x4000022, core->gpu2D[0].writeBgPB(2, IOWR_PARAMS))            // BG2PB (engine A)
            DEF_IO16(0x4000024, core->gpu2D[0].writeBgPC(2, IOWR_PARAMS))            // BG2PC (engine A)
            DEF_IO16(0x4000026, core->gpu2D[0].writeBgPD(2, IOWR_PARAMS))            // BG2PD (engine A)
            DEF_IO32(0x4000028, core->gpu2D[0].writeBgX(2, IOWR_PARAMS))             // BG2X (engine A)
            DEF_IO32(0x400002C, core->gpu2D[0].writeBgY(2, IOWR_PARAMS))             // BG2Y (engine A)
            DEF_IO16(0x4000030, core->gpu2D[0].writeBgPA(3, IOWR_PARAMS))            // BG3PA (engine A)
            DEF_IO16(0x4000032, core->gpu2D[0].writeBgPB(3, IOWR_PARAMS))            // BG3PB (engine A)
            DEF_IO16(0x4000034, core->gpu2D[0].writeBgPC(3, IOWR_PARAMS))            // BG3PC (engine A)
            DEF_IO16(0x4000036, core->gpu2D[0].writeBgPD(3, IOWR_PARAMS))            // BG3PD (engine A)
            DEF_IO32(0x4000038, core->gpu2D[0].writeBgX(3, IOWR_PARAMS))             // BG3X (engine A)
            DEF_IO32(0x400003C, core->gpu2D[0].writeBgY(3, IOWR_PARAMS))             // BG3Y (engine A)
            DEF_IO16(0x4000040, core->gpu2D[0].writeWinH(0, IOWR_PARAMS))            // WIN0H (engine A)
            DEF_IO16(0x4000042, core->gpu2D[0].writeWinH(1, IOWR_PARAMS))            // WIN1H (engine A)
            DEF_IO16(0x4000044, core->gpu2D[0].writeWinV(0, IOWR_PARAMS))            // WIN0V (engine A)
            DEF_IO16(0x4000046, core->gpu2D[0].writeWinV(1, IOWR_PARAMS))            // WIN1V (engine A)
            DEF_IO16(0x4000048, core->gpu2D[0].writeWinIn(IOWR_PARAMS))              // WININ (engine A)
            DEF_IO16(0x400004A, core->gpu2D[0].writeWinOut(IOWR_PARAMS))             // WINOUT (engine A)
            DEF_IO16(0x400004C, core->gpu2D[0].writeMosaic(IOWR_PARAMS))             // MOSAIC (engine A)
            DEF_IO16(0x4000050, core->gpu2D[0].writeBldCnt(IOWR_PARAMS))             // BLDCNT (engine A)
            DEF_IO16(0x4000052, core->gpu2D[0].writeBldAlpha(IOWR_PARAMS))           // BLDALPHA (engine A)
            DEF_IO_8(0x4000054, core->gpu2D[0].writeBldY(IOWR_PARAMS8))              // BLDY (engine A)
            DEF_IO16(0x4000060, core->gpu3DRenderer.writeDisp3DCnt(IOWR_PARAMS))     // DISP3DCNT
            DEF_IO32(0x4000064, core->gpu.writeDispCapCnt(IOWR_PARAMS))              // DISPCAPCNT
            DEF_IO16(0x400006C, core->gpu2D[0].writeMasterBright(IOWR_PARAMS))       // MASTER_BRIGHT (engine A)
            DEF_IO32(0x40000B0, core->dma[0].writeDmaSad(0, IOWR_PARAMS))            // DMA0SAD (ARM9)
            DEF_IO32(0x40000B4, core->dma[0].writeDmaDad(0, IOWR_PARAMS))            // DMA0DAD (ARM9)
            DEF_IO32(0x40000B8, core->dma[0].writeDmaCnt(0, IOWR_PARAMS))            // DMA0CNT (ARM9)
            DEF_IO32(0x40000BC, core->dma[0].writeDmaSad(1, IOWR_PARAMS))            // DMA1SAD (ARM9)
            DEF_IO32(0x40000C0, core->dma[0].writeDmaDad(1, IOWR_PARAMS))            // DMA1DAD (ARM9)
            DEF_IO32(0x40000C4, core->dma[0].writeDmaCnt(1, IOWR_PARAMS))            // DMA1CNT (ARM9)
            DEF_IO32(0x40000C8, core->dma[0].writeDmaSad(2, IOWR_PARAMS))            // DMA2SAD (ARM9)
            DEF_IO32(0x40000CC, core->dma[0].writeDmaDad(2, IOWR_PARAMS))            // DMA2DAD (ARM9)
            DEF_IO32(0x40000D0, core->dma[0].writeDmaCnt(2, IOWR_PARAMS))            // DMA2CNT (ARM9)
            DEF_IO32(0x40000D4, core->dma[0].writeDmaSad(3, IOWR_PARAMS))            // DMA3SAD (ARM9)
            DEF_IO32(0x40000D8, core->dma[0].writeDmaDad(3, IOWR_PARAMS))            // DMA3DAD (ARM9)
            DEF_IO32(0x40000DC, core->dma[0].writeDmaCnt(3, IOWR_PARAMS))            // DMA3CNT (ARM9)
            DEF_IO32(0x40000E0, writeDmaFill(0, IOWR_PARAMS))                        // DMA0FILL
            DEF_IO32(0x40000E4, writeDmaFill(1, IOWR_PARAMS))                        // DMA1FILL
            DEF_IO32(0x40000E8, writeDmaFill(2, IOWR_PARAMS))                        // DMA2FILL
            DEF_IO32(0x40000EC, writeDmaFill(3, IOWR_PARAMS))                        // DMA3FILL
            DEF_IO16(0x4000100, core->timers[0].writeTmCntL(0, IOWR_PARAMS))         // TM0CNT_L (ARM9)
            DEF_IO16(0x4000102, core->timers[0].writeTmCntH(0, IOWR_PARAMS))         // TM0CNT_H (ARM9)
            DEF_IO16(0x4000104, core->timers[0].writeTmCntL(1, IOWR_PARAMS))         // TM1CNT_L (ARM9)
            DEF_IO16(0x4000106, core->timers[0].writeTmCntH(1, IOWR_PARAMS))         // TM1CNT_H (ARM9)
            DEF_IO16(0x4000108, core->timers[0].writeTmCntL(2, IOWR_PARAMS))         // TM2CNT_L (ARM9)
            DEF_IO16(0x400010A, core->timers[0].writeTmCntH(2, IOWR_PARAMS))         // TM2CNT_H (ARM9)
            DEF_IO16(0x400010C, core->timers[0].writeTmCntL(3, IOWR_PARAMS))         // TM3CNT_L (ARM9)
            DEF_IO16(0x400010E, core->timers[0].writeTmCntH(3, IOWR_PARAMS))         // TM3CNT_H (ARM9)
            DEF_IO16(0x4000180, core->ipc.writeIpcSync(0, IOWR_PARAMS))              // IPCSYNC (ARM9)
            DEF_IO16(0x4000184, core->ipc.writeIpcFifoCnt(0, IOWR_PARAMS))           // IPCFIFOCNT (ARM9)
            DEF_IO32(0x4000188, core->ipc.writeIpcFifoSend(0, IOWR_PARAMS))          // IPCFIFOSEND (ARM9)
            DEF_IO16(0x40001A0, core->cartridgeNds.writeAuxSpiCnt(0, IOWR_PARAMS))   // AUXSPICNT (ARM9)
            DEF_IO_8(0x40001A2, core->cartridgeNds.writeAuxSpiData(0, IOWR_PARAMS8)) // AUXSPIDATA (ARM9)
            DEF_IO32(0x40001A4, core->cartridgeNds.writeRomCtrl(0, IOWR_PARAMS))     // ROMCTRL (ARM9)
            DEF_IO32(0x40001A8, core->cartridgeNds.writeRomCmdOutL(0, IOWR_PARAMS))  // ROMCMDOUT_L (ARM9)
            DEF_IO32(0x40001AC, core->cartridgeNds.writeRomCmdOutH(0, IOWR_PARAMS))  // ROMCMDOUT_H (ARM9)
            DEF_IO_8(0x4000208, core->interpreter[0].writeIme(IOWR_PARAMS8))         // IME (ARM9)
            DEF_IO32(0x4000210, core->interpreter[0].writeIe(IOWR_PARAMS))           // IE (ARM9)
            DEF_IO32(0x4000214, core->interpreter[0].writeIrf(IOWR_PARAMS))          // IF (ARM9)
            DEF_IO_8(0x4000240, writeVramCnt(0, IOWR_PARAMS8))                       // VRAMCNT_A
            DEF_IO_8(0x4000241, writeVramCnt(1, IOWR_PARAMS8))                       // VRAMCNT_B
            DEF_IO_8(0x4000242, writeVramCnt(2, IOWR_PARAMS8))                       // VRAMCNT_C
            DEF_IO_8(0x4000243, writeVramCnt(3, IOWR_PARAMS8))                       // VRAMCNT_D
            DEF_IO_8(0x4000244, writeVramCnt(4, IOWR_PARAMS8))                       // VRAMCNT_E
            DEF_IO_8(0x4000245, writeVramCnt(5, IOWR_PARAMS8))                       // VRAMCNT_F
            DEF_IO_8(0x4000246, writeVramCnt(6, IOWR_PARAMS8))                       // VRAMCNT_G
            DEF_IO_8(0x4000247, writeWramCnt(IOWR_PARAMS8))                          // WRAMCNT
            DEF_IO_8(0x4000248, writeVramCnt(7, IOWR_PARAMS8))                       // VRAMCNT_H
            DEF_IO_8(0x4000249, writeVramCnt(8, IOWR_PARAMS8))                       // VRAMCNT_I
            DEF_IO16(0x4000280, core->divSqrt.writeDivCnt(IOWR_PARAMS))              // DIVCNT
            DEF_IO32(0x4000290, core->divSqrt.writeDivNumerL(IOWR_PARAMS))           // DIVNUMER_L
            DEF_IO32(0x4000294, core->divSqrt.writeDivNumerH(IOWR_PARAMS))           // DIVNUMER_H
            DEF_IO32(0x4000298, core->divSqrt.writeDivDenomL(IOWR_PARAMS))           // DIVDENOM_L
            DEF_IO32(0x400029C, core->divSqrt.writeDivDenomH(IOWR_PARAMS))           // DIVDENOM_H
            DEF_IO16(0x40002B0, core->divSqrt.writeSqrtCnt(IOWR_PARAMS))             // SQRTCNT
            DEF_IO32(0x40002B8, core->divSqrt.writeSqrtParamL(IOWR_PARAMS))          // SQRTPARAM_L
            DEF_IO32(0x40002BC, core->divSqrt.writeSqrtParamH(IOWR_PARAMS))          // SQRTPARAM_H
            DEF_IO_8(0x4000300, core->interpreter[0].writePostFlg(IOWR_PARAMS8))     // POSTFLG (ARM9)
            DEF_IO16(0x4000304, core->gpu.writePowCnt1(IOWR_PARAMS))                 // POWCNT1
            DEF_IO16(0x4000330, core->gpu3DRenderer.writeEdgeColor(0, IOWR_PARAMS))  // EDGE_COLOR
            DEF_IO16(0x4000332, core->gpu3DRenderer.writeEdgeColor(1, IOWR_PARAMS))  // EDGE_COLOR
            DEF_IO16(0x4000334, core->gpu3DRenderer.writeEdgeColor(2, IOWR_PARAMS))  // EDGE_COLOR
            DEF_IO16(0x4000336, core->gpu3DRenderer.writeEdgeColor(3, IOWR_PARAMS))  // EDGE_COLOR
            DEF_IO16(0x4000338, core->gpu3DRenderer.writeEdgeColor(4, IOWR_PARAMS))  // EDGE_COLOR
            DEF_IO16(0x400033A, core->gpu3DRenderer.writeEdgeColor(5, IOWR_PARAMS))  // EDGE_COLOR
            DEF_IO16(0x400033C, core->gpu3DRenderer.writeEdgeColor(6, IOWR_PARAMS))  // EDGE_COLOR
            DEF_IO16(0x400033E, core->gpu3DRenderer.writeEdgeColor(7, IOWR_PARAMS))  // EDGE_COLOR
            DEF_IO32(0x4000350, core->gpu3DRenderer.writeClearColor(IOWR_PARAMS))    // CLEAR_COLOR
            DEF_IO16(0x4000354, core->gpu3DRenderer.writeClearDepth(IOWR_PARAMS))    // CLEAR_DEPTH
            DEF_IO32(0x4000358, core->gpu3DRenderer.writeFogColor(IOWR_PARAMS))      // FOG_COLOR
            DEF_IO16(0x400035C, core->gpu3DRenderer.writeFogOffset(IOWR_PARAMS))     // FOG_OFFSET
            DEF_IO_8(0x4000360, core->gpu3DRenderer.writeFogTable(0,  IOWR_PARAMS8)) // FOG_TABLE
            DEF_IO_8(0x4000361, core->gpu3DRenderer.writeFogTable(1,  IOWR_PARAMS8)) // FOG_TABLE
            DEF_IO_8(0x4000362, core->gpu3DRenderer.writeFogTable(2,  IOWR_PARAMS8)) // FOG_TABLE
            DEF_IO_8(0x4000363, core->gpu3DRenderer.writeFogTable(3,  IOWR_PARAMS8)) // FOG_TABLE
            DEF_IO_8(0x4000364, core->gpu3DRenderer.writeFogTable(4,  IOWR_PARAMS8)) // FOG_TABLE
            DEF_IO_8(0x4000365, core->gpu3DRenderer.writeFogTable(5,  IOWR_PARAMS8)) // FOG_TABLE
            DEF_IO_8(0x4000366, core->gpu3DRenderer.writeFogTable(6,  IOWR_PARAMS8)) // FOG_TABLE
            DEF_IO_8(0x4000367, core->gpu3DRenderer.writeFogTable(7,  IOWR_PARAMS8)) // FOG_TABLE
            DEF_IO_8(0x4000368, core->gpu3DRenderer.writeFogTable(8,  IOWR_PARAMS8)) // FOG_TABLE
            DEF_IO_8(0x4000369, core->gpu3DRenderer.writeFogTable(9,  IOWR_PARAMS8)) // FOG_TABLE
            DEF_IO_8(0x400036A, core->gpu3DRenderer.writeFogTable(10, IOWR_PARAMS8)) // FOG_TABLE
            DEF_IO_8(0x400036B, core->gpu3DRenderer.writeFogTable(11, IOWR_PARAMS8)) // FOG_TABLE
            DEF_IO_8(0x400036C, core->gpu3DRenderer.writeFogTable(12, IOWR_PARAMS8)) // FOG_TABLE
            DEF_IO_8(0x400036D, core->gpu3DRenderer.writeFogTable(13, IOWR_PARAMS8)) // FOG_TABLE
            DEF_IO_8(0x400036E, core->gpu3DRenderer.writeFogTable(14, IOWR_PARAMS8)) // FOG_TABLE
            DEF_IO_8(0x400036F, core->gpu3DRenderer.writeFogTable(15, IOWR_PARAMS8)) // FOG_TABLE
            DEF_IO_8(0x4000370, core->gpu3DRenderer.writeFogTable(16, IOWR_PARAMS8)) // FOG_TABLE
            DEF_IO_8(0x4000371, core->gpu3DRenderer.writeFogTable(17, IOWR_PARAMS8)) // FOG_TABLE
            DEF_IO_8(0x4000372, core->gpu3DRenderer.writeFogTable(18, IOWR_PARAMS8)) // FOG_TABLE
            DEF_IO_8(0x4000373, core->gpu3DRenderer.writeFogTable(19, IOWR_PARAMS8)) // FOG_TABLE
            DEF_IO_8(0x4000374, core->gpu3DRenderer.writeFogTable(20, IOWR_PARAMS8)) // FOG_TABLE
            DEF_IO_8(0x4000375, core->gpu3DRenderer.writeFogTable(21, IOWR_PARAMS8)) // FOG_TABLE
            DEF_IO_8(0x4000376, core->gpu3DRenderer.writeFogTable(22, IOWR_PARAMS8)) // FOG_TABLE
            DEF_IO_8(0x4000377, core->gpu3DRenderer.writeFogTable(23, IOWR_PARAMS8)) // FOG_TABLE
            DEF_IO_8(0x4000378, core->gpu3DRenderer.writeFogTable(24, IOWR_PARAMS8)) // FOG_TABLE
            DEF_IO_8(0x4000379, core->gpu3DRenderer.writeFogTable(25, IOWR_PARAMS8)) // FOG_TABLE
            DEF_IO_8(0x400037A, core->gpu3DRenderer.writeFogTable(26, IOWR_PARAMS8)) // FOG_TABLE
            DEF_IO_8(0x400037B, core->gpu3DRenderer.writeFogTable(27, IOWR_PARAMS8)) // FOG_TABLE
            DEF_IO_8(0x400037C, core->gpu3DRenderer.writeFogTable(28, IOWR_PARAMS8)) // FOG_TABLE
            DEF_IO_8(0x400037D, core->gpu3DRenderer.writeFogTable(29, IOWR_PARAMS8)) // FOG_TABLE
            DEF_IO_8(0x400037E, core->gpu3DRenderer.writeFogTable(30, IOWR_PARAMS8)) // FOG_TABLE
            DEF_IO_8(0x400037F, core->gpu3DRenderer.writeFogTable(31, IOWR_PARAMS8)) // FOG_TABLE
            DEF_IO16(0x4000380, core->gpu3DRenderer.writeToonTable(0,  IOWR_PARAMS)) // TOON_TABLE
            DEF_IO16(0x4000382, core->gpu3DRenderer.writeToonTable(1,  IOWR_PARAMS)) // TOON_TABLE
            DEF_IO16(0x4000384, core->gpu3DRenderer.writeToonTable(2,  IOWR_PARAMS)) // TOON_TABLE
            DEF_IO16(0x4000386, core->gpu3DRenderer.writeToonTable(3,  IOWR_PARAMS)) // TOON_TABLE
            DEF_IO16(0x4000388, core->gpu3DRenderer.writeToonTable(4,  IOWR_PARAMS)) // TOON_TABLE
            DEF_IO16(0x400038A, core->gpu3DRenderer.writeToonTable(5,  IOWR_PARAMS)) // TOON_TABLE
            DEF_IO16(0x400038C, core->gpu3DRenderer.writeToonTable(6,  IOWR_PARAMS)) // TOON_TABLE
            DEF_IO16(0x400038E, core->gpu3DRenderer.writeToonTable(7,  IOWR_PARAMS)) // TOON_TABLE
            DEF_IO16(0x4000390, core->gpu3DRenderer.writeToonTable(8,  IOWR_PARAMS)) // TOON_TABLE
            DEF_IO16(0x4000392, core->gpu3DRenderer.writeToonTable(9,  IOWR_PARAMS)) // TOON_TABLE
            DEF_IO16(0x4000394, core->gpu3DRenderer.writeToonTable(10, IOWR_PARAMS)) // TOON_TABLE
            DEF_IO16(0x4000396, core->gpu3DRenderer.writeToonTable(11, IOWR_PARAMS)) // TOON_TABLE
            DEF_IO16(0x4000398, core->gpu3DRenderer.writeToonTable(12, IOWR_PARAMS)) // TOON_TABLE
            DEF_IO16(0x400039A, core->gpu3DRenderer.writeToonTable(13, IOWR_PARAMS)) // TOON_TABLE
            DEF_IO16(0x400039C, core->gpu3DRenderer.writeToonTable(14, IOWR_PARAMS)) // TOON_TABLE
            DEF_IO16(0x400039E, core->gpu3DRenderer.writeToonTable(15, IOWR_PARAMS)) // TOON_TABLE
            DEF_IO16(0x40003A0, core->gpu3DRenderer.writeToonTable(16, IOWR_PARAMS)) // TOON_TABLE
            DEF_IO16(0x40003A2, core->gpu3DRenderer.writeToonTable(17, IOWR_PARAMS)) // TOON_TABLE
            DEF_IO16(0x40003A4, core->gpu3DRenderer.writeToonTable(18, IOWR_PARAMS)) // TOON_TABLE
            DEF_IO16(0x40003A6, core->gpu3DRenderer.writeToonTable(19, IOWR_PARAMS)) // TOON_TABLE
            DEF_IO16(0x40003A8, core->gpu3DRenderer.writeToonTable(20, IOWR_PARAMS)) // TOON_TABLE
            DEF_IO16(0x40003AA, core->gpu3DRenderer.writeToonTable(21, IOWR_PARAMS)) // TOON_TABLE
            DEF_IO16(0x40003AC, core->gpu3DRenderer.writeToonTable(22, IOWR_PARAMS)) // TOON_TABLE
            DEF_IO16(0x40003AE, core->gpu3DRenderer.writeToonTable(23, IOWR_PARAMS)) // TOON_TABLE
            DEF_IO16(0x40003B0, core->gpu3DRenderer.writeToonTable(24, IOWR_PARAMS)) // TOON_TABLE
            DEF_IO16(0x40003B2, core->gpu3DRenderer.writeToonTable(25, IOWR_PARAMS)) // TOON_TABLE
            DEF_IO16(0x40003B4, core->gpu3DRenderer.writeToonTable(26, IOWR_PARAMS)) // TOON_TABLE
            DEF_IO16(0x40003B6, core->gpu3DRenderer.writeToonTable(27, IOWR_PARAMS)) // TOON_TABLE
            DEF_IO16(0x40003B8, core->gpu3DRenderer.writeToonTable(28, IOWR_PARAMS)) // TOON_TABLE
            DEF_IO16(0x40003BA, core->gpu3DRenderer.writeToonTable(29, IOWR_PARAMS)) // TOON_TABLE
            DEF_IO16(0x40003BC, core->gpu3DRenderer.writeToonTable(30, IOWR_PARAMS)) // TOON_TABLE
            DEF_IO16(0x40003BE, core->gpu3DRenderer.writeToonTable(31, IOWR_PARAMS)) // TOON_TABLE
            DEF_IO32(0x4000400, core->gpu3D.writeGxFifo(IOWR_PARAMS))                // GXFIFO
            DEF_IO32(0x4000404, core->gpu3D.writeGxFifo(IOWR_PARAMS))                // GXFIFO
            DEF_IO32(0x4000408, core->gpu3D.writeGxFifo(IOWR_PARAMS))                // GXFIFO
            DEF_IO32(0x400040C, core->gpu3D.writeGxFifo(IOWR_PARAMS))                // GXFIFO
            DEF_IO32(0x4000410, core->gpu3D.writeGxFifo(IOWR_PARAMS))                // GXFIFO
            DEF_IO32(0x4000414, core->gpu3D.writeGxFifo(IOWR_PARAMS))                // GXFIFO
            DEF_IO32(0x4000418, core->gpu3D.writeGxFifo(IOWR_PARAMS))                // GXFIFO
            DEF_IO32(0x400041C, core->gpu3D.writeGxFifo(IOWR_PARAMS))                // GXFIFO
            DEF_IO32(0x4000420, core->gpu3D.writeGxFifo(IOWR_PARAMS))                // GXFIFO
            DEF_IO32(0x4000424, core->gpu3D.writeGxFifo(IOWR_PARAMS))                // GXFIFO
            DEF_IO32(0x4000428, core->gpu3D.writeGxFifo(IOWR_PARAMS))                // GXFIFO
            DEF_IO32(0x400042C, core->gpu3D.writeGxFifo(IOWR_PARAMS))                // GXFIFO
            DEF_IO32(0x4000430, core->gpu3D.writeGxFifo(IOWR_PARAMS))                // GXFIFO
            DEF_IO32(0x4000434, core->gpu3D.writeGxFifo(IOWR_PARAMS))                // GXFIFO
            DEF_IO32(0x4000438, core->gpu3D.writeGxFifo(IOWR_PARAMS))                // GXFIFO
            DEF_IO32(0x400043C, core->gpu3D.writeGxFifo(IOWR_PARAMS))                // GXFIFO
            DEF_IO32(0x4000440, core->gpu3D.writeMtxMode(IOWR_PARAMS))               // MTX_MODE
            DEF_IO32(0x4000444, core->gpu3D.writeMtxPush(IOWR_PARAMS))               // MTX_PUSH
            DEF_IO32(0x4000448, core->gpu3D.writeMtxPop(IOWR_PARAMS))                // MTX_POP
            DEF_IO32(0x400044C, core->gpu3D.writeMtxStore(IOWR_PARAMS))              // MTX_STORE
            DEF_IO32(0x4000450, core->gpu3D.writeMtxRestore(IOWR_PARAMS))            // MTX_RESTORE
            DEF_IO32(0x4000454, core->gpu3D.writeMtxIdentity(IOWR_PARAMS))           // MTX_IDENTITY
            DEF_IO32(0x4000458, core->gpu3D.writeMtxLoad44(IOWR_PARAMS))             // MTX_LOAD_4x4
            DEF_IO32(0x400045C, core->gpu3D.writeMtxLoad43(IOWR_PARAMS))             // MTX_LOAD_4x3
            DEF_IO32(0x4000460, core->gpu3D.writeMtxMult44(IOWR_PARAMS))             // MTX_MULT_4x4
            DEF_IO32(0x4000464, core->gpu3D.writeMtxMult43(IOWR_PARAMS))             // MTX_MULT_4x3
            DEF_IO32(0x4000468, core->gpu3D.writeMtxMult33(IOWR_PARAMS))             // MTX_MULT_3x3
            DEF_IO32(0x400046C, core->gpu3D.writeMtxScale(IOWR_PARAMS))              // MTX_SCALE
            DEF_IO32(0x4000470, core->gpu3D.writeMtxTrans(IOWR_PARAMS))              // MTX_TRANS
            DEF_IO32(0x4000480, core->gpu3D.writeColor(IOWR_PARAMS))                 // COLOR
            DEF_IO32(0x4000484, core->gpu3D.writeNormal(IOWR_PARAMS))                // NORMAL
            DEF_IO32(0x4000488, core->gpu3D.writeTexCoord(IOWR_PARAMS))              // TEXCOORD
            DEF_IO32(0x400048C, core->gpu3D.writeVtx16(IOWR_PARAMS))                 // VTX_16
            DEF_IO32(0x4000490, core->gpu3D.writeVtx10(IOWR_PARAMS))                 // VTX_10
            DEF_IO32(0x4000494, core->gpu3D.writeVtxXY(IOWR_PARAMS))                 // VTX_XY
            DEF_IO32(0x4000498, core->gpu3D.writeVtxXZ(IOWR_PARAMS))                 // VTX_XZ
            DEF_IO32(0x400049C, core->gpu3D.writeVtxYZ(IOWR_PARAMS))                 // VTX_YZ
            DEF_IO32(0x40004A0, core->gpu3D.writeVtxDiff(IOWR_PARAMS))               // VTX_DIFF
            DEF_IO32(0x40004A4, core->gpu3D.writePolygonAttr(IOWR_PARAMS))           // POLYGON_ATTR
            DEF_IO32(0x40004A8, core->gpu3D.writeTexImageParam(IOWR_PARAMS))         // TEXIMAGE_PARAM
            DEF_IO32(0x40004AC, core->gpu3D.writePlttBase(IOWR_PARAMS))              // PLTT_BASE
            DEF_IO32(0x40004C0, core->gpu3D.writeDifAmb(IOWR_PARAMS))                // DIF_AMB
            DEF_IO32(0x40004C4, core->gpu3D.writeSpeEmi(IOWR_PARAMS))                // SPE_EMI
            DEF_IO32(0x40004C8, core->gpu3D.writeLightVector(IOWR_PARAMS))           // LIGHT_VECTOR
            DEF_IO32(0x40004CC, core->gpu3D.writeLightColor(IOWR_PARAMS))            // LIGHT_COLOR
            DEF_IO32(0x40004D0, core->gpu3D.writeShininess(IOWR_PARAMS))             // SHININESS
            DEF_IO32(0x4000500, core->gpu3D.writeBeginVtxs(IOWR_PARAMS))             // BEGIN_VTXS
            DEF_IO32(0x4000504, core->gpu3D.writeEndVtxs(IOWR_PARAMS))               // END_VTXS
            DEF_IO32(0x4000540, core->gpu3D.writeSwapBuffers(IOWR_PARAMS))           // SWAP_BUFFERS
            DEF_IO32(0x4000580, core->gpu3D.writeViewport(IOWR_PARAMS))              // VIEWPORT
            DEF_IO32(0x40005C0, core->gpu3D.writeBoxTest(IOWR_PARAMS))               // BOX_TEST
            DEF_IO32(0x40005C4, core->gpu3D.writePosTest(IOWR_PARAMS))               // POS_TEST
            DEF_IO32(0x40005C8, core->gpu3D.writeVecTest(IOWR_PARAMS))               // VEC_TEST
            DEF_IO32(0x4000600, core->gpu3D.writeGxStat(IOWR_PARAMS))                // GXSTAT
            DEF_IO32(0x4001000, core->gpu2D[1].writeDispCnt(IOWR_PARAMS))            // DISPCNT (engine B)
            DEF_IO16(0x4001008, core->gpu2D[1].writeBgCnt(0, IOWR_PARAMS))           // BG0CNT (engine B)
            DEF_IO16(0x400100A, core->gpu2D[1].writeBgCnt(1, IOWR_PARAMS))           // BG1CNT (engine B)
            DEF_IO16(0x400100C, core->gpu2D[1].writeBgCnt(2, IOWR_PARAMS))           // BG2CNT (engine B)
            DEF_IO16(0x400100E, core->gpu2D[1].writeBgCnt(3, IOWR_PARAMS))           // BG3CNT (engine B)
            DEF_IO16(0x4001010, core->gpu2D[1].writeBgHOfs(0, IOWR_PARAMS))          // BG0HOFS (engine B)
            DEF_IO16(0x4001012, core->gpu2D[1].writeBgVOfs(0, IOWR_PARAMS))          // BG0VOFS (engine B)
            DEF_IO16(0x4001014, core->gpu2D[1].writeBgHOfs(1, IOWR_PARAMS))          // BG1HOFS (engine B)
            DEF_IO16(0x4001016, core->gpu2D[1].writeBgVOfs(1, IOWR_PARAMS))          // BG1VOFS (engine B)
            DEF_IO16(0x4001018, core->gpu2D[1].writeBgHOfs(2, IOWR_PARAMS))          // BG2HOFS (engine B)
            DEF_IO16(0x400101A, core->gpu2D[1].writeBgVOfs(2, IOWR_PARAMS))          // BG2VOFS (engine B)
            DEF_IO16(0x400101C, core->gpu2D[1].writeBgHOfs(3, IOWR_PARAMS))          // BG3HOFS (engine B)
            DEF_IO16(0x400101E, core->gpu2D[1].writeBgVOfs(3, IOWR_PARAMS))          // BG3VOFS (engine B)
            DEF_IO16(0x4001020, core->gpu2D[1].writeBgPA(2, IOWR_PARAMS))            // BG2PA (engine B)
            DEF_IO16(0x4001022, core->gpu2D[1].writeBgPB(2, IOWR_PARAMS))            // BG2PB (engine B)
            DEF_IO16(0x4001024, core->gpu2D[1].writeBgPC(2, IOWR_PARAMS))            // BG2PC (engine B)
            DEF_IO16(0x4001026, core->gpu2D[1].writeBgPD(2, IOWR_PARAMS))            // BG2PD (engine B)
            DEF_IO32(0x4001028, core->gpu2D[1].writeBgX(2, IOWR_PARAMS))             // BG2X (engine B)
            DEF_IO32(0x400102C, core->gpu2D[1].writeBgY(2, IOWR_PARAMS))             // BG2Y (engine B)
            DEF_IO16(0x4001030, core->gpu2D[1].writeBgPA(3, IOWR_PARAMS))            // BG3PA (engine B)
            DEF_IO16(0x4001032, core->gpu2D[1].writeBgPB(3, IOWR_PARAMS))            // BG3PB (engine B)
            DEF_IO16(0x4001034, core->gpu2D[1].writeBgPC(3, IOWR_PARAMS))            // BG3PC (engine B)
            DEF_IO16(0x4001036, core->gpu2D[1].writeBgPD(3, IOWR_PARAMS))            // BG3PD (engine B)
            DEF_IO32(0x4001038, core->gpu2D[1].writeBgX(3, IOWR_PARAMS))             // BG3X (engine B)
            DEF_IO32(0x400103C, core->gpu2D[1].writeBgY(3, IOWR_PARAMS))             // BG3Y (engine B)
            DEF_IO16(0x4001040, core->gpu2D[1].writeWinH(0, IOWR_PARAMS))            // WIN0H (engine B)
            DEF_IO16(0x4001042, core->gpu2D[1].writeWinH(1, IOWR_PARAMS))            // WIN1H (engine B)
            DEF_IO16(0x4001044, core->gpu2D[1].writeWinV(0, IOWR_PARAMS))            // WIN0V (engine B)
            DEF_IO16(0x4001046, core->gpu2D[1].writeWinV(1, IOWR_PARAMS))            // WIN1V (engine B)
            DEF_IO16(0x4001048, core->gpu2D[1].writeWinIn(IOWR_PARAMS))              // WININ (engine B)
            DEF_IO16(0x400104A, core->gpu2D[1].writeWinOut(IOWR_PARAMS))             // WINOUT (engine B)
            DEF_IO16(0x400104C, core->gpu2D[1].writeMosaic(IOWR_PARAMS))             // MOSAIC (engine B)
            DEF_IO16(0x4001050, core->gpu2D[1].writeBldCnt(IOWR_PARAMS))             // BLDCNT (engine B)
            DEF_IO16(0x4001052, core->gpu2D[1].writeBldAlpha(IOWR_PARAMS))           // BLDALPHA (engine B)
            DEF_IO_8(0x4001054, core->gpu2D[1].writeBldY(IOWR_PARAMS8))              // BLDY (engine B)
            DEF_IO16(0x400106C, core->gpu2D[1].writeMasterBright(IOWR_PARAMS))       // MASTER_BRIGHT (engine B)

            default:
                // Catch unknown writes
                if (i == 0)
                {
                    LOG("Unknown ARM9 I/O register write: 0x%X\n", address);
                    return;
                }

                // Ignore unknown writes if they occur after the first byte
                // This is in case, for example, a 16-bit register is accessed with a 32-bit write
                i++;
                continue;
        }

        i += size - base;
    }
}

template <typename T> void Memory::ioWrite7(uint32_t address, T value)
{
    // Mirror the WiFi regions
    if (address >= 0x04808000 && address < 0x04810000)
        address &= ~0x00008000;

    size_t i = 0;

    // Write a value to one or more ARM7 I/O registers
    while (i < sizeof(T))
    {
        uint32_t base = address + i;
        size_t size;
        uint32_t mask = ((uint64_t)1 << ((sizeof(T) - i) * 8)) - 1;
        uint32_t data = value >> (i * 8);

        switch (base)
        {
            DEF_IO16(0x4000004, core->gpu.writeDispStat(1, IOWR_PARAMS))             // DISPSTAT (ARM7)
            DEF_IO32(0x40000B0, core->dma[1].writeDmaSad(0, IOWR_PARAMS))            // DMA0SAD (ARM7)
            DEF_IO32(0x40000B4, core->dma[1].writeDmaDad(0, IOWR_PARAMS))            // DMA0DAD (ARM7)
            DEF_IO32(0x40000B8, core->dma[1].writeDmaCnt(0, IOWR_PARAMS))            // DMA0CNT (ARM7)
            DEF_IO32(0x40000BC, core->dma[1].writeDmaSad(1, IOWR_PARAMS))            // DMA1SAD (ARM7)
            DEF_IO32(0x40000C0, core->dma[1].writeDmaDad(1, IOWR_PARAMS))            // DMA1DAD (ARM7)
            DEF_IO32(0x40000C4, core->dma[1].writeDmaCnt(1, IOWR_PARAMS))            // DMA1CNT (ARM7)
            DEF_IO32(0x40000C8, core->dma[1].writeDmaSad(2, IOWR_PARAMS))            // DMA2SAD (ARM7)
            DEF_IO32(0x40000CC, core->dma[1].writeDmaDad(2, IOWR_PARAMS))            // DMA2DAD (ARM7)
            DEF_IO32(0x40000D0, core->dma[1].writeDmaCnt(2, IOWR_PARAMS))            // DMA2CNT (ARM7)
            DEF_IO32(0x40000D4, core->dma[1].writeDmaSad(3, IOWR_PARAMS))            // DMA3SAD (ARM7)
            DEF_IO32(0x40000D8, core->dma[1].writeDmaDad(3, IOWR_PARAMS))            // DMA3DAD (ARM7)
            DEF_IO32(0x40000DC, core->dma[1].writeDmaCnt(3, IOWR_PARAMS))            // DMA3CNT (ARM7)
            DEF_IO16(0x4000100, core->timers[1].writeTmCntL(0, IOWR_PARAMS))         // TM0CNT_L (ARM7)
            DEF_IO16(0x4000102, core->timers[1].writeTmCntH(0, IOWR_PARAMS))         // TM0CNT_H (ARM7)
            DEF_IO16(0x4000104, core->timers[1].writeTmCntL(1, IOWR_PARAMS))         // TM1CNT_L (ARM7)
            DEF_IO16(0x4000106, core->timers[1].writeTmCntH(1, IOWR_PARAMS))         // TM1CNT_H (ARM7)
            DEF_IO16(0x4000108, core->timers[1].writeTmCntL(2, IOWR_PARAMS))         // TM2CNT_L (ARM7)
            DEF_IO16(0x400010A, core->timers[1].writeTmCntH(2, IOWR_PARAMS))         // TM2CNT_H (ARM7)
            DEF_IO16(0x400010C, core->timers[1].writeTmCntL(3, IOWR_PARAMS))         // TM3CNT_L (ARM7)
            DEF_IO16(0x400010E, core->timers[1].writeTmCntH(3, IOWR_PARAMS))         // TM3CNT_H (ARM7)
            DEF_IO_8(0x4000138, core->rtc.writeRtc(IOWR_PARAMS8))                    // RTC
            DEF_IO16(0x4000180, core->ipc.writeIpcSync(1, IOWR_PARAMS))              // IPCSYNC (ARM7)
            DEF_IO16(0x4000184, core->ipc.writeIpcFifoCnt(1, IOWR_PARAMS))           // IPCFIFOCNT (ARM7)
            DEF_IO32(0x4000188, core->ipc.writeIpcFifoSend(1, IOWR_PARAMS))          // IPCFIFOSEND (ARM7)
            DEF_IO16(0x40001A0, core->cartridgeNds.writeAuxSpiCnt(1, IOWR_PARAMS))   // AUXSPICNT (ARM7)
            DEF_IO_8(0x40001A2, core->cartridgeNds.writeAuxSpiData(1, IOWR_PARAMS8)) // AUXSPIDATA (ARM7)
            DEF_IO32(0x40001A4, core->cartridgeNds.writeRomCtrl(1, IOWR_PARAMS))     // ROMCTRL (ARM7)
            DEF_IO32(0x40001A8, core->cartridgeNds.writeRomCmdOutL(1, IOWR_PARAMS))  // ROMCMDOUT_L (ARM7)
            DEF_IO32(0x40001AC, core->cartridgeNds.writeRomCmdOutH(1, IOWR_PARAMS))  // ROMCMDOUT_H (ARM7)
            DEF_IO16(0x40001C0, core->spi.writeSpiCnt(IOWR_PARAMS))                  // SPICNT
            DEF_IO_8(0x40001C2, core->spi.writeSpiData(IOWR_PARAMS8))                // SPIDATA
            DEF_IO_8(0x4000208, core->interpreter[1].writeIme(IOWR_PARAMS8))         // IME (ARM7)
            DEF_IO32(0x4000210, core->interpreter[1].writeIe(IOWR_PARAMS))           // IE (ARM7)
            DEF_IO32(0x4000214, core->interpreter[1].writeIrf(IOWR_PARAMS))          // IF (ARM7)
            DEF_IO_8(0x4000300, core->interpreter[1].writePostFlg(IOWR_PARAMS8))     // POSTFLG (ARM7)
            DEF_IO_8(0x4000301, writeHaltCnt(IOWR_PARAMS8))                          // HALTCNT
            DEF_IO32(0x4000400, core->spu.writeSoundCnt(0, IOWR_PARAMS))             // SOUND0CNT
            DEF_IO32(0x4000404, core->spu.writeSoundSad(0, IOWR_PARAMS))             // SOUND0SAD
            DEF_IO16(0x4000408, core->spu.writeSoundTmr(0, IOWR_PARAMS))             // SOUND0TMR
            DEF_IO16(0x400040A, core->spu.writeSoundPnt(0, IOWR_PARAMS))             // SOUND0PNT
            DEF_IO32(0x400040C, core->spu.writeSoundLen(0, IOWR_PARAMS))             // SOUND0LEN
            DEF_IO32(0x4000410, core->spu.writeSoundCnt(1, IOWR_PARAMS))             // SOUND1CNT
            DEF_IO32(0x4000414, core->spu.writeSoundSad(1, IOWR_PARAMS))             // SOUND1SAD
            DEF_IO16(0x4000418, core->spu.writeSoundTmr(1, IOWR_PARAMS))             // SOUND1TMR
            DEF_IO16(0x400041A, core->spu.writeSoundPnt(1, IOWR_PARAMS))             // SOUND1PNT
            DEF_IO32(0x400041C, core->spu.writeSoundLen(1, IOWR_PARAMS))             // SOUND1LEN
            DEF_IO32(0x4000420, core->spu.writeSoundCnt(2, IOWR_PARAMS))             // SOUND2CNT
            DEF_IO32(0x4000424, core->spu.writeSoundSad(2, IOWR_PARAMS))             // SOUND2SAD
            DEF_IO16(0x4000428, core->spu.writeSoundTmr(2, IOWR_PARAMS))             // SOUND2TMR
            DEF_IO16(0x400042A, core->spu.writeSoundPnt(2, IOWR_PARAMS))             // SOUND2PNT
            DEF_IO32(0x400042C, core->spu.writeSoundLen(2, IOWR_PARAMS))             // SOUND2LEN
            DEF_IO32(0x4000430, core->spu.writeSoundCnt(3, IOWR_PARAMS))             // SOUND3CNT
            DEF_IO32(0x4000434, core->spu.writeSoundSad(3, IOWR_PARAMS))             // SOUND3SAD
            DEF_IO16(0x4000438, core->spu.writeSoundTmr(3, IOWR_PARAMS))             // SOUND3TMR
            DEF_IO16(0x400043A, core->spu.writeSoundPnt(3, IOWR_PARAMS))             // SOUND3PNT
            DEF_IO32(0x400043C, core->spu.writeSoundLen(3, IOWR_PARAMS))             // SOUND3LEN
            DEF_IO32(0x4000440, core->spu.writeSoundCnt(4, IOWR_PARAMS))             // SOUND4CNT
            DEF_IO32(0x4000444, core->spu.writeSoundSad(4, IOWR_PARAMS))             // SOUND4SAD
            DEF_IO16(0x4000448, core->spu.writeSoundTmr(4, IOWR_PARAMS))             // SOUND4TMR
            DEF_IO16(0x400044A, core->spu.writeSoundPnt(4, IOWR_PARAMS))             // SOUND4PNT
            DEF_IO32(0x400044C, core->spu.writeSoundLen(4, IOWR_PARAMS))             // SOUND4LEN
            DEF_IO32(0x4000450, core->spu.writeSoundCnt(5, IOWR_PARAMS))             // SOUND5CNT
            DEF_IO32(0x4000454, core->spu.writeSoundSad(5, IOWR_PARAMS))             // SOUND5SAD
            DEF_IO16(0x4000458, core->spu.writeSoundTmr(5, IOWR_PARAMS))             // SOUND5TMR
            DEF_IO16(0x400045A, core->spu.writeSoundPnt(5, IOWR_PARAMS))             // SOUND5PNT
            DEF_IO32(0x400045C, core->spu.writeSoundLen(5, IOWR_PARAMS))             // SOUND5LEN
            DEF_IO32(0x4000460, core->spu.writeSoundCnt(6, IOWR_PARAMS))             // SOUND6CNT
            DEF_IO32(0x4000464, core->spu.writeSoundSad(6, IOWR_PARAMS))             // SOUND6SAD
            DEF_IO16(0x4000468, core->spu.writeSoundTmr(6, IOWR_PARAMS))             // SOUND6TMR
            DEF_IO16(0x400046A, core->spu.writeSoundPnt(6, IOWR_PARAMS))             // SOUND6PNT
            DEF_IO32(0x400046C, core->spu.writeSoundLen(6, IOWR_PARAMS))             // SOUND6LEN
            DEF_IO32(0x4000470, core->spu.writeSoundCnt(7, IOWR_PARAMS))             // SOUND7CNT
            DEF_IO32(0x4000474, core->spu.writeSoundSad(7, IOWR_PARAMS))             // SOUND7SAD
            DEF_IO16(0x4000478, core->spu.writeSoundTmr(7, IOWR_PARAMS))             // SOUND7TMR
            DEF_IO16(0x400047A, core->spu.writeSoundPnt(7, IOWR_PARAMS))             // SOUND7PNT
            DEF_IO32(0x400047C, core->spu.writeSoundLen(7, IOWR_PARAMS))             // SOUND7LEN
            DEF_IO32(0x4000480, core->spu.writeSoundCnt(8, IOWR_PARAMS))             // SOUND8CNT
            DEF_IO32(0x4000484, core->spu.writeSoundSad(8, IOWR_PARAMS))             // SOUND8SAD
            DEF_IO16(0x4000488, core->spu.writeSoundTmr(8, IOWR_PARAMS))             // SOUND8TMR
            DEF_IO16(0x400048A, core->spu.writeSoundPnt(8, IOWR_PARAMS))             // SOUND8PNT
            DEF_IO32(0x400048C, core->spu.writeSoundLen(8, IOWR_PARAMS))             // SOUND8LEN
            DEF_IO32(0x4000490, core->spu.writeSoundCnt(9, IOWR_PARAMS))             // SOUND9CNT
            DEF_IO32(0x4000494, core->spu.writeSoundSad(9, IOWR_PARAMS))             // SOUND9SAD
            DEF_IO16(0x4000498, core->spu.writeSoundTmr(9, IOWR_PARAMS))             // SOUND9TMR
            DEF_IO16(0x400049A, core->spu.writeSoundPnt(9, IOWR_PARAMS))             // SOUND9PNT
            DEF_IO32(0x400049C, core->spu.writeSoundLen(9, IOWR_PARAMS))             // SOUND9LEN
            DEF_IO32(0x40004A0, core->spu.writeSoundCnt(10, IOWR_PARAMS))            // SOUND10CNT
            DEF_IO32(0x40004A4, core->spu.writeSoundSad(10, IOWR_PARAMS))            // SOUND10SAD
            DEF_IO16(0x40004A8, core->spu.writeSoundTmr(10, IOWR_PARAMS))            // SOUND10TMR
            DEF_IO16(0x40004AA, core->spu.writeSoundPnt(10, IOWR_PARAMS))            // SOUND10PNT
            DEF_IO32(0x40004AC, core->spu.writeSoundLen(10, IOWR_PARAMS))            // SOUND10LEN
            DEF_IO32(0x40004B0, core->spu.writeSoundCnt(11, IOWR_PARAMS))            // SOUND11CNT
            DEF_IO32(0x40004B4, core->spu.writeSoundSad(11, IOWR_PARAMS))            // SOUND11SAD
            DEF_IO16(0x40004B8, core->spu.writeSoundTmr(11, IOWR_PARAMS))            // SOUND11TMR
            DEF_IO16(0x40004BA, core->spu.writeSoundPnt(11, IOWR_PARAMS))            // SOUND11PNT
            DEF_IO32(0x40004BC, core->spu.writeSoundLen(11, IOWR_PARAMS))            // SOUND11LEN
            DEF_IO32(0x40004C0, core->spu.writeSoundCnt(12, IOWR_PARAMS))            // SOUND12CNT
            DEF_IO32(0x40004C4, core->spu.writeSoundSad(12, IOWR_PARAMS))            // SOUND12SAD
            DEF_IO16(0x40004C8, core->spu.writeSoundTmr(12, IOWR_PARAMS))            // SOUND12TMR
            DEF_IO16(0x40004CA, core->spu.writeSoundPnt(12, IOWR_PARAMS))            // SOUND12PNT
            DEF_IO32(0x40004CC, core->spu.writeSoundLen(12, IOWR_PARAMS))            // SOUND12LEN
            DEF_IO32(0x40004D0, core->spu.writeSoundCnt(13, IOWR_PARAMS))            // SOUND13CNT
            DEF_IO32(0x40004D4, core->spu.writeSoundSad(13, IOWR_PARAMS))            // SOUND13SAD
            DEF_IO16(0x40004D8, core->spu.writeSoundTmr(13, IOWR_PARAMS))            // SOUND13TMR
            DEF_IO16(0x40004DA, core->spu.writeSoundPnt(13, IOWR_PARAMS))            // SOUND13PNT
            DEF_IO32(0x40004DC, core->spu.writeSoundLen(13, IOWR_PARAMS))            // SOUND13LEN
            DEF_IO32(0x40004E0, core->spu.writeSoundCnt(14, IOWR_PARAMS))            // SOUND14CNT
            DEF_IO32(0x40004E4, core->spu.writeSoundSad(14, IOWR_PARAMS))            // SOUND14SAD
            DEF_IO16(0x40004E8, core->spu.writeSoundTmr(14, IOWR_PARAMS))            // SOUND14TMR
            DEF_IO16(0x40004EA, core->spu.writeSoundPnt(14, IOWR_PARAMS))            // SOUND14PNT
            DEF_IO32(0x40004EC, core->spu.writeSoundLen(14, IOWR_PARAMS))            // SOUND14LEN
            DEF_IO32(0x40004F0, core->spu.writeSoundCnt(15, IOWR_PARAMS))            // SOUND15CNT
            DEF_IO32(0x40004F4, core->spu.writeSoundSad(15, IOWR_PARAMS))            // SOUND15SAD
            DEF_IO16(0x40004F8, core->spu.writeSoundTmr(15, IOWR_PARAMS))            // SOUND15TMR
            DEF_IO16(0x40004FA, core->spu.writeSoundPnt(15, IOWR_PARAMS))            // SOUND15PNT
            DEF_IO32(0x40004FC, core->spu.writeSoundLen(15, IOWR_PARAMS))            // SOUND15LEN
            DEF_IO16(0x4000500, core->spu.writeMainSoundCnt(IOWR_PARAMS))            // SOUNDCNT
            DEF_IO16(0x4000504, core->spu.writeSoundBias(IOWR_PARAMS))               // SOUNDBIAS
            DEF_IO_8(0x4000508, core->spu.writeSndCapCnt(0, IOWR_PARAMS8))           // SNDCAP0CNT
            DEF_IO_8(0x4000509, core->spu.writeSndCapCnt(1, IOWR_PARAMS8))           // SNDCAP1CNT
            DEF_IO32(0x4000510, core->spu.writeSndCapDad(0, IOWR_PARAMS))            // SNDCAP0DAD
            DEF_IO16(0x4000514, core->spu.writeSndCapLen(0, IOWR_PARAMS))            // SNDCAP0LEN
            DEF_IO32(0x4000518, core->spu.writeSndCapDad(1, IOWR_PARAMS))            // SNDCAP1DAD
            DEF_IO16(0x400051C, core->spu.writeSndCapLen(1, IOWR_PARAMS))            // SNDCAP1LEN
            DEF_IO16(0x4800006, core->wifi.writeWModeWep(IOWR_PARAMS))               // W_MODE_WEP
            DEF_IO16(0x4800010, core->wifi.writeWIrf(IOWR_PARAMS))                   // W_IF
            DEF_IO16(0x4800012, core->wifi.writeWIe(IOWR_PARAMS))                    // W_IE
            DEF_IO16(0x4800018, core->wifi.writeWMacaddr(0, IOWR_PARAMS))            // W_MACADDR_0
            DEF_IO16(0x480001A, core->wifi.writeWMacaddr(1, IOWR_PARAMS))            // W_MACADDR_1
            DEF_IO16(0x480001C, core->wifi.writeWMacaddr(2, IOWR_PARAMS))            // W_MACADDR_2
            DEF_IO16(0x4800020, core->wifi.writeWBssid(0, IOWR_PARAMS))              // W_BSSID_0
            DEF_IO16(0x4800022, core->wifi.writeWBssid(1, IOWR_PARAMS))              // W_BSSID_1
            DEF_IO16(0x4800024, core->wifi.writeWBssid(2, IOWR_PARAMS))              // W_BSSID_2
            DEF_IO16(0x480002A, core->wifi.writeWAidFull(IOWR_PARAMS))               // W_AID_FULL
            DEF_IO16(0x4800030, core->wifi.writeWRxcnt(IOWR_PARAMS))                 // W_RXCNT
            DEF_IO16(0x480003C, core->wifi.writeWPowerstate(IOWR_PARAMS))            // W_POWERSTATE
            DEF_IO16(0x4800040, core->wifi.writeWPowerforce(IOWR_PARAMS))            // W_POWERFORCE
            DEF_IO16(0x4800050, core->wifi.writeWRxbufBegin(IOWR_PARAMS))            // W_RXBUF_BEGIN
            DEF_IO16(0x4800052, core->wifi.writeWRxbufEnd(IOWR_PARAMS))              // W_RXBUF_END
            DEF_IO16(0x4800056, core->wifi.writeWRxbufWrAddr(IOWR_PARAMS))           // W_RXBUF_WR_ADDR
            DEF_IO16(0x4800058, core->wifi.writeWRxbufRdAddr(IOWR_PARAMS))           // W_RXBUF_RD_ADDR
            DEF_IO16(0x480005A, core->wifi.writeWRxbufReadcsr(IOWR_PARAMS))          // W_RXBUF_READCSR
            DEF_IO16(0x480005C, core->wifi.writeWRxbufCount(IOWR_PARAMS))            // W_RXBUF_COUNT
            DEF_IO16(0x4800062, core->wifi.writeWRxbufGap(IOWR_PARAMS))              // W_RXBUF_GAP
            DEF_IO16(0x4800064, core->wifi.writeWRxbufGapdisp(IOWR_PARAMS))          // W_RXBUF_GAPDISP
            DEF_IO16(0x4800068, core->wifi.writeWTxbufWrAddr(IOWR_PARAMS))           // W_TXBUF_WR_ADDR
            DEF_IO16(0x480006C, core->wifi.writeWTxbufCount(IOWR_PARAMS))            // W_TXBUF_COUNT
            DEF_IO16(0x4800070, core->wifi.writeWTxbufWrData(IOWR_PARAMS))           // W_TXBUF_WR_DATA
            DEF_IO16(0x4800074, core->wifi.writeWTxbufGap(IOWR_PARAMS))              // W_TXBUF_GAP
            DEF_IO16(0x4800076, core->wifi.writeWTxbufGapdisp(IOWR_PARAMS))          // W_TXBUF_GAPDISP
            DEF_IO16(0x4800080, core->wifi.writeWTxbufLoc(4, IOWR_PARAMS))           // W_TXBUF_BEACON
            DEF_IO16(0x480008C, core->wifi.writeWBeaconInt(IOWR_PARAMS))             // W_BEACON_INT
            DEF_IO16(0x4800090, core->wifi.writeWTxbufLoc(1, IOWR_PARAMS))           // W_TXBUF_CMD
            DEF_IO16(0x48000A0, core->wifi.writeWTxbufLoc(0, IOWR_PARAMS))           // W_TXBUF_LOC1
            DEF_IO16(0x48000A4, core->wifi.writeWTxbufLoc(2, IOWR_PARAMS))           // W_TXBUF_LOC2
            DEF_IO16(0x48000A8, core->wifi.writeWTxbufLoc(3, IOWR_PARAMS))           // W_TXBUF_LOC3
            DEF_IO16(0x48000AC, core->wifi.writeWTxreqReset(IOWR_PARAMS))            // W_TXREQ_RESET
            DEF_IO16(0x48000AE, core->wifi.writeWTxreqSet(IOWR_PARAMS))              // W_TXREQ_SET
            DEF_IO16(0x48000E8, core->wifi.writeWUsCountcnt(IOWR_PARAMS))            // W_US_COUNTCNT
            DEF_IO16(0x48000EA, core->wifi.writeWUsComparecnt(IOWR_PARAMS))          // W_US_COMPARECNT
            DEF_IO16(0x4800110, core->wifi.writeWPreBeacon(IOWR_PARAMS))             // W_PRE_BEACON
            DEF_IO16(0x480011C, core->wifi.writeWBeaconCount(IOWR_PARAMS))           // W_BEACON_COUNT
            DEF_IO16(0x4800120, core->wifi.writeWConfig(0,  IOWR_PARAMS))            // W_CONFIG_120
            DEF_IO16(0x4800122, core->wifi.writeWConfig(1,  IOWR_PARAMS))            // W_CONFIG_122
            DEF_IO16(0x4800124, core->wifi.writeWConfig(2,  IOWR_PARAMS))            // W_CONFIG_124
            DEF_IO16(0x4800128, core->wifi.writeWConfig(3,  IOWR_PARAMS))            // W_CONFIG_128
            DEF_IO16(0x4800130, core->wifi.writeWConfig(4,  IOWR_PARAMS))            // W_CONFIG_130
            DEF_IO16(0x4800132, core->wifi.writeWConfig(5,  IOWR_PARAMS))            // W_CONFIG_132
            DEF_IO16(0x4800134, core->wifi.writeWPostBeacon(IOWR_PARAMS))            // W_POST_BEACON
            DEF_IO16(0x4800140, core->wifi.writeWConfig(6,  IOWR_PARAMS))            // W_CONFIG_140
            DEF_IO16(0x4800142, core->wifi.writeWConfig(7,  IOWR_PARAMS))            // W_CONFIG_142
            DEF_IO16(0x4800144, core->wifi.writeWConfig(8,  IOWR_PARAMS))            // W_CONFIG_144
            DEF_IO16(0x4800146, core->wifi.writeWConfig(9,  IOWR_PARAMS))            // W_CONFIG_146
            DEF_IO16(0x4800148, core->wifi.writeWConfig(10, IOWR_PARAMS))            // W_CONFIG_148
            DEF_IO16(0x480014A, core->wifi.writeWConfig(11, IOWR_PARAMS))            // W_CONFIG_14A
            DEF_IO16(0x480014C, core->wifi.writeWConfig(12, IOWR_PARAMS))            // W_CONFIG_14C
            DEF_IO16(0x4800150, core->wifi.writeWConfig(13, IOWR_PARAMS))            // W_CONFIG_150
            DEF_IO16(0x4800154, core->wifi.writeWConfig(14, IOWR_PARAMS))            // W_CONFIG_154
            DEF_IO16(0x4800158, core->wifi.writeWBbCnt(IOWR_PARAMS))                 // W_BB_CNT
            DEF_IO16(0x480015A, core->wifi.writeWBbWrite(IOWR_PARAMS))               // W_BB_WRITE
            DEF_IO16(0x480021C, core->wifi.writeWIrfSet(IOWR_PARAMS))                // W_IF_SET

            default:
                // Catch unknown writes
                if (i == 0)
                {
                    LOG("Unknown ARM7 I/O register write: 0x%X\n", address);
                    return;
                }

                // Ignore unknown writes if they occur after the first byte
                // This is in case, for example, a 16-bit register is accessed with a 32-bit write
                i++;
                continue;
        }

        i += size - base;
    }
}

template <typename T> void Memory::ioWriteGba(uint32_t address, T value)
{
    size_t i = 0;

    // Write a value to one or more GBA I/O registers
    while (i < sizeof(T))
    {
        uint32_t base = address + i;
        size_t size;
        uint32_t mask = ((uint64_t)1 << ((sizeof(T) - i) * 8)) - 1;
        uint32_t data = value >> (i * 8);

        switch (base)
        {
            DEF_IO16(0x4000000, core->gpu2D[0].writeDispCnt(IOWR_PARAMS))        // DISPCNT
            DEF_IO16(0x4000004, core->gpu.writeDispStat(1, IOWR_PARAMS))         // DISPSTAT
            DEF_IO16(0x4000008, core->gpu2D[0].writeBgCnt(0, IOWR_PARAMS))       // BG0CNT
            DEF_IO16(0x400000A, core->gpu2D[0].writeBgCnt(1, IOWR_PARAMS))       // BG1CNT
            DEF_IO16(0x400000C, core->gpu2D[0].writeBgCnt(2, IOWR_PARAMS))       // BG2CNT
            DEF_IO16(0x400000E, core->gpu2D[0].writeBgCnt(3, IOWR_PARAMS))       // BG3CNT
            DEF_IO16(0x4000010, core->gpu2D[0].writeBgHOfs(0, IOWR_PARAMS))      // BG0HOFS
            DEF_IO16(0x4000012, core->gpu2D[0].writeBgVOfs(0, IOWR_PARAMS))      // BG0VOFS
            DEF_IO16(0x4000014, core->gpu2D[0].writeBgHOfs(1, IOWR_PARAMS))      // BG1HOFS
            DEF_IO16(0x4000016, core->gpu2D[0].writeBgVOfs(1, IOWR_PARAMS))      // BG1VOFS
            DEF_IO16(0x4000018, core->gpu2D[0].writeBgHOfs(2, IOWR_PARAMS))      // BG2HOFS
            DEF_IO16(0x400001A, core->gpu2D[0].writeBgVOfs(2, IOWR_PARAMS))      // BG2VOFS
            DEF_IO16(0x400001C, core->gpu2D[0].writeBgHOfs(3, IOWR_PARAMS))      // BG3HOFS
            DEF_IO16(0x400001E, core->gpu2D[0].writeBgVOfs(3, IOWR_PARAMS))      // BG3VOFS
            DEF_IO16(0x4000020, core->gpu2D[0].writeBgPA(2, IOWR_PARAMS))        // BG2PA
            DEF_IO16(0x4000022, core->gpu2D[0].writeBgPB(2, IOWR_PARAMS))        // BG2PB
            DEF_IO16(0x4000024, core->gpu2D[0].writeBgPC(2, IOWR_PARAMS))        // BG2PC
            DEF_IO16(0x4000026, core->gpu2D[0].writeBgPD(2, IOWR_PARAMS))        // BG2PD
            DEF_IO32(0x4000028, core->gpu2D[0].writeBgX(2, IOWR_PARAMS))         // BG2X
            DEF_IO32(0x400002C, core->gpu2D[0].writeBgY(2, IOWR_PARAMS))         // BG2Y
            DEF_IO16(0x4000030, core->gpu2D[0].writeBgPA(3, IOWR_PARAMS))        // BG3PA
            DEF_IO16(0x4000032, core->gpu2D[0].writeBgPB(3, IOWR_PARAMS))        // BG3PB
            DEF_IO16(0x4000034, core->gpu2D[0].writeBgPC(3, IOWR_PARAMS))        // BG3PC
            DEF_IO16(0x4000036, core->gpu2D[0].writeBgPD(3, IOWR_PARAMS))        // BG3PD
            DEF_IO32(0x4000038, core->gpu2D[0].writeBgX(3, IOWR_PARAMS))         // BG3X
            DEF_IO32(0x400003C, core->gpu2D[0].writeBgY(3, IOWR_PARAMS))         // BG3Y
            DEF_IO16(0x4000040, core->gpu2D[0].writeWinH(0, IOWR_PARAMS))        // WIN0H
            DEF_IO16(0x4000042, core->gpu2D[0].writeWinH(1, IOWR_PARAMS))        // WIN1H
            DEF_IO16(0x4000044, core->gpu2D[0].writeWinV(0, IOWR_PARAMS))        // WIN0V
            DEF_IO16(0x4000046, core->gpu2D[0].writeWinV(1, IOWR_PARAMS))        // WIN1V
            DEF_IO16(0x4000048, core->gpu2D[0].writeWinIn(IOWR_PARAMS))          // WININ
            DEF_IO16(0x400004A, core->gpu2D[0].writeWinOut(IOWR_PARAMS))         // WINOUT
            DEF_IO16(0x400004C, core->gpu2D[0].writeMosaic(IOWR_PARAMS))         // MOSAIC
            DEF_IO16(0x4000050, core->gpu2D[0].writeBldCnt(IOWR_PARAMS))         // BLDCNT
            DEF_IO16(0x4000052, core->gpu2D[0].writeBldAlpha(IOWR_PARAMS))       // BLDALPHA
            DEF_IO_8(0x4000054, core->gpu2D[0].writeBldY(IOWR_PARAMS8))          // BLDY
            DEF_IO16(0x4000060, core->spu.writeGbaSoundCntL(0, IOWR_PARAMS8))    // SOUND0CNT_L
            DEF_IO16(0x4000062, core->spu.writeGbaSoundCntH(0, IOWR_PARAMS))     // SOUND0CNT_H
            DEF_IO16(0x4000064, core->spu.writeGbaSoundCntX(0, IOWR_PARAMS))     // SOUND0CNT_X
            DEF_IO16(0x4000068, core->spu.writeGbaSoundCntH(1, IOWR_PARAMS))     // SOUND1CNT_H
            DEF_IO16(0x400006C, core->spu.writeGbaSoundCntX(1, IOWR_PARAMS))     // SOUND1CNT_X
            DEF_IO16(0x4000070, core->spu.writeGbaSoundCntL(2, IOWR_PARAMS8))    // SOUND2CNT_L
            DEF_IO16(0x4000072, core->spu.writeGbaSoundCntH(2, IOWR_PARAMS))     // SOUND2CNT_H
            DEF_IO16(0x4000074, core->spu.writeGbaSoundCntX(2, IOWR_PARAMS))     // SOUND2CNT_X
            DEF_IO16(0x4000078, core->spu.writeGbaSoundCntH(3, IOWR_PARAMS))     // SOUND3CNT_H
            DEF_IO16(0x400007C, core->spu.writeGbaSoundCntX(3, IOWR_PARAMS))     // SOUND3CNT_X
            DEF_IO16(0x4000080, core->spu.writeGbaMainSoundCntL(IOWR_PARAMS))    // SOUNDCNT_L
            DEF_IO16(0x4000082, core->spu.writeGbaMainSoundCntH(IOWR_PARAMS))    // SOUNDCNT_H
            DEF_IO16(0x4000084, core->spu.writeGbaMainSoundCntX(IOWR_PARAMS8))   // SOUNDCNT_X
            DEF_IO16(0x4000088, core->spu.writeGbaSoundBias(IOWR_PARAMS))        // SOUNDBIAS
            DEF_IO_8(0x4000090, core->spu.writeGbaWaveRam(0,  IOWR_PARAMS8))     // WAVE_RAM
            DEF_IO_8(0x4000091, core->spu.writeGbaWaveRam(1,  IOWR_PARAMS8))     // WAVE_RAM
            DEF_IO_8(0x4000092, core->spu.writeGbaWaveRam(2,  IOWR_PARAMS8))     // WAVE_RAM
            DEF_IO_8(0x4000093, core->spu.writeGbaWaveRam(3,  IOWR_PARAMS8))     // WAVE_RAM
            DEF_IO_8(0x4000094, core->spu.writeGbaWaveRam(4,  IOWR_PARAMS8))     // WAVE_RAM
            DEF_IO_8(0x4000095, core->spu.writeGbaWaveRam(5,  IOWR_PARAMS8))     // WAVE_RAM
            DEF_IO_8(0x4000096, core->spu.writeGbaWaveRam(6,  IOWR_PARAMS8))     // WAVE_RAM
            DEF_IO_8(0x4000097, core->spu.writeGbaWaveRam(7,  IOWR_PARAMS8))     // WAVE_RAM
            DEF_IO_8(0x4000098, core->spu.writeGbaWaveRam(8,  IOWR_PARAMS8))     // WAVE_RAM
            DEF_IO_8(0x4000099, core->spu.writeGbaWaveRam(9,  IOWR_PARAMS8))     // WAVE_RAM
            DEF_IO_8(0x400009A, core->spu.writeGbaWaveRam(10, IOWR_PARAMS8))     // WAVE_RAM
            DEF_IO_8(0x400009B, core->spu.writeGbaWaveRam(11, IOWR_PARAMS8))     // WAVE_RAM
            DEF_IO_8(0x400009C, core->spu.writeGbaWaveRam(12, IOWR_PARAMS8))     // WAVE_RAM
            DEF_IO_8(0x400009D, core->spu.writeGbaWaveRam(13, IOWR_PARAMS8))     // WAVE_RAM
            DEF_IO_8(0x400009E, core->spu.writeGbaWaveRam(14, IOWR_PARAMS8))     // WAVE_RAM
            DEF_IO_8(0x400009F, core->spu.writeGbaWaveRam(15, IOWR_PARAMS8))     // WAVE_RAM
            DEF_IO32(0x40000A0, core->spu.writeGbaFifoA(IOWR_PARAMS))            // FIFO_A
            DEF_IO32(0x40000A4, core->spu.writeGbaFifoB(IOWR_PARAMS))            // FIFO_B
            DEF_IO32(0x40000B0, core->dma[1].writeDmaSad(0, IOWR_PARAMS))        // DMA0SAD
            DEF_IO32(0x40000B4, core->dma[1].writeDmaDad(0, IOWR_PARAMS))        // DMA0DAD
            DEF_IO32(0x40000B8, core->dma[1].writeDmaCnt(0, IOWR_PARAMS))        // DMA0CNT
            DEF_IO32(0x40000BC, core->dma[1].writeDmaSad(1, IOWR_PARAMS))        // DMA1SAD
            DEF_IO32(0x40000C0, core->dma[1].writeDmaDad(1, IOWR_PARAMS))        // DMA1DAD
            DEF_IO32(0x40000C4, core->dma[1].writeDmaCnt(1, IOWR_PARAMS))        // DMA1CNT
            DEF_IO32(0x40000C8, core->dma[1].writeDmaSad(2, IOWR_PARAMS))        // DMA2SAD
            DEF_IO32(0x40000CC, core->dma[1].writeDmaDad(2, IOWR_PARAMS))        // DMA2DAD
            DEF_IO32(0x40000D0, core->dma[1].writeDmaCnt(2, IOWR_PARAMS))        // DMA2CNT
            DEF_IO32(0x40000D4, core->dma[1].writeDmaSad(3, IOWR_PARAMS))        // DMA3SAD
            DEF_IO32(0x40000D8, core->dma[1].writeDmaDad(3, IOWR_PARAMS))        // DMA3DAD
            DEF_IO32(0x40000DC, core->dma[1].writeDmaCnt(3, IOWR_PARAMS))        // DMA3CNT
            DEF_IO16(0x4000100, core->timers[1].writeTmCntL(0, IOWR_PARAMS))     // TM0CNT_L
            DEF_IO16(0x4000102, core->timers[1].writeTmCntH(0, IOWR_PARAMS))     // TM0CNT_H
            DEF_IO16(0x4000104, core->timers[1].writeTmCntL(1, IOWR_PARAMS))     // TM1CNT_L
            DEF_IO16(0x4000106, core->timers[1].writeTmCntH(1, IOWR_PARAMS))     // TM1CNT_H
            DEF_IO16(0x4000108, core->timers[1].writeTmCntL(2, IOWR_PARAMS))     // TM2CNT_L
            DEF_IO16(0x400010A, core->timers[1].writeTmCntH(2, IOWR_PARAMS))     // TM2CNT_H
            DEF_IO16(0x400010C, core->timers[1].writeTmCntL(3, IOWR_PARAMS))     // TM3CNT_L
            DEF_IO16(0x400010E, core->timers[1].writeTmCntH(3, IOWR_PARAMS))     // TM3CNT_H
            DEF_IO16(0x4000200, core->interpreter[1].writeIe(IOWR_PARAMS))       // IE
            DEF_IO16(0x4000202, core->interpreter[1].writeIrf(IOWR_PARAMS))      // IF
            DEF_IO_8(0x4000208, core->interpreter[1].writeIme(IOWR_PARAMS8))     // IME
            DEF_IO_8(0x4000300, core->interpreter[1].writePostFlg(IOWR_PARAMS8)) // POSTFLG
            DEF_IO_8(0x4000301, writeGbaHaltCnt(IOWR_PARAMS8))                   // HALTCNT
            DEF_IO16(0x80000C4, core->rtc.writeGpData(IOWR_PARAMS))              // GP_DATA
            DEF_IO16(0x80000C6, core->rtc.writeGpDirection(IOWR_PARAMS))         // GP_DIRECTION
            DEF_IO16(0x80000C8, core->rtc.writeGpControl(IOWR_PARAMS))           // GP_CONTROL

            default:
                // Catch unknown writes
                if (i == 0)
                {
                    LOG("Unknown GBA I/O register write: 0x%X\n", address);
                    return;
                }

                // Ignore unknown writes if they occur after the first byte
                // This is in case, for example, a 16-bit register is accessed with a 32-bit write
                i++;
                continue;
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
    // Write to one of the VRAMCNT registers
    const uint8_t masks[] = { 0x9B, 0x9B, 0x9F, 0x9F, 0x87, 0x9F, 0x9F, 0x83, 0x83 };
    if ((value & masks[index]) == (vramCnt[index] & masks[index])) return;
    vramCnt[index] = value & masks[index];
    updateVram();
}

void Memory::writeWramCnt(uint8_t value)
{
    // Write to the WRAMCNT register
    wramCnt = value & 0x03;

    // Update the memory maps at the WRAM locations
    updateMap9<false>(0x03000000, 0x04000000);
    updateMap7(0x03000000, 0x04000000);
}

void Memory::writeHaltCnt(uint8_t value)
{
    // Write to the HALTCNT register
    haltCnt = value & 0xC0;

    // Change the ARM7's power mode
    switch (haltCnt >> 6)
    {
        case 1: // GBA
            core->enterGbaMode();
            break;

        case 2: // Halt
            core->interpreter[1].halt(0);
            break;

        case 3: // Sleep
            LOG("Unhandled request for sleep mode\n");
            break;
    }
}

void Memory::writeGbaHaltCnt(uint8_t value)
{
    // Halt the CPU
    core->interpreter[1].halt(0);

    if (value & BIT(7)) // Stop
        LOG("Unhandled request for stop mode\n");
}
