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

#include "wifi.h"
#include "defines.h"

void Wifi::writeWBbCnt(uint16_t mask, uint16_t value)
{
    // Start a fake BB transfer
    if (bbCount == 0)
        bbCount++;
}

uint16_t Wifi::readWBbBusy()
{
    // If a BB transfer was triggered, set the busy flag for an arbitrary amount of reads
    // This is a gross hack meant to stall WiFi initialization in the 4th gen Pokémon games
    // The games give an error message on the save select screen, preventing save loading
    // This allows enough time to load a save, and shouldn't break anything since it eventually clears
    // This is temporary; a proper fix will likely require a good chunk of the WiFi to be implemented
    if (bbCount > 0)
    {
        if (++bbCount == 3000)
            bbCount = 0;
        return 1;
    }
    else
    {
        return 0;
    }
}
