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
uint8_t *save;

uint32_t writeCount, auxWriteCount;
uint32_t address, auxAddress;
uint8_t command, auxCommand;

uint16_t touchX, touchY;
uint32_t saveSize;

void write(uint8_t value)
{
    // Don't do anything if the SPI isn't enabled
    if (!(*memory::spicnt & BIT(15)))
        return;

    if (writeCount == 0)
    {
        // On the first write, set the command byte
        command = value;
        address = 0;
        *memory::spidata = 0;
    }
    else
    {
        // Receive the value written by the CPU and send a value back
        uint8_t device = (*memory::spicnt & 0x0300) >> 8;
        switch (device)
        {
            case 1: // Firmware
            {
                switch (command)
                {
                    case 0x03: // Read data bytes
                    {
                        if (writeCount < 4)
                        {
                            // On writes 2-4, set the 3 byte address to read from
                            address |= value << ((3 - writeCount) * 8);
                        }
                        else
                        {
                            // On writes 5+, read data from the firmware and send it back
                            if (address < sizeof(firmware))
                                    *memory::spidata = firmware[address];

                            // 16-bit mode is bugged; the address is incremented accordingly, but only the lower 8 bits are sent
                            address += (*memory::spicnt & BIT(10)) ? 2 : 1;
                        }

                        break;
                    }

                    default:
                    {
                        *memory::spidata = 0;
                        printf("Unknown firmware SPI instruction: 0x%X\n", command);
                        break;
                    }
                }

                break;
            }

            case 2: // Touchscreen
            {
                uint8_t channel = (command & 0x70) >> 4;
                switch (channel)
                {
                    case 1: // Y-coordinate
                    {
                        // Send the ADC Y coordinate MSB first, with 3 dummy bits in front
                        *memory::spidata = ((touchY << 11) & 0xFF00) | ((touchY >> 5) & 0x00FF) >> ((writeCount - 1) % 2);
                        break;
                    }

                    case 5: // X-coordinate
                    {
                        // Send the ADC X coordinate MSB first, with 3 dummy bits in front
                        *memory::spidata = ((touchX << 11) & 0xFF00) | ((touchX >> 5) & 0x00FF) >> ((writeCount - 1) % 2);
                        break;
                    }

                    default:
                    {
                        *memory::spidata = 0;
                        printf("Unknown touchscreen SPI channel: %d\n", channel);
                        break;
                    }
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

void auxWrite(interpreter::Cpu *cpu, uint8_t value)
{
    if (auxWriteCount == 0)
    {
        // On the first write, set the command byte
        auxCommand = value;
        auxAddress = 0;
        *cpu->auxspidata = 0;
    }
    else
    {
        // Receive the value written by the CPU and send a value back
        switch (saveSize)
        {
            case 0x2000: case 0x10000: // EEPROM 8KB, 64KB
            {
                switch (auxCommand)
                {
                    case 0x03: // Read from memory
                    {
                        if (auxWriteCount < 3)
                        {
                            // On writes 2-3, set the 2 byte address to write to
                            auxAddress |= value << ((2 - auxWriteCount) * 8);
                        }
                        else
                        {
                            // On writes 4+, read data from the save and send it back
                            if (auxAddress < saveSize)
                                *cpu->auxspidata = save[auxAddress];
                            auxAddress++;
                        }
                        break;
                    }

                    case 0x02: // Write to memory
                    {
                        if (auxWriteCount < 3)
                        {
                            // On writes 2-3, set the 2 byte address to write to
                            auxAddress |= value << ((2 - auxWriteCount) * 8);
                        }
                        else
                        {
                            // On writes 4+, write data to the save
                            if (auxAddress < saveSize)
                                save[auxAddress] = value;
                            auxAddress++;
                        }
                        break;
                    }

                    default:
                    {
                        *cpu->auxspidata = 0;
                        printf("Unknown EEPROM SPI command: %d\n", auxCommand);
                        break;
                    }
                }

                break;
            }

            case 0x40000: case 0x80000: // FLASH 256KB, 512KB
            {
                // Receive the value written by the CPU and send a value back
                switch (auxCommand)
                {
                    case 0x03: // Read data bytes
                    {
                        if (auxWriteCount < 4)
                        {
                            // On writes 2-4, set the 3 byte address to write to
                            auxAddress |= value << ((3 - auxWriteCount) * 8);
                        }
                        else
                        {
                            // On writes 5+, read data from the save and send it back
                            if (auxAddress < saveSize)
                                *cpu->auxspidata = save[auxAddress];
                            auxAddress++;
                        }
                        break;
                    }

                    case 0x0A: // Page write
                    {
                        if (auxWriteCount < 4)
                        {
                            // On writes 2-4, set the 3 byte address to write to
                            auxAddress |= value << ((3 - auxWriteCount) * 8);
                        }
                        else
                        {
                            // On writes 5+, write data to the save
                            if (auxAddress < saveSize)
                                save[auxAddress] = value;
                            auxAddress++;
                        }
                        break;
                    }

                    default:
                    {
                        *cpu->auxspidata = 0;
                        printf("Unknown FLASH SPI command: %d\n", auxCommand);
                        break;
                    }
                }

                break;
            }

            default:
            {
                printf("Write to AUX SPI with unknown save type\n");
                break;
            }
        }
    }

    // Keep track of the write count
    if (*cpu->auxspicnt & BIT(6)) // Keep chip selected
        auxWriteCount++;
    else // Deselect chip
        auxWriteCount = 0;
}

void init()
{
    writeCount = auxWriteCount = 0;
    address = auxAddress = 0;
    command = auxCommand = 0;

    touchX = touchY = 0;
}

}
