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
#include <cstdlib>
#include <cstring>

#include "gpu_3d_renderer.h"
#include "defines.h"
#include "gpu_3d.h"

void Gpu3DRenderer::drawScanline(int line)
{
    // Clear the scanline
    memset(&lineCache[(line % 48) * 256], 0, 256 * sizeof(uint16_t));

    // "Empty" the Z-buffer by setting all values to maximum
    for (int i = 0; i < 256; i++)
        zBuffer[i] = 0x7FFFFFFF;

    // Draw the polygons
    for (int i = 0; i < gpu3D->getPolygonCount(); i++)
    {
        _Polygon polygon = gpu3D->getPolygons()[i];

        // Get the triangle vertices
        Vertex v1 = normalize(polygon.vertices[0]);
        Vertex v2 = normalize(polygon.vertices[1]);
        Vertex v3 = normalize(polygon.vertices[2]);

        if (polygon.type & 1) // Quad
        {
            // Get the quad's fourth vertex
            Vertex v4 = normalize(polygon.vertices[3]);

            // Sort the vertices in order of increasing Y values
            if (v2.y < v1.y)
            {
                Vertex v = v1;
                v1 = v2;
                v2 = v;
            }
            if (v3.y < v1.y)
            {
                Vertex v = v1;
                v1 = v3;
                v3 = v;
            }
            if (v4.y < v1.y)
            {
                Vertex v = v1;
                v1 = v4;
                v4 = v;
            }
            if (v3.y < v2.y)
            {
                Vertex v = v2;
                v2 = v3;
                v3 = v;
            }
            if (v4.y < v2.y)
            {
                Vertex v = v2;
                v2 = v4;
                v4 = v;
            }
            if (v4.y < v3.y)
            {
                Vertex v = v3;
                v3 = v4;
                v4 = v;
            }

            // Ensure the quad intersects with the current scanline
            if (line < v1.y || line >= v4.y) continue;

            // Calculate the Z value of the cross products
            // These determine the posistions of V2 and V3 relative to the line between V1 and V4
            int cross2 = (v2.x - v1.x) * (v4.y - v1.y) - (v2.y - v1.y) * (v4.x - v1.x);
            int cross3 = (v3.x - v1.x) * (v4.y - v1.y) - (v3.y - v1.y) * (v4.x - v1.x);

            // Rasterize the quad
            if (cross2 > 0 && cross3 <= 0) // V2 is on the right, V3 is on the left
            {
                if (line < v2.y) // Above V2
                    rasterize(line, &polygon, &v1, &v3, &v1, &v2);
                else if (line < v3.y) // Above V3
                    rasterize(line, &polygon, &v1, &v3, &v2, &v4);
                else // Below V3
                    rasterize(line, &polygon, &v3, &v4, &v2, &v4);
            }
            else if (cross2 <= 0 && cross3 > 0) // V2 is on the left, V3 is on the right
            {
                if (line < v2.y) // Above V2
                    rasterize(line, &polygon, &v1, &v2, &v1, &v3);
                else if (line < v3.y) // Above V3
                    rasterize(line, &polygon, &v2, &v4, &v1, &v3);
                else // Below V3
                    rasterize(line, &polygon, &v2, &v4, &v3, &v4);
            }
            else if (cross2 > 0 && cross3 > 0) // V2 and V3 are on the right
            {
                if (line < v2.y) // Above V2
                    rasterize(line, &polygon, &v1, &v4, &v1, &v2);
                else if (line < v3.y) // Above V3
                    rasterize(line, &polygon, &v1, &v4, &v2, &v3);
                else // Below V3
                    rasterize(line, &polygon, &v1, &v4, &v3, &v4);
            }
            else if (cross2 <= 0 && cross3 <= 0) // V2 and V3 are on the left
            {
                if (line < v2.y) // Above V2
                    rasterize(line, &polygon, &v1, &v2, &v1, &v4);
                else if (line < v3.y) // Above V3
                    rasterize(line, &polygon, &v2, &v3, &v1, &v4);
                else // Below V3
                    rasterize(line, &polygon, &v3, &v4, &v1, &v4);
            }
        }
        else // Triangle
        {
            // Sort the vertices in order of increasing Y values
            if (v2.y < v1.y)
            {
                Vertex v = v1;
                v1 = v2;
                v2 = v;
            }
            if (v3.y < v1.y)
            {
                Vertex v = v1;
                v1 = v3;
                v3 = v;
            }
            if (v3.y < v2.y)
            {
                Vertex v = v2;
                v2 = v3;
                v3 = v;
            }

            // Ensure the triangle intersects with the current scanline
            if (line < v1.y || line > v3.y) continue;

            // Calculate the Z value of the cross product
            // This determines the posistion of V2 relative to the line between V1 and V3
            int cross2 = (v2.x - v1.x) * (v3.y - v1.y) - (v2.y - v1.y) * (v3.x - v1.x);

            // Rasterize the triangle
            if (cross2 > 0) // V2 is on the right
            {
                if (line < v2.y) // Above V2
                    rasterize(line, &polygon, &v1, &v3, &v1, &v2);
                else // Below V2
                    rasterize(line, &polygon, &v1, &v3, &v2, &v3);
            }
            else // V2 is on the left
            {
                if (line < v2.y) // Above V2
                    rasterize(line, &polygon, &v1, &v2, &v1, &v3);
                else // Below V2
                    rasterize(line, &polygon, &v2, &v3, &v1, &v3);
            }
        }
    }
}

Vertex Gpu3DRenderer::normalize(Vertex vertex)
{
    // Normalize a vertex's coordinates and convert them to DS screen coordinates
    if (vertex.w != 0)
    {
        if (vertex.w < 0)
        {
            vertex.w = -vertex.w;
            vertex.z = -vertex.z;
        }

        vertex.x = (( vertex.x * 128) / vertex.w) + 128;
        vertex.y = ((-vertex.y *  96) / vertex.w) +  96;
    }

    return vertex;
}

int Gpu3DRenderer::interpolate(int min, int max, int start, int current, int end)
{
    // Calculate the gradient
    // This is the percentage distance between the start and end positions
    float gradient = (start == end) ? 0 : ((float)(current - start) / (end - start));

    // Keep the gradient within bounds
    if (gradient > 1)
        gradient = 1;
    else if (gradient < 0)
        gradient = 0;

    // Calculate a new value between the min and max values
    return min + gradient * (max - min);
}

uint16_t Gpu3DRenderer::interpolateColor(uint16_t min, uint16_t max, int start, int current, int end)
{
    // Apply interpolation separately on the RGB values
    int r = interpolate((min >>  0) & 0x1F, (max >>  0) & 0x1F, start, current, end);
    int g = interpolate((min >>  5) & 0x1F, (max >>  5) & 0x1F, start, current, end);
    int b = interpolate((min >> 10) & 0x1F, (max >> 10) & 0x1F, start, current, end);
    return (b << 10) | (g << 5) | r | BIT(15);
}

uint16_t Gpu3DRenderer::readTexture(_Polygon *polygon, int s, int t)
{
    s %= polygon->sizeS;
    t %= polygon->sizeT;

    switch (polygon->texFormat)
    {
        case 3: // 16-color palette
        {
            uint8_t index = polygon->texData[(t * polygon->sizeS + s) / 2];
            if (s % 2 == 0) index &= 0x0F; else index >>= 4;
            if (polygon->transparent && index == 0) return 0; else
            return U8TO16(polygon->texPalette, index * 2) | BIT(15);
        }

        case 4: // 256-color palette
        {
            uint8_t index = polygon->texData[t * polygon->sizeS + s];
            if (polygon->transparent && index == 0) return 0; else
            return U8TO16(polygon->texPalette, index * 2) | BIT(15);
        }

        default:
            printf("Unknown texture format: %d\n", polygon->texFormat);
            return 0xFFFF;
    }
}

void Gpu3DRenderer::rasterize(int line, _Polygon *polygon, Vertex *v1, Vertex *v2, Vertex *v3, Vertex *v4)
{
    // Calculate the X bounds between the line between V1 and V2 and the line between V3 and V4
    int lx0 = interpolate(v1->x, v2->x, v1->y, line, v2->y);
    int lx1 = interpolate(v3->x, v4->x, v3->y, line, v4->y);

    // Draw a line segment
    for (int x = ((lx0 < 0) ? 0 : lx0); x < ((lx1 > 256) ? 256 : lx1); x++)
    {
        // Calculate the Z value of the current pixel
        int z1 = interpolate(v1->z, v2->z, v1->y, line, v2->y);
        int z2 = interpolate(v3->z, v4->z, v3->y, line, v4->y);
        int z  = interpolate(z1, z2, lx0, x, lx1);

        // Calculate the W value of the current pixel
        int w1 = interpolate(v1->w, v2->w, v1->y, line, v2->y);
        int w2 = interpolate(v3->w, v4->w, v3->y, line, v4->y);
        int w  = interpolate(w1, w2, lx0, x, lx1);

        // Draw a new pixel if the old one is behind the new one and the Z value is in range
        if (zBuffer[x] >= z && abs(z) <= abs(w))
        {
            uint16_t color;

            // Calculate the pixel color
            if (polygon->texFormat == 0) // No texture
            {
                // Interpolate the vertex colors
                uint16_t color1 = interpolateColor(v1->color, v2->color, v1->y, line, v2->y);
                uint16_t color2 = interpolateColor(v3->color, v4->color, v3->y, line, v4->y);
                color = interpolateColor(color1, color2, lx0, x, lx1);
            }
            else
            {
                // Interpolate the texture S coordinate
                int s1 = interpolate(v1->s, v2->s, v1->y, line, v2->y);
                int s2 = interpolate(v3->s, v4->s, v3->y, line, v4->y);
                int s  = interpolate(s1, s2, lx0, x, lx1);

                // Interpolate the texture T coordinate
                int t1 = interpolate(v1->t, v2->t, v1->y, line, v2->y);
                int t2 = interpolate(v3->t, v4->t, v3->y, line, v4->y);
                int t  = interpolate(t1, t2, lx0, x, lx1);

                // Read the color from a texture
                color = readTexture(polygon, s >> 4, t >> 4);
            }

            // Draw a pixel
            if (color & BIT(15))
            {
                lineCache[(line % 48) * 256 + x] = color;
                zBuffer[x] = z;
            }
        }
    }
}
