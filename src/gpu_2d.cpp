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

#include "gpu_2d.h"
#include "core.h"

Gpu2D::Gpu2D(Core *core, bool engine): core(core), engine(engine)
{
    if (engine == 0)
    {
        // Set up 2D GPU engine A
        bgVramAddr = 0x6000000;
        objVramAddr = 0x6400000;
        palette = core->memory.getPalette();
        oam = core->memory.getOam();
    }
    else
    {
        // Set up 2D GPU engine B
        bgVramAddr = 0x6200000;
        objVramAddr = 0x6600000;
        palette = core->memory.getPalette() + 0x400;
        oam = core->memory.getOam() + 0x400;
    }
}

uint32_t Gpu2D::rgb5ToRgb6(uint32_t color)
{
    // Convert an RGB5 value to an RGB6 value (the way the 2D engine does it)
    // Also keep the extra bits because some of them are used to keep track of stuff
    uint8_t r = ((color >>  0) & 0x1F) * 2;
    uint8_t g = ((color >>  5) & 0x1F) * 2;
    uint8_t b = ((color >> 10) & 0x1F) * 2;
    return (color & 0xFFFC0000) | (b << 12) | (g << 6) | r;
}

void Gpu2D::drawGbaScanline(int line)
{
    // Reload the internal registers at the start of the frame
    if (line == 0)
    {
        internalX[0] = bgX[0];
        internalX[1] = bgX[1];
        internalY[0] = bgY[0];
        internalY[1] = bgY[1];
    }

    // Clear the layers
    for (int i = 0; i < 5; i++)
        memset(layers[i], 0, 240 * sizeof(uint32_t));
    memset(objPrio, 4, 240 * sizeof(uint8_t));

    // Draw the background layers
    // The type of each layer depends on the BG mode
    switch (dispCnt & 0x0007)
    {
        case 0:
        {
            if (dispCnt & BIT(8))  drawText(0, line);
            if (dispCnt & BIT(9))  drawText(1, line);
            if (dispCnt & BIT(10)) drawText(2, line);
            if (dispCnt & BIT(11)) drawText(3, line);
            break;
        }

        case 1:
        {
            if (dispCnt & BIT(8))    drawText(0, line);
            if (dispCnt & BIT(9))    drawText(1, line);
            if (dispCnt & BIT(10)) drawAffine(2, line);
            break;
        }

        case 2:
        {
            if (dispCnt & BIT(10)) drawAffine(2, line);
            if (dispCnt & BIT(11)) drawAffine(3, line);
            break;
        }


        case 3: case 4: case 5:
        {
            if (dispCnt & BIT(10)) drawExtended(2, line);
            break;
        }

        default:
        {
            printf("Unknown GBA BG mode: %d\n", dispCnt & 0x0007);
            break;
        }
    }

    // Draw the objects
    if (dispCnt & BIT(12)) drawObjects(line);

    // Blend the layers to form the final image
    for (int i = 0; i < 240; i++)
    {
        uint8_t enabled = BIT(5) | (dispCnt >> 8);

        // If the current pixel is in the bounds of a window, disable layers that are disabled in that window
        if (dispCnt & 0xE000) // Windows enabled
        {
            if ((dispCnt & BIT(13)) && i >= winX1[0] && i < winX2[0] && line >= winY1[0] && line < winY2[0])
                enabled &= winIn >> 0; // Window 0
            else if ((dispCnt & BIT(14)) && i >= winX1[1] && i < winX2[1] && line >= winY1[1] && line < winY2[1])
                enabled &= winIn >> 8; // Window 1
            else if ((dispCnt & BIT(15)) && (framebuffer[line * 256 + i] & BIT(24)))
                enabled &= winOut >> 8; // Object window
            else
                enabled &= winOut >> 0; // Outside of windows
        }

        // Set the topmost two pixels to the backdrop color (first palette index)
        uint32_t pixel = U8TO16(palette, 0), pixel2 = pixel;
        int blendBit = 5, blendBit2 = 5;
        int priority = 4, priority2 = 4;

        // If an object pixel exists, set it to the topmost pixel
        // Objects are higher priority than background layers, so the priority is given a little boost
        if ((enabled & BIT(4)) && (layers[4][i] & BIT(15)))
        {
            pixel = layers[4][i];
            blendBit = 4;
            priority = objPrio[i] - 1;
        }

        // Look for pixels in the background layers
        for (int j = 3; j >= 0; j--)
        {
            // Update the topmost pixels if a higher priority pixel is found
            if ((enabled & BIT(j)) && (layers[j][i] & BIT(15)))
            {
                if ((bgCnt[j] & 0x0003) <= priority) // Higher than topmost
                {
                    // Move the topmost pixel to the second topmost
                    pixel2 = pixel;
                    blendBit2 = blendBit;
                    priority2 = priority;

                    // Update the topmost pixel
                    pixel = layers[j][i];
                    blendBit = j;
                    priority = (bgCnt[j] & 0x0003);
                }
                else if ((bgCnt[j] & 0x0003) <= priority2) // Higher than second topmost
                {
                    // Update the second topmost pixel
                    pixel2 = layers[j][i];
                    blendBit2 = j;
                    priority2 = (bgCnt[j] & 0x0003);
                }
            }
        }

        int mode = (bldCnt & 0x00C0) >> 6;
        bool blend = ((enabled & BIT(5)) && (bldCnt & BIT(blendBit)));

        // Blend the pixel if enabled
        // Semi-transparent objects are special cases that force alpha blending (marked by bit 25)
        // If special cases don't have a second target to blend with, they can fall back to brightness effects
        // Blending is done with 15-bit colors on the GBA
        if (((blend && mode == 1) || (pixel & BIT(25))) && (bldCnt & BIT(8 + blendBit2))) // Alpha blending
        {
            int eva = (bldAlpha & 0x001F) >> 0; if (eva > 16) eva = 16;
            int evb = (bldAlpha & 0x1F00) >> 8; if (evb > 16) evb = 16;
            int r = ((pixel >>  0) & 0x1F) * eva / 16 + ((pixel2 >>  0) & 0x1F) * evb / 16; if (r > 31) r = 31;
            int g = ((pixel >>  5) & 0x1F) * eva / 16 + ((pixel2 >>  5) & 0x1F) * evb / 16; if (g > 31) g = 31;
            int b = ((pixel >> 10) & 0x1F) * eva / 16 + ((pixel2 >> 10) & 0x1F) * evb / 16; if (b > 31) b = 31;
            pixel = (b << 10) | (g << 5) | r;
        }
        else if (blend && mode == 2) // Brightness increase
        {
            int r = (pixel >>  0) & 0x1F; r += (31 - r) * bldY / 16;
            int g = (pixel >>  5) & 0x1F; g += (31 - g) * bldY / 16;
            int b = (pixel >> 10) & 0x1F; b += (31 - b) * bldY / 16;
            pixel = (b << 10) | (g << 5) | r;
        }
        else if (blend && mode == 3) // Brightness decrease
        {
            int r = (pixel >>  0) & 0x1F; r -= r * bldY / 16;
            int g = (pixel >>  5) & 0x1F; g -= g * bldY / 16;
            int b = (pixel >> 10) & 0x1F; b -= b * bldY / 16;
            pixel = (b << 10) | (g << 5) | r;
        }

        // Write the pixel to the framebuffer, centered on the screen
        framebuffer[(line + 16) * 256 + 8 + i] = rgb5ToRgb6(pixel);
    }

    // At the end of the frame, copy the VRAM border to the framebuffer around the GBA display
    // The DS draws the GBA screen by capturing it to VRAM and then displaying that
    // The current frame is captured to one VRAM block and the previous is displayed from another
    // This introduces a frame of latency on hardware, but is avoided here by simply drawing directly to the framebuffer
    if (line == 159)
    {
        uint32_t base = 0x6800000 + gbaBlock * 0x20000;
        gbaBlock = !gbaBlock;

        for (int y = 0; y < 192; y++)
        {
            for (int x = 0; x < 256; x++)
            {
                if (x == 8 && y >= 16 && y < 192 - 16) x += 240;
                framebuffer[y * 256 + x] = rgb5ToRgb6(core->memory.read<uint16_t>(0, base + (y * 256 + x) * 2));
            }
        }
    }
}

void Gpu2D::drawScanline(int line)
{
    // Reload the internal registers at the start of the frame
    if (line == 0)
    {
        internalX[0] = bgX[0];
        internalX[1] = bgX[1];
        internalY[0] = bgY[0];
        internalY[1] = bgY[1];
    }

    // Clear the layers
    for (int i = 0; i < 5; i++)
        memset(layers[i], 0, 256 * sizeof(uint32_t));
    memset(objPrio, 4, 256 * sizeof(uint8_t));

    // Draw the background layers
    // The type of each layer depends on the BG mode
    switch (dispCnt & 0x00000007)
    {
        case 0:
        {
            if (dispCnt & BIT(8))  drawText(0, line);
            if (dispCnt & BIT(9))  drawText(1, line);
            if (dispCnt & BIT(10)) drawText(2, line);
            if (dispCnt & BIT(11)) drawText(3, line);
            break;
        }

        case 1:
        {
            if (dispCnt & BIT(8))    drawText(0, line);
            if (dispCnt & BIT(9))    drawText(1, line);
            if (dispCnt & BIT(10))   drawText(2, line);
            if (dispCnt & BIT(11)) drawAffine(3, line);
            break;
        }

        case 2:
        {
            if (dispCnt & BIT(8))    drawText(0, line);
            if (dispCnt & BIT(9))    drawText(1, line);
            if (dispCnt & BIT(10)) drawAffine(2, line);
            if (dispCnt & BIT(11)) drawAffine(3, line);
            break;
        }

        case 3:
        {
            if (dispCnt & BIT(8))      drawText(0, line);
            if (dispCnt & BIT(9))      drawText(1, line);
            if (dispCnt & BIT(10))     drawText(2, line);
            if (dispCnt & BIT(11)) drawExtended(3, line);
            break;
        }

        case 4:
        {
            if (dispCnt & BIT(8))      drawText(0, line);
            if (dispCnt & BIT(9))      drawText(1, line);
            if (dispCnt & BIT(10))   drawAffine(2, line);
            if (dispCnt & BIT(11)) drawExtended(3, line);
            break;
        }

        case 5:
        {
            if (dispCnt & BIT(8))      drawText(0, line);
            if (dispCnt & BIT(9))      drawText(1, line);
            if (dispCnt & BIT(10)) drawExtended(2, line);
            if (dispCnt & BIT(11)) drawExtended(3, line);
            break;
        }

        case 6:
        {
            if (dispCnt & BIT(10)) drawLarge(2, line);
            break;
        }

        default:
        {
            printf("Unknown engine %c BG mode: %d\n", ((engine == 0) ? 'A' : 'B'), dispCnt & 0x00000007);
            break;
        }
    }

    // Draw the objects
    if (dispCnt & BIT(12)) drawObjects(line);

    // Blend the layers to form the final image
    for (int i = 0; i < 256; i++)
    {
        uint8_t enabled = BIT(5) | (dispCnt >> 8);
        uint32_t *pixel = &framebuffer[line * 256 + i];

        // If the current pixel is in the bounds of a window, disable layers that are disabled in that window
        if (dispCnt & 0x0000E000) // Windows enabled
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

        // Set the topmost two pixels to the backdrop color (first palette index)
        uint32_t pixel2 = *pixel = U8TO16(palette, 0);
        int blendBit = 5, blendBit2 = 5;
        int priority = 4, priority2 = 4;

        // If an object pixel exists, set it to the topmost pixel
        // Objects are higher priority than background layers, so the priority is given a little boost
        if ((enabled & BIT(4)) && (layers[4][i] & BIT(15)))
        {
            *pixel = layers[4][i];
            blendBit = 4;
            priority = objPrio[i] - 1;
        }

        // Look for pixels in the background layers
        for (int j = 3; j >= 0; j--)
        {
            // Update the topmost pixels if a higher priority pixel is found
            // 3D pixels (marked by bit 26) are special cases that have higher-precision alpha values
            if ((enabled & BIT(j)) && (layers[j][i] & ((layers[j][i] & BIT(26)) ? 0xFC0000 : BIT(15))))
            {
                if ((bgCnt[j] & 0x0003) <= priority) // Higher than topmost
                {
                    // Move the topmost pixel to the second topmost
                    pixel2 = *pixel;
                    blendBit2 = blendBit;
                    priority2 = priority;

                    // Update the topmost pixel
                    *pixel = layers[j][i];
                    blendBit = j;
                    priority = (bgCnt[j] & 0x0003);
                }
                else if ((bgCnt[j] & 0x0003) <= priority2) // Higher than second topmost
                {
                    // Update the second topmost pixel
                    pixel2 = layers[j][i];
                    blendBit2 = j;
                    priority2 = (bgCnt[j] & 0x0003);
                }
            }
        }

        // Convert the pixels to 18-bit if they aren't 3D pixels that were already 18-bit
        if (!(*pixel & BIT(26))) *pixel = rgb5ToRgb6(*pixel);
        if (!(pixel2 & BIT(26))) pixel2 = rgb5ToRgb6(pixel2);

        int mode = (bldCnt & 0x00C0) >> 6;
        bool blend = ((enabled & BIT(5)) && (bldCnt & BIT(blendBit)));

        // Blend the pixel if enabled
        // Semi-transparent objects and 3D are special cases that force alpha blending (marked by bits 25 and 26)
        // If special cases don't have a second target to blend with, they can fall back to brightness effects
        // Blending is done with 18-bit colors on the DS
        if (((blend && mode == 1) || (*pixel & (BIT(25) | BIT(26)))) && (bldCnt & BIT(8 + blendBit2))) // Alpha blending
        {
            if (*pixel & BIT(26)) // 3D
            {
                int eva = ((*pixel >> 18) & 0x3F) + 1;
                int evb = 64 - eva;
                int r = ((*pixel >>  0) & 0x3F) * eva / 64 + ((pixel2 >>  0) & 0x3F) * evb / 64; if (r > 63) r = 63;
                int g = ((*pixel >>  6) & 0x3F) * eva / 64 + ((pixel2 >>  6) & 0x3F) * evb / 64; if (g > 63) g = 63;
                int b = ((*pixel >> 12) & 0x3F) * eva / 64 + ((pixel2 >> 12) & 0x3F) * evb / 64; if (b > 63) b = 63;
                *pixel = (b << 12) | (g << 6) | r;
            }
            else
            {
                int eva = (bldAlpha & 0x001F) >> 0; if (eva > 16) eva = 16;
                int evb = (bldAlpha & 0x1F00) >> 8; if (evb > 16) evb = 16;
                int r = ((*pixel >>  0) & 0x3F) * eva / 16 + ((pixel2 >>  0) & 0x3F) * evb / 16; if (r > 63) r = 63;
                int g = ((*pixel >>  6) & 0x3F) * eva / 16 + ((pixel2 >>  6) & 0x3F) * evb / 16; if (g > 63) g = 63;
                int b = ((*pixel >> 12) & 0x3F) * eva / 16 + ((pixel2 >> 12) & 0x3F) * evb / 16; if (b > 63) b = 63;
                *pixel = (b << 12) | (g << 6) | r;
            }
        }
        else if (blend && mode == 2) // Brightness increase
        {
            int r = (*pixel >>  0) & 0x3F; r += (63 - r) * bldY / 16;
            int g = (*pixel >>  6) & 0x3F; g += (63 - g) * bldY / 16;
            int b = (*pixel >> 12) & 0x3F; b += (63 - b) * bldY / 16;
            *pixel = (b << 12) | (g << 6) | r;
        }
        else if (blend && mode == 3) // Brightness decrease
        {
            int r = (*pixel >>  0) & 0x3F; r -= r * bldY / 16;
            int g = (*pixel >>  6) & 0x3F; g -= g * bldY / 16;
            int b = (*pixel >> 12) & 0x3F; b -= b * bldY / 16;
            *pixel = (b << 12) | (g << 6) | r;
        }
    }
}

void Gpu2D::finishScanline(int line)
{
    // Redraw the scanline if the display isn't set to layer mode
    switch ((dispCnt & 0x00030000) >> 16) // Display mode
    {
        case 0: // Display off
        {
            // Fill the display with white
            memset(&framebuffer[line * 256], 0xFF, 256 * sizeof(uint32_t));
            break;
        }

        case 2: // VRAM display
        {
            // Draw raw bitmap data from a VRAM block
            uint32_t address = 0x6800000 + ((dispCnt & 0x000C0000) >> 18) * 0x20000 + line * 256 * 2;
            for (int i = 0; i < 256; i++)
                framebuffer[line * 256 + i] = rgb5ToRgb6(core->memory.read<uint16_t>(0, address + i * 2));
            break;
        }

        case 3: // Main memory display
        {
            printf("Unimplemented engine %c display mode: display FIFO\n", ((engine == 0) ? 'A' : 'B'));
            break;
        }
    }

    // Apply the master brightness
    // This is only on the DS, and is done with 18-bit colors
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
    if (!core->isGbaMode() && bg == 0 && (dispCnt & BIT(3)))
    {
        memcpy(layers[bg], &core->gpu3DRenderer.getFramebuffer()[line * 256], 256 * sizeof(uint32_t));
        return;
    }

    // Get the base data addresses
    uint32_t tileBase  = bgVramAddr + ((bgCnt[bg] & 0x1F00) >> 8) * 0x0800 + ((dispCnt & 0x38000000) >> 27) * 0x10000;
    uint32_t indexBase = bgVramAddr + ((bgCnt[bg] & 0x003C) >> 2) * 0x4000 + ((dispCnt & 0x07000000) >> 24) * 0x10000;

    // Move the tile address to the current line
    int yOffset = (line + bgVOfs[bg]) % 512;
    tileBase += ((yOffset / 8) % 32) * 64;

    // If the Y-offset exceeds 256 and the background is 512 pixels tall, move to the next 256x256 section
    // If the background is 512 pixels wide, move 2 sections to skip the second X section
    if (yOffset >= 256 && (bgCnt[bg] & BIT(15)))
        tileBase += (bgCnt[bg] & BIT(14)) ? 0x1000 : 0x800;

    // Draw a line
    if (bgCnt[bg] & BIT(7)) // 8-bit
    {
        for (int i = 0; i <= 256; i += 8)
        {
            // Move the tile address to the current tile
            int xOffset = (bgHOfs[bg] + i) % 512;
            uint32_t tileAddr = tileBase + ((xOffset / 8) % 32) * 2;

            // If the X-offset exceeds 256 and the background is 512 pixels wide, move to the next 256x256 section
            if (xOffset >= 256 && (bgCnt[bg] & BIT(14)))
                tileAddr += 0x800;

            // Get the current tile
            uint16_t tile = core->memory.read<uint16_t>(core->isGbaMode(), tileAddr);

            // Get the tile's palette
            uint8_t *pal;
            if (dispCnt & BIT(30)) // Extended palette
            {
                // Determine the extended palette slot
                // Backgrounds 0 and 1 can alternatively use slots 2 and 3
                int slot = (bg < 2 && (bgCnt[bg] & BIT(13))) ? (bg + 2) : bg;

                // In extended palette mode, the tile can select from multiple 256-color palettes
                if (!extPalettes[slot]) return;
                pal = &extPalettes[slot][((tile & 0xF000) >> 12) * 512];
            }
            else // Standard palette
            {
                pal = palette;
            }

            // Get the palette indices for the current line of the tile, flipped vertically if enabled
            uint32_t indexAddr = indexBase + (tile & 0x03FF) * 64 + ((tile & BIT(11)) ? ((7 - yOffset % 8) * 8) : ((yOffset % 8) * 8));
            uint64_t indices = core->memory.read<uint32_t>(core->isGbaMode(), indexAddr) |
                ((uint64_t)core->memory.read<uint32_t>(core->isGbaMode(), indexAddr + 4) << 32);

            // Draw the current line of the tile
            for (int j = 0; j < 8; j++)
            {
                // Flip the tile horizontally if enabled
                int offset = i - (xOffset % 8) + ((tile & BIT(10)) ? (7 - j) : j);

                // Draw a pixel
                if (offset >= 0 && offset < 256 && (indices & 0xFF))
                    layers[bg][offset] = U8TO16(pal, (indices & 0xFF) * 2) | BIT(15);

                // Move to the next palette index
                indices >>= 8;
            }
        }
    }
    else // 4-bit
    {
        for (int i = 0; i <= 256; i += 8)
        {
            // Move the tile address to the current tile
            int xOffset = (bgHOfs[bg] + i) % 512;
            uint32_t tileAddr = tileBase + ((xOffset / 8) % 32) * 2;

            // If the X-offset exceeds 256 and the background is 512 pixels wide, move to the next 256x256 section
            if (xOffset >= 256 && (bgCnt[bg] & BIT(14)))
                tileAddr += 0x800;

            // Get the current tile
            uint16_t tile = core->memory.read<uint16_t>(core->isGbaMode(), tileAddr);

            // Get the tile's palette
            // In 4-bit mode, the tile can select from multiple 16-color palettes
            uint8_t *pal = &palette[((tile & 0xF000) >> 12) * 32];

            // Get the palette indices for the current line of the tile, flipped vertically if enabled
            uint32_t indexAddr = indexBase + (tile & 0x03FF) * 32 + ((tile & BIT(11)) ? ((7 - yOffset % 8) * 4) : ((yOffset % 8) * 4));
            uint32_t indices = core->memory.read<uint32_t>(core->isGbaMode(), indexAddr);

            // Draw the current line of the tile
            for (int j = 0; j < 8; j++)
            {
                // Flip the tile horizontally if enabled
                int offset = i - (xOffset % 8) + ((tile & BIT(10)) ? (7 - j) : j);

                // Draw a pixel
                if (offset >= 0 && offset < 256 && (indices & 0xF))
                    layers[bg][offset] = U8TO16(pal, (indices & 0xF) * 2) | BIT(15);

                // Move to the next palette index
                indices >>= 4;
            }
        }
    }
}

void Gpu2D::drawAffine(int bg, int line)
{
    // Get the base data addresses
    uint32_t tileBase  = bgVramAddr + ((bgCnt[bg] & 0x1F00) >> 8) * 0x0800 + ((dispCnt & 0x38000000) >> 27) * 0x10000;
    uint32_t indexBase = bgVramAddr + ((bgCnt[bg] & 0x003C) >> 2) * 0x4000 + ((dispCnt & 0x07000000) >> 24) * 0x10000;

    // Get the background's size
    int size = 128 << ((bgCnt[bg] & 0xC000) >> 14);

    // Get the background's palette
    uint8_t *pal;
    if (dispCnt & BIT(30)) // Extended palette
    {
        if (!extPalettes[bg]) return;
        pal = extPalettes[bg];
    }
    else // Standard palette
    {
        pal = palette;
    }

    // Draw a line
    for (int i = 0; i < 256; i++)
    {
        // Calculate the rotscaled coordinates relative to the background
        int rotscaleX = (internalX[bg - 2] + bgPA[bg - 2] * i) >> 8;
        int rotscaleY = (internalY[bg - 2] + bgPC[bg - 2] * i) >> 8;

        // Handle display area overflow
        if (bg < 2 || (bgCnt[bg] & BIT(13))) // Wraparound
        {
            rotscaleX %= size; if (rotscaleX < 0) rotscaleX += size;
            rotscaleY %= size; if (rotscaleY < 0) rotscaleY += size;
        }
        else if (rotscaleX < 0 || rotscaleX >= size || rotscaleY < 0 || rotscaleY >= size) // Transparent
        {
            continue;
        }

        // Get the current tile
        uint8_t tile = core->memory.read<uint8_t>(core->isGbaMode(), tileBase + (rotscaleY / 8) * (size / 8) + (rotscaleX / 8));

        // Get the palette index for the current pixel of the tile
        uint32_t indexAddr = indexBase + tile * 64 + (rotscaleY % 8) * 8 + (rotscaleX % 8);
        uint8_t index = core->memory.read<uint8_t>(core->isGbaMode(), indexAddr);

        // Draw a pixel
        if (index)
            layers[bg][i] = U8TO16(pal, index * 2) | BIT(15);
    }

    // Increment the internal registers at the end of the scanline
    internalX[bg - 2] += bgPB[bg - 2];
    internalY[bg - 2] += bgPD[bg - 2];
}

void Gpu2D::drawExtended(int bg, int line)
{
    if (core->isGbaMode() || (bgCnt[bg] & BIT(7))) // Bitmap
    {
        uint32_t dataBase = bgVramAddr;
        int sizeX, sizeY;

        // Get information about the bitmap data
        if (core->isGbaMode())
        {
            // Get the base data address
            dataBase += ((bgCnt[bg] & 0x1F00) >> 8) * 0x0800;

            // Modes 4 and 5 have two bitmaps; one can be drawn while the other is displayed
            // Draw the second bitmap if enabled
            if ((dispCnt & BIT(4)) && (dispCnt & 0x0007) > 3)
                dataBase += 0xA000;

            // Get the bitmap size
            switch (dispCnt & 0x0007)
            {
                case 5:  sizeX = 160; sizeY = 128; break;
                default: sizeX = 240; sizeY = 160; break;
            }
        }
        else
        {
            // Get the base data address
            dataBase += ((bgCnt[bg] & 0x1F00) >> 8) * 0x4000;

            // Get the bitmap size
            switch ((bgCnt[bg] & 0xC000) >> 14)
            {
                case 0: sizeX = 128; sizeY = 128; break;
                case 1: sizeX = 256; sizeY = 256; break;
                case 2: sizeX = 512; sizeY = 256; break;
                case 3: sizeX = 512; sizeY = 512; break;
            }
        }

        if ((core->isGbaMode() && (dispCnt & 0x0007) != 4) || (!core->isGbaMode() && (bgCnt[bg] & BIT(2)))) // Direct color bitmap
        {
            // Draw a line
            for (int i = 0; i < 256; i++)
            {
                // Calculate the rotscaled coordinates relative to the background
                int rotscaleX = (internalX[bg - 2] + bgPA[bg - 2] * i) >> 8;
                int rotscaleY = (internalY[bg - 2] + bgPC[bg - 2] * i) >> 8;

                // Handle display area overflow
                if (bgCnt[bg] & BIT(13)) // Wraparound
                {
                    rotscaleX %= sizeX; if (rotscaleX < 0) rotscaleX += sizeX;
                    rotscaleY %= sizeY; if (rotscaleY < 0) rotscaleY += sizeY;
                }
                else if (rotscaleX < 0 || rotscaleX >= sizeX || rotscaleY < 0 || rotscaleY >= sizeY) // Transparent
                {
                    continue;
                }

                // Draw a pixel
                layers[bg][i] = core->memory.read<uint16_t>(core->isGbaMode(), dataBase + (rotscaleY * sizeX + rotscaleX) * 2);

                // Ignore transparency in GBA mode
                if (core->isGbaMode())
                    layers[bg][i] |= BIT(15);
            }
        }
        else // 256 color bitmap
        {
            // Draw a line
            for (int i = 0; i < 256; i++)
            {
                // Calculate the rotscaled coordinates relative to the background
                int rotscaleX = (internalX[bg - 2] + bgPA[bg - 2] * i) >> 8;
                int rotscaleY = (internalY[bg - 2] + bgPC[bg - 2] * i) >> 8;

                // Handle display area overflow
                if (bgCnt[bg] & BIT(13)) // Wraparound
                {
                    rotscaleX %= sizeX; if (rotscaleX < 0) rotscaleX += sizeX;
                    rotscaleY %= sizeY; if (rotscaleY < 0) rotscaleY += sizeY;
                }
                else if (rotscaleX < 0 || rotscaleX >= sizeX || rotscaleY < 0 || rotscaleY >= sizeY) // Transparent
                {
                    continue;
                }

                // Get the palette index for the current pixel
                uint8_t index = core->memory.read<uint8_t>(core->isGbaMode(), dataBase + rotscaleY * sizeX + rotscaleX);

                // Draw a pixel
                if (index)
                    layers[bg][i] = U8TO16(palette, index * 2) | BIT(15);
            }
        }
    }
    else // Extended affine
    {
        // Get the base data addresses
        uint32_t tileBase  = bgVramAddr + ((bgCnt[bg] & 0x1F00) >> 8) * 0x0800 + ((dispCnt & 0x38000000) >> 27) * 0x10000;
        uint32_t indexBase = bgVramAddr + ((bgCnt[bg] & 0x003C) >> 2) * 0x4000 + ((dispCnt & 0x07000000) >> 24) * 0x10000;

        // Get the background's size
        int size = 128 << ((bgCnt[bg] & 0xC000) >> 14);

        // Draw a line
        for (int i = 0; i < 256; i++)
        {
            // Calculate the rotscaled coordinates relative to the background
            int rotscaleX = (internalX[bg - 2] + bgPA[bg - 2] * i) >> 8;
            int rotscaleY = (internalY[bg - 2] + bgPC[bg - 2] * i) >> 8;

            // Handle display area overflow
            if (bg < 2 || (bgCnt[bg] & BIT(13))) // Wraparound
            {
                rotscaleX %= size; if (rotscaleX < 0) rotscaleX += size;
                rotscaleY %= size; if (rotscaleY < 0) rotscaleY += size;
            }
            else if (rotscaleX < 0 || rotscaleX >= size || rotscaleY < 0 || rotscaleY >= size) // Transparent
            {
                continue;
            }

            // Get the current tile
            uint32_t tileAddr = tileBase + ((rotscaleY / 8) * (size / 8) + (rotscaleX / 8)) * 2;
            uint16_t tile = core->memory.read<uint16_t>(core->isGbaMode(), tileAddr);

            // Get the tile's palette
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

            // Get the palette index for the current pixel of the tile, flipped vertically or horizontally if enabled
            uint32_t indexAddr = indexBase + (tile & 0x03FF) * 64;
            indexAddr += ((tile & BIT(11)) ? (7 - rotscaleY % 8) : (rotscaleY % 8)) * 8;
            indexAddr += ((tile & BIT(10)) ? (7 - rotscaleX % 8) : (rotscaleX % 8));
            uint8_t index = core->memory.read<uint8_t>(core->isGbaMode(), indexAddr);

            // Draw a pixel
            if (index)
                layers[bg][i] = U8TO16(pal, index * 2) | BIT(15);
        }
    }

    // Increment the internal registers at the end of the scanline
    internalX[bg - 2] += bgPB[bg - 2];
    internalY[bg - 2] += bgPD[bg - 2];
}

void Gpu2D::drawLarge(int bg, int line)
{
    // Get the bitmap size
    int sizeX, sizeY;
    if (((bgCnt[bg] & 0xC000) >> 14) == 0)
    {
        sizeX =  512;
        sizeY = 1024;
    }
    else
    {
        sizeX = 1024;
        sizeY =  512;
    }

    // Draw a line
    for (int i = 0; i < 256; i++)
    {
        // Calculate the rotscaled coordinates relative to the background
        int rotscaleX = (internalX[bg - 2] + bgPA[bg - 2] * i) >> 8;
        int rotscaleY = (internalY[bg - 2] + bgPC[bg - 2] * i) >> 8;

        // Handle display area overflow
        if (bgCnt[bg] & BIT(13)) // Wraparound
        {
            rotscaleX %= sizeX; if (rotscaleX < 0) rotscaleX += sizeX;
            rotscaleY %= sizeY; if (rotscaleY < 0) rotscaleY += sizeY;
        }
        else if (rotscaleX < 0 || rotscaleX >= sizeX || rotscaleY < 0 || rotscaleY >= sizeY) // Transparent
        {
            continue;
        }

        // A full large bitmap requires 512KB of VRAM, but engine B can only use 128KB
        // For engine B, wrap the 128KB bitmap 4 times to cover the full area
        if (engine == 1)
            rotscaleY %= sizeY / 4;

        // Get the palette index for the current pixel
        uint8_t index = core->memory.read<uint8_t>(0, bgVramAddr + rotscaleY * sizeX + rotscaleX);

        // Draw a pixel
        if (index)
            layers[bg][i] = U8TO16(palette, index * 2) | BIT(15);
    }

    // Increment the internal registers at the end of the scanline
    internalX[bg - 2] += bgPB[bg - 2];
    internalY[bg - 2] += bgPD[bg - 2];
}

void Gpu2D::drawObjects(int line)
{
    // Loop through and draw the 128 sprites in OAM
    for (int i = 0; i < 128; i++)
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

        int type = (object[0] & 0x0C00) >> 10;
        uint8_t prio = (object[2] & 0x0C00) >> 10;

        // Draw bitmap objects
        if (!core->isGbaMode() && type == 3)
        {
            uint32_t dataBase;
            int bitmapWidth;

            // Determine the address and width of the bitmap
            if (dispCnt & BIT(6)) // 1D mapping
            {
                dataBase = objVramAddr + (object[2] & 0x03FF) * ((dispCnt & BIT(22)) ? 256 : 128);
                bitmapWidth = width;
            }
            else // 2D mapping
            {
                uint8_t xMask = (dispCnt & BIT(5)) ? 0x1F : 0x0F;
                dataBase = objVramAddr + (object[2] & 0x03FF & xMask) * 0x10 + (object[2] & 0x03FF & ~xMask) * 0x80;
                bitmapWidth = (dispCnt & BIT(5)) ? 256 : 128;
            }

            if (object[0] & BIT(8)) // Rotscale
            {
                // Get the rotscale parameters
                int params[4];
                for (int j = 0; j < 4; j++)
                    params[j] = (int16_t)U8TO16(oam, ((object[1] & 0x3E00) >> 9) * 0x20 + j * 8 + 6);

                // Draw a line of the object
                for (int j = 0; j < width2; j++)
                {
                    int offset = x + j;
                    if (offset < 0 || offset >= 256) continue;

                    // Calculate the rotscaled X coordinate relative to the object
                    int rotscaleX = ((params[0] * (j - width2 / 2) + params[1] * (spriteY - height2 / 2)) >> 8) + width / 2;
                    if (rotscaleX < 0 || rotscaleX >= width) continue;

                    // Calculate the rotscaled Y coordinate relative to the object
                    int rotscaleY = ((params[2] * (j - width2 / 2) + params[3] * (spriteY - height2 / 2)) >> 8) + height / 2;
                    if (rotscaleY < 0 || rotscaleY >= height) continue;

                    // Draw a pixel if the old one is lower priority
                    uint16_t pixel = core->memory.read<uint16_t>(0, dataBase + (rotscaleY * bitmapWidth + rotscaleX) * 2);
                    if ((pixel & BIT(15)) && prio < objPrio[offset])
                    {
                        layers[4][offset] = pixel;
                        objPrio[offset] = prio;
                    }
                }
            }
            else
            {
                // Draw a line of the object
                for (int j = 0; j < width; j++)
                {
                    int offset = x + j;
                    if (offset < 0 || offset >= 256) continue;

                    // Draw a pixel if the old one is lower priority
                    uint16_t pixel = core->memory.read<uint16_t>(0, dataBase + (spriteY * bitmapWidth + j) * 2);
                    if ((pixel & BIT(15)) && prio < objPrio[offset])
                    {
                        layers[4][offset] = pixel;
                        objPrio[offset] = prio;
                    }
                }
            }

            continue;
        }

        // Get the base tile address
        uint32_t tileBase;
        if (core->isGbaMode())
        {
            tileBase = 0x6010000 + (object[2] & 0x03FF) * 32;
        }
        else
        {
            // On the DS, the boundary between tiles can be 32, 64, 128, or 256 bytes for 1D tile mapping
            uint16_t bound = (dispCnt & BIT(4)) ? (32 << ((dispCnt & 0x00300000) >> 20)) : 32;
            tileBase = objVramAddr + (object[2] & 0x03FF) * bound;
        }

        if (object[0] & BIT(8)) // Rotscale
        {
            // Get the rotscale parameters
            int params[4];
            for (int j = 0; j < 4; j++)
                params[j] = (int16_t)U8TO16(oam, ((object[1] & 0x3E00) >> 9) * 0x20 + j * 8 + 6);

            if (object[0] & BIT(13)) // 8-bit
            {
                int mapWidth = (dispCnt & BIT(core->isGbaMode() ? 6 : 4)) ? width : 128;

                // Get the object's palette
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

                // Draw a line of the object
                for (int j = 0; j < width2; j++)
                {
                    int offset = x + j;
                    if (offset < 0 || offset >= 256) continue;

                    // Calculate the rotscaled X coordinate relative to the object
                    int rotscaleX = ((params[0] * (j - width2 / 2) + params[1] * (spriteY - height2 / 2)) >> 8) + width / 2;
                    if (rotscaleX < 0 || rotscaleX >= width) continue;

                    // Calculate the rotscaled Y coordinate relative to the object
                    int rotscaleY = ((params[2] * (j - width2 / 2) + params[3] * (spriteY - height2 / 2)) >> 8) + height / 2;
                    if (rotscaleY < 0 || rotscaleY >= height) continue;

                    // Get the palette index for the current pixel
                    uint8_t index = core->memory.read<uint8_t>(core->isGbaMode(), tileBase +
                        ((rotscaleY / 8) * mapWidth + rotscaleY % 8) * 8 + (rotscaleX / 8) * 64 + rotscaleX % 8);

                    if (index && type == 2) // Object window
                    {
                        // Mark object window pixels with an extra bit, and don't draw anything
                        framebuffer[line * 256 + offset] |= BIT(24);
                    }
                    else
                    {
                        // Draw a pixel if the old one is transparent or lower priority
                        // Semi-transparent pixels are marked with an extra bit
                        if (index && (!(layers[4][offset] & BIT(15)) || prio < objPrio[offset]))
                        {
                            layers[4][offset] = ((type == 1) ? BIT(25) : 0) | BIT(15) | U8TO16(pal, index * 2);
                            objPrio[offset] = prio;
                        }

                        // Update the priority even if the pixel is transparent
                        // This is a quirky behavior of the GBA, but it seems to have been fixed on the DS
                        if (core->isGbaMode() && prio < objPrio[offset])
                            objPrio[offset] = prio;
                    }
                }
            }
            else // 4-bit
            {
                int mapWidth = (dispCnt & BIT(core->isGbaMode() ? 6 : 4)) ? width : 256;

                // Get the object's palette
                // In 4-bit mode, the object can select from multiple 16-color palettes
                uint8_t *pal = &palette[0x200 + ((object[2] & 0xF000) >> 12) * 32];

                // Draw a line of the object
                for (int j = 0; j < width2; j++)
                {
                    int offset = x + j;
                    if (offset < 0 || offset >= 256) continue;

                    // Calculate the rotscaled X coordinate relative to the object
                    int rotscaleX = ((params[0] * (j - width2 / 2) + params[1] * (spriteY - height2 / 2)) >> 8) + width / 2;
                    if (rotscaleX < 0 || rotscaleX >= width) continue;

                    // Calculate the rotscaled Y coordinate relative to the object
                    int rotscaleY = ((params[2] * (j - width2 / 2) + params[3] * (spriteY - height2 / 2)) >> 8) + height / 2;
                    if (rotscaleY < 0 || rotscaleY >= height) continue;

                    // Get the palette index for the current pixel
                    uint8_t index = core->memory.read<uint8_t>(core->isGbaMode(), tileBase +
                        ((rotscaleY / 8) * mapWidth + rotscaleY % 8) * 4 + (rotscaleX / 8) * 32 + (rotscaleX % 8) / 2);
                    index = (rotscaleX % 2 == 1) ? ((index & 0xF0) >> 4) : (index & 0x0F);

                    if (index && type == 2) // Object window
                    {
                        // Mark object window pixels with an extra bit, and don't draw anything
                        framebuffer[line * 256 + offset] |= BIT(24);
                    }
                    else
                    {
                        // Draw a pixel if the old one is transparent or lower priority
                        // Semi-transparent pixels are marked with an extra bit
                        if (index && (!(layers[4][offset] & BIT(15)) || prio < objPrio[offset]))
                        {
                            layers[4][offset] = ((type == 1) ? BIT(25) : 0) | BIT(15) | U8TO16(pal, index * 2);
                            objPrio[offset] = prio;
                        }

                        // Update the priority even if the pixel is transparent
                        // This is a quirky behavior of the GBA, but it seems to have been fixed on the DS
                        if (core->isGbaMode() && prio < objPrio[offset])
                            objPrio[offset] = prio;
                    }
                }
            }
        }
        else if (object[0] & BIT(13)) // 8-bit
        {
            // Adjust the current tile to align with the current Y coordinate relative to the object
            int mapWidth = (dispCnt & BIT(core->isGbaMode() ? 6 : 4)) ? width : 128;
            if (object[1] & BIT(13)) // Vertical flip
                tileBase += (7 - (spriteY % 8) + ((height - 1 - spriteY) / 8) * mapWidth) * 8;
            else
                tileBase += ((spriteY % 8) + (spriteY / 8) * mapWidth) * 8;

            // Get the object's palette
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

            // Draw a line of the object
            for (int j = 0; j < width; j++)
            {
                // Determine the horizontal pixel offset based on whether or not the sprite is horizontally flipped
                int offset = (object[1] & BIT(12)) ? (x + width - j - 1) : (x + j);
                if (offset < 0 || offset >= 256) continue;

                // Get the palette index for the current pixel
                uint8_t index = core->memory.read<uint8_t>(core->isGbaMode(), tileBase + (j / 8) * 64 + j % 8);

                if (index && type == 2) // Object window
                {
                    // Mark object window pixels with an extra bit, and don't draw anything
                    framebuffer[line * 256 + offset] |= BIT(24);
                }
                else
                {
                    // Draw a pixel if the old one is transparent or lower priority
                    // Semi-transparent pixels are marked with an extra bit
                    if (index && (!(layers[4][offset] & BIT(15)) || prio < objPrio[offset]))
                    {
                        layers[4][offset] = ((type == 1) ? BIT(25) : 0) | BIT(15) | U8TO16(pal, index * 2);
                        objPrio[offset] = prio;
                    }

                    // Update the priority even if the pixel is transparent
                    // This is a quirky behavior of the GBA, but it seems to have been fixed on the DS
                    if (core->isGbaMode() && prio < objPrio[offset])
                        objPrio[offset] = prio;
                }
            }
        }
        else // 4-bit
        {
            // Adjust the current tile to align with the current Y coordinate relative to the object
            int mapWidth = (dispCnt & BIT(core->isGbaMode() ? 6 : 4)) ? width : 256;
            if (object[1] & BIT(13)) // Vertical flip
                tileBase += (7 - (spriteY % 8) + ((height - 1 - spriteY) / 8) * mapWidth) * 4;
            else
                tileBase += ((spriteY % 8) + (spriteY / 8) * mapWidth) * 4;

            // Get the palette of the object
            // In 4-bit mode, the object can select from multiple 16-color palettes
            uint8_t *pal = &palette[0x200 + ((object[2] & 0xF000) >> 12) * 32];

            // Draw a line of the object
            for (int j = 0; j < width; j++)
            {
                // Determine the horizontal pixel offset based on whether or not the sprite is horizontally flipped
                int offset = (object[1] & BIT(12)) ? (x + width - j - 1) : (x + j);
                if (offset < 0 || offset >= 256) continue;

                // Get the palette index for the current pixel
                uint8_t index = core->memory.read<uint8_t>(core->isGbaMode(), tileBase + (j / 8) * 32 + (j % 8) / 2);
                index = (j & 1) ? ((index & 0xF0) >> 4) : (index & 0x0F);

                if (index && type == 2) // Object window
                {
                    // Mark object window pixels with an extra bit, and don't draw anything
                    framebuffer[line * 256 + offset] |= BIT(24);
                }
                else
                {
                    // Draw a pixel if the old one is transparent or lower priority
                    // Semi-transparent pixels are marked with an extra bit
                    if (index && (!(layers[4][offset] & BIT(15)) || prio < objPrio[offset]))
                    {
                        layers[4][offset] = ((type == 1) ? BIT(25) : 0) | BIT(15) | U8TO16(pal, index * 2);
                        objPrio[offset] = prio;
                    }

                    // Update the priority even if the pixel is transparent
                    // This is a quirky behavior of the GBA, but it seems to have been fixed on the DS
                    if (core->isGbaMode() && prio < objPrio[offset])
                        objPrio[offset] = prio;
                }
            }
        }
    }
}

void Gpu2D::writeDispCnt(uint32_t mask, uint32_t value)
{
    // Write to the DISPCNT register
    mask &= ((engine == 0) ? 0xFFFFFFFF : 0xC0B1FFF7);
    dispCnt = (dispCnt & ~mask) | (value & mask);
    if (core->isGbaMode()) dispCnt &= 0xFFFF;
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

    // Reload the internal register
    internalX[bg - 2] = bgX[bg - 2];
}

void Gpu2D::writeBgY(int bg, uint32_t mask, uint32_t value)
{
    // Write to one of the BGY registers
    mask &= 0x0FFFFFFF;
    bgY[bg - 2] = (bgY[bg - 2] & ~mask) | (value & mask);

    // Extend the sign to 32 bits
    if (bgY[bg - 2] & BIT(27)) bgY[bg - 2] |= 0xF0000000; else bgY[bg - 2] &= ~0xF0000000;

    // Reload the internal register
    internalY[bg - 2] = bgY[bg - 2];
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
