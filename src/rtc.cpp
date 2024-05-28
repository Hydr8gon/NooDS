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

#include <ctime>

#include "rtc.h"
#include "core.h"

void Rtc::updateRtc(bool cs, bool sck, bool sio)
{
    if (cs)
    {
        // Transfer a bit to the RTC when SCK changes from low to high
        if (!sckCur && sck)
        {
            if (writeCount < 8)
            {
                // Write the first 8 bits to the command register
                command |= sio << (7 - writeCount);

                // Once the command is written, reverse the bit order if necessary
                if (writeCount == 7 && (command & 0xF0) != 0x60)
                {
                    uint8_t value = 0;
                    for (int i = 0; i < 8; i++)
                        value |= ((command >> i) & BIT(0)) << (7 - i);
                    command = value;
                }
            }
            else if (command & BIT(0))
            {
                // Read a bit from an RTC register
                sio = readRegister((command >> 1) & 0x7);
            }
            else
            {
                // Write a bit to an RTC register
                writeRegister((command >> 1) & 0x7, sio);
            }

            writeCount++;
        }
    }
    else
    {
        // Reset the transfer when CS is low
        writeCount = 0;
        command = 0;
    }

    // Update the CS/SCK/SIO bits
    csCur = cs;
    sckCur = sck;
    sioCur = sio;
}

void Rtc::updateDateTime()
{
    // Get the local time
    std::time_t t = std::time(nullptr);
    std::tm *time = std::localtime(&t);
    time->tm_year %= 100; // The DS only counts years 2000-2099
    time->tm_mon++; // The DS starts month values at 1, not 0

    // Convert to 12-hour format if enabled
    if (!(control & BIT(core->gbaMode ? 6 : 1)))
        time->tm_hour %= 12;

    // Save to the date and time registers in BCD format
    // Index 3 contains the day of the week, but most things don't care
    dateTime[0] = ((time->tm_year / 10) << 4) | (time->tm_year % 10);
    dateTime[1] = ((time->tm_mon  / 10) << 4) | (time->tm_mon  % 10);
    dateTime[2] = ((time->tm_mday / 10) << 4) | (time->tm_mday % 10);
    dateTime[4] = ((time->tm_hour / 10) << 4) | (time->tm_hour % 10);
    dateTime[5] = ((time->tm_min  / 10) << 4) | (time->tm_min  % 10);
    dateTime[6] = ((time->tm_sec  / 10) << 4) | (time->tm_sec  % 10);

    // Set the AM/PM bit
    if (time->tm_hour >= 12)
        dateTime[4] |= BIT(6 << core->gbaMode);
}

void Rtc::reset()
{
    // Reset the RTC registers
    updateRtc(0, 0, 0);
    control = 0;
    rtc = 0;
    gpDirection = 0;
    gpControl = 0;
}

bool Rtc::readRegister(uint8_t index)
{
    if (core->gbaMode)
    {
        // Read a bit from a GBA RTC register
        switch (index)
        {
            case 0: // Reset
                reset();
                return 0;

            case 1: // Control
                return (control >> (writeCount & 7)) & BIT(0);

            case 2: // Date and time
                if (writeCount == 8) updateDateTime();
                return (dateTime[(writeCount / 8) - 1] >> (writeCount % 8)) & BIT(0);

            case 3: // Time
                if (writeCount == 8) updateDateTime();
                return (dateTime[(writeCount / 8) - 5] >> (writeCount % 8)) & BIT(0);

            default:
                LOG("Read from unknown GBA RTC register: %d\n", index);
                return 0;
        }
    }

    // Read a bit from an NDS RTC register
    switch (index)
    {
        case 0: // Status 1
            return (control >> (writeCount & 7)) & BIT(0);

        case 2: // Date and time
            if (writeCount == 8) updateDateTime();
            return (dateTime[(writeCount / 8) - 1] >> (writeCount & 7)) & BIT(0);

        case 3: // Time
            if (writeCount == 8) updateDateTime();
            return (dateTime[(writeCount / 8) - 5] >> (writeCount & 7)) & BIT(0);

        default:
            LOG("Read from unknown RTC register: %d\n", index);
            return 0;
    }
}

void Rtc::writeRegister(uint8_t index, bool value)
{
    if (core->gbaMode)
    {
        // Read a bit from a GBA RTC register
        switch (index)
        {
            case 1: // Control
                if (BIT(writeCount & 7) & 0x6A) // R/W bits
                    control = (control & ~BIT(writeCount & 7)) | (value << (writeCount & 7));
                return;

            default:
                LOG("Write to unknown GBA RTC register: %d\n", index);
                return;
        }
    }

    // Write a bit to an NDS RTC register
    switch (index)
    {
        case 0: // Status 1
            if ((BIT(writeCount & 7) & 0x01) && value) // Reset bit
                reset();
            else if (BIT(writeCount & 7) & 0x0E) // R/W bits
                control = (control & ~BIT(writeCount & 7)) | (value << (writeCount & 7));
            return;

        default:
            LOG("Write to unknown RTC register: %d\n", index);
            return;
    }
}

void Rtc::writeRtc(uint8_t value)
{
    // Write to the RTC register
    rtc = value & ~0x07;

    // Change the CS/SCK/SIO bits if writable and update the RTC
    bool cs  = (rtc & BIT(6)) ?  (value & BIT(2)) : csCur;
    bool sck = (rtc & BIT(5)) ? !(value & BIT(1)) : sckCur;
    bool sio = (rtc & BIT(4)) ?  (value & BIT(0)) : sioCur;
    updateRtc(cs, sck, sio);
}

void Rtc::writeGpData(uint16_t mask, uint16_t value)
{
    if (mask & 0xFF)
    {
        // Change the CS/SCK/SIO bits if writable and update the RTC
        bool cs  = (gpDirection & BIT(2)) ? (value & BIT(2)) : csCur;
        bool sio = (gpDirection & BIT(1)) ? (value & BIT(1)) : sioCur;
        bool sck = (gpDirection & BIT(0)) ? (value & BIT(0)) : sckCur;
        updateRtc(cs, sck, sio);
    }
}

void Rtc::writeGpDirection(uint16_t mask, uint16_t value)
{
    // Write to the GP_DIRECTION register
    mask &= 0x000F;
    gpDirection = (gpDirection & ~mask) | (value & mask);
}

void Rtc::writeGpControl(uint16_t mask, uint16_t value)
{
    // Only allow enabling register reads if an RTC was detected
    if (!gpRtc)
        return;

    // Write to the GP_CONTROL register
    mask &= 0x0001;
    gpControl = (gpControl & ~mask) | (value & mask);

    // Update the memory map to reflect the read status of the GP registers
    core->memory.updateMap7(0x8000000, 0x8001000);
}

uint8_t Rtc::readRtc()
{
    // Get the CS/SCK/SIO bits if readable and read from the RTC register
    bool cs  = csCur;
    bool sck = (rtc & BIT(5)) ? 0 : sckCur;
    bool sio = (rtc & BIT(4)) ? 0 : sioCur;
    return rtc | (cs << 2) | (sck << 1) | (sio << 0);
}

uint16_t Rtc::readGpData()
{
    // Get the CS/SCK/SIO bits if readable and read from the GP_DATA register
    bool cs  = (gpDirection & BIT(2)) ? 0 : csCur;
    bool sio = (gpDirection & BIT(1)) ? 0 : sioCur;
    bool sck = (gpDirection & BIT(0)) ? 0 : sckCur;
    return (cs << 2) | (sio << 1) | (sck << 0);
}