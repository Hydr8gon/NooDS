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

#include <cstdio>
#include <cstring>

#include "gpu_3d_renderer.h"
#include "defines.h"
#include "gpu_3d.h"

void Gpu3DRenderer::drawScanline(unsigned int line)
{
    // Clear the line
    memset(&lineCache[(line % 48) * 256], 0, 256 * sizeof(uint16_t));

    // Draw the polygon vertices
    for (int i = 0; i < gpu3D->getPolygonCount(); i++)
    {
        for (int j = 0; j < gpu3D->getPolygons()[i].size; j++)
        {
            Vertex vertex = gpu3D->getPolygons()[i].vertices[j];
            if (vertex.w != 0)
            {
                int x = (( vertex.x * 128) / vertex.w) + 128;
                int y = ((-vertex.y * 96)  / vertex.w) + 96;
                if (line == y && x >= 0 && x < 256)
                    lineCache[(line % 48) * 256 + x] = vertex.color | BIT(15);
            }
        }
    }
}
