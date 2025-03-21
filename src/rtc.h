/*
    Copyright 2019-2025 Hydr8gon

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

#include "defines.h"

class Core;

class Rtc {
    public:
        Rtc(Core *core): core(core) {}
        void saveState(FILE *file);
        void loadState(FILE *file);

        void enableGpRtc() { gpRtc = true; }
        void reset();

        uint8_t readRtc();
        uint16_t readGpData();
        uint16_t readGpDirection() { return gpDirection; }
        uint16_t readGpControl() { return gpControl; }

        void writeRtc(uint8_t value);
        void writeGpData(uint16_t value, uint16_t mask);
        void writeGpDirection(uint16_t value, uint16_t mask);
        void writeGpControl(uint16_t value, uint16_t mask);

    private:
        Core *core;
        bool gpRtc = false;

        bool csCur = false;
        bool sckCur = false;
        bool sioCur = false;

        uint8_t writeCount = 0;
        uint8_t command = 0;
        uint8_t control = 0;
        uint8_t dateTime[7] = {};

        uint8_t rtc = 0;
        uint16_t gpDirection = 0;
        uint16_t gpControl = 0;

        void updateRtc(bool cs, bool sck, bool sio);
        void updateDateTime();

        bool readRegister(uint8_t index);
        void writeRegister(uint8_t index, bool value);
};

#endif // RTC_H
