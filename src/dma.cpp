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

#include "dma.h"
#include "core.h"
#include "memory.h"

namespace dma
{

void transfer(interpreter::Cpu *cpu, uint8_t channel)
{
    // Determine the timing mode of the transfer
    // The ARM7 doesn't use bit 27 and has different values
    uint8_t timing = (*cpu->dmacnt[channel] & 0x38000000) >> 27;
    if (cpu->type == 7 && timing == 4) timing = 5;

    switch (timing)
    {
        case 0: // Start immediately
        {
            // Always transfer
            break;
        }

        case 5: // DS cartridge slot
        {
            // Only transfer when a word from the cart is ready
            if (!(*cpu->romctrl & BIT(23)))
                return;
            break;
        }

        default:
        {
            printf("Unknown ARM%d DMA transfer timing: %d\n", cpu->type, timing);
            *cpu->dmacnt[channel] &= ~BIT(31);
            return;
        }
    }

    // Get some starting values from the registers
    uint8_t dstAddrCnt = (*cpu->dmacnt[channel] & 0x00600000) >> 21;
    uint8_t srcAddrCnt = (*cpu->dmacnt[channel] & 0x01800000) >> 23;
    uint32_t size = (*cpu->dmacnt[channel] & 0x001FFFFF);

    if (*cpu->dmacnt[channel] & BIT(26)) // Whole word transfer
    {
        for (unsigned int i = 0; i < size; i++)
        {
            // Transfer a word
            memory::write<uint32_t>(cpu, cpu->dmaDstAddrs[channel], memory::read<uint32_t>(cpu, cpu->dmaSrcAddrs[channel]));

            // Adjust the destination address
            if (dstAddrCnt == 0 || dstAddrCnt == 3) // Increment
                cpu->dmaDstAddrs[channel] += 4;
            else if (dstAddrCnt == 1) // Decrement
                cpu->dmaDstAddrs[channel] -= 4;

            // Adjust the source address
            if (srcAddrCnt == 0) // Increment
                cpu->dmaSrcAddrs[channel] += 4;
            else if (srcAddrCnt == 1) // Decrement
                cpu->dmaSrcAddrs[channel] -= 4;
        }
    }
    else // Halfword transfer
    {
        for (unsigned int i = 0; i < size; i++)
        {
            // Transfer a halfword
            memory::write<uint16_t>(cpu, cpu->dmaDstAddrs[channel], memory::read<uint16_t>(cpu, cpu->dmaSrcAddrs[channel]));

            // Adjust the destination address
            if (dstAddrCnt == 0 || dstAddrCnt == 3) // Increment
                cpu->dmaDstAddrs[channel] += 2;
            else if (dstAddrCnt == 1) // Decrement
                cpu->dmaDstAddrs[channel] -= 2;

            // Adjust the source address
            if (srcAddrCnt == 0) // Increment
                cpu->dmaSrcAddrs[channel] += 2;
            else if (srcAddrCnt == 1) // Decrement
                cpu->dmaSrcAddrs[channel] -= 2;
        }
    }

    // Trigger an end of transfer IRQ if enabled
    if (*cpu->dmacnt[channel] & BIT(30))
        *cpu->irf |= BIT(8 + channel);

    // Unless the repeat bit is set, clear the enable bit to indicate transfer completion
    if (!(*cpu->dmacnt[channel] & BIT(25)))
        *cpu->dmacnt[channel] &= ~BIT(31);

    // Reload the destination address if increment/reload is selected
    if (dstAddrCnt == 3)
        cpu->dmaDstAddrs[channel] = *cpu->dmadad[channel];
}

}
