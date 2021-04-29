/*
    Copyright 2019-2021 Hydr8gon

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

// I find GBATEK's RTC documentation to be lacking, so here's a quick summary of how the I/O works
//
// Bits 2 and 6 are connected to the CS pinout
// Bit 6 should always be set, so setting bit 2 to 1 or 0 causes CS to be high or low, respectively
//
// Bits 1 and 5 are connected to the SCK pinout
// Bit 5 should always be set, so setting bit 1 to 1 or 0 causes SCK to be high or low, respectively
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

void Rtc::writeRtc(uint8_t value)
{
    if (value & BIT(2)) // CS high
    {
        if ((rtc & BIT(1)) && !(value & BIT(1))) // SCK high to low
        {
            if (writeCount < 8)
            {
                // Write the first 8 bits to the command register, in reverse order
                command |= (value & BIT(0)) << (7 - writeCount);
                writeCount++;
            }
            else
            {
                if (!(value & BIT(4))) // Read
                {
                    value &= ~BIT(0);

                    // Read a bit from an RTC register
                    switch ((command & 0x0E) >> 1) // Register select
                    {
                        case 0: // Status register 1
                        {
                            value |= ((status1 >> (writeCount - 8)) & BIT(0));
                            break;
                        }

                        case 2: // Date and time
                        {
                            // Update the date and time
                            if (writeCount == 8)
                            {
                                // Get the local time
                                std::time_t t = std::time(nullptr);
                                std::tm *time = std::localtime(&t);
                                time->tm_year %= 100; // The DS only counts years 2000-2099
                                time->tm_mon++; // The DS starts month values at 1, not 0

                                // Convert to 12-hour mode if enabled
                                uint8_t hour = time->tm_hour;
                                if (!(status1 & BIT(1))) time->tm_hour %= 12;

                                // Save to the date and time registers in BCD format
                                // Index 3 contains the day of the week, but most things don't care
                                dateTime[0] = ((time->tm_year / 10) << 4) | (time->tm_year % 10);
                                dateTime[1] = ((time->tm_mon  / 10) << 4) | (time->tm_mon  % 10);
                                dateTime[2] = ((time->tm_mday / 10) << 4) | (time->tm_mday % 10);
                                dateTime[4] = ((time->tm_hour / 10) << 4) | (time->tm_hour % 10);
                                dateTime[5] = ((time->tm_min  / 10) << 4) | (time->tm_min  % 10);
                                dateTime[6] = ((time->tm_sec  / 10) << 4) | (time->tm_sec  % 10);

                                // Set the AM/PM bit
                                if (hour >= 12) dateTime[4] |= BIT(6);
                            }

                            value |= ((dateTime[(writeCount / 8) - 1] >> (writeCount % 8)) & BIT(0));
                            break;
                        }

                        case 3: // Time
                        {
                            // Update the time
                            if (writeCount == 8)
                            {
                                // Get the local time
                                std::time_t t = std::time(nullptr);
                                std::tm *time = std::localtime(&t);

                                // Convert to 12-hour mode if enabled
                                uint8_t hour = time->tm_hour;
                                if (!(status1 & BIT(1))) time->tm_hour %= 12;

                                // Save to the date and time registers in BCD format
                                dateTime[4] = ((time->tm_hour / 10) << 4) | (time->tm_hour % 10);
                                dateTime[5] = ((time->tm_min  / 10) << 4) | (time->tm_min  % 10);
                                dateTime[6] = ((time->tm_sec  / 10) << 4) | (time->tm_sec  % 10);

                                // Set the AM/PM bit
                                if (hour >= 12) dateTime[4] |= BIT(6);
                            }

                            value |= ((dateTime[(writeCount / 8) - 5] >> (writeCount % 8)) & BIT(0));
                            break;
                        }

                        default:
                        {
                            LOG("Read from unknown RTC registers: %d\n", (command & 0x0E) >> 1);
                            break;
                        }
                    }
                }
                else // Write
                {
                    // Write a bit to an RTC register
                    switch ((command & 0x0E) >> 1) // Register select
                    {
                        case 0: // Status register 1
                        {
                            // Only write to the writable bits 1 through 3
                            if (writeCount - 8 >= 1 && writeCount - 8 <= 3)
                                status1 = (status1 & ~BIT(writeCount - 8)) | ((value & BIT(0)) << (writeCount - 8));
                            break;
                        }

                        default:
                        {
                            LOG("Write to unknown RTC registers: %d\n", (command & 0x0E) >> 1);
                            break;
                        }
                    }
                }

                writeCount++;
            }
        }
        else if (!(rtc & BIT(1)) && (value & BIT(1)) && !(value & BIT(4))) // SCK low to high, read
        {
            // Read bits can still be read after switching SCK to high, so keep the previous bit value
            value = (value & ~BIT(0)) | (rtc & BIT(0));
        }
    }
    else // CS low
    {
        // Reset the transfer
        writeCount = 0;
        command = 0;
    }

    rtc = value;
}
