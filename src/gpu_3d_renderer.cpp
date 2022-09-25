/*
    Copyright 2019-2022 Hydr8gon

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
    for (int i = 0; i < 192 * 2; i++)
        ready[i].store(3);
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

uint32_t *Gpu3DRenderer::getLine(int line)
{
    // Get 2 lines when high-res is enabled, to ensure they're both finished
    if (resShift)
    {
        uint32_t *data = getLine1(line * 2);
        getLine1(line * 2 + 1);
        return data;
    }

    return getLine1(line);
}

uint32_t *Gpu3DRenderer::getLine1(int line)
{
    // If a thread is falling behind, see if this thread can help out instead of waiting around
    // Threads go back for the final pass after drawing their next scanline, so check 2 scanlines ahead
    if (ready[line].load() < 3 && line + activeThreads * 2 < 192)
    {
        int next = line + activeThreads * 2;
        switch (ready[next].exchange(1))
        {
            case 0:
                // Draw the scanline if it hasn't been started yet
                drawScanline1(next);
                ready[next].store(2);
                break;

            case 2:
                // If the thread somehow caught up, restore the scanline's state
                if (ready[next].exchange(2) == 3)
                    ready[next].store(3);
                break;

            case 3:
                // If the thread somehow caught up, restore the scanline's state
                ready[next].store(3);
                break;
        }
    }

    // Wait until a scanline is ready, and then return it
    while (ready[line].load() < 3) std::this_thread::yield();
    return &framebuffer[0][line * 256 * 2];
}

void Gpu3DRenderer::drawScanline(int line)
{
    if (line == 0)
    {
        // Calculate the scanline bounds for each polygon
        for (int i = 0; i < core->gpu3D.getPolygonCount(); i++)
        {
            polygonTop[i] = 192 * 2;
            polygonBot[i] =   0 * 2;

            _Polygon *polygon = &core->gpu3D.getPolygons()[i];
            for (int j = 0; j < polygon->size; j++)
            {
                Vertex *vertex = &polygon->vertices[j];
                if (vertex->y < polygonTop[i]) polygonTop[i] = vertex->y;
                if (vertex->y > polygonBot[i]) polygonBot[i] = vertex->y;
            }

            // Allow horizontal line polygons to be drawn
            if (polygonTop[i] == polygonBot[i]) polygonBot[i]++;
        }

        // Update the resolution shift for the next frame
        resShift = Settings::getHighRes3D();

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
            int end = 192 << resShift;
            for (int i = 0; i < end; i++)
                ready[i].store(0);

            // Create threads to draw the scanlines
            for (int i = 0; i < activeThreads; i++)
                threads[i] = new std::thread(&Gpu3DRenderer::drawThreaded, this, i);
        }
    }

    // Draw scanlines normally when threading is disabled
    if (activeThreads == 0)
    {
        if (resShift)
        {
            // Draw two scanlines at a time when high-res is enabled
            for (int i = line * 2; i < line * 2 + 2; i++)
            {
                drawScanline1(i);
                if (i > 0) finishScanline(i - 1);
                if (i == 383) finishScanline(383);
            }
        }
        else
        {
            // Draw one scanline at a time
            drawScanline1(line);
            if (line > 0) finishScanline(line - 1);
            if (line == 191) finishScanline(191);
        }
    }
}

void Gpu3DRenderer::drawThreaded(int thread)
{
    // Draw the 3D scanlines in a threaded sequence
    // The amount of scanlines skipped per thread depends on the number of active threads
    // Together, they render the entire 3D image
    int i, end = 192 << resShift;
    for (i = thread; i < end; i += activeThreads)
    {
        switch (ready[i].exchange(1))
        {
            case 0:
                // Draw a scanline if it hasn't been started, save for the final pass
                drawScanline1(i);
                ready[i].store(2);
                break;

            case 2:
                // Restore the scanline's state if it was already drawn
                ready[i].store(2);
                break;
        }

        if (i < activeThreads) continue;
        int prev = i - activeThreads;

        // Wait for this thread's previous scanline and its surrounding scanlines to be drawn
        while ((prev > 0 && ready[prev - 1].load() < 2) || ready[prev].load() < 2 || ready[prev + 1].load() < 2)
            std::this_thread::yield();

        // Finish this thread's previous scanline
        finishScanline(prev);
        ready[prev].store(3);
    }

    int prev = i - activeThreads;

    // Wait for this thread's final scanline and its surrounding scanlines to be drawn
    while (ready[prev - 1].load() < 2 || ready[prev].load() < 2 || (prev < 191 && ready[prev + 1].load() < 2))
        std::this_thread::yield();

    // Finish this thread's final scanline
    finishScanline(prev);
    ready[prev].store(3);
}

void Gpu3DRenderer::drawScanline1(int line)
{
    // Convert the clear values
    // The attribute buffer contains the polygon IDs (0-5, 6-11), transparency bit (12), fog bit (13), edge bit (14), and edge alpha (15-20)
    uint32_t color = BIT(26) | rgba5ToRgba6(((clearColor & 0x001F0000) >> 1) | (clearColor & 0x00007FFF));
    int32_t depth = (clearDepth == 0x7FFF) ? 0xFFFFFF : (clearDepth << 9);
    uint32_t attrib = ((clearColor & BIT(15)) >> 2) | ((clearColor & 0x3F000000) >> 18) | ((clearColor & 0x3F000000) >> 24) |
        (0x3F << 15) | (((clearColor & 0x001F0000) && ((clearColor & 0x001F0000) >> 16) < 31) << 12);

    // Clear the scanline buffers with the clear values
    int start = line * 256 * 2, end = start + (256 << resShift);
    for (int i = start; i < end; i++)
    {
        framebuffer[0][i]  = color;
        depthBuffer[0][i]  = depth;
        attribBuffer[0][i] = attrib;
    }

    stencilClear[line] = false;

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
            drawPolygon(line, i);
    }

    // Draw the translucent polygons
    for (unsigned int i = 0; i < translucent.size(); i++)
        drawPolygon(line, translucent[i]);
}

void Gpu3DRenderer::finishScanline(int line)
{
    // Perform edge marking if enabled
    if (disp3DCnt & BIT(5))
    {
        int offset = line * 256 * 2;
        int w = (256 << resShift) - 1;
        int h = (192 << resShift) - 1;

        for (int i = offset; i <= offset + w; i++)
        {
            if (attribBuffer[0][i] & BIT(14)) // Edge bit
            {
                // Get the polygon IDs of the surrounding pixels
                uint32_t id[4] =
                {
                    (((i & w) > 0) ? attribBuffer[0][i -   1] : (clearColor >> 24)) & 0x3F, // Left
                    (((i & w) < w) ? attribBuffer[0][i +   1] : (clearColor >> 24)) & 0x3F, // Right
                    ((line    > 0) ? attribBuffer[0][i - 512] : (clearColor >> 24)) & 0x3F, // Up
                    ((line    < h) ? attribBuffer[0][i + 512] : (clearColor >> 24)) & 0x3F  // Down
                };

                // Get the depth values of the surrounding pixels
                int32_t depth[4] =
                {
                    (((i & w) > 0) ? depthBuffer[0][i -   1] : ((clearDepth == 0x7FFF) ? 0xFFFFFF : (clearDepth << 9))), // Left
                    (((i & w) < w) ? depthBuffer[0][i +   1] : ((clearDepth == 0x7FFF) ? 0xFFFFFF : (clearDepth << 9))), // Right
                    ((line    > 0) ? depthBuffer[0][i - 512] : ((clearDepth == 0x7FFF) ? 0xFFFFFF : (clearDepth << 9))), // Up
                    ((line    < h) ? depthBuffer[0][i + 512] : ((clearDepth == 0x7FFF) ? 0xFFFFFF : (clearDepth << 9)))  // Down
                };

                // Check the surrounding pixels, and mark the edge if at least one has a different ID and greater depth
                for (int j = 0; j < 4; j++)
                {
                    if ((attribBuffer[0][i] & 0x3F) != id[j] && depthBuffer[0][i] < depth[j])
                    {
                        framebuffer[0][i] = BIT(26) | rgba5ToRgba6((0x1F << 15) | edgeColor[(attribBuffer[0][i] & 0x3F) >> 3]);
                        attribBuffer[0][i] = (attribBuffer[0][i] & ~(0x3F << 15)) | (0x20 << 15);
                        break;
                    }
                }
            }
        }
    }

    // Draw fog if enabled
    if (disp3DCnt & BIT(7))
    {
        uint32_t fog = rgba5ToRgba6(((fogColor & 0x001F0000) >> 1) | (fogColor & 0x00007FFF));
        int fogStep = 0x400 >> ((disp3DCnt & 0x0F00) >> 8);

        for (int layer = 0; layer < ((disp3DCnt & BIT(4)) ? 2 : 1); layer++) // Apply to the back layer as well if anti-aliased
        {
            int start = line * 256 * 2, end = start + (256 << resShift);
            for (int i = start; i < end; i++)
            {
                if (attribBuffer[layer][i] & BIT(13)) // Fog bit
                {
                    // Determine the fog table index for the current pixel's depth
                    int32_t offset = ((depthBuffer[layer][i] / 0x200) - fogOffset);
                    int n = (fogStep > 0) ? (offset / fogStep - 1) : ((offset > 0) ? 31 : 0);

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

                    if (density == 127)
                        density++;

                    // Blend the fog with the pixel
                    uint8_t a = (((fog >> 18) & 0x3F) * density + ((framebuffer[layer][i] >> 18) & 0x3F) * (128 - density)) / 128;
                    if (disp3DCnt & BIT(6)) // Only alpha
                    {
                        framebuffer[layer][i] = (framebuffer[layer][i] & ~(0x3F << 18)) | (a << 18);
                    }
                    else
                    {
                        uint8_t r = (((fog >>  0) & 0x3F) * density + ((framebuffer[layer][i] >>  0) & 0x3F) * (128 - density)) / 128;
                        uint8_t g = (((fog >>  6) & 0x3F) * density + ((framebuffer[layer][i] >>  6) & 0x3F) * (128 - density)) / 128;
                        uint8_t b = (((fog >> 12) & 0x3F) * density + ((framebuffer[layer][i] >> 12) & 0x3F) * (128 - density)) / 128;
                        framebuffer[layer][i] = BIT(26) | (a << 18) | (b << 12) | (g << 6) | r;
                    }
                }
            }
        }
    }

    // Perform anti-aliasing if enabled
    if (disp3DCnt & BIT(4))
    {
        int start = line * 256 * 2, end = start + (256 << resShift);
        for (int i = start; i < end; i++)
        {
            if (((attribBuffer[0][i] >> 15) & 0x3F) < 0x3F) // Edge not opaque
            {
                // Blend with the lower pixel, or simply set the alpha if the lower pixel has alpha 0
                if ((framebuffer[1][i] >> 18) & 0x3F)
                    framebuffer[0][i] = BIT(26) | interpolateColor(framebuffer[1][i], framebuffer[0][i], 0, ((attribBuffer[0][i] >> 15) & 0x3F), 0x3F);
                else
                    framebuffer[0][i] = (framebuffer[0][i] & ~0xFC0000) | ((attribBuffer[0][i] & 0x1F8000) << 3);
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

uint32_t Gpu3DRenderer::interpolateFactor(uint32_t factor, uint32_t shift, uint32_t v1, uint32_t v2)
{
    // Interpolate a new value between the min and max values using a factor
    if (v1 <= v2)
        return v1 + (((v2 - v1) * factor) >> shift);
    else
        return v2 + (((v1 - v2) * ((1 << shift) - factor)) >> shift);
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
        // Flip the S-coordinate every second repeat
        if (polygon->flipS && (s & polygon->sizeS))
            s = -1 - s;

        // Wrap the S-coordinate
        s &= polygon->sizeS - 1;
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
        if (polygon->flipT && (t & polygon->sizeT))
            t = -1 - t;

        // Wrap the T-coordinate
        t &= polygon->sizeT - 1;
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
            if (!(data = getTexture(address))) return 0;
            uint16_t palBase = U8TO16(data, tile * 2);
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

void Gpu3DRenderer::drawPolygon(int line, int polygonIndex)
{
    _Polygon *polygon = &core->gpu3D.getPolygons()[polygonIndex];

    // Get the polygon vertices
    Vertex *vertices[10];
    for (int i = 0; i < polygon->size; i++)
        vertices[i] = &polygon->vertices[i];

    // Unclipped quad strip polygons have their vertices crossed, so uncross them
    if (polygon->crossed)
        SWAP(vertices[2], vertices[3]);

    // Find the starting (top) vertex
    int start = 0;
    for (int i = 0; i < polygon->size; i++)
    {
        if (vertices[start]->y > vertices[i]->y)
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
        v[0] = v[1];
        v[1] = (v[1] + 1) % polygon->size;
        if (v[0] == start) break; // Full loop, no intersection
    }

    // Follow the vertices backwards to the first intersecting edge
    while (vertices[v[3]]->y <= line)
    {
        v[2] = v[3];
        v[3] = (v[3] - 1 + polygon->size) % polygon->size;
        if (v[2] == start) break; // Full loop, no intersection
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
    uint32_t x1a = 0x3F, x2a = 0x3F, x3a = 0x3F, x4a = 0x3F;

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
        x1 = interpolateLinear(vertices[v[0]]->x << 1, vertices[v[1]]->x << 1, vertices[v[0]]->y, line,     vertices[v[1]]->y) + 1;
        x2 = interpolateLinear(vertices[v[0]]->x << 1, vertices[v[1]]->x << 1, vertices[v[0]]->y, line + 1, vertices[v[1]]->y) + 1;

        bool negative = (vertices[v[0]]->x > vertices[v[1]]->x);

        // Rearrange the vertices so the lower X values come first
        if (negative)
        {
            SWAP(v[0], v[1]);
            SWAP(x1, x2);
        }

        // Calculate the edge alpha values if anti-aliasing is enabled
        if (disp3DCnt & BIT(4))
        {
            x1a = interpolateLinear(vertices[v[0]]->y << 6, vertices[v[1]]->y << 6, vertices[v[0]]->x << 1, x1,     vertices[v[1]]->x << 1) & 0x3F;
            x2a = interpolateLinear(vertices[v[0]]->y << 6, vertices[v[1]]->y << 6, vertices[v[0]]->x << 1, x2 - 2, vertices[v[1]]->x << 1) & 0x3F;

            if (negative)
            {
                x1a = 0x3F - x1a;
                x2a = 0x3F - x2a;
            }
        }

        x1 >>= 1;
        x2 >>= 1;
    }
    else // Y-major
    {
        // Interpolate with extra precision for edge alpha calculation
        // Note that negative Y-major edges seem to be interpolated in reverse from positive ones
        if (vertices[v[0]]->x > vertices[v[1]]->x)
            x1 = interpolateLinRev(vertices[v[0]]->x << 6, vertices[v[1]]->x << 6, vertices[v[0]]->y, line, vertices[v[1]]->y) - 1;
        else
            x1 = interpolateLinear(vertices[v[0]]->x << 6, vertices[v[1]]->x << 6, vertices[v[0]]->y, line, vertices[v[1]]->y);

        // Set the edge alpha values if anti-aliasing is enabled
        if (disp3DCnt & BIT(4))
        {
            if (abs(vertices[v[1]]->x - vertices[v[0]]->x) == vertices[v[1]]->y - vertices[v[0]]->y)
                x2a = x1a = 0x20;
            else
                x2a = x1a = 0x3F - (x1 & 0x3F);
        }

        x2 = x1 >>= 6;
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
        x3 = interpolateLinear(vertices[v[2]]->x << 1, vertices[v[3]]->x << 1, vertices[v[2]]->y, line,     vertices[v[3]]->y) + 1;
        x4 = interpolateLinear(vertices[v[2]]->x << 1, vertices[v[3]]->x << 1, vertices[v[2]]->y, line + 1, vertices[v[3]]->y) + 1;

        bool negative = (vertices[v[2]]->x > vertices[v[3]]->x);

        // Rearrange the vertices so the lower X values come first
        if (negative)
        {
            SWAP(v[2], v[3]);
            SWAP(x3, x4);
        }

        // Calculate the edge alpha values if anti-aliasing is enabled
        if (disp3DCnt & BIT(4))
        {
            x3a = interpolateLinear(vertices[v[2]]->y << 6, vertices[v[3]]->y << 6, vertices[v[2]]->x << 1, x3,     vertices[v[3]]->x << 1) & 0x3F;
            x4a = interpolateLinear(vertices[v[2]]->y << 6, vertices[v[3]]->y << 6, vertices[v[2]]->x << 1, x4 - 2, vertices[v[3]]->x << 1) & 0x3F;

            if (!negative)
            {
                x3a = 0x3F - x3a;
                x4a = 0x3F - x4a;
            }
        }

        x3 >>= 1;
        x4 >>= 1;
    }
    else // Y-major
    {
        // Interpolate with extra precision for edge alpha calculation
        // Note that negative Y-major edges seem to be interpolated in reverse from positive ones
        if (vertices[v[2]]->x > vertices[v[3]]->x)
            x3 = interpolateLinRev(vertices[v[2]]->x << 6, vertices[v[3]]->x << 6, vertices[v[2]]->y, line, vertices[v[3]]->y) - 1;
        else
            x3 = interpolateLinear(vertices[v[2]]->x << 6, vertices[v[3]]->x << 6, vertices[v[2]]->y, line, vertices[v[3]]->y);

        // Set the edge alpha values if anti-aliasing is enabled
        if (disp3DCnt & BIT(4))
        {
            if (abs(vertices[v[3]]->x - vertices[v[2]]->x) == vertices[v[3]]->y - vertices[v[2]]->y)
                x4a = x3a = 0x20;
            else
                x4a = x3a = x3 & 0x3F;
        }

        x4 = x3 >>= 6;
    }

    // Reduce the bounds so they don't overlap
    if (x2 > x1) x2--;
    if (x4 > x3) x4--;

    // Handle crossed edges; these become "dotted" if their orientation is wrong
    if (x2 > x3) { x2 = x3;                                     }
    if (x1 > x2) { x2 = x1; x1a = 0x3F - x1a; x2a = 0x3F - x2a; }
    if (x2 > x3) { x3 = x2;                                     }
    if (x3 > x4) { x3 = x4; x3a = 0x3F - x3a; x4a = 0x3F - x4a; }

    // Swap the edges so the leftmost one comes first
    if (x1 > x4)
    {
        SWAP(v[0], v[2]);
        SWAP(v[1], v[3]);
        SWAP(x1, x3);
        SWAP(x2, x4);
        SWAP(x1a, x3a);
        SWAP(x2a, x4a);
    }

    uint32_t xe1[2], xe[2], xe2[2];
    bool hideLeft, hideRight;

    // Choose between X and Y coordinates for interpolation on the left (whichever is more precise)
    if (abs(vertices[v[1]]->x - vertices[v[0]]->x) > abs(vertices[v[1]]->y - vertices[v[0]]->y)) // X-major
    {
        xe1[0] = vertices[v[0]]->x;
        xe[0] = x1;
        xe2[0] = vertices[v[1]]->x;
        hideLeft = (vertices[v[0]]->y < vertices[v[1]]->y);
    }
    else // Y-major
    {
        xe1[0] = vertices[v[0]]->y;
        xe[0] = line;
        xe2[0] = vertices[v[1]]->y;
        hideLeft = false;
    }

    // Choose between X and Y coordinates for interpolation on the right (whichever is more precise)
    if (abs(vertices[v[3]]->x - vertices[v[2]]->x) > abs(vertices[v[3]]->y - vertices[v[2]]->y)) // X-major
    {
        xe1[1] = vertices[v[2]]->x;
        xe[1] = x4;
        xe2[1] = vertices[v[3]]->x;
        hideRight = (vertices[v[2]]->y > vertices[v[3]]->y);
    }
    else // Y-major
    {
        xe1[1] = vertices[v[2]]->y;
        xe[1] = line;
        xe2[1] = vertices[v[3]]->y;
        hideRight = (vertices[v[2]]->x != vertices[v[3]]->x);
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

    uint32_t ze[2], we[2];
    uint32_t re[2], ge[2], be[2];
    uint32_t se[2], te[2];

    // Interpolate values along the left and right polygon edges for the current line
    for (int i = 0; i < 2; i++)
    {
        int i2 = i * 2;

        // Linearly interpolate the Z value of a polygon edge
        ze[i] = interpolateLinear(vertices[v[i2]]->z, vertices[v[i2 + 1]]->z, xe1[i], xe[i], xe2[i]);
        
        // Fall back to linear interpolation if the W values are equal and their lower bits are clear
        if (ws[i2] == ws[i2 + 1] && !(ws[i2] & 0x00FE))
        {
            // Linearly interpolate the W value of a polygon edge
            we[i] = interpolateLinear(ws[i2], ws[i2 + 1], xe1[i], xe[i], xe2[i]);

            // Linearly interpolate the vertex color of a polygon edge
            // The color values are expanded to 9 bits during interpolation for extra precision
            re[i] = interpolateLinear(((vertices[v[i2]]->color >>  0) & 0x3F) << 3, ((vertices[v[i2 + 1]]->color >>  0) & 0x3F) << 3, xe1[i], xe[i], xe2[i]);
            ge[i] = interpolateLinear(((vertices[v[i2]]->color >>  6) & 0x3F) << 3, ((vertices[v[i2 + 1]]->color >>  6) & 0x3F) << 3, xe1[i], xe[i], xe2[i]);
            be[i] = interpolateLinear(((vertices[v[i2]]->color >> 12) & 0x3F) << 3, ((vertices[v[i2 + 1]]->color >> 12) & 0x3F) << 3, xe1[i], xe[i], xe2[i]);

            // Linearly interpolate the texture coordinates of a polygon edge
            // Interpolation is unsigned, so temporarily convert the signed values to unsigned
            se[i] = interpolateLinear((int32_t)vertices[v[i2]]->s + 0xFFFF, (int32_t)vertices[v[i2 + 1]]->s + 0xFFFF, xe1[i], xe[i], xe2[i]) - 0xFFFF;
            te[i] = interpolateLinear((int32_t)vertices[v[i2]]->t + 0xFFFF, (int32_t)vertices[v[i2 + 1]]->t + 0xFFFF, xe1[i], xe[i], xe2[i]) - 0xFFFF;
        }
        else
        {
            // Calculate the interpolation factor with a precision of 9 bits for polygon edges
            uint32_t factor;
            if (xe[i] <= xe1[i])
            {
                // Clamp to the minimum value
                factor = 0;
            }
            else if (xe[i] >= xe2[i])
            {
                // Clamp to the maximum value
                factor = (1 << 9);
            }
            else
            {
                // Adjust the W values to be 15-bit so the calculation doesn't overflow
                // Also adjust interpolation precision to avoid overflow in high-res mode
                uint32_t wa = (ws[i2] >> 1) + ((ws[i2] & 1) && !(ws[i2 + 1] & 1));
                uint32_t s = (resShift & ((xe[i] - xe1[i]) >> 8));
                factor = ((((ws[i2] >> 1) * (xe[i] - xe1[i])) << (9 - s)) /
                    ((ws[i2 + 1] >> 1) * (xe2[i] - xe[i]) + wa * (xe[i] - xe1[i]))) << s;
            }

            // Interpolate the W value of a polygon edge using a factor
            we[i] = interpolateFactor(factor, 9, ws[i2], ws[i2 + 1]);

            // Interpolate the vertex color of a polygon edge using a factor
            // The color values are expanded to 9 bits during interpolation for extra precision
            re[i] = interpolateFactor(factor, 9, ((vertices[v[i2]]->color >>  0) & 0x3F) << 3, ((vertices[v[i2 + 1]]->color >>  0) & 0x3F) << 3);
            ge[i] = interpolateFactor(factor, 9, ((vertices[v[i2]]->color >>  6) & 0x3F) << 3, ((vertices[v[i2 + 1]]->color >>  6) & 0x3F) << 3);
            be[i] = interpolateFactor(factor, 9, ((vertices[v[i2]]->color >> 12) & 0x3F) << 3, ((vertices[v[i2 + 1]]->color >> 12) & 0x3F) << 3);

            // Interpolate the texture coordinates of a polygon edge using a factor
            // Interpolation is unsigned, so temporarily convert the signed values to unsigned
            se[i] = interpolateFactor(factor, 9, (int32_t)vertices[v[i2]]->s + 0xFFFF, (int32_t)vertices[v[i2 + 1]]->s + 0xFFFF) - 0xFFFF;
            te[i] = interpolateFactor(factor, 9, (int32_t)vertices[v[i2]]->t + 0xFFFF, (int32_t)vertices[v[i2 + 1]]->t + 0xFFFF) - 0xFFFF;
        }
    }

    // Keep track of shadow mask polygons
    if (polygon->mode == 3 && polygon->id == 0) // Shadow mask polygon
    {
        // Clear the stencil buffer at the start of a shadow mask polygon group
        if (!stencilClear[line])
        {
            memset(&stencilBuffer[line * 256 * 2], 0, 256 << resShift);
            stencilClear[line] = true;
        }
    }
    else
    {
        // End a shadow mask polygon group
        stencilClear[line] = false;
    }

    // Increment the right bound not only for drawing, but for interpolation across the scanline as well
    // This seems to give results accurate to hardware
    uint32_t x1e = x1, x4e = ++x4;

    // Set special bounds that hide some edges for opaque pixels with no edge effects
    if (polygon->alpha != 0 && !(disp3DCnt & (BIT(4) | BIT(5))))
    {
        if (hideLeft)  x1e = x2 + 1;
        if (hideRight) x4e = x3;
        if (!(hideLeft && hideRight) && x4e <= x1e)
            x4e = x1e + 1;
    }

    // Because edge traversal doesn't consider equal values to be intersecting, it skips horizontal edges
    // Instead, simply consider the entire span across the top and bottom of a polygon to be an edge
    bool horizontal = (line == polygonTop[polygonIndex] || line == polygonBot[polygonIndex] - 1);

    // Draw a line segment
    for (uint32_t x = x1; x < x4; x++)
    {
        // Skip the polygon interior for wireframe polygons
        if (!horizontal && polygon->alpha == 0 && x == x2 + 1 && x3 > x2)
            x = x3;

        // Invalid viewports can cause out-of-bounds vertices, so only draw within bounds
        if (x >= (256 << resShift))
            break;

        bool layer = 0;
        int i = line * 256 * 2 + x;

        // Calculate the interpolation factor with a precision of 8 bits for polygon fills
        uint32_t factor;
        if (we[0] == we[1] && !(we[0] & 0x007F))
        {
            // Fall back to linear interpolation if the W values are equal and their lower bits are clear
            factor = -1;
        }
        else if (x <= x1)
        {
            // Clamp to the minimum value
            factor = 0;
        }
        else if (x >= x4)
        {
            // Clamp to the maximum value
            factor = (1 << 8);
        }
        else
        {
            // Adjust interpolation precision to avoid overflow in high-res mode
            uint32_t s = (resShift & ((x - x1) >> 8));
            factor = (((we[0] * (x - x1)) << (8 - s)) / (we[1] * (x4 - x) + we[0] * (x - x1))) << s;
        }

        // Calculate the depth value of the current pixel
        int32_t depth;
        if (polygon->wBuffer)
        {
            depth = (factor == -1) ? interpolateLinear(we[0], we[1], x1, x, x4) :
                interpolateFactor(factor, 8, we[0], we[1]);
            if (polygon->wShift > 0)
                depth <<= polygon->wShift;
            else if (polygon->wShift < 0)
                depth >>= -polygon->wShift;
        }
        else
        {
            depth = interpolateLinear(ze[0], ze[1], x1, x, x4);
        }

        // Depth test the pixel on the front layer, and on the back layer if under an anti-aliased edge
        bool depthPass[2];
        if (polygon->depthTestEqual)
        {
            uint32_t margin = (polygon->wBuffer ? 0xFF : 0x200);
            depthPass[0] = (depthBuffer[0][i] >= depth - margin && depthBuffer[0][i] <= depth + margin);
            depthPass[1] = (disp3DCnt & BIT(4)) && (attribBuffer[0][i] & BIT(14)) &&
                (depthBuffer[1][i] >= depth - margin && depthBuffer[1][i] <= depth + margin);
        }
        else
        {
            depthPass[0] = (depthBuffer[0][i] > depth);
            depthPass[1] = (disp3DCnt & BIT(4)) && (attribBuffer[0][i] & BIT(14)) && (depthBuffer[1][i] > depth);
        }

        // Check if the pixel should be drawn
        if (polygon->mode == 3) // Shadow
        {
            if (polygon->id == 0) // Mask
            {
                // Don't render shadow mask pixels, but set a stencil buffer bit for ones that fail the depth test
                if (!depthPass[0]) stencilBuffer[i] |= BIT(0);
                if (!depthPass[1]) stencilBuffer[i] |= BIT(1);
                continue;
            }
            else if (!depthPass[0] || !(stencilBuffer[i] & BIT(0)) || (attribBuffer[0][i] & 0x3F) == polygon->id)
            {
                // Only render non-mask shadow pixels if the stencil bit is set and the old pixel's polygon ID differs
                if (!depthPass[1] || !(stencilBuffer[i] & BIT(1)) || (attribBuffer[1][i] & 0x3F) == polygon->id)
                    continue;

                // Draw the pixel on the back layer
                layer = 1;
            }
        }
        else if (!depthPass[0])
        {
            // Only render normal pixels if the depth test passes
            if (!depthPass[1])
                continue;

            // Draw the pixel on the back layer
            layer = 1;
        }

        // Interpolate the vertex color at the current pixel
        uint32_t rv, gv, bv;
        if (factor == -1)
        {
            rv = interpolateLinear(re[0], re[1], x1, x, x4) >> 3;
            gv = interpolateLinear(ge[0], ge[1], x1, x, x4) >> 3;
            bv = interpolateLinear(be[0], be[1], x1, x, x4) >> 3;
        }
        else
        {
            rv = interpolateFactor(factor, 8, re[0], re[1]) >> 3;
            gv = interpolateFactor(factor, 8, ge[0], ge[1]) >> 3;
            bv = interpolateFactor(factor, 8, be[0], be[1]) >> 3;
        }
        uint32_t color = ((polygon->alpha ? polygon->alpha : 0x3F) << 18) | (bv << 12) | (gv << 6) | rv;

        // Blend the texture with the vertex color
        if (polygon->textureFmt != 0)
        {
            // Interpolate the texture coordinates at the current pixel
            int s, t;
            if (factor == -1)
            {
                s = interpolateLinear(se[0] + 0xFFFF, se[1] + 0xFFFF, x1, x, x4) - 0xFFFF;
                t = interpolateLinear(te[0] + 0xFFFF, te[1] + 0xFFFF, x1, x, x4) - 0xFFFF;
            }
            else
            {
                s = interpolateFactor(factor, 8, se[0] + 0xFFFF, se[1] + 0xFFFF) - 0xFFFF;
                t = interpolateFactor(factor, 8, te[0] + 0xFFFF, te[1] + 0xFFFF) - 0xFFFF;
            }

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

        // Skip fully transparent pixels, and hidden edge pixels if the pixel is opaque or blending is disabled
        if (!(color & 0xFC0000) || ((x < x1e || x >= x4e) && ((color >> 18) == 0x3F || !(disp3DCnt & BIT(3)))))
            continue;

        // Draw a pixel, marked with an extra bit as an indicator for 2D blending
        if ((color >> 18) == 0x3F) // Opaque
        {
            uint8_t edgeAlpha = 0x3F;
            bool edge = (x <= x2 || x >= x3 || horizontal);

            // Push the previous pixel to the back layer if drawing a front anti-aliased edge pixel
            if ((disp3DCnt & BIT(4)) && layer == 0 && edge)
            {
                framebuffer[1][i]  = framebuffer[0][i];
                depthBuffer[1][i]  = depthBuffer[0][i];
                attribBuffer[1][i] = attribBuffer[0][i];

                // Set the pixel transparency for anti-aliasing
                if (x <= x2)
                    edgeAlpha = interpolateLinear(x1a, x2a, x1, x, x2);
                else if (x >= x3)
                    edgeAlpha = interpolateLinear(x3a, x4a, x3, x, x4);
            }

            framebuffer[layer][i] = BIT(26) | color;
            depthBuffer[layer][i] = depth;
            attribBuffer[layer][i] = (attribBuffer[layer][i] & 0x0FC0) | (edgeAlpha << 15) |
                (edge << 14) | (polygon->fog << 13) | polygon->id;
        }
        else if (!(attribBuffer[layer][i] & BIT(12)) || ((attribBuffer[layer][i] >> 6) & 0x3F) != polygon->id) // Transparent
        {
            // Transparent pixels are only drawn if the old pixel isn't transparent or the polygon ID differs
            framebuffer[layer][i] = BIT(26) | (((disp3DCnt & BIT(3)) && (framebuffer[layer][i] & 0xFC0000)) ?
                interpolateColor(framebuffer[layer][i], color, 0, color >> 18, 63) : color);
            if (polygon->transNewDepth) depthBuffer[layer][i] = depth;
            attribBuffer[layer][i] = (attribBuffer[layer][i] & (0x1FC03F | (polygon->fog << 13))) | BIT(12) | (polygon->id << 6);

            // Blend with the back layer as well if drawing over a front anti-aliased edge pixel
            if ((disp3DCnt & BIT(4)) && layer == 0 && (attribBuffer[0][i] & BIT(14)))
            {
                framebuffer[1][i] = BIT(26) | (((disp3DCnt & BIT(3)) && (framebuffer[1][i] & 0xFC0000)) ?
                    interpolateColor(framebuffer[1][i], color, 0, color >> 18, 63) : color);
                if (polygon->transNewDepth) depthBuffer[1][i] = depth;
                attribBuffer[1][i] = (attribBuffer[1][i] & (0x1FC03F | (polygon->fog << 13))) | BIT(12) | (polygon->id << 6);
            }
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

void Gpu3DRenderer::writeEdgeColor(int index, uint16_t mask, uint16_t value)
{
    // Write to one of the EDGE_COLOR registers and invalidate the 3D if a parameter changed
    mask &= 0x7FFF;
    if ((value & mask) == (edgeColor[index] & mask)) return;
    edgeColor[index] = (edgeColor[index] & ~mask) | (value & mask);
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
