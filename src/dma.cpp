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
#include "defines.h"
#include "cartridge.h"
#include "gpu_3d.h"
#include "interpreter.h"
#include "memory.h"

void Dma::transfer()
{
    for (int i = 0; i < 4; i++)
    {
        // Only transfer on enabled channels
        if (!(enabled & BIT(i))) continue;

        // Determine the transfer mode
        unsigned int mode;
        if (gpu3D) // ARM9
        {
            mode = (dmaCnt[i] & 0x38000000) >> 27;
        }
        else
        {
            // Redirect ARM7 mode values to the ARM9 equivalents
            switch ((dmaCnt[i] & 0x30000000) >> 28)
            {
                case 0: mode = 0; break;
                case 1: mode = 1; break;
                case 2: mode = 5; break;
                case 3: mode = 8; break;
            }
        }

        // Check if the transfer should be performed now
        switch (mode)
        {
            case 0: // Start immediately
                // Transfer now
                break;

            case 5: // DS cartridge slot
                // Only transfer when a word from the cart is ready
                if (!(cart->readRomCtrl(2) & BIT(7)))
                    continue;
                break;

            case 7: // Geometry command FIFO
                // Only transfer when the FIFO is less than half full
                if (!(gpu3D->readGxStat(3) & BIT(1)))
                    continue;
                break;

            default:
                printf("Unknown ARM%d DMA transfer timing: %d\n", (gpu3D ? 9 : 7), mode);

                // Pretend that the transfer completed
                dmaCnt[i] &= ~BIT(31);
                enabled &= ~BIT(i);
                if (dmaCnt[i] & BIT(30))
                    cpu->sendInterrupt(8 + i);

                continue;
        }

        unsigned int dstAddrCnt = (dmaCnt[i] & 0x00600000) >> 21;
        unsigned int srcAddrCnt = (dmaCnt[i] & 0x01800000) >> 23;

        // Perform the transfer
        if (dmaCnt[i] & BIT(26)) // Whole word transfer
        {
            for (unsigned int j = 0; j < wordCounts[i]; j++)
            {
                // Transfer a word
                memory->write<uint32_t>(gpu3D, dstAddrs[i], memory->read<uint32_t>(gpu3D, srcAddrs[i]));

                // Adjust the source address
                if (srcAddrCnt == 0) // Increment
                    srcAddrs[i] += 4;
                else if (srcAddrCnt == 1) // Decrement
                    srcAddrs[i] -= 4;

                // Adjust the destination address
                if (dstAddrCnt == 0 || dstAddrCnt == 3) // Increment
                    dstAddrs[i] += 4;
                else if (dstAddrCnt == 1) // Decrement
                    dstAddrs[i] -= 4;

                // In gometry command FIFO mode, only 112 words are sent at a time
                if (mode == 7)
                {
                    gxFifoCount++;
                    if (gxFifoCount == 112)
                    {
                        wordCounts[i] -= 112;
                        gxFifoCount = 0;
                        return;
                    }
                }
            }
        }
        else // Half-word transfer
        {
            for (unsigned int j = 0; j < wordCounts[i]; j++)
            {
                // Transfer a half-word
                memory->write<uint16_t>(gpu3D, dstAddrs[i], memory->read<uint16_t>(gpu3D, srcAddrs[i]));

                // Adjust the source address
                if (srcAddrCnt == 0) // Increment
                    srcAddrs[i] += 2;
                else if (srcAddrCnt == 1) // Decrement
                    srcAddrs[i] -= 2;

                // Adjust the destination address
                if (dstAddrCnt == 0 || dstAddrCnt == 3) // Increment
                    dstAddrs[i] += 2;
                else if (dstAddrCnt == 1) // Decrement
                    dstAddrs[i] -= 2;

                // In gometry command FIFO mode, only 112 words are sent at a time
                if (mode == 7)
                {
                    gxFifoCount++;
                    if (gxFifoCount == 112)
                    {
                        wordCounts[i] -= 112;
                        gxFifoCount = 0;
                        return;
                    }
                }
            }
        }

        if (dmaCnt[i] & BIT(25)) // Repeat
        {
            // Reload the internal registers on repeat
            wordCounts[i] = dmaCnt[i] & 0x001FFFFF;
            if (dstAddrCnt == 3) // Increment and reload
                dstAddrs[i] = dmaDad[i];
        }
        else
        {
            // End the transfer
            dmaCnt[i] &= ~BIT(31);
            enabled &= ~BIT(i);
            gxFifoCount = 0;
        }

        // Trigger an end of transfer IRQ if enabled
        if (dmaCnt[i] & BIT(30))
            cpu->sendInterrupt(8 + i);
    }
}

void Dma::writeDmaSad(unsigned int channel, unsigned int byte, uint8_t value)
{
    // Write to one of the DMASAD registers
    uint32_t mask = (gpu3D ? 0x0FFFFFFF : 0x07FFFFFF) & (0xFF << (byte * 8));
    dmaSad[channel] = (dmaSad[channel] & ~mask) | ((value << (byte * 8)) & mask);
}

void Dma::writeDmaDad(unsigned int channel, unsigned int byte, uint8_t value)
{
    // Write to one of the DMADAD registers
    uint32_t mask = (gpu3D ? 0x0FFFFFFF : 0x07FFFFFF) & (0xFF << (byte * 8));
    dmaDad[channel] = (dmaDad[channel] & ~mask) | ((value << (byte * 8)) & mask);
}

void Dma::writeDmaCnt(unsigned int channel, unsigned int byte, uint8_t value)
{
    if (byte == 3)
    {
        // Enable or disable the channel
        if (value & BIT(7)) enabled |= BIT(channel); else enabled &= ~BIT(channel);

        // Reload the internal registers if the enable bit changes from 0 to 1
        if (!(dmaCnt[channel] & BIT(31)) && (value & BIT(7)))
        {
            dstAddrs[channel] = dmaDad[channel];
            srcAddrs[channel] = dmaSad[channel];
            wordCounts[channel] = dmaCnt[channel] & 0x001FFFFF;
        }
    }

    // Write to one of the DMACNT registers
    uint32_t mask = (gpu3D ? 0xFFFFFFFF : (channel == 3 ? 0xF7E0FFFF : 0xF7E03FFF)) & (0xFF << (byte * 8));
    dmaCnt[channel] = (dmaCnt[channel] & ~mask) | ((value << (byte * 8)) & mask);
}
