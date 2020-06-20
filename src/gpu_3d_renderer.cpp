/*
    Copyright 2019-2020 Hydr8gon

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

#include <vector>

#include "gpu_3d_renderer.h"
#include "defines.h"
#include "gpu_3d.h"
#include "settings.h"

Gpu3DRenderer::~Gpu3DRenderer()
{
    // Free the threads
    for (int i = 0; i < activeThreads; i++)
    {
        if (threads[i]) 
        {
            threads[i]->join();
            delete threads[i];
        }
    }
}

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
    if (line == 0)
    {
        // Ensure the threads are free
        for (int i = 0; i < activeThreads; i++)
        {
            if (threads[i]) 
            {
                threads[i]->join();
                delete threads[i];
            }
        }

        // Update the thread count
        activeThreads = Settings::getThreaded3D();
        if (activeThreads > 4) activeThreads = 4;

        // If threading is enabled, draw the 3D scene in 48-scanline blocks across up to 4 separate threads
        // An actual DS only has a 48-scanline cache instead of a full framebuffer for 3D
        // It makes no difference to the output though, so a full framebuffer is used to make this possible
        // Even timing shouldn't affect the output, since the geometry buffers can only be swapped at V-blank!
        for (int i = 0; i < activeThreads; i++)
            threads[i] = new std::thread(&Gpu3DRenderer::drawScanline48, this, i);
    }

    if (activeThreads > 0) // Threaded
    {
        if (line % 48 == 47)
        {
            int i = line / 48;
            int j = i % activeThreads;

            // The 3D scene is drawn 48 scanlines in advance
            // Ensure the thread responsible for the next block is finished, and free it
            if (threads[j])
            {
                threads[j]->join();
                delete threads[j];
            }

            // If there are still blocks to render, start the next one
            if (i + activeThreads < 4)
                threads[j] = new std::thread(&Gpu3DRenderer::drawScanline48, this, i + activeThreads);
            else
                threads[j] = nullptr;
        }
    }
    else
    {
        // Draw one scanline at a time
        drawScanline1(line);
    }
}

void Gpu3DRenderer::drawScanline48(int block)
{
    // Draw a block of 48 scanlines, or 1/4 of the screen
    for (int i = block * 48; i < (block + 1) * 48; i++)
        drawScanline1(i);
}

void Gpu3DRenderer::drawScanline1(int line)
{
    // Convert the clear values
    // The attribute buffer contains the polygon ID, and the fog bit in bit 7
    uint32_t color = BIT(26) | rgba5ToRgba6(((clearColor & 0x001F0000) >> 1) | (clearColor & 0x00007FFF));
    int32_t depth = (clearDepth * 0x200) + ((clearDepth + 1) / 0x8000) * 0x1FF;
    uint8_t attrib = ((clearColor & BIT(15)) >> 8) | ((clearColor & 0x3F000000) >> 24);

    // Clear the scanline buffers with the clear values
    for (int i = 0; i < 256; i++)
    {
        framebuffer[line * 256 + i] = color;
        depthBuffer[line / 48][i] = depth;
        attribBuffer[line / 48][i] = attrib;
        stencilBuffer[line / 48][i] = 0;
    }

    std::vector<_Polygon*> translucent;

    // Draw the solid polygons
    for (int i = 0; i < gpu3D->getPolygonCount(); i++)
    {
        _Polygon *polygon = &gpu3D->getPolygons()[i];

        // If the polygon is translucent, save it for last
        if ((polygon->vertices[0].color >> 18) < 0x3F || polygon->textureFmt == 1 || polygon->textureFmt == 6)
            translucent.push_back(polygon);
        else
            drawPolygon(line, polygon);
    }

    // Draw the translucent polygons
    for (int i = 0; i < translucent.size(); i++)
        drawPolygon(line, translucent[i]);

    // Draw fog if enabled
    if (disp3DCnt & BIT(7))
    {
        uint32_t fog = rgba5ToRgba6(((fogColor & 0x001F0000) >> 1) | (fogColor & 0x00007FFF));
        int fogStep = 0x400 >> ((disp3DCnt & 0x0F00) >> 8);

        for (int i = 0; i < 256; i++)
        {
            if (attribBuffer[line / 48][i] & BIT(7)) // Fog bit
            {
                // Determine the fog table index for the current pixel's depth
                int32_t offset = ((depthBuffer[line / 48][i] / 0x200) - fogOffset);
                int n = (fogStep > 0) ? (offset / fogStep) : ((offset > 0) ? 31 : 0);

                // Get the fog density from the table
                uint8_t density;
                if (n >= 31) // Maximum
                {
                    density = fogTable[31];
                }
                else if (n < 0 || fogStep == 0) // Minimum
                {
                    density = fogTable[0];
                }
                else // Linear interpolation
                {
                    int m = offset % fogStep;
                    density = ((m >= 0) ? ((fogTable[n + 1] * m + fogTable[n] * (fogStep - m)) / fogStep) : fogTable[0]);
                }

                // Blend the fog with the pixel
                uint32_t *pixel = &framebuffer[line * 256 + i];
                uint8_t a = (((fog >> 18) & 0x3F) * density + ((*pixel >> 18) & 0x3F) * (128 - density)) / 128;
                if (disp3DCnt & BIT(6)) // Only alpha
                {
                    *pixel = (*pixel & ~(0x3F << 18)) | (a << 18);
                }
                else
                {
                    uint8_t r = (((fog >>  0) & 0x3F) * density + ((*pixel >>  0) & 0x3F) * (128 - density)) / 128;
                    uint8_t g = (((fog >>  6) & 0x3F) * density + ((*pixel >>  6) & 0x3F) * (128 - density)) / 128;
                    uint8_t b = (((fog >> 12) & 0x3F) * density + ((*pixel >> 12) & 0x3F) * (128 - density)) / 128;
                    *pixel = BIT(26) | (a << 18) | (b << 12) | (g << 6) | r;
                }
            }
        }
    }
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

int Gpu3DRenderer::interpolate(int64_t v1, int64_t v2, int x1, int x, int x2)
{
    // Linearly interpolate a new value between the min and max values
    return (v1 * (x2 - x) + v2 * (x - x1)) / (x2 - x1);
}

int Gpu3DRenderer::interpolateW(int64_t w1, int64_t w2, int x1, int x, int x2)
{
    // Interpolate a new W value between the min and max values
    return w2 * w1 * (x2 - x1) / (w2 * (x2 - x) + w1 * (x - x1));
}

int Gpu3DRenderer::interpolate(int64_t v1, int64_t v2, int x1, int x, int x2, int w1, int w2)
{
    // Interpolate a new perspective-correct value between the min and max values
    return (v1 * w2 * (x2 - x) + v2 * w1 * (x - x1)) / (w2 * (x2 - x) + w1 * (x - x1));
}

uint32_t Gpu3DRenderer::interpolateColor(uint32_t c1, uint32_t c2, int x1, int x, int x2)
{
    // Apply linear interpolation separately on the RGB values
    int r = interpolate((c1 >>  0) & 0x3F, (c2 >>  0) & 0x3F, x1, x, x2);
    int g = interpolate((c1 >>  6) & 0x3F, (c2 >>  6) & 0x3F, x1, x, x2);
    int b = interpolate((c1 >> 12) & 0x3F, (c2 >> 12) & 0x3F, x1, x, x2);
    int a = (((c1 >> 18) & 0x3F) > ((c2 >> 18) & 0x3F)) ? ((c1 >> 18) & 0x3F) : ((c2 >> 18) & 0x3F);
    return (a << 18) | (b << 12) | (g << 6) | r;
}

uint32_t Gpu3DRenderer::interpolateColor(uint32_t c1, uint32_t c2, int x1, int x, int x2, int w1, int w2)
{
    // Apply perspective-correct interpolation separately on the RGB values
    int r = interpolate((c1 >>  0) & 0x3F, (c2 >>  0) & 0x3F, x1, x, x2, w1, w2);
    int g = interpolate((c1 >>  6) & 0x3F, (c2 >>  6) & 0x3F, x1, x, x2, w1, w2);
    int b = interpolate((c1 >> 12) & 0x3F, (c2 >> 12) & 0x3F, x1, x, x2, w1, w2);
    int a = (((c1 >> 18) & 0x3F) > ((c2 >> 18) & 0x3F)) ? ((c1 >> 18) & 0x3F) : ((c2 >> 18) & 0x3F);
    return (a << 18) | (b << 12) | (g << 6) | r;
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
    for (int i = 0; i < polygon->size; i++)
        vertices[i] = &polygon->vertices[i];

    // Unclipped quad strip polygons have their vertices crossed, so uncross them
    if (polygon->crossed)
    {
        Vertex *vertex = vertices[2];
        vertices[2] = vertices[3];
        vertices[3] = vertex;
    }

    Vertex *v[4];
    int count = 0;

    // Find the polygon edges that intersect with the current line
    for (int i = 0; i < polygon->size; i++)
    {
        Vertex *current = vertices[i];
        Vertex *previous = vertices[(i - 1 + polygon->size) % polygon->size];

        if (current->y > previous->y)
        {
            if (current->y > line && previous->y <= line)
            {
                v[count++] = previous;
                v[count++] = current;
            }
        }
        else if (previous->y > line && current->y <= line)
        {
            v[count++] = current;
            v[count++] = previous;
        }

        if (count == 4) break;
    }

    // Rasterize the polygon if edges were found
    if (count == 4)
        rasterize(line, polygon, v[0], v[1], v[2], v[3]);
}

void Gpu3DRenderer::rasterize(int line, _Polygon *polygon, Vertex *v1, Vertex *v2, Vertex *v3, Vertex *v4)
{
    // Calculate the X bounds of the polygon on the current line
    int x1 = interpolate(v1->x, v2->x, v1->y, line, v2->y);
    int x2 = interpolate(v3->x, v4->x, v3->y, line, v4->y);

    // Swap the edges if the first one is on the right
    if (x1 > x2)
    {
        int x = x1;
        x1 = x2;
        x2 = x;

        Vertex *v = v1;
        v1 = v3;
        v3 = v;

        v = v2;
        v2 = v4;
        v4 = v;
    }

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

    // Calculate the Z values of the polygon edges on the current line
    int z1 = polygon->wBuffer ? 0 : interpolate(v1->z, v2->z, v1->y, line, v2->y);
    int z2 = polygon->wBuffer ? 0 : interpolate(v3->z, v4->z, v3->y, line, v4->y);

    // Calculate the W values of the polygon edges on the current line
    int w1 = interpolateW(vw[0], vw[1], v1->y, line, v2->y);
    int w2 = interpolateW(vw[2], vw[3], v3->y, line, v4->y);

    int c1, c2, s1, s2, t1, t2;
    bool colorDone = false, texDone = false;

    // Draw a line segment
    for (int x = x1; x < x2; x++)
    {
        // Calculate the depth value of the current pixel
        int depth = polygon->wBuffer ? (interpolateW(w1, w2, x1, x, x2) << wShift) : interpolate(z1, z2, x1, x, x2);

        // Draw a new pixel if the old one is behind the new one
        // The polygon can optionally use an "equal" depth test, which has a margin of 0x200
        if ((polygon->depthTestEqual && depthBuffer[line / 48][x] + 0x200 >= depth) || depthBuffer[line / 48][x] > depth)
        {
            // Handle shadow polygons
            if (polygon->mode == 3)
            {
                if (polygon->id == 0)
                {
                    // Shadow polygons with ID 0 set a stencil buffer bit instead of rendering
                    stencilBuffer[line / 48][x] = 1;
                    continue;
                }
                else if (stencilBuffer[line / 48][x] || (attribBuffer[line / 48][x] & 0x3F) == polygon->id)
                {
                    // Shadow polygons with ID not 0 only render if the stencil bit is clear and the pixel ID differs
                    stencilBuffer[line / 48][x] = 0;
                    continue;
                }
            }

            // Interpolate the vertex color at the polygon edges
            if (!colorDone)
            {
                c1 = interpolateColor(v1->color, v2->color, v1->y, line, v2->y, vw[0], vw[1]);
                c2 = interpolateColor(v3->color, v4->color, v3->y, line, v4->y, vw[2], vw[3]);
                colorDone = true;
            }

            // Interpolate the vertex color at the current pixel
            uint32_t color = interpolateColor(c1, c2, x1, x, x2, w1, w2);

            // Blend the texture with the vertex color
            if (polygon->textureFmt != 0)
            {
                // Interpolate the texture coordinates at the polygon edges
                if (!texDone)
                {
                    s1 = interpolate(v1->s, v2->s, v1->y, line, v2->y, vw[0], vw[1]);
                    s2 = interpolate(v3->s, v4->s, v3->y, line, v4->y, vw[2], vw[3]);
                    t1 = interpolate(v1->t, v2->t, v1->y, line, v2->y, vw[0], vw[1]);
                    t2 = interpolate(v3->t, v4->t, v3->y, line, v4->y, vw[2], vw[3]);
                    texDone = true;
                }

                // Interpolate the texture coordinates at the current pixel
                int s = interpolate(s1, s2, x1, x, x2, w1, w2);
                int t = interpolate(t1, t2, x1, x, x2, w1, w2);

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
                        uint32_t toon = rgba5ToRgba6(toonTable[(color & 0x3F) / 2]);
                        uint8_t r, g, b;

                        if (disp3DCnt & BIT(1)) // Highlight
                        {
                            r = ((((texel >>  0) & 0x3F) + 1) * (((color >>  0) & 0x3F) + 1) - 1) / 64;
                            g = ((((texel >>  6) & 0x3F) + 1) * (((color >>  6) & 0x3F) + 1) - 1) / 64;
                            b = ((((texel >> 12) & 0x3F) + 1) * (((color >> 12) & 0x3F) + 1) - 1) / 64;
                            r += ((toon >>  0) & 0x3F); if (r > 63) r = 63;
                            g += ((toon >>  6) & 0x3F); if (g > 63) g = 63;
                            b += ((toon >> 12) & 0x3F); if (b > 63) b = 63;
                        }
                        else // Toon
                        {
                            r = ((((texel >>  0) & 0x3F) + 1) * (((toon >>  0) & 0x3F) + 1) - 1) / 64;
                            g = ((((texel >>  6) & 0x3F) + 1) * (((toon >>  6) & 0x3F) + 1) - 1) / 64;
                            b = ((((texel >> 12) & 0x3F) + 1) * (((toon >> 12) & 0x3F) + 1) - 1) / 64;
                        }

                        uint8_t a = ((((texel >> 18) & 0x3F) + 1) * (((color >> 18) & 0x3F) + 1) - 1) / 64;
                        color = (a << 18) | (b << 12) | (g << 6) | r;
                        break;
                    }
                }
            }

            // Draw a pixel
            // 3D pixels are marked with an extra bit as an indicator for 2D blending
            if (color & 0xFC0000)
            {
                uint32_t *pixel = &framebuffer[line * 256 + x];
                uint8_t *attrib = &attribBuffer[line / 48][x];

                if ((color >> 18) < 0x3F && (*pixel & 0xFC0000)) // Alpha blending
                {
                    *pixel = BIT(26) | interpolateColor(*pixel, color, 0, color >> 18, 63);
                    if (polygon->transNewDepth) depthBuffer[line / 48][x] = depth;
                    *attrib = (*attrib & (polygon->fog << 7)) | polygon->id;
                }
                else
                {
                    *pixel = BIT(26) | color;
                    depthBuffer[line / 48][x] = depth;
                    *attrib = (polygon->fog << 7) | polygon->id;
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

void Gpu3DRenderer::writeClearColor(uint32_t mask, uint32_t value)
{
    // Write to the CLEAR_COLOR register
    mask &= 0x3F1FFFFF;
    clearColor = (clearColor & ~mask) | (value & mask);
}

void Gpu3DRenderer::writeClearDepth(uint16_t mask, uint16_t value)
{
    // Write to the CLEAR_DEPTH register
    mask &= 0x7FFF;
    clearDepth = (clearDepth & ~mask) | (value & mask);
}

void Gpu3DRenderer::writeToonTable(int index, uint16_t mask, uint16_t value)
{
    // Write to one of the TOON_TABLE registers
    mask &= 0x7FFF;
    toonTable[index] = (toonTable[index] & ~mask) | (value & mask);
}

void Gpu3DRenderer::writeFogColor(uint32_t mask, uint32_t value)
{
    // Write to the FOG_COLOR register
    mask &= 0x001F7FFF;
    fogColor = (fogColor & ~mask) | (value & mask);
}

void Gpu3DRenderer::writeFogOffset(uint16_t mask, uint16_t value)
{
    // Write to the FOG_OFFSET register
    mask &= 0x7FFF;
    fogOffset = (fogOffset & ~mask) | (value & mask);
}

void Gpu3DRenderer::writeFogTable(int index, uint8_t value)
{
    // Write to one of the FOG_TABLE registers
    fogTable[index] = value & 0x7F;
}
