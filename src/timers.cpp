/*
    Copyright 2019-2020 Hydr8gon

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

void Timers::tick(int cycles)
{
    bool countUp = false;

    for (int i = 0; i < 4; i++)
    {
        // Only tick enabled timers
        if (!(enabled & BIT(i)))
            continue;

        int amount;

        if (countUp)
        {
            // If count-up mode was triggered, tick once and disable the timer until the next trigger
            amount = 1;
            enabled &= ~BIT(i);
            countUp = false;
        }
        else
        {
            // Tick normally
            amount = cycles;
        }

        // Tick the timer
        timers[i] += amount;

        // Handle overflow
        if ((timers[i] & masks[i]) < amount)
        {
            // Reload the timer
            timers[i] += tmCntL[i] << shifts[i];

            // Trigger a timer overflow IRQ if enabled
            if (tmCntH[i] & BIT(6))
                core->interpreter[cpu].sendInterrupt(3 + i);

            // Trigger a GBA sound FIFO event
            if (core->isGbaMode() && i < 2)
                core->spu.gbaFifoTimer(i);

            // If the next timer has count-up timing enabled, let it tick now
            // In count-up timing mode, the timer only ticks when the previous timer overflows
            if (i < 3 && (tmCntH[i + 1] & BIT(2)))
            {
                enabled |= BIT(i + 1);
                countUp = true;
            }
        }
    }
}

void Timers::writeTmCntL(int timer, uint16_t mask, uint16_t value)
{
    // Write to one of the TMCNT_L registers
    // This value doesn't affect the current counter, and is instead used as the reload value
    tmCntL[timer] = (tmCntL[timer] & ~mask) | (value & mask);
}

void Timers::writeTmCntH(int timer, uint16_t mask, uint16_t value)
{
    // Update the timer shift if the prescaler setting was changed
    // The prescaler allows timers to tick at frequencies of f/1, f/64, f/256, or f/1024
    // The timers are implemented as fixed-point numbers, with the shift representing the fractional length
    if (mask & 0x0003)
    {
        int shift = ((value & 0x0003) ? (4 + (value & 0x0003) * 2) : 0);
        timers[timer] = (timers[timer] >> shifts[timer]) << shift;
        masks[timer] = (1 << (16 + shift)) - 1;
        shifts[timer] = shift;
    }

    // Reload the counter if the enable bit changes from 0 to 1
    if (!(tmCntH[timer] & BIT(7)) && (value & BIT(7)))
        timers[timer] = tmCntL[timer] << shifts[timer];

    // Write to one of the TMCNT_H registers
    mask &= 0x00C7;
    tmCntH[timer] = (tmCntH[timer] & ~mask) | (value & mask);

    // Set the timer enabled bit
    // A timer in count-up mode is disabled because its ticking is handled by the previous timer
    if ((tmCntH[timer] & BIT(7)) && (timer == 0 || !(tmCntH[timer] & BIT(2))))
        enabled |= BIT(timer);
    else
        enabled &= ~BIT(timer);
}

uint16_t Timers::readTmCntL(int timer)
{
    // Read the current timer value, shifted to remove the prescaler fraction
    return timers[timer] >> shifts[timer];
}
