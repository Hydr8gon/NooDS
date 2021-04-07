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
#include <functional>

class Core;

class Timers
{
    public:
        Timers(Core *core, bool cpu);

        void resetCycles();

        uint16_t readTmCntH(int timer) { return tmCntH[timer]; }
        uint16_t readTmCntL(int timer);

        void writeTmCntL(int timer, uint16_t mask, uint16_t value);
        void writeTmCntH(int timer, uint16_t mask, uint16_t value);

    private:
        Core *core;
        bool cpu;

        uint16_t timers[4] = {};
        uint8_t shifts[4] = {};
        uint32_t endCycles[4] = {};

        uint16_t tmCntL[4] = {};
        uint16_t tmCntH[4] = {};

        std::function<void()> overflowTask[4];

        void overflow(int timer);
};

#endif // TIMERS_H
