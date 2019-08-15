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

#ifndef GPU_H
#define GPU_H

#include <cstdint>

namespace gpu
{

typedef struct
{
    uint16_t framebuffer[256 * 192];
    uint16_t bgBuffers[4][256 * 192];

    uint32_t *dispcnt;
    uint16_t *bgcnt[4];
    uint16_t *bghofs[4], *bgvofs[4];

    uint16_t *palette;
    uint16_t *oam;
    uint16_t **extPalettes;

    uint32_t bgVramAddr, objVramAddr;
} Engine;

extern Engine engineA, engineB;

extern uint16_t displayBuffer[256 * 192 * 2];
extern uint16_t fps;

void scanline256();
void scanline355();

}

#endif // GPU_H
