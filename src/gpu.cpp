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

#include <chrono>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "gpu.h"
#include "core.h"
#include "memory.h"

namespace gpu
{

Engine engineA, engineB;

uint16_t displayBuffer[256 * 192 * 2];
std::chrono::steady_clock::time_point frameTimer, fpsTimer;
uint16_t fpsCount, fps;

void drawText(Engine *engine, uint8_t bg, uint16_t pixel)
{
    // If 3D is enabled, it's rendered to BG0 in text mode
    // But 3D isn't supported yet, so don't render anything
    if (bg == 0 && (*engine->dispcnt & BIT(3)))
        return;

    // Get the background data offsets
    uint32_t screenBase = ((*engine->bgcnt[bg] & 0x1F00) >> 8) * 0x0800 + ((*engine->dispcnt & 0x38000000) >> 27) * 0x10000;
    uint32_t charBase   = ((*engine->bgcnt[bg] & 0x003C) >> 2) * 0x4000 + ((*engine->dispcnt & 0x07000000) >> 24) * 0x10000;

    // Get the screen data that contains the current line
    uint16_t yOffset = (*interpreter::arm9.vcount + *engine->bgvofs[bg]) % 512;
    uint16_t *screen = (uint16_t*)memory::vramMap(engine->bgVramAddr + screenBase + ((yOffset / 8) % 32) * 64);
    if (!screen) return;

    // If the Y-offset exceeds 256 and the background is 512 pixels tall, move to the next 256x256 section
    // When the background is 256 pixels wide, this means moving one section
    // When the background is 512 pixels wide, this means moving two sections
    if (yOffset >= 256 && *engine->bgcnt[bg] & BIT(15))
        screen += (*engine->bgcnt[bg] & BIT(14)) ? 0x800 : 0x400;

    // Draw a line
    if (*engine->bgcnt[bg] & BIT(7)) // 8-bit
    {
        for (int i = 0; i <= 256; i += 8)
        {
            // Get the data for the current tile
            uint16_t xOffset = (*engine->bghofs[bg] + i) % 512;
            uint16_t *tile = &screen[(xOffset / 8) % 32];

            // If the X-offset exceeds 256 and the background is 512 pixels wide, move to the next 256x256 section
            if (xOffset >= 256 && (*engine->bgcnt[bg] & BIT(14)))
                tile += 0x400;

            // Determine the palette index based on whether or not the tile is vertically flipped
            uint8_t *indices = (uint8_t*)memory::vramMap(engine->bgVramAddr + charBase + (*tile & 0x03FF) * 64);
            if (!indices) continue;
            indices += (*tile & BIT(11)) ? ((7 - yOffset % 8) * 8) : ((yOffset % 8) * 8);

            // Get the palette of the tile
            uint16_t *palette;
            if (*engine->dispcnt & BIT(30)) // Extended palette
            {
                // Determine the extended palette slot
                // Backgrounds 0 and 1 can alternatively use slots 2 and 3
                uint8_t slot = (bg < 2 && (*engine->bgcnt[bg] & BIT(13))) ? (bg + 2) : bg;

                // In extended palette mode, the tile can select from multiple 256-color palettes
                palette = &engine->extPalettes[slot][((*tile & 0xF000) >> 12) * 256];
                if (!engine->extPalettes[slot]) continue;
            }
            else // Standard palette
            {
                palette = engine->palette;
            }

            for (int j = 0; j < 8; j++)
            {
                // Determine the horizontal pixel offset based on whether or not the tile is horizontally flipped
                int32_t offset = (*tile & BIT(10)) ? (pixel + i - (xOffset % 8) + 7 - j) : (pixel + i - (xOffset % 8) + j);

                // Draw a pixel if one exists at the current position
                if (offset >= pixel && offset < pixel + 256 && indices[j])
                    engine->layers[bg][offset] = palette[indices[j]] | BIT(15);
            }
        }
    }
    else // 4-bit
    {
        for (int i = 0; i <= 256; i += 8)
        {
            // Get the data for the current tile
            uint16_t xOffset = (*engine->bghofs[bg] + i) % 512;
            uint16_t *tile = &screen[(xOffset / 8) % 32];

            // If the X-offset exceeds 256 and the background is 512 pixels wide, move to the next 256x256 section
            if ((*engine->bgcnt[bg] & BIT(14)) && xOffset >= 256)
                tile += 0x400;

            // Determine the palette index based on whether or not the tile is vertically flipped
            uint8_t *indices = (uint8_t*)memory::vramMap(engine->bgVramAddr + charBase + (*tile & 0x03FF) * 32);
            if (!indices) continue;
            indices += (*tile & BIT(11)) ? ((7 - yOffset % 8) * 4) : ((yOffset % 8) * 4);

            // Get the palette of the tile
            // In 4-bit mode, the tile can select from multiple 16-color palettes
            uint16_t *palette = &engine->palette[((*tile & 0xF000) >> 12) * 16];

            for (int j = 0; j < 8; j++)
            {
                // Determine the horizontal pixel offset based on whether or not the tile is horizontally flipped
                int32_t offset = (*tile & BIT(10)) ? (pixel + i - (xOffset % 8) + 7 - j) : (pixel + i - (xOffset % 8) + j);

                // Get the appropriate palette index from the tile for the current position
                uint8_t index = (j % 2 == 1) ? ((indices[j / 2] & 0xF0) >> 4) : (indices[j / 2] & 0x0F);

                // Draw a pixel if one exists at the current position
                if (offset >= pixel && offset < pixel + 256 && index)
                    engine->layers[bg][offset] = palette[index] | BIT(15);
            }
        }
    }
}

void drawAffine(Engine *engine, uint8_t bg, uint16_t pixel)
{
    // Affine backgrounds aren't implemented yet
}

void drawExtended(Engine *engine, uint8_t bg, uint16_t pixel)
{
    if (*engine->bgcnt[bg] & BIT(7))
    {
        uint32_t screenBase = ((*engine->bgcnt[bg] & 0x1F00) >> 8) * 0x4000;
        if (*engine->bgcnt[bg] & BIT(2)) // Direct color bitmap
        {
            uint16_t *line = (uint16_t*)memory::vramMap(engine->bgVramAddr + screenBase + pixel * sizeof(uint16_t));
            if (line)
                memcpy(&engine->layers[bg][pixel], line, 256 * sizeof(uint16_t));
        }
        else if (*engine->bgcnt[bg] & BIT(7)) // 256 color bitmap
        {
            uint8_t *line = (uint8_t*)memory::vramMap(engine->bgVramAddr + screenBase + pixel);
            if (line)
            {
                for (int i = 0; i < 256; i++)
                    engine->layers[bg][pixel + i] = (line[i] ? (engine->palette[line[i]] | BIT(15)) : 0);
            }
        }
    }
}

void drawObjects(Engine *engine, uint16_t pixel)
{
    // Loop through the 128 sprites in OAM, in order of priority from high to low
    for (int i = 127; i >= 0; i--)
    {
        // Get the current object
        // Each object occupies 4 bytes
        uint16_t *object = &engine->oam[i * 4];

        // Skip sprites that are disabled
        if (!(object[0] & BIT(8)) && (object[0] & BIT(9)))
            continue;

        // Determine the dimensions of the object
        uint8_t width = 0, height = 0;
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
        uint8_t width2 = width;
        uint8_t height2 = height;
        if ((object[0] & BIT(8)) && (object[0] & BIT(9)))
        {
            width2 *= 2;
            height2 *= 2;
        }

        // Get the Y coordinate and wrap it around if it exceeds the screen bounds
        int16_t y = (object[0] & 0x00FF);
        if (y >= 192) y -= 256;

        // Don't draw anything if the current scanline lies outside of the object's bounds
        int16_t spriteY = *interpreter::arm9.vcount - y;
        if (spriteY < 0 || spriteY >= height2)
            continue;

        // Get the current tile
        // For 1D tile mapping, the boundary between tiles can be 32, 64, 128, or 256 bytes
        uint16_t bound = (*engine->dispcnt & BIT(4)) ? (32 << ((*engine->dispcnt & 0x00300000) >> 20)) : 32;
        uint8_t *tile = (uint8_t*)memory::vramMap(engine->objVramAddr + (object[2] & 0x03FF) * bound);
        if (!tile) continue;

        // Get the X coordinate and wrap it around if it exceeds the screen bounds
        int16_t x = (object[1] & 0x01FF);
        if (x >= 256) x -= 512;

        // Determine the layer to draw to based on the priority of the object
        uint16_t *layer = engine->layers[4 + ((object[2] & 0x0C00) >> 10)];

        // Draw the object
        if (object[0] & BIT(8)) // Rotscale
        {
            // Get the rotscale parameters and convert them to floats
            float params[4];
            for (int j = 0; j < 4; j++)
            {
                uint16_t param = engine->oam[((object[1] & 0x3E00) >> 9) * 0x10 + j * 4 + 3];
                params[j] = (float)(param & 0x00FF) / 0x100; // Fractional
                params[j] += (param & 0x7F00) >> 8; // Integer
                if (param & BIT(15)) params[j] = -0x80 + params[j]; // Sign
            }

            if (object[0] & BIT(13)) // 8-bit
            {
                uint16_t mapWidth = (*engine->dispcnt & BIT(4)) ? width : 128;

                // Get the palette of the object
                uint16_t *palette;
                if (*engine->dispcnt & BIT(31)) // Extended palette
                {
                    // In extended palette mode, the object can select from multiple 256-color palettes
                    palette = &engine->extPalettes[4][((object[2] & 0xF000) >> 12) * 256];
                    if (!engine->extPalettes[4]) continue;
                }
                else // Standard palette
                {
                    palette = &engine->palette[0x100];
                }

                for (int j = 0; j < width2; j++)
                {
                    // Get the rotscaled X coordinate relative to the sprite
                    int16_t rotscaleX = (j - width2 / 2) * params[0] + (spriteY - height2 / 2) * params[1] + width / 2;
                    if (rotscaleX < 0 || rotscaleX >= width) continue;

                    // Get the rotscaled Y coordinate relative to the sprite
                    int16_t rotscaleY = (j - width2 / 2) * params[2] + (spriteY - height2 / 2) * params[3] + height / 2;
                    if (rotscaleY < 0 || rotscaleY >= height) continue;

                    // Get the appropriate palette index from the tile for the current position
                    uint8_t index = tile[((rotscaleY / 8) * mapWidth + rotscaleY % 8) * 8 + (rotscaleX / 8) * 64 + rotscaleX % 8];

                    // Draw a pixel if one exists at the current position
                    if (x + j < 256 && index)
                        layer[pixel + x + j] = palette[index] | BIT(15);
                }
            }
            else // 4-bit
            {
                uint16_t mapWidth = (*engine->dispcnt & BIT(4)) ? width : 256;

                // Get the palette of the object
                // In 4-bit mode, the object can select from multiple 16-color palettes
                uint16_t *palette = &engine->palette[0x100 + ((object[2] & 0xF000) >> 12) * 16];

                for (int j = 0; j < width2; j++)
                {
                    // Get the rotscaled X coordinate relative to the sprite
                    int16_t rotscaleX = (j - width2 / 2) * params[0] + (spriteY - height2 / 2) * params[1] + width / 2;
                    if (rotscaleX < 0 || rotscaleX >= width) continue;

                    // Get the rotscaled Y coordinate relative to the sprite
                    int16_t rotscaleY = (j - width2 / 2) * params[2] + (spriteY - height2 / 2) * params[3] + height / 2;
                    if (rotscaleY < 0 || rotscaleY >= height) continue;

                    // Get the appropriate palette index from the tile for the current position
                    uint8_t index = tile[((rotscaleY / 8) * mapWidth + rotscaleY % 8) * 4 + (rotscaleX / 8) * 32 + (rotscaleX / 2) % 4];
                    index = (rotscaleX % 2 == 1) ? ((index & 0xF0) >> 4) : (index & 0x0F);

                    // Draw a pixel if one exists at the current position
                    if (x + j < 256 && index)
                        layer[pixel + x + j] = palette[index] | BIT(15);
                }
            }
        }
        else if (object[0] & BIT(13)) // 8-bit
        {
            // Adjust the current tile to align with the current Y coordinate relative to the object
            uint16_t mapWidth = (*engine->dispcnt & BIT(4)) ? width : 128;
            if (object[1] & BIT(13)) // Vertical flip
                tile += (7 - (spriteY % 8) + ((height - 1 - spriteY) / 8) * mapWidth) * 8;
            else
                tile += ((spriteY % 8) + (spriteY / 8) * mapWidth) * 8;

            // Get the palette of the object
            uint16_t *palette;
            if (*engine->dispcnt & BIT(31)) // Extended palette
            {
                // In extended palette mode, the object can select from multiple 256-color palettes
                palette = &engine->extPalettes[4][((object[2] & 0xF000) >> 12) * 256];
                if (!engine->extPalettes[4]) continue;
            }
            else // Standard palette
            {
                palette = &engine->palette[0x100];
            }

            for (int j = 0; j < width; j++)
            {
                // Determine the horizontal pixel offset based on whether or not the sprite is horizontally flipped
                uint16_t offset = (object[1] & BIT(12)) ? (x + width - j - 1) : (x + j);

                // Get the appropriate palette index from the tile for the current position
                uint8_t index = tile[(j / 8) * 64 + j % 8];

                // Draw a pixel if one exists at the current position
                if (offset >= 0 && offset < 256 && index)
                    layer[pixel + offset] = palette[index] | BIT(15);
            }
        }
        else // 4-bit
        {
            // Adjust the current tile to align with the current Y coordinate relative to the object
            uint16_t mapWidth = (*engine->dispcnt & BIT(4)) ? width : 256;
            if (object[1] & BIT(13)) // Vertical flip
                tile += (7 - (spriteY % 8) + ((height - 1 - spriteY) / 8) * mapWidth) * 4;
            else
                tile += ((spriteY % 8) + (spriteY / 8) * mapWidth) * 4;

            // Get the palette of the object
            // In 4-bit mode, the object can select from multiple 16-color palettes
            uint16_t *palette = &engine->palette[0x100 + ((object[2] & 0xF000) >> 12) * 16];

            for (int j = 0; j < width; j++)
            {
                // Determine the horizontal pixel offset based on whether or not the sprite is horizontally flipped
                uint16_t offset = (object[1] & BIT(12)) ? (x + width - j - 1) : (x + j);

                // Get the appropriate palette index from the tile for the current position
                uint8_t index = tile[(j / 8) * 32 + (j / 2) % 4];
                index = (j % 2 == 1) ? ((index & 0xF0) >> 4) : (index & 0x0F);

                // Draw a pixel if one exists at the current position
                if (offset >= 0 && offset < 256 && index)
                    layer[pixel + offset] = palette[index] | BIT(15);
            }
        }
    }
}

void drawScanline(Engine *engine)
{
    uint16_t pixel = *interpreter::arm9.vcount * 256;

    switch ((*engine->dispcnt & 0x00030000) >> 16) // Display mode
    {
        case 0: // Display off
        {
            // Fill the display with white
            memset(&engine->framebuffer[pixel], 0xFF, 256 * sizeof(uint16_t));
            break;
        }

        case 1: // Graphics display
        {
            // Draw the backgrounds
            // The type of each background is determined by the mode
            uint8_t mode = (*engine->dispcnt & 0x00000007);
            switch (mode)
            {
                case 0:
                {
                    if (*engine->dispcnt & BIT(8))  drawText(engine, 0, pixel);
                    if (*engine->dispcnt & BIT(9))  drawText(engine, 1, pixel);
                    if (*engine->dispcnt & BIT(10)) drawText(engine, 2, pixel);
                    if (*engine->dispcnt & BIT(11)) drawText(engine, 3, pixel);
                    break;
                }

                case 1:
                {
                    if (*engine->dispcnt & BIT(8))  drawText  (engine, 0, pixel);
                    if (*engine->dispcnt & BIT(9))  drawText  (engine, 1, pixel);
                    if (*engine->dispcnt & BIT(10)) drawText  (engine, 2, pixel);
                    if (*engine->dispcnt & BIT(11)) drawAffine(engine, 3, pixel);
                    break;
                }

                case 2:
                {
                    if (*engine->dispcnt & BIT(8))  drawText  (engine, 0, pixel);
                    if (*engine->dispcnt & BIT(9))  drawText  (engine, 1, pixel);
                    if (*engine->dispcnt & BIT(10)) drawAffine(engine, 2, pixel);
                    if (*engine->dispcnt & BIT(11)) drawAffine(engine, 3, pixel);
                    break;
                }

                case 3:
                {
                    if (*engine->dispcnt & BIT(8))  drawText    (engine, 0, pixel);
                    if (*engine->dispcnt & BIT(9))  drawText    (engine, 1, pixel);
                    if (*engine->dispcnt & BIT(10)) drawText    (engine, 2, pixel);
                    if (*engine->dispcnt & BIT(11)) drawExtended(engine, 3, pixel);
                    break;
                }

                case 4:
                {
                    if (*engine->dispcnt & BIT(8))  drawText    (engine, 0, pixel);
                    if (*engine->dispcnt & BIT(9))  drawText    (engine, 1, pixel);
                    if (*engine->dispcnt & BIT(10)) drawAffine  (engine, 2, pixel);
                    if (*engine->dispcnt & BIT(11)) drawExtended(engine, 3, pixel);
                    break;
                }

                case 5:
                {
                    if (*engine->dispcnt & BIT(8))  drawText    (engine, 0, pixel);
                    if (*engine->dispcnt & BIT(9))  drawText    (engine, 1, pixel);
                    if (*engine->dispcnt & BIT(10)) drawExtended(engine, 2, pixel);
                    if (*engine->dispcnt & BIT(11)) drawExtended(engine, 3, pixel);
                    break;
                }

                default:
                {
                    printf("Unknown BG mode: %d\n", mode);
                    break;
                }
            }

            // Draw the objects
            drawObjects(engine, pixel);

            // Copy the pixels from the highest priority layer to the framebuffer
            for (int i = 0; i < 256; i++)
            {
                // Set the background color to the first entry in the palette
                engine->framebuffer[pixel + i] = (engine->palette[0] & ~BIT(15));

                for (int j = 0; j < 4; j++)
                {
                    // Check for visible pixels in the object layers
                    if (engine->layers[4 + j][pixel + i] & BIT(15))
                    {
                        engine->framebuffer[pixel + i] = engine->layers[4 + j][pixel + i];
                        break;
                    }

                    // Check for visible pixels in the background layers
                    for (int k = 0; k < 4; k++)
                    {
                        if ((*engine->bgcnt[k] & 0x0003) == j && (*engine->dispcnt & BIT(8 + k)) && (engine->layers[k][pixel + i] & BIT(15)))
                        {
                            engine->framebuffer[pixel + i] = engine->layers[k][pixel + i];
                            break;
                        }
                    }
                    if (engine->framebuffer[pixel + i] & BIT(15))
                        break;
                }
            }

            break;
        }

        case 2: // VRAM display
        {
            // Draw raw bitmap data from a VRAM block
            switch ((*engine->dispcnt & 0x000C0000) >> 18) // VRAM block
            {
                case 0:  memcpy(&engine->framebuffer[pixel], &memory::vramA[pixel * sizeof(uint16_t)], 256 * sizeof(uint16_t)); break;
                case 1:  memcpy(&engine->framebuffer[pixel], &memory::vramB[pixel * sizeof(uint16_t)], 256 * sizeof(uint16_t)); break;
                case 2:  memcpy(&engine->framebuffer[pixel], &memory::vramC[pixel * sizeof(uint16_t)], 256 * sizeof(uint16_t)); break;
                default: memcpy(&engine->framebuffer[pixel], &memory::vramD[pixel * sizeof(uint16_t)], 256 * sizeof(uint16_t)); break;
            }
            break;
        }

        case 3: // Main memory display
        {
            printf("Unsupported display mode: main memory\n");
            *engine->dispcnt &= ~0x00030000;
            break;
        }
    }
}

void hBlankStart(interpreter::Cpu *cpu)
{
    // Set the H-blank bit
    *cpu->dispstat |= BIT(1);

    // Trigger an H-blank IRQ if enabled
    if (*cpu->dispstat & BIT(4))
        *cpu->irf |= BIT(1);
}

void hBlankEnd(interpreter::Cpu *cpu)
{
    // Clear the H-blank bit and move to the next scanline
    *cpu->dispstat &= ~BIT(1);
    (*cpu->vcount)++;

    // Check the V-counter
    if (*cpu->vcount == ((*cpu->dispstat >> 8) | ((*cpu->dispstat & BIT(7)) << 1)))
    {
        *cpu->dispstat |= BIT(2); // V-counter flag
        if (*cpu->dispstat & BIT(5)) // V-counter IRQ flag
            *cpu->irf |= BIT(2);
    }
    else if (*cpu->dispstat & BIT(2))
    {
        *cpu->dispstat &= ~BIT(2); // V-counter flag
    }

    if (*cpu->vcount == 192) // Start of V-Blank
    {
        *cpu->dispstat |= BIT(0); // V-blank flag
        if (*cpu->dispstat & BIT(3)) // V-blank IRQ flag
            *cpu->irf |= BIT(0);
    }
    else if (*cpu->vcount == 263) // End of frame
    {
        *cpu->dispstat &= ~BIT(0); // V-blank flag
        *cpu->vcount = 0;
    }
}

void scanline256()
{
    // Draw a scanline
    if (*interpreter::arm9.vcount < 192)
    {
        drawScanline(&engineA);
        drawScanline(&engineB);
    }

    // Start H-blank
    hBlankStart(&interpreter::arm9);
    hBlankStart(&interpreter::arm7);
}

void scanline355()
{
    if (*interpreter::arm9.vcount == 262) // End of frame
    {
        // Display the frame
        if (*memory::powcnt1 & BIT(0))
        {
            if (*memory::powcnt1 & BIT(15))
            {
                memcpy(&displayBuffer[0],         engineA.framebuffer, sizeof(engineA.framebuffer));
                memcpy(&displayBuffer[256 * 192], engineB.framebuffer, sizeof(engineB.framebuffer));
            }
            else
            {
                memcpy(&displayBuffer[0],         engineB.framebuffer, sizeof(engineB.framebuffer));
                memcpy(&displayBuffer[256 * 192], engineA.framebuffer, sizeof(engineA.framebuffer));
            }
        }
        else
        {
            memset(displayBuffer, 0, sizeof(displayBuffer));
        }

        // Clear the layers in preparation for the next frame
        for (int i = 0; i < 8; i++)
        {
            memset(engineA.layers[i], 0, sizeof(engineA.layers[i]));
            memset(engineB.layers[i], 0, sizeof(engineB.layers[i]));
        }

        // Limit FPS to 60
        std::chrono::duration<double> frameTime = std::chrono::steady_clock::now() - frameTimer;
        if (frameTime.count() < 1.0f / 60)
        {
#ifdef _WIN32
            // Windows has to be special and have their own sleep function
            Sleep((1.0f / 60 - frameTime.count()) * 1000);
#else
            usleep((1.0f / 60 - frameTime.count()) * 1000000);
#endif
        }
        frameTimer = std::chrono::steady_clock::now();

        // Count FPS
        fpsCount++;
        std::chrono::duration<double> fpsTime = std::chrono::steady_clock::now() - fpsTimer;
        if (fpsTime.count() >= 1.0f)
        {
            fps = fpsCount;
            fpsCount = 0;
            fpsTimer = std::chrono::steady_clock::now();
        }
    }

    // End H-blank
    hBlankEnd(&interpreter::arm9);
    hBlankEnd(&interpreter::arm7);
}

}
