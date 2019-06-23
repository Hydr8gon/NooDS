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

typedef struct
{
    uint32_t *dispcnt;
    uint32_t *bgcnt;
    uint16_t *palette;
    uint32_t bgVram;
    uint16_t bgBuffers[4][256 * 192];
    uint16_t framebuffer[256 * 192];
} Engine;

Engine engineA, engineB;
uint16_t displayBuffer[256 * 192 * 2];
std::chrono::steady_clock::time_point timer;

void drawText(Engine *engine, uint8_t bg, uint16_t pixel)
{
    if (bg == 0 && *engine->dispcnt & BIT(3)) // 3D
    {
        memset(&engine->bgBuffers[bg][pixel], 0, 256 * sizeof(uint16_t));
        return;
    }

    uint32_t screenBase = ((engine->bgcnt[bg] & 0x1F00) >> 8) * 0x800  + ((*engine->dispcnt & 0x38000000) >> 27) * 0x10000;
    uint32_t charBase   = ((engine->bgcnt[bg] & 0x003C) >> 2) * 0x4000 + ((*engine->dispcnt & 0x07000000) >> 24) * 0x10000;
    uint16_t *tiles = (uint16_t*)memory::vramMap(engine->bgVram + screenBase + ((memory::vcount / 8) * 32) * sizeof(uint16_t));
    if (tiles)
    {
        if (engine->bgcnt[bg] & BIT(7)) // 8-bit
        {
            for (int i = 0; i < 32; i++)
            {
                uint8_t *sprite = (uint8_t*)memory::vramMap(engine->bgVram + charBase + (tiles[i] & 0x03FF) * 64 + (memory::vcount % 8) * 8);
                if (sprite)
                {
                    for (int j = 0; j < 8; j++)
                        engine->bgBuffers[bg][pixel + j] = engine->palette[sprite[j]] | (sprite[j] ? BIT(15) : 0);
                }
                pixel += 8;
            }
        }
        else // 4-bit
        {
            for (int i = 0; i < 32; i++)
            {
                uint8_t *sprite = (uint8_t*)memory::vramMap(engine->bgVram + charBase + (tiles[i] & 0x03FF) * 32 + (memory::vcount % 8) * 4);
                if (sprite)
                {
                    uint8_t palette = ((tiles[i] & 0xF000) >> 12) * 0x10;
                    for (int j = 0; j < 4; j++)
                    {
                        engine->bgBuffers[bg][pixel + j * 2]     = engine->palette[palette + (sprite[j] & 0x0F)]        | ((sprite[j] & 0x0F) ? BIT(15) : 0);
                        engine->bgBuffers[bg][pixel + j * 2 + 1] = engine->palette[palette + ((sprite[j] & 0xF0) >> 4)] | ((sprite[j] & 0xF0) ? BIT(15) : 0);
                    }
                }
                pixel += 8;
            }
        }
    }
}

void drawAffine(Engine *engine, uint8_t bg, uint16_t pixel)
{
    memset(&engine->bgBuffers[bg][pixel], 0, 256 * sizeof(uint16_t));
}

void drawExtended(Engine *engine, uint8_t bg, uint16_t pixel)
{
    if (engine->bgcnt[bg] & BIT(7))
    {
        uint32_t screenBase = ((engine->bgcnt[bg] & 0x1F00) >> 8) * 0x4000;
        if (engine->bgcnt[bg] & BIT(2)) // Direct color bitmap
        {
            uint16_t *line = (uint16_t*)memory::vramMap(engine->bgVram + screenBase + pixel * sizeof(uint16_t));
            if (line)
                memcpy(&engine->bgBuffers[bg][pixel], line, 256 * sizeof(uint16_t));
        }
        else if (engine->bgcnt[bg] & BIT(7)) // 256 color bitmap
        {
            uint8_t *line = (uint8_t*)memory::vramMap(engine->bgVram + screenBase + pixel);
            if (line)
            {
                for (int i = 0; i < 256; i++)
                    engine->bgBuffers[bg][pixel + i] = engine->palette[line[i]] | ((line[i]) ? BIT(15) : 0);
            }
        }
    }
    else
    {
        memset(&engine->bgBuffers[bg][pixel], 0, 256 * sizeof(uint16_t));
    }
}

void drawScanline(Engine *engine)
{
    uint16_t pixel = memory::vcount * 256;

    switch ((*engine->dispcnt & 0x00030000) >> 16) // Display mode
    {
        case 0x0: // Display off
            memset(&engine->framebuffer[pixel], 0xFF, 256 * sizeof(uint16_t));
            break;

        case 0x1: // Graphics display
            switch (*engine->dispcnt & 0x00000007) // BG mode
            {
                case 0x0:
                    if (*engine->dispcnt & BIT(8))  drawText(engine, 0, pixel);
                    if (*engine->dispcnt & BIT(9))  drawText(engine, 1, pixel);
                    if (*engine->dispcnt & BIT(10)) drawText(engine, 2, pixel);
                    if (*engine->dispcnt & BIT(11)) drawText(engine, 3, pixel);
                    break;

                case 0x1:
                    if (*engine->dispcnt & BIT(8))  drawText  (engine, 0, pixel);
                    if (*engine->dispcnt & BIT(9))  drawText  (engine, 1, pixel);
                    if (*engine->dispcnt & BIT(10)) drawText  (engine, 2, pixel);
                    if (*engine->dispcnt & BIT(11)) drawAffine(engine, 3, pixel);
                    break;

                case 0x2:
                    if (*engine->dispcnt & BIT(8))  drawText  (engine, 0, pixel);
                    if (*engine->dispcnt & BIT(9))  drawText  (engine, 1, pixel);
                    if (*engine->dispcnt & BIT(10)) drawAffine(engine, 2, pixel);
                    if (*engine->dispcnt & BIT(11)) drawAffine(engine, 3, pixel);
                    break;

                case 0x3:
                    if (*engine->dispcnt & BIT(8))  drawText    (engine, 0, pixel);
                    if (*engine->dispcnt & BIT(9))  drawText    (engine, 1, pixel);
                    if (*engine->dispcnt & BIT(10)) drawText    (engine, 2, pixel);
                    if (*engine->dispcnt & BIT(11)) drawExtended(engine, 3, pixel);
                    break;

                case 0x4:
                    if (*engine->dispcnt & BIT(8))  drawText    (engine, 0, pixel);
                    if (*engine->dispcnt & BIT(9))  drawText    (engine, 1, pixel);
                    if (*engine->dispcnt & BIT(10)) drawAffine  (engine, 2, pixel);
                    if (*engine->dispcnt & BIT(11)) drawExtended(engine, 3, pixel);
                    break;

                case 0x5:
                    if (*engine->dispcnt & BIT(8))  drawText    (engine, 0, pixel);
                    if (*engine->dispcnt & BIT(9))  drawText    (engine, 1, pixel);
                    if (*engine->dispcnt & BIT(10)) drawExtended(engine, 2, pixel);
                    if (*engine->dispcnt & BIT(11)) drawExtended(engine, 3, pixel);
                    break;

                default:
                    printf("Unknown BG mode: %d\n", (*engine->dispcnt & 0x00000007));
                    break;
            }

            // Draw the pixels from the highest priority background
            for (int i = 0; i < 256; i++)
            {
                engine->framebuffer[pixel + i] = 0x0000;
                for (int j = 0; j < 4; j++)
                {
                    for (int k = 0; k < 4; k++)
                    {
                        if ((engine->bgcnt[k] & 0x0003) == j && (*engine->dispcnt & BIT(8 + k)) && (engine->bgBuffers[k][pixel + i] & BIT(15)))
                        {
                            engine->framebuffer[pixel + i] = engine->bgBuffers[k][pixel + i];
                            break;
                        }
                    }
                    if (engine->framebuffer[pixel + i])
                        break;
                }
            }
            break;

        case 0x2: // VRAM display
            switch ((*engine->dispcnt & 0x000C0000) >> 18) // VRAM block
            {
                case 0x0: memcpy(&engine->framebuffer[pixel], &memory::vramA[pixel * sizeof(uint16_t)], 256 * sizeof(uint16_t)); break;
                case 0x1: memcpy(&engine->framebuffer[pixel], &memory::vramB[pixel * sizeof(uint16_t)], 256 * sizeof(uint16_t)); break;
                case 0x2: memcpy(&engine->framebuffer[pixel], &memory::vramC[pixel * sizeof(uint16_t)], 256 * sizeof(uint16_t)); break;
                default:  memcpy(&engine->framebuffer[pixel], &memory::vramD[pixel * sizeof(uint16_t)], 256 * sizeof(uint16_t)); break;
            }
            break;

        case 0x3: // Main memory display
            printf("Unsupported display mode: main memory\n");
            *engine->dispcnt &= ~0x00030000;
            break;
    }
}

void scanline256()
{
    // Draw a scanline
    if (memory::vcount < 192)
    {
        drawScanline(&engineA);
        drawScanline(&engineB);
    }

    // Start H-blank
    memory::dispstat |= BIT(1); // H-blank flag
    if (memory::dispstat & BIT(4)) // H-blank IRQ flag
    {
        interpreter::irq9(1);
        interpreter::irq7(1);
    }
}

void scanline355()
{
    // End H-blank
    memory::dispstat &= ~BIT(1); // H-blank flag
    memory::vcount++;

    // Check the V-counter
    if (memory::vcount == (memory::dispstat >> 8) | ((memory::dispstat & BIT(7)) << 1))
    {
        memory::dispstat |= BIT(2); // V-counter flag
        if (memory::dispstat & BIT(5)) // V-counter IRQ flag
        {
            interpreter::irq9(2);
            interpreter::irq7(2);
        }
    }
    else if (memory::dispstat & BIT(2))
    {
        memory::dispstat &= ~BIT(2); // V-counter flag
    }

    if (memory::vcount == 192) // Start of V-Blank
    {
        memory::dispstat |= BIT(0); // V-blank flag
        if (memory::dispstat & BIT(3)) // V-blank IRQ flag
        {
            interpreter::irq9(0);
            interpreter::irq7(0);
        }
    }
    else if (memory::vcount == 263) // End of frame
    {
        memory::dispstat &= ~BIT(0); // V-blank flag
        memory::vcount = 0;

        // Display the frame
        if (memory::powcnt1 & BIT(0))
        {
            if (memory::powcnt1 & BIT(15))
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
}

void init()
{
    engineA.dispcnt = &memory::dispcntA;
    engineA.bgcnt   = memory::bgcntA;
    engineA.palette = (uint16_t*)memory::paletteA;
    engineA.bgVram  = 0x6000000;

    engineB.dispcnt = &memory::dispcntB;
    engineB.bgcnt   = memory::bgcntB;
    engineB.palette = (uint16_t*)memory::paletteB;
    engineB.bgVram  = 0x6200000;
}

}
