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

#include <cstring>

#include "gpu.h"
#include "defines.h"
#include "gpu_2d.h"
#include "gpu_3d_renderer.h"
#include "interpreter.h"

void Gpu::scanline256()
{
    // Draw visible scanlines
    if (vCount < 192)
    {
        engineA->drawScanline(vCount);
        engineB->drawScanline(vCount);
    }

    // Draw 3D scanlines 48 lines in advance
    if (engineA->is3DEnabled() && ((vCount + 48) % 263) < 192)
        gpu3DRenderer->drawScanline((vCount + 48) % 263);

    // Set the H-blank flag
    dispStat9 |= BIT(1);
    dispStat7 |= BIT(1);

    // Trigger an H-blank IRQ if enabled
    if (dispStat9 & BIT(4))
        arm9->sendInterrupt(1);
    if (dispStat7 & BIT(4))
        arm7->sendInterrupt(1);
}

void Gpu::scanline355()
{
    // Check if the current scanline matches the ARM9 V-counter
    if (vCount == ((dispStat9 >> 8) | ((dispStat9 & BIT(7)) << 1)))
    {
        // Set the V-counter flag
        dispStat9 |= BIT(2);

        // Trigger a V-counter IRQ if enabled
        if (dispStat9 & BIT(5))
            arm9->sendInterrupt(2);
    }
    else if (dispStat9 & BIT(2))
    {
        // Clear the V-counter flag
        dispStat9 &= ~BIT(2);
    }

    // Check if the current scanline matches the ARM7 V-counter
    if (vCount == ((dispStat7 >> 8) | ((dispStat7 & BIT(7)) << 1)))
    {
        // Set the V-counter flag
        dispStat7 |= BIT(2);

        // Trigger a V-counter IRQ if enabled
        if (dispStat7 & BIT(5))
            arm7->sendInterrupt(2);
    }
    else if (dispStat7 & BIT(2))
    {
        // Clear the V-counter flag
        dispStat7 &= ~BIT(2);
    }

    if (vCount == 262) // End of frame
    {
        // Copy the sub-framebuffers to the main framebuffer on frame completion
        if (powCnt1 & BIT(0)) // LCDs enabled
        {
            if (powCnt1 & BIT(15)) // Display swap
            {
                memcpy(&framebuffer[0],         engineA->getFramebuffer(), 256 * 192 * sizeof(uint32_t));
                memcpy(&framebuffer[256 * 192], engineB->getFramebuffer(), 256 * 192 * sizeof(uint32_t));
            }
            else
            {
                memcpy(&framebuffer[0],         engineB->getFramebuffer(), 256 * 192 * sizeof(uint32_t));
                memcpy(&framebuffer[256 * 192], engineA->getFramebuffer(), 256 * 192 * sizeof(uint32_t));
            }
        }
        else
        {
            memset(framebuffer, 0, 256 * 192 * 2 * sizeof(uint32_t));
        }

        // Start the next frame
        vCount = 0;

        // Clear the V-blank flag
        dispStat9 &= ~BIT(0);
        dispStat7 &= ~BIT(0);
    }
    else
    {
        // Move to the next scanline
        vCount++;

        if (vCount == 192) // End of visible scanlines
        {
            // Set the V-blank flag
            dispStat9 |= BIT(0);
            dispStat7 |= BIT(0);

            // Trigger a V-blank IRQ if enabled
            if (dispStat9 & BIT(3))
                arm9->sendInterrupt(0);
            if (dispStat7 & BIT(3))
                arm7->sendInterrupt(0);
        }
    }

    // Clear the H-blank flag
    dispStat9 &= ~BIT(1);
    dispStat7 &= ~BIT(1);
}

void Gpu::writeDispStat9(unsigned int byte, uint8_t value)
{
    // Write to the ARM9's DISPSTAT register
    uint16_t mask = 0xFFB8 & (0xFF << (byte * 8));
    dispStat9 = (dispStat9 & ~mask) | ((value << (byte * 8)) & mask);
}

void Gpu::writeDispStat7(unsigned int byte, uint8_t value)
{
    // Write to the ARM7's DISPSTAT register
    uint16_t mask = 0xFFB8 & (0xFF << (byte * 8));
    dispStat7 = (dispStat7 & ~mask) | ((value << (byte * 8)) & mask);
}

void Gpu::writePowCnt1(unsigned int byte, uint8_t value)
{
    // Write to the POWCNT1 register
    uint16_t mask = 0x820F & (0xFF << (byte * 8));
    powCnt1 = (powCnt1 & ~mask) | ((value << (byte * 8)) & mask);
}
