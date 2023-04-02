/*
    Copyright 2019-2022 Hydr8gon

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

#include <coreinit/memdefaultheap.h>
#include <gx2/draw.h>
#include <gx2/mem.h>
#include <gx2r/draw.h>
#include <vpad/input.h>
#include <whb/proc.h>
#include <whb/sdcard.h>
#include <whb/gfx.h>
#include <SDL2/SDL.h>

#include "../core.h"
#include "../settings.h"
#include "../common/screen_layout.h"

// Shader taken from https://github.com/rw-r-r-0644/gx2-texture
#include "texture_shader_gsh.h"

const uint32_t vpadMap[] =
{
     VPAD_BUTTON_A,     VPAD_BUTTON_B,    VPAD_BUTTON_MINUS, VPAD_BUTTON_PLUS,
     VPAD_BUTTON_RIGHT, VPAD_BUTTON_LEFT, VPAD_BUTTON_UP,    VPAD_BUTTON_DOWN,
     VPAD_BUTTON_ZR,    VPAD_BUTTON_ZL,   VPAD_BUTTON_X,     VPAD_BUTTON_Y
};

Core *core;
bool running = false;
std::thread *coreThread;

ScreenLayout layout;
uint32_t framebuffer[256 * 192 * 8];
bool gbaMode = false;

WHBGfxShaderGroup group;
GX2RBuffer posBuffer, texBuffer;
GX2Texture textures[2];
GX2Sampler sampler;
uint32_t bufOffset = 0;

static void runCore()
{
    // Run the emulator
    while (running)
        core->runFrame();
}

static void audioCallback(void *data, uint8_t *buffer, int length)
{
    // Copy samples directly to the audio buffer
    uint32_t *samples = core->spu.getSamples(length / 4);
    memcpy(buffer, samples, length);
    delete[] samples;
}

void drawTexture(int tex, int x, int y, int w, int h)
{
    // Calculate the position coordinates based on gamepad resolution
    float x1 =  ((float)(x + 0) / (854 / 2) - 1.0f);
    float x2 =  ((float)(x + w) / (854 / 2) - 1.0f);
    float y1 = -((float)(y + 0) / (480 / 2) - 1.0f);
    float y2 = -((float)(y + h) / (480 / 2) - 1.0f);
    float posCoords[] = { x1, y1, x2, y1, x2, y2, x1, y2 };

    // Upload the position coodinates to the buffer
    uint8_t *buffer = (uint8_t*)GX2RLockBufferEx(&posBuffer, GX2R_RESOURCE_BIND_NONE);
    memcpy(&buffer[bufOffset], posCoords, posBuffer.elemSize * 4);
    GX2RUnlockBufferEx(&posBuffer, GX2R_RESOURCE_BIND_NONE);
    GX2RSetAttributeBuffer(&posBuffer, 0, posBuffer.elemSize, bufOffset);

    // Define the texture coordinates based on rotation
    uint32_t texOffset = ScreenLayout::getScreenRotation() * 8;
    static const float texCoords[] =
    {
        0, 0, 1, 0, 1, 1, 0, 1, // None
        0, 1, 0, 0, 1, 0, 1, 1, // Clockwise
        1, 0, 1, 1, 0, 1, 0, 0  // Counter-clockwise
    };

    // Upload the texture coordinates to the buffer
    buffer = (uint8_t*)GX2RLockBufferEx(&texBuffer, GX2R_RESOURCE_BIND_NONE);
    memcpy(&buffer[bufOffset], &texCoords[texOffset], texBuffer.elemSize * 4);
    GX2RUnlockBufferEx(&texBuffer, GX2R_RESOURCE_BIND_NONE);
    GX2RSetAttributeBuffer(&texBuffer, 1, texBuffer.elemSize, bufOffset);
    bufOffset += 8 * sizeof(float);

    // Set shaders and draw the texture
    GX2SetFetchShader(&group.fetchShader);
    GX2SetVertexShader(group.vertexShader);
    GX2SetPixelShader(group.pixelShader);
    GX2SetPixelTexture(&textures[tex], group.pixelShader->samplerVars[0].location);
    GX2SetPixelSampler(&sampler, group.pixelShader->samplerVars[0].location);
    GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 0, 1);
}

int main()
{
    // Initialize various things
    WHBProcInit();
    WHBGfxInit();
    VPADInit();
    WHBMountSdCard();

    // Initialize the shader
    WHBGfxLoadGFDShaderGroup(&group, 0, texture_shader_gsh);
    WHBGfxInitShaderAttribute(&group, "position",     0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32);
    WHBGfxInitShaderAttribute(&group, "tex_coord_in", 1, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32);
    WHBGfxInitFetchShader(&group);

    // Initialize the position coordinate buffer
    posBuffer.flags = GX2R_RESOURCE_BIND_VERTEX_BUFFER | GX2R_RESOURCE_USAGE_CPU_READ |
                      GX2R_RESOURCE_USAGE_CPU_WRITE    | GX2R_RESOURCE_USAGE_GPU_READ;
    posBuffer.elemSize = 2 * sizeof(float);
    posBuffer.elemCount = 4 * 4;
    GX2RCreateBuffer(&posBuffer);

    // Initialize the texture coordinate buffer
    texBuffer.flags = GX2R_RESOURCE_BIND_VERTEX_BUFFER | GX2R_RESOURCE_USAGE_CPU_READ |
                      GX2R_RESOURCE_USAGE_CPU_WRITE    | GX2R_RESOURCE_USAGE_GPU_READ;
    texBuffer.elemSize = 2 * sizeof(float);
    texBuffer.elemCount = 4 * 4;
    GX2RCreateBuffer(&texBuffer);

    // Initialize textures
    for (int i = 0; i < 2; i++)
    {
        textures[i].surface.width    = 256;
        textures[i].surface.height   = 192;
        textures[i].surface.depth    = 1;
        textures[i].surface.dim      = GX2_SURFACE_DIM_TEXTURE_2D;
        textures[i].surface.format   = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
        textures[i].surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
        textures[i].viewNumSlices    = 1;
        textures[i].compMap          = 0x03020100;
        GX2CalcSurfaceSizeAndAlignment(&textures[i].surface);
        GX2InitTextureRegs(&textures[i]);
        textures[i].surface.image = MEMAllocFromDefaultHeapEx(textures[i].surface.imageSize, textures[i].surface.alignment);
    }

    // Initialize the texture sampler
    GX2InitSampler(&sampler, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_LINEAR);

    // Get the base path to look for files in
    std::string base = WHBGetSdCardMountPath();
    base += "/wiiu/apps/noods/";

    // Load the settings
    // If this is the first time, set the default Wii U path settings
    ScreenLayout::addSettings();
    if (!Settings::load(base + "noods.ini"))
    {
        Settings::setBios9Path(base + "bios9.bin");
        Settings::setBios7Path(base + "bios7.bin");
        Settings::setFirmwarePath(base + "firmware.bin");
        Settings::setGbaBiosPath(base + "gba_bios.bin");
        Settings::setSdImagePath(base + "sd.img");
        Settings::save();
    }

    // Initialize the screen layout
    layout.update(854, 480, gbaMode);

    // Initialize the emulator with an NDS and/or GBA ROM
    std::string ndsPath = "", gbaPath = "";
    if (FILE *file = fopen((base + "rom.nds").c_str(), "rb"))
    {
        ndsPath = base + "rom.nds";
        fclose(file);
    }
    if (FILE *file = fopen((base + "rom2.gba").c_str(), "rb"))
    {
        gbaPath = base + "rom2.gba";
        fclose(file);
    }
    core = new Core(ndsPath, gbaPath);

    // Initialize audio output
    SDL_Init(SDL_INIT_AUDIO);
    SDL_AudioSpec AudioSettings, ObtainedSettings;
    AudioSettings.freq = 32768;
    AudioSettings.format = AUDIO_S16MSB;
    AudioSettings.channels = 2;
    AudioSettings.samples = 1024;
    AudioSettings.callback = &audioCallback;
    AudioSettings.userdata = nullptr;
    SDL_AudioDeviceID id = SDL_OpenAudioDevice(nullptr, 0, &AudioSettings, &ObtainedSettings, 0);
    SDL_PauseAudioDevice(id, 0);

    // Start running the emulator
    running = true;
    coreThread = new std::thread(runCore);

    while (WHBProcIsRunning())
    {
        // Scan for gamepad input
        VPADStatus vpadStatus;
        VPADRead(VPAD_CHAN_0, &vpadStatus, 1, nullptr);

        // Send input to the core
        for (int i = 0; i < 12; i++)
        {
            if (vpadStatus.hold & vpadMap[i])
                core->input.pressKey(i);
            else
                core->input.releaseKey(i);
        }

        // Scan for touch input
        VPADTouchData touchData;
        VPADGetTPCalibratedPoint(VPAD_CHAN_0, &touchData, &vpadStatus.tpNormal);

        if (touchData.touched)
        {
            // Determine the touch position relative to the emulated touch screen
            int touchX = layout.getTouchX(touchData.x * 854 / 1280, touchData.y * 480 / 720);
            int touchY = layout.getTouchY(touchData.x * 854 / 1280, touchData.y * 480 / 720);

            // Send the touch coordinates to the core
            core->input.pressScreen();
            core->spi.setTouch(touchX, touchY);
        }
        else
        {
            // If the screen isn't being touched, release the touch screen press
            core->input.releaseScreen();
            core->spi.clearTouch();
        }

        // Update the screen textures when a new frame is ready
        if (core->gpu.getFrame(framebuffer, false))
        {
            for (int i = 0; i < 2; i++)
            {
                uint32_t *dst = (uint32_t*)textures[i].surface.image;
                for (int y = 0; y < 192; y++)
                {
                    memcpy(dst, &framebuffer[(i * 192 + y) * 256], 256 * sizeof(uint32_t));
                    dst += textures[i].surface.pitch;
                }
                GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, textures[i].surface.image, textures[i].surface.imageSize);
            }
        }

        // Start drawing
        bufOffset = 0;
        WHBGfxBeginRender();

        // Draw the TV screen
        WHBGfxBeginRenderTV();
        WHBGfxClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        drawTexture(0, layout.getTopX(), layout.getTopY(), layout.getTopWidth(), layout.getTopHeight());
        drawTexture(1, layout.getBotX(), layout.getBotY(), layout.getBotWidth(), layout.getBotHeight());
        WHBGfxFinishRenderTV();

        // Draw the gamepad screen
        WHBGfxBeginRenderDRC();
        WHBGfxClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        drawTexture(0, layout.getTopX(), layout.getTopY(), layout.getTopWidth(), layout.getTopHeight());
        drawTexture(1, layout.getBotX(), layout.getBotY(), layout.getBotWidth(), layout.getBotHeight());
        WHBGfxFinishRenderDRC();

        // Finish drawing
        WHBGfxFinishRender();
    }

    // Stop running the emulator
    running = false;
    coreThread->join();
    delete coreThread;
    delete core;

    // Clean up graphics objects
    for (int i = 0; i < 2; i++)
        MEMFreeToDefaultHeap(textures[i].surface.image);
    GX2RDestroyBufferEx(&posBuffer, GX2R_RESOURCE_BIND_NONE);
    GX2RDestroyBufferEx(&texBuffer, GX2R_RESOURCE_BIND_NONE);

    // Deinitialize various things
    WHBUnmountSdCard();
    VPADShutdown();
    WHBGfxShutdown();
    WHBProcShutdown();
    return 0;
}
