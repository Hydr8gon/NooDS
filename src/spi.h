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

#ifndef SPI_H
#define SPI_H

#include <cstdint>

class Core;

class Spi
{
    public:
        Spi(Core *core): core(core) {}

        void loadFirmware();
        void directBoot();

        void setTouch(int x, int y);
        void clearTouch();

        uint16_t readSpiCnt()  { return spiCnt;  }
        uint8_t  readSpiData() { return spiData; }

        void writeSpiCnt(uint16_t mask, uint16_t value);
        void writeSpiData(uint8_t value);

    private:
        Core *core;

        uint8_t firmware[0x40000] = {};

        unsigned int writeCount = 0;
        uint32_t address = 0;
        uint8_t command = 0;

        uint16_t touchX = 0x000, touchY = 0xFFF;
        uint16_t spiCnt = 0;
        uint8_t spiData = 0;
};

#endif // SPI_H
