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

#include "ipc.h"
#include "defines.h"
#include "interpreter.h"

void Ipc::writeIpcSync9(unsigned int byte, uint8_t value)
{
    // Write to the ARM9's IPCSYNC register
    if (byte == 1)
    {
        ipcSync9 = (ipcSync9 & ~0x4F00) | ((value << 8) & 0x4F00);
        ipcSync7 = (ipcSync7 & ~0x000F) | (value & 0x000F);

        // Trigger a remote IRQ if enabled on both sides
        if ((value & BIT(5)) && (ipcSync7 & BIT(14)))
            arm7->sendInterrupt(16);
    }
}

void Ipc::writeIpcSync7(unsigned int byte, uint8_t value)
{
    // Write to the ARM7's IPCSYNC register
    if (byte == 1)
    {
        ipcSync7 = (ipcSync7 & ~0x4F00) | ((value << 8) & 0x4F00);
        ipcSync9 = (ipcSync9 & ~0x000F) | (value & 0x000F);

        // Trigger a remote IRQ if enabled on both sides
        if ((value & BIT(5)) && (ipcSync9 & BIT(14)))
            arm9->sendInterrupt(16);
    }
}

void Ipc::writeIpcFifoCnt9(unsigned int byte, uint8_t value)
{
    if (byte == 0)
    {
        // Clear the FIFO if the clear bit is set
        if (value & BIT(3) && !fifo9.empty())
        {
            // Empty the FIFO
            while (!fifo9.empty())
                fifo9.pop();
            ipcFifoRecv7 = 0;

            // Set the FIFO empty bits and clear the FIFO full bits
            ipcFifoCnt9 |=  BIT(0);
            ipcFifoCnt9 &= ~BIT(1);
            ipcFifoCnt7 |=  BIT(8);
            ipcFifoCnt7 &= ~BIT(9);

            // Trigger a send FIFO empty IRQ if enabled
            if (ipcFifoCnt9 & BIT(2))
                arm9->sendInterrupt(17);
        }

        // Trigger a send FIFO empty IRQ if the enable bit is set and the FIFO is empty
        if ((ipcFifoCnt9 & BIT(0)) && !(ipcFifoCnt9 & BIT(2)) && (value & BIT(2)))
            arm9->sendInterrupt(17);
    }
    else if (byte == 1)
    {
        // Trigger a receive FIFO not empty IRQ if the enable bit is set and the FIFO isn't empty
        if (!(ipcFifoCnt9 & BIT(8)) && !(ipcFifoCnt9 & BIT(10)) && (value & BIT(2)))
            arm9->sendInterrupt(18);

        // If the error bit is set, acknowledge the error by clearing it
        if (value & BIT(6)) ipcFifoCnt9 &= ~BIT(14);
    }

    // Write to the ARM9's IPCFIFOCNT register
    uint16_t mask = 0x8404 & (0xFF << (byte * 8));
    ipcFifoCnt9 = (ipcFifoCnt9 & ~mask) | ((value << (byte * 8)) & mask);
}

void Ipc::writeIpcFifoCnt7(unsigned int byte, uint8_t value)
{
    if (byte == 0)
    {
        // Clear the FIFO if the clear bit is set
        if (value & BIT(3) && !fifo7.empty())
        {
            // Empty the FIFO
            while (!fifo7.empty())
                fifo7.pop();
            ipcFifoRecv9 = 0;

            // Set the FIFO empty bits and clear the FIFO full bits
            ipcFifoCnt7 |=  BIT(0);
            ipcFifoCnt7 &= ~BIT(1);
            ipcFifoCnt9 |=  BIT(8);
            ipcFifoCnt9 &= ~BIT(9);

            // Trigger a send FIFO empty IRQ if enabled
            if (ipcFifoCnt7 & BIT(2))
                arm7->sendInterrupt(17);
        }

        // Trigger a send FIFO empty IRQ if the enable bit is set and the FIFO is empty
        if ((ipcFifoCnt7 & BIT(0)) && !(ipcFifoCnt7 & BIT(2)) && (value & BIT(2)))
            arm7->sendInterrupt(17);
    }
    else if (byte == 1)
    {
        // Trigger a receive FIFO not empty IRQ if the enable bit is set and the FIFO isn't empty
        if (!(ipcFifoCnt7 & BIT(8)) && !(ipcFifoCnt7 & BIT(10)) && (value & BIT(2)))
            arm7->sendInterrupt(18);

        // If the error bit is set, acknowledge the error by clearing it
        if (value & BIT(6)) ipcFifoCnt7 &= ~BIT(14);
    }

    // Write to the ARM7's IPCFIFOCNT register
    uint16_t mask = 0x8404 & (0xFF << (byte * 8));
    ipcFifoCnt7 = (ipcFifoCnt7 & ~mask) | ((value << (byte * 8)) & mask);
}

void Ipc::writeIpcFifoSend9(unsigned int byte, uint8_t value)
{
    // Write to the ARM9's IPCFIFOSEND register
    ipcFifoSend9 = (ipcFifoSend9 & ~(0xFF << (byte * 8))) | (value << (byte * 8));

    if (byte == 3)
    {
        if (ipcFifoCnt9 & BIT(15)) // FIFO enabled
        {
            if (fifo9.size() < 16) // FIFO not full
            {
                // Push a word to the FIFO
                fifo9.push(ipcFifoSend9);

                if (fifo9.size() == 1)
                {
                    // If the FIFO is no longer empty, clear the empty bits
                    ipcFifoCnt9 &= ~BIT(0);
                    ipcFifoCnt7 &= ~BIT(8);

                    // Trigger a receive FIFO not empty IRQ if enabled
                    if (ipcFifoCnt7 & BIT(10))
                        arm7->sendInterrupt(18);
                }
                else if (fifo9.size() == 16)
                {
                    // If the FIFO is now full, set the full bits
                    ipcFifoCnt9 |= BIT(1);
                    ipcFifoCnt7 |= BIT(9);
                }
            }
            else
            {
                // The FIFO can only hold 16 words, so indicate a send full error
                ipcFifoCnt9 |= BIT(14);
            }
        }

        ipcFifoSend9 = 0;
    }
}

void Ipc::writeIpcFifoSend7(unsigned int byte, uint8_t value)
{
    // Write to the ARM7's IPCFIFOSEND register
    ipcFifoSend7 = (ipcFifoSend7 & ~(0xFF << (byte * 8))) | (value << (byte * 8));

    if (byte == 3)
    {
        if (ipcFifoCnt7 & BIT(15)) // FIFO enabled
        {
            if (fifo7.size() < 16) // FIFO not full
            {
                // Push a word to the FIFO
                fifo7.push(ipcFifoSend7);

                if (fifo7.size() == 1)
                {
                    // If the FIFO is no longer empty, clear the empty bits
                    ipcFifoCnt7 &= ~BIT(0);
                    ipcFifoCnt9 &= ~BIT(8);

                    // Trigger a receive FIFO not empty IRQ if enabled
                    if (ipcFifoCnt9 & BIT(10))
                        arm9->sendInterrupt(18);
                }
                else if (fifo7.size() == 16)
                {
                    // If the FIFO is now full, set the full bits
                    ipcFifoCnt7 |= BIT(1);
                    ipcFifoCnt9 |= BIT(9);
                }
            }
            else
            {
                // The FIFO can only hold 16 words, so indicate a send full error
                ipcFifoCnt7 |= BIT(14);
            }
        }

        ipcFifoSend7 = 0;
    }
}

uint8_t Ipc::readIpcFifoRecv9(unsigned int byte)
{
    if (byte == 0)
    {
        if (!fifo7.empty())
        {
            // Receive a word from the FIFO
            ipcFifoRecv9 = fifo7.front();

            if (ipcFifoCnt9 & BIT(15)) // FIFO enabled
            {
                // Remove the received word from the FIFO
                fifo7.pop();

                if (fifo7.empty())
                {
                    // If the FIFO is now empty, set the empty bits
                    ipcFifoCnt9 |= BIT(8);
                    ipcFifoCnt7 |= BIT(0);

                    // Trigger a receive FIFO empty IRQ if enabled
                    if (ipcFifoCnt7 & BIT(2))
                        arm7->sendInterrupt(17);
                }
                else if (fifo7.size() == 15)
                {
                    // If the FIFO is no longer full, clear the full bits
                    ipcFifoCnt9 &= ~BIT(9);
                    ipcFifoCnt7 &= ~BIT(1);
                }
            }
        }
        else
        {
            // If the FIFO is empty, indicate a receive empty error
            ipcFifoCnt9 |= BIT(14);
        }
    }

    return ipcFifoRecv9 >> (byte * 8);
}

uint8_t Ipc::readIpcFifoRecv7(unsigned int byte)
{
    if (byte == 0)
    {
        if (!fifo9.empty())
        {
            // Receive a word from the FIFO
            ipcFifoRecv7 = fifo9.front();

            if (ipcFifoCnt7 & BIT(15)) // FIFO enabled
            {
                // Remove the received word from the FIFO
                fifo9.pop();

                if (fifo9.empty())
                {
                    // If the FIFO is now empty, set the empty bits
                    ipcFifoCnt7 |= BIT(8);
                    ipcFifoCnt9 |= BIT(0);

                    // Trigger a receive FIFO empty IRQ if enabled
                    if (ipcFifoCnt9 & BIT(2))
                        arm9->sendInterrupt(17);
                }
                else if (fifo9.size() == 15)
                {
                    // If the FIFO is no longer full, clear the full bits
                    ipcFifoCnt7 &= ~BIT(9);
                    ipcFifoCnt9 &= ~BIT(1);
                }
            }
        }
        else
        {
            // If the FIFO is empty, indicate a receive empty error
            ipcFifoCnt7 |= BIT(14);
        }
    }

    return ipcFifoRecv7 >> (byte * 8);
}
