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

#include "cp15.h"
#include "core.h"

namespace cp15
{

uint32_t exceptions = 0x00000000;
bool     dtcmEnable = false;
bool     itcmEnable = false;
uint32_t dtcmBase   = 0x027C0000;
uint32_t dtcmSize   = 0x00004000;
uint32_t itcmSize   = 0x02000000;

uint32_t control = 0x00000078;
uint32_t dtcmReg = 0x027C0005;
uint32_t itcmReg = 0x00000010;

uint32_t readRegister(uint8_t cn, uint8_t cm, uint8_t cp)
{
    if (cn == 1 && cm == 0 && cp == 0) // Control
    {
        return control;
    }
    else if (cn == 9 && cm == 1 && cp == 0) // Data TCM size/base
    {
        return dtcmReg;
    }
    else if (cn == 9 && cm == 1 && cp == 1) // Instruction TCM size/base
    {
        return itcmReg;
    }
    else
    {
        printf("Unknown CP15 register read: C%d,C%d,%d\n", cn, cm, cp);
        return 0;
    }
}

void writeRegister(uint8_t cn, uint8_t cm, uint8_t cp, uint32_t value)
{
    if (cn == 1 && cm == 0 && cp == 0) // Control
    {
        control = (control & ~0x000FF085) | (value & 0x000FF085);
        exceptions = (control & BIT(13)) ? 0xFFFF0000 : 0x00000000;
        dtcmEnable = (control & BIT(16));
        itcmEnable = (control & BIT(18));
        if (control & BIT(0))  printf("Unhandled request: Protection unit enable\n");
        if (control & BIT(2))  printf("Unhandled request: Data cache enable\n");
        if (control & BIT(7))  printf("Unhandled request: Big endian mode\n");
        if (control & BIT(12)) printf("Unhandled request: Instruction cache enable\n");
        if (control & BIT(15)) printf("Unhandled request: Pre-ARMv5 mode\n");
        if (control & BIT(17)) printf("Unhandled request: DTCM load mode\n");
        if (control & BIT(19)) printf("Unhandled request: ITCM load mode\n");
    }
    else if (cn == 7 && cm == 0 && cp == 4) // Wait for interrupt
    {
        core::arm9.halt = true;
    }
    else if (cn == 7 && cm == 8 && cp == 2) // Wait for interrupt (alternate)
    {
        core::arm9.halt = true;
    }
    else if (cn == 9 && cm == 1 && cp == 0) // Data TCM base/size
    {
        uint8_t shift = (value & 0x0000003E) >> 1;
        if (shift < 3) // Min 4KB
            shift = 3;
        else if (shift > 23) // Max 4GB
            shift = 23;
        dtcmReg = value;
        dtcmSize = 0x200 << shift;
        dtcmBase = value & 0xFFFFF000;
    }
    else if (cn == 9 && cm == 1 && cp == 1) // Instruction TCM base/size
    {
        uint8_t shift = (value & 0x0000003E) >> 1;
        if (shift < 3) // Min 4KB
            shift = 3;
        else if (shift > 23) // Max 4GB
            shift = 23;
        itcmReg = value;
        itcmSize = 0x200 << shift;
    }
    else
    {
        printf("Unknown CP15 register write: C%d,C%d,%d\n", cn, cm, cp);
    }
}

}
