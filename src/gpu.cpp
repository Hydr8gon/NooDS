/*
    Copyright 2019-2021 Hydr8gon

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
#include "core.h"
#include "settings.h"

Gpu::Gpu(Core *core): core(core)
{
    // Mark the thread as not drawing to start
    ready.store(true);
    drawing.store(false);

    // Prepare tasks to be used with the scheduler
    gbaScanline240Task = std::bind(&Gpu::gbaScanline240, this);
    gbaScanline308Task = std::bind(&Gpu::gbaScanline308, this);
    scanline256Task = std::bind(&Gpu::scanline256, this);
    scanline355Task = std::bind(&Gpu::scanline355, this);
}

Gpu::~Gpu()
{
    // Clean up the thread
    if (thread)
    {
        running = false;
        thread->join();
        delete thread;
    }
}

void Gpu::scheduleInit()
{
    // Schedule initial NDS GPU tasks (these will reschedule themselves indefinitely)
    core->schedule(Task(&scanline256Task, 256 * 6));
    core->schedule(Task(&scanline355Task, 355 * 6));
}

void Gpu::gbaScheduleInit()
{
    // Schedule initial GBA GPU tasks (these will reschedule themselves indefinitely)
    core->schedule(Task(&gbaScanline240Task, 240 * 4));
    core->schedule(Task(&gbaScanline308Task, 308 * 4));
}

uint32_t Gpu::rgb6ToRgb8(uint32_t color)
{
    // Convert an RGB6 value to an RGB8 value
    uint8_t r = ((color >>  0) & 0x3F) * 255 / 63;
    uint8_t g = ((color >>  6) & 0x3F) * 255 / 63;
    uint8_t b = ((color >> 12) & 0x3F) * 255 / 63;
    return (0xFF << 24) | (b << 16) | (g << 8) | r;
}

uint16_t Gpu::rgb6ToRgb5(uint32_t color)
{
    // Convert an RGB6 value to an RGB5 value
    uint8_t r = ((color >>  0) & 0x3F) / 2;
    uint8_t g = ((color >>  6) & 0x3F) / 2;
    uint8_t b = ((color >> 12) & 0x3F) / 2;
    return BIT(15) | (b << 10) | (g << 5) | r;
}

bool Gpu::getFrame(uint32_t *out, bool gbaCrop)
{
    // Check if a new frame is ready
    if (!ready.load()) return false;
    mutex.lock();

    // Output the frame in RGB8 format, optionally cropped for GBA
    if (gbaCrop)
    {
        int offset = (powCnt1 & BIT(15)) ? 0 : (256 * 192); // Display swap
        for (int y = 0; y < 160; y++)
            for (int x = 0; x < 240; x++)
                out[y * 240 + x] = rgb6ToRgb8(framebuffer[offset + (y + 16) * 256 + (x + 8)]);
    }
    else
    {
        for (int i = 0; i < 256 * 192 * 2; i++)
            out[i] = rgb6ToRgb8(framebuffer[i]);
    }

    mutex.unlock();
    ready.store(false);
    return true;
}

void Gpu::gbaScanline240()
{
    if (vCount < 160)
    {
        if (thread)
        {
            // Wait for the scanline to finish drawing
            while (drawing.load()) std::this_thread::yield();
        }
        else
        {
            // Draw visible scanlines
            core->gpu2D[0].drawGbaScanline(vCount);
        }

        // Trigger H-blank DMA transfers for visible scanlines
        core->dma[1].trigger(2);
    }

    // Set the H-blank flag
    dispStat[1] |= BIT(1);

    // Trigger an H-blank IRQ if enabled
    if (dispStat[1] & BIT(4))
        core->interpreter[1].sendInterrupt(1);

    // Reschedule the task for the next scanline
    core->schedule(Task(&gbaScanline240Task, 308 * 4));
}

void Gpu::gbaScanline308()
{
    // Clear the H-blank flag
    dispStat[1] &= ~BIT(1);

    // Move to the next scanline
    switch (++vCount)
    {
        case 160: // End of visible scanlines
        {
            // Stop the thread now that the frame has been drawn
            if (thread)
            {
                running = false;
                thread->join();
                delete thread;
                thread = nullptr;
            }

            // Set the V-blank flag
            dispStat[1] |= BIT(0);

            // Trigger a V-blank IRQ if enabled
            if (dispStat[1] & BIT(3))
                core->interpreter[1].sendInterrupt(0);

            // Trigger V-blank DMA transfers
            core->dma[1].trigger(1);

            mutex.lock();

            // Copy the completed sub-framebuffers to the main framebuffer
            if (powCnt1 & BIT(15)) // Display swap
            {
                memcpy(&framebuffer[0],         core->gpu2D[0].getFramebuffer(), 256 * 192 * sizeof(uint32_t));
                memset(&framebuffer[256 * 192], 0,                               256 * 192 * sizeof(uint32_t));
            }
            else
            {
                memset(&framebuffer[0],         0,                               256 * 192 * sizeof(uint32_t));
                memcpy(&framebuffer[256 * 192], core->gpu2D[0].getFramebuffer(), 256 * 192 * sizeof(uint32_t));
            }

            mutex.unlock();
            ready.store(true);

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

            // Start the 2D thread if enabled
            if (Settings::getThreaded2D() && !thread)
            {
                running = true;
                thread = new std::thread(&Gpu::drawGbaThreaded, this);
            }

            break;
        }
    }

    // Signal that the next scanline has started
    if (vCount < 160 && thread)
        drawing.store(true);

    // Check if the current scanline matches the V-counter
    if (vCount == (dispStat[1] >> 8))
    {
        // Set the V-counter flag
        dispStat[1] |= BIT(2);

        // Trigger a V-counter IRQ if enabled
        if (dispStat[1] & BIT(5))
            core->interpreter[1].sendInterrupt(2);
    }
    else if (dispStat[1] & BIT(2))
    {
        // Clear the V-counter flag on the next line
        dispStat[1] &= ~BIT(2);
    }

    // Reschedule the task for the next scanline
    core->schedule(Task(&gbaScanline308Task, 308 * 4));
}

void Gpu::scanline256()
{
    if (vCount < 192)
    {
        if (thread)
        {
            // Wait for the scanlines to finish drawing
            while (drawing.load()) std::this_thread::yield();
        }
        else
        {
            // Draw visible scanlines
            core->gpu2D[0].drawScanline(vCount);
            core->gpu2D[1].drawScanline(vCount);
        }

        // Trigger H-blank DMA transfers for visible scanlines (ARM9 only)
        core->dma[0].trigger(2);

        // Start a display capture at the beginning of the frame if one was requested
        if (vCount == 0 && (dispCapCnt & BIT(31)))
            displayCapture = true;

        // Perform a display capture
        if (displayCapture)
        {
            // Determine the capture size
            int width, height;
            switch ((dispCapCnt & 0x00300000) >> 20) // Capture size
            {
                case 0: width = 128; height = 128; break;
                case 1: width = 256; height =  64; break;
                case 2: width = 256; height = 128; break;
                case 3: width = 256; height = 192; break;
            }

            // Get the VRAM destination address for the current scanline
            uint32_t base = 0x6800000 + ((dispCapCnt & 0x00030000) >> 16) * 0x20000;
            uint32_t writeOffset = ((dispCapCnt & 0x000C0000) >> 3) + vCount * width * 2;

            // Copy the source contents to memory as a 15-bit bitmap
            switch ((dispCapCnt & 0x60000000) >> 29) // Capture source
            {
                case 0: // Source A
                {
                    // Choose from 2D engine A or the 3D engine
                    uint32_t *source = (dispCapCnt & BIT(24)) ? core->gpu3DRenderer.getLine(vCount) : core->gpu2D[0].getRawLine();

                    // Copy a scanline to memory
                    for (int i = 0; i < width; i++)
                        core->memory.write<uint16_t>(0, base + (writeOffset + i * 2) % 0x20000, rgb6ToRgb5(source[i]));

                    break;
                }

                case 1: // Source B
                {
                    if (dispCapCnt & BIT(25))
                    {
                        LOG("Unimplemented display capture source: display FIFO\n");
                        break;
                    }

                    // Get the VRAM source address for the current scanline
                    uint32_t readOffset = ((dispCapCnt & 0x0C000000) >> 11) + vCount * width * 2;

                    // Copy a scanline to memory
                    for (int i = 0; i < width; i++)
                    {
                        uint16_t color = core->memory.read<uint16_t>(0, base + (readOffset + i * 2) % 0x20000);
                        core->memory.write<uint16_t>(0, base + (writeOffset + i * 2) % 0x20000, color);
                    }

                    break;
                }

                default: // Blended
                {
                    if (dispCapCnt & BIT(25))
                    {
                        LOG("Unimplemented display capture source: display FIFO\n");
                        break;
                    }

                    // Choose from 2D engine A or the 3D engine
                    uint32_t *source = (dispCapCnt & BIT(24)) ? core->gpu3DRenderer.getLine(vCount) : core->gpu2D[0].getRawLine();

                    // Get the VRAM source address for the current scanline
                    uint32_t readOffset = ((dispCapCnt & 0x0C000000) >> 11) + vCount * width * 2;

                    // Copy a scanline to memory
                    for (int i = 0; i < width; i++)
                    {
                        // Get colors from the two sources
                        uint16_t c1 = rgb6ToRgb5(source[i]);
                        uint16_t c2 = core->memory.read<uint16_t>(0, base + (readOffset + i * 2) % 0x20000);

                        // Get the blending factors for the two sources
                        int eva = (dispCapCnt & 0x0000001F) >> 0; if (eva > 16) eva = 16;
                        int evb = (dispCapCnt & 0x00001F00) >> 8; if (evb > 16) evb = 16;

                        // Blend the color values
                        uint8_t r = (((c1 >>  0) & 0x1F) * eva + ((c2 >>  0) & 0x1F) * evb) / 16;
                        uint8_t g = (((c1 >>  5) & 0x1F) * eva + ((c2 >>  5) & 0x1F) * evb) / 16;
                        uint8_t b = (((c1 >> 10) & 0x1F) * eva + ((c2 >> 10) & 0x1F) * evb) / 16;

                        uint16_t color = BIT(15) | (b << 10) | (g << 5) | r;
                        core->memory.write<uint16_t>(0, base + (writeOffset + i * 2) % 0x20000, color);
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
    }

    // Draw 3D scanlines 48 lines in advance, if the current 3D is dirty
    // If the 3D parameters haven't changed since the last frame, there's no need to draw it again
    // Bit 0 of the dirty variable represents invalidation, and bit 1 represents a frame currently drawing
    if (dirty3D && (core->gpu2D[0].readDispCnt() & BIT(3)) && ((vCount + 48) % 263) < 192)
    {
        if (vCount == 215) dirty3D = BIT(1);
        core->gpu3DRenderer.drawScanline((vCount + 48) % 263);
        if (vCount == 143) dirty3D &= ~BIT(1);
    }

    for (int i = 0; i < 2; i++)
    {
        // Set the H-blank flag
        dispStat[i] |= BIT(1);

        // Trigger an H-blank IRQ if enabled
        if (dispStat[i] & BIT(4))
            core->interpreter[i].sendInterrupt(1);
    }

    // Reschedule the task for the next scanline
    core->schedule(Task(&scanline256Task, 355 * 6));
}

void Gpu::scanline355()
{
    // Move to the next scanline
    switch (++vCount)
    {
        case 192: // End of visible scanlines
        {
            // Stop the thread now that the frame has been drawn
            if (thread)
            {
                running = false;
                thread->join();
                delete thread;
                thread = nullptr;
            }

            for (int i = 0; i < 2; i++)
            {
                // Set the V-blank flag
                dispStat[i] |= BIT(0);

                // Trigger a V-blank IRQ if enabled
                if (dispStat[i] & BIT(3))
                    core->interpreter[i].sendInterrupt(0);

                // Trigger V-blank DMA transfers
                core->dma[i].trigger(1);
            }

            // Swap the buffers of the 3D engine if needed
            if (core->gpu3D.shouldSwap())
                core->gpu3D.swapBuffers();

            mutex.lock();

            // Copy the completed sub-framebuffers to the main framebuffer
            if (powCnt1 & BIT(0)) // LCDs enabled
            {
                if (powCnt1 & BIT(15)) // Display swap
                {
                    memcpy(&framebuffer[0],         core->gpu2D[0].getFramebuffer(), 256 * 192 * sizeof(uint32_t));
                    memcpy(&framebuffer[256 * 192], core->gpu2D[1].getFramebuffer(), 256 * 192 * sizeof(uint32_t));
                }
                else
                {
                    memcpy(&framebuffer[0],         core->gpu2D[1].getFramebuffer(), 256 * 192 * sizeof(uint32_t));
                    memcpy(&framebuffer[256 * 192], core->gpu2D[0].getFramebuffer(), 256 * 192 * sizeof(uint32_t));
                }
            }
            else
            {
                memset(framebuffer, 0, 256 * 192 * 2 * sizeof(uint32_t));
            }

            mutex.unlock();
            ready.store(true);

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

            // Start the 2D thread if enabled
            if (Settings::getThreaded2D() && !thread)
            {
                running = true;
                thread = new std::thread(&Gpu::drawThreaded, this);
            }

            break;
        }
    }

    // Signal that the next scanline has started
    if (vCount < 192 && thread)
        drawing.store(true);

    for (int i = 0; i < 2; i++)
    {
        // Check if the current scanline matches the V-counter
        if (vCount == ((dispStat[i] >> 8) | ((dispStat[i] & BIT(7)) << 1)))
        {
            // Set the V-counter flag
            dispStat[i] |= BIT(2);

            // Trigger a V-counter IRQ if enabled
            if (dispStat[i] & BIT(5))
                core->interpreter[i].sendInterrupt(2);
        }
        else if (dispStat[i] & BIT(2))
        {
            // Clear the V-counter flag on the next line
            dispStat[i] &= ~BIT(2);
        }

        // Clear the H-blank flag
        dispStat[i] &= ~BIT(1);
    }

    // Reschedule the task for the next scanline
    core->schedule(Task(&scanline355Task, 355 * 6));
}

void Gpu::drawGbaThreaded()
{
    while (running)
    {
        // Wait until the next scanline starts
        while (!drawing.load())
        {
            if (!running) return;
            std::this_thread::yield();
        }

        // Draw the scanline
        core->gpu2D[0].drawGbaScanline(vCount);

        // Signal that the scanline is finished
        drawing.store(false);
    }
}

void Gpu::drawThreaded()
{
    while (running)
    {
        // Wait until the next scanline starts
        while (!drawing.load())
        {
            if (!running) return;
            std::this_thread::yield();
        }

        // Draw the scanlines
        core->gpu2D[0].drawScanline(vCount);
        core->gpu2D[1].drawScanline(vCount);

        // Signal that the scanline is finished
        drawing.store(false);
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
