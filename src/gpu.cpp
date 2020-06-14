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

#include "gpu.h"
#include "defines.h"
#include "dma.h"
#include "gpu_2d.h"
#include "gpu_3d.h"
#include "gpu_3d_renderer.h"
#include "interpreter.h"

uint32_t *Gpu::getFrame(bool gbaCrop)
{
    mutex.lock();

    // Return the current frame in RGBA8 format if it's ready
    // If it's not ready (the current frame has already been requested), return nothing
    // Frontends should only draw each frame once to avoid stutter, or at least reuse the old frame to avoid repeated conversion
    if (ready)
    {
        uint32_t *out;

        if (gbaCrop)
        {
            out = new uint32_t[240 * 160];

            // Convert the framebuffer to RGBA8 format (GBA window only)
            int offset = (powCnt1 & BIT(15)) ? 0 : (256 * 192); // Display swap
            for (int y = 0; y < 160; y++)
            {
                for (int x = 0; x < 240; x++)
                {
                    uint32_t color = framebuffer[offset + (y + 16) * 256 + (x + 8)];
                    uint8_t r = ((color >>  0) & 0x3F) * 255 / 63;
                    uint8_t g = ((color >>  6) & 0x3F) * 255 / 63;
                    uint8_t b = ((color >> 12) & 0x3F) * 255 / 63;
                    out[y * 240 + x] = (0xFF << 24) | (b << 16) | (g << 8) | r;
                }
            }
        }
        else
        {
            out = new uint32_t[256 * 192 * 2];

            // Convert the framebuffer to RGBA8 format
            for (int i = 0; i < 256 * 192 * 2; i++)
            {
                uint32_t color = framebuffer[i];
                uint8_t r = ((color >>  0) & 0x3F) * 255 / 63;
                uint8_t g = ((color >>  6) & 0x3F) * 255 / 63;
                uint8_t b = ((color >> 12) & 0x3F) * 255 / 63;
                out[i] = (0xFF << 24) | (b << 16) | (g << 8) | r;
            }
        }

        ready = false;
        mutex.unlock();
        return out;
    }
    else
    {
        mutex.unlock();
        return nullptr;
    }
}

uint16_t Gpu::rgb6ToRgb5(uint32_t color)
{
    // Convert an RGB6 value to an RGB5 value
    uint8_t r = ((color >>  0) & 0x3F) / 2;
    uint8_t g = ((color >>  6) & 0x3F) / 2;
    uint8_t b = ((color >> 12) & 0x3F) / 2;
    return BIT(15) | (b << 10) | (g << 5) | r;
}

void Gpu::gbaScanline240()
{
    if (vCount < 160)
    {
        // Draw visible scanlines
        engineA->drawGbaScanline(vCount);

        // Enable H-blank DMA transfers for visible scanlines
        dmas[1]->setMode(2, true);
    }

    // Set the H-blank flag
    dispStat[1] |= BIT(1);

    // Trigger an H-blank IRQ if enabled
    if (dispStat[1] & BIT(4))
        cpus[1]->sendInterrupt(1);
}

void Gpu::gbaScanline308()
{
    // Check if the current scanline matches the V-counter
    if (vCount == (dispStat[1] >> 8))
    {
        // Set the V-counter flag
        dispStat[1] |= BIT(2);

        // Trigger a V-counter IRQ if enabled
        if (dispStat[1] & BIT(5))
            cpus[1]->sendInterrupt(2);
    }
    else if (dispStat[1] & BIT(2))
    {
        // Clear the V-counter flag on the next line
        dispStat[1] &= ~BIT(2);
    }

    // Clear the H-blank flag
    dispStat[1] &= ~BIT(1);

    // Move to the next scanline
    switch (++vCount)
    {
        case 160: // End of visible scanlines
        {
            // Set the V-blank flag
            dispStat[1] |= BIT(0);

            // Trigger a V-blank IRQ if enabled
            if (dispStat[1] & BIT(3))
                cpus[1]->sendInterrupt(0);

            // Enable V-blank DMA transfers
            dmas[1]->setMode(1, true);

            mutex.lock();

            // Copy the completed sub-framebuffers to the main framebuffer
            if (powCnt1 & BIT(15)) // Display swap
            {
                memcpy(&framebuffer[0],         engineA->getFramebuffer(), 256 * 192 * sizeof(uint32_t));
                memset(&framebuffer[256 * 192], 0,                         256 * 192 * sizeof(uint32_t));
            }
            else
            {
                memset(&framebuffer[0],         0,                         256 * 192 * sizeof(uint32_t));
                memcpy(&framebuffer[256 * 192], engineA->getFramebuffer(), 256 * 192 * sizeof(uint32_t));
            }

            ready = true;
            mutex.unlock();
            break;
        }

        case 227: // Last scanline
        {
            // Clear the V-blank flag
            dispStat[1] &= ~BIT(0);
            break;
        }

        case 228: // End of frame
        {
            // Start the next frame
            vCount = 0;
            break;
        }
    }
}

void Gpu::scanline256()
{
    if (vCount < 192)
    {
        // Draw visible scanlines
        engineA->drawScanline(vCount);
        engineB->drawScanline(vCount);

        // Enable H-blank DMA transfers for visible scanlines (ARM9 only)
        dmas[0]->setMode(2, true);

        // Start a display capture at the beginning of the frame if one was requested
        if (vCount == 0 && (dispCapCnt & BIT(31)))
            displayCapture = true;

        // Perform a display capture
        if (displayCapture)
        {
            // Get the capture destination
            uint8_t *block = memory->getVramBlock((dispCapCnt & 0x00030000) >> 16);
            uint32_t writeOffset = (dispCapCnt & 0x000C0000) >> 3;

            // Determine the capture size
            int width, height;
            switch ((dispCapCnt & 0x00300000) >> 20) // Capture size
            {
                case 0: width = 128; height = 128; break;
                case 1: width = 256; height =  64; break;
                case 2: width = 256; height = 128; break;
                case 3: width = 256; height = 192; break;
            }

            // Copy the source contents to memory as a 15-bit bitmap
            switch ((dispCapCnt & 0x60000000) >> 29) // Capture source
            {
                case 0: // Source A
                {
                    // Choose from 2D engine A or the 3D engine
                    uint32_t *source;
                    if (dispCapCnt & BIT(24))
                        source = &gpu3DRenderer->getFramebuffer()[vCount * 256];
                    else
                        source = &engineA->getFramebuffer()[vCount * 256];

                    // Copy a scanline to memory
                    for (int i = 0; i < width; i++)
                    {
                        uint16_t color = rgb6ToRgb5(source[i]);
                        block[(writeOffset + (vCount * width + i) * 2 + 0) % 0x20000] = color >> 0;
                        block[(writeOffset + (vCount * width + i) * 2 + 1) % 0x20000] = color >> 8;
                    }

                    break;
                }

                case 1: // Source B
                {
                    if (dispCapCnt & BIT(25))
                    {
                        printf("Unimplemented display capture source: display FIFO\n");
                        break;
                    }

                    // Get the VRAM source address
                    uint32_t readOffset = (dispCapCnt & 0x0C000000) >> 11;

                    // Copy a scanline to memory
                    for (int i = 0; i < width; i++)
                    {
                        uint16_t color = U8TO16(block, (readOffset + (vCount * width + i) * 2) % 0x20000);
                        block[(writeOffset + (vCount * width + i) * 2 + 0) % 0x20000] = color >> 0;
                        block[(writeOffset + (vCount * width + i) * 2 + 1) % 0x20000] = color >> 8;
                    }

                    break;
                }

                default: // Blended
                {
                    if (dispCapCnt & BIT(25))
                    {
                        printf("Unimplemented display capture source: display FIFO\n");
                        break;
                    }

                    // Choose from 2D engine A or the 3D engine
                    uint32_t *source;
                    if (dispCapCnt & BIT(24))
                        source = &engineA->getFramebuffer()[vCount * 256];
                    else
                        source = &gpu3DRenderer->getFramebuffer()[vCount * 256];

                    // Get the VRAM source address
                    uint32_t readOffset = (dispCapCnt & 0x0C000000) >> 11;

                    // Copy a scanline to memory
                    for (int i = 0; i < width; i++)
                    {
                        // Get colors from the two sources
                        uint16_t c1 = rgb6ToRgb5(source[i]);
                        uint16_t c2 = U8TO16(block, (readOffset + (vCount * width + i) * 2) % 0x20000);

                        // Get the blending factors for the two sources
                        int eva = (dispCapCnt & 0x0000001F) >> 0; if (eva > 16) eva = 16;
                        int evb = (dispCapCnt & 0x00001F00) >> 8; if (evb > 16) evb = 16;

                        // Blend the color values
                        uint8_t r = (((c1 >>  0) & 0x1F) * eva + ((c2 >>  0) & 0x1F) * evb) / 16;
                        uint8_t g = (((c1 >>  5) & 0x1F) * eva + ((c2 >>  5) & 0x1F) * evb) / 16;
                        uint8_t b = (((c1 >> 10) & 0x1F) * eva + ((c2 >> 10) & 0x1F) * evb) / 16;

                        uint16_t color = BIT(15) | (b << 10) | (g << 5) | r;
                        block[(writeOffset + (vCount * width + i) * 2 + 0) % 0x20000] = color >> 0;
                        block[(writeOffset + (vCount * width + i) * 2 + 1) % 0x20000] = color >> 8;
                    }

                    break;
                }
            }

            // End the display capture
            if (vCount + 1 == height)
            {
                displayCapture = false;
                dispCapCnt &= ~BIT(31);
            }
        }

        // Display captures are performed on the layers, even if the display is set to something else
        // After the display capture, redraw the scanline or apply master brightness if needed
        engineA->finishScanline(vCount);
        engineB->finishScanline(vCount);
    }

    // Draw 3D scanlines 48 lines in advance
    if ((engineA->readDispCnt() & BIT(3)) && ((vCount + 48) % 263) < 192)
        gpu3DRenderer->drawScanline((vCount + 48) % 263);

    for (int i = 0; i < 2; i++)
    {
        // Set the H-blank flag
        dispStat[i] |= BIT(1);

        // Trigger an H-blank IRQ if enabled
        if (dispStat[i] & BIT(4))
            cpus[i]->sendInterrupt(1);
    }
}

void Gpu::scanline355()
{
    for (int i = 0; i < 2; i++)
    {
        // Check if the current scanline matches the V-counter
        if (vCount == ((dispStat[i] >> 8) | ((dispStat[i] & BIT(7)) << 1)))
        {
            // Set the V-counter flag
            dispStat[i] |= BIT(2);

            // Trigger a V-counter IRQ if enabled
            if (dispStat[i] & BIT(5))
                cpus[i]->sendInterrupt(2);
        }
        else if (dispStat[i] & BIT(2))
        {
            // Clear the V-counter flag on the next line
            dispStat[i] &= ~BIT(2);
        }

        // Clear the H-blank flag
        dispStat[i] &= ~BIT(1);
    }

    // Move to the next scanline
    switch (++vCount)
    {
        case 192: // End of visible scanlines
        {
            for (int i = 0; i < 2; i++)
            {
                // Set the V-blank flag
                dispStat[i] |= BIT(0);

                // Trigger a V-blank IRQ if enabled
                if (dispStat[i] & BIT(3))
                    cpus[i]->sendInterrupt(0);

                // Enable V-blank DMA transfers
                dmas[i]->setMode(1, true);
            }

            // Swap the buffers of the 3D engine if needed
            if (gpu3D->shouldSwap())
                gpu3D->swapBuffers();

            mutex.lock();

            // Copy the completed sub-framebuffers to the main framebuffer
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

            ready = true;
            mutex.unlock();
            break;
        }

        case 262: // Last scanline
        {
            // Clear the V-blank flag
            for (int i = 0; i < 2; i++)
                dispStat[i] &= ~BIT(0);
            break;
        }

        case 263: // End of frame
        {
            // Start the next frame
            vCount = 0;
            break;
        }
    }
}

void Gpu::writeDispStat(bool cpu, uint16_t mask, uint16_t value)
{
    // Write to one of the DISPSTAT registers
    mask &= 0xFFB8;
    dispStat[cpu] = (dispStat[cpu] & ~mask) | (value & mask);
}

void Gpu::writeDispCapCnt(uint32_t mask, uint32_t value)
{
    // Write to the DISPCAPCNT register
    mask &= 0xEF3F1F1F;
    dispCapCnt = (dispCapCnt & ~mask) | (value & mask);
}

void Gpu::writePowCnt1(uint16_t mask, uint16_t value)
{
    // Write to the POWCNT1 register
    mask &= 0x820F;
    powCnt1 = (powCnt1 & ~mask) | (value & mask);
}
