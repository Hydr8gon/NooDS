/*
    Copyright 2019-2026 Hydr8gon

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

#pragma once

#include <cstdint>

class Core;

class I2c {
public:
    I2c(Core *core): core(core) {}
    void saveState(FILE *file);
    void loadState(FILE *file);

    uint8_t readData() { return i2cData; }
    uint8_t readCnt() { return i2cCnt; }

    void writeData(uint8_t value);
    void writeCnt(uint8_t value);

private:
    Core *core;

    uint32_t writeCount = 0;
    uint8_t devAddr = 0;
    uint8_t regAddr = 0;

    uint8_t i2cData = 0;
    uint8_t i2cCnt = 0;

    uint8_t readMcu();
    void writeMcu(uint8_t value);
};
