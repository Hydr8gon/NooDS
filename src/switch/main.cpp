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
#include <switch.h>
#include <malloc.h>

#include "../core.h"
#include "../settings.h"

const uint32_t keyMap[] = { KEY_A, KEY_B, KEY_MINUS, KEY_PLUS, KEY_RIGHT, KEY_LEFT, KEY_UP, KEY_DOWN, KEY_ZR, KEY_ZL, KEY_X, KEY_Y };

Core *core;
bool running = true;
Thread coreThread, audioThread;

AudioOutBuffer audioBuffers[2];
AudioOutBuffer *audioReleasedBuffer;
int16_t *audioData[2];
uint32_t count;

void runCore(void *args)
{
    // Run the emulator
    while (running)
        core->runFrame();
}

void outputAudio(void *args)
{
    // The NDS sample rate is 32768Hz, but audout uses 48000Hz
    // 1024 samples at 48000Hz is equal to ~700 samples at 32768Hz
    while (running)
    {
        audoutWaitPlayFinish(&audioReleasedBuffer, &count, U64_MAX);

        // Get 700 samples at the original rate
        uint32_t original[700];
        for (int i = 0; i < 700; i++)
            original[i] = core->getSample();

        // Stretch the 700 samples out to 1024 samples in the audio buffer
        int16_t *buffer = (int16_t*)audioReleasedBuffer->buffer;
        for (int i = 0; i < 1024; i++)
        {
            uint32_t sample = original[i * 700 / 1024];
            buffer[i * 2 + 0] = sample >>  0;
            buffer[i * 2 + 1] = sample >> 16;
        }

        audoutAppendAudioOutBuffer(audioReleasedBuffer);
    }
}

uint32_t rgb6ToRgba8(uint32_t color)
{
    // Convert an RGB6 value to an RGBA8 value
    uint8_t r = ((color >>  0) & 0x3F) * 255 / 63;
    uint8_t g = ((color >>  6) & 0x3F) * 255 / 63;
    uint8_t b = ((color >> 12) & 0x3F) * 255 / 63;
    return (0xFF << 24) | (b << 16) | (g << 8) | r;
}

int main()
{
    // Load the settings
    Settings::load(std::vector<Setting>());

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

    audoutInitialize();
    audoutStartAudioOut();

    // Setup the audio buffer
    for (int i = 0; i < 2; i++)
    {
        int size = 1024 * 2 * sizeof(int16_t);
        int alignedSize = (size + 0xFFF) & ~0xFFF;
        audioData[i] = (int16_t*)memalign(0x1000, size);
        memset(audioData[i], 0, alignedSize);
        audioBuffers[i].next = NULL;
        audioBuffers[i].buffer = audioData[i];
        audioBuffers[i].buffer_size = alignedSize;
        audioBuffers[i].data_size = size;
        audioBuffers[i].data_offset = 0;
        audoutAppendAudioOutBuffer(&audioBuffers[i]);
    }

    // Start the audio thread
    threadCreate(&audioThread, outputAudio, NULL, NULL, 0x8000, 0x30, 2);
    threadStart(&audioThread);

    // Start the emulator thread
    threadCreate(&coreThread, runCore, NULL, NULL, 0x8000, 0x30, 1);
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

        uint32_t stride;
        uint32_t *switchBuf = (uint32_t*)framebufferBegin(&fb, &stride);
        uint32_t *coreBuf = core->getFramebuffer();

        // Draw the display
        for (int y = 0; y < 192 * 2; y++)
        {
            for (int x = 0; x < 256 * 2; x++)
            {
                switchBuf[(y + 168) * stride / sizeof(uint32_t) + (x + 128)] = rgb6ToRgba8(coreBuf[(y / 2) * 256 + (x / 2)]);
                switchBuf[(y + 168) * stride / sizeof(uint32_t) + (x + 640)] = rgb6ToRgba8(coreBuf[((y / 2) + 192) * 256 + (x / 2)]);
            }
        }

        framebufferEnd(&fb);
    }

    // Clean up
    running = false;
    audoutStopAudioOut();
    audoutExit();
    framebufferClose(&fb);
    threadWaitForExit(&coreThread);
    threadClose(&coreThread);
    delete core;
    Settings::save();
    clkrstSetClockRate(&cpuSession, 1020000000);
    clkrstExit();
    appletUnlockExit();
    return 0;
}
