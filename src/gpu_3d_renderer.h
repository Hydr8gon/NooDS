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

#ifndef GPU_3D_RENDERER_H
#define GPU_3D_RENDERER_H

#include <cstdint>

#include "defines.h"

class Gpu3D;

class Gpu3DRenderer
{
    public:
        Gpu3DRenderer(Gpu3D *gpu3D): gpu3D(gpu3D) {};

        void drawScanline(unsigned int line);

        uint16_t *getLineCache() { return lineCache; }

    private:
        uint16_t lineCache[48 * 256] = {};

        Gpu3D *gpu3D;
};

#endif // GPU_3D_RENDERER_H
