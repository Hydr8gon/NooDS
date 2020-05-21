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
#include "defines.h"
#include "interpreter.h"

void Timers::tick(bool twice)
{
    bool countUp = false;

    for (int i = 0; i < 4; i++)
    {
        // Only tick enabled timers
        if (!(enabled & BIT(i)))
            continue;

        int amount;

        if ((tmCntH[i] & 0x0003) > 0)
        {
            // Timers can tick at frequencies of f/1, f/64, f/256, or f/1024
            // For slower frequencies, increment the scaler value until it reaches the desired amount, and only then tick
            scalers[i] += 1 + twice;
            if (scalers[i] == (0x10 << ((tmCntH[i] & 0x0003) * 2)))
            {
                amount = 1;
                scalers[i] = 0;
            }
            else
            {
                continue;
            }
        }
        else if (countUp)
        {
            // Always tick once in count-up mode
            amount = 1;
            enabled &= ~BIT(i);
            countUp = false;
        }
        else
        {
            // Tick normally
            amount = 1 + twice;
        }

        // Tick the timer
        tmCntL[i] += amount;

        // Handle overflow
        if (tmCntL[i] < amount)
        {
            // Reload the timer
            tmCntL[i] += reloads[i];

            // Trigger a timer overflow IRQ if enabled
            if (tmCntH[i] & BIT(6))
                cpu->sendInterrupt(3 + i);

            // In count-up timing mode, the timer only ticks when the previous timer overflows
            // If the next timer has count-up timing enabled, let it tick now
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
    // The value gets redirected and used as the reload value
    reloads[timer] = (reloads[timer] & ~mask) | (value & mask);
}

void Timers::writeTmCntH(int timer, uint16_t mask, uint16_t value)
{
    // Reload the counter if the enable bit changes from 0 to 1
    if (!(tmCntH[timer] & BIT(7)) && (value & BIT(7)))
    {
        tmCntL[timer] = reloads[timer];
        scalers[timer] = 0;
    }

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
