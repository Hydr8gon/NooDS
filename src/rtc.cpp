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
#include <ctime>

#include "rtc.h"
#include "core.h"

namespace rtc
{

uint64_t dateTime;
uint8_t writeCount;
uint8_t command;
uint8_t status1;
uint8_t lastValue;

void write(uint8_t *value)
{
    // I find GBATEK's RTC documentation confusing, so here's a quick summary of how it works
    //
    // Bits 2 and 6 are connected to the CS pinout
    // Bit 6 should always be set, so bit 2 being 1 or 0 causes CS to be high or low, respectively
    //
    // Bits 1 and 5 are connected to the SCK pinout
    // Bit 5 should always be set, so bit 1 being 1 or 0 causes SCK to be high or low, respectively
    //
    // Bits 0 and 4 are connected to the SIO pinout
    // Bit 4 indicates data direction; 0 is read, and 1 is write
    // Bit 0 is where data sent from the RTC is read, and where data sent to the RTC is written
    //
    // To start a transfer, switch CS from low to high
    // To end a transfer, switch CS from high to low
    //
    // To transfer a bit, set SCK to low and then high (it should be high when the transfer starts)
    // When writing a bit to the RTC, you should set bit 0 at the same time as setting SCK to low
    // When reading a bit from the RTC, you should read bit 0 after setting SCK to low (or high?)
    //
    // Start by writing the 8 bits of the command register, LSB first
    // After that, read or write the registers selected with the command register, MSB first
    // RTC registers at least are documented nicely on GBATEK

    if (*value & BIT(2)) // CS high
    {
        if ((lastValue & BIT(1)) && !(*value & BIT(1))) // SCK high to low
        {
            if (writeCount < 8)
            {
                // Write the first 8 bits to the command register, reversing the bit order
                command |= (*value & BIT(0)) << (7 - writeCount);
                writeCount++;
            }
            else
            {
                uint8_t regSelect = (command & 0x0E) >> 1;

                // Transfer all following bits to or from the registers selected by the command register
                if (*value & BIT(4)) // Write
                {
                    switch (regSelect)
                    {
                        case 0: // Status register 1
                        {
                            // Only write to the writeable bits 1 through 3
                            if (writeCount - 8 >= 1 && writeCount - 8 <= 3)
                            {
                                status1 &= ~BIT(writeCount - 8);
                                status1 |= (*value & BIT(0)) << (writeCount - 8);
                            }
                            break;
                        }

                        default:
                        {
                            printf("Unhandled write to RTC registers: %d\n", regSelect);
                            break;
                        }
                    }
                }
                else // Read
                {
                    *value &= ~BIT(0);

                    switch (regSelect)
                    {
                        case 0: // Status register 1
                        {
                            *value |= ((status1 >> (writeCount - 8)) & BIT(0));
                            break;
                        }

                        case 2: // Date and time
                        {
                            // Update the time at the start of the read
                            if (writeCount == 8)
                            {
                                // Get the local time, and adjust its values to better match the DS formatting
                                std::time_t t = std::time(nullptr);
                                std::tm *time = std::localtime(&t);
                                time->tm_year %= 100;
                                time->tm_mon++;

                                // Convert to 12-hour mode if set
                                uint8_t hour = time->tm_hour;
                                if (!(status1 & BIT(1)))
                                    time->tm_hour %= 12;

                                // Save to the date and time registers in BCD format
                                // The bytes here (from low to high) represent:
                                // Year, month, day, day of week, hours, minutes, seconds
                                ((uint8_t*)&dateTime)[0] = ((time->tm_year / 10) << 4) | (time->tm_year % 10);
                                ((uint8_t*)&dateTime)[1] = ((time->tm_mon  / 10) << 4) | (time->tm_mon  % 10);
                                ((uint8_t*)&dateTime)[2] = ((time->tm_mday / 10) << 4) | (time->tm_mday % 10);
                                ((uint8_t*)&dateTime)[4] = ((time->tm_hour / 10) << 4) | (time->tm_hour % 10);
                                ((uint8_t*)&dateTime)[5] = ((time->tm_min  / 10) << 4) | (time->tm_min  % 10);
                                ((uint8_t*)&dateTime)[6] = ((time->tm_sec  / 10) << 4) | (time->tm_sec  % 10);

                                // Set the AM/PM bit
                                if (hour >= 12)
                                    ((uint8_t*)&dateTime)[4] |= BIT(6);
                            }

                            *value |= ((dateTime >> (writeCount - 8)) & BIT(0));
                            break;
                        }

                        case 3: // Time
                        {
                            // Update the time at the start of the read
                            if (writeCount == 8)
                            {
                                // Get the local time
                                std::time_t t = std::time(nullptr);
                                std::tm *time = std::localtime(&t);

                                // Convert to 12-hour mode if set
                                uint8_t hour = time->tm_hour;
                                if (!(status1 & BIT(1)))
                                    time->tm_hour %= 12;

                                // Save to the date and time registers in BCD format
                                // The bytes here (from low to high) represent:
                                // Hours, minutes, seconds
                                ((uint8_t*)&dateTime)[4] = ((time->tm_hour / 10) << 4) | (time->tm_hour % 10);
                                ((uint8_t*)&dateTime)[5] = ((time->tm_min  / 10) << 4) | (time->tm_min  % 10);
                                ((uint8_t*)&dateTime)[6] = ((time->tm_sec  / 10) << 4) | (time->tm_sec  % 10);

                                // Set the AM/PM bit
                                if (hour >= 12)
                                    ((uint8_t*)&dateTime)[4] |= BIT(6);
                            }

                            *value |= ((dateTime >> (writeCount + 24)) & BIT(0));
                            break;
                        }

                        default:
                        {
                            printf("Unhandled read from RTC registers: %d\n", regSelect);
                            break;
                        }
                    }
                }

                writeCount++;
            }
        }
        else if (!(lastValue & BIT(1)) && (*value & BIT(1)) && !(*value & BIT(4))) // SCK low to high, read
        {
            // Read bits can still be read after switching SCK to high, so keep the previous bit value
            *value &= ~BIT(0);
            *value |= lastValue & BIT(0);
        }
    }
    else // CS low
    {
        // Reset the transfer
        writeCount = 0;
        command = 0;
    }

    // Save the current RTC register so it can be compared to the next write
    lastValue = *value;
}

void init()
{
    writeCount = 0;
    command = 0;
    status1 = 0;
    lastValue = 0;
}

}
