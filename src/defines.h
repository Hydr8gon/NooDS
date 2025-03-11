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

#ifndef DEFINES_H
#define DEFINES_H

#include <cstdio>

// Control how many cycles of 3D GPU commands are batched
#define GPU3D_BATCH 32

// Enable or disable debug log printing
#ifdef DEBUG
#define LOG(...) printf(__VA_ARGS__)
#else
#define LOG(...) (0)
#endif

// Compatibility toggle for systems that don't have fdopen
#ifdef NO_FDOPEN
#define fdopen(...) (0)
#define ftruncate(...) (0)
#else
#include <unistd.h>
#endif

// Macro to handle differing mkdir arguments on Windows
#ifdef WINDOWS
#define MKDIR_ARGS
#else
#define MKDIR_ARGS , 0777
#endif

// Macro to force inlining
#ifdef _MSC_VER
#define FORCE_INLINE __forceinline
#else
#define FORCE_INLINE inline __attribute__((always_inline))
#endif

// Simple bit macro
#define BIT(i) (1 << (i))

// Macro to swap two values
#define SWAP(a, b) \
{ \
    auto c = a; \
    a = b; \
    b = c; \
}

// Macros that read a value larger than 8 bits from an 8-bit array
#define U8TO16(data, index) ((data)[index] | ((data)[(index) + 1] << 8))
#define U8TO32(data, index) ((data)[index] | ((data)[(index) + 1] << 8) | \
    ((data)[(index) + 2] << 16) | ((data)[(index) + 3] << 24))
#define U8TO64(data, index) ((uint64_t)U8TO32(data, (index) + 4) << 32) | (uint32_t)U8TO32(data, index)

// Macro that stores a 32-bit value to an 8-bit array
#define U32TO8(data, index, value) \
    (data)[(index) + 0] = (uint8_t)((value) >>  0); \
    (data)[(index) + 1] = (uint8_t)((value) >>  8); \
    (data)[(index) + 2] = (uint8_t)((value) >> 16); \
    (data)[(index) + 3] = (uint8_t)((value) >> 24);

#endif // DEFINES_H
