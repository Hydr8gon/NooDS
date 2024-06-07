/*
    Copyright 2019-2024 Hydr8gon

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

#include "timers.h"
#include "core.h"

void Timers::saveState(FILE *file)
{
    // Write state data to the file
    fwrite(timers, 2, sizeof(timers) / 2, file);
    fwrite(shifts, 1, sizeof(shifts), file);
    fwrite(endCycles, 4, sizeof(endCycles) / 4, file);
    fwrite(tmCntL, 2, sizeof(tmCntL) / 2, file);
    fwrite(tmCntH, 2, sizeof(tmCntH) / 2, file);
}

void Timers::loadState(FILE *file)
{
    // Read state data from the file
    fread(timers, 2, sizeof(timers) / 2, file);
    fread(shifts, 1, sizeof(shifts), file);
    fread(endCycles, 4, sizeof(endCycles) / 4, file);
    fread(tmCntL, 2, sizeof(tmCntL) / 2, file);
    fread(tmCntH, 2, sizeof(tmCntH) / 2, file);
}

void Timers::resetCycles()
{
    // Adjust timer end cycles for a global cycle reset
    for (int i = 0; i < 4; i++)
        endCycles[i] -= core->globalCycles;
}

void Timers::overflow(int timer)
{
    // Ensure the timer is enabled and the end cycle is correct if not in count-up mode
    // The end cycle check ensures that if a timer was changed while running, outdated events are ignored
    if (!(tmCntH[timer] & BIT(7)) || ((timer == 0 || !(tmCntH[timer] & BIT(2)))
        && endCycles[timer] != core->globalCycles)) return;

    // Reload the timer and schedule another overflow if not in count-up mode
    timers[timer] = tmCntL[timer];
    if (timer == 0 || !(tmCntH[timer] & BIT(2)))
    {
        core->schedule(SchedTask(TIMER9_OVERFLOW0 + (arm7 << 2) + timer), (0x10000 - timers[timer]) << shifts[timer]);
        endCycles[timer] = core->globalCycles + ((0x10000 - timers[timer]) << shifts[timer]);
    }

    // Trigger a timer overflow IRQ if enabled
    if (tmCntH[timer] & BIT(6))
        core->interpreter[arm7].sendInterrupt(3 + timer);

    // Trigger a GBA sound FIFO event
    if (core->gbaMode && timer < 2)
        core->spu.gbaFifoTimer(timer);

    // If the next timer has count-up timing enabled, tick it now
    // In count-up timing mode, the timer only ticks when the previous timer overflows
    if (timer < 3 && (tmCntH[timer + 1] & BIT(2)) && ++timers[timer + 1] == 0)
        overflow(timer + 1);
}

void Timers::writeTmCntL(int timer, uint16_t mask, uint16_t value)
{
    // Write to one of the TMCNT_L registers
    // This value doesn't affect the current counter, and is instead used as the reload value
    tmCntL[timer] = (tmCntL[timer] & ~mask) | (value & mask);
}

void Timers::writeTmCntH(int timer, uint16_t mask, uint16_t value)
{
    // Update the current timer value if it's running on the scheduler
    bool dirty = false;
    if ((tmCntH[timer] & BIT(7)) && (timer == 0 || !(value & BIT(2))))
        timers[timer] = 0x10000 - ((endCycles[timer] - core->globalCycles) >> shifts[timer]);

    // Update the timer shift if the prescaler setting was changed
    // The prescaler allows timers to tick at frequencies of f/1, f/64, f/256, or f/1024 (when not in count-up mode)
    if (mask & 0xFF)
    {
        uint8_t shift = (((value & 0x3) && (timer == 0 || !(value & BIT(2)))) ? (4 + (value & 0x3) * 2) : 0);
        if (shifts[timer] != shift)
        {
            shifts[timer] = shift;
            dirty = true;
        }
    }

    // Reload the counter if the enable bit changes from 0 to 1
    if (!(tmCntH[timer] & BIT(7)) && (value & BIT(7)))
    {
        timers[timer] = tmCntL[timer];
        dirty = true;
    }

    // Write to one of the TMCNT_H registers
    mask &= 0x00C7;
    tmCntH[timer] = (tmCntH[timer] & ~mask) | (value & mask);

    // Schedule a timer overflow if the timer changed and isn't in count-up mode
    if (dirty && (tmCntH[timer] & BIT(7)) && (timer == 0 || !(tmCntH[timer] & BIT(2))))
    {
        core->schedule(SchedTask(TIMER9_OVERFLOW0 + (arm7 << 2) + timer), (0x10000 - timers[timer]) << shifts[timer]);
        endCycles[timer] = core->globalCycles + ((0x10000 - timers[timer]) << shifts[timer]);
    }
}

uint16_t Timers::readTmCntL(int timer)
{
    // Read the current timer value, updating it if it's running on the scheduler
    if ((tmCntH[timer] & BIT(7)) && (timer == 0 || !(tmCntH[timer] & BIT(2))))
        timers[timer] = 0x10000 - ((endCycles[timer] - core->globalCycles) >> shifts[timer]);
    return timers[timer];
}
