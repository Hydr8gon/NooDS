/*
    Copyright 2019-2025 Hydr8gon

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
    ready.store(false);
    running.store(false);
    drawing.store(0);
}

Gpu::~Gpu()
{
    // Clean up the thread
    if (thread)
    {
        running.store(false);
        thread->join();
        delete thread;
    }

    // Clean up any queued framebuffers
    while (!framebuffers.empty())
    {
        Buffers &buffers = framebuffers.front();
        delete[] buffers.framebuffer;
        delete[] buffers.hiRes3D;
        framebuffers.pop();
    }
}

void Gpu::saveState(FILE *file)
{
    // Write state data to the file
    fwrite(dispStat, 2, sizeof(dispStat) / 2, file);
    fwrite(&vCount, sizeof(vCount), 1, file);
    fwrite(&dispCapCnt, sizeof(dispCapCnt), 1, file);
    fwrite(&powCnt1, sizeof(powCnt1), 1, file);
}

void Gpu::loadState(FILE *file)
{
    // Read state data from the file
    fread(dispStat, 2, sizeof(dispStat) / 2, file);
    fread(&vCount, sizeof(vCount), 1, file);
    fread(&dispCapCnt, sizeof(dispCapCnt), 1, file);
    fread(&powCnt1, sizeof(powCnt1), 1, file);
}

uint32_t Gpu::rgb5ToRgb8(uint32_t color)
{
    // Convert an RGB5 value to an RGB8 value, with RGB6 as an intermediate
    uint8_t r = (((color >> 0) & 0x1F) << 1) * 255 / 63;
    uint8_t g = (((color >> 5) & 0x1F) << 1) * 255 / 63;
    uint8_t b = (((color >> 10) & 0x1F) << 1) * 255 / 63;
    return (0xFF << 24) | (b << 16) | (g << 8) | r;
}

uint32_t Gpu::rgb6ToRgb8(uint32_t color)
{
    // Convert an RGB6 value to an RGB8 value
    uint8_t r = ((color >> 0) & 0x3F) * 255 / 63;
    uint8_t g = ((color >> 6) & 0x3F) * 255 / 63;
    uint8_t b = ((color >> 12) & 0x3F) * 255 / 63;
    return (0xFF << 24) | (b << 16) | (g << 8) | r;
}

uint16_t Gpu::rgb6ToRgb5(uint32_t color)
{
    // Convert an RGB6 value to an RGB5 value
    uint8_t r = ((color >> 0) & 0x3F) / 2;
    uint8_t g = ((color >> 6) & 0x3F) / 2;
    uint8_t b = ((color >> 12) & 0x3F) / 2;
    return BIT(15) | (b << 10) | (g << 5) | r;
}

bool Gpu::getFrame(uint32_t *out, bool gbaCrop)
{
    // Check if a new frame is ready
    if (!ready.load())
        return false;

    // Get the next queued buffers
    Buffers &buffers = framebuffers.front();

    if (gbaCrop)
    {
        // Output the frame in RGB8 format, cropped for GBA
        if (Settings::highRes3D || Settings::screenFilter == 1)
        {
            // GBA doesn't have 3D, but draw the screen upscaled for consistency
            for (int y = 0; y < 160; y++)
            {
                uint32_t *line = &out[y * 240 * 4];
                for (int x = 0; x < 240; x++)
                    line[x * 2] = line[x * 2 + 1] = rgb5ToRgb8(buffers.framebuffer[y * 256 + x]);
                memcpy(&line[240 * 2], line, 240 * 8);
            }
        }
        else
        {
            // Draw to a native resolution buffer
            for (int y = 0; y < 160; y++)
                for (int x = 0; x < 240; x++)
                    out[y * 240 + x] = rgb5ToRgb8(buffers.framebuffer[y * 256 + x]);
        }
    }
    else if (core->gbaMode)
    {
        int offset = (powCnt1 & BIT(15)) ? 0 : (256 * 192); // Display swap
        uint32_t base = 0x6800000 + gbaBlock * 0x20000;
        gbaBlock = !gbaBlock;

        // The DS draws the GBA screen by capturing it to alternating VRAM blocks and then displaying that
        // While not used officially, it's possible to copy images into VRAM before entering GBA mode to use as a border
        // Output the GBA frame, centered, with the current VRAM border around it
        if (Settings::highRes3D || Settings::screenFilter == 1)
        {
            // GBA doesn't have 3D, but draw the screen upscaled for consistency
            for (int y = 0; y < 192; y++)
            {
                uint32_t *line = &out[offset * 4 + y * 256 * 4];
                for (int x = 0; x < 256; x++)
                    line[x * 2] = line[x * 2 + 1] = rgb5ToRgb8((x >= 8 && x < 248 && y >= 16 && y < 176) ? buffers.
                        framebuffer[(y - 16) * 256 + x - 8] : core->memory.read<uint16_t>(0, base + (y * 256 + x) * 2));
                memcpy(&line[256 * 2], line, 256 * 8);
            }

            // Clear the secondary display
            memset(&out[(256 * 192 - offset) * 4], 0, 256 * 192 * 4 * sizeof(uint32_t));
        }
        else
        {
            // Draw to a native resolution buffer
            for (int y = 0; y < 192; y++)
                for (int x = 0; x < 256; x++)
                    out[offset + y * 256 + x] = rgb5ToRgb8((x >= 8 && x < 248 && y >= 16 && y < 176) ? buffers.
                        framebuffer[(y - 16) * 256 + x - 8] : core->memory.read<uint16_t>(0, base + (y * 256 + x) * 2));

            // Clear the secondary display
            memset(&out[256 * 192 - offset], 0, 256 * 192 * sizeof(uint32_t));
        }
    }
    else
    {
        // Output the full frame in RGB8 format
        if (Settings::highRes3D || Settings::screenFilter == 1)
        {
            if (buffers.hiRes3D)
            {
                // Draw the screens upscaled, replacing any 3D pixels with high-res output
                for (int y = 0; y < 192 * 2; y++)
                {
                    for (int x = 0; x < 256; x++)
                    {
                        uint32_t value = buffers.framebuffer[y * 256 + x];
                        int i = (y * 2) * (256 * 2) + (x * 2);
                        if (value & BIT(26)) // 3D
                        {
                            uint32_t value2 = buffers.hiRes3D[(i + 0) % (256 * 192 * 4)];
                            out[i + 0] = rgb6ToRgb8((value2 & 0xFC0000) ? value2 : value);
                            value2 = buffers.hiRes3D[(i + 1) % (256 * 192 * 4)];
                            out[i + 1] = rgb6ToRgb8((value2 & 0xFC0000) ? value2 : value);
                            value2 = buffers.hiRes3D[(i + 512) % (256 * 192 * 4)];
                            out[i + 512] = rgb6ToRgb8((value2 & 0xFC0000) ? value2 : value);
                            value2 = buffers.hiRes3D[(i + 513) % (256 * 192 * 4)];
                            out[i + 513] = rgb6ToRgb8((value2 & 0xFC0000) ? value2 : value);
                        }
                        else
                        {
                            uint32_t color = rgb6ToRgb8(value);
                            out[i + 0] = out[i + 1] = color;
                            out[i + 512] = out[i + 513] = color;
                        }
                    }
                }
            }
            else
            {
                // Even when 3D isn't enabled, draw the screens upscaled for consistency
                for (int y = 0; y < 192 * 2; y++)
                {
                    uint32_t *line = &out[y * 256 * 4];
                    for (int x = 0; x < 256; x++)
                        line[x * 2] = line[x * 2 + 1] = rgb6ToRgb8(buffers.framebuffer[y * 256 + x]);
                    memcpy(&line[256 * 2], line, 256 * 8);
                }
            }
        }
        else
        {
            // Draw to a native resolution buffer
            for (int i = 0; i < 256 * 192 * 2; i++)
                out[i] = rgb6ToRgb8(buffers.framebuffer[i]);
        }
    }

    // Free the used buffers
    delete[] buffers.framebuffer;
    delete[] buffers.hiRes3D;

    if (Settings::screenGhost)
    {
        // Get the size of the output framebuffer
        static uint32_t prev[256 * 192 * 8];
        uint32_t width = (gbaCrop ? 240 : 256) << (Settings::highRes3D || Settings::screenFilter == 1);
        uint32_t height = (gbaCrop ? 160 : (192 * 2)) << (Settings::highRes3D || Settings::screenFilter == 1);
        uint32_t size = width * height;

        // Blend output with the previous frame if ghosting is enabled
        for (uint32_t i = 0; i < size; i++)
        {
            uint8_t r = (((prev[i] >> 0) & 0xFF) + ((out[i] >> 0) & 0xFF)) >> 1;
            uint8_t g = (((prev[i] >> 8) & 0xFF) + ((out[i] >> 8) & 0xFF)) >> 1;
            uint8_t b = (((prev[i] >> 16) & 0xFF) + ((out[i] >> 16) & 0xFF)) >> 1;
            prev[i] = out[i];
            out[i] = 0xFF000000 | (b << 16) | (g << 8) | r;
        }
    }

    // Remove the frame from the queue
    mutex.lock();
    framebuffers.pop();
    ready.store(!framebuffers.empty());
    mutex.unlock();
    return true;
}

void Gpu::gbaScanline240()
{
    if (vCount < 160)
    {
        if (thread)
        {
            // Wait for the thread to finish the scanline
            while (drawing.load() != 0)
                std::this_thread::yield();
        }
        else if (frames == 0)
        {
            // Draw the current scanline
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
    core->schedule(GBA_SCANLINE240, 308 * 4);
}

void Gpu::gbaScanline308()
{
    // Clear the H-blank flag
    dispStat[1] &= ~BIT(1);

    // Move to the next scanline
    switch (++vCount)
    {
        case 160: // End of visible scanlines
            // Stop the thread now that the frame has been drawn
            if (thread)
            {
                running.store(false);
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

            // Allow up to 2 framebuffers to be queued, to preserve frame pacing if emulation runs ahead
            if (frames == 0 && framebuffers.size() < 2)
            {
                // Copy the completed sub-framebuffer to a new framebuffer
                Buffers buffers;
                buffers.framebuffer = new uint32_t[256 * 160];
                memcpy(buffers.framebuffer, core->gpu2D[0].getFramebuffer(), 256 * 160 * sizeof(uint32_t));

                // Add the frame to the queue
                mutex.lock();
                framebuffers.push(buffers);
                ready.store(true);
                mutex.unlock();
            }

            // Update the frame count to skip frames when non-zero
            if (frames++ >= Settings::frameskip)
                frames = 0;

            // Stop execution here in case the frontend needs to do things
            core->endFrame();
            break;

        case 227: // Last scanline
            // Clear the V-blank flag
            dispStat[1] &= ~BIT(0);
            break;

        case 228: // End of frame
            // Start the next frame
            vCount = 0;
            core->gpu2D[0].reloadRegisters();

            // Start the 2D thread if enabled
            if (Settings::threaded2D && frames == 0 && !thread)
            {
                running.store(true);
                thread = new std::thread(&Gpu::drawGbaThreaded, this);
            }
            break;
    }

    // Update window flags for the next scanline
    core->gpu2D[0].updateWindows(vCount);

    // Signal that the next scanline should start drawing
    if (vCount < 160 && thread)
        drawing.store(1);

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
    core->schedule(GBA_SCANLINE308, 308 * 4);
}

void Gpu::scanline256()
{
    if (vCount < 192)
    {
        if (thread)
        {
            // Make sure the thread has started before changing the state
            while (drawing.load() == 1)
                std::this_thread::yield();

            switch (drawing.exchange(3))
            {
                case 2:
                    // Draw engine B's scanline if it hasn't started yet (and purposely fall through)
                    core->gpu2D[1].drawScanline(vCount);

                case 3:
                    // Wait for the thread to finish the scanlines
                    while (drawing.load() != 0)
                        std::this_thread::yield();
                    break;
            }
        }
        else if (frames == 0)
        {
            // Draw the current scanlines
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
            static const uint16_t sizes[] = { 128, 128, 256, 64, 256, 128, 256, 192 };
            const uint16_t *size = &sizes[(dispCapCnt >> 19) & 0x6];
            const uint16_t width = size[0];
            const uint16_t height = size[1];

            // Get the VRAM destination address for the current scanline
            uint32_t base = 0x6800000 + ((dispCapCnt & 0x00030000) >> 16) * 0x20000;
            uint32_t writeOffset = ((dispCapCnt & 0x000C0000) >> 3) + vCount * width * 2;

            // Copy the source contents to memory as a 15-bit bitmap
            switch ((dispCapCnt & 0x60000000) >> 29) // Capture source
            {
                case 0: // Source A
                {
                    // Choose from 2D engine A or the 3D engine
                    // In high-res mode, skip every other pixel when capturing 3D
                    uint32_t *source = (dispCapCnt & BIT(24)) ? core->gpu3DRenderer.getLine(vCount) : core->gpu2D[0].getRawLine();
                    bool resShift = (Settings::highRes3D && (dispCapCnt & BIT(24)));

                    // Copy a scanline to memory
                    for (int i = 0; i < width; i++)
                        core->memory.write<uint16_t>(0, base + ((writeOffset + i * 2) & 0x1FFFF), rgb6ToRgb5(source[i << resShift]));
                    break;
                }

                case 1: // Source B
                {
                    if (dispCapCnt & BIT(25))
                    {
                        LOG_CRIT("Unimplemented display capture source: display FIFO\n");
                        break;
                    }

                    // Get the VRAM source address for the current scanline
                    uint32_t readOffset = ((dispCapCnt & 0x0C000000) >> 11) + vCount * width * 2;

                    // Copy a scanline to memory
                    for (int i = 0; i < width; i++)
                    {
                        uint16_t color = core->memory.read<uint16_t>(0, base + ((readOffset + i * 2) & 0x1FFFF));
                        core->memory.write<uint16_t>(0, base + ((writeOffset + i * 2) & 0x1FFFF), color);
                    }
                    break;
                }

                default: // Blended
                {
                    if (dispCapCnt & BIT(25))
                    {
                        LOG_CRIT("Unimplemented display capture source: display FIFO\n");
                        break;
                    }

                    // Choose from 2D engine A or the 3D engine
                    // In high-res mode, skip every other pixel when capturing 3D
                    uint32_t *source = (dispCapCnt & BIT(24)) ? core->gpu3DRenderer.getLine(vCount) : core->gpu2D[0].getRawLine();
                    bool resShift = (Settings::highRes3D && (dispCapCnt & BIT(24)));

                    // Get the VRAM source address for the current scanline
                    uint32_t readOffset = ((dispCapCnt & 0x0C000000) >> 11) + vCount * width * 2;

                    // Get the blending factors for the two sources
                    uint8_t eva = std::min((dispCapCnt >> 0) & 0x1F, 16U);
                    uint8_t evb = std::min((dispCapCnt >> 8) & 0x1F, 16U);

                    // Copy a scanline to memory
                    for (int i = 0; i < width; i++)
                    {
                        // Get colors from the two sources
                        uint16_t c1 = rgb6ToRgb5(source[i << resShift]);
                        uint16_t c2 = core->memory.read<uint16_t>(0, base + ((readOffset + i * 2) & 0x1FFFF));

                        // Blend the color values
                        uint8_t r = std::min((((c1 >> 0) & 0x1F) * eva + ((c2 >> 0) & 0x1F) * evb) / 16, 31);
                        uint8_t g = std::min((((c1 >> 5) & 0x1F) * eva + ((c2 >> 5) & 0x1F) * evb) / 16, 31);
                        uint8_t b = std::min((((c1 >> 10) & 0x1F) * eva + ((c2 >> 10) & 0x1F) * evb) / 16, 31);

                        uint16_t color = BIT(15) | (b << 10) | (g << 5) | r;
                        core->memory.write<uint16_t>(0, base + ((writeOffset + i * 2) & 0x1FFFF), color);
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
    if (frames == 0 && dirty3D && (core->gpu2D[0].readDispCnt() & BIT(3)) && ((vCount + 48) % 263) < 192)
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
    core->schedule(NDS_SCANLINE256, 355 * 6);
}

void Gpu::scanline355()
{
    // Move to the next scanline
    switch (++vCount)
    {
        case 192: // End of visible scanlines
            // Stop the thread now that the frame has been drawn
            if (thread)
            {
                running.store(false);
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

            // Allow up to 2 framebuffers to be queued, to preserve frame pacing if emulation runs ahead
            if (frames == 0 && framebuffers.size() < 2)
            {
                // Copy the completed sub-framebuffers to a new framebuffer
                Buffers buffers;
                buffers.framebuffer = new uint32_t[256 * 192 * 2];
                if (powCnt1 & BIT(0)) // LCDs enabled
                {
                    if (powCnt1 & BIT(15)) // Display swap
                    {
                        memcpy(&buffers.framebuffer[0], core->gpu2D[0].getFramebuffer(), 256 * 192 * sizeof(uint32_t));
                        memcpy(&buffers.framebuffer[256 * 192], core->gpu2D[1].getFramebuffer(), 256 * 192 * sizeof(uint32_t));
                    }
                    else
                    {
                        memcpy(&buffers.framebuffer[0], core->gpu2D[1].getFramebuffer(), 256 * 192 * sizeof(uint32_t));
                        memcpy(&buffers.framebuffer[256 * 192], core->gpu2D[0].getFramebuffer(), 256 * 192 * sizeof(uint32_t));
                    }
                }
                else
                {
                    memset(buffers.framebuffer, 0, 256 * 192 * 2 * sizeof(uint32_t));
                }

                // Copy the upscaled 3D output to a new buffer if enabled
                if (Settings::highRes3D && (core->gpu2D[0].readDispCnt() & BIT(3)))
                {
                    buffers.hiRes3D = new uint32_t[256 * 192 * 4];
                    memcpy(buffers.hiRes3D, core->gpu3DRenderer.getLine(0), 256 * 192 * 4 * sizeof(uint32_t));
                    buffers.top3D = (powCnt1 & BIT(15));
                }

                // Add the frame to the queue
                mutex.lock();
                framebuffers.push(buffers);
                ready.store(true);
                mutex.unlock();
            }

            // Update the frame count to skip frames when non-zero
            if (frames++ >= Settings::frameskip)
                frames = 0;

            // Apply cheats and stop execution in case the frontend needs to do things
            core->actionReplay.applyCheats();
            core->endFrame();
            break;

        case 262: // Last scanline
            // Clear the V-blank flags
            dispStat[0] &= ~BIT(0);
            dispStat[1] &= ~BIT(0);
            break;

        case 263: // End of frame
            // Start the next frame
            vCount = 0;
            core->gpu2D[0].reloadRegisters();
            core->gpu2D[1].reloadRegisters();

            // Start the 2D thread if enabled
            if (Settings::threaded2D && frames == 0 && !thread)
            {
                running.store(true);
                thread = new std::thread(&Gpu::drawThreaded, this);
            }
            break;
    }

    // Update window flags for the next scanline
    core->gpu2D[0].updateWindows(vCount);
    core->gpu2D[1].updateWindows(vCount);

    // Signal that the next scanline should start drawing
    if (vCount < 192 && thread)
        drawing.store(1);

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
    core->schedule(NDS_SCANLINE355, 355 * 6);
}

void Gpu::drawGbaThreaded()
{
    while (running.load())
    {
        // Wait until the next scanline should start
        while (drawing.load() != 1)
        {
            if (!running.load()) return;
            std::this_thread::yield();
        }

        // Draw the current scanline
        core->gpu2D[0].drawGbaScanline(vCount);

        // Signal that the scanline is finished
        drawing.store(0);
    }
}

void Gpu::drawThreaded()
{
    while (running.load())
    {
        // Wait until the next scanline should start
        while (drawing.load() != 1)
        {
            if (!running.load()) return;
            std::this_thread::yield();
        }

        // Draw engine A's scanline
        drawing.store(2);
        core->gpu2D[0].drawScanline(vCount);

        // Draw engine B's scanline if it hasn't started yet
        if (drawing.exchange(3) == 2)
            core->gpu2D[1].drawScanline(vCount);

        // Signal that the scanlines are finished
        drawing.store(0);
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
