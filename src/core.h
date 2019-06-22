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

#ifndef CORE_H
#define CORE_H

#include "interpreter.h"

#define BIT(i) (1 << i)

namespace core
{

extern interpreter::Cpu arm9, arm7;

bool loadRom(char *filename);

void runScanline();
void pressKey(uint8_t key);
void releaseKey(uint8_t key);

}

#endif // CORE_H
