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

#include "memory_transfer.h"
#include "core.h"
#include "memory.h"

namespace memory_transfer
{

uint16_t romReadSize, romReadCount;

uint32_t spiWriteCount;
uint32_t spiAddr;
uint8_t spiInstr;

void dmaTransfer(interpreter::Cpu *cpu, uint8_t channel)
{
    if (!(*cpu->dmacnt[channel] & BIT(31)) || channel > 3)
        return;

    uint8_t mode = (*cpu->dmacnt[channel] & 0x38000000) >> 27;
    if (mode == 0) // Start immediately
    {
        uint8_t dstAddrCnt = (*cpu->dmacnt[channel] & 0x00600000) >> 21;
        uint8_t srcAddrCnt = (*cpu->dmacnt[channel] & 0x01800000) >> 23;
        uint32_t dstAddr = *cpu->dmadad[channel];
        uint32_t srcAddr = *cpu->dmasad[channel];
        uint32_t size = (*cpu->dmacnt[channel] & 0x001FFFFF);

        if (*cpu->dmacnt[channel] & BIT(26)) // 32-bit
        {
            for (int i = 0; i < size; i++)
            {
                memory::write<uint32_t>(cpu, dstAddr, memory::read<uint32_t>(cpu, srcAddr));

                if (dstAddrCnt == 0 || dstAddrCnt == 3) // Destination increment
                    dstAddr += 4;
                else if (dstAddrCnt == 1) // Destination decrement
                    dstAddr -= 4;

                if (srcAddrCnt == 0) // Source increment
                    srcAddr += 4;
                else if (srcAddrCnt == 1) // Source decrement
                    srcAddr -= 4;
            }
        }
        else // 16-bit
        {
            for (int i = 0; i < size; i++)
            {
                memory::write<uint16_t>(cpu, dstAddr, memory::read<uint16_t>(cpu, srcAddr));

                if (dstAddrCnt == 0 || dstAddrCnt == 3) // Destination increment
                    dstAddr += 2;
                else if (dstAddrCnt == 1) // Destination decrement
                    dstAddr -= 2;

                if (srcAddrCnt == 0) // Source increment
                    srcAddr += 2;
                else if (srcAddrCnt == 1) // Source decrement
                    srcAddr -= 2;
            }
        }
    }
    else
    {
        printf("Unknown ARM%d DMA transfer mode: %d\n", cpu->type, mode);
    }

    if (*cpu->dmacnt[channel] & BIT(30)) // End of transfer IRQ
        *cpu->irf |= BIT(8 + channel);
    *cpu->dmacnt[channel] &= ~BIT(31);
}

void fifoClear(interpreter::Cpu *cpuSend, interpreter::Cpu *cpuRecv)
{
    if (cpuSend->fifo->empty())
        return;

    // Empty the FIFO
    while (!cpuSend->fifo->empty())
        cpuSend->fifo->pop();
    cpuSend->ipcfiforecv = 0;

    // Set FIFO empty bits
    *cpuSend->ipcfifocnt |=  BIT(0);
    *cpuSend->ipcfifocnt &= ~BIT(1);
    *cpuRecv->ipcfifocnt |=  BIT(8);
    *cpuRecv->ipcfifocnt &= ~BIT(9);

    if (*cpuSend->ipcfifocnt & BIT(2)) // Send FIFO empty IRQ
        *cpuSend->irf |= BIT(17);
}

void fifoSend(interpreter::Cpu *cpuSend, interpreter::Cpu *cpuRecv)
{
    if (*cpuSend->ipcfifocnt & BIT(15)) // FIFO enabled
    {
        if (cpuSend->fifo->size() < 16)
        {
            cpuSend->fifo->push(*cpuSend->ipcfifosend);

            if (cpuSend->fifo->size() == 1)
            {
                // Clear FIFO empty bits
                *cpuSend->ipcfifocnt &= ~BIT(0);
                *cpuRecv->ipcfifocnt &= ~BIT(8);

                if (*cpuRecv->ipcfifocnt & BIT(10)) // Receive FIFO not empty IRQ
                    *cpuRecv->irf |= BIT(18);
            }
            else if (cpuSend->fifo->size() == 16)
            {
                // Set FIFO full bits
                *cpuSend->ipcfifocnt |= BIT(1);
                *cpuRecv->ipcfifocnt |= BIT(9);
            }
        }
        else
        {
            // Send full error
            *cpuSend->ipcfifocnt |= BIT(14);
        }
    }
}

uint32_t fifoReceive(interpreter::Cpu *cpuSend, interpreter::Cpu *cpuRecv)
{
    if (!cpuRecv->fifo->empty())
    {
        cpuSend->ipcfiforecv = cpuRecv->fifo->front();

        if (*cpuSend->ipcfifocnt & BIT(15)) // FIFO enabled
        {
            cpuRecv->fifo->pop();

            if (cpuRecv->fifo->empty())
            {
                // Set FIFO empty bits
                *cpuSend->ipcfifocnt |= BIT(8);
                *cpuRecv->ipcfifocnt |= BIT(0);

                if (*cpuRecv->ipcfifocnt & BIT(2)) // Send FIFO empty IRQ
                    *cpuRecv->irf |= BIT(17);
            }
            else if (cpuRecv->fifo->size() == 15)
            {
                // Clear FIFO full bits
                *cpuSend->ipcfifocnt &= ~BIT(9);
                *cpuRecv->ipcfifocnt &= ~BIT(1);
            }
        }
    }
    else
    {
        // Receive empty error
        *cpuSend->ipcfifocnt |= BIT(14);
    }

    return cpuSend->ipcfiforecv;
}

void romTransferStart(interpreter::Cpu *cpu)
{
    if (!(*cpu->romctrl & BIT(31)))
        return;

    switch (cpu->romcmdout[0])
    {
        case 0x9F: romReadSize = 0x2000; break; // Dummy
        case 0x00: romReadSize = 0x0200; break; // Get header
        case 0x90: romReadSize = 0x0004; break; // Get first chip ID

        default:
            *cpu->romctrl &= ~BIT(23); // Word not ready
            *cpu->romctrl &= ~BIT(31); // Block ready
            if (*cpu->auxspicnt & BIT(14)) // Block ready IRQ
                *cpu->irf |= BIT(19);

            printf("Unknown ROM transfer command: 0x");
            for (int i = 0; i < 8; i++)
                printf("%.2X", cpu->romcmdout[i]);
            printf("\n");
            return;
    }

    *cpu->romctrl |= BIT(23); // Word ready
    romReadCount = 0;
}

uint32_t romTransfer(interpreter::Cpu *cpu)
{
    if (!(*cpu->romctrl & BIT(23)))
        return 0;

    romReadCount += 4;
    if (romReadCount == romReadSize)
    {
        *cpu->romctrl &= ~BIT(23); // Word not ready
        *cpu->romctrl &= ~BIT(31); // Block ready
        if (*cpu->auxspicnt & BIT(14)) // Block ready IRQ
            *cpu->irf |= BIT(19);
    }

    // Return an endless stream of 0xFFs as if there isn't a cart inserted
    return 0xFFFFFFFF;
}

void spiWrite(uint8_t value)
{
    if (!(*memory::spicnt & BIT(15))) // SPI bus enabled
        return;

    uint8_t device = (*memory::spicnt & 0x0300) >> 8;
    if (device != 1) // Firmware
    {
        printf("Write to unimplemented SPI device: %d\n", device);
        return;
    }

    if (spiWriteCount == 0)
    {
        spiInstr = value;
        spiAddr = 0;
    }
    else if (spiInstr == 0x03) // READ
    {
        if (spiWriteCount < 4)
        {
            // Set the 3 byte address
            spiAddr |= value << (24 - spiWriteCount * 8);
        }
        else
        {
            // Read from the firmware
            if (spiAddr < sizeof(memory::firmware))
                *memory::spidata = memory::firmware[spiAddr];
            spiAddr += (*memory::spicnt & BIT(10)) ? 2 : 1;
        }
    }
    else
    {
        printf("Unknown firmware SPI instruction: 0x%X\n", spiInstr);
    }

    if (*memory::spicnt & BIT(11)) // Keep chip selected
        spiWriteCount++;
    else // Deselect chip
        spiWriteCount = 0;

    if (*memory::spicnt & BIT(14)) // Transfer finished IRQ
        *interpreter::arm7.irf |= BIT(23);
}

}
