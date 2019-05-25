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

#include "cp15.h"

namespace cp15
{

uint32_t dtcmBase = 0x27C0000;
uint32_t dtcmSize = 0x4000;
uint32_t itcmSize = 0x2000000;

uint32_t dtcmReg;
uint32_t itcmReg;

uint32_t readRegister(uint8_t cn, uint8_t cm, uint8_t cp)
{
    if (cn == 9 && cm == 1 && cp == 0) // Data TCM size/base
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
    if (cn == 9 && cm == 1 && cp == 0) // Data TCM base/size
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
