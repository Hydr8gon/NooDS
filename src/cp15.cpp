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

#include "cp15.h"
#include "core.h"

void Cp15::saveState(FILE *file)
{
    // Write state data to the file
    fwrite(&ctrlReg, sizeof(ctrlReg), 1, file);
    fwrite(&dtcmReg, sizeof(dtcmReg), 1, file);
    fwrite(&itcmReg, sizeof(itcmReg), 1, file);
}

void Cp15::loadState(FILE *file)
{
    // Read state data from the file
    uint32_t ctrl, dtcm, itcm;
    fread(&ctrl, sizeof(ctrl), 1, file);
    fread(&dtcm, sizeof(dtcm), 1, file);
    fread(&itcm, sizeof(itcm), 1, file);

    // Set registers along with values based on them
    write(1, 0, 0, ctrl);
    write(9, 1, 0, dtcm);
    write(9, 1, 1, itcm);
}

uint32_t Cp15::read(uint8_t cn, uint8_t cm, uint8_t cp)
{
    // Read a value from a CP15 register
    switch ((cn << 16) | (cm << 8) | (cp << 0))
    {
        case 0x000000: return 0x41059461; // Main ID
        case 0x000001: return 0x0F0D2112; // Cache type
        case 0x010000: return ctrlReg; // Control
        case 0x090100: return dtcmReg; // Data TCM base/size
        case 0x090101: return itcmReg; // Instruction TCM size
        case 0x0D0001: case 0x0D0101: return procId; // Trace process ID

        default:
            LOG("Unknown CP15 register read: C%d,C%d,%d\n", cn, cm, cp);
            return 0;
    }
}

void Cp15::write(uint8_t cn, uint8_t cm, uint8_t cp, uint32_t value)
{
    // Write a value to a CP15 register
    uint32_t oldAddr, oldSize;
    switch ((cn << 16) | (cm << 8) | (cp << 0))
    {
        case 0x010000: // Control
            // Set writable control bits and update their state values
            ctrlReg = (ctrlReg & ~0xFF085) | (value & 0xFF085);
            exceptionAddr = (ctrlReg & BIT(13)) ? 0xFFFF0000 : 0x00000000;
            dtcmCanRead = (ctrlReg & BIT(16)) && !(ctrlReg & BIT(17));
            dtcmCanWrite = (ctrlReg & BIT(16));
            itcmCanRead = (ctrlReg & BIT(18)) && !(ctrlReg & BIT(19));
            itcmCanWrite = (ctrlReg & BIT(18));

            // Update the memory map at the current TCM locations
            core->memory.updateMap9(dtcmAddr, dtcmAddr + dtcmSize, true);
            core->memory.updateMap9(0x00000000, itcmSize, true);
            return;

        case 0x090100: // Data TCM base/size
            // Set the DTCM register and update its address and size
            dtcmReg = value;
            oldAddr = dtcmAddr;
            oldSize = dtcmSize;
            dtcmAddr = dtcmReg & 0xFFFFF000;
            dtcmSize = std::max(0x1000, 0x200 << ((dtcmReg >> 1) & 0x1F)); // Min 4KB

            // Update the memory map at the old and new DTCM areas
            core->memory.updateMap9(oldAddr, oldAddr + oldSize, true);
            core->memory.updateMap9(dtcmAddr, dtcmAddr + dtcmSize, true);
            return;

        case 0x070004: case 0x070802: // Wait for interrupt
            // Halt the CPU
            core->interpreter[0].halt(0);
            return;

        case 0x090101: // Instruction TCM size
            // Set the ITCM register and update its size
            itcmReg = value;
            oldSize = itcmSize;
            itcmSize = std::max(0x1000, 0x200 << ((itcmReg >> 1) & 0x1F)); // Min 4KB

            // Update the memory map at the old and new ITCM areas
            core->memory.updateMap9(0x00000000, std::max(oldSize, itcmSize), true);
            return;

        case 0x0D0001: case 0x0D0101: // Trace process ID
            // Set the trace process ID register
            procId = value;
            return;

        default:
            LOG("Unknown CP15 register write: C%d,C%d,%d\n", cn, cm, cp);
            return;
    }
}
