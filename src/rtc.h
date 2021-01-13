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

#ifndef RTC_H
#define RTC_H

#include <cstdint>

class Core;

class Rtc
{
    public:
        Rtc(Core *core): core(core) {}

        uint8_t readRtc() { return rtc; }

        void writeRtc(uint8_t value);

    private:
        Core *core;

        uint8_t writeCount = 0;
        uint8_t command = 0;
        uint8_t status1 = 0;
        uint8_t dateTime[7] = {};

        uint8_t rtc = 0;
};

#endif // RTC_H
