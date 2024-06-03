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

#include "ipc.h"
#include "core.h"

void Ipc::saveState(FILE *file)
{
    // Write state data to the file
    fwrite(ipcSync, 2, sizeof(ipcSync) / 2, file);
    fwrite(ipcFifoCnt, 2, sizeof(ipcFifoCnt) / 2, file);
    fwrite(ipcFifoRecv, 4, sizeof(ipcFifoRecv) / 4, file);

    // Parse the FIFOs and save their values
    for (int i = 0; i < 2; i++)
    {
        uint32_t count = fifos[i].size();
        fwrite(&count, sizeof(count), 1, file);
        for (uint32_t j = 0; j < count; j++)
            fwrite(&fifos[i][j], sizeof(fifos[i][j]), 1, file);
    }
}

void Ipc::loadState(FILE *file)
{
    // Read state data from the file
    fread(ipcSync, 2, sizeof(ipcSync) / 2, file);
    fread(ipcFifoCnt, 2, sizeof(ipcFifoCnt) / 2, file);
    fread(ipcFifoRecv, 4, sizeof(ipcFifoRecv) / 4, file);

    // Reset the FIFOs and refill them with loaded values
    for (int i = 0; i < 2; i++)
    {
        fifos[i].clear();
        uint32_t count, value;
        fread(&count, sizeof(count), 1, file);
        for (uint32_t j = 0; j < count; j++)
        {
            fread(&value, sizeof(value), 1, file);
            fifos[i].push_back(value);
        }
    }
}

void Ipc::writeIpcSync(bool cpu, uint16_t mask, uint16_t value)
{
    // Write to one of the IPCSYNC registers
    mask &= 0x4F00;
    ipcSync[cpu] = (ipcSync[cpu] & ~mask) | (value & mask);

    // Copy the input bits from this CPU to the output bits for the other CPU
    ipcSync[!cpu] = (ipcSync[!cpu] & ~((mask >> 8) & 0x000F)) | (((value & mask) >> 8) & 0x000F);

    // Trigger a remote IRQ if enabled on both sides
    if ((value & BIT(13)) && (ipcSync[!cpu] & BIT(14)))
        core->interpreter[!cpu].sendInterrupt(16);
}

void Ipc::writeIpcFifoCnt(bool cpu, uint16_t mask, uint16_t value)
{
    // Clear the FIFO if the clear bit is set
    if ((value & BIT(3)) && !fifos[cpu].empty())
    {
        // Empty the FIFO
        while (!fifos[cpu].empty())
            fifos[cpu].pop_front();
        ipcFifoRecv[!cpu] = 0;

        // Set the FIFO empty bits and clear the FIFO full bits
        ipcFifoCnt[cpu]  |=  BIT(0);
        ipcFifoCnt[cpu]  &= ~BIT(1);
        ipcFifoCnt[!cpu] |=  BIT(8);
        ipcFifoCnt[!cpu] &= ~BIT(9);

        // Trigger a send FIFO empty IRQ if enabled
        if (ipcFifoCnt[cpu] & BIT(2))
            core->interpreter[cpu].sendInterrupt(17);
    }

    // Trigger a send FIFO empty IRQ if the enable bit is set and the FIFO is empty
    if ((ipcFifoCnt[cpu] & BIT(0)) && !(ipcFifoCnt[cpu] & BIT(2)) && (value & BIT(2)))
        core->interpreter[cpu].sendInterrupt(17);

    // Trigger a receive FIFO not empty IRQ if the enable bit is set and the FIFO isn't empty
    if (!(ipcFifoCnt[cpu] & BIT(8)) && !(ipcFifoCnt[cpu] & BIT(10)) && (value & BIT(10)))
        core->interpreter[cpu].sendInterrupt(18);

    // If the error bit is set, acknowledge the error by clearing it
    if (value & BIT(14)) ipcFifoCnt[cpu] &= ~BIT(14);

    // Write to one of the IPCFIFOCNT registers
    mask &= 0x8404;
    ipcFifoCnt[cpu] = (ipcFifoCnt[cpu] & ~mask) | (value & mask);
}

void Ipc::writeIpcFifoSend(bool cpu, uint32_t mask, uint32_t value)
{
    if (ipcFifoCnt[cpu] & BIT(15)) // FIFO enabled
    {
        if (fifos[cpu].size() < 16) // FIFO not full
        {
            // Push a word to the FIFO
            fifos[cpu].push_back(value & mask);

            if (fifos[cpu].size() == 1)
            {
                // If the FIFO is no longer empty, clear the empty bits
                ipcFifoCnt[cpu]  &= ~BIT(0);
                ipcFifoCnt[!cpu] &= ~BIT(8);

                // Trigger a receive FIFO not empty IRQ if enabled
                if (ipcFifoCnt[!cpu] & BIT(10))
                    core->interpreter[!cpu].sendInterrupt(18);
            }
            else if (fifos[cpu].size() == 16)
            {
                // If the FIFO is now full, set the full bits
                ipcFifoCnt[cpu]  |= BIT(1);
                ipcFifoCnt[!cpu] |= BIT(9);
            }
        }
        else
        {
            // The FIFO can only hold 16 words, so indicate a send full error
            ipcFifoCnt[cpu] |= BIT(14);
        }
    }
}

uint32_t Ipc::readIpcFifoRecv(bool cpu)
{
    if (!fifos[!cpu].empty())
    {
        // Receive a word from the FIFO
        ipcFifoRecv[cpu] = fifos[!cpu].front();

        if (ipcFifoCnt[cpu] & BIT(15)) // FIFO enabled
        {
            // Remove the received word from the FIFO
            fifos[!cpu].pop_front();

            if (fifos[!cpu].empty())
            {
                // If the FIFO is now empty, set the empty bits
                ipcFifoCnt[cpu]  |= BIT(8);
                ipcFifoCnt[!cpu] |= BIT(0);

                // Trigger a receive FIFO empty IRQ if enabled
                if (ipcFifoCnt[!cpu] & BIT(2))
                    core->interpreter[!cpu].sendInterrupt(17);
            }
            else if (fifos[!cpu].size() == 15)
            {
                // If the FIFO is no longer full, clear the full bits
                ipcFifoCnt[cpu]  &= ~BIT(9);
                ipcFifoCnt[!cpu] &= ~BIT(1);
            }
        }
    }
    else
    {
        // If the FIFO is empty, indicate a receive empty error
        ipcFifoCnt[cpu] |= BIT(14);
    }

    return ipcFifoRecv[cpu];
}
