/*
    Copyright 2019-2025 Hydr8gon

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

#include "dma.h"
#include "core.h"

void Dma::saveState(FILE *file)
{
    // Write state data to the file
    fwrite(srcAddrs, 4, sizeof(srcAddrs) / 4, file);
    fwrite(dstAddrs, 4, sizeof(dstAddrs) / 4, file);
    fwrite(wordCounts, 4, sizeof(wordCounts) / 4, file);
    fwrite(dmaSad, 4, sizeof(dmaSad) / 4, file);
    fwrite(dmaDad, 4, sizeof(dmaDad) / 4, file);
    fwrite(dmaCnt, 4, sizeof(dmaCnt) / 4, file);
}

void Dma::loadState(FILE *file)
{
    // Read state data from the file
    fread(srcAddrs, 4, sizeof(srcAddrs) / 4, file);
    fread(dstAddrs, 4, sizeof(dstAddrs) / 4, file);
    fread(wordCounts, 4, sizeof(wordCounts) / 4, file);
    fread(dmaSad, 4, sizeof(dmaSad) / 4, file);
    fread(dmaDad, 4, sizeof(dmaDad) / 4, file);
    fread(dmaCnt, 4, sizeof(dmaCnt) / 4, file);
}

void Dma::transfer(int channel)
{
    int dstAddrCnt = (dmaCnt[channel] & 0x00600000) >> 21;
    int srcAddrCnt = (dmaCnt[channel] & 0x01800000) >> 23;
    int mode = (dmaCnt[channel] & 0x38000000) >> 27;
    int gxFifoCount = 0;

    // Perform the transfer
    if (core->gbaMode && mode == 6 && (channel == 1 || channel == 2)) // GBA sound DMA
    {
        LOG_INFO("ARM%d DMA channel %d transferring 4 words from 0x%X to 0x%X in mode %d\n",
            cpu ? 7 : 9, channel, srcAddrs[channel], dstAddrs[channel], mode);
        for (unsigned int i = 0; i < 4; i++)
        {
            // Transfer a word
            // GBA sound DMAs always transfer 4 words and never adjust the destination address
            uint32_t value = core->memory.read<uint32_t>(cpu, srcAddrs[channel], false);
            core->memory.write<uint32_t>(cpu, dstAddrs[channel], value, false);

            // Adjust the source address
            if (srcAddrCnt == 0) // Increment
                srcAddrs[channel] += 4;
            else if (srcAddrCnt == 1) // Decrement
                srcAddrs[channel] -= 4;
        }
    }
    else if (dmaCnt[channel] & BIT(26)) // Whole word transfer
    {
        LOG_INFO("ARM%d DMA channel %d transferring %d words from 0x%X to 0x%X in mode %d\n",
            cpu ? 7 : 9, channel, wordCounts[channel], srcAddrs[channel], dstAddrs[channel], mode);
        for (unsigned int i = 0; i < wordCounts[channel]; i++)
        {
            // Transfer a word
            uint32_t value = core->memory.read<uint32_t>(cpu, srcAddrs[channel], false);
            core->memory.write<uint32_t>(cpu, dstAddrs[channel], value, false);

            // Adjust the source address
            if (srcAddrCnt == 0) // Increment
                srcAddrs[channel] += 4;
            else if (srcAddrCnt == 1) // Decrement
                srcAddrs[channel] -= 4;

            // Adjust the destination address
            if (dstAddrCnt == 0 || dstAddrCnt == 3) // Increment
                dstAddrs[channel] += 4;
            else if (dstAddrCnt == 1) // Decrement
                dstAddrs[channel] -= 4;

            // In GXFIFO mode, only send 112 words at a time
            if (mode == 7 && ++gxFifoCount == 112)
                break;
        }
    }
    else // Half-word transfer
    {
        LOG_INFO("ARM%d DMA channel %d transferring %d half-words from 0x%X to 0x%X in mode %d\n",
            cpu ? 7 : 9, channel, wordCounts[channel], srcAddrs[channel], dstAddrs[channel], mode);
        for (unsigned int i = 0; i < wordCounts[channel]; i++)
        {
            // Transfer a half-word
            uint16_t value = core->memory.read<uint16_t>(cpu, srcAddrs[channel], false);
            core->memory.write<uint16_t>(cpu, dstAddrs[channel], value, false);

            // Adjust the source address
            if (srcAddrCnt == 0) // Increment
                srcAddrs[channel] += 2;
            else if (srcAddrCnt == 1) // Decrement
                srcAddrs[channel] -= 2;

            // Adjust the destination address
            if (dstAddrCnt == 0 || dstAddrCnt == 3) // Increment
                dstAddrs[channel] += 2;
            else if (dstAddrCnt == 1) // Decrement
                dstAddrs[channel] -= 2;

            // In GXFIFO mode, only send 112 words at a time
            if (mode == 7 && ++gxFifoCount == 112)
                break;
        }
    }

    if (mode == 7)
    {
        // Don't end a GXFIFO transfer if there are still words left
        wordCounts[channel] -= gxFifoCount;
        if (wordCounts[channel] > 0)
        {
            // Schedule another transfer immediately if the FIFO is still half empty
            if (core->gpu3D.readGxStat() & BIT(25))
                core->schedule(SchedTask(DMA9_TRANSFER0 + (cpu << 2) + channel), 1);
            return;
        }
    }

    if ((dmaCnt[channel] & BIT(25)) && mode != 0) // Repeat
    {
        // Reload the internal registers on repeat
        wordCounts[channel] = dmaCnt[channel] & 0x001FFFFF;
        if (dstAddrCnt == 3) // Increment and reload
            dstAddrs[channel] = dmaDad[channel];

        // In GXFIFO mode, schedule another transfer immediately if the FIFO is still half empty
        if (mode == 7 && core->gpu3D.readGxStat() & BIT(25))
            core->schedule(SchedTask(DMA9_TRANSFER0 + (cpu << 2) + channel), 1);
    }
    else
    {
        // End the transfer
        dmaCnt[channel] &= ~BIT(31);
    }

    // Trigger an end of transfer IRQ if enabled
    if (dmaCnt[channel] & BIT(30))
        core->interpreter[cpu].sendInterrupt(8 + channel);
}

void Dma::trigger(int mode, uint8_t channels)
{
    // ARM7 DMAs don't use the lowest mode bit, so adjust accordingly
    if (cpu == 1) mode <<= 1;

    // Schedule a transfer on channels that are enabled and set to the triggered mode
    for (int i = 0; i < 4; i++)
    {
        if ((channels & BIT(i)) && (dmaCnt[i] & BIT(31)) && ((dmaCnt[i] & 0x38000000) >> 27) == mode)
            core->schedule(SchedTask(DMA9_TRANSFER0 + (cpu << 2) + i), 1);
    }
}

void Dma::writeDmaSad(int channel, uint32_t mask, uint32_t value)
{
    // Write to one of the DMASAD registers
    mask &= ((cpu == 0 || channel != 0) ? 0x0FFFFFFF : 0x07FFFFFF);
    dmaSad[channel] = (dmaSad[channel] & ~mask) | (value & mask);
}

void Dma::writeDmaDad(int channel, uint32_t mask, uint32_t value)
{
    // Write to one of the DMADAD registers
    mask &= ((cpu == 0 || channel == 3) ? 0x0FFFFFFF : 0x07FFFFFF);
    dmaDad[channel] = (dmaDad[channel] & ~mask) | (value & mask);
}

void Dma::writeDmaCnt(int channel, uint32_t mask, uint32_t value)
{
    uint32_t old = dmaCnt[channel];

    // Write to one of the DMACNT registers
    mask &= ((cpu == 0) ? 0xFFFFFFFF : (channel == 3 ? 0xF7E0FFFF : 0xF7E03FFF));
    dmaCnt[channel] = (dmaCnt[channel] & ~mask) | (value & mask);

    // In GXFIFO mode, schedule a transfer on the channel immediately if the FIFO is already half empty
    // All other modes are only triggered at the moment when the event happens
    // For example, if a word from the DS cart is ready before starting a DMA, the DMA will not be triggered
    if ((dmaCnt[channel] & BIT(31)) && ((dmaCnt[channel] & 0x38000000) >> 27) == 7 && (core->gpu3D.readGxStat() & BIT(25)))
        core->schedule(SchedTask(DMA9_TRANSFER0 + (cpu << 2) + channel), 1);

    // Don't reload the internal registers unless the enable bit changed from 0 to 1
    if ((old & BIT(31)) || !(dmaCnt[channel] & BIT(31)))
        return;

    // Reload the internal registers
    dstAddrs[channel] = dmaDad[channel];
    srcAddrs[channel] = dmaSad[channel];
    wordCounts[channel] = dmaCnt[channel] & 0x001FFFFF;

    // Schedule a transfer on the channel if it's set to immediate mode
    // Reloading seems to be the only trigger for this, so an enabled channel changed to immediate will never transfer
    // This also means that repeating doesn't work; in this case, the enabled bit is cleared after only one transfer
    if (((dmaCnt[channel] & 0x38000000) >> 27) == 0)
        core->schedule(SchedTask(DMA9_TRANSFER0 + (cpu << 2) + channel), 1);
}

uint32_t Dma::readDmaCnt(int channel)
{
    // Read from one of the DMACNT registers
    // The lower half-word isn't readable in GBA mode
    return dmaCnt[channel] & ~(core->gbaMode ? 0x0000FFFF : 0x00000000);
}
