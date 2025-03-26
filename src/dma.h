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

#pragma once

#include <cstdint>
#include <cstdio>

class Core;

class Dma {
public:
    Dma(Core *core, bool cpu): core(core), cpu(cpu) {}
    void saveState(FILE *file);
    void loadState(FILE *file);

    void transfer(int channel);
    void trigger(int mode, uint8_t channels = 0xF);

    uint32_t readDmaSad(int channel) { return dmaSad[channel]; }
    uint32_t readDmaDad(int channel) { return dmaDad[channel]; }
    uint32_t readDmaCnt(int channel);

    void writeDmaSad(int channel, uint32_t mask, uint32_t value);
    void writeDmaDad(int channel, uint32_t mask, uint32_t value);
    void writeDmaCnt(int channel, uint32_t mask, uint32_t value);

private:
    Core *core;
    bool cpu;

    uint32_t srcAddrs[4] = {};
    uint32_t dstAddrs[4] = {};
    uint32_t wordCounts[4] = {};

    uint32_t dmaSad[4] = {};
    uint32_t dmaDad[4] = {};
    uint32_t dmaCnt[4] = {};
};
