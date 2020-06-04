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

#ifndef DEFINES_H
#define DEFINES_H

#include <cstdio>

// Disable print statements for non-debug builds
#ifndef DEBUG
#define printf(fmt, ...) (0)
#endif

// Macro to force inlining
#ifdef _MSC_VER
#define FORCE_INLINE __forceinline
#else
#define FORCE_INLINE inline __attribute__((always_inline))
#endif

// Simple bit macro
#define BIT(i) (1 << (i))

// Macros that read a value larger than 8 bits from an 8-bit array
#define U8TO16(data, index) ((data[(index) + 1] << 8) | data[index])
#define U8TO32(data, index) ((data[(index) + 3] << 24) | (data[(index) + 2] << 16) | (data[(index) + 1] << 8) | data[index])

#endif // DEFINES_H
