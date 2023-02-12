/*
    Copyright 2019-2022 Hydr8gon

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

#include <cstring>

#include "spi.h"
#include "core.h"
#include "settings.h"

Language Spi::language = LG_ENGLISH;

Spi::~Spi()
{
    // Free any dynamic memory
    if (firmware)  delete[] firmware;
    if (micBuffer) delete[] micBuffer;
}

uint16_t Spi::crc16(uint32_t value, uint8_t *data, size_t size)
{
    static const uint16_t table[] = { 0xC0C1, 0xC181, 0xC301, 0xC601, 0xCC01, 0xD801, 0xF001, 0xA001 };

    // Calculate a CRC16 value for the given data
    for (size_t i = 0; i < size; i++)
    {
        value ^= data[i];
        for (size_t j = 0; j < 8; j++)
            value = (value >> 1) ^ ((value & 1) ? (table[j] << (7 - j)) : 0);
    }

    return value;
}

bool Spi::loadFirmware()
{
    // Ensure firmware memory isn't already allocated
    if (firmware)
        delete[] firmware;

    // Load the firmware from a file if it exists
    if (FILE *file = fopen(Settings::getFirmwarePath().c_str(), "rb"))
    {
        fseek(file, 0, SEEK_END);
        firmSize = ftell(file);
        fseek(file, 0, SEEK_SET);
        firmware = new uint8_t[firmSize];
        fread(firmware, sizeof(uint8_t), firmSize, file);
        fclose(file);

        if (core->getId() > 0)
        {
            // Increment the MAC address based on the instance ID
            // This allows instances to be detected as separate systems
            firmware[0x36] += core->getId();

            // Recalculate the WiFi config CRC
            uint16_t crc = crc16(0, &firmware[0x2C], 0x138);
            firmware[0x2A] = crc >> 0;
            firmware[0x2B] = crc >> 8;
        }

        return (firmSize > 0x20000); // Bootable
    }

    // Create a basic, non-bootable firmware if one isn't provided
    firmSize = 0x20000;
    firmware = new uint8_t[firmSize];
    memset(firmware, 0, firmSize);

    // Set some firmware header data
    firmware[0x20] = 0xC0; // User settings offset / 8, byte 1
    firmware[0x21] = 0x3F; // User settings offset / 8, byte 2

    // Set some WiFi config data
    firmware[0x2C] = 0x38; // Config length, byte 1
    firmware[0x2D] = 0x01; // Config length, byte 2
    firmware[0x36] = core->getId(); // MAC address, byte 1
    firmware[0x37] = 0x09; // MAC address, byte 2
    firmware[0x38] = 0xBF; // MAC address, byte 3
    firmware[0x39] = 0x12; // MAC address, byte 4
    firmware[0x3A] = 0x34; // MAC address, byte 5
    firmware[0x3B] = 0x56; // MAC address, byte 6
    firmware[0x3C] = 0xFE; // Enabled channels, byte 1
    firmware[0x3D] = 0x3F; // Enabled channels, byte 2

    // Calculate the WiFi config CRC
    uint16_t crc = crc16(0, &firmware[0x2C], 0x138);
    firmware[0x2A] = crc >> 0;
    firmware[0x2B] = crc >> 8;

    // Configure the WiFi access points
    for (uint32_t addr = 0x1FA00; addr <= 0x1FC00; addr += 0x100)
    {
        // Set some access point data
        firmware[addr + 0xE7] = 0xFF; // Not configured
        firmware[addr + 0xF5] = 0x28; // Unknown

        // Calculate the access point CRC
        crc = crc16(0, &firmware[addr], 0xFE);
        firmware[addr + 0xFE] = crc >> 0;
        firmware[addr + 0xFF] = crc >> 8;
    }

    // Configure the user settings
    for (uint32_t addr = 0x1FE00; addr <= 0x1FF00; addr += 0x100)
    {
        // Set some user settings data
        firmware[addr + 0x00] =  5;  // Version
        firmware[addr + 0x02] =  2;  // Favorite color
        firmware[addr + 0x03] =  5;  // Birthday month
        firmware[addr + 0x04] = 25;  // Birthday day
        firmware[addr + 0x06] = 'N'; // Nickname, char 1
        firmware[addr + 0x08] = 'o'; // Nickname, char 2
        firmware[addr + 0x0A] = 'o'; // Nickname, char 3
        firmware[addr + 0x0C] = 'D'; // Nickname, char 4
        firmware[addr + 0x0E] = 'S'; // Nickname, char 5
        firmware[addr + 0x1A] =  5;  // Nickname length

        // Set the touch calibration data
        firmware[addr + 0x5E] = 0xF0; // ADC X2, byte 1
        firmware[addr + 0x5F] = 0x0F; // ADC X2, byte 2
        firmware[addr + 0x60] = 0xF0; // ADC Y2, byte 1
        firmware[addr + 0x61] = 0x0B; // ADC Y2, byte 2
        firmware[addr + 0x62] = 0xFF; // SCR X2
        firmware[addr + 0x63] = 0xBF; // SCR Y2

        // Set the language specified by the frontend
        firmware[addr + 0x64] = language;

        // Calculate the user settings CRC
        crc = crc16(0xFFFF, &firmware[addr], 0x70);
        firmware[addr + 0x72] = crc >> 0;
        firmware[addr + 0x73] = crc >> 8;
    }

    return false;
}

void Spi::directBoot()
{
    // Load the user settings into memory
    for (uint32_t i = 0; i < 0x70; i++)
        core->memory.write<uint8_t>(0, 0x27FFC80 + i, firmware[firmSize - 0x100 + i]);
}

void Spi::setTouch(int x, int y)
{
    if (!firmware)
        return;

    // Read calibration points from the firmware
    uint16_t adcX1 = U8TO16(firmware, firmSize - 0xA8);
    uint16_t adcY1 = U8TO16(firmware, firmSize - 0xA6);
    uint8_t  scrX1 = firmware[firmSize - 0xA4];
    uint8_t  scrY1 = firmware[firmSize - 0xA3];
    uint16_t adcX2 = U8TO16(firmware, firmSize - 0xA2);
    uint16_t adcY2 = U8TO16(firmware, firmSize - 0xA0);
    uint8_t  scrX2 = firmware[firmSize - 0x9E];
    uint8_t  scrY2 = firmware[firmSize - 0x9D];

    // Ensure the coordinates are within bounds
    // A one pixel border around the screen is ignored to avoid potential underflow/overflow
    // This should be fine; GBATEK says that pressing near the screen borders may be impossible anyways
    if (x < 1) x = 1; else if (x > 254) x = 254;
    if (y < 1) y = 1; else if (y > 190) y = 190;

    // Convert the coordinates to ADC values
    if (scrX2 - scrX1 != 0) touchX = (x - (scrX1 - 1)) * (adcX2 - adcX1) / (scrX2 - scrX1) + adcX1;
    if (scrY2 - scrY1 != 0) touchY = (y - (scrY1 - 1)) * (adcY2 - adcY1) / (scrY2 - scrY1) + adcY1;
}

void Spi::clearTouch()
{
    // Set the ADC values to their default state
    touchX = 0x000;
    touchY = 0xFFF;
}

void Spi::sendMicData(const int16_t* samples, size_t count, size_t rate)
{
    mutex.lock();

    // Copy samples into the microphone buffer
    if (micBuffer) delete[] micBuffer;
    micBuffer = new int16_t[count];
    memcpy(micBuffer, samples, count * sizeof(int16_t));
    micBufSize = count;

    // Set the cycle start time of the microphone buffer, and the number of cycles per sample
    micCycles = core->getGlobalCycles();
    micStep = (60 * 263 * 355 * 6) / rate;

    mutex.unlock();
}

void Spi::writeSpiCnt(uint16_t mask, uint16_t value)
{
    // Write to the SPICNT register
    mask &= 0xCF03;
    spiCnt = (spiCnt & ~mask) | (value & mask);
}

void Spi::writeSpiData(uint8_t value)
{
    // Don't do anything if the SPI isn't enabled
    if (!(spiCnt & BIT(15)))
    {
        spiData = 0;
        return;
    }

    if (writeCount == 0)
    {
        // On the first write, set the command byte
        command = value;
        address = 0;
        spiData = 0;
    }
    else
    {
        switch ((spiCnt & 0x0300) >> 8) // Device
        {
            case 1: // Firmware
            {
                switch (command)
                {
                    case 0x03: // Read data bytes
                        if (writeCount < 4)
                        {
                            // On writes 2-4, set the 3 byte address to read from
                            address |= value << ((3 - writeCount) * 8);
                        }
                        else
                        {
                            // On writes 5+, read data from the firmware and send it back
                            spiData = (address < firmSize) ? firmware[address] : 0;

                            // Increment the address
                            // 16-bit mode is bugged; the address is incremented accordingly, but only the lower 8 bits are sent
                            address += (spiCnt & BIT(10)) ? 2 : 1;
                        }
                        break;

                    default:
                        LOG("Write to SPI with unknown firmware command: 0x%X\n", command);
                        spiData = 0;
                        break;
                }
                break;
            }

            case 2: // Touchscreen
            {
                switch ((command & 0x70) >> 4) // Channel
                {
                    case 1: // Y-coordinate
                        // Send the ADC Y coordinate MSB first, with 3 dummy bits in front
                        spiData = (writeCount & 1) ? (touchY >> 5) : (touchY << 3);
                        break;

                    case 5: // X-coordinate
                        // Send the ADC X coordinate MSB first, with 3 dummy bits in front
                        spiData = (writeCount & 1) ? (touchX >> 5) : (touchX << 3);
                        break;

                    case 6: // AUX input
                        if (writeCount & 1)
                        {
                            // Load a sample based on cycle time since the buffer was sent
                            // The sample is converted to an unsigned 12-bit value
                            mutex.lock();
                            size_t index = std::min<size_t>((core->getGlobalCycles() - micCycles) / micStep, micBufSize);
                            micSample = (micBufSize > 0) ? ((micBuffer[index] >> 4) + 0x800) : 0;
                            mutex.unlock();

                            // Send the most significant 7 bits of the sample first
                            spiData = micSample >> 5;
                            break;
                        }

                        // Send the last 5 bits of the sample. with 3 dummy bits in front
                        spiData = micSample << 3;
                        break;

                    default:
                        LOG("Write to SPI with unknown touchscreen channel: %d\n", (command & 0x70) >> 4);
                        spiData = 0;
                        break;
                }
                break;
            }

            default:
                LOG("Write to SPI with unknown device: %d\n", (spiCnt & 0x0300) >> 8);
                spiData = 0;
                break;
        }
    }

    // Keep track of the write count
    if (spiCnt & BIT(11)) // Keep chip selected
        writeCount++;
    else // Deselect chip
        writeCount = 0;

    // Trigger a transfer finished IRQ if enabled
    if (spiCnt & BIT(14))
        core->interpreter[1].sendInterrupt(23);
}
