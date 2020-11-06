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

#include <cstring>
#include <vector>

#include "gpu_3d_renderer.h"
#include "core.h"
#include "settings.h"

Gpu3DRenderer::Gpu3DRenderer(Core *core): core(core)
{
    // Mark the scanlines as ready to start
    // This is mainly in case 3D is requested before the threads have a chance to start
    for (int i = 0; i < 192; i++)
        ready[i].store(true);
}

Gpu3DRenderer::~Gpu3DRenderer()
{
    // Clean up the threads
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
    uint8_t r = ((color >>  0) & 0x1F) * 2; if (r > 0) r++;
    uint8_t g = ((color >>  5) & 0x1F) * 2; if (g > 0) g++;
    uint8_t b = ((color >> 10) & 0x1F) * 2; if (b > 0) b++;
    uint8_t a = ((color >> 15) & 0x1F) * 2; if (a > 0) a++;
    return (a << 18) | (b << 12) | (g << 6) | r;
}

uint32_t *Gpu3DRenderer::getFramebuffer(int line)
{
    // Wait until a scanline is ready, and then return it
    while (!ready[line].load()) std::this_thread::yield();
    return &framebuffer[256 * line];
}

void Gpu3DRenderer::drawScanline(int line)
{
    if (line == 0)
    {
        // Clean up any existing threads
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
        if (activeThreads > 3) activeThreads = 3;

        // Set up threaded 3D rendering if enabled
        if (activeThreads > 0)
        {
            // Mark the scanlines as not ready
            for (int i = 0; i < 192; i++)
                ready[i].store(false);

            // Create threads to draw the scanlines
            for (int i = 0; i < activeThreads; i++)
                threads[i] = new std::thread(&Gpu3DRenderer::drawThreaded, this, i);
        }
    }

    // Draw one scanline at a time if threading is disabled
    if (activeThreads == 0)
        drawScanline1(line, 0);
}

void Gpu3DRenderer::drawThreaded(int thread)
{
    // Draw the 3D scanlines in a threaded sequence
    // The amount of scanlines skipped per thread depends on the number of active threads
    // Together, they render the entire 3D image
    for (int i = thread; i < 192; i += activeThreads)
    {
        drawScanline1(i, thread);
        ready[i].store(true);
    }
}

void Gpu3DRenderer::drawScanline1(int line, int thread)
{
    // Convert the clear values
    // The attribute buffer contains the polygon ID, the transparency bit (6), and the fog bit (7)
    uint32_t color = BIT(26) | rgba5ToRgba6(((clearColor & 0x001F0000) >> 1) | (clearColor & 0x00007FFF));
    uint32_t depth = (clearDepth * 0x200) + ((clearDepth + 1) / 0x8000) * 0x1FF;
    uint8_t attrib = ((clearColor & BIT(15)) >> 8) | ((clearColor & 0x3F000000) >> 24);
    if (((clearColor & 0x001F0000) >> 16) < 31) attrib |= BIT(6);

    // Clear the scanline buffers with the clear values
    for (int i = 0; i < 256; i++)
    {
        framebuffer[line * 256 + i] = color;
        depthBuffer[thread][i] = depth;
        attribBuffer[thread][i] = attrib;
    }

    stencilClear[thread] = false;

    std::vector<_Polygon*> translucent;

    // Draw the solid polygons
    for (int i = 0; i < core->gpu3D.getPolygonCount(); i++)
    {
        _Polygon *polygon = &core->gpu3D.getPolygons()[i];

        // If the polygon is translucent, save it for last
        if (polygon->alpha < 0x3F || polygon->textureFmt == 1 || polygon->textureFmt == 6)
            translucent.push_back(polygon);
        else
            drawPolygon(line, thread, polygon);
    }

    // Draw the translucent polygons
    for (unsigned int i = 0; i < translucent.size(); i++)
        drawPolygon(line, thread, translucent[i]);

    // Draw fog if enabled
    if (disp3DCnt & BIT(7))
    {
        uint32_t fog = rgba5ToRgba6(((fogColor & 0x001F0000) >> 1) | (fogColor & 0x00007FFF));
        int fogStep = 0x400 >> ((disp3DCnt & 0x0F00) >> 8);

        for (int i = 0; i < 256; i++)
        {
            if (attribBuffer[thread][i] & BIT(7)) // Fog bit
            {
                // Determine the fog table index for the current pixel's depth
                int32_t offset = ((depthBuffer[thread][i] / 0x200) - fogOffset);
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
    uint8_t *slot = core->memory.getTex3D()[address >> 17];
    return slot ? &slot[address & 0x1FFFF] : nullptr;
}

uint8_t *Gpu3DRenderer::getPalette(uint32_t address)
{
    // Get a pointer to palette data
    uint8_t *slot = core->memory.getPal3D()[address >> 14];
    return slot ? &slot[address & 0x3FFF] : nullptr;
}

uint32_t Gpu3DRenderer::interpolateLinear(uint32_t v1, uint32_t v2, uint32_t x1, uint32_t x, uint32_t x2)
{
    // Linearly interpolate a new value between the min and max values
    if (v1 <= v2)
        return v1 + (v2 - v1) * (x - x1) / (x2 - x1);
    else
        return v2 + (v1 - v2) * (x2 - x) / (x2 - x1);
}

uint32_t Gpu3DRenderer::interpolateFill(uint32_t v1, uint32_t v2, uint32_t x1, uint32_t x, uint32_t x2, uint32_t w1, uint32_t w2)
{
    // Fall back to linear interpolation if the W values are equal and their lower bits are clear
    if (w1 == w2 && !(w1 & 0x007F))
        return interpolateLinear(v1, v2, x1, x, x2);

    // Calculate the interpolation factor with a precision of 8 bits for polygon fills
    uint32_t factor = ((w1 * (x - x1)) << 8) / (w2 * (x2 - x) + w1 * (x - x1));

    // Interpolate a new value between the min and max values
    if (v1 <= v2)
        return v1 + (((v2 - v1) * factor) >> 8);
    else
        return v2 + (((v1 - v2) * ((1 << 8) - factor)) >> 8);
}

uint32_t Gpu3DRenderer::interpolateEdge(uint32_t v1, uint32_t v2, uint32_t x1, uint32_t x, uint32_t x2, uint32_t w1, uint32_t w2)
{
    // Fall back to linear interpolation if the W values are equal and their lower bits are clear
    if (w1 == w2 && !(w1 & 0x00FE))
        return interpolateLinear(v1, v2, x1, x, x2);

    // Adjust the W values to be 15-bit so the calculation doesn't overflow
    uint32_t w1a = w1 >> 1;
    if ((w1 & 1) && !(w2 & 1)) w1a++;
    w1 >>= 1;
    w2 >>= 1;

    // Calculate the interpolation factor with a precision of 9 bits for polygon edges
    uint32_t factor = ((w1 * (x - x1)) << 9) / (w2 * (x2 - x) + w1a * (x - x1));

    // Interpolate a new value between the min and max values
    if (v1 <= v2)
        return v1 + (((v2 - v1) * factor) >> 9);
    else
        return v2 + (((v1 - v2) * ((1 << 9) - factor)) >> 9);
}

uint32_t Gpu3DRenderer::interpolateColor(uint32_t c1, uint32_t c2, uint32_t x1, uint32_t x, uint32_t x2)
{
    // Apply linear interpolation separately on the RGB values
    uint32_t r = interpolateLinear((c1 >>  0) & 0x3F, (c2 >>  0) & 0x3F, x1, x, x2);
    uint32_t g = interpolateLinear((c1 >>  6) & 0x3F, (c2 >>  6) & 0x3F, x1, x, x2);
    uint32_t b = interpolateLinear((c1 >> 12) & 0x3F, (c2 >> 12) & 0x3F, x1, x, x2);
    uint32_t a = (((c1 >> 18) & 0x3F) > ((c2 >> 18) & 0x3F)) ? ((c1 >> 18) & 0x3F) : ((c2 >> 18) & 0x3F);
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

void Gpu3DRenderer::drawPolygon(int line, int thread, _Polygon *polygon)
{
    // Get the polygon vertices
    Vertex *vertices[8];
    for (int i = 0; i < polygon->size; i++)
        vertices[i] = &polygon->vertices[i];

    // Unclipped quad strip polygons have their vertices crossed, so uncross them
    if (polygon->crossed)
        SWAP(vertices[2], vertices[3]);

    Vertex *vCur[4];
    int countCur = 0;

    // Find the polygon edges that intersect with the current line
    for (int i = 0; i < polygon->size && countCur < 4; i++)
    {
        Vertex *current = vertices[i];
        Vertex *previous = vertices[(i - 1 + polygon->size) % polygon->size];

        if (current->y > previous->y)
        {
            if (current->y > line && previous->y <= line)
            {
                vCur[countCur++] = previous;
                vCur[countCur++] = current;
            }
        }
        else if (previous->y > line && current->y <= line)
        {
            vCur[countCur++] = current;
            vCur[countCur++] = previous;
        }
    }

    // Don't draw anything if the polygon doesn't intersect with the current line
    if (countCur < 4) return;

    // Calculate the X bounds of the polygon on the current line
    uint32_t x1 = interpolateLinear(vCur[0]->x, vCur[1]->x, vCur[0]->y, line, vCur[1]->y);
    uint32_t x2 = interpolateLinear(vCur[2]->x, vCur[3]->x, vCur[2]->y, line, vCur[3]->y);

    // Swap the bounds if the first one is on the right
    if (x1 > x2)
    {
        SWAP(x1, x2);
        SWAP(vCur[0], vCur[2]);
        SWAP(vCur[1], vCur[3]);
    }

    uint32_t x3 = 0, x4 = 0;

    // Polygons with an alpha value of 0 are rendered as opaque wireframe polygons
    // In this case, calculate the X bounds of the polygon interior so it can be skipped during rendering
    if (polygon->alpha == 0)
    {
        Vertex *vTop[4], *vBot[4];
        int countTop = 0, countBot = 0;

        // Find the polygon edges that intersect with the above and below lines
        // If either line doesn't intersect, the current line is along an edge and can be drawn normally
        for (int i = 0; i < polygon->size && (countTop < 4 || countBot < 4); i++)
        {
            Vertex *current = vertices[i];
            Vertex *previous = vertices[(i - 1 + polygon->size) % polygon->size];

            if (current->y > previous->y)
            {
                if (current->y > line - 1 && previous->y <= line - 1)
                {
                    vTop[countTop++] = previous;
                    vTop[countTop++] = current;
                }
                if (current->y > line + 1 && previous->y <= line + 1)
                {
                    vBot[countBot++] = previous;
                    vBot[countBot++] = current;
                }
            }
            else
            {
                if (previous->y > line - 1 && current->y <= line - 1)
                {
                    vTop[countTop++] = current;
                    vTop[countTop++] = previous;
                }
                if (previous->y > line + 1 && current->y <= line + 1)
                {
                    vBot[countBot++] = current;
                    vBot[countBot++] = previous;
                }
            }
        }

        if (countTop == 4 && countBot == 4) // Both lines intersect
        {
            // Calculate the X bounds of the polygon on the above and below lines
            uint32_t xa = interpolateLinear(vTop[0]->x, vTop[1]->x, vTop[0]->y, line - 1, vTop[1]->y);
            uint32_t xb = interpolateLinear(vTop[2]->x, vTop[3]->x, vTop[2]->y, line - 1, vTop[3]->y);
            uint32_t xc = interpolateLinear(vBot[0]->x, vBot[1]->x, vBot[0]->y, line + 1, vBot[1]->y);
            uint32_t xd = interpolateLinear(vBot[2]->x, vBot[3]->x, vBot[2]->y, line + 1, vBot[3]->y);

            // Swap the bounds if the first one is on the right
            if (xa > xb) SWAP(xa, xb);
            if (xc > xd) SWAP(xc, xd);

            // Set the X bounds of the polygon interior
            // On the left, the polygon will be drawn from the left edge to the point where the edge starts on an adjacent line
            // On the right, the polygon will be drawn from the point where the edge starts on an adjacent line to the right edge
            // For this, the left interior bound should be larger, and the right interior bound should be smaller
            x3 = (xa > xc) ? xa : xc;
            x4 = (xb < xd) ? xb : xd;

            // If the difference between bounds is less than a pixel, adjust them so the edge will still be drawn
            if (x3 <= x1) x3 = x1 + 1;
            if (x4 >= x2) x4 = x2 - 1;
        }
    }

    // Apply W-shift to reduce (or expand) W values to 16 bits
    uint32_t vw[4];
    if (polygon->wShift >= 0)
    {
        for (int i = 0; i < 4; i++)
            vw[i] = vCur[i]->w >> polygon->wShift;
    }
    else if (polygon->wShift < 0)
    {
        for (int i = 0; i < 4; i++)
            vw[i] = vCur[i]->w << -polygon->wShift;
    }

    int e[4];
    uint32_t lx1, lx, lx2;
    uint32_t rx1, rx, rx2;

    // Choose between X and Y coordinates for edge interpolation on the left side (whichever is more precise)
    if (abs(vCur[1]->x - vCur[0]->x) > vCur[1]->y - vCur[0]->y)
    {
        // Reorder the vertices so the greater X coordinate comes last
        if (vCur[1]->x > vCur[0]->x)
        {
            e[0] = 0;
            e[1] = 1;
        }
        else
        {
            e[0] = 1;
            e[1] = 0;
        }

        lx1 = vCur[e[0]]->x;
        lx = x1;
        lx2 = vCur[e[1]]->x;
    }
    else
    {
        e[0] = 0;
        e[1] = 1;

        lx1 = vCur[0]->y;
        lx = line;
        lx2 = vCur[1]->y;
    }

    // Choose between X and Y coordinates for edge interpolation on the right side (whichever is more precise)
    if (abs(vCur[3]->x - vCur[2]->x) > vCur[3]->y - vCur[2]->y)
    {
        // Reorder the vertices so the greater X coordinate comes last
        if (vCur[3]->x > vCur[2]->x)
        {
            e[2] = 2;
            e[3] = 3;
        }
        else
        {
            e[2] = 3;
            e[3] = 2;
        }

        rx1 = vCur[e[2]]->x;
        rx = x2;
        rx2 = vCur[e[3]]->x;
    }
    else
    {
        e[2] = 2;
        e[3] = 3;

        rx1 = vCur[2]->y;
        rx = line;
        rx2 = vCur[3]->y;
    }

    // Calculate the Z values of the polygon edges on the current line
    uint32_t z1 = interpolateLinear(vCur[e[0]]->z, vCur[e[1]]->z, lx1, lx, lx2);
    uint32_t z2 = interpolateLinear(vCur[e[2]]->z, vCur[e[3]]->z, rx1, rx, rx2);

    // Calculate the W values of the polygon edges on the current line
    uint32_t w1 = interpolateEdge(vw[e[0]], vw[e[1]], lx1, lx, lx2, vw[e[0]], vw[e[1]]);
    uint32_t w2 = interpolateEdge(vw[e[2]], vw[e[3]], rx1, rx, rx2, vw[e[2]], vw[e[3]]);

    // Interpolate the vertex color of the polygon edges on the current line
    // The color values are expanded to 9 bits during interpolation for extra precision
    uint32_t r1 = interpolateEdge(((vCur[e[0]]->color >>  0) & 0x3F) << 3, ((vCur[e[1]]->color >>  0) & 0x3F) << 3, lx1, lx, lx2, vw[e[0]], vw[e[1]]);
    uint32_t g1 = interpolateEdge(((vCur[e[0]]->color >>  6) & 0x3F) << 3, ((vCur[e[1]]->color >>  6) & 0x3F) << 3, lx1, lx, lx2, vw[e[0]], vw[e[1]]);
    uint32_t b1 = interpolateEdge(((vCur[e[0]]->color >> 12) & 0x3F) << 3, ((vCur[e[1]]->color >> 12) & 0x3F) << 3, lx1, lx, lx2, vw[e[0]], vw[e[1]]);
    uint32_t r2 = interpolateEdge(((vCur[e[2]]->color >>  0) & 0x3F) << 3, ((vCur[e[3]]->color >>  0) & 0x3F) << 3, rx1, rx, rx2, vw[e[2]], vw[e[3]]);
    uint32_t g2 = interpolateEdge(((vCur[e[2]]->color >>  6) & 0x3F) << 3, ((vCur[e[3]]->color >>  6) & 0x3F) << 3, rx1, rx, rx2, vw[e[2]], vw[e[3]]);
    uint32_t b2 = interpolateEdge(((vCur[e[2]]->color >> 12) & 0x3F) << 3, ((vCur[e[3]]->color >> 12) & 0x3F) << 3, rx1, rx, rx2, vw[e[2]], vw[e[3]]);

    // Interpolate the texture coordinates of the polygon edges on the current line
    int s1 = interpolateEdge(vCur[e[0]]->s + 0xFFFF, vCur[e[1]]->s + 0xFFFF, lx1, lx, lx2, vw[e[0]], vw[e[1]]) - 0xFFFF;
    int s2 = interpolateEdge(vCur[e[2]]->s + 0xFFFF, vCur[e[3]]->s + 0xFFFF, rx1, rx, rx2, vw[e[2]], vw[e[3]]) - 0xFFFF;
    int t1 = interpolateEdge(vCur[e[0]]->t + 0xFFFF, vCur[e[1]]->t + 0xFFFF, lx1, lx, lx2, vw[e[0]], vw[e[1]]) - 0xFFFF;
    int t2 = interpolateEdge(vCur[e[2]]->t + 0xFFFF, vCur[e[3]]->t + 0xFFFF, rx1, rx, rx2, vw[e[2]], vw[e[3]]) - 0xFFFF;

    // Keep track of shadow mask polygons
    if (polygon->mode == 3 && polygon->id == 0) // Shadow mask polygon
    {
        // Clear the stencil buffer at the start of a shadow mask polygon group
        if (!stencilClear[thread])
        {
            memset(stencilBuffer[thread], 0, 256 * sizeof(uint8_t));
            stencilClear[thread] = true;
        }
    }
    else
    {
        // End a shadow mask polygon group
        stencilClear[thread] = false;
    }

    // Draw a line segment
    for (uint32_t x = x1; x < x2; x++)
    {
        // Skip the polygon interior for wireframe polygons
        if (x3 < x4 && x == x3) x = x4;

        // Invalid viewports can cause out-of-bounds vertices, so only draw within bounds
        if (x >= 256) break;

        // Calculate the depth value of the current pixel
        uint32_t depth;
        if (polygon->wBuffer)
        {
            depth = interpolateFill(w1, w2, x1, x, x2, w1, w2);
            if (polygon->wShift > 0)
                depth <<= polygon->wShift;
            else if (polygon->wShift < 0)
                depth >>= -polygon->wShift;
        }
        else
        {
            depth = interpolateLinear(z1, z2, x1, x, x2);
        }

        // Draw a new pixel if the old one is behind the new one
        // The polygon can optionally use an "equal" depth test, which has a margin of 0x200
        if ((polygon->depthTestEqual && depthBuffer[thread][x] + 0x200 >= depth) || depthBuffer[thread][x] > depth)
        {
            // Only render non-mask shadow polygons if the stencil bit is set and the old pixel's polygon ID differs
            if (polygon->mode == 3 && (polygon->id == 0 || !stencilBuffer[thread][x] || (attribBuffer[thread][x] & 0x3F) == polygon->id))
                continue;

            // Interpolate the vertex color at the current pixel
            uint32_t r = interpolateFill(r1, r2, x1, x, x2, w1, w2) >> 3;
            uint32_t g = interpolateFill(g1, g2, x1, x, x2, w1, w2) >> 3;
            uint32_t b = interpolateFill(b1, b2, x1, x, x2, w1, w2) >> 3;
            uint32_t color = ((polygon->alpha ? polygon->alpha : 0x3F) << 18) | (b << 12) | (g << 6) | r;

            // Blend the texture with the vertex color
            if (polygon->textureFmt != 0)
            {
                // Interpolate the texture coordinates at the current pixel
                int s = interpolateFill(s1 + 0xFFFF, s2 + 0xFFFF, x1, x, x2, w1, w2) - 0xFFFF;
                int t = interpolateFill(t1 + 0xFFFF, t2 + 0xFFFF, x1, x, x2, w1, w2) - 0xFFFF;

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
                uint8_t *attrib = &attribBuffer[thread][x];

                if ((disp3DCnt & BIT(3)) && ((color & 0xFC0000) >> 18) < 0x3F) // Alpha blending
                {
                    // Only render transparent pixels if the old pixel isn't transparent or the polygon ID differs
                    if (!(*attrib & BIT(6)) || (*attrib & 0x3F) != polygon->id)
                    {
                        *pixel = BIT(26) | ((*pixel & 0xFC0000) ? interpolateColor(*pixel, color, 0, color >> 18, 63) : color);
                        if (polygon->transNewDepth) depthBuffer[thread][x] = depth;
                        *attrib = (*attrib & (polygon->fog << 7)) | BIT(6) | polygon->id;
                    }
                }
                else
                {
                    *pixel = BIT(26) | color;
                    depthBuffer[thread][x] = depth;
                    *attrib = (polygon->fog << 7) | polygon->id;
                }
            }
        }
        else if (polygon->mode == 3 && polygon->id == 0)
        {
            // Set a stencil buffer bit for shadow mask pixels that fail the depth test
            stencilBuffer[thread][x] = 1;
        }
    }
}

void Gpu3DRenderer::writeDisp3DCnt(uint16_t mask, uint16_t value)
{
    // If any of the error bits are set, acknowledge the errors by clearing them
    if (value & BIT(12)) disp3DCnt &= ~BIT(12);
    if (value & BIT(13)) disp3DCnt &= ~BIT(13);

    // Write to the DISP3DCNT register and invalidate the 3D if a parameter changed
    mask &= 0x4FFF;
    if ((value & mask) == (disp3DCnt & mask)) return;
    disp3DCnt = (disp3DCnt & ~mask) | (value & mask);
    core->gpu.invalidate3D();
}

void Gpu3DRenderer::writeClearColor(uint32_t mask, uint32_t value)
{
    // Write to the CLEAR_COLOR register and invalidate the 3D if a parameter changed
    mask &= 0x3F1FFFFF;
    if ((value & mask) == (clearColor & mask)) return;
    clearColor = (clearColor & ~mask) | (value & mask);
    core->gpu.invalidate3D();
}

void Gpu3DRenderer::writeClearDepth(uint16_t mask, uint16_t value)
{
    // Write to the CLEAR_DEPTH register and invalidate the 3D if a parameter changed
    mask &= 0x7FFF;
    if ((value & mask) == (clearDepth & mask)) return;
    clearDepth = (clearDepth & ~mask) | (value & mask);
    core->gpu.invalidate3D();
}

void Gpu3DRenderer::writeToonTable(int index, uint16_t mask, uint16_t value)
{
    // Write to one of the TOON_TABLE registers and invalidate the 3D if a parameter changed
    mask &= 0x7FFF;
    if ((value & mask) == (toonTable[index] & mask)) return;
    toonTable[index] = (toonTable[index] & ~mask) | (value & mask);
    core->gpu.invalidate3D();
}

void Gpu3DRenderer::writeFogColor(uint32_t mask, uint32_t value)
{
    // Write to the FOG_COLOR register and invalidate the 3D if a parameter changed
    mask &= 0x001F7FFF;
    if ((value & mask) == (fogColor & mask)) return;
    fogColor = (fogColor & ~mask) | (value & mask);
    core->gpu.invalidate3D();
}

void Gpu3DRenderer::writeFogOffset(uint16_t mask, uint16_t value)
{
    // Write to the FOG_OFFSET register and invalidate the 3D if a parameter changed
    mask &= 0x7FFF;
    if ((value & mask) == (fogOffset & mask)) return;
    fogOffset = (fogOffset & ~mask) | (value & mask);
    core->gpu.invalidate3D();
}

void Gpu3DRenderer::writeFogTable(int index, uint8_t value)
{
    // Write to one of the FOG_TABLE registers and invalidate the 3D if a parameter changed
    if ((value & 0x7F) == (fogTable[index] & 0x7F)) return;
    fogTable[index] = value & 0x7F;
    core->gpu.invalidate3D();
}
