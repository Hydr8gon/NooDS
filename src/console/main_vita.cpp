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

#ifdef __VITA__

#include <cstring>
#include <vita2d.h>

#include <psp2/appmgr.h>
#include <psp2/audioout.h>
#include <psp2/ctrl.h>
#include <psp2/power.h>
#include <psp2/touch.h>

#include "console_ui.h"

// Reserve 192MB of allocatable memory
int _newlib_heap_size_user = 192 * 1024 * 1024;

uint32_t audioBuffer[1024];
int audioPort;
bool playing = true;

uint32_t ConsoleUI::defaultKeys[] {
    SCE_CTRL_CIRCLE, SCE_CTRL_CROSS, SCE_CTRL_SELECT, SCE_CTRL_START,
    SCE_CTRL_RIGHT | BIT(17), SCE_CTRL_LEFT | BIT(19), SCE_CTRL_UP | BIT(16), SCE_CTRL_DOWN | BIT(18),
    SCE_CTRL_RTRIGGER, SCE_CTRL_LTRIGGER, SCE_CTRL_TRIANGLE, SCE_CTRL_SQUARE,
    BIT(20) | BIT(21) | BIT(22) | BIT(23)
};

const char *ConsoleUI::keyNames[] {
    "Select", "L3", "R3", "Start", "Up", "Right", "Down", "Left",
    "L2", "R2", "L1", "R1", "Triangle", "Circle", "Cross", "Square",
    "LS Up", "LS Right", "LS Down", "LS Left", "RS Up", "RS Right", "RS Down", "RS Left"
};

void ConsoleUI::startFrame(uint32_t color) {
    // Clear the screen for a new frame
    vita2d_start_drawing();
    vita2d_set_clear_color(color);
    vita2d_clear_screen();
}

void ConsoleUI::endFrame() {
    // Finish and display a frame
    vita2d_end_drawing();
    vita2d_swap_buffers();
}

void *ConsoleUI::createTexture(uint32_t *data, int width, int height) {
    // Create a new texture and copy data to it
    vita2d_texture *texture = vita2d_create_empty_texture(width, height);
    uint32_t stride = vita2d_texture_get_stride(texture) / sizeof(uint32_t);
    uint32_t *texData = (uint32_t*)vita2d_texture_get_datap(texture);
    for (int y = 0; y < height; y++)
        memcpy(&texData[y * stride], &data[y * width], width * sizeof(uint32_t));
    return texture;
}

void ConsoleUI::destroyTexture(void *texture) {
    // Clean up a texture when it's safe to do so
    vita2d_wait_rendering_done();
    vita2d_free_texture((vita2d_texture*)texture);
}

void ConsoleUI::drawTexture(void *texture, float tx, float ty, float tw, float th,
    float x, float y, float w, float h, bool filter, int rotation, uint32_t color) {
    // Set the texture filter
    SceGxmTextureFilter texFilter = filter ? SCE_GXM_TEXTURE_FILTER_LINEAR : SCE_GXM_TEXTURE_FILTER_POINT;
    vita2d_texture_set_filters((vita2d_texture*)texture, texFilter, texFilter);

    // Draw a texture depending on its rotation
    if (rotation == 0)
        vita2d_draw_texture_tint_part_scale((vita2d_texture*)texture, x, y, tx, ty, tw, th, w / tw, h / th, color);
    else
        vita2d_draw_texture_part_tint_scale_rotate((vita2d_texture*)texture, x + w / 2, y + h / 2,
            tx, ty, tw, th, w / th, h / tw, 3.14159f * ((rotation == 1) ? 0.5f : -0.5f), color);
}

uint32_t ConsoleUI::getInputHeld() {
    // Scan for held buttons
    SceCtrlData held;
    sceCtrlPeekBufferPositive(0, &held, 1);

    // Return a mask of mappable keys, including stick movements
    uint32_t value = held.buttons & 0xFFFF;
    value |= (held.ly < 32) << 16; // LS Up
    value |= (held.lx > 224) << 17; // LS Right
    value |= (held.ly > 224) << 18; // LS Down
    value |= (held.lx < 32) << 19; // LS Left
    value |= (held.ry < 32) << 20; // RS Up
    value |= (held.rx > 224) << 21; // RS Right
    value |= (held.ry > 224) << 22; // RS Down
    value |= (held.rx < 32) << 23; // RS Left
    return value;
}

MenuTouch ConsoleUI::getInputTouch() {
    // Scan for touch input and scale it to 720p
    SceTouchData touch;
    sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1);
    return MenuTouch(touch.reportNum > 0, touch.report[0].x * 1280 / 1920, touch.report[0].y * 720 / 1080);
}

void outputAudio() {
    while (playing) {
        // Refill the audio buffer until stopped
        ConsoleUI::fillAudioBuffer(audioBuffer, 1024, 48000);
        sceAudioOutOutput(audioPort, audioBuffer);
    }
}

int main() {
    // Initialize hardware and the UI
    scePowerSetArmClockFrequency(444);
    vita2d_init();
    audioPort = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM, 1024, 48000, SCE_AUDIO_OUT_MODE_STEREO);
    std::thread audioThread(outputAudio);
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
    ConsoleUI::initialize(960, 544, "ux0:", "ux0:/data/noods/");

    // Open the file browser if a ROM can't be loaded from arguments
    char params[1024], *path;
    sceAppMgrGetAppParam(params);
    if (!strstr(params, "psgm:play") || !(path = strstr(params, "&param=")) || ConsoleUI::setPath(path + 7) < 2)
        ConsoleUI::fileBrowser();

    // Run the emulator until it exits
    ConsoleUI::mainLoop();
    playing = false;
    audioThread.join();
    vita2d_fini();
    return 0;
}

#endif // __VITA__
