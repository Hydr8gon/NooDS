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

#include <switch.h>

#include "../core.h"
#include "../gpu.h"

const u32 keyMap[] = { KEY_A, KEY_B, KEY_MINUS, KEY_PLUS, KEY_RIGHT, KEY_LEFT, KEY_UP, KEY_DOWN, KEY_R, KEY_L, KEY_X, KEY_Y };

u32 bgr5ToRgba8(u16 color)
{
    // Convert a BGR5 value to an RGBA8 value
    uint8_t r = ((color >>  0) & 0x1F) * 255 / 31;
    uint8_t g = ((color >>  5) & 0x1F) * 255 / 31;
    uint8_t b = ((color >> 10) & 0x1F) * 255 / 31;
    return (0xFF << 24) | (b << 16) | (g << 8) | r;
}

void runCore(void *args)
{
    while (true)
        core::runScanline();
}

int main(int argc, char **argv)
{
    // Attempt to load a ROM
    // If this fails, the firmware will still boot
    core::loadRom((char*)"rom.nds");

    // Attempt to initialize the emulator
    // If the BIOS and firmware files are missing, exit
    if (!core::init()) return 0;

    // Overclock the Switch CPU
    ClkrstSession cpuSession;
    clkrstInitialize();
    clkrstOpenSession(&cpuSession, PcvModuleId_CpuBus, 0);
    clkrstSetClockRate(&cpuSession, 1785000000);

    // Create a framebuffer
    NWindow* win = nwindowGetDefault();
    Framebuffer fb;
    framebufferCreate(&fb, win, 1280, 720, PIXEL_FORMAT_RGBA_8888, 2);
    framebufferMakeLinear(&fb);

    // Start the emulator
    Thread core;
    threadCreate(&core, runCore, NULL, 0x8000, 0x30, 1);
    threadStart(&core);

    while (appletMainLoop())
    {
        // Scan for key input and pass it to the emulator
        hidScanInput();
        u32 pressed = hidKeysDown(CONTROLLER_P1_AUTO);
        u32 released = hidKeysUp(CONTROLLER_P1_AUTO);
        for (int i = 0; i < 12; i++)
        {
            if (pressed & keyMap[i])
                core::pressKey(i);
            else if (released & keyMap[i])
                core::releaseKey(i);
        }

        if (hidTouchCount() > 0)
        {
            // If the screen is being touched, determine the position relative to the emulated touch screen
            touchPosition touch;
            hidTouchRead(&touch, 0);
            int16_t touchX = (float)(touch.px - 400) * 256 / 480;
            int16_t touchY = (float)touch.py * 384 / 720 - 192;

            // Send the touch coordinates to the emulator if they're within bounds
            if (touchX >= 0 && touchX < 256 && touchY >= 0 && touchY < 192)
                core::pressScreen(touchX, touchY);
        }
        else
        {
            // If the screen isn't being touched, release the touch screen press
            core::releaseScreen();
        }

        // Draw the display
        u32 stride;
        u32 *framebuf = (u32*)framebufferBegin(&fb, &stride);
        for (u32 y = 0; y < 720; y++)
        {
            int scaledY = (float)y * (384.0f / 720);
            for (u32 x = 0; x < 480; x++)
            {
                int scaledX = (float)x * (256.0f / 480);
                framebuf[y * stride / sizeof(u32) + x + 400] = bgr5ToRgba8(gpu::displayBuffer[scaledY * 256 + scaledX]);
            }
        }
        framebufferEnd(&fb);
    }

    // Clean up
    framebufferClose(&fb);
    clkrstSetClockRate(&cpuSession, 1020000000);
    clkrstExit();
    return 0;
}
