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
    uint32_t bgVram;
    uint16_t bgBuffers[4][256 * 192];
    uint16_t framebuffer[256 * 192];
} Engine;

Engine engineA, engineB;

uint16_t dot;
uint16_t displayBuffer[256 * 192 * 2];
std::chrono::steady_clock::time_point timer;

void drawText(Engine *engine, uint8_t bg, uint16_t pixel)
{
    engine->bgBuffers[bg][pixel] = 0xFEDC; // Placeholder
}

void drawAffine(Engine *engine, uint8_t bg, uint16_t pixel)
{
    engine->bgBuffers[bg][pixel] = 0xBAAB; // Placeholder
}

void drawExtended(Engine *engine, uint8_t bg, uint16_t pixel)
{
    if ((engine->bgcnt[bg] & BIT(7)) && (engine->bgcnt[bg] & BIT(2))) // Direct color bitmap
    {
        uint32_t base = ((engine->bgcnt[bg] & 0x1F00) >> 8) * 0x4000;
        uint16_t *color = (uint16_t*)memory::vramMap(engine->bgVram + base + pixel * 2);
        if (color)
            engine->bgBuffers[bg][pixel] = *color;
    }
    else
    {
        engine->bgBuffers[bg][pixel] = 0xCDEF; // Placeholder
    }
}

void drawDot(Engine *engine)
{
    uint16_t pixel = memory::vcount * 256 + dot;
    switch ((*engine->dispcnt & 0x00030000) >> 16) // Display mode
    {
        case 0x0: // Display off
            engine->framebuffer[pixel] = 0xFFFF;
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

            // Draw the pixel from the highest priority background
            for (int i = 0; i < 4; i++)
            {
                for (int j = 0; j < 4; j++)
                {
                    if ((*engine->dispcnt & BIT(8 + j)) && (engine->bgcnt[j] & 0x0003) == i)
                    {
                        engine->framebuffer[pixel] = engine->bgBuffers[j][pixel];
                        if (engine->framebuffer[pixel] & BIT(15))
                            return;
                    }
                }
            }

            engine->framebuffer[pixel] = 0x0000;
            break;

        case 0x2: // VRAM display
            switch ((*engine->dispcnt & 0x000C0000) >> 18) // VRAM block
            {
                case 0x0: engine->framebuffer[pixel] = ((uint16_t*)memory::vramA)[pixel]; break;
                case 0x1: engine->framebuffer[pixel] = ((uint16_t*)memory::vramB)[pixel]; break;
                case 0x2: engine->framebuffer[pixel] = ((uint16_t*)memory::vramC)[pixel]; break;
                default:  engine->framebuffer[pixel] = ((uint16_t*)memory::vramD)[pixel]; break;
            }
            break;

        case 0x3: // Main memory display
            printf("Unsupported display mode: main memory\n");
            *engine->dispcnt &= ~0x00030000;
            break;
    }
}

void runDot()
{
    // Draw a pixel
    if (dot < 256 && memory::vcount < 192)
    {
        drawDot(&engineA);
        drawDot(&engineB);
    }

    dot++;
    if (dot == 256) // Start of H-blank
    {
        memory::dispstat |= BIT(1); // H-blank flag
        if (memory::dispstat & BIT(4)) // H-blank IRQ flag
        {
            interpreter::irq9(1);
            interpreter::irq7(1);
        }
    }
    else if (dot == 355) // End of scanline
    {
        memory::dispstat &= ~BIT(1); // H-blank flag
        memory::vcount++;
        dot = 0;

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
    }

    if (memory::vcount == 192 && dot == 0) // Start of V-Blank
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
    engineA.bgVram  = 0x6000000;

    engineB.dispcnt = &memory::dispcntB;
    engineB.bgcnt   = memory::bgcntB;
    engineB.bgVram  = 0x6200000;
}

}
