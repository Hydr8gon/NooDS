/*
    Copyright 2019 Hydr8gon

    This file is part of NoiDS.

    NoiDS is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    NoiDS is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with NoiDS. If not, see <https://www.gnu.org/licenses/>.
*/

#include <chrono>
#include <cstdio>
#include <cstring>
#include <unistd.h>

#include "gpu.h"
#include "memory.h"

#define BIT(i) (1 << i)

namespace gpu
{

uint16_t dot;
uint32_t framebuffers[2][256 * 192];
uint32_t displayBuffer[256 * 192 * 2];
std::chrono::steady_clock::time_point timer;

uint32_t bgr5ToRgba8(uint16_t pixel)
{
    uint8_t r = ((pixel >>  0) & 0x1F) * 255 / 31;
    uint8_t g = ((pixel >>  5) & 0x1F) * 255 / 31;
    uint8_t b = ((pixel >> 10) & 0x1F) * 255 / 31;
    return (r << 24) | (g << 16) | (b << 8) | 0xFF;
}

void drawDot()
{
    // Draw a pixel
    if (dot < 256 && memory::vcount < 192)
    {
        if (!(memory::powcnt1 & BIT(0))) // LCDs disabled
        {
            framebuffers[0][memory::vcount * 256 + dot] = 0x000000FF;
            framebuffers[1][memory::vcount * 256 + dot] = 0x000000FF;
        }
        else
        {
            switch ((memory::dispcntA & 0x00030000) >> 16) // Engine A display Mode
            {
                case 0x0: // Display off
                    framebuffers[0][memory::vcount * 256 + dot] = 0xFFFFFFFF;
                    break;

                case 0x1: // Graphics display
                    printf("Unsupported engine A graphics mode: graphics\n");
                    memory::dispcntA &= ~0x00030000;
                    break;

                case 0x2: // VRAM display
                    uint16_t pixel;
                    switch ((memory::dispcntA & 0x000C0000) >> 18) // VRAM block
                    {
                        case 0x0: // A
                            pixel = *(uint16_t*)&memory::vramA[(memory::vcount * 256 + dot) * 2];
                            break;

                        case 0x1: // B
                            pixel = *(uint16_t*)&memory::vramB[(memory::vcount * 256 + dot) * 2];
                            break;

                        case 0x2: // C
                            pixel = *(uint16_t*)&memory::vramC[(memory::vcount * 256 + dot) * 2];
                            break;

                        case 0x3: // D
                            pixel = *(uint16_t*)&memory::vramD[(memory::vcount * 256 + dot) * 2];
                            break;
                    }
                    framebuffers[0][memory::vcount * 256 + dot] = bgr5ToRgba8(pixel);
                    break;

                case 0x3: // Main memory display
                    printf("Unsupported engine A graphics mode: main memory\n");
                    memory::dispcntA &= ~0x00030000;
                    break;
            }

            switch ((memory::dispcntB & 0x00030000) >> 16) // Engine B display Mode
            {
                case 0x0: // Display off
                    framebuffers[1][memory::vcount * 256 + dot] = 0xFFFFFFFF;
                    break;

                case 0x1: // Graphics display
                    printf("Unsupported engine B graphics mode: graphics\n");
                    memory::dispcntB &= ~0x00030000;
                    break;
            }
        }
    }

    dot++;
    if (dot == 256) // Start of H-blank
    {
        memory::dispstat |= BIT(1); // H-blank flag
        if (memory::dispstat & BIT(4)) // H-blank IRQ flag
            printf("Unhandled H-blank IRQ\n");
    }
    else if (dot == 355) // End of scanline
    {
        memory::dispstat &= ~BIT(1); // H-blank flag
        dot = 0;
        memory::vcount++;

        // Check the V-counter
        if (memory::vcount == (memory::dispstat >> 8) | ((memory::dispstat & BIT(7)) << 1))
        {
            memory::dispstat |= BIT(2); // V-counter flag
            if (memory::dispstat & BIT(5)) // V-counter IRQ flag
                printf("Unhandled V-counter IRQ\n");
        }
        else if (memory::dispstat & BIT(2))
        {
            memory::dispstat &= ~BIT(2); // V-counter flag
        }
    }

    if (memory::vcount == 192) // Start of V-Blank
    {
        memory::dispstat |= BIT(0); // V-blank flag
        if (memory::dispstat & BIT(3)) // V-blank IRQ flag
            printf("Unhandled V-blank IRQ\n");
    }
    else if (memory::vcount == 263) // End of frame
    {
        memory::dispstat &= BIT(0); // V-blank flag
        memory::vcount = 0;

        // Display the frame
        if (memory::powcnt1 & BIT(15))
        {
            memcpy(&displayBuffer[0],         &framebuffers[0], sizeof(framebuffers[1]));
            memcpy(&displayBuffer[256 * 192], &framebuffers[1], sizeof(framebuffers[0]));
        }
        else
        {
            memcpy(&displayBuffer[0],         &framebuffers[1], sizeof(framebuffers[1]));
            memcpy(&displayBuffer[256 * 192], &framebuffers[0], sizeof(framebuffers[0]));
        }

        // Limit FPS to 60
        std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - timer;
        if (elapsed.count() < 1.0f / 60)
            usleep((1.0f / 60 - elapsed.count()) * 1000000);
        timer = std::chrono::steady_clock::now();
    }
}

}
