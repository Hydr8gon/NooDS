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

#include <cstdio>

#include "cp15.h"
#include "defines.h"
#include "interpreter.h"

uint32_t Cp15::read(int cn, int cm, int cp)
{
    // Read a value from a CP15 register
    switch ((cn << 16) | (cm << 8) | (cp << 0))
    {
        case 0x000000: return 0x41059461; // Main ID
        case 0x000001: return 0x0F0D2112; // Cache type
        case 0x010000: return ctrlReg;    // Control
        case 0x090100: return dtcmReg;    // Data TCM base/size
        case 0x090101: return itcmReg;    // Instruction TCM size

        default:
        {
            printf("Unknown CP15 register read: C%d,C%d,%d\n", cn, cm, cp);
            return 0;
        }
    }
}

void Cp15::write(int cn, int cm, int cp, uint32_t value)
{
    // Write a value to a CP15 register
    switch ((cn << 16) | (cm << 8) | (cp << 0))
    {
        case 0x010000: // Control
        {
            // Some control bits are read only, so only set the writeable ones
            ctrlReg = (ctrlReg & ~0x000FF085) | (value & 0x000FF085);
            exceptionAddr = (ctrlReg & BIT(13)) ? 0xFFFF0000 : 0x00000000;
            dtcmEnabled = (ctrlReg & BIT(16));
            itcmEnabled = (ctrlReg & BIT(18));
            return;
        }

        case 0x090100: // Data TCM base/size
        {
            // DTCM size is calculated as 512 shifted left N bits, with a minimum of 4KB
            dtcmReg = value;
            dtcmAddr = dtcmReg & 0xFFFFF000;
            dtcmSize = 0x200 << ((dtcmReg & 0x0000003E) >> 1);
            if (dtcmSize < 0x1000) dtcmSize = 0x1000;
            return;
        }

        case 0x070004: case 0x070802: // Wait for interrupt
        {
            arm9->halt();
            return;
        }

        case 0x090101: // Instruction TCM size
        {
            // ITCM base is fixed, so that part of the value is unused
            // ITCM size is calculated as 512 shifted left N bits, with a minimum of 4KB
            itcmReg = value;
            itcmSize = 0x200 << ((itcmReg & 0x0000003E) >> 1);
            if (itcmSize < 0x1000) itcmSize = 0x1000;
            return;
        }

        default:
        {
            printf("Unknown CP15 register write: C%d,C%d,%d\n", cn, cm, cp);
            return;
        }
    }
}
