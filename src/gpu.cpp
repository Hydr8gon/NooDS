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

uint16_t dot;
uint16_t framebuffers[2][256 * 192];
uint16_t displayBuffer[256 * 192 * 2];
std::chrono::steady_clock::time_point timer;

void drawDot()
{
    // Draw a pixel
    if (dot < 256 && memory::vcount < 192)
    {
        if (!(memory::powcnt1 & BIT(0))) // LCDs disabled
        {
            framebuffers[0][memory::vcount * 256 + dot] = 0x0000;
            framebuffers[1][memory::vcount * 256 + dot] = 0x0000;
        }
        else
        {
            switch ((memory::dispcntA & 0x00030000) >> 16) // Engine A display Mode
            {
                case 0x0: // Display off
                    framebuffers[0][memory::vcount * 256 + dot] = 0x7FFF;
                    break;

                case 0x1: // Graphics display
                    printf("Unsupported engine A graphics mode: graphics\n");
                    memory::dispcntA &= ~0x00030000;
                    break;

                case 0x2: // VRAM display
                    uint16_t *bank;
                    switch ((memory::dispcntA & 0x000C0000) >> 18) // VRAM block
                    {
                        case 0x0: bank = (uint16_t*)memory::vramA; break;
                        case 0x1: bank = (uint16_t*)memory::vramB; break;
                        case 0x2: bank = (uint16_t*)memory::vramC; break;
                        default:  bank = (uint16_t*)memory::vramD; break;
                    }
                    framebuffers[0][memory::vcount * 256 + dot] = bank[memory::vcount * 256 + dot];
                    break;

                case 0x3: // Main memory display
                    printf("Unsupported engine A graphics mode: main memory\n");
                    memory::dispcntA &= ~0x00030000;
                    break;
            }

            switch ((memory::dispcntB & 0x00030000) >> 16) // Engine B display Mode
            {
                case 0x0: // Display off
                    framebuffers[1][memory::vcount * 256 + dot] = 0x7FFF;
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

}
