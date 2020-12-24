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
        // Calculate the scanline bounds for each polygon
        for (int i = 0; i < core->gpu3D.getPolygonCount(); i++)
        {
            polygonTop[i] = 192;
            polygonBot[i] =   0;

            _Polygon *polygon = &core->gpu3D.getPolygons()[i];
            for (int j = 0; j < polygon->size; j++)
            {
                Vertex *vertex = &polygon->vertices[j];
                if (vertex->y < polygonTop[i]) polygonTop[i] = vertex->y;
                if (vertex->y > polygonBot[i]) polygonBot[i] = vertex->y;
            }

            // Allow horizontal line polygons to be drawn
            if (polygonTop[i] == polygonBot[i]) polygonTop[i]--;
        }

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
    uint32_t depth = (clearDepth << 9) + ((clearDepth == 0x7FFF) ? 0x1FF : 0);
    uint8_t attrib = ((clearColor & BIT(15)) >> 8) | ((clearColor & 0x3F000000) >> 24);
    if ((clearColor & 0x001F0000) && ((clearColor & 0x001F0000) >> 16) < 31) attrib |= BIT(6);

    // Clear the scanline buffers with the clear values
    for (int i = 0; i < 256; i++)
    {
        framebuffer[line * 256 + i] = color;
        depthBuffer[thread][i] = depth;
        attribBuffer[thread][i] = attrib;
    }

    stencilClear[thread] = false;

    std::vector<int> translucent;

    // Draw the polygons
    for (int i = 0; i < core->gpu3D.getPolygonCount(); i++)
    {
        // Skip polygons that aren't on the current scanline
        if (line < polygonTop[i] || line >= polygonBot[i])
            continue;

        // Draw solid polygons and save the translucent ones for later
        _Polygon *polygon = &core->gpu3D.getPolygons()[i];
        if (polygon->alpha < 0x3F || polygon->textureFmt == 1 || polygon->textureFmt == 6)
            translucent.push_back(i);
        else
            drawPolygon(line, thread, i);
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
    if (x <= x1) return v1;
    if (x >= x2) return v2;

    // Linearly interpolate a new value between the min and max values
    if (v1 <= v2)
        return v1 + (v2 - v1) * (x - x1) / (x2 - x1);
    else
        return v2 + (v1 - v2) * (x2 - x) / (x2 - x1);
}

uint32_t Gpu3DRenderer::interpolateLinRev(uint32_t v1, uint32_t v2, uint32_t x1, uint32_t x, uint32_t x2)
{
    if (x <= x1) return v1;
    if (x >= x2) return v2;

    // Linearly interpolate a new value between the min and max values (reverse formula)
    if (v1 <= v2)
        return v2 - (v2 - v1) * (x2 - x) / (x2 - x1);
    else
        return v1 - (v1 - v2) * (x - x1) / (x2 - x1);
}

uint32_t Gpu3DRenderer::interpolateFill(uint32_t v1, uint32_t v2, uint32_t x1, uint32_t x, uint32_t x2, uint32_t w1, uint32_t w2)
{
    if (x <= x1) return v1;
    if (x >= x2) return v2;

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
    if (x <= x1) return v1;
    if (x >= x2) return v2;

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

void Gpu3DRenderer::drawPolygon(int line, int thread, int polygonIndex)
{
    _Polygon *polygon = &core->gpu3D.getPolygons()[polygonIndex];

    // Get the polygon vertices
    Vertex *vertices[10];
    for (int i = 0; i < polygon->size; i++)
        vertices[i] = &polygon->vertices[i];

    // Unclipped quad strip polygons have their vertices crossed, so uncross them
    if (polygon->crossed)
        SWAP(vertices[2], vertices[3]);

    // Find the starting (top-left) vertex
    int start = 0;
    for (int i = 0; i < polygon->size; i++)
    {
        if (vertices[start]->y > vertices[i]->y || (vertices[start]->y == vertices[i]->y && vertices[start]->x > vertices[i]->x))
            start = i;
    }

    // Set the starting edges
    int v[4] =
    {
        start, (start + 1)                 % polygon->size,
        start, (start - 1 + polygon->size) % polygon->size
    };

    // Follow the vertices forwards to the first intersecting edge
    while (vertices[v[1]]->y <= line)
    {
        if (v[1] == start) return; // Full loop, no intersection
        v[0] = v[1];
        v[1] = (v[1] + 1) % polygon->size;
    }

    // Follow the vertices backwards to the first intersecting edge
    while (vertices[v[3]]->y <= line)
    {
        if (v[3] == start) return; // Full loop, no intersection
        v[2] = v[3];
        v[3] = (v[3] - 1 + polygon->size) % polygon->size;
    }

    // Swap the edges depending on polygon orientation
    if (polygon->clockwise)
    {
        SWAP(v[0], v[2]);
        SWAP(v[1], v[3]);
    }

    // Rearrange the vertices so the lower Y values come first
    if (vertices[v[0]]->y > vertices[v[1]]->y) SWAP(v[0], v[1]);
    if (vertices[v[2]]->y > vertices[v[3]]->y) SWAP(v[2], v[3]);

    uint32_t x1, x2, x3, x4;

    // Calculate the left edge bounds
    if (vertices[v[0]]->y == vertices[v[1]]->y) // Horizontal
    {
        x1 = vertices[v[0]]->x;
        x2 = vertices[v[1]]->x;

        // Rearrange the vertices so the lower X values come first
        if (vertices[v[0]]->x > vertices[v[1]]->x)
        {
            SWAP(v[0], v[1]);
            SWAP(x1, x2);
        }
    }
    else if (abs(vertices[v[1]]->x - vertices[v[0]]->x) > vertices[v[1]]->y - vertices[v[0]]->y) // X-major
    {
        // Interpolate with an extra bit of precision so the result can be rounded
        x1 = (interpolateLinear(vertices[v[0]]->x << 1, vertices[v[1]]->x << 1, vertices[v[0]]->y, line,     vertices[v[1]]->y) + 1) >> 1;
        x2 = (interpolateLinear(vertices[v[0]]->x << 1, vertices[v[1]]->x << 1, vertices[v[0]]->y, line + 1, vertices[v[1]]->y) + 1) >> 1;

        // Rearrange the vertices so the lower X values come first
        if (vertices[v[0]]->x > vertices[v[1]]->x)
        {
            SWAP(v[0], v[1]);
            SWAP(x1, x2);
        }
    }
    else // Y-major
    {
        // Interpolate without rounding; since the edge is only one pixel thick, only one interpolation is needed
        x1 = interpolateLinear(vertices[v[0]]->x, vertices[v[1]]->x, vertices[v[0]]->y, line, vertices[v[1]]->y);
        x2 = x1;
    }

    // Calculate the right edge bounds
    if (vertices[v[2]]->y == vertices[v[3]]->y) // Horizontal
    {
        x3 = vertices[v[2]]->x;
        x4 = vertices[v[3]]->x;

        // Rearrange the vertices so the lower X values come first
        if (vertices[v[2]]->x > vertices[v[3]]->x)
        {
            SWAP(v[2], v[3]);
            SWAP(x3, x4);
        }
    }
    else if (vertices[v[2]]->x == vertices[v[3]]->x) // Vertical
    {
        // Unless a line polygon, vertical right edges have their X reduced by 1
        x3 = vertices[v[2]]->x;
        if ((vertices[v[0]]->x != vertices[v[1]]->x || vertices[v[0]]->x != vertices[v[2]]->x) && x3 > 0) x3--;
        x4 = x3;
    }
    else if (abs(vertices[v[3]]->x - vertices[v[2]]->x) > vertices[v[3]]->y - vertices[v[2]]->y) // X-major
    {
        // Interpolate with an extra bit of precision so the result can be rounded
        x3 = (interpolateLinear(vertices[v[2]]->x << 1, vertices[v[3]]->x << 1, vertices[v[2]]->y, line,     vertices[v[3]]->y) + 1) >> 1;
        x4 = (interpolateLinear(vertices[v[2]]->x << 1, vertices[v[3]]->x << 1, vertices[v[2]]->y, line + 1, vertices[v[3]]->y) + 1) >> 1;

        // Rearrange the vertices so the lower X values come first
        if (vertices[v[2]]->x > vertices[v[3]]->x)
        {
            SWAP(v[2], v[3]);
            SWAP(x3, x4);
        }
    }
    else // Y-major
    {
        // Interpolate without rounding; since the edge is only one pixel thick, only one interpolation is needed
        // Note that negative Y-major right edges seem to be interpolated in reverse from other edges
        if (vertices[v[2]]->x > vertices[v[3]]->x)
            x3 = interpolateLinRev(vertices[v[2]]->x, vertices[v[3]]->x, vertices[v[2]]->y, line, vertices[v[3]]->y) - 1;
        else
            x3 = interpolateLinear(vertices[v[2]]->x, vertices[v[3]]->x, vertices[v[2]]->y, line, vertices[v[3]]->y);
        x4 = x3;
    }

    // Reduce the bounds so they don't overlap
    if (x2 > x1) x2--;
    if (x4 > x3) x4--;

    // Handle crossed edges; these become "dotted" if their orientation is wrong
    if (x2 > x3) x2 = x3;
    if (x1 > x2) x2 = x1;
    if (x2 > x3) x3 = x2;
    if (x3 > x4) x3 = x4;

    // Swap the edges so the leftmost one comes first
    if (x1 > x4)
    {
        SWAP(v[0], v[2]);
        SWAP(v[1], v[3]);
        SWAP(x1, x3);
        SWAP(x2, x4);
    }

    uint32_t lx1, lx, lx2;
    uint32_t rx1, rx, rx2;

    // Choose between X and Y coordinates for interpolation on the left (whichever is more precise)
    if (abs(vertices[v[1]]->x - vertices[v[0]]->x) > abs(vertices[v[1]]->y - vertices[v[0]]->y))
    {
        lx1 = vertices[v[0]]->x;
        lx = x1;
        lx2 = vertices[v[1]]->x;
    }
    else
    {
        lx1 = vertices[v[0]]->y;
        lx = line;
        lx2 = vertices[v[1]]->y;
    }

    // Choose between X and Y coordinates for interpolation on the right (whichever is more precise)
    if (abs(vertices[v[3]]->x - vertices[v[2]]->x) > abs(vertices[v[3]]->y - vertices[v[2]]->y))
    {
        rx1 = vertices[v[2]]->x;
        rx = x4;
        rx2 = vertices[v[3]]->x;
    }
    else
    {
        rx1 = vertices[v[2]]->y;
        rx = line;
        rx2 = vertices[v[3]]->y;
    }

    // Apply W-shift to reduce (or expand) W values to 16 bits
    uint32_t ws[4];
    if (polygon->wShift >= 0)
    {
        for (int i = 0; i < 4; i++)
            ws[i] = vertices[v[i]]->w >> polygon->wShift;
    }
    else
    {
        for (int i = 0; i < 4; i++)
            ws[i] = vertices[v[i]]->w << -polygon->wShift;
    }

    // Calculate the Z values of the polygon edges on the current line
    uint32_t z1 = interpolateLinear(vertices[v[0]]->z, vertices[v[1]]->z, lx1, lx, lx2);
    uint32_t z2 = interpolateLinear(vertices[v[2]]->z, vertices[v[3]]->z, rx1, rx, rx2);

    // Calculate the W values of the polygon edges on the current line
    uint32_t w1 = interpolateEdge(ws[0], ws[1], lx1, lx, lx2, ws[0], ws[1]);
    uint32_t w2 = interpolateEdge(ws[2], ws[3], rx1, rx, rx2, ws[2], ws[3]);

    // Interpolate the vertex color of the polygon edges on the current line
    // The color values are expanded to 9 bits during interpolation for extra precision
    uint32_t r1 = interpolateEdge(((vertices[v[0]]->color >>  0) & 0x3F) << 3, ((vertices[v[1]]->color >>  0) & 0x3F) << 3, lx1, lx, lx2, ws[0], ws[1]);
    uint32_t g1 = interpolateEdge(((vertices[v[0]]->color >>  6) & 0x3F) << 3, ((vertices[v[1]]->color >>  6) & 0x3F) << 3, lx1, lx, lx2, ws[0], ws[1]);
    uint32_t b1 = interpolateEdge(((vertices[v[0]]->color >> 12) & 0x3F) << 3, ((vertices[v[1]]->color >> 12) & 0x3F) << 3, lx1, lx, lx2, ws[0], ws[1]);
    uint32_t r2 = interpolateEdge(((vertices[v[2]]->color >>  0) & 0x3F) << 3, ((vertices[v[3]]->color >>  0) & 0x3F) << 3, rx1, rx, rx2, ws[2], ws[3]);
    uint32_t g2 = interpolateEdge(((vertices[v[2]]->color >>  6) & 0x3F) << 3, ((vertices[v[3]]->color >>  6) & 0x3F) << 3, rx1, rx, rx2, ws[2], ws[3]);
    uint32_t b2 = interpolateEdge(((vertices[v[2]]->color >> 12) & 0x3F) << 3, ((vertices[v[3]]->color >> 12) & 0x3F) << 3, rx1, rx, rx2, ws[2], ws[3]);

    // Interpolate the texture coordinates of the polygon edges on the current line
    // Interpolation is unsigned, so temporarily convert the signed values to unsigned
    int32_t s1 = interpolateEdge((int32_t)vertices[v[0]]->s + 0xFFFF, (int32_t)vertices[v[1]]->s + 0xFFFF, lx1, lx, lx2, ws[0], ws[1]) - 0xFFFF;
    int32_t t1 = interpolateEdge((int32_t)vertices[v[0]]->t + 0xFFFF, (int32_t)vertices[v[1]]->t + 0xFFFF, lx1, lx, lx2, ws[0], ws[1]) - 0xFFFF;
    int32_t s2 = interpolateEdge((int32_t)vertices[v[2]]->s + 0xFFFF, (int32_t)vertices[v[3]]->s + 0xFFFF, rx1, rx, rx2, ws[2], ws[3]) - 0xFFFF;
    int32_t t2 = interpolateEdge((int32_t)vertices[v[2]]->t + 0xFFFF, (int32_t)vertices[v[3]]->t + 0xFFFF, rx1, rx, rx2, ws[2], ws[3]) - 0xFFFF;

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

    // Increment the right bound not only for drawing, but for interpolation across the scanline as well
    // This seems to give results accurate to hardware
    x4++;

    // Because edge traversal doesn't consider equal values to be intersecting, it skips horizontal edges
    // Instead, the hardware simply considers the entire span across the top and bottom of a polygon to be an edge
    if (line == polygonTop[polygonIndex] || line == polygonBot[polygonIndex] - 1)
        x2 = x3 = x4;

    // Draw a line segment
    for (uint32_t x = x1; x < x4; x++)
    {
        // Skip the polygon interior for wireframe polygons
        if (polygon->alpha == 0 && x == x2 + 1 && x3 > x2) x = x3;

        // Invalid viewports can cause out-of-bounds vertices, so only draw within bounds
        if (x >= 256) break;

        // Calculate the depth value of the current pixel
        uint32_t depth;
        if (polygon->wBuffer)
        {
            depth = interpolateFill(w1, w2, x1, x, x4, w1, w2);
            if (polygon->wShift > 0)
                depth <<= polygon->wShift;
            else if (polygon->wShift < 0)
                depth >>= -polygon->wShift;
            depth &= 0xFFFFFF;
        }
        else
        {
            depth = interpolateLinear(z1, z2, x1, x, x4);
        }

        // Draw a new pixel if the old one is behind the new one
        // The polygon can optionally use an "equal" depth test, which has a margin of 0x200
        if ((polygon->depthTestEqual && depthBuffer[thread][x] + 0x200 >= depth) || depthBuffer[thread][x] > depth)
        {
            // Only render non-mask shadow polygons if the stencil bit is set and the old pixel's polygon ID differs
            if (polygon->mode == 3 && (polygon->id == 0 || !stencilBuffer[thread][x] || (attribBuffer[thread][x] & 0x3F) == polygon->id))
                continue;

            // Interpolate the vertex color at the current pixel
            uint32_t r = interpolateFill(r1, r2, x1, x, x4, w1, w2) >> 3;
            uint32_t g = interpolateFill(g1, g2, x1, x, x4, w1, w2) >> 3;
            uint32_t b = interpolateFill(b1, b2, x1, x, x4, w1, w2) >> 3;
            uint32_t color = ((polygon->alpha ? polygon->alpha : 0x3F) << 18) | (b << 12) | (g << 6) | r;

            // Blend the texture with the vertex color
            if (polygon->textureFmt != 0)
            {
                // Interpolate the texture coordinates at the current pixel
                int s = interpolateFill(s1 + 0xFFFF, s2 + 0xFFFF, x1, x, x4, w1, w2) - 0xFFFF;
                int t = interpolateFill(t1 + 0xFFFF, t2 + 0xFFFF, x1, x, x4, w1, w2) - 0xFFFF;

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
            else if (polygon->mode == 2) // Toon/Highlight (no texture)
            {
                uint32_t toon = rgba5ToRgba6(toonTable[(color & 0x3F) / 2]);
                uint8_t r, g, b;

                if (disp3DCnt & BIT(1)) // Highlight
                {
                    r = ((color >>  0) & 0x3F) + ((toon >>  0) & 0x3F); if (r > 63) r = 63;
                    g = ((color >>  6) & 0x3F) + ((toon >>  6) & 0x3F); if (g > 63) g = 63;
                    b = ((color >> 12) & 0x3F) + ((toon >> 12) & 0x3F); if (b > 63) b = 63;
                }
                else // Toon
                {
                    r = ((toon >>  0) & 0x3F);
                    g = ((toon >>  6) & 0x3F);
                    b = ((toon >> 12) & 0x3F);
                }

                color = (color & 0xFC0000) | (b << 12) | (g << 6) | r;
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
