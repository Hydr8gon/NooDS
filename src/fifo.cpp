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

#include "fifo.h"
#include "core.h"
#include "memory.h"

namespace fifo
{

std::queue<uint32_t> fifo9, fifo7;

void clear(interpreter::Cpu *cpuSend, interpreter::Cpu *cpuRecv)
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

void send(interpreter::Cpu *cpuSend, interpreter::Cpu *cpuRecv)
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

uint32_t receive(interpreter::Cpu *cpuSend, interpreter::Cpu *cpuRecv)
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

void init()
{
    // Empty the FIFOs
    while (!fifo9.empty()) fifo9.pop();
    while (!fifo7.empty()) fifo7.pop();

    interpreter::arm9.fifo = &fifo9;
    interpreter::arm7.fifo = &fifo7;
}

}
