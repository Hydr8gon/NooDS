/*
    Copyright 2020 Hydr8gon

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

#ifndef WIFI_H
#define WIFI_H

#include <cstdint>

class Wifi
{
    public:
        uint16_t readWBbBusy();

        void writeWBbCnt(uint16_t mask, uint16_t value);

    private:
        int bbCount = 0;
};

#endif // WIFI_H
