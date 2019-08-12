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
#include "cp15.h"
#include "interpreter.h"
#include "gpu.h"
#include "memory.h"
#include "memory_transfer.h"

namespace core
{

bool init()
{
    // Initialize everything
    cp15::init();
    interpreter::init();
    memory::init();
    memory_transfer::init();

    // Load the ARM9 BIOS
    FILE *bios9File = fopen("bios9.bin", "rb");
    if (!bios9File) return false;
    fread(memory::bios9, sizeof(uint8_t), 0x1000, bios9File);
    fclose(bios9File);

    // Load the ARM7 BIOS
    FILE *bios7File = fopen("bios7.bin", "rb");
    if (!bios7File) return false;
    fread(memory::bios7, sizeof(uint8_t), 0x4000, bios7File);
    fclose(bios7File);

    // Load the firmware
    FILE *firmwareFile = fopen("firmware.bin", "rb");
    if (!firmwareFile) return false;
    fread(memory_transfer::firmware, sizeof(uint8_t), 0x40000, firmwareFile);
    fclose(firmwareFile);

    return true;
}

bool loadRom(char *filename)
{
    // Load the ROM
    FILE *romFile = fopen(filename, "rb");
    if (!romFile) return false;
    fseek(romFile, 0, SEEK_END);
    int size = ftell(romFile);
    if (memory_transfer::rom) delete[] memory_transfer::rom;
    memory_transfer::rom = new uint8_t[size];
    fseek(romFile, 0, SEEK_SET);
    fread(memory_transfer::rom, sizeof(uint8_t), size, romFile);
    fclose(romFile);

    // Set some register values as the BIOS/firmware would
    memory::write<uint8_t>(&interpreter::arm9, 0x4000247, 0x03); // WRAMCNT
    memory::write<uint8_t>(&interpreter::arm9, 0x4000300, 0x01); // POSTFLG_9
    memory::write<uint8_t>(&interpreter::arm7, 0x4000300, 0x01); // POSTFLG_7
    cp15::writeRegister(9, 1, 0, 0x027C0005); // Data TCM base/size
    cp15::writeRegister(9, 1, 1, 0x00000010); // Instruction TCM base/size

    // Load the initial ROM code into memory using values from the header
    for (unsigned int i = 0; i < *(uint32_t*)&memory_transfer::rom[0x2C]; i++)
    {
        memory::write<uint8_t>(&interpreter::arm9, *(uint32_t*)&memory_transfer::rom[0x28] + i,
                               memory_transfer::rom[*(uint32_t*)&memory_transfer::rom[0x20] + i]);
    }
    for (unsigned int i = 0; i < *(uint32_t*)&memory_transfer::rom[0x3C]; i++)
    {
        memory::write<uint8_t>(&interpreter::arm7, *(uint32_t*)&memory_transfer::rom[0x38] + i,
                               memory_transfer::rom[*(uint32_t*)&memory_transfer::rom[0x30] + i]);
    }

    // Point the ARM9 to its starting execution point and initialize the stack pointers
    interpreter::arm9.registersUsr[12] = *(uint32_t*)&memory_transfer::rom[0x24];
    interpreter::arm9.registersUsr[13] = 0x03002F7C;
    interpreter::arm9.registersUsr[14] = *(uint32_t*)&memory_transfer::rom[0x24];
    interpreter::arm9.registersUsr[15] = *(uint32_t*)&memory_transfer::rom[0x24] + 8;
    interpreter::arm9.registersIrq[0]  = 0x03003F80;
    interpreter::arm9.registersSvc[0]  = 0x03003FC0;
    interpreter::setMode(&interpreter::arm9, 0x1F);

    // Point the ARM7 to its starting execution point and initialize the stack pointers
    interpreter::arm7.registersUsr[12] = *(uint32_t*)&memory_transfer::rom[0x34];
    interpreter::arm7.registersUsr[13] = 0x0380FD80;
    interpreter::arm7.registersUsr[14] = *(uint32_t*)&memory_transfer::rom[0x34];
    interpreter::arm7.registersUsr[15] = *(uint32_t*)&memory_transfer::rom[0x34] + 8;
    interpreter::arm7.registersIrq[0]  = 0x0380FF80;
    interpreter::arm7.registersSvc[0]  = 0x0380FFC0;
    interpreter::setMode(&interpreter::arm7, 0x1F);

    return true;
}

void timerTick(interpreter::Cpu *cpu, uint8_t timer)
{
    // Don't tick normally if count-up timing is enabled
    if (timer > 0 && (*cpu->tmcntH[timer] & BIT(2)))
        return;

    // Timers can tick at frequencies of f/1, f/64, f/256, or f/1024
    // For slower frequencies, increment the scaler value until it reaches the desired amount, and only then tick
    if ((*cpu->tmcntH[timer] & 0x0003) > 0)
    {
        cpu->timerScalers[timer]++;
        if (cpu->timerScalers[timer] >= 0x10 << ((*cpu->tmcntH[timer] & 0x0003) * 2))
            cpu->timerScalers[timer] = 0;
        else
            return;
    }

    // Increment and handle overflows
    (*cpu->tmcntL[timer])++;
    if (*cpu->tmcntL[timer] == 0) // Overflow
    {
        // Reload the timer
        *cpu->tmcntL[timer] = cpu->timerReloads[timer];
        if (*cpu->tmcntH[timer] & BIT(6)) // Timer overflow IRQ
            *cpu->irf |= BIT(3 + timer);

        // Count-up timing means the timer only ticks when the previous timer overflows
        // If the next timer has count-up timing enabled, now's the time to tick it
        if (timer < 3 && (*cpu->tmcntH[timer + 1] & BIT(2)))
        {
            (*cpu->tmcntL[timer + 1])++;
            if (*cpu->tmcntL[timer + 1] == 0) // Overflow
            {
                // Reload the timer
                *cpu->tmcntL[timer + 1] = cpu->timerReloads[timer + 1];
                if (*cpu->tmcntH[timer + 1] & BIT(6)) // Timer overflow IRQ
                    *cpu->irf |= BIT(3 + timer + 1);
            }
        }
    }
}

void runScanline()
{
    for (int i = 0; i < 355 * 6; i++)
    {
        // Run the CPUs, with the ARM7 running at half the frequency of the ARM9
        interpreter::execute(&interpreter::arm9);
        if (i % 2 == 0)
            interpreter::execute(&interpreter::arm7);

        // The end of the visible scanline, and the start of H-blank
        if (i == 256 * 6)
            gpu::scanline256();

        for (int j = 0; j < 4; j++)
        {
            // Tick the timers if they're enabled
            if (*interpreter::arm9.tmcntH[j] & BIT(7))
                timerTick(&interpreter::arm9, j);
            if (*interpreter::arm7.tmcntH[j] & BIT(7))
                timerTick(&interpreter::arm7, j);

            // Perform DMA transfers if they're enabled
            if (*interpreter::arm9.dmacnt[j] & BIT(31))
                memory_transfer::dmaTransfer(&interpreter::arm9, j);
            if (*interpreter::arm7.dmacnt[j] & BIT(31))
                memory_transfer::dmaTransfer(&interpreter::arm7, j);
        }
    }

    // The end of the scanline
    gpu::scanline355();
}

void pressKey(uint8_t key)
{
    // Clear key bits to indicate presses
    if (key < 10) // A, B, select, start, right, left, up, down, R, L
    {
        *interpreter::arm9.keyinput &= ~BIT(key);
        *interpreter::arm7.keyinput &= ~BIT(key);
    }
    else if (key < 12) // X, Y
    {
        *memory::extkeyin &= ~BIT(key - 10);
    }
}

void releaseKey(uint8_t key)
{
    // Set key bits to indicate releases
    if (key < 10) // A, B, select, start, right, left, up, down, R, L
    {
        *interpreter::arm9.keyinput |= BIT(key);
        *interpreter::arm7.keyinput |= BIT(key);
    }
    else if (key < 12) // X, Y
    {
        *memory::extkeyin |= BIT(key - 10);
    }
}

}
