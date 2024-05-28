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

#include <cmath>

#include "bios.h"
#include "core.h"

// HLE ARM9 BIOS SWI lookup table
int (Bios::*Bios::swiTable9[])(uint32_t**) =
{
    &Bios::swiUnknown,       &Bios::swiUnknown,        &Bios::swiUnknown,      &Bios::swiWaitByLoop, // 0x00-0x03
    &Bios::swiInterruptWait, &Bios::swiVBlankIntrWait, &Bios::swiHalt,         &Bios::swiUnknown,    // 0x04-0x07
    &Bios::swiUnknown,       &Bios::swiDivide,         &Bios::swiUnknown,      &Bios::swiCpuSet,     // 0x08-0x0B
    &Bios::swiCpuFastSet,    &Bios::swiSquareRoot,     &Bios::swiGetCrc16,     &Bios::swiIsDebugger, // 0x0C-0x0F
    &Bios::swiBitUnpack,     &Bios::swiLz77Uncomp,     &Bios::swiLz77Uncomp,   &Bios::swiHuffUncomp, // 0x10-0x13
    &Bios::swiRunlenUncomp,  &Bios::swiRunlenUncomp,   &Bios::swiDiffUnfilt8,  &Bios::swiUnknown,    // 0x14-0x17
    &Bios::swiDiffUnfilt16,  &Bios::swiUnknown,        &Bios::swiUnknown,      &Bios::swiUnknown,    // 0x18-0x1B
    &Bios::swiUnknown,       &Bios::swiUnknown,        &Bios::swiUnknown,      &Bios::swiUnknown,    // 0x1C-0x1F
    &Bios::swiUnknown                                                                                // 0x20
};

// HLE ARM7 BIOS SWI lookup table
int (Bios::*Bios::swiTable7[])(uint32_t**) =
{
    &Bios::swiUnknown,        &Bios::swiUnknown,        &Bios::swiUnknown,      &Bios::swiWaitByLoop,    // 0x00-0x03
    &Bios::swiInterruptWait,  &Bios::swiVBlankIntrWait, &Bios::swiHalt,         &Bios::swiSleep,         // 0x04-0x07
    &Bios::swiSoundBias,      &Bios::swiDivide,         &Bios::swiUnknown,      &Bios::swiCpuSet,        // 0x08-0x0B
    &Bios::swiCpuFastSet,     &Bios::swiSquareRoot,     &Bios::swiGetCrc16,     &Bios::swiIsDebugger,    // 0x0C-0x0F
    &Bios::swiBitUnpack,      &Bios::swiLz77Uncomp,     &Bios::swiLz77Uncomp,   &Bios::swiHuffUncomp,    // 0x10-0x13
    &Bios::swiRunlenUncomp,   &Bios::swiRunlenUncomp,   &Bios::swiUnknown,      &Bios::swiUnknown,       // 0x14-0x17
    &Bios::swiUnknown,        &Bios::swiUnknown,        &Bios::swiGetSineTable, &Bios::swiGetPitchTable, // 0x18-0x1B
    &Bios::swiGetVolumeTable, &Bios::swiUnknown,        &Bios::swiUnknown,      &Bios::swiUnknown,       // 0x1C-0x1F
    &Bios::swiUnknown                                                                                    // 0x20
};

// HLE GBA BIOS SWI lookup table
int (Bios::*Bios::swiTableGba[])(uint32_t**) =
{
    &Bios::swiUnknown,       &Bios::swiRegRamReset,    &Bios::swiHalt,        &Bios::swiSleep,        // 0x00-0x03
    &Bios::swiInterruptWait, &Bios::swiVBlankIntrWait, &Bios::swiDivide,      &Bios::swiDivArm,       // 0x04-0x07
    &Bios::swiSquareRoot,    &Bios::swiArcTan,         &Bios::swiArcTan2,     &Bios::swiCpuSet,       // 0x08-0x0B
    &Bios::swiCpuFastSet,    &Bios::swiUnknown,        &Bios::swiBgAffineSet, &Bios::swiObjAffineSet, // 0x0C-0x0F
    &Bios::swiBitUnpack,     &Bios::swiLz77Uncomp,     &Bios::swiLz77Uncomp,  &Bios::swiHuffUncomp,   // 0x10-0x13
    &Bios::swiRunlenUncomp,  &Bios::swiRunlenUncomp,   &Bios::swiDiffUnfilt8, &Bios::swiDiffUnfilt8,  // 0x14-0x17
    &Bios::swiDiffUnfilt16,  &Bios::swiSoundBias,      &Bios::swiUnknown,     &Bios::swiUnknown,      // 0x18-0x1B
    &Bios::swiUnknown,       &Bios::swiUnknown,        &Bios::swiUnknown,     &Bios::swiUnknown,      // 0x1C-0x1F
    &Bios::swiUnknown                                                                                 // 0x20
};

int Bios::execute(uint8_t vector, uint32_t **registers)
{
    // Execute the HLE version of the given exception vector
    switch (vector)
    {
        case 0x08: // SWI
        {
            // The PC was adjusted for an exception, so adjust it back
            *registers[15] += 4;

            // Use the comment from the SWI opcode to lookup what function to execute
            uint32_t address = *registers[15] - (core->interpreter[arm7].isThumb() ? 4 : 6);
            uint8_t comment = core->memory.read<uint8_t>(arm7, address);
            return (this->*swiTable[std::min<uint8_t>(comment, 0x20)])(registers);
        }

        case 0x18: // IRQ
            // Let the interpreter handle HLE interrupts
            return core->interpreter[arm7].handleHleIrq();

        default:
            LOG("Unimplemented ARM%d BIOS vector: 0x%02X\n", (arm7 ? 7 : 9), vector);
            return 3;
    }
}

void Bios::checkWaitFlags()
{
    // Read the BIOS interrupt flags from memory
    uint32_t address = arm7 ? 0x3FFFFF8 : (core->cp15.getDtcmAddr() + 0x3FF8);
    uint32_t flags = core->memory.read<uint32_t>(arm7, address);

    // If a flag being waited for is set, clear it and stop waiting
    if (flags & waitFlags)
    {
        core->memory.write<uint32_t>(arm7, address, flags & ~waitFlags);
        waitFlags = 0;
        return;
    }

    // Continue waiting until a flag is set
    core->interpreter[arm7].halt(0);
}

int Bios::swiRegRamReset(uint32_t **registers)
{
    // Enable forced blank for PPU memory access
    core->memory.write<uint16_t>(arm7, 0x4000000, 0x80);

    // Clear GBA on-board WRAM if bit 0 is set
    if (*registers[0] & BIT(0))
        for (uint32_t i = 0x2000000; i < 0x2040000; i += 4)
            core->memory.write<uint32_t>(arm7, i, 0);

    // Clear GBA on-chip WRAM if bit 1 is set
    if (*registers[0] & BIT(1))
        for (uint32_t i = 0x3000000; i < 0x3007E00; i += 4)
            core->memory.write<uint32_t>(arm7, i, 0);

    // Clear GBA palette if bit 2 is set
    if (*registers[0] & BIT(2))
        for (uint32_t i = 0x5000000; i < 0x5000400; i += 4)
            core->memory.write<uint32_t>(arm7, i, 0);

    // Clear GBA VRAM if bit 3 is set
    if (*registers[0] & BIT(3))
        for (uint32_t i = 0x6000000; i < 0x6018000; i += 4)
            core->memory.write<uint32_t>(arm7, i, 0);

    // Clear GBA OAM if bit 4 is set
    if (*registers[0] & BIT(4))
        for (uint32_t i = 0x7000000; i < 0x7000800; i += 4)
            core->memory.write<uint32_t>(arm7, i, 0);

    // Don't handle register resets for now
    if (uint8_t bits = *registers[0] & 0xE0)
        LOG("Unimplemented GBA HLE reset bits: 0x%X\n", bits);
    return 3;
}

int Bios::swiWaitByLoop(uint32_t **registers)
{
    // Wait 4 cycles for each loop iteration (1 for subtraction, 3 for branch)
    uint32_t loops = *registers[0];
    *registers[0] = 0;
    return loops * 4 + 3;
}

int Bios::swiInterruptWait(uint32_t **registers)
{
    // Set the flags to wait for and start waiting
    waitFlags = *registers[1];
    core->interpreter[arm7].halt(0);

    if (*registers[0]) // Discard old
    {
        // Clear old flags and continue waiting for a new one
        checkWaitFlags();
        waitFlags = *registers[1];
        core->interpreter[arm7].halt(0);
    }
    else if (arm7)
    {
        // Check old flags and don't wait if one is already set
        // This is bugged on ARM9; it always waits for at least one interrupt
        checkWaitFlags();
    }
    return 3;
}

int Bios::swiVBlankIntrWait(uint32_t **registers)
{
    // Wait until a new V-blank interrupt occurs
    *registers[0] = 1;
    *registers[1] = 1;
    return swiInterruptWait(registers);
}

int Bios::swiHalt(uint32_t **registers)
{
    // Halt the CPU
    core->interpreter[arm7].halt(0);
    return 3;
}

int Bios::swiSleep(uint32_t **registers)
{
    // Put the ARM7 in sleep mode
    core->memory.write<uint8_t>(1, 0x4000301, 0xC0); // HALTCNT
    return 3;
}

int Bios::swiSoundBias(uint32_t **registers)
{
    // Set the sound bias value
    // Actual BIOS adjusts this over time with delay, but oh well
    core->memory.write<uint16_t>(1, 0x4000504, *registers[0] ? 0x200 : 0); // SOUNDBIAS
    return 3;
}

int Bios::swiDivide(uint32_t **registers)
{
    // Calculate the division result and remainder
    int32_t div = int32_t(*registers[0]) / int32_t(*registers[1]);
    int32_t mod = int32_t(*registers[0]) % int32_t(*registers[1]);

    // Return the results
    *registers[0] = div;
    *registers[1] = mod;
    *registers[3] = abs(div);
    return 3;
}

int Bios::swiDivArm(uint32_t **registers)
{
    // Divide with numerator and denominator swapped
    SWAP(*registers[0], *registers[1]);
    return swiDivide(registers);
}

int Bios::swiSquareRoot(uint32_t **registers)
{
    // Calculate the square root result
    *registers[0] = sqrt(*registers[0]);
    return 3;
}

int Bios::swiArcTan(uint32_t **registers)
{
    // Calculate the inverse of a fixed-point tangent
    int32_t square = -(int32_t(*registers[0] * *registers[0]) >> 14);
    int32_t result = ((square * 0xA9) >> 14) + 0x390;
    result = ((result * square) >> 14) + 0x91C;
    result = ((result * square) >> 14) + 0xFB6;
    result = ((result * square) >> 14) + 0x16AA;
    result = ((result * square) >> 14) + 0x2081;
    result = ((result * square) >> 14) + 0x3651;
    result = ((result * square) >> 14) + 0xA2F9;
    *registers[0] = int32_t(*registers[0] * result) >> 16;
    return 3;
}

int Bios::swiArcTan2(uint32_t **registers)
{
    // Define parameters for calculating inverse tangent with correction processing
    static const uint8_t offsets[] = { 0, 1, 1, 2, 2, 3, 3, 4 };
    int32_t x = *registers[0];
    int32_t y = *registers[1];
    uint8_t octant = 0;

    // Determine which octant the angle resides in
    octant += (y < 0) << 2;
    octant += ((x ^ y) < 0) << 1;
    octant += ((x ^ y ^ (abs(x) - abs(y))) < 0);

    // Calculate a tangent within -pi/4 and pi/4, swapping parameters if necessary
    bool swap = (abs(x) >= abs(y));
    if (swap) SWAP(x, y);
    *registers[0] = y ? ((x << 14) / y) : 0;

    // Calculate the tangent's inverse and adjust based on octant
    swiArcTan(registers);
    if (!swap) *registers[0] = -*registers[0];
    *registers[0] += offsets[octant] << 14;
    return 3;
}

int Bios::swiCpuSet(uint32_t **registers)
{
    // Decode some parameters
    bool word = (*registers[2] & BIT(26));
    bool fixed = (*registers[2] & BIT(24));
    uint32_t size = (*registers[2] & 0xFFFFF) << (1 + word);

    if (word)
    {
        // Copy/fill memory from the source to the destination (32-bit)
        for (uint32_t i = 0; i < size; i += 4)
        {
            uint32_t address = *registers[0] + (fixed ? 0 : i);
            uint32_t value = core->memory.read<uint32_t>(arm7, address);
            core->memory.write<uint32_t>(arm7, *registers[1] + i, value);
        }
    }
    else
    {
        // Copy/fill memory from the source to the destination (16-bit)
        for (uint32_t i = 0; i < size; i += 2)
        {
            uint32_t address = *registers[0] + (fixed ? 0 : i);
            uint16_t value = core->memory.read<uint16_t>(arm7, address);
            core->memory.write<uint16_t>(arm7, *registers[1] + i, value);
        }
    }
    return 3;
}

int Bios::swiCpuFastSet(uint32_t **registers)
{
    // Decode some parameters
    bool fixed = (*registers[2] & BIT(24));
    uint32_t size = (*registers[2] & 0xFFFFF) << 2;

    // Copy/fill memory from the source to the destination
    for (uint32_t i = 0; i < size; i += 4)
    {
        uint32_t address = *registers[0] + (fixed ? 0 : i);
        uint32_t value = core->memory.read<uint32_t>(arm7, address);
        core->memory.write<uint32_t>(arm7, *registers[1] + i, value);
    }
    return 3;
}

int Bios::swiGetCrc16(uint32_t **registers)
{
    static const uint16_t table[] = { 0xC0C1, 0xC181, 0xC301, 0xC601, 0xCC01, 0xD801, 0xF001, 0xA001 };

    // Calculate a CRC16 value for the given data
    for (size_t i = 0; i < *registers[2]; i++)
    {
        *registers[0] ^= core->memory.read<uint8_t>(arm7, *registers[1] + i);
        for (size_t j = 0; j < 8; j++)
            *registers[0] = (*registers[0] >> 1) ^ ((*registers[0] & 1) ? (table[j] << (7 - j)) : 0);
    }
    return 3;
}

int Bios::swiIsDebugger(uint32_t **registers)
{
    // Report that this isn't a debugger
    *registers[0] = 0;
    return 3;
}

// Affine table taken from the open-source Cult of GBA BIOS
const uint16_t Bios::affineTable[0x100] =
{
    0x0000, 0x0192, 0x0323, 0x04B5, 0x0645, 0x07D5, 0x0964, 0x0AF1,
    0x0C7C, 0x0E05, 0x0F8C, 0x1111, 0x1294, 0x1413, 0x158F, 0x1708,
    0x187D, 0x19EF, 0x1B5D, 0x1CC6, 0x1E2B, 0x1F8B, 0x20E7, 0x223D,
    0x238E, 0x24DA, 0x261F, 0x275F, 0x2899, 0x29CD, 0x2AFA, 0x2C21,
    0x2D41, 0x2E5A, 0x2F6B, 0x3076, 0x3179, 0x3274, 0x3367, 0x3453,
    0x3536, 0x3612, 0x36E5, 0x37AF, 0x3871, 0x392A, 0x39DA, 0x3A82,
    0x3B20, 0x3BB6, 0x3C42, 0x3CC5, 0x3D3E, 0x3DAE, 0x3E14, 0x3E71,
    0x3EC5, 0x3F0E, 0x3F4E, 0x3F84, 0x3FB1, 0x3FD3, 0x3FEC, 0x3FFB,
    0x4000, 0x3FFB, 0x3FEC, 0x3FD3, 0x3FB1, 0x3F84, 0x3F4E, 0x3F0E,
    0x3EC5, 0x3E71, 0x3E14, 0x3DAE, 0x3D3E, 0x3CC5, 0x3C42, 0x3BB6,
    0x3B20, 0x3A82, 0x39DA, 0x392A, 0x3871, 0x37AF, 0x36E5, 0x3612,
    0x3536, 0x3453, 0x3367, 0x3274, 0x3179, 0x3076, 0x2F6B, 0x2E5A,
    0x2D41, 0x2C21, 0x2AFA, 0x29CD, 0x2899, 0x275F, 0x261F, 0x24DA,
    0x238E, 0x223D, 0x20E7, 0x1F8B, 0x1E2B, 0x1CC6, 0x1B5D, 0x19EF,
    0x187D, 0x1708, 0x158F, 0x1413, 0x1294, 0x1111, 0x0F8C, 0x0E05,
    0x0C7C, 0x0AF1, 0x0964, 0x07D5, 0x0645, 0x04B5, 0x0323, 0x0192,
    0x0000, 0xFE6E, 0xFCDD, 0xFB4B, 0xF9BB, 0xF82B, 0xF69C, 0xF50F,
    0xF384, 0xF1FB, 0xF074, 0xEEEF, 0xED6C, 0xEBED, 0xEA71, 0xE8F8,
    0xE783, 0xE611, 0xE4A3, 0xE33A, 0xE1D5, 0xE075, 0xDF19, 0xDDC3,
    0xDC72, 0xDB26, 0xD9E1, 0xD8A1, 0xD767, 0xD633, 0xD506, 0xD3DF,
    0xD2BF, 0xD1A6, 0xD095, 0xCF8A, 0xCE87, 0xCD8C, 0xCC99, 0xCBAD,
    0xCACA, 0xC9EE, 0xC91B, 0xC851, 0xC78F, 0xC6D6, 0xC626, 0xC57E,
    0xC4E0, 0xC44A, 0xC3BE, 0xC33B, 0xC2C2, 0xC252, 0xC1EC, 0xC18F,
    0xC13B, 0xC0F2, 0xC0B2, 0xC07C, 0xC04F, 0xC02D, 0xC014, 0xC005,
    0xC000, 0xC005, 0xC014, 0xC02D, 0xC04F, 0xC07C, 0xC0B2, 0xC0F2,
    0xC13B, 0xC18F, 0xC1EC, 0xC252, 0xC2C2, 0xC33B, 0xC3BE, 0xC44A,
    0xC4E0, 0xC57E, 0xC626, 0xC6D6, 0xC78F, 0xC851, 0xC91B, 0xC9EE,
    0xCACA, 0xCBAD, 0xCC99, 0xCD8C, 0xCE87, 0xCF8A, 0xD095, 0xD1A6,
    0xD2BF, 0xD3DF, 0xD506, 0xD633, 0xD767, 0xD8A1, 0xD9E1, 0xDB26,
    0xDC72, 0xDDC3, 0xDF19, 0xE075, 0xE1D5, 0xE33A, 0xE4A3, 0xE611,
    0xE783, 0xE8F8, 0xEA71, 0xEBED, 0xED6C, 0xEEEF, 0xF074, 0xF1FB,
    0xF384, 0xF50F, 0xF69C, 0xF82B, 0xF9BB, 0xFB4B, 0xFCDD, 0xFE6E
};

int Bios::swiBgAffineSet(uint32_t **registers)
{
    // Process the specified number of background affine parameters
    for (int i = 0; i < *registers[2]; i++)
    {
        // Read the input parameters
        int32_t origX = core->memory.read<uint32_t>(arm7, *registers[0] + i * 18 + 0);
        int32_t origY = core->memory.read<uint32_t>(arm7, *registers[0] + i * 18 + 4);
        int16_t dispX = core->memory.read<uint16_t>(arm7, *registers[0] + i * 18 + 8);
        int16_t dispY = core->memory.read<uint16_t>(arm7, *registers[0] + i * 18 + 10);
        int16_t scaleX = core->memory.read<uint16_t>(arm7, *registers[0] + i * 18 + 12);
        int16_t scaleY = core->memory.read<uint16_t>(arm7, *registers[0] + i * 18 + 14);
        uint16_t angle = core->memory.read<uint16_t>(arm7, *registers[0] + i * 18 + 16);

        // Look up sin and cos values for the given angle
        int16_t sin = affineTable[angle >>= 8];
        int16_t cos = affineTable[(angle + 0x40) & 0xFF];
        int16_t a, b, c, d;

        // Calculate and write the output parameters
        core->memory.write<uint16_t>(arm7, *registers[1] + i * 16 + 0, a = (cos * scaleX) >> 14);
        core->memory.write<uint16_t>(arm7, *registers[1] + i * 16 + 2, b = -(sin * scaleX) >> 14);
        core->memory.write<uint16_t>(arm7, *registers[1] + i * 16 + 4, c = (sin * scaleY) >> 14);
        core->memory.write<uint16_t>(arm7, *registers[1] + i * 16 + 6, d = (cos * scaleY) >> 14);
        core->memory.write<uint32_t>(arm7, *registers[1] + i * 16 + 8, origX - dispX * a - dispY * b);
        core->memory.write<uint32_t>(arm7, *registers[1] + i * 16 + 12, origY - dispX * c - dispY * d);
    }
    return 3;
}

int Bios::swiObjAffineSet(uint32_t **registers)
{
    // Process the specified number of object affine parameters
    for (int i = 0; i < *registers[2]; i++)
    {
        // Read the input parameters
        int16_t scaleX = core->memory.read<uint16_t>(arm7, *registers[0] + i * 6 + 0);
        int16_t scaleY = core->memory.read<uint16_t>(arm7, *registers[0] + i * 6 + 2);
        uint16_t angle = core->memory.read<uint16_t>(arm7, *registers[0] + i * 6 + 4);

        // Look up sin and cos values for the given angle
        int16_t sin = affineTable[angle >>= 8];
        int16_t cos = affineTable[(angle + 0x40) & 0xFF];

        // Calculate and write the output parameters
        core->memory.write<uint16_t>(arm7, *registers[1] + *registers[3] * (i * 4 + 0), (cos * scaleX) >> 14);
        core->memory.write<uint16_t>(arm7, *registers[1] + *registers[3] * (i * 4 + 1), -(sin * scaleX) >> 14);
        core->memory.write<uint16_t>(arm7, *registers[1] + *registers[3] * (i * 4 + 2), (sin * scaleY) >> 14);
        core->memory.write<uint16_t>(arm7, *registers[1] + *registers[3] * (i * 4 + 3), (cos * scaleY) >> 14);
    }
    return 3;
}

int Bios::swiBitUnpack(uint32_t **registers)
{
    // Read the parameters from memory
    uint16_t size = core->memory.read<uint16_t>(arm7, *registers[2]);
    uint8_t srcWidth = core->memory.read<uint8_t>(arm7, *registers[2] + 2);
    uint8_t dstWidth = core->memory.read<uint8_t>(arm7, *registers[2] + 3);
    uint32_t offset = core->memory.read<uint32_t>(arm7, *registers[2] + 4);

    uint32_t dst = 0;
    uint32_t dstValue = 0;
    uint8_t dstBits = 0;

    for (uint32_t src = 0; src < size; src++)
    {
        // Read 8 bits of source data
        uint8_t srcValue = core->memory.read<uint8_t>(arm7, *registers[0] + src);

        for (uint8_t srcBits = 0; srcBits < 8; srcBits += srcWidth)
        {
            // Isolate a single value from the source
            uint32_t value = srcValue << (dstWidth - srcWidth);
            uint32_t mask = (1 << dstWidth) - 1;
            value &= mask;

            // Apply the offset if non-zero or the zero flag is set
            if (value || (offset & BIT(31)))
                value = (value + offset) & mask;

            // Add the value to the destination data
            dstValue |= value << dstBits;
            dstBits += dstWidth;

            // Flush the destination data once there are 32 bits
            if (dstBits == 32)
            {
                core->memory.write<uint32_t>(arm7, *registers[1] + dst, dstValue);
                dst += 4;
                dstValue = 0;
                dstBits = 0;
            }

            // Move to the next source value
            srcValue >>= srcWidth;
        }
    }
    return 3;
}

int Bios::swiLz77Uncomp(uint32_t **registers)
{
    // Get the size from the header and set the initial addresses
    uint32_t size = core->memory.read<uint32_t>(arm7, *registers[0]) >> 8;
    uint32_t src = 4;
    uint32_t dst = 0;

    while (true)
    {
        // Read the flags for the next 8 sections
        uint16_t flags = core->memory.read<uint8_t>(arm7, *registers[0] + src++);

        for (uint32_t i = 0; i < 8; i++)
        {
            // Finish once the destination size is reached
            if (dst >= size)
                return 3;

            if ((flags <<= 1) & BIT(8)) // Next flag
            {
                // Decode some parameters
                uint8_t val1 = core->memory.read<uint8_t>(arm7, *registers[0] + src++);
                uint8_t val2 = core->memory.read<uint8_t>(arm7, *registers[0] + src++);
                uint8_t size = 3 + ((val1 >> 4) & 0xF);
                uint16_t offset = 1 + ((val1 & 0xF) << 8) + val2;

                // Repeat a group of bytes from a previous offset in the destination
                for (uint32_t j = 0; j < size; j++)
                {
                    uint8_t value = core->memory.read<uint8_t>(arm7, *registers[1] + dst - offset);
                    core->memory.write<uint8_t>(arm7, *registers[1] + dst++, value);
                }
            }
            else
            {
                // Copy a new byte from the source to the destination
                uint8_t value = core->memory.read<uint8_t>(arm7, *registers[0] + src++);
                core->memory.write<uint8_t>(arm7, *registers[1] + dst++, value);
            }
        }
    }
}

int Bios::swiHuffUncomp(uint32_t **registers)
{
    // Read the header and set size parameters
    uint32_t header = core->memory.read<uint32_t>(arm7, *registers[0]);
    uint8_t treeSize = core->memory.read<uint8_t>(arm7, *registers[0] + 4);
    uint8_t dataSize = header & 0xF;
    uint8_t wordCount = 32 / dataSize;
    uint8_t count = 0;

    // Set the initial addresses for decompression
    uint32_t nodeAddress = *registers[0] + 5;
    uint32_t bitsAddress = *registers[0] + (treeSize << 1) + 7;
    uint32_t outAddress = *registers[1];
    uint32_t endAddress = *registers[1] + (header >> 8);
    uint32_t buffer = 0;

    while (true)
    {
        // Read the next set of node bits
        uint32_t bits = core->memory.read<uint32_t>(arm7, bitsAddress);
        bitsAddress += 4;

        // Process the node bits
        for (int i = 0; i < 32; i++)
        {
            // Move to the next node based on the current node and bit
            uint8_t bit = (bits >> 31);
            uint8_t node = core->memory.read<uint8_t>(arm7, nodeAddress);
            nodeAddress = (nodeAddress & ~0x1) + bit + ((node & 0x3F) << 1) + 2;
            bits <<= 1;

            // Push data to the buffer when reached and return to the root node
            if (~node & BIT(7 - bit)) continue;
            node = core->memory.read<uint8_t>(arm7, nodeAddress);
            buffer = (buffer >> dataSize) | (node << (32 - dataSize));
            nodeAddress = *registers[0] + 5;

            // Write the buffer to memory when it's full and stop when finished
            if (++count != wordCount) continue;
            core->memory.write<uint32_t>(arm7, outAddress, buffer);
            if ((outAddress += 4) >= endAddress) return 3;
            count = 0;
        }
    }
}

int Bios::swiRunlenUncomp(uint32_t **registers)
{
    // Get the size from the header and set the initial addresses
    uint32_t size = core->memory.read<uint32_t>(arm7, *registers[0]) >> 8;
    uint32_t src = 4;
    uint32_t dst = 0;

    while (dst < size)
    {
        // Read the flags for the next section
        uint8_t flags = core->memory.read<uint8_t>(arm7, *registers[0] + src++);

        if (flags & BIT(7)) // Compressed
        {
            // Fill a length of destination data with the same source value
            uint8_t value = core->memory.read<uint8_t>(arm7, *registers[0] + src++);
            for (uint32_t j = 0; j < (flags & 0x7F) + 3; j++)
                core->memory.write<uint8_t>(arm7, *registers[1] + dst++, value);
        }
        else
        {
            // Copy a length of uncompressed data from the source to the destination
            for (uint32_t j = 0; j < (flags & 0x7F) + 1; j++)
            {
                uint8_t value = core->memory.read<uint8_t>(arm7, *registers[0] + src++);
                core->memory.write<uint8_t>(arm7, *registers[1] + dst++, value);
            }
        }
    }
    return 3;
}

int Bios::swiDiffUnfilt8(uint32_t **registers)
{
    // Get the size from the header and set the initial value
    uint32_t size = core->memory.read<uint32_t>(0, *registers[0]) >> 8;
    uint8_t value = 0;

    // Accumulate the source values and write them to the destination (8-bit)
    for (uint32_t i = 0; i < size; i++)
    {
        uint32_t address = *registers[0] + 4 + i;
        value += core->memory.read<uint8_t>(0, address);
        core->memory.write<uint8_t>(0, *registers[1] + i, value);
    }
    return 3;
}

int Bios::swiDiffUnfilt16(uint32_t **registers)
{
    // Get the size from the header and set the initial value
    uint32_t size = core->memory.read<uint32_t>(0, *registers[0]) >> 8;
    uint16_t value = 0;

    // Accumulate the source values and write them to the destination (16-bit)
    for (uint32_t i = 0; i < size; i += 2)
    {
        uint32_t address = *registers[0] + 4 + i;
        value += core->memory.read<uint16_t>(0, address);
        core->memory.write<uint16_t>(0, *registers[1] + i, value);
    }
    return 3;
}

int Bios::swiGetSineTable(uint32_t **registers)
{
    // Sine table taken from the open-source DraStic BIOS
    static const uint16_t sineTable[] =
    {
        0x0000, 0x0324, 0x0648, 0x096A, 0x0C8C, 0x0FAB, 0x12C8, 0x15E2,
        0x18F9, 0x1C0B, 0x1F1A, 0x2223, 0x2528, 0x2826, 0x2B1F, 0x2E11,
        0x30FB, 0x33DF, 0x36BA, 0x398C, 0x3C56, 0x3F17, 0x41CE, 0x447A,
        0x471C, 0x49B4, 0x4C3F, 0x4EBF, 0x5133, 0x539B, 0x55F5, 0x5842,
        0x5A82, 0x5CB3, 0x5ED7, 0x60EB, 0x62F1, 0x64E8, 0x66CF, 0x68A6,
        0x6A6D, 0x6C23, 0x6DC9, 0x6F5E, 0x70E2, 0x7254, 0x73B5, 0x7504,
        0x7641, 0x776B, 0x7884, 0x7989, 0x7A7C, 0x7B5C, 0x7C29, 0x7CE3,
        0x7D89, 0x7E1D, 0x7E9C, 0x7F09, 0x7F61, 0x7FA6, 0x7FD8, 0x7FF5
    };

    // Read a value from the sine table
    *registers[0] = sineTable[std::min(*registers[0], 0x40U)];
    return 3;
}

int Bios::swiGetPitchTable(uint32_t **registers)
{
    // Pitch table taken from the open-source DraStic BIOS
    static const uint16_t pitchTable[] =
    {
        0x0000, 0x003B, 0x0076, 0x00B2, 0x00ED, 0x0128, 0x0164, 0x019F,
        0x01DB, 0x0217, 0x0252, 0x028E, 0x02CA, 0x0305, 0x0341, 0x037D,
        0x03B9, 0x03F5, 0x0431, 0x046E, 0x04AA, 0x04E6, 0x0522, 0x055F,
        0x059B, 0x05D8, 0x0614, 0x0651, 0x068D, 0x06CA, 0x0707, 0x0743,
        0x0780, 0x07BD, 0x07FA, 0x0837, 0x0874, 0x08B1, 0x08EF, 0x092C,
        0x0969, 0x09A7, 0x09E4, 0x0A21, 0x0A5F, 0x0A9C, 0x0ADA, 0x0B18,
        0x0B56, 0x0B93, 0x0BD1, 0x0C0F, 0x0C4D, 0x0C8B, 0x0CC9, 0x0D07,
        0x0D45, 0x0D84, 0x0DC2, 0x0E00, 0x0E3F, 0x0E7D, 0x0EBC, 0x0EFA,
        0x0F39, 0x0F78, 0x0FB6, 0x0FF5, 0x1034, 0x1073, 0x10B2, 0x10F1,
        0x1130, 0x116F, 0x11AE, 0x11EE, 0x122D, 0x126C, 0x12AC, 0x12EB,
        0x132B, 0x136B, 0x13AA, 0x13EA, 0x142A, 0x146A, 0x14A9, 0x14E9,
        0x1529, 0x1569, 0x15AA, 0x15EA, 0x162A, 0x166A, 0x16AB, 0x16EB,
        0x172C, 0x176C, 0x17AD, 0x17ED, 0x182E, 0x186F, 0x18B0, 0x18F0,
        0x1931, 0x1972, 0x19B3, 0x19F5, 0x1A36, 0x1A77, 0x1AB8, 0x1AFA,
        0x1B3B, 0x1B7D, 0x1BBE, 0x1C00, 0x1C41, 0x1C83, 0x1CC5, 0x1D07,
        0x1D48, 0x1D8A, 0x1DCC, 0x1E0E, 0x1E51, 0x1E93, 0x1ED5, 0x1F17,
        0x1F5A, 0x1F9C, 0x1FDF, 0x2021, 0x2064, 0x20A6, 0x20E9, 0x212C,
        0x216F, 0x21B2, 0x21F5, 0x2238, 0x227B, 0x22BE, 0x2301, 0x2344,
        0x2388, 0x23CB, 0x240E, 0x2452, 0x2496, 0x24D9, 0x251D, 0x2561,
        0x25A4, 0x25E8, 0x262C, 0x2670, 0x26B4, 0x26F8, 0x273D, 0x2781,
        0x27C5, 0x280A, 0x284E, 0x2892, 0x28D7, 0x291C, 0x2960, 0x29A5,
        0x29EA, 0x2A2F, 0x2A74, 0x2AB9, 0x2AFE, 0x2B43, 0x2B88, 0x2BCD,
        0x2C13, 0x2C58, 0x2C9D, 0x2CE3, 0x2D28, 0x2D6E, 0x2DB4, 0x2DF9,
        0x2E3F, 0x2E85, 0x2ECB, 0x2F11, 0x2F57, 0x2F9D, 0x2FE3, 0x302A,
        0x3070, 0x30B6, 0x30FD, 0x3143, 0x318A, 0x31D0, 0x3217, 0x325E,
        0x32A5, 0x32EC, 0x3332, 0x3379, 0x33C1, 0x3408, 0x344F, 0x3496,
        0x34DD, 0x3525, 0x356C, 0x35B4, 0x35FB, 0x3643, 0x368B, 0x36D3,
        0x371A, 0x3762, 0x37AA, 0x37F2, 0x383A, 0x3883, 0x38CB, 0x3913,
        0x395C, 0x39A4, 0x39ED, 0x3A35, 0x3A7E, 0x3AC6, 0x3B0F, 0x3B58,
        0x3BA1, 0x3BEA, 0x3C33, 0x3C7C, 0x3CC5, 0x3D0E, 0x3D58, 0x3DA1,
        0x3DEA, 0x3E34, 0x3E7D, 0x3EC7, 0x3F11, 0x3F5A, 0x3FA4, 0x3FEE,
        0x4038, 0x4082, 0x40CC, 0x4116, 0x4161, 0x41AB, 0x41F5, 0x4240,
        0x428A, 0x42D5, 0x431F, 0x436A, 0x43B5, 0x4400, 0x444B, 0x4495,
        0x44E1, 0x452C, 0x4577, 0x45C2, 0x460D, 0x4659, 0x46A4, 0x46F0,
        0x473B, 0x4787, 0x47D3, 0x481E, 0x486A, 0x48B6, 0x4902, 0x494E,
        0x499A, 0x49E6, 0x4A33, 0x4A7F, 0x4ACB, 0x4B18, 0x4B64, 0x4BB1,
        0x4BFE, 0x4C4A, 0x4C97, 0x4CE4, 0x4D31, 0x4D7E, 0x4DCB, 0x4E18,
        0x4E66, 0x4EB3, 0x4F00, 0x4F4E, 0x4F9B, 0x4FE9, 0x5036, 0x5084,
        0x50D2, 0x5120, 0x516E, 0x51BC, 0x520A, 0x5258, 0x52A6, 0x52F4,
        0x5343, 0x5391, 0x53E0, 0x542E, 0x547D, 0x54CC, 0x551A, 0x5569,
        0x55B8, 0x5607, 0x5656, 0x56A5, 0x56F4, 0x5744, 0x5793, 0x57E2,
        0x5832, 0x5882, 0x58D1, 0x5921, 0x5971, 0x59C1, 0x5A10, 0x5A60,
        0x5AB0, 0x5B01, 0x5B51, 0x5BA1, 0x5BF1, 0x5C42, 0x5C92, 0x5CE3,
        0x5D34, 0x5D84, 0x5DD5, 0x5E26, 0x5E77, 0x5EC8, 0x5F19, 0x5F6A,
        0x5FBB, 0x600D, 0x605E, 0x60B0, 0x6101, 0x6153, 0x61A4, 0x61F6,
        0x6248, 0x629A, 0x62EC, 0x633E, 0x6390, 0x63E2, 0x6434, 0x6487,
        0x64D9, 0x652C, 0x657E, 0x65D1, 0x6624, 0x6676, 0x66C9, 0x671C,
        0x676F, 0x67C2, 0x6815, 0x6869, 0x68BC, 0x690F, 0x6963, 0x69B6,
        0x6A0A, 0x6A5E, 0x6AB1, 0x6B05, 0x6B59, 0x6BAD, 0x6C01, 0x6C55,
        0x6CAA, 0x6CFE, 0x6D52, 0x6DA7, 0x6DFB, 0x6E50, 0x6EA4, 0x6EF9,
        0x6F4E, 0x6FA3, 0x6FF8, 0x704D, 0x70A2, 0x70F7, 0x714D, 0x71A2,
        0x71F7, 0x724D, 0x72A2, 0x72F8, 0x734E, 0x73A4, 0x73FA, 0x7450,
        0x74A6, 0x74FC, 0x7552, 0x75A8, 0x75FF, 0x7655, 0x76AC, 0x7702,
        0x7759, 0x77B0, 0x7807, 0x785E, 0x78B4, 0x790C, 0x7963, 0x79BA,
        0x7A11, 0x7A69, 0x7AC0, 0x7B18, 0x7B6F, 0x7BC7, 0x7C1F, 0x7C77,
        0x7CCF, 0x7D27, 0x7D7F, 0x7DD7, 0x7E2F, 0x7E88, 0x7EE0, 0x7F38,
        0x7F91, 0x7FEA, 0x8042, 0x809B, 0x80F4, 0x814D, 0x81A6, 0x81FF,
        0x8259, 0x82B2, 0x830B, 0x8365, 0x83BE, 0x8418, 0x8472, 0x84CB,
        0x8525, 0x857F, 0x85D9, 0x8633, 0x868E, 0x86E8, 0x8742, 0x879D,
        0x87F7, 0x8852, 0x88AC, 0x8907, 0x8962, 0x89BD, 0x8A18, 0x8A73,
        0x8ACE, 0x8B2A, 0x8B85, 0x8BE0, 0x8C3C, 0x8C97, 0x8CF3, 0x8D4F,
        0x8DAB, 0x8E07, 0x8E63, 0x8EBF, 0x8F1B, 0x8F77, 0x8FD4, 0x9030,
        0x908C, 0x90E9, 0x9146, 0x91A2, 0x91FF, 0x925C, 0x92B9, 0x9316,
        0x9373, 0x93D1, 0x942E, 0x948C, 0x94E9, 0x9547, 0x95A4, 0x9602,
        0x9660, 0x96BE, 0x971C, 0x977A, 0x97D8, 0x9836, 0x9895, 0x98F3,
        0x9952, 0x99B0, 0x9A0F, 0x9A6E, 0x9ACD, 0x9B2C, 0x9B8B, 0x9BEA,
        0x9C49, 0x9CA8, 0x9D08, 0x9D67, 0x9DC7, 0x9E26, 0x9E86, 0x9EE6,
        0x9F46, 0x9FA6, 0xA006, 0xA066, 0xA0C6, 0xA127, 0xA187, 0xA1E8,
        0xA248, 0xA2A9, 0xA30A, 0xA36B, 0xA3CC, 0xA42D, 0xA48E, 0xA4EF,
        0xA550, 0xA5B2, 0xA613, 0xA675, 0xA6D6, 0xA738, 0xA79A, 0xA7FC,
        0xA85E, 0xA8C0, 0xA922, 0xA984, 0xA9E7, 0xAA49, 0xAAAC, 0xAB0E,
        0xAB71, 0xABD4, 0xAC37, 0xAC9A, 0xACFD, 0xAD60, 0xADC3, 0xAE27,
        0xAE8A, 0xAEED, 0xAF51, 0xAFB5, 0xB019, 0xB07C, 0xB0E0, 0xB145,
        0xB1A9, 0xB20D, 0xB271, 0xB2D6, 0xB33A, 0xB39F, 0xB403, 0xB468,
        0xB4CD, 0xB532, 0xB597, 0xB5FC, 0xB662, 0xB6C7, 0xB72C, 0xB792,
        0xB7F7, 0xB85D, 0xB8C3, 0xB929, 0xB98F, 0xB9F5, 0xBA5B, 0xBAC1,
        0xBB28, 0xBB8E, 0xBBF5, 0xBC5B, 0xBCC2, 0xBD29, 0xBD90, 0xBDF7,
        0xBE5E, 0xBEC5, 0xBF2C, 0xBF94, 0xBFFB, 0xC063, 0xC0CA, 0xC132,
        0xC19A, 0xC202, 0xC26A, 0xC2D2, 0xC33A, 0xC3A2, 0xC40B, 0xC473,
        0xC4DC, 0xC544, 0xC5AD, 0xC616, 0xC67F, 0xC6E8, 0xC751, 0xC7BB,
        0xC824, 0xC88D, 0xC8F7, 0xC960, 0xC9CA, 0xCA34, 0xCA9E, 0xCB08,
        0xCB72, 0xCBDC, 0xCC47, 0xCCB1, 0xCD1B, 0xCD86, 0xCDF1, 0xCE5B,
        0xCEC6, 0xCF31, 0xCF9C, 0xD008, 0xD073, 0xD0DE, 0xD14A, 0xD1B5,
        0xD221, 0xD28D, 0xD2F8, 0xD364, 0xD3D0, 0xD43D, 0xD4A9, 0xD515,
        0xD582, 0xD5EE, 0xD65B, 0xD6C7, 0xD734, 0xD7A1, 0xD80E, 0xD87B,
        0xD8E9, 0xD956, 0xD9C3, 0xDA31, 0xDA9E, 0xDB0C, 0xDB7A, 0xDBE8,
        0xDC56, 0xDCC4, 0xDD32, 0xDDA0, 0xDE0F, 0xDE7D, 0xDEEC, 0xDF5B,
        0xDFC9, 0xE038, 0xE0A7, 0xE116, 0xE186, 0xE1F5, 0xE264, 0xE2D4,
        0xE343, 0xE3B3, 0xE423, 0xE493, 0xE503, 0xE573, 0xE5E3, 0xE654,
        0xE6C4, 0xE735, 0xE7A5, 0xE816, 0xE887, 0xE8F8, 0xE969, 0xE9DA,
        0xEA4B, 0xEABC, 0xEB2E, 0xEB9F, 0xEC11, 0xEC83, 0xECF5, 0xED66,
        0xEDD9, 0xEE4B, 0xEEBD, 0xEF2F, 0xEFA2, 0xF014, 0xF087, 0xF0FA,
        0xF16D, 0xF1E0, 0xF253, 0xF2C6, 0xF339, 0xF3AD, 0xF420, 0xF494,
        0xF507, 0xF57B, 0xF5EF, 0xF663, 0xF6D7, 0xF74C, 0xF7C0, 0xF834,
        0xF8A9, 0xF91E, 0xF992, 0xFA07, 0xFA7C, 0xFAF1, 0xFB66, 0xFBDC,
        0xFC51, 0xFCC7, 0xFD3C, 0xFDB2, 0xFE28, 0xFE9E, 0xFF14, 0xFF8A
    };

    // Read a value from the pitch table
    *registers[0] = pitchTable[std::min(*registers[0], 0x300U)];
    return 3;
}

int Bios::swiGetVolumeTable(uint32_t **registers)
{
    // Volume table taken from the open-source DraStic BIOS
    static const uint8_t volumeTable[] =
    {
        0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
        0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
        0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
        0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
        0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
        0x02, 0x02, 0x02, 0x02, 0x02, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
        0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
        0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
        0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
        0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
        0x05, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
        0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
        0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
        0x07, 0x07, 0x07, 0x07, 0x08, 0x08, 0x08, 0x08,
        0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x09,
        0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
        0x09, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A,
        0x0A, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B,
        0x0B, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C,
        0x0C, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0E,
        0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0F, 0x0F,
        0x0F, 0x0F, 0x0F, 0x10, 0x10, 0x10, 0x10, 0x10,
        0x10, 0x11, 0x11, 0x11, 0x11, 0x11, 0x12, 0x12,
        0x12, 0x12, 0x12, 0x13, 0x13, 0x13, 0x13, 0x14,
        0x14, 0x14, 0x14, 0x14, 0x15, 0x15, 0x15, 0x15,
        0x16, 0x16, 0x16, 0x16, 0x17, 0x17, 0x17, 0x18,
        0x18, 0x18, 0x18, 0x19, 0x19, 0x19, 0x19, 0x1A,
        0x1A, 0x1A, 0x1B, 0x1B, 0x1B, 0x1C, 0x1C, 0x1C,
        0x1D, 0x1D, 0x1D, 0x1E, 0x1E, 0x1E, 0x1F, 0x1F,
        0x1F, 0x20, 0x20, 0x20, 0x21, 0x21, 0x22, 0x22,
        0x22, 0x23, 0x23, 0x24, 0x24, 0x24, 0x25, 0x25,
        0x26, 0x26, 0x27, 0x27, 0x27, 0x28, 0x28, 0x29,
        0x29, 0x2A, 0x2A, 0x2B, 0x2B, 0x2C, 0x2C, 0x2D,
        0x2D, 0x2E, 0x2E, 0x2F, 0x2F, 0x30, 0x31, 0x31,
        0x32, 0x32, 0x33, 0x33, 0x34, 0x35, 0x35, 0x36,
        0x36, 0x37, 0x38, 0x38, 0x39, 0x3A, 0x3A, 0x3B,
        0x3C, 0x3C, 0x3D, 0x3E, 0x3F, 0x3F, 0x40, 0x41,
        0x42, 0x42, 0x43, 0x44, 0x45, 0x45, 0x46, 0x47,
        0x48, 0x49, 0x4A, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E,
        0x4F, 0x50, 0x51, 0x52, 0x52, 0x53, 0x54, 0x55,
        0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5D, 0x5E,
        0x5F, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x67,
        0x68, 0x69, 0x6A, 0x6B, 0x6D, 0x6E, 0x6F, 0x71,
        0x72, 0x73, 0x75, 0x76, 0x77, 0x79, 0x7A, 0x7B,
        0x7D, 0x7E, 0x7F, 0x20, 0x21, 0x21, 0x21, 0x22,
        0x22, 0x23, 0x23, 0x23, 0x24, 0x24, 0x25, 0x25,
        0x26, 0x26, 0x26, 0x27, 0x27, 0x28, 0x28, 0x29,
        0x29, 0x2A, 0x2A, 0x2B, 0x2B, 0x2C, 0x2C, 0x2D,
        0x2D, 0x2E, 0x2E, 0x2F, 0x2F, 0x30, 0x30, 0x31,
        0x31, 0x32, 0x33, 0x33, 0x34, 0x34, 0x35, 0x36,
        0x36, 0x37, 0x37, 0x38, 0x39, 0x39, 0x3A, 0x3B,
        0x3B, 0x3C, 0x3D, 0x3E, 0x3E, 0x3F, 0x40, 0x40,
        0x41, 0x42, 0x43, 0x43, 0x44, 0x45, 0x46, 0x47,
        0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4D,
        0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55,
        0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D,
        0x5E, 0x5F, 0x60, 0x62, 0x63, 0x64, 0x65, 0x66,
        0x67, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6F, 0x70,
        0x71, 0x73, 0x74, 0x75, 0x77, 0x78, 0x79, 0x7B,
        0x7C, 0x7E, 0x7E, 0x40, 0x41, 0x42, 0x43, 0x43,
        0x44, 0x45, 0x46, 0x47, 0x47, 0x48, 0x49, 0x4A,
        0x4B, 0x4C, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51,
        0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
        0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61,
        0x62, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6B,
        0x6C, 0x6D, 0x6E, 0x70, 0x71, 0x72, 0x74, 0x75,
        0x76, 0x78, 0x79, 0x7B, 0x7C, 0x7D, 0x7E, 0x40,
        0x41, 0x42, 0x42, 0x43, 0x44, 0x45, 0x46, 0x46,
        0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4B, 0x4C, 0x4D,
        0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55,
        0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D,
        0x5E, 0x5F, 0x60, 0x61, 0x62, 0x63, 0x65, 0x66,
        0x67, 0x68, 0x69, 0x6A, 0x6C, 0x6D, 0x6E, 0x6F,
        0x71, 0x72, 0x73, 0x75, 0x76, 0x77, 0x79, 0x7A,
        0x7C, 0x7D, 0x7E, 0x7F
    };

    // Read a value from the volume table
    *registers[0] = volumeTable[std::min(*registers[0], 0x2D4U)];
    return 3;
}

int Bios::swiUnknown(uint32_t **registers)
{
    // Handle an unknown SWI comment
    uint32_t address = *registers[15] - (core->interpreter[arm7].isThumb() ? 4 : 6);
    uint8_t comment = core->memory.read<uint8_t>(arm7, address);
    if (swiTable == swiTableGba)
        LOG("Unknown GBA BIOS SWI: 0x%02X\n", comment);
    else
        LOG("Unknown ARM%d BIOS SWI: 0x%02X\n", (arm7 ? 7 : 9), comment);
    return 3;
}
