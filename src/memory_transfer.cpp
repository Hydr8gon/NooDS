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

uint8_t firmware[0x40000];
uint8_t *rom;

std::queue<uint32_t> fifo9, fifo7;

uint16_t romReadSize, romReadCount;

uint32_t spiWriteCount;
uint32_t spiAddr;
uint8_t spiInstr;

void dmaTransfer(interpreter::Cpu *cpu, uint8_t channel)
{
    // Don't transfer if the channel isn't enabled
    if (!(*cpu->dmacnt[channel] & BIT(31)) || channel > 3)
        return;

    uint8_t mode = (*cpu->dmacnt[channel] & 0x38000000) >> 27;
    if (mode == 0) // Start immediately
    {
        // Get some values from the registers
        uint8_t dstAddrCnt = (*cpu->dmacnt[channel] & 0x00600000) >> 21;
        uint8_t srcAddrCnt = (*cpu->dmacnt[channel] & 0x01800000) >> 23;
        uint32_t dstAddr = *cpu->dmadad[channel];
        uint32_t srcAddr = *cpu->dmasad[channel];
        uint32_t size = (*cpu->dmacnt[channel] & 0x001FFFFF);

        if (*cpu->dmacnt[channel] & BIT(26)) // Whole word transfer
        {
            for (unsigned int i = 0; i < size; i++)
            {
                // Transfer a word
                memory::write<uint32_t>(cpu, dstAddr, memory::read<uint32_t>(cpu, srcAddr));

                // Adjust the destination address
                if (dstAddrCnt == 0 || dstAddrCnt == 3) // Increment
                    dstAddr += 4;
                else if (dstAddrCnt == 1) // Decrement
                    dstAddr -= 4;

                // Adjust the source address
                if (srcAddrCnt == 0) // Increment
                    srcAddr += 4;
                else if (srcAddrCnt == 1) // Decrement
                    srcAddr -= 4;
            }
        }
        else // Halfword transfer
        {
            for (unsigned int i = 0; i < size; i++)
            {
                // Transfer a halfword
                memory::write<uint16_t>(cpu, dstAddr, memory::read<uint16_t>(cpu, srcAddr));

                // Adjust the destination address
                if (dstAddrCnt == 0 || dstAddrCnt == 3) // Increment
                    dstAddr += 2;
                else if (dstAddrCnt == 1) // Decrement
                    dstAddr -= 2;

                // Adjust the source address
                if (srcAddrCnt == 0) // Increment
                    srcAddr += 2;
                else if (srcAddrCnt == 1) // Decrement
                    srcAddr -= 2;
            }
        }
    }
    else
    {
        printf("Unknown ARM%d DMA transfer mode: %d\n", cpu->type, mode);
    }

    // Trigger an end of transfer IRQ if enabled
    if (*cpu->dmacnt[channel] & BIT(30))
        *cpu->irf |= BIT(8 + channel);

    // Clear the enable bit to indicate transfer completion
    // Transfers shouldn't complete instantly like this, but there's no timing system yet
    *cpu->dmacnt[channel] &= ~BIT(31);
}

void fifoClear(interpreter::Cpu *cpuSend, interpreter::Cpu *cpuRecv)
{
    // Nothing needs to be done if the FIFO is already empty
    if (cpuSend->fifo->empty())
        return;

    // Empty the FIFO
    while (!cpuSend->fifo->empty())
        cpuSend->fifo->pop();
    cpuSend->ipcfiforecv = 0;

    // Set the FIFO empty bits and clear the FIFO full bits
    *cpuSend->ipcfifocnt |=  BIT(0);
    *cpuSend->ipcfifocnt &= ~BIT(1);
    *cpuRecv->ipcfifocnt |=  BIT(8);
    *cpuRecv->ipcfifocnt &= ~BIT(9);

    // Trigger a send FIFO empty IRQ if enabled
    if (*cpuSend->ipcfifocnt & BIT(2))
        *cpuSend->irf |= BIT(17);
}

void fifoSend(interpreter::Cpu *cpuSend, interpreter::Cpu *cpuRecv)
{
    if (*cpuSend->ipcfifocnt & BIT(15)) // FIFO enabled
    {
        if (cpuSend->fifo->size() < 16) // FIFO not full
        {
            // Push a word to the back of the send FIFO
            cpuSend->fifo->push(*cpuSend->ipcfifosend);

            if (cpuSend->fifo->size() == 1)
            {
                // If the send FIFO isn't empty now, clear the FIFO empty bits
                *cpuSend->ipcfifocnt &= ~BIT(0);
                *cpuRecv->ipcfifocnt &= ~BIT(8);

                // Trigger a receive FIFO not empty IRQ if enabled
                if (*cpuRecv->ipcfifocnt & BIT(10))
                    *cpuRecv->irf |= BIT(18);
            }
            else if (cpuSend->fifo->size() == 16)
            {
                // If the send FIFO is full now, set the FIFO full bits
                *cpuSend->ipcfifocnt |= BIT(1);
                *cpuRecv->ipcfifocnt |= BIT(9);
            }
        }
        else
        {
            // The FIFO can only hold 16 words, so indicate a send full error
            *cpuSend->ipcfifocnt |= BIT(14);
        }
    }
}

uint32_t fifoReceive(interpreter::Cpu *cpuSend, interpreter::Cpu *cpuRecv)
{
    if (!cpuRecv->fifo->empty())
    {
        // The front word is received even if the FIFO isn't enabled
        cpuSend->ipcfiforecv = cpuRecv->fifo->front();

        if (*cpuSend->ipcfifocnt & BIT(15)) // FIFO enabled
        {
            // Remove a word from the front of receive FIFO
            cpuRecv->fifo->pop();

            if (cpuRecv->fifo->empty())
            {
                // If the receive FIFO is empty now, set the FIFO empty bits
                *cpuSend->ipcfifocnt |= BIT(8);
                *cpuRecv->ipcfifocnt |= BIT(0);

                // Trigger a receive FIFO empty IRQ if enabled
                if (*cpuRecv->ipcfifocnt & BIT(2))
                    *cpuRecv->irf |= BIT(17);
            }
            else if (cpuRecv->fifo->size() == 15)
            {
                // If the receive FIFO isn't full now, clear the FIFO full bits
                *cpuSend->ipcfifocnt &= ~BIT(9);
                *cpuRecv->ipcfifocnt &= ~BIT(1);
            }
        }
    }
    else
    {
        // If the receive FIFO is empty, indicate a receive empty error
        *cpuSend->ipcfifocnt |= BIT(14);
    }

    return cpuSend->ipcfiforecv;
}

void romTransferStart(interpreter::Cpu *cpu)
{
    // Don't do anything if the start bit isn't set
    if (!(*cpu->romctrl & BIT(31)))
        return;

    // Set the default size of the transfer
    switch (cpu->romcmdout[0])
    {
        case 0x9F: romReadSize = 0x2000; break; // Dummy
        case 0x00: romReadSize = 0x0200; break; // Get header
        case 0x90: romReadSize = 0x0004; break; // Get first chip ID

        default:
        {
            // Pretend the transfer completed so the program can continue
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
    }

    // Indicate that a word is ready
    // Transfers shouldn't complete instantly like this, but there's no timing system yet
    *cpu->romctrl |= BIT(23);
    romReadCount = 0;
}

uint32_t romTransfer(interpreter::Cpu *cpu)
{
    // Don't transfer if the word bit isn't set
    if (!(*cpu->romctrl & BIT(23)))
        return 0;

    romReadCount += 4;
    if (romReadCount == romReadSize)
    {
        // Indicated the transfer has finished once the transfer size is reached
        *cpu->romctrl &= ~BIT(23); // Word not ready
        *cpu->romctrl &= ~BIT(31); // Block ready
        if (*cpu->auxspicnt & BIT(14)) // Block ready IRQ
            *cpu->irf |= BIT(19);
    }

    // Return an endless stream of 0xFFs as if there isn't a cart inserted
    // Right now the ROM transfer system is just a hack designed to let the firmware boot
    return 0xFFFFFFFF;
}

void spiWrite(uint8_t value)
{
    // Don't do anything if the SPI isn't enabled
    if (!(*memory::spicnt & BIT(15)))
        return;

    // Receive the value written by the CPU and send a value back
    uint8_t device = (*memory::spicnt & 0x0300) >> 8;
    switch (device)
    {
        case 1: // Firmware
        {
            if (spiWriteCount == 0)
            {
                // On the first write, set the instruction byte
                spiInstr = value;
                spiAddr = 0;
                *memory::spidata = 0;
            }
            else if (spiInstr == 0x03) // READ
            {
                if (spiWriteCount < 4)
                {
                    // On writes 2-4, set the 3 byte address to read from
                    ((uint8_t*)&spiAddr)[3 - spiWriteCount] = value;
                }
                else
                {
                    // On writes 5+, read from the firmware and send it back
                    if (spiAddr < sizeof(firmware))
                        *memory::spidata = firmware[spiAddr];

                    // 16-bit mode is bugged; the address is incremented accordingly, but only the lower 8 bits are sent
                    spiAddr += (*memory::spicnt & BIT(10)) ? 2 : 1;
                }
            }
            else
            {
                *memory::spidata = 0;
                printf("Unknown firmware SPI instruction: 0x%X\n", spiInstr);
            }
            break;
        }

        default:
        {
            *memory::spidata = 0;
            printf("Write to unknown SPI device: %d\n", device);
            break;
        }
    }

    // Keep track of the write count
    if (*memory::spicnt & BIT(11)) // Keep chip selected
        spiWriteCount++;
    else // Deselect chip
        spiWriteCount = 0;

    // Trigger a transfer finished IRQ if enabled
    // Transfers shouldn't complete instantly like this, but there's no timing system yet
    if (*memory::spicnt & BIT(14))
        *interpreter::arm7.irf |= BIT(23);
}

void init()
{
    // Empty the FIFOs
    while (!fifo9.empty()) fifo9.pop();
    while (!fifo7.empty()) fifo7.pop();

    interpreter::arm9.fifo = &fifo9;
    interpreter::arm7.fifo = &fifo7;
}

}
