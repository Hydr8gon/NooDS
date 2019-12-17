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

uint32_t Gpu3DRenderer::rgb5ToRgb6(uint32_t color)
{
    // Convert an RGB5 value to an RGB6 value (the way the 3D engine does it)
    uint8_t r = ((color >>  0) & 0x1F); r = r * 2 + (r + 31) / 32;
    uint8_t g = ((color >>  5) & 0x1F); g = g * 2 + (g + 31) / 32;
    uint8_t b = ((color >> 10) & 0x1F); b = b * 2 + (b + 31) / 32;
    uint8_t a = ((color >> 15) & 0x01);
    return (a << 18) | (b << 12) | (g << 6) | r;
}

void Gpu3DRenderer::drawScanline(int line)
{
    // Clear the scanline
    memset(&lineCache[(line % 48) * 256], 0, 256 * sizeof(uint32_t));

    // "Empty" the depth buffer by setting all values to maximum
    for (int i = 0; i < 256; i++)
        depthBuffer[i] = 0x7FFFFFFF;

    // Draw the polygons
    for (int i = 0; i < gpu3D->getPolygonCount(); i++)
    {
        _Polygon *polygon = &gpu3D->getPolygons()[i];

        // Get the polygon vertices
        Vertex vertices[polygon->size];
        for (int j = 0; j < polygon->size; j++)
            vertices[j] = polygon->vertices[j];

        // Sort the vertices in order of increasing Y values
        for (int j = 0; j < polygon->size - 1; j++)
        {
            for (int k = j + 1; k < polygon->size; k++)
            {
                if (vertices[k].y < vertices[j].y)
                {
                    Vertex vertex = vertices[j];
                    vertices[j] = vertices[k];
                    vertices[k] = vertex;
                }
            }
        }

        // Ensure the polygon intersects with the current scanline
        if (line < vertices[0].y || line >= vertices[polygon->size - 1].y)
            continue;

        // Calculate the cross products of the middle vertices
        // These determine whether a vertex is on the left or right of the middle of its polygon
        int crosses[polygon->size - 2];
        for (int j = 0; j < polygon->size - 2; j++)
        {
            crosses[j] = (vertices[j + 1].x - vertices[0].x) * (vertices[polygon->size - 1].y - vertices[0].y) -
                         (vertices[j + 1].y - vertices[0].y) * (vertices[polygon->size - 1].x - vertices[0].x);
        }

        // Rasterize the polygon
        for (int j = 1; j < polygon->size; j++)
        {
            if (line < vertices[j].y)
            {
                // The highest point equal or below j on the left
                int v1;
                for (v1 = j; v1 < polygon->size; v1++)
                    if (v1 == polygon->size - 1 || crosses[v1 - 1] <= 0) break;

                // The lowest point above v1 on the left
                int v0;
                for (v0 = v1 - 1; v0 >= 0; v0--)
                    if (v0 == 0 || crosses[v0 - 1] <= 0) break;

                // The highest point equal or below j on the right
                int v3;
                for (v3 = j; v3 < polygon->size; v3++)
                    if (v3 == polygon->size - 1 || crosses[v3 - 1] > 0) break;

                // The lowest point above v3 on the right
                int v2;
                for (v2 = v3 - 1; v2 >= 0; v2--)
                    if (v2 == 0 || crosses[v2 - 1] > 0) break;

                rasterize(line, polygon, &vertices[v0], &vertices[v1], &vertices[v2], &vertices[v3]);
                break;
            }
        }
    }
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

uint32_t Gpu3DRenderer::interpolateColor(uint32_t min, uint32_t max, int start, int current, int end)
{
    // Apply interpolation separately on the RGB values
    int r = interpolate((min >>  0) & 0x3F, (max >>  0) & 0x3F, start, current, end);
    int g = interpolate((min >>  6) & 0x3F, (max >>  6) & 0x3F, start, current, end);
    int b = interpolate((min >> 12) & 0x3F, (max >> 12) & 0x3F, start, current, end);
    return BIT(18) | (b << 12) | (g << 6) | r;
}

uint32_t Gpu3DRenderer::readTexture(_Polygon *polygon, int s, int t)
{
    // Handle S-coordinate overflows
    if (polygon->repeatS)
    {
        // Flip the S-coordinate every second repeat
        if (polygon->flipS && (s / polygon->sizeS) % 2 != 0)
            s = polygon->sizeS - 1 - s;

        // Wrap the S-coordinate
        s %= polygon->sizeS;
        if (s < 0) s += polygon->sizeS;
    }
    else if (s < 0)
    {
        // Clamp the S-coordinate on the left
        s = 0;
    }
    else if (s >= polygon->sizeS)
    {
        // Clamp the S-coordinate on the right
        s = polygon->sizeS - 1;
    }

    // Handle T-coordinate overflows
    if (polygon->repeatT)
    {
        // Flip the T-coordinate every second repeat
        if (polygon->flipT && (t / polygon->sizeT) % 2 != 0)
            t = polygon->sizeT - 1 - t;

        // Wrap the T-coordinate
        t %= polygon->sizeT;
        if (t < 0) t += polygon->sizeT;
    }
    else if (t < 0)
    {
        // Clamp the T-coordinate on the top
        t = 0;
    }
    else if (t >= polygon->sizeT)
    {
        // Clamp the T-coordinate on the bottom
        t = polygon->sizeT - 1;
    }

    switch (polygon->texFormat)
    {
        case 1: // A3I5 translucent
        {
            // Get the 8-bit palette index
            uint32_t address = polygon->texDataAddr + (t * polygon->sizeS + s);
            uint8_t index = *gpu3D->getTexData(address);

            // Get the palette
            uint8_t *palette = gpu3D->getTexPalette(polygon->texPaletteAddr);

            // Return the palette color or a transparent pixel
            return ((index & 0xE0) == 0) ? 0 : (rgb5ToRgb6(U8TO16(palette, (index & 0x1F) * 2)) | BIT(18));
        }

        case 2: // 4-color palette
        {
            // Get the 2-bit palette index
            uint32_t address = polygon->texDataAddr + (t * polygon->sizeS + s) / 4;
            uint8_t index = (*gpu3D->getTexData(address) >> ((s % 4) * 2)) & 0x03;

            // Get the palette
            uint8_t *palette = gpu3D->getTexPalette(polygon->texPaletteAddr);

            // Return the palette color or a transparent pixel if enabled
            return (polygon->transparent && index == 0) ? 0 : (rgb5ToRgb6(U8TO16(palette, index * 2)) | BIT(18));
        }

        case 3: // 16-color palette
        {
            // Get the 4-bit palette index
            uint32_t address = polygon->texDataAddr + (t * polygon->sizeS + s) / 2;
            uint8_t index = (*gpu3D->getTexData(address) >> ((s % 2) * 4)) & 0x0F;

            // Get the palette
            uint8_t *palette = gpu3D->getTexPalette(polygon->texPaletteAddr);

            // Return the palette color or a transparent pixel if enabled
            return (polygon->transparent && index == 0) ? 0 : (rgb5ToRgb6(U8TO16(palette, index * 2)) | BIT(18));
        }

        case 4: // 256-color palette
        {
            // Get the 8-bit palette index
            uint32_t address = polygon->texDataAddr + (t * polygon->sizeS + s);
            uint8_t index = *gpu3D->getTexData(address);

            // Get the palette
            uint8_t *palette = gpu3D->getTexPalette(polygon->texPaletteAddr);

            // Return the palette color or a transparent pixel if enabled
            return (polygon->transparent && index == 0) ? 0 : (rgb5ToRgb6(U8TO16(palette, index * 2)) | BIT(18));
        }

        case 5: // 4x4 compressed
        {
            // Get the 2-bit palette index
            int tile = (t / 4) * (polygon->sizeS / 4) + (s / 4);
            uint32_t address = polygon->texDataAddr + (tile * 4 + t % 4);
            uint8_t index = (*gpu3D->getTexData(address) >> ((s % 4) * 2)) & 0x03;

            // Get the palette, using the base for the tile stored in slot 1
            address = 0x20000 + (polygon->texDataAddr % 0x20000) / 2 + ((polygon->texDataAddr / 0x20000 == 2) ? 0x10000 : 0);
            uint16_t palBase = U8TO16(gpu3D->getTexData(address), tile * 2);
            uint8_t *palette = gpu3D->getTexPalette(polygon->texPaletteAddr + (palBase & 0x3FFF) * 4);

            // Return the palette color or a transparent or interpolated color based on the mode
            switch ((palBase & 0xC000) >> 14) // Interpolation mode
            {
                case 0:
                    return (index == 3) ? 0 : (rgb5ToRgb6(U8TO16(palette, index * 2)) | BIT(18));

                case 1:
                    switch (index)
                    {
                        case 2:
                        {
                            uint32_t color1 = rgb5ToRgb6(U8TO16(palette, 0 * 2));
                            uint32_t color2 = rgb5ToRgb6(U8TO16(palette, 1 * 2));
                            return interpolateColor(color1, color2, 0, 1, 2);
                        }

                        case 3:
                            return 0;

                        default:
                            return rgb5ToRgb6(U8TO16(palette, index * 2)) | BIT(18);
                    }

                case 2:
                    return rgb5ToRgb6(U8TO16(palette, index * 2)) | BIT(18);

                case 3:
                    switch (index)
                    {
                        case 2:
                        {
                            uint32_t color1 = rgb5ToRgb6(U8TO16(palette, 0 * 2));
                            uint32_t color2 = rgb5ToRgb6(U8TO16(palette, 1 * 2));
                            return interpolateColor(color1, color2, 0, 3, 8);
                        }

                        case 3:
                        {
                            uint32_t color1 = rgb5ToRgb6(U8TO16(palette, 0 * 2));
                            uint32_t color2 = rgb5ToRgb6(U8TO16(palette, 1 * 2));
                            return interpolateColor(color1, color2, 0, 5, 8);
                        }

                        default:
                            return rgb5ToRgb6(U8TO16(palette, index * 2)) | BIT(18);
                    }
            }
        }

        case 6: // A5I3 translucent
        {
            // Get the 8-bit palette index
            uint32_t address = polygon->texDataAddr + (t * polygon->sizeS + s);
            uint8_t index = *gpu3D->getTexData(address);

            // Get the palette
            uint8_t *palette = gpu3D->getTexPalette(polygon->texPaletteAddr);

            // Return the palette color or a transparent pixel
            return ((index & 0xF8) == 0) ? 0 : (rgb5ToRgb6(U8TO16(palette, (index & 0x07) * 2)) | BIT(18));
        }

        default: // Direct color
            // Return the direct color
            return rgb5ToRgb6(U8TO16(gpu3D->getTexData(polygon->texDataAddr), (t * polygon->sizeS + s) * 2));
    }
}

void Gpu3DRenderer::rasterize(int line, _Polygon *polygon, Vertex *v0, Vertex *v1, Vertex *v2, Vertex *v3)
{
    // Calculate the X bounds between the line between V0 and V1 and the line between V2 and V3
    int lx0 = interpolate(v0->x, v1->x, v0->y, line, v1->y);
    int lx1 = interpolate(v2->x, v3->x, v2->y, line, v3->y);

    // Draw a line segment
    for (int x = ((lx0 < 0) ? 0 : lx0); x < ((lx1 > 256) ? 256 : lx1); x++)
    {
        // Calculate the Z value of the current pixel
        int z0 = interpolate(v0->z, v1->z, v0->y, line, v1->y);
        int z1 = interpolate(v2->z, v3->z, v2->y, line, v3->y);
        int z  = interpolate(z0, z1, lx0, x, lx1);

        // Draw a new pixel if the old one is behind the new one
        if (depthBuffer[x] >= z)
        {
            uint32_t color;

            // Calculate the pixel color
            if (polygon->texFormat == 0) // No texture
            {
                // Interpolate the vertex colors
                uint32_t color0 = interpolateColor(rgb5ToRgb6(v0->color), rgb5ToRgb6(v1->color), v0->y, line, v1->y);
                uint32_t color1 = interpolateColor(rgb5ToRgb6(v2->color), rgb5ToRgb6(v3->color), v2->y, line, v3->y);
                color = interpolateColor(color0, color1, lx0, x, lx1);
            }
            else
            {
                // Interpolate the texture S coordinate
                int s0 = interpolate(v0->s, v1->s, v0->y, line, v1->y);
                int s1 = interpolate(v2->s, v3->s, v2->y, line, v3->y);
                int s  = interpolate(s0, s1, lx0, x, lx1);

                // Interpolate the texture T coordinate
                int t0 = interpolate(v0->t, v1->t, v0->y, line, v1->y);
                int t1 = interpolate(v2->t, v3->t, v2->y, line, v3->y);
                int t  = interpolate(t0, t1, lx0, x, lx1);

                // Read the color from a texture
                color = readTexture(polygon, s >> 4, t >> 4);
            }

            // Draw a pixel
            if (color & BIT(18))
            {
                lineCache[(line % 48) * 256 + x] = color;
                depthBuffer[x] = z;
            }
        }
    }
}
