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

#ifndef TIMERS_H
#define TIMERS_H

#include <cstdint>

class Core;

class Timers
{
    public:
        Timers(Core *core, bool cpu): core(core), cpu(cpu) {}

        void tick(int cycles);

        bool shouldTick() { return enabled; }

        uint16_t readTmCntH(int timer) { return tmCntH[timer]; }
        uint16_t readTmCntL(int timer);

        void writeTmCntL(int timer, uint16_t mask, uint16_t value);
        void writeTmCntH(int timer, uint16_t mask, uint16_t value);

    private:
        Core *core;
        bool cpu;

        uint8_t enabled = 0;

        uint32_t timers[4] = {};
        uint32_t masks[4] = {};
        int shifts[4] = {};

        uint16_t tmCntL[4] = {};
        uint16_t tmCntH[4] = {};
};

#endif // TIMERS_H
