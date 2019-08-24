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
#include "interpreter.h"

namespace cp15
{

uint32_t control = 0x00000078;
uint32_t dtcmReg = 0x00000000;
uint32_t itcmReg = 0x00000000;

uint32_t exceptionBase;

bool dtcmEnable;
uint32_t dtcmBase, dtcmSize;

bool itcmEnable;
uint32_t itcmSize;

uint32_t readRegister(uint8_t cn, uint8_t cm, uint8_t cp)
{
    switch ((cn << 16) | (cm << 8) | (cp << 0))
    {
        case 0x000000: return 0x41059461; // Main ID (read-only)
        case 0x000001: return 0x0F0D2112; // Cache type (read-only)
        case 0x010000: return control;    // Control
        case 0x090100: return dtcmReg;    // Data TCM base/size
        case 0x090101: return itcmReg;    // Instruction TCM base/size

        default:
        {
            printf("Unknown CP15 register read: C%d,C%d,%d\n", cn, cm, cp);
            return 0;
        }
    }
}

void writeRegister(uint8_t cn, uint8_t cm, uint8_t cp, uint32_t value)
{
    switch ((cn << 16) | (cm << 8) | (cp << 0))
    {
        case 0x010000: // Control
        {
            // Some bits are read only, so only set the ones that are writeable
            control = (control & ~0x000FF085) | (value & 0x000FF085);

            // Set some control values that are used elsewhere
            // The writeable bits that aren't used here also do stuff, but for now this is enough
            exceptionBase = (control & BIT(13)) ? 0xFFFF0000 : 0x00000000;
            dtcmEnable = (control & BIT(16));
            itcmEnable = (control & BIT(18));

            break;
        }

        case 0x070004: case 0x070802: // Wait for interrupt
        {
            interpreter::arm9.halt = true;
            break;
        }

        case 0x090100: // Data TCM base/size
        {
            dtcmReg = value;
            dtcmBase = value & 0xFFFFF000;

            // TCM size is calculated as 512 shifted left N bits, with a minimum of 4KB
            dtcmSize = 0x200 << ((value & 0x0000003E) >> 1);
            if (dtcmSize < 0x1000) dtcmSize = 0x1000;

            break;
        }

        case 0x090101: // Instruction TCM base/size
        {
            // ITCM base is fixed, so that part of the value is unused
            itcmReg = value;

            // TCM size is calculated as 512 shifted left N bits, with a minimum of 4KB
            itcmSize = 0x200 << ((value & 0x0000003E) >> 1);
            if (itcmSize < 0x1000) itcmSize = 0x1000;

            break;
        }

        default:
        {
            printf("Unknown CP15 register write: C%d,C%d,%d\n", cn, cm, cp);
            break;
        }
    }
}

void init()
{
    // Reset all values
    writeRegister(1, 0, 0, 0x00000000); // Control
    writeRegister(9, 1, 0, 0x00000000); // Data TCM base/size
    writeRegister(9, 1, 1, 0x00000000); // Instruction TCM base/size
}

}
