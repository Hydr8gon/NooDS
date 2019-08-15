/*
    Copyright 2019 Hydr8gon

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

#include "timer.h"
#include "core.h"

namespace timer
{

void tick(interpreter::Cpu *cpu, uint8_t timer)
{
    // Don't tick normally if count-up timing is enabled
    if (timer > 0 && (*cpu->tmcntH[timer] & BIT(2)))
        return;

    // Timers can tick at frequencies of f/1, f/64, f/256, or f/1024
    // For slower frequencies, increment the scaler value until it reaches the desired amount, and only then tick
    if ((*cpu->tmcntH[timer] & 0x0003) > 0)
    {
        cpu->timerScalers[timer]++;
        if (cpu->timerScalers[timer] >= 0x10 << ((*cpu->tmcntH[timer] & 0x0003) * 2))
            cpu->timerScalers[timer] = 0;
        else
            return;
    }

    // Increment and handle overflows
    (*cpu->tmcntL[timer])++;
    if (*cpu->tmcntL[timer] == 0) // Overflow
    {
        // Reload the timer
        *cpu->tmcntL[timer] = cpu->timerReloads[timer];
        if (*cpu->tmcntH[timer] & BIT(6)) // Timer overflow IRQ
            *cpu->irf |= BIT(3 + timer);

        // Count-up timing means the timer only ticks when the previous timer overflows
        // If the next timer has count-up timing enabled, now's the time to tick it
        if (timer < 3 && (*cpu->tmcntH[timer + 1] & BIT(2)))
        {
            (*cpu->tmcntL[timer + 1])++;
            if (*cpu->tmcntL[timer + 1] == 0) // Overflow
            {
                // Reload the timer
                *cpu->tmcntL[timer + 1] = cpu->timerReloads[timer + 1];
                if (*cpu->tmcntH[timer + 1] & BIT(6)) // Timer overflow IRQ
                    *cpu->irf |= BIT(3 + timer + 1);
            }
        }
    }
}

}
