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

#include "spi.h"
#include "core.h"
#include "memory.h"

namespace spi
{

uint8_t firmware[0x40000];

uint32_t writeCount;
uint32_t address;
uint8_t command;

void write(uint8_t value)
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
            if (writeCount == 0)
            {
                // On the first write, set the command byte
                command = value;
                address = 0;
                *memory::spidata = 0;
            }
            else if (command == 0x03) // READ
            {
                if (writeCount < 4)
                {
                    // On writes 2-4, set the 3 byte address to read from
                    ((uint8_t*)&address)[3 - writeCount] = value;
                }
                else
                {
                    // On writes 5+, read from the firmware and send it back
                    if (address < sizeof(firmware))
                        *memory::spidata = firmware[address];

                    // 16-bit mode is bugged; the address is incremented accordingly, but only the lower 8 bits are sent
                    address += (*memory::spicnt & BIT(10)) ? 2 : 1;
                }
            }
            else
            {
                *memory::spidata = 0;
                printf("Unknown firmware SPI instruction: 0x%X\n", command);
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
        writeCount++;
    else // Deselect chip
        writeCount = 0;

    // Trigger a transfer finished IRQ if enabled
    // Transfers shouldn't complete instantly like this, but there's no timing system yet
    if (*memory::spicnt & BIT(14))
        *interpreter::arm7.irf |= BIT(23);
}

void init()
{
    writeCount = 0;
}

}
