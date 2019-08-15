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
std::chrono::steady_clock::time_point timer;

void drawText(Engine *engine, uint8_t bg, uint16_t pixel)
{
    // If 3D is enabled, it's rendered to BG0 in text mode
    // But 3D isn't supported yet, so don't render anything
    if (bg == 0 && (*engine->dispcnt & BIT(3)))
    {
        memset(&engine->bgBuffers[bg][pixel], 0, 256 * sizeof(uint16_t));
        return;
    }

    uint32_t screenBase = ((*engine->bgcnt[bg] & 0x1F00) >> 8) * 0x800  + ((*engine->dispcnt & 0x38000000) >> 27) * 0x10000;
    uint32_t charBase   = ((*engine->bgcnt[bg] & 0x003C) >> 2) * 0x4000 + ((*engine->dispcnt & 0x07000000) >> 24) * 0x10000;
    uint16_t yOffset = *interpreter::arm9.vcount + *engine->bgvofs[bg];
    uint16_t *tiles = (uint16_t*)memory::vramMap(engine->bgVramAddr + screenBase + ((yOffset / 8) % 32) * 32 * sizeof(uint16_t));

    if (tiles)
    {
        if ((*engine->bgcnt[bg] & BIT(15)) && yOffset >= 256 && yOffset < 512)
            tiles += (*engine->bgcnt[bg] & BIT(14)) ? 0x800 : 0x400;

        if (*engine->bgcnt[bg] & BIT(7)) // 8-bit tiles
        {
            uint16_t *palette;
            if (*engine->dispcnt & BIT(30)) // Extended palette
                palette = engine->extPalettes[bg + ((bg < 2 && (*engine->bgcnt[bg] & BIT(13))) ? 2 : 0)];
            else // Standard palette
                palette = engine->palette;

            if (palette)
            {
                uint16_t start = pixel;
                for (int i = 0; i <= 32; i++)
                {
                    uint16_t xOffset = *engine->bghofs[bg] + i * 8;
                    uint16_t *tile = &tiles[(xOffset / 8) % 32];
                    if ((*engine->bgcnt[bg] & BIT(14)) && xOffset >= 256 && xOffset < 512)
                        tile += 0x400;

                    uint8_t *sprite;
                    if (*tile & BIT(11)) // Vertical flip
                        sprite = (uint8_t*)memory::vramMap(engine->bgVramAddr + charBase + (*tile & 0x03FF) * 64 + (7 - yOffset % 8) * 8);
                    else
                        sprite = (uint8_t*)memory::vramMap(engine->bgVramAddr + charBase + (*tile & 0x03FF) * 64 + (yOffset % 8) * 8);

                    if (sprite)
                    {
                        if (*tile & BIT(10)) // Horizontal flip
                        {
                            for (int j = 0; j < 8; j++)
                            {
                                int32_t p = pixel - (xOffset % 8) + 7 - j;
                                if (p >= start && p < start + 256)
                                    engine->bgBuffers[bg][p] = (sprite[j] ? (palette[sprite[j]] | BIT(15)) : 0);
                            }
                        }
                        else
                        {
                            for (int j = 0; j < 8; j++)
                            {
                                int32_t p = pixel - (xOffset % 8) + j;
                                if (p >= start && p < start + 256)
                                    engine->bgBuffers[bg][p] = (sprite[j] ? (palette[sprite[j]] | BIT(15)) : 0);
                            }
                        }
                    }
                    pixel += 8;
                }
            }
        }
        else // 4-bit tiles
        {
            uint16_t start = pixel;
            for (int i = 0; i < 32; i++)
            {
                uint16_t xOffset = *engine->bghofs[bg] + i * 8;
                uint16_t *tile = &tiles[(xOffset / 8) % 32];
                if ((*engine->bgcnt[bg] & BIT(14)) && xOffset >= 256 && xOffset < 512)
                    tile += 0x400;

                uint8_t *sprite;
                if (tiles[i] & BIT(11)) // Vertical flip
                    sprite = (uint8_t*)memory::vramMap(engine->bgVramAddr + charBase + (*tile & 0x03FF) * 32 + (7 - yOffset % 8) * 4);
                else
                    sprite = (uint8_t*)memory::vramMap(engine->bgVramAddr + charBase + (*tile & 0x03FF) * 32 + (yOffset % 8) * 4);

                if (sprite)
                {
                    uint16_t *palette = &engine->palette[((tiles[i] & 0xF000) >> 12) * 0x10];
                    if (*tile & BIT(10)) // Horizontal flip
                    {
                        for (int j = 0; j < 4; j++)
                        {
                            int32_t p = pixel - (xOffset % 8) + 7 - j * 2;
                            if (p >= start && p < start + 256)
                                engine->bgBuffers[bg][p] = ((sprite[j] & 0x0F) ? (palette[sprite[j] & 0x0F] | BIT(15)) : 0);
                            p--;
                            if (p >= start && p < start + 256)
                                engine->bgBuffers[bg][p] = ((sprite[j] & 0xF0) ? (palette[(sprite[j] & 0xF0) >> 4] | BIT(15)) : 0);
                        }
                    }
                    else
                    {
                        for (int j = 0; j < 4; j++)
                        {
                            int32_t p = pixel - (xOffset % 8) + j * 2;
                            if (p >= start && p < start + 256)
                                engine->bgBuffers[bg][p] = ((sprite[j] & 0x0F) ? (palette[sprite[j] & 0x0F] | BIT(15)) : 0);
                            p++;
                            if (p >= start && p < start + 256)
                                engine->bgBuffers[bg][p] = ((sprite[j] & 0xF0) ? (palette[(sprite[j] & 0xF0) >> 4] | BIT(15)) : 0);
                        }
                    }
                }
                pixel += 8;
            }
        }
    }
}

void drawAffine(Engine *engine, uint8_t bg, uint16_t pixel)
{
    // Affine backgrounds aren't implemented yet, so just fill them with nothing
    memset(&engine->bgBuffers[bg][pixel], 0, 256 * sizeof(uint16_t));
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
                memcpy(&engine->bgBuffers[bg][pixel], line, 256 * sizeof(uint16_t));
        }
        else if (*engine->bgcnt[bg] & BIT(7)) // 256 color bitmap
        {
            uint8_t *line = (uint8_t*)memory::vramMap(engine->bgVramAddr + screenBase + pixel);
            if (line)
            {
                for (int i = 0; i < 256; i++)
                    engine->bgBuffers[bg][pixel + i] = (line[i] ? (engine->palette[line[i]] | BIT(15)) : 0);
            }
        }
    }
    else
    {
        memset(&engine->bgBuffers[bg][pixel], 0, 256 * sizeof(uint16_t));
    }
}

void drawObjects(Engine *engine, uint16_t pixel)
{
    for (int i = 127; i >= 0; i--)
    {
        uint16_t *object = &engine->oam[i * 4];
        if (!(object[0] & 0x0300)) // Not rotscale, visible
        {
            uint8_t sizeX = 0, sizeY = 0;
            switch ((object[1] & 0xC000) >> 14) // Size
            {
                case 0:
                {
                    switch ((object[0] & 0xC000) >> 14) // Shape
                    {
                        case 0x0: sizeX =  8; sizeY =  8; break; // Square
                        case 0x1: sizeX = 16; sizeY =  8; break; // Horizontal
                        case 0x2: sizeX =  8; sizeY = 16; break; // Vertical
                    }
                    break;
                }

                case 1:
                {
                    switch ((object[0] & 0xC000) >> 14) // Shape
                    {
                        case 0x0: sizeX = 16; sizeY = 16; break; // Square
                        case 0x1: sizeX = 32; sizeY =  8; break; // Horizontal
                        case 0x2: sizeX =  8; sizeY = 32; break; // Vertical
                    }
                    break;
                }

                case 2:
                {
                    switch ((object[0] & 0xC000) >> 14) // Shape
                    {
                        case 0x0: sizeX = 32; sizeY = 32; break; // Square
                        case 0x1: sizeX = 32; sizeY = 16; break; // Horizontal
                        case 0x2: sizeX = 16; sizeY = 32; break; // Vertical
                    }
                    break;
                }

                case 3:
                {
                    switch ((object[0] & 0xC000) >> 14) // Shape
                    {
                        case 0x0: sizeX = 64; sizeY = 64; break; // Square
                        case 0x1: sizeX = 64; sizeY = 32; break; // Horizontal
                        case 0x2: sizeX = 32; sizeY = 64; break; // Vertical
                    }
                    break;
                }
            }

            uint16_t y = (object[0] & 0x00FF);
            if (*interpreter::arm9.vcount >= y && *interpreter::arm9.vcount < y + sizeY)
            {
                uint16_t bound = (32 << ((*engine->dispcnt & 0x00300000) >> 20));
                uint8_t *tile = (uint8_t*)memory::vramMap(engine->objVramAddr + (object[2] & 0x03FF) * bound);
                uint16_t x = (object[1] & 0x01FF);

                if (tile)
                {
                    if (object[0] & BIT(13)) // 8-bit
                    {
                        if (object[1] & BIT(13)) // Vertical flip
                            tile += (7 - ((*interpreter::arm9.vcount - y) % 8) + ((sizeY - 1 - (*interpreter::arm9.vcount - y)) / 8) * sizeX) * 8;
                        else
                            tile += (((*interpreter::arm9.vcount - y) % 8) + ((*interpreter::arm9.vcount - y) / 8) * sizeX) * 8;

                        uint16_t *palette;
                        if (*engine->dispcnt & BIT(31)) // Extended palette
                            palette = engine->extPalettes[4];
                        else // Standard palette
                            palette = &engine->palette[0x100];

                        if (palette)
                        {
                            if (object[1] & BIT(12)) // Horizontal flip
                            {
                                tile += sizeX * 8 - 64;

                                for (int j = 0; j < sizeX; j++)
                                {
                                    if (j != 0 && j % 8 == 0)
                                        tile -= 64;

                                    if (x + j < 256 && tile && tile[7 - j % 8])
                                        engine->framebuffer[pixel + x + j] = palette[tile[7 - j % 8]];
                                }
                            }
                            else
                            {
                                for (int j = 0; j < sizeX; j++)
                                {
                                    if (j != 0 && j % 8 == 0)
                                            tile += 64;

                                    if (x + j < 256 && tile && tile[j % 8])
                                        engine->framebuffer[pixel + x + j] = palette[tile[j % 8]];
                                }
                            }
                        }
                    }
                    else // 4-bit
                    {
                        uint16_t *palette = &engine->palette[0x100 + ((object[2] & 0xF000) >> 12) * 0x10];

                        if (object[1] & BIT(13)) // Vertical flip
                            tile += (7 - ((*interpreter::arm9.vcount - y) % 8) + ((sizeY - 1 - (*interpreter::arm9.vcount - y)) / 8) * sizeX) * 4;
                        else
                            tile += (((*interpreter::arm9.vcount - y) % 8) + ((*interpreter::arm9.vcount - y) / 8) * sizeX) * 4;

                        if (object[1] & BIT(12)) // Horizontal flip
                        {
                            tile += sizeX * 4 - 32;

                            for (int j = 0; j < sizeX; j += 2)
                            {
                                if (j != 0 && j % 8 == 0)
                                    tile -= 32;

                                if (x + j < 256 && tile && (tile[3 - (j / 2) % 4] & 0xF0))
                                    engine->framebuffer[pixel + x + j] = palette[(tile[3 - (j / 2) % 4] & 0xF0) >> 4];
                                if (x + j + 1 < 256 && tile && (tile[3 - (j / 2) % 4] & 0x0F))
                                    engine->framebuffer[pixel + x + j + 1] = palette[(tile[3 - (j / 2) % 4] & 0x0F)];
                            }
                        }
                        else
                        {
                            for (int j = 0; j < sizeX; j += 2)
                            {
                                if (j != 0 && j % 8 == 0)
                                    tile += 32;

                                if (x + j < 256 && tile && (tile[(j / 2) % 4] & 0x0F))
                                    engine->framebuffer[pixel + x + j] = palette[(tile[(j / 2) % 4] & 0x0F)];
                                if (x + j + 1 < 256 && tile && (tile[(j / 2) % 4] & 0xF0))
                                    engine->framebuffer[pixel + x + j + 1] = palette[(tile[(j / 2) % 4] & 0xF0) >> 4];
                            }
                        }
                    }
                }
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
            switch (*engine->dispcnt & 0x00000007) // BG mode
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
                    printf("Unknown BG mode: %d\n", (*engine->dispcnt & 0x00000007));
                    break;
                }
            }

            // Copy the pixels from the highest priority background to the framebuffer
            for (int i = 0; i < 256; i++)
            {
                engine->framebuffer[pixel + i] = (engine->palette[0] & ~BIT(15));
                for (int j = 0; j < 4; j++)
                {
                    for (int k = 0; k < 4; k++)
                    {
                        if ((*engine->bgcnt[k] & 0x0003) == j && (*engine->dispcnt & BIT(8 + k)) && (engine->bgBuffers[k][pixel + i] & BIT(15)))
                        {
                            engine->framebuffer[pixel + i] = engine->bgBuffers[k][pixel + i];
                            break;
                        }
                    }
                    if (engine->framebuffer[pixel + i] & BIT(15))
                        break;
                }
            }

            // Just draw objects on top of everything for now
            drawObjects(engine, pixel);

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

        // Limit FPS to 60
        std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - timer;
        if (elapsed.count() < 1.0f / 60)
        {
#ifdef _WIN32
            Sleep((1.0f / 60 - elapsed.count()) * 1000);
#else
            usleep((1.0f / 60 - elapsed.count()) * 1000000);
#endif
        }
        timer = std::chrono::steady_clock::now();
    }

    // End H-blank
    hBlankEnd(&interpreter::arm9);
    hBlankEnd(&interpreter::arm7);
}

}
