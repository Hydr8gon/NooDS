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

#include <cstdio>
#include <cstring>

#include "gpu_2d.h"
#include "defines.h"
#include "gpu_3d_renderer.h"
#include "memory.h"

Gpu2D::Gpu2D(Memory *memory): gpu3DRenderer(nullptr), memory(memory)
{
    // Prepare a 2D GPU to be engine B
    palette = memory->getPalette() + 0x400;
    oam = memory->getOam() + 0x400;
    bgVramAddr = 0x6200000;
    objVramAddr = 0x6600000;
}

Gpu2D::Gpu2D(Gpu3DRenderer *gpu3DRenderer, Memory *memory): gpu3DRenderer(gpu3DRenderer), memory(memory)
{
    // Prepare a 2D GPU to be engine A
    palette = memory->getPalette();
    oam = memory->getOam();
    bgVramAddr = 0x6000000;
    objVramAddr = 0x6400000;
}

uint32_t Gpu2D::rgb5ToRgba6(uint16_t color)
{
    // Convert an RGB5 value to an RGBA6 value (the way the 2D engine does it)
    uint8_t r = ((color >>  0) & 0x1F) * 2;
    uint8_t g = ((color >>  5) & 0x1F) * 2;
    uint8_t b = ((color >> 10) & 0x1F) * 2;
    uint8_t a = (color & BIT(15)) ? 0x3F : 0;
    return (a << 18) | (b << 12) | (g << 6) | r;
}

void Gpu2D::drawScanline(int line)
{
    switch ((dispCnt & 0x00030000) >> 16) // Display mode
    {
        case 0: // Display off
        {
            // Fill the display with white
            for (int i = 0; i < 256; i++)
                framebuffer[line * 256 + i] = 0x7FFFF;
            break;
        }

        case 1: // Graphics display
        {
            // Clear the layers at the start of the frame
            if (line == 0)
            {
                for (int i = 0; i < 8; i++)
                    memset(layers[i], 0, 256 * 192 * sizeof(uint32_t));
            }

            // Draw the background layers
            // The type of each layer depends on the BG mode
            switch (dispCnt & 0x00000007)
            {
                case 0:
                    if (dispCnt & BIT(8))  drawText(0, line);
                    if (dispCnt & BIT(9))  drawText(1, line);
                    if (dispCnt & BIT(10)) drawText(2, line);
                    if (dispCnt & BIT(11)) drawText(3, line);
                    break;

                case 1:
                    if (dispCnt & BIT(8))    drawText(0, line);
                    if (dispCnt & BIT(9))    drawText(1, line);
                    if (dispCnt & BIT(10))   drawText(2, line);
                    if (dispCnt & BIT(11)) drawAffine(3, line);
                    break;

                case 2:
                    if (dispCnt & BIT(8))    drawText(0, line);
                    if (dispCnt & BIT(9))    drawText(1, line);
                    if (dispCnt & BIT(10)) drawAffine(2, line);
                    if (dispCnt & BIT(11)) drawAffine(3, line);
                    break;

                case 3:
                    if (dispCnt & BIT(8))      drawText(0, line);
                    if (dispCnt & BIT(9))      drawText(1, line);
                    if (dispCnt & BIT(10))     drawText(2, line);
                    if (dispCnt & BIT(11)) drawExtended(3, line);
                    break;

                case 4:
                    if (dispCnt & BIT(8))      drawText(0, line);
                    if (dispCnt & BIT(9))      drawText(1, line);
                    if (dispCnt & BIT(10))   drawAffine(2, line);
                    if (dispCnt & BIT(11)) drawExtended(3, line);
                    break;

                case 5:
                    if (dispCnt & BIT(8))      drawText(0, line);
                    if (dispCnt & BIT(9))      drawText(1, line);
                    if (dispCnt & BIT(10)) drawExtended(2, line);
                    if (dispCnt & BIT(11)) drawExtended(3, line);
                    break;

                default:
                    printf("Unknown engine %c BG mode: %d\n", (gpu3DRenderer ? 'A' : 'B'), dispCnt & 0x00000007);
                    break;
            }

            // Draw the objects
            if (dispCnt & BIT(12)) drawObjects(line);

            // Blend the layers to form the final image
            for (int i = 0; i < 256; i++)
            {
                uint8_t enabled = BIT(5) | (dispCnt >> 8);
                uint32_t *pixel = &framebuffer[line * 256 + i];
                int layer, blendBit = -1;

                // If the current pixel is in the bounds of a window, disable layers that are disabled in that window
                if (dispCnt & 0xE000) // Windows enabled
                {
                    if ((dispCnt & BIT(13)) && i >= winX1[0] && i < winX2[0] && line >= winY1[0] && line < winY2[0])
                        enabled &= winIn >> 0; // Window 0
                    else if ((dispCnt & BIT(14)) && i >= winX1[1] && i < winX2[1] && line >= winY1[1] && line < winY2[1])
                        enabled &= winIn >> 8; // Window 1
                    else if ((dispCnt & BIT(15)) && (*pixel & BIT(24)))
                        enabled &= winOut >> 8; // Object window
                    else
                        enabled &= winOut >> 0; // Outside of windows
                }

                // Find the topmost pixel from the layers
                for (layer = 0; layer < 4; layer++)
                {
                    // Check for visible pixels in the object layers
                    if ((enabled & BIT(4)) && (layers[4 + layer][line * 256 + i] & 0xFC0000))
                    {
                        *pixel = layers[4 + layer][line * 256 + i];
                        blendBit = 4;
                        break;
                    }

                    // Check for visible pixels in the background layers
                    // The BG layers can be rearranged, so they need to be checked in the correct order
                    for (int j = 0; j < 4; j++)
                    {
                        if ((bgCnt[j] & 0x0003) == layer && (enabled & BIT(j)) && (layers[j][line * 256 + i] & 0xFC0000))
                        {
                            *pixel = layers[j][line * 256 + i];
                            blendBit = j;
                            layer++;
                            break;
                        }
                    }

                    if (blendBit != -1) break;
                }

                // Use the backdrop color if no visible layer pixels were found
                if (blendBit == -1)
                {
                    *pixel = rgb5ToRgba6(U8TO16(palette, 0));
                    blendBit = 5;
                }

                // Skip blending if it's not enabled
                if (!(enabled & BIT(5))) continue;

                int mode = (bldCnt & 0x00C0) >> 6;

                // Apply alpha blending if it's enabled for the current layer
                // Semi-transparent objects and 3D are special cases that force alpha blending
                if ((mode == 1 && blendBit != 5 && (bldCnt & BIT(blendBit))) ||
                    (*pixel & BIT(25)) || ((*pixel & BIT(26)) && (*pixel & 0xFC0000) < 0xFC0000))
                {
                    int blendBit2 = -1;
                    uint32_t blend;

                    // Find the second topmost pixel from the layers, continuing from the first search
                    for (layer; layer < 4; layer++)
                    {
                        // Check for visible pixels in the object layers
                        // Blending only counts the topmost object pixel, so skip this if the first pixel was from an object
                        if (blendBit != 4 && (enabled & BIT(4)) && (layers[4 + layer][line * 256 + i] & 0xFC0000))
                        {
                            blend = layers[4 + layer][line * 256 + i];
                            blendBit2 = 4;
                            break;
                        }

                        // Check for visible pixels in the background layers
                        // The BG layers can be rearranged, so they need to be checked in the correct order
                        for (int j = 0; j < 4; j++)
                        {
                            if ((bgCnt[j] & 0x0003) == layer && (enabled & BIT(j)) && (layers[j][line * 256 + i] & 0xFC0000))
                            {
                                blend = layers[j][line * 256 + i];
                                blendBit2 = j;
                                break;
                            }
                        }

                        if (blendBit2 != -1) break;
                    }

                    // Use the backdrop color if no visible layer pixels were found
                    if (blendBit2 == -1)
                    {
                        blend = rgb5ToRgba6(U8TO16(palette, 0));
                        blendBit2 = 5;
                    }

                    // Blend the topmost pixel with the second topmost pixel, if the second pixel is a blend target
                    // 3D uses its own alpha values instead of the normal 2D values
                    if (blendBit2 != -1 && (bldCnt & BIT(8 + blendBit2)))
                    {
                        if (*pixel & BIT(26)) // 3D
                        {
                            int eva = ((*pixel >> 18) & 0x3F) + 1;
                            int evb = 64 - eva;
                            int r = ((*pixel >>  0) & 0x3F) * eva / 64 + ((blend >>  0) & 0x3F) * evb / 64; if (r > 63) r = 63;
                            int g = ((*pixel >>  6) & 0x3F) * eva / 64 + ((blend >>  6) & 0x3F) * evb / 64; if (g > 63) g = 63;
                            int b = ((*pixel >> 12) & 0x3F) * eva / 64 + ((blend >> 12) & 0x3F) * evb / 64; if (b > 63) b = 63;
                            *pixel = (b << 12) | (g << 6) | r;
                        }
                        else
                        {
                            int eva = (bldAlpha & 0x001F) >> 0; if (eva > 16) eva = 16;
                            int evb = (bldAlpha & 0x1F00) >> 8; if (evb > 16) evb = 16;
                            int r = ((*pixel >>  0) & 0x3F) * eva / 16 + ((blend >>  0) & 0x3F) * evb / 16; if (r > 63) r = 63;
                            int g = ((*pixel >>  6) & 0x3F) * eva / 16 + ((blend >>  6) & 0x3F) * evb / 16; if (g > 63) g = 63;
                            int b = ((*pixel >> 12) & 0x3F) * eva / 16 + ((blend >> 12) & 0x3F) * evb / 16; if (b > 63) b = 63;
                            *pixel = (b << 12) | (g << 6) | r;
                        }
                        continue;
                    }
                }

                // Apply brightness blending
                // If the special cases don't have a second target to blend with, they can fall back here
                if (mode == 2 && (bldCnt & BIT(blendBit))) // Brightness increase
                {
                    int r = (*pixel >>  0) & 0x3F; r += (63 - r) * bldY / 16;
                    int g = (*pixel >>  6) & 0x3F; g += (63 - g) * bldY / 16;
                    int b = (*pixel >> 12) & 0x3F; b += (63 - b) * bldY / 16;
                    *pixel = (b << 12) | (g << 6) | r;
                }
                else if (mode == 3 && (bldCnt & BIT(blendBit))) // Brightness decrease
                {
                    int r = (*pixel >>  0) & 0x3F; r -= r * bldY / 16;
                    int g = (*pixel >>  6) & 0x3F; g -= g * bldY / 16;
                    int b = (*pixel >> 12) & 0x3F; b -= b * bldY / 16;
                    *pixel = (b << 12) | (g << 6) | r;
                }
            }

            break;
        }

        case 2: // VRAM display
        {
            // Draw raw bitmap data from a VRAM block
            uint8_t *data = memory->getVramBlock((dispCnt & 0x000C0000) >> 18);
            for (int i = 0; i < 256; i++)
                framebuffer[line * 256 + i] = rgb5ToRgba6(U8TO16(data, (line * 256 + i) * 2));
            break;
        }

        case 3: // Main memory display
        {
            printf("Unimplemented engine %c display mode: display FIFO\n", (gpu3DRenderer ? 'A' : 'B'));
            dispCnt &= ~0x00030000;
            break;
        }
    }
}

void Gpu2D::applyMasterBright(int line)
{
    // Apply the master brightness
    switch ((masterBright & 0xC000) >> 14) // Mode
    {
        case 1: // Up
        {
            // Get the brightness factor
            int factor = (masterBright & 0x001F);
            if (factor > 16) factor = 16;

            for (int i = 0; i < 256; i++)
            {
                // Extract the RGB values
                uint32_t *pixel = &framebuffer[line * 256 + i];
                uint8_t r = (*pixel >>  0) & 0x3F;
                uint8_t g = (*pixel >>  6) & 0x3F;
                uint8_t b = (*pixel >> 12) & 0x3F;

                // Adjust the values and put them back
                r += (63 - r) * factor / 16;
                g += (63 - g) * factor / 16;
                b += (63 - b) * factor / 16;
                *pixel = (b << 12) | (g << 6) | r;
            }

            break;
        }

        case 2: // Down
        {
            // Get the brightness factor
            int factor = (masterBright & 0x001F);
            if (factor > 16) factor = 16;

            for (int i = 0; i < 256; i++)
            {
                // Extract the RGB values
                uint32_t *pixel = &framebuffer[line * 256 + i];
                uint8_t r = (*pixel >>  0) & 0x3F;
                uint8_t g = (*pixel >>  6) & 0x3F;
                uint8_t b = (*pixel >> 12) & 0x3F;

                // Adjust the values and put them back
                r -= r * factor / 16;
                g -= g * factor / 16;
                b -= b * factor / 16;
                *pixel = (b << 12) | (g << 6) | r;
            }

            break;
        }
    }
}

void Gpu2D::drawText(int bg, int line)
{
    // If 3D is enabled, render it to BG0 in text mode
    if (bg == 0 && (dispCnt & BIT(3)))
    {
        memcpy(&layers[bg][line * 256], &gpu3DRenderer->getFramebuffer()[line * 256], 256 * sizeof(uint32_t));
        return;
    }

    // Get information about the tile data
    uint32_t screenBase = ((bgCnt[bg] & 0x1F00) >> 8) * 0x0800 + ((dispCnt & 0x38000000) >> 27) * 0x10000;
    uint32_t charBase   = ((bgCnt[bg] & 0x003C) >> 2) * 0x4000 + ((dispCnt & 0x07000000) >> 24) * 0x10000;

    // If the Y-offset exceeds 256 and the background is 512 pixels tall, move to the next 256x256 section
    // If the background is 512 pixels wide, move 2 sections to skip the second X section
    int yOffset = (line + bgVOfs[bg]) % 512;
    if (yOffset >= 256 && (bgCnt[bg] & BIT(15)))
        screenBase += (bgCnt[bg] & BIT(14)) ? 0x1000 : 0x800;

    // Get the tile data for the current line
    uint8_t *data = memory->getMappedVram(bgVramAddr + screenBase + ((yOffset / 8) % 32) * 64);
    if (!data) return;

    // Draw a line
    if (bgCnt[bg] & BIT(7)) // 8-bit
    {
        for (int i = 0; i <= 256; i += 8)
        {
            // Get the data for the current tile
            // If the X-offset exceeds 256 and the background is 512 pixels wide, move to the next 256x256 section
            int xOffset = (bgHOfs[bg] + i) % 512;
            uint16_t tile = U8TO16(data, ((xOffset / 8) % 32) * 2 + ((xOffset >= 256 && (bgCnt[bg] & BIT(14))) ? 0x800 : 0));

            // Get the palette of the tile
            uint8_t *pal;
            if (dispCnt & BIT(30)) // Extended palette
            {
                // Determine the extended palette slot
                // Backgrounds 0 and 1 can alternatively use slots 2 and 3
                int slot = (bg < 2 && (bgCnt[bg] & BIT(13))) ? (bg + 2) : bg;

                // In extended palette mode, the tile can select from multiple 256-color palettes
                if (!extPalettes[slot]) continue;
                pal = &extPalettes[slot][((tile & 0xF000) >> 12) * 512];
            }
            else // Standard palette
            {
                pal = palette;
            }

            // Find the palette indeces for the right pixel row of the tile
            // The tile can be vertically flipped
            uint8_t *indices = memory->getMappedVram(bgVramAddr + charBase + (tile & 0x03FF) * 64);
            if (!indices) continue;
            indices += (tile & BIT(11)) ? ((7 - yOffset % 8) * 8) : ((yOffset % 8) * 8);

            for (int j = 0; j < 8; j++)
            {
                // Draw the tile horizontally flipped if enabled
                int offset = i - (xOffset % 8) + ((tile & BIT(10)) ? (7 - j) : j);

                // Draw a pixel
                if (offset >= 0 && offset < 256 && indices[j])
                    layers[bg][line * 256 + offset] = rgb5ToRgba6(U8TO16(pal, indices[j] * 2) | BIT(15));
            }
        }
    }
    else // 4-bit
    {
        for (int i = 0; i <= 256; i += 8)
        {
            // Get the data for the current tile
            // If the X-offset exceeds 256 and the background is 512 pixels wide, move to the next 256x256 section
            uint16_t xOffset = (bgHOfs[bg] + i) % 512;
            uint16_t tile = U8TO16(data, ((xOffset / 8) % 32) * 2 + ((xOffset >= 256 && (bgCnt[bg] & BIT(14))) ? 0x800 : 0));

            // Get the palette of the tile
            // In 4-bit mode, the tile can select from multiple 16-color palettes
            uint8_t *pal = &palette[((tile & 0xF000) >> 12) * 32];

            // Find the palette indeces for the right pixel row of the tile
            // The tile can be vertically flipped
            uint8_t *indices = memory->getMappedVram(bgVramAddr + charBase + (tile & 0x03FF) * 32);
            if (!indices) return;
            indices += (tile & BIT(11)) ? ((7 - yOffset % 8) * 4) : ((yOffset % 8) * 4);

            for (int j = 0; j < 8; j++)
            {
                // Draw the tile horizontally flipped if enabled
                int offset = i - (xOffset % 8) + ((tile & BIT(10)) ? (7 - j) : j);

                // Extract the 4-bit palette index from the 8-bit value
                uint8_t index = (j & 1) ? ((indices[j / 2] & 0xF0) >> 4) : (indices[j / 2] & 0x0F);

                // Draw a pixel
                if (offset >= 0 && offset < 256 && index)
                    layers[bg][line * 256 + offset] = rgb5ToRgba6(U8TO16(pal, index * 2) | BIT(15));
            }
        }
    }
}

void Gpu2D::drawAffine(int bg, int line)
{
    // Get information about the tile data
    uint32_t screenBase = ((bgCnt[bg] & 0x1F00) >> 8) * 0x0800 + ((dispCnt & 0x38000000) >> 27) * 0x10000;
    uint32_t charBase   = ((bgCnt[bg] & 0x003C) >> 2) * 0x4000 + ((dispCnt & 0x07000000) >> 24) * 0x10000;
    int size = 128 << ((bgCnt[bg] & 0xC000) >> 14);

    // Get the tile data
    uint8_t *data = memory->getMappedVram(bgVramAddr + screenBase);
    if (!data) return;

    // Calculate the scroll values
    int scrollX = bgX[bg - 2] + (bgPA[bg - 2] * (size / 2) + bgPB[bg - 2] * (size / 2));
    int scrollY = bgY[bg - 2] + (bgPC[bg - 2] * (size / 2) + bgPD[bg - 2] * (size / 2));

    // Draw a line
    for (int i = 0; i < 256; i++)
    {
        // Calculate the rotscaled coordinates relative to the background
        int rotscaleX = (bgPA[bg - 2] * (i - size / 2) + bgPB[bg - 2] * (line - size / 2) + scrollX) >> 8;
        int rotscaleY = (bgPC[bg - 2] * (i - size / 2) + bgPD[bg - 2] * (line - size / 2) + scrollY) >> 8;

        // Handle display area overflow
        if (bg < 2 || (bgCnt[bg] & BIT(13))) // Wraparound
        {
            rotscaleX %= size;
            rotscaleY %= size;

            if (rotscaleX < 0) rotscaleX += size;
            if (rotscaleY < 0) rotscaleY += size;
        }
        else if (rotscaleX < 0 || rotscaleX >= size || rotscaleY < 0 || rotscaleY >= size) // Transparent
        {
            continue;
        }

        // Get the data for the current tile
        uint8_t tile = data[(rotscaleY / 8) * (size / 8) + (rotscaleX / 8)];

        // Get the palette of the tile
        uint8_t *pal;
        if (dispCnt & BIT(30)) // Extended palette
        {
            if (!extPalettes[bg]) continue;
            pal = extPalettes[bg];
        }
        else // Standard palette
        {
            pal = palette;
        }

        // Find the palette index for the right pixel of the tile
        uint8_t *index = memory->getMappedVram(bgVramAddr + charBase + tile * 64);
        if (!index) continue;
        index += (rotscaleY % 8) * 8;
        index += (rotscaleX % 8);

        // Draw a pixel
        if (*index)
            layers[bg][line * 256 + i] = rgb5ToRgba6(U8TO16(pal, *index * 2) | BIT(15));
    }
}

void Gpu2D::drawExtended(int bg, int line)
{
    if (bgCnt[bg] & BIT(7)) // Bitmap
    {
        // Get information about the bitmap data
        uint32_t screenBase = ((bgCnt[bg] & 0x1F00) >> 8) * 0x4000;
        int sizeX, sizeY;
        switch ((bgCnt[bg] & 0xC000) >> 14)
        {
            case 0: sizeX = 128; sizeY = 128; break;
            case 1: sizeX = 256; sizeY = 256; break;
            case 2: sizeX = 512; sizeY = 256; break;
            case 3: sizeX = 512; sizeY = 512; break;
        }

        // Get the bitmap data
        uint8_t *data = memory->getMappedVram(bgVramAddr + screenBase);
        if (!data) return;

        // Calculate the scroll values
        int scrollX = bgX[bg - 2] + (bgPA[bg - 2] * (sizeX / 2) + bgPB[bg - 2] * (sizeY / 2));
        int scrollY = bgY[bg - 2] + (bgPC[bg - 2] * (sizeX / 2) + bgPD[bg - 2] * (sizeY / 2));

        if (bgCnt[bg] & BIT(2)) // Direct color bitmap
        {
            // Draw a line
            for (int i = 0; i < 256; i++)
            {
                // Calculate the rotscaled coordinates relative to the background
                int rotscaleX = (bgPA[bg - 2] * (i - sizeX / 2) + bgPB[bg - 2] * (line - sizeX / 2) + scrollX) >> 8;
                int rotscaleY = (bgPC[bg - 2] * (i - sizeY / 2) + bgPD[bg - 2] * (line - sizeY / 2) + scrollY) >> 8;

                // Handle display area overflow
                if (bgCnt[bg] & BIT(13)) // Wraparound
                {
                    rotscaleX %= sizeX;
                    rotscaleY %= sizeY;

                    if (rotscaleX < 0) rotscaleX += sizeX;
                    if (rotscaleY < 0) rotscaleY += sizeY;
                }
                else if (rotscaleX < 0 || rotscaleX >= sizeX || rotscaleY < 0 || rotscaleY >= sizeY) // Transparent
                {
                    continue;
                }

                // Draw a pixel
                layers[bg][line * 256 + i] = rgb5ToRgba6(U8TO16(data, (rotscaleY * sizeX + rotscaleX) * 2));
            }
        }
        else // 256 color bitmap
        {
            // Draw a line
            for (int i = 0; i < 256; i++)
            {
                // Calculate the rotscaled coordinates relative to the background
                int rotscaleX = (bgPA[bg - 2] * (i - sizeX / 2) + bgPB[bg - 2] * (line - sizeX / 2) + scrollX) >> 8;
                int rotscaleY = (bgPC[bg - 2] * (i - sizeY / 2) + bgPD[bg - 2] * (line - sizeY / 2) + scrollY) >> 8;

                // Handle display area overflow
                if (bgCnt[bg] & BIT(13)) // Wraparound
                {
                    rotscaleX %= sizeX;
                    rotscaleY %= sizeY;

                    if (rotscaleX < 0) rotscaleX += sizeX;
                    if (rotscaleY < 0) rotscaleY += sizeY;
                }
                else if (rotscaleX < 0 || rotscaleX >= sizeX || rotscaleY < 0 || rotscaleY >= sizeY) // Transparent
                {
                    continue;
                }

                // Draw a pixel
                if (data[rotscaleY * sizeX + rotscaleX])
                    layers[bg][line * 256 + i] = rgb5ToRgba6(U8TO16(palette, (data[rotscaleY * sizeX + rotscaleX]) * 2) | BIT(15));
            }
        }
    }
    else // Extended affine
    {
        // Get information about the tile data
        uint32_t screenBase = ((bgCnt[bg] & 0x1F00) >> 8) * 0x0800 + ((dispCnt & 0x38000000) >> 27) * 0x10000;
        uint32_t charBase   = ((bgCnt[bg] & 0x003C) >> 2) * 0x4000 + ((dispCnt & 0x07000000) >> 24) * 0x10000;
        int size = 128 << ((bgCnt[bg] & 0xC000) >> 14);

        // Get the tile data
        uint8_t *data = memory->getMappedVram(bgVramAddr + screenBase);
        if (!data) return;

        // Calculate the scroll values
        int scrollX = bgX[bg - 2] + (bgPA[bg - 2] * (size / 2) + bgPB[bg - 2] * (size / 2));
        int scrollY = bgY[bg - 2] + (bgPC[bg - 2] * (size / 2) + bgPD[bg - 2] * (size / 2));

        // Draw a line
        for (int i = 0; i < 256; i++)
        {
            // Calculate the rotscaled coordinates relative to the background
            int rotscaleX = (bgPA[bg - 2] * (i - size / 2) + bgPB[bg - 2] * (line - size / 2) + scrollX) >> 8;
            int rotscaleY = (bgPC[bg - 2] * (i - size / 2) + bgPD[bg - 2] * (line - size / 2) + scrollY) >> 8;

            // Handle display area overflow
            if (bg < 2 || (bgCnt[bg] & BIT(13))) // Wraparound
            {
                rotscaleX %= size;
                rotscaleY %= size;

                if (rotscaleX < 0) rotscaleX += size;
                if (rotscaleY < 0) rotscaleY += size;
            }
            else if (rotscaleX < 0 || rotscaleX >= size || rotscaleY < 0 || rotscaleY >= size) // Transparent
            {
                continue;
            }

            // Get the data for the current tile
            uint16_t tile = U8TO16(data, ((rotscaleY / 8) * (size / 8) + (rotscaleX / 8)) * 2);

            // Get the palette of the tile
            uint8_t *pal;
            if (dispCnt & BIT(30)) // Extended palette
            {
                // In extended palette mode, the tile can select from multiple 256-color palettes
                if (!extPalettes[bg]) continue;
                pal = &extPalettes[bg][((tile & 0xF000) >> 12) * 512];
            }
            else // Standard palette
            {
                pal = palette;
            }

            // Find the palette index for the right pixel of the tile
            // The tile can be vertically or horizontally flipped
            uint8_t *index = memory->getMappedVram(bgVramAddr + charBase + (tile & 0x03FF) * 64);
            if (!index) continue;
            index += ((tile & BIT(11)) ? (7 - rotscaleY % 8) : (rotscaleY % 8)) * 8;
            index += ((tile & BIT(10)) ? (7 - rotscaleX % 8) : (rotscaleX % 8));

            // Draw a pixel
            if (*index)
                layers[bg][line * 256 + i] = rgb5ToRgba6(U8TO16(pal, *index * 2) | BIT(15));
        }
    }
}

void Gpu2D::drawObjects(int line)
{
    // Loop through the 128 sprites in OAM, in order of priority from high to low
    for (int i = 127; i >= 0; i--)
    {
        // Get the current object
        // Each object takes up 8 bytes in memory, but the last 2 bytes are reserved for rotscale
        uint16_t object[3];
        object[0] = U8TO16(oam, i * 8);

        // Skip sprites that are disabled
        if (!(object[0] & BIT(8)) && (object[0] & BIT(9)))
            continue;

        object[1] = U8TO16(oam, i * 8 + 2);
        object[2] = U8TO16(oam, i * 8 + 4);

        // Determine the dimensions of the object
        int width = 0, height = 0;
        switch ((object[1] & 0xC000) >> 14) // Size
        {
            case 0:
            {
                switch ((object[0] & 0xC000) >> 14) // Shape
                {
                    case 0: width =  8; height =  8; break; // Square
                    case 1: width = 16; height =  8; break; // Horizontal
                    case 2: width =  8; height = 16; break; // Vertical
                }
                break;
            }

            case 1:
            {
                switch ((object[0] & 0xC000) >> 14) // Shape
                {
                    case 0: width = 16; height = 16; break; // Square
                    case 1: width = 32; height =  8; break; // Horizontal
                    case 2: width =  8; height = 32; break; // Vertical
                }
                break;
            }

            case 2:
            {
                switch ((object[0] & 0xC000) >> 14) // Shape
                {
                    case 0: width = 32; height = 32; break; // Square
                    case 1: width = 32; height = 16; break; // Horizontal
                    case 2: width = 16; height = 32; break; // Vertical
                }
                break;
            }

            case 3:
            {
                switch ((object[0] & 0xC000) >> 14) // Shape
                {
                    case 0: width = 64; height = 64; break; // Square
                    case 1: width = 64; height = 32; break; // Horizontal
                    case 2: width = 32; height = 64; break; // Vertical
                }
                break;
            }
        }

        // Double the object bounds for rotscale objects with the double size bit set
        int width2 = width, height2 = height;
        if ((object[0] & BIT(8)) && (object[0] & BIT(9)))
        {
            width2 *= 2;
            height2 *= 2;
        }

        // Get the Y coordinate and wrap it around if it exceeds the screen bounds
        int y = (object[0] & 0x00FF);
        if (y >= 192) y -= 256;

        // Don't draw anything if the current scanline lies outside of the object's bounds
        int spriteY = line - y;
        if (spriteY < 0 || spriteY >= height2)
            continue;

        // Get the X coordinate and wrap it around if it exceeds the screen bounds
        int x = (object[1] & 0x01FF);
        if (x >= 256) x -= 512;

        uint32_t *layer = layers[4 + ((object[2] & 0x0C00) >> 10)];
        int type = (object[0] & 0x0C00) >> 10;

        // Draw the object if it's a bitmap
        if (type == 3)
        {
            uint32_t address;
            int bitmapWidth;

            // Determine the address and width of the bitmap
            if (dispCnt & BIT(6)) // 1D mapping
            {
                address = objVramAddr + (object[2] & 0x03FF) * ((dispCnt & BIT(22)) ? 256 : 128);
                bitmapWidth = width;
            }
            else // 2D mapping
            {
                uint8_t xMask = (dispCnt & BIT(5)) ? 0x1F : 0x0F;
                address = objVramAddr + (object[2] & 0x03FF & xMask) * 0x10 + (object[2] & 0x03FF & ~xMask) * 0x80;
                bitmapWidth = (dispCnt & BIT(5)) ? 256 : 128;
            }

            // Get the bitmap data
            uint8_t *data = memory->getMappedVram(address);
            if (!data) continue;

            if (object[0] & BIT(8)) // Rotscale
            {
                // Get the rotscale parameters
                int params[4];
                for (int j = 0; j < 4; j++)
                    params[j] = (int16_t)U8TO16(oam, ((object[1] & 0x3E00) >> 9) * 0x20 + j * 8 + 6);

                for (int j = 0; j < width2; j++)
                {
                    // Calculate the rotscaled X coordinate relative to the sprite
                    int rotscaleX = ((params[0] * (j - width2 / 2) + params[1] * (spriteY - height2 / 2)) >> 8) + width / 2;
                    if (rotscaleX < 0 || rotscaleX >= width) continue;

                    // Calculate the rotscaled Y coordinate relative to the sprite
                    int rotscaleY = ((params[2] * (j - width2 / 2) + params[3] * (spriteY - height2 / 2)) >> 8) + height / 2;
                    if (rotscaleY < 0 || rotscaleY >= height) continue;

                    // Draw a pixel of the bitmap object
                    if (x + j >= 0 && x + j < 256)
                        layer[line * 256 + x + j] = rgb5ToRgba6(U8TO16(data, (rotscaleY * bitmapWidth + rotscaleX) * 2));
                }
            }
            else
            {
                for (int j = 0; j < width; j++)
                {
                    // Draw a pixel of the bitmap object
                    if (x + j >= 0 && x + j < 256)
                        layer[line * 256 + x + j] = rgb5ToRgba6(U8TO16(data, (spriteY * bitmapWidth + j) * 2));
                }
            }

            continue;
        }

        // Get the current tile
        // For 1D tile mapping, the boundary between tiles can be 32, 64, 128, or 256 bytes
        uint16_t bound = (dispCnt & BIT(4)) ? (32 << ((dispCnt & 0x00300000) >> 20)) : 32;
        uint8_t *tile = memory->getMappedVram(objVramAddr + (object[2] & 0x03FF) * bound);
        if (!tile) continue;

        // Draw the object if it's tile-based
        if (object[0] & BIT(8)) // Rotscale
        {
            // Get the rotscale parameters
            int params[4];
            for (int j = 0; j < 4; j++)
                params[j] = (int16_t)U8TO16(oam, ((object[1] & 0x3E00) >> 9) * 0x20 + j * 8 + 6);

            if (object[0] & BIT(13)) // 8-bit
            {
                int mapWidth = (dispCnt & BIT(4)) ? width : 128;

                // Get the palette of the object
                uint8_t *pal;
                if (dispCnt & BIT(31)) // Extended palette
                {
                    // In extended palette mode, the object can select from multiple 256-color palettes
                    if (!extPalettes[4]) continue;
                    pal = &extPalettes[4][((object[2] & 0xF000) >> 12) * 512];
                }
                else // Standard palette
                {
                    pal = &palette[0x200];
                }

                for (int j = 0; j < width2; j++)
                {
                    // Calculate the rotscaled X coordinate relative to the sprite
                    int rotscaleX = ((params[0] * (j - width2 / 2) + params[1] * (spriteY - height2 / 2)) >> 8) + width / 2;
                    if (rotscaleX < 0 || rotscaleX >= width) continue;

                    // Calculate the rotscaled Y coordinate relative to the sprite
                    int rotscaleY = ((params[2] * (j - width2 / 2) + params[3] * (spriteY - height2 / 2)) >> 8) + height / 2;
                    if (rotscaleY < 0 || rotscaleY >= height) continue;

                    // Get the appropriate palette index from the tile for the current position
                    uint8_t index = tile[((rotscaleY / 8) * mapWidth + rotscaleY % 8) * 8 + (rotscaleX / 8) * 64 + rotscaleX % 8];

                    // Draw a pixel if one exists at the current position
                    if (x + j >= 0 && x + j < 256 && index)
                    {
                        if (type == 2) // Object window
                        {
                            // Mark object window pixels with an extra bit, and don't actually draw anything
                            framebuffer[line * 256 + x + j] |= BIT(24);
                        }
                        else
                        {
                            // Draw a pixel
                            layer[line * 256 + x + j] = rgb5ToRgba6(U8TO16(pal, index * 2) | BIT(15));

                            // Mark semi-transparent pixels with an extra bit
                            if (type == 1) layer[line * 256 + x + j] |= BIT(25);
                        }
                    }
                }
            }
            else // 4-bit
            {
                int mapWidth = (dispCnt & BIT(4)) ? width : 256;

                // Get the palette of the object
                // In 4-bit mode, the object can select from multiple 16-color palettes
                uint8_t *pal = &palette[0x200 + ((object[2] & 0xF000) >> 12) * 32];

                for (int j = 0; j < width2; j++)
                {
                    // Calculate the rotscaled X coordinate relative to the sprite
                    int rotscaleX = ((params[0] * (j - width2 / 2) + params[1] * (spriteY - height2 / 2)) >> 8) + width / 2;
                    if (rotscaleX < 0 || rotscaleX >= width) continue;

                    // Calculate the rotscaled Y coordinate relative to the sprite
                    int rotscaleY = ((params[2] * (j - width2 / 2) + params[3] * (spriteY - height2 / 2)) >> 8) + height / 2;
                    if (rotscaleY < 0 || rotscaleY >= height) continue;

                    // Get the appropriate palette index from the tile for the current position
                    uint8_t index = tile[((rotscaleY / 8) * mapWidth + rotscaleY % 8) * 4 + (rotscaleX / 8) * 32 + (rotscaleX / 2) % 4];
                    index = (rotscaleX % 2 == 1) ? ((index & 0xF0) >> 4) : (index & 0x0F);

                    // Draw a pixel if one exists at the current position
                    if (x + j >= 0 && x + j < 256 && index)
                    {
                        if (type == 2) // Object window
                        {
                            // Mark object window pixels with an extra bit, and don't actually draw anything
                            framebuffer[line * 256 + x + j] |= BIT(24);
                        }
                        else
                        {
                            // Draw a pixel
                            layer[line * 256 + x + j] = rgb5ToRgba6(U8TO16(pal, index * 2) | BIT(15));

                            // Mark semi-transparent pixels with an extra bit
                            if (type == 1) layer[line * 256 + x + j] |= BIT(25);
                        }
                    }
                }
            }
        }
        else if (object[0] & BIT(13)) // 8-bit
        {
            // Adjust the current tile to align with the current Y coordinate relative to the object
            int mapWidth = (dispCnt & BIT(4)) ? width : 128;
            if (object[1] & BIT(13)) // Vertical flip
                tile += (7 - (spriteY % 8) + ((height - 1 - spriteY) / 8) * mapWidth) * 8;
            else
                tile += ((spriteY % 8) + (spriteY / 8) * mapWidth) * 8;

            // Get the palette of the object
            uint8_t *pal;
            if (dispCnt & BIT(31)) // Extended palette
            {
                // In extended palette mode, the object can select from multiple 256-color palettes
                if (!extPalettes[4]) continue;
                pal = &extPalettes[4][((object[2] & 0xF000) >> 12) * 512];
            }
            else // Standard palette
            {
                pal = &palette[0x200];
            }

            for (int j = 0; j < width; j++)
            {
                // Determine the horizontal pixel offset based on whether or not the sprite is horizontally flipped
                int offset = (object[1] & BIT(12)) ? (x + width - j - 1) : (x + j);

                // Get the appropriate palette index from the tile for the current position
                uint8_t index = tile[(j / 8) * 64 + j % 8];

                // Draw a pixel if one exists at the current position
                if (offset >= 0 && offset < 256 && index)
                {
                    if (type == 2) // Object window
                    {
                        // Mark object window pixels with an extra bit, and don't actually draw anything
                        framebuffer[line * 256 + offset] |= BIT(24);
                    }
                    else
                    {
                        // Draw a pixel
                        layer[line * 256 + offset] = rgb5ToRgba6(U8TO16(pal, index * 2) | BIT(15));

                        // Mark semi-transparent pixels with an extra bit
                        if (type == 1) layer[line * 256 + offset] |= BIT(25);
                    }
                }
            }
        }
        else // 4-bit
        {
            // Adjust the current tile to align with the current Y coordinate relative to the object
            int mapWidth = (dispCnt & BIT(4)) ? width : 256;
            if (object[1] & BIT(13)) // Vertical flip
                tile += (7 - (spriteY % 8) + ((height - 1 - spriteY) / 8) * mapWidth) * 4;
            else
                tile += ((spriteY % 8) + (spriteY / 8) * mapWidth) * 4;

            // Get the palette of the object
            // In 4-bit mode, the object can select from multiple 16-color palettes
            uint8_t *pal = &palette[0x200 + ((object[2] & 0xF000) >> 12) * 32];

            for (int j = 0; j < width; j++)
            {
                // Determine the horizontal pixel offset based on whether or not the sprite is horizontally flipped
                int offset = (object[1] & BIT(12)) ? (x + width - j - 1) : (x + j);

                // Get the appropriate palette index from the tile for the current position
                uint8_t index = tile[(j / 8) * 32 + (j / 2) % 4];
                index = (j & 1) ? ((index & 0xF0) >> 4) : (index & 0x0F);

                // Draw a pixel if one exists at the current position
                if (offset >= 0 && offset < 256 && index)
                {
                    if (type == 2) // Object window
                    {
                        // Mark object window pixels with an extra bit, and don't actually draw anything
                        framebuffer[line * 256 + offset] |= BIT(24);
                    }
                    else
                    {
                        // Draw a pixel
                        layer[line * 256 + offset] = rgb5ToRgba6(U8TO16(pal, index * 2) | BIT(15));

                        // Mark semi-transparent pixels with an extra bit
                        if (type == 1) layer[line * 256 + offset] |= BIT(25);
                    }
                }
            }
        }
    }
}

void Gpu2D::writeDispCnt(uint32_t mask, uint32_t value)
{
    // Write to the DISPCNT register
    mask &= (gpu3DRenderer ? 0xFFFFFFFF : 0xC0B1FFF7);
    dispCnt = (dispCnt & ~mask) | (value & mask);
}

void Gpu2D::writeBgCnt(int bg, uint16_t mask, uint16_t value)
{
    // Write to one of the BGCNT registers
    bgCnt[bg] = (bgCnt[bg] & ~mask) | (value & mask);
}

void Gpu2D::writeBgHOfs(int bg, uint16_t mask, uint16_t value)
{
    // Write to one of the BGHOFS registers
    mask &= 0x01FF;
    bgHOfs[bg] = (bgHOfs[bg] & ~mask) | (value & mask);
}

void Gpu2D::writeBgVOfs(int bg, uint16_t mask, uint16_t value)
{
    // Write to one of the BGVOFS registers
    mask &= 0x01FF;
    bgVOfs[bg] = (bgVOfs[bg] & ~mask) | (value & mask);
}

void Gpu2D::writeBgPA(int bg, uint16_t mask, uint16_t value)
{
    // Write to one of the BGPA registers
    bgPA[bg - 2] = (bgPA[bg - 2] & ~mask) | (value & mask);
}

void Gpu2D::writeBgPB(int bg, uint16_t mask, uint16_t value)
{
    // Write to one of the BGPB registers
    bgPB[bg - 2] = (bgPB[bg - 2] & ~mask) | (value & mask);
}

void Gpu2D::writeBgPC(int bg, uint16_t mask, uint16_t value)
{
    // Write to one of the BGPC registers
    bgPC[bg - 2] = (bgPC[bg - 2] & ~mask) | (value & mask);
}

void Gpu2D::writeBgPD(int bg, uint16_t mask, uint16_t value)
{
    // Write to one of the BGPD registers
    bgPD[bg - 2] = (bgPD[bg - 2] & ~mask) | (value & mask);
}

void Gpu2D::writeBgX(int bg, uint32_t mask, uint32_t value)
{
    // Write to one of the BGX registers
    mask &= 0x0FFFFFFF;
    bgX[bg - 2] = (bgX[bg - 2] & ~mask) | (value & mask);

    // Extend the sign to 32 bits
    if (bgX[bg - 2] & BIT(27)) bgX[bg - 2] |= 0xF0000000; else bgX[bg - 2] &= ~0xF0000000;
}

void Gpu2D::writeBgY(int bg, uint32_t mask, uint32_t value)
{
    // Write to one of the BGY registers
    mask &= 0x0FFFFFFF;
    bgY[bg - 2] = (bgY[bg - 2] & ~mask) | (value & mask);

    // Extend the sign to 32 bits
    if (bgY[bg - 2] & BIT(27)) bgY[bg - 2] |= 0xF0000000; else bgY[bg - 2] &= ~0xF0000000;
}


void Gpu2D::writeWinH(int win, uint16_t mask, uint16_t value)
{
    // Write to one of the WINH registers
    if (mask & 0x00FF) winX2[win] = (value & 0x00FF) >> 0;
    if (mask & 0xFF00) winX1[win] = (value & 0xFF00) >> 8;

    // Handle invalid values
    if (winX1[win] > winX2[win])
        winX2[win] = 256;
}

void Gpu2D::writeWinV(int win, uint16_t mask, uint16_t value)
{
    // Write to one of the WINV registers
    if (mask & 0x00FF) winY2[win] = (value & 0x00FF) >> 0;
    if (mask & 0xFF00) winY1[win] = (value & 0xFF00) >> 8;

    // Handle invalid values
    if (winY1[win] > winY2[win])
        winY2[win] = 192;
}

void Gpu2D::writeWinIn(uint16_t mask, uint16_t value)
{
    // Write to the WININ register
    mask &= 0x3F3F;
    winIn = (winIn & ~mask) | (value & mask);
}

void Gpu2D::writeWinOut(uint16_t mask, uint16_t value)
{
    // Write to the WINOUT register
    mask &= 0x3F3F;
    winOut = (winOut & ~mask) | (value & mask);
}

void Gpu2D::writeBldCnt(uint16_t mask, uint16_t value)
{
    // Write to the BLDCNT register
    mask &= 0x3FFF;
    bldCnt = (bldCnt & ~mask) | (value & mask);
}

void Gpu2D::writeBldAlpha(uint16_t mask, uint16_t value)
{
    // Write to the BLDALPHA register
    mask &= 0x1F1F;
    bldAlpha = (bldAlpha & ~mask) | (value & mask);
}

void Gpu2D::writeBldY(uint8_t value)
{
    // Write to the BLDY register
    bldY = value & 0x1F;
    if (bldY > 16) bldY = 16;
}

void Gpu2D::writeMasterBright(uint16_t mask, uint16_t value)
{
    // Write to the MASTER_BRIGHT register
    mask &= 0xC01F;
    masterBright = (masterBright & ~mask) | (value & mask);
}
