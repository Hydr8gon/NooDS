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
    // Clear the scanline
    memset(&lineCache[(line % 48) * 256], 0, 256 * sizeof(uint16_t));

    // Draw the polygons
    for (int i = 0; i < gpu3D->getPolygonCount(); i++)
    {
        unsigned int type = gpu3D->getPolygons()[i].type;

        for (int j = 1; j <= 3 + (type & 1); j++)
        {
            // Determine the vertex order
            // Usually this is just clockwise, but quad strips are arranged in an "up-down" order
            int j1, j2;
            if (type != 3 || j % 2 == 1)
            {
                j1 = j - 1;
                j2 = j % (3 + (type & 1));
            }
            else if (j == 2)
            {
                j1 = 1;
                j2 = 3;
            }
            else if (j == 4)
            {
                j1 = 0;
                j2 = 2;
            }

            // Normalize the coordinates of a vertex
            Vertex vertex1 = gpu3D->getPolygons()[i].vertices[j1];
            if (vertex1.w == 0) continue;
            int x0 = (( vertex1.x * 128) / vertex1.w) + 128;
            int y0 = ((-vertex1.y * 96)  / vertex1.w) + 96;

            // Normalize the coordinates of the next vertex
            Vertex vertex2 = gpu3D->getPolygons()[i].vertices[j2];
            if (vertex2.w == 0) continue;
            int x1 = (( vertex2.x * 128) / vertex2.w) + 128;
            int y1 = ((-vertex2.y * 96)  / vertex2.w) + 96;

            // Draw a line between the vertices
            if (((int)line >= y0 && (int)line <= y1) || ((int)line >= y1 && (int)line <= y0))
            {
                int lx0, lx1;

                // Swap the coordinates if necessary to ensure the leftmost one comes first
                if (x0 > x1)
                {
                    int x = x0, y = y0;
                    x0 = x1; y0 = y1;
                    x1 = x;  y1 = y;
                }

                if (y0 == y1) // Horizontal line
                {
                    // The entire line exists on the current scanline
                    lx0 = x0;
                    lx1 = x1;
                }
                else
                {
                    // Calculate the length of the line segment that intersects with the current scanline
                    int slope = ((x1 - x0) << 12) / (y1 - y0);
                    lx0 = x0 + ((slope * ((int)line - y0)) >> 12);
                    lx1 = x0 + ((slope * ((int)line - y0 + ((y0 < y1) ? 1 : -1))) >> 12);
                    if (lx0 != lx1) lx1--;
                    if (lx1 > x1) lx1 = x1;
                }

                // Stay within the screen bounds
                if (lx0 <   0) lx0 =   0; else if (lx0 > 255) continue;
                if (lx1 > 255) lx1 = 255; else if (lx1 <   0) continue;

                // Draw a line segment
                for (int x = lx0; x <= lx1; x++)
                    lineCache[(line % 48) * 256 + x] = vertex1.color | BIT(15);
            }
        }
    }
}
