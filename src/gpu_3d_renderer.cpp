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
#include <vector>

#include "gpu_3d_renderer.h"
#include "defines.h"
#include "gpu_3d.h"

uint32_t Gpu3DRenderer::rgba5ToRgba6(uint32_t color)
{
    // Convert an RGBA5 value to an RGBA6 value (the way the 3D engine does it)
    uint8_t r = ((color >>  0) & 0x1F); r = r * 2 + (r + 31) / 32;
    uint8_t g = ((color >>  5) & 0x1F); g = g * 2 + (g + 31) / 32;
    uint8_t b = ((color >> 10) & 0x1F); b = b * 2 + (b + 31) / 32;
    uint8_t a = ((color >> 15) & 0x1F); a = a * 2 + (a + 31) / 32;
    return (a << 18) | (b << 12) | (g << 6) | r;
}

void Gpu3DRenderer::drawScanline(int line)
{
    // Clear the scanline
    memset(&lineCache[(line % 48) * 256], 0, 256 * sizeof(uint32_t));

    // "Empty" the depth buffer by setting all values to maximum
    for (int i = 0; i < 256; i++)
        depthBuffer[i] = 0xFFFFFF;

    std::vector<_Polygon*> translucent;

    // Draw the solid polygons
    for (int i = 0; i < gpu3D->getPolygonCount(); i++)
    {
        _Polygon *polygon = &gpu3D->getPolygons()[i];

        // If the polygon is translucent, save it for later
        if ((polygon->vertices[0].color >> 18) < 0x3F)
            translucent.push_back(polygon);
        else
            drawPolygon(line, polygon);
    }

    // Draw the translucent polygons
    for (int i = 0; i < translucent.size(); i++)
        drawPolygon(line, translucent[i]);
}

uint8_t *Gpu3DRenderer::getTexture(uint32_t address)
{
    // Get a pointer to texture data
    uint8_t *slot = textures[address / 0x20000];
    return slot ? &slot[address % 0x20000] : nullptr;
}

uint8_t *Gpu3DRenderer::getPalette(uint32_t address)
{
    // Get a pointer to palette data
    uint8_t *slot = palettes[address / 0x4000];
    return slot ? &slot[address % 0x4000] : nullptr;
}

int Gpu3DRenderer::interpolateW(int w1, int w2, int x1, int x, int x2)
{
    // Interpolate a new value between the min and max values
    int result = w2 + (w1 - w2) * (x - x1) / (x2 - x1);
    return result ? (w1 * w2 / result) : 0;
}

int Gpu3DRenderer::interpolate(int v1, int v2, int x1, int x, int x2)
{
    // Interpolate a new value between the min and max values
    return v1 + (v2 - v1) * (x - x1) / (x2 - x1);
}

int Gpu3DRenderer::interpolate(int v1, int v2, int x1, int x, int x2, int w1, int w, int w2)
{
    // Get the parameters
    int min = w1 ? (v1 * w / w1) : 0;
    int max = w2 ? (v2 * w / w2) : 0;

    // Interpolate a new value between the min and max values
    return min + (max - min) * (x - x1) / (x2 - x1);
}

uint32_t Gpu3DRenderer::interpolateColor(uint32_t c1, uint32_t c2, int x1, int x, int x2)
{
    // Apply interpolation separately on the RGB values
    int r = interpolate((c1 >>  0) & 0x3F, (c2 >>  0) & 0x3F, x1, x, x2);
    int g = interpolate((c1 >>  6) & 0x3F, (c2 >>  6) & 0x3F, x1, x, x2);
    int b = interpolate((c1 >> 12) & 0x3F, (c2 >> 12) & 0x3F, x1, x, x2);
    return (c1 & 0xFC0000) | (b << 12) | (g << 6) | r;
}

uint32_t Gpu3DRenderer::interpolateColor(uint32_t c1, uint32_t c2, int x1, int x, int x2, int w1, int w, int w2)
{
    // Apply interpolation separately on the RGB values
    int r = interpolate((c1 >>  0) & 0x3F, (c2 >>  0) & 0x3F, x1, x, x2, w1, w, w2);
    int g = interpolate((c1 >>  6) & 0x3F, (c2 >>  6) & 0x3F, x1, x, x2, w1, w, w2);
    int b = interpolate((c1 >> 12) & 0x3F, (c2 >> 12) & 0x3F, x1, x, x2, w1, w, w2);
    return (c1 & 0xFC0000) | (b << 12) | (g << 6) | r;
}

uint32_t Gpu3DRenderer::readTexture(_Polygon *polygon, int s, int t)
{
    // Handle S-coordinate overflows
    if (polygon->repeatS)
    {
        // Wrap the S-coordinate
        int count = 0;
        while (s < 0)               { s += polygon->sizeS; count++; }
        while (s >= polygon->sizeS) { s -= polygon->sizeS; count++; }

        // Flip the S-coordinate every second repeat
        if (polygon->flipS && count % 2 != 0)
            s = polygon->sizeS - 1 - s;
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
        // Wrap the T-coordinate
        int count = 0;
        while (t < 0)               { t += polygon->sizeT; count++; }
        while (t >= polygon->sizeT) { t -= polygon->sizeT; count++; }

        // Flip the T-coordinate every second repeat
        if (polygon->flipT && count % 2 != 0)
            t = polygon->sizeT - 1 - t;
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

    // Decode a texel
    switch (polygon->textureFmt)
    {
        case 1: // A3I5 translucent
        {
            // Get the 8-bit palette index
            uint32_t address = polygon->textureAddr + (t * polygon->sizeS + s);
            uint8_t *data = getTexture(address);
            if (!data) return 0;
            uint8_t index = *data;

            // Get the palette
            uint8_t *palette = getPalette(polygon->paletteAddr);
            if (!palette) return 0;

            // Return the palette color
            uint16_t color = U8TO16(palette, (index & 0x1F) * 2) & ~BIT(15);
            uint8_t alpha = (index >> 5) * 4 + (index >> 5) / 2;
            return rgba5ToRgba6((alpha << 15) | color);
        }

        case 2: // 4-color palette
        {
            // Get the 2-bit palette index
            uint32_t address = polygon->textureAddr + (t * polygon->sizeS + s) / 4;
            uint8_t *data = getTexture(address);
            if (!data) return 0;
            uint8_t index = (*data >> ((s % 4) * 2)) & 0x03;

            // Return a transparent pixel if enabled
            if (polygon->transparent0 && index == 0)
                return 0;

            // Get the palette
            uint8_t *palette = getPalette(polygon->paletteAddr);
            if (!palette) return 0;

            // Return the palette color
            return rgba5ToRgba6((0x1F << 15) | U8TO16(palette, index * 2));
        }

        case 3: // 16-color palette
        {
            // Get the 4-bit palette index
            uint32_t address = polygon->textureAddr + (t * polygon->sizeS + s) / 2;
            uint8_t *data = getTexture(address);
            if (!data) return 0;
            uint8_t index = (*data >> ((s % 2) * 4)) & 0x0F;

            // Return a transparent pixel if enabled
            if (polygon->transparent0 && index == 0)
                return 0;

            // Get the palette
            uint8_t *palette = getPalette(polygon->paletteAddr);
            if (!palette) return 0;

            // Return the palette color
            return rgba5ToRgba6((0x1F << 15) | U8TO16(palette, index * 2));
        }

        case 4: // 256-color palette
        {
            // Get the 8-bit palette index
            uint32_t address = polygon->textureAddr + (t * polygon->sizeS + s);
            uint8_t *data = getTexture(address);
            if (!data) return 0;
            uint8_t index = *data;

            // Return a transparent pixel if enabled
            if (polygon->transparent0 && index == 0)
                return 0;

            // Get the palette
            uint8_t *palette = getPalette(polygon->paletteAddr);
            if (!palette) return 0;

            // Return the palette color
            return rgba5ToRgba6((0x1F << 15) | U8TO16(palette, index * 2));
        }

        case 5: // 4x4 compressed
        {
            // Get the 2-bit palette index
            int tile = (t / 4) * (polygon->sizeS / 4) + (s / 4);
            uint32_t address = polygon->textureAddr + (tile * 4 + t % 4);
            uint8_t *data = getTexture(address);
            if (!data) return 0;
            uint8_t index = (*data >> ((s % 4) * 2)) & 0x03;

            // Get the palette, using the base for the tile stored in slot 1
            address = 0x20000 + (polygon->textureAddr % 0x20000) / 2 + ((polygon->textureAddr / 0x20000 == 2) ? 0x10000 : 0);
            uint16_t palBase = U8TO16(getTexture(address), tile * 2);
            uint8_t *palette = getPalette(polygon->paletteAddr + (palBase & 0x3FFF) * 4);
            if (!palette) return 0;

            // Return the palette color or a transparent or interpolated color based on the mode
            switch ((palBase & 0xC000) >> 14) // Interpolation mode
            {
                case 0:
                {
                    if (index == 3) return 0;
                    return rgba5ToRgba6((0x1F << 15) | U8TO16(palette, index * 2));
                }

                case 1:
                {
                    switch (index)
                    {
                        case 2:
                        {
                            uint32_t c1 = rgba5ToRgba6((0x1F << 15) | U8TO16(palette, 0));
                            uint32_t c2 = rgba5ToRgba6((0x1F << 15) | U8TO16(palette, 2));
                            return interpolateColor(c1, c2, 0, 1, 2);
                        }

                        case 3:
                        {
                            return 0;
                        }

                        default:
                        {
                            return rgba5ToRgba6((0x1F << 15) | U8TO16(palette, index * 2));
                        }
                    }
                }

                case 2:
                {
                    return rgba5ToRgba6((0x1F << 15) | U8TO16(palette, index * 2));
                }

                case 3:
                {
                    switch (index)
                    {
                        case 2:
                        {
                            uint32_t c1 = rgba5ToRgba6((0x1F << 15) | U8TO16(palette, 0));
                            uint32_t c2 = rgba5ToRgba6((0x1F << 15) | U8TO16(palette, 2));
                            return interpolateColor(c1, c2, 0, 3, 8);
                        }

                        case 3:
                        {
                            uint32_t c1 = rgba5ToRgba6((0x1F << 15) | U8TO16(palette, 0));
                            uint32_t c2 = rgba5ToRgba6((0x1F << 15) | U8TO16(palette, 2));
                            return interpolateColor(c1, c2, 0, 5, 8);
                        }

                        default:
                        {
                            return rgba5ToRgba6((0x1F << 15) | U8TO16(palette, index * 2));
                        }
                    }
                }
            }
        }

        case 6: // A5I3 translucent
        {
            // Get the 8-bit palette index
            uint32_t address = polygon->textureAddr + (t * polygon->sizeS + s);
            uint8_t *data = getTexture(address);
            if (!data) return 0;
            uint8_t index = *data;

            // Get the palette
            uint8_t *palette = getPalette(polygon->paletteAddr);
            if (!palette) return 0;

            // Return the palette color
            uint16_t color = U8TO16(palette, (index & 0x07) * 2) & ~BIT(15);
            uint8_t alpha = index >> 3;
            return rgba5ToRgba6((alpha << 15) | color);
        }

        default: // Direct color
        {
            // Get the color data
            uint8_t *data = getTexture(polygon->textureAddr);
            if (!data) return 0;

            // Return the direct color
            uint16_t color = U8TO16(data, (t * polygon->sizeS + s) * 2);
            uint8_t alpha = (color & BIT(15)) ? 0x1F : 0;
            return rgba5ToRgba6((alpha << 15) | color);
        }
    }
}

void Gpu3DRenderer::drawPolygon(int line, _Polygon *polygon)
{
    // Get the polygon vertices
    Vertex *vertices[8];
    for (int j = 0; j < polygon->size; j++)
        vertices[j] = &polygon->vertices[j];

    // Sort the vertices in order of increasing Y values
    for (int j = 0; j < polygon->size - 1; j++)
    {
        for (int k = j + 1; k < polygon->size; k++)
        {
            if (vertices[k]->y < vertices[j]->y)
            {
                Vertex *vertex = vertices[j];
                vertices[j] = vertices[k];
                vertices[k] = vertex;
            }
        }
    }

    // Check if the polygon intersects with the current scanline
    if (line < vertices[0]->y || line >= vertices[polygon->size - 1]->y)
        return;

    // Calculate the cross products of the middle vertices
    // These determine whether a vertex is on the left or right of the middle of its polygon
    int crosses[6];
    for (int j = 0; j < polygon->size - 2; j++)
    {
        crosses[j] = (vertices[j + 1]->x - vertices[0]->x) * (vertices[polygon->size - 1]->y - vertices[0]->y) -
                     (vertices[j + 1]->y - vertices[0]->y) * (vertices[polygon->size - 1]->x - vertices[0]->x);
    }

    // Rasterize the polygon
    for (int j = 1; j < polygon->size; j++)
    {
        if (line < vertices[j]->y)
        {
            int v1, v2, v3, v4;
            // Find the bottom vertex of the left side of the polygon on the current line
            // This is equal to the highest point equal or below j on the left
            for (v2 = j; v2 < polygon->size; v2++)
                if (v2 == polygon->size - 1 || crosses[v2 - 1] <= 0) break;

            // Find the top vertex of the left side of the polygon on the current line
            // This is equal to the lowest point above v2 on the left
            for (v1 = v2 - 1; v1 >= 0; v1--)
                if (v1 == 0 || crosses[v1 - 1] <= 0) break;

            // Find the bottom vertex of the right side of the polygon on the current line
            // This is equal to the highest point equal or below j on the right
            for (v4 = j; v4 < polygon->size; v4++)
                if (v4 == polygon->size - 1 || crosses[v4 - 1] > 0) break;

            // Find the top vertex of the right side of the polygon on the current line
            // This is equal to the lowest point above v4 on the right
            for (v3 = v4 - 1; v3 >= 0; v3--)
                if (v3 == 0 || crosses[v3 - 1] > 0) break;

            rasterize(line, polygon, vertices[v1], vertices[v2], vertices[v3], vertices[v4]);
            break;
        }
    }
}

void Gpu3DRenderer::rasterize(int line, _Polygon *polygon, Vertex *v1, Vertex *v2, Vertex *v3, Vertex *v4)
{
    // "Normalize" the W values by reducing them to 16-bits
    int64_t vw[] = { v1->w, v2->w, v3->w, v4->w };
    int wShift = 0;
    for (int i = 0; i < 4; i++)
    {
        while (vw[i] != (int16_t)vw[i])
        {
            for (int j = 0; j < 4; j++)
                vw[j] >>= 4;
            wShift += 4;
        }
    }

    // Calculate the X bounds of the polygon on the current line
    int x1 = interpolate(v1->x, v2->x, v1->y, line, v2->y);
    int x2 = interpolate(v3->x, v4->x, v3->y, line, v4->y);

    // Calculate the Z values of the polygon edges on the current line
    int z1 = polygon->wBuffer ? 0 : interpolate(v1->z, v2->z, v1->y, line, v2->y);
    int z2 = polygon->wBuffer ? 0 : interpolate(v3->z, v4->z, v3->y, line, v4->y);

    // Calculate the W values of the polygon edges on the current line
    int w1 = interpolateW(vw[0], vw[1], v1->y, line, v2->y);
    int w2 = interpolateW(vw[2], vw[3], v3->y, line, v4->y);

    // Draw a line segment
    for (int x = x1; x < x2; x++)
    {
        // Calculate the depth value of the current pixel
        int depth = polygon->wBuffer ? (interpolateW(w1, w2, x1, x, x2) << wShift) : interpolate(z1, z2, x1, x, x2);

        // Draw a new pixel if the old one is behind the new one
        // The polygon can optionally use an "equal" depth test, which has a margin of 0x200
        if ((polygon->depthTestEqual && depthBuffer[x] - 0x200 >= depth) || depthBuffer[x] > depth)
        {
            // Calculate the W value of the current pixel
            int w = polygon->wBuffer ? (depth >> wShift) : interpolateW(w1, w2, x1, x, x2);

            // Interpolate the vertex color
            uint32_t c1 = interpolateColor(v1->color, v2->color, v1->y, line, v2->y, vw[0], w1, vw[1]);
            uint32_t c2 = interpolateColor(v3->color, v4->color, v3->y, line, v4->y, vw[2], w2, vw[3]);
            uint32_t color = interpolateColor(c1, c2, x1, x, x2, w1, w, w2);

            // Blend the texture with the vertex color
            if (polygon->textureFmt != 0)
            {
                // Interpolate the texture S coordinate
                int s1 = interpolate(v1->s, v2->s, v1->y, line, v2->y, vw[0], w1, vw[1]);
                int s2 = interpolate(v3->s, v4->s, v3->y, line, v4->y, vw[2], w2, vw[3]);
                int s  = interpolate(s1, s2, x1, x, x2, w1, w, w2);

                // Interpolate the texture T coordinate
                int t1 = interpolate(v1->t, v2->t, v1->y, line, v2->y, vw[0], w1, vw[1]);
                int t2 = interpolate(v3->t, v4->t, v3->y, line, v4->y, vw[2], w2, vw[3]);
                int t  = interpolate(t1, t2, x1, x, x2, w1, w, w2);

                // Read a texel from the texture
                uint32_t texel = readTexture(polygon, s >> 4, t >> 4);

                // Apply texture blending
                // These formulas are a translation of the pseudocode from GBATEK to C++
                switch (polygon->mode)
                {
                    case 0: // Modulation
                    {
                        uint8_t r = ((((texel >>  0) & 0x3F) + 1) * (((color >>  0) & 0x3F) + 1) - 1) / 64;
                        uint8_t g = ((((texel >>  6) & 0x3F) + 1) * (((color >>  6) & 0x3F) + 1) - 1) / 64;
                        uint8_t b = ((((texel >> 12) & 0x3F) + 1) * (((color >> 12) & 0x3F) + 1) - 1) / 64;
                        uint8_t a = ((((texel >> 18) & 0x3F) + 1) * (((color >> 18) & 0x3F) + 1) - 1) / 64;
                        color = (a << 18) | (b << 12) | (g << 6) | r;
                        break;
                    }

                    case 1: // Decal
                    case 3: // Shadow
                    {
                        uint8_t at = ((texel >> 18) & 0x3F);
                        uint8_t r = (((texel >>  0) & 0x3F) * at + ((color >>  0) & 0x3F) * (63 - at)) / 64;
                        uint8_t g = (((texel >>  6) & 0x3F) * at + ((color >>  6) & 0x3F) * (63 - at)) / 64;
                        uint8_t b = (((texel >> 12) & 0x3F) * at + ((color >> 12) & 0x3F) * (63 - at)) / 64;
                        uint8_t a =  ((color >> 18) & 0x3F);
                        color = (a << 18) | (b << 12) | (g << 6) | r;
                        break;
                    }

                    case 2: // Toon/Highlight
                    {
                        uint8_t rv = (((color >>  0) & 0x3F) / 2);
                        uint8_t r = ((((texel >>  0) & 0x3F) + 1) * (((toonTable[rv] >>  0) & 0x3F) + 1) - 1) / 64;
                        uint8_t g = ((((texel >>  6) & 0x3F) + 1) * (((toonTable[rv] >>  6) & 0x3F) + 1) - 1) / 64;
                        uint8_t b = ((((texel >> 12) & 0x3F) + 1) * (((toonTable[rv] >> 12) & 0x3F) + 1) - 1) / 64;
                        uint8_t a = ((((texel >> 18) & 0x3F) + 1) * (((color         >> 18) & 0x3F) + 1) - 1) / 64;

                        if (disp3DCnt & BIT(1))
                        {
                            r += ((toonTable[rv] >>  0) & 0x3F); if (r > 63) r = 63;
                            g += ((toonTable[rv] >>  6) & 0x3F); if (g > 63) g = 63;
                            b += ((toonTable[rv] >> 12) & 0x3F); if (b > 63) b = 63;
                        }

                        color = (a << 18) | (b << 12) | (g << 6) | r;
                        break;
                    }
                }
            }

            // Draw a pixel
            if (color & 0xFC0000)
            {
                uint32_t *pixel = &lineCache[(line % 48) * 256 + x];

                if ((color >> 18) < 0x3F && (*pixel & 0xFC0000)) // Alpha blending
                {
                    *pixel = BIT(18) | interpolateColor(*pixel, color, 0, color >> 18, 63);
                    if (polygon->transNewDepth) depthBuffer[x] = depth;
                }
                else
                {
                    *pixel = BIT(18) | color;
                    depthBuffer[x] = depth;
                }
            }
        }
    }
}

void Gpu3DRenderer::writeDisp3DCnt(uint16_t mask, uint16_t value)
{
    // If any of the error bits are set, acknowledge the errors by clearing them
    if (value & BIT(12)) disp3DCnt &= ~BIT(12);
    if (value & BIT(13)) disp3DCnt &= ~BIT(13);

    // Write to the DISP3DCNT register
    mask &= 0x4FFF;
    disp3DCnt = (disp3DCnt & ~mask) | (value & mask);
}

void Gpu3DRenderer::writeToonTable(int index, uint16_t mask, uint16_t value)
{
    // Write to one of the TOON_TABLE registers
    mask &= 0x7FFF;
    toonTable[index] = rgba5ToRgba6(value & mask);
}
