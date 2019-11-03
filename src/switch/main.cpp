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

const uint32_t keyMap[] = { KEY_A, KEY_B, KEY_MINUS, KEY_PLUS, KEY_RIGHT, KEY_LEFT, KEY_UP, KEY_DOWN, KEY_ZR, KEY_ZL, KEY_X, KEY_Y };

Core *core;
Thread coreThread;
bool running = true;

void runCore(void *args)
{
    while (running)
        core->runFrame();
}

uint32_t bgr5ToRgba8(uint16_t color)
{
    // Convert a BGR5 value to an RGBA8 value
    uint8_t r = ((color >>  0) & 0x1F) * 255 / 31;
    uint8_t g = ((color >>  5) & 0x1F) * 255 / 31;
    uint8_t b = ((color >> 10) & 0x1F) * 255 / 31;
    return (0xFF << 24) | (b << 16) | (g << 8) | r;
}

int main()
{
    // Prepare the core
    try
    {
        // Attempt to boot a ROM
        core = new Core("rom.nds");
    }
    catch (std::exception *e)
    {
        try
        {
            // Attempt to boot the firmware if ROM loading failed
            core = new Core();
        }
        catch (std::exception *e)
        {
            // Exit if the BIOS or firmware files are missing
            return 1;
        }
    }

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
    threadCreate(&coreThread, runCore, NULL, 0x8000, 0x30, 1);
    threadStart(&coreThread);
    appletLockExit();

    while (appletMainLoop())
    {
        // Scan for key input and pass it to the emulator
        hidScanInput();
        uint32_t pressed = hidKeysDown(CONTROLLER_P1_AUTO);
        uint32_t released = hidKeysUp(CONTROLLER_P1_AUTO);
        for (int i = 0; i < 12; i++)
        {
            if (pressed & keyMap[i])
                core->pressKey(i);
            else if (released & keyMap[i])
                core->releaseKey(i);
        }

        // Scan for touch input
        if (hidTouchCount() > 0)
        {
            // If the screen is being touched, determine the position relative to the emulated touch screen
            touchPosition touch;
            hidTouchRead(&touch, 0);
            int touchX = (touch.px - 640) / 2;
            int touchY = (touch.py - 168) / 2;

            // Send the touch coordinates to the core
            core->pressScreen(touchX, touchY);
        }
        else
        {
            // If the screen isn't being touched, release the touch screen press
            core->releaseScreen();
        }

        // Draw the display
        uint32_t stride;
        uint32_t *switchBuf = (uint32_t*)framebufferBegin(&fb, &stride);
        uint16_t *coreBuf = core->getFramebuffer();
        for (int y = 0; y < 192 * 2; y++)
        {
            for (int x = 0; x < 256 * 2; x++)
            {
                switchBuf[(y + 168) * stride / sizeof(uint32_t) + (x + 128)] = bgr5ToRgba8(coreBuf[(y / 2) * 256 + (x / 2)]);
                switchBuf[(y + 168) * stride / sizeof(uint32_t) + (x + 640)] = bgr5ToRgba8(coreBuf[((y / 2) + 192) * 256 + (x / 2)]);
            }
        }
        framebufferEnd(&fb);
    }

    // Clean up
    running = false;
    threadWaitForExit(&coreThread);
    threadClose(&coreThread);
    delete core;
    framebufferClose(&fb);
    clkrstSetClockRate(&cpuSession, 1020000000);
    clkrstExit();
    appletUnlockExit();
    return 0;
}
