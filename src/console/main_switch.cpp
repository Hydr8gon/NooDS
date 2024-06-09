/*
    Copyright 2019-2024 Hydr8gon

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

#ifdef __SWITCH__

#include <cstring>
#include <glad/glad.h>
#include <malloc.h>
#include <switch.h>

#include "console_ui.h"

#define GYRO_TOUCH_RANGE 0.08f

struct VertexData
{
    float x, y;
    float s, t;
    float r, g, b;

    VertexData(float x, float y, float s, float t, float r, float g, float b):
        x(x), y(y), s(s), t(t), r(r), g(g), b(b) {}
};

struct Texture
{
    GLuint tex;
    int width, height;

    Texture(int width, int height):
        width(width), height(height) {}
};

EGLDisplay display;
EGLContext context;
EGLSurface surface;
GLuint program;
GLuint vbo;

PadState pad;
HidSixAxisSensorHandle sensors[3];
int pointerMode;
bool initAngleDirty;
float initAngleX, initAngleZ;
uint32_t toggle;
bool scanned;

AudioOutBuffer audioBuffers[2];
AudioOutBuffer *releasedBuffer;
uint32_t *audioData[2];
uint32_t count;
bool playing = true;

const char *vertexShader =
R"(
    #version 330 core
    precision mediump float;

    layout (location = 0) in vec2 inPos;
    layout (location = 1) in vec2 inTexCoord;
    layout (location = 2) in vec3 inColor;
    out vec2 vtxTexCoord;
    out vec3 vtxColor;

    void main()
    {
        gl_Position = vec4(-1.0 + inPos.x / 640, 1.0 - inPos.y / 360, 0.0, 1.0);
        vtxTexCoord = inTexCoord;
        vtxColor = inColor;
    }
)";

const char *fragmentShader =
R"(
    #version 330 core
    precision mediump float;

    in vec2 vtxTexCoord;
    in vec3 vtxColor;
    out vec4 fragColor;
    uniform sampler2D texDiffuse;

    void main()
    {
        fragColor = texture(texDiffuse, vtxTexCoord) * vec4(vtxColor.x / 255, vtxColor.y / 255, vtxColor.z / 255, 1.0);
    }
)";

uint32_t ConsoleUI::defaultKeys[]
{
    HidNpadButton_A, HidNpadButton_B, HidNpadButton_Minus, HidNpadButton_Plus,
    HidNpadButton_AnyRight, HidNpadButton_AnyLeft, HidNpadButton_AnyUp, HidNpadButton_AnyDown,
    HidNpadButton_ZR, HidNpadButton_ZL, HidNpadButton_X, HidNpadButton_Y,
    HidNpadButton_L | HidNpadButton_R
};

const char *ConsoleUI::keyNames[]
{
    "A", "B", "X", "Y", "L Stick", "R Stick", "L", "R",
    "ZL", "ZR", "Plus", "Minus", "Left", "Up", "Right", "Down",
    "LS Left", "LS Up", "LS Right", "LS Down", "RS Left", "RS Up", "RS Right", "RS Down"
};

void ConsoleUI::startFrame(uint32_t color)
{
    // Convert the clear color to floats
    float r = float(color & 0xFF) / 0xFF;
    float g = float((color >> 8) & 0xFF) / 0xFF;
    float b = float((color >> 16) & 0xFF) / 0xFF;
    float a = float(color >> 24) / 0xFF;

    // Clear the screen for a new frame
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void ConsoleUI::endFrame()
{
    // Finish and display a frame
    glFinish();
    eglSwapBuffers(display, surface);
    scanned = false;
}

void *ConsoleUI::createTexture(uint32_t *data, int width, int height)
{
    // Create a new texture and copy data to it
    Texture *texture = new Texture(width, height);
    glGenTextures(1, &texture->tex);
    glBindTexture(GL_TEXTURE_2D, texture->tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    return texture;
}

void ConsoleUI::destroyTexture(void *texture)
{
    // Clean up a texture
    glDeleteTextures(1, &((Texture*)texture)->tex);
    delete (Texture*)texture;
}

void ConsoleUI::drawTexture(void *texture, float tx, float ty, float tw, float th,
    float x, float y, float w, float h, bool filter, int rotation, uint32_t color)
{
    // Convert texture coordinates to floats
    float s1 = tx / ((Texture*)texture)->width;
    float t1 = ty / ((Texture*)texture)->height;
    float s2 = (tx + tw) / ((Texture*)texture)->width;
    float t2 = (ty + th) / ((Texture*)texture)->height;

    // Convert the surface color to floats
    float r = (color >> 0) & 0xFF;
    float g = (color >> 8) & 0xFF;
    float b = (color >> 16) & 0xFF;

    // Arrange texture coordinates so they can be rotated
    float texCoords[] = { s2, t2, s1, t2, s1, t1, s2, t1 };
    int offsets[] = { 0, 6, 2 };
    int o = offsets[rotation];

    // Define vertex data to upload
    VertexData vertices[] =
    {
        VertexData(x + w, y + h, texCoords[(o + 0) & 0x7], texCoords[(o + 1) & 0x7], r, g, b),
        VertexData(x + 0, y + h, texCoords[(o + 2) & 0x7], texCoords[(o + 3) & 0x7], r, g, b),
        VertexData(x + 0, y + 0, texCoords[(o + 4) & 0x7], texCoords[(o + 5) & 0x7], r, g, b),
        VertexData(x + w, y + 0, texCoords[(o + 6) & 0x7], texCoords[(o + 7) & 0x7], r, g, b)
    };

    // Draw a texture
    glBindTexture(GL_TEXTURE_2D, ((Texture*)texture)->tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter ? GL_LINEAR : GL_NEAREST);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

uint32_t ConsoleUI::getInputHeld()
{
    // Scan for input once per frame
    if (!scanned)
    {
        padUpdate(&pad);
        scanned = true;
    }

    // Return a mask of mappable keys, excluding pause keys if in gyro mode
    uint32_t value = padGetButtons(&pad) & 0xFFFFFF & ~(HidNpadButton_StickL | HidNpadButton_StickR);
    if (toggle) value &= ~keyBinds[INPUT_PAUSE];
    return value;
}

MenuTouch ConsoleUI::getInputTouch()
{
    // Scan for touch input
    HidTouchScreenState touch;
    hidGetTouchScreenStates(&touch, 1);
    return MenuTouch(touch.count > 0, touch.touches[0].x, touch.touches[0].y);
}

void outputAudio()
{
    while (playing)
    {
        // Refill the audio buffers until stopped
        audoutWaitPlayFinish(&releasedBuffer, &count, UINT64_MAX);
        for (uint32_t i = 0; i < count; i++)
        {
            ConsoleUI::fillAudioBuffer((uint32_t*)releasedBuffer[i].buffer, 1024, 48000);
            audoutAppendAudioOutBuffer(&releasedBuffer[i]);
        }
    }
}

MenuTouch gyroTouch()
{
    // Toggle gyro touch mode if a stick is clicked while docked
    if (appletGetOperationMode() == AppletOperationMode_Console && !ConsoleUI::gbaMode)
        toggle ^= (padGetButtonsDown(&pad) & (HidNpadButton_StickL | HidNpadButton_StickR));
    else
        toggle = 0;

    // Do nothing if not in gyro touch mode
    if (!toggle)
    {
        pointerMode = 0;
        return MenuTouch(false, 0, 0);
    }

    // Set the pointer mode depending on which stick was pressed
    if (pointerMode == 0)
    {
        pointerMode = (toggle & HidNpadButton_StickL) ? 1 : 2;
        initAngleDirty = true;
    }

    // Read a controller's gyro state; for Joy-Cons, use the one whose stick was clicked
    HidSixAxisSensorState sensorState;
    bool joycon = (padGetStyleSet(&pad) & HidNpadStyleTag_NpadJoyDual);
    hidGetSixAxisSensorStates(sensors[joycon ? pointerMode : 0], &sensorState, 1);

    // Save the initial angle to be used as a center position
    if (initAngleDirty)
    {
        initAngleX = sensorState.angle.x;
        initAngleZ = sensorState.angle.z;
        initAngleDirty = false;
    }

    // Get the current motion angle, clamped, relative to the initial angle
    float relativeX = -std::max(std::min(sensorState.angle.z - initAngleZ,
        GYRO_TOUCH_RANGE / 2), -(GYRO_TOUCH_RANGE / 2)) + GYRO_TOUCH_RANGE / 2;
    float relativeY = -std::max(std::min(sensorState.angle.x - initAngleX,
        GYRO_TOUCH_RANGE / 2), -(GYRO_TOUCH_RANGE / 2)) + GYRO_TOUCH_RANGE / 2;

    // Scale the motion angle to a position on the touch screen
    int screenX = ConsoleUI::layout.botX + relativeX * ConsoleUI::layout.botWidth / GYRO_TOUCH_RANGE;
    int screenY = ConsoleUI::layout.botY + relativeY * ConsoleUI::layout.botHeight / GYRO_TOUCH_RANGE;

    // Touch the screen when L or R are pressed
    uint32_t held = padGetButtons(&pad);
    bool touched = held & (HidNpadButton_L | HidNpadButton_R);

    // Draw a pointer on the screen to show the current touch position
    uint32_t color = touched ? 0xFF7F7F7F : 0xFFFFFFFF;
    ConsoleUI::drawRectangle(screenX - 10, screenY - 10, 20, 20, 0);
    ConsoleUI::drawRectangle(screenX - 8, screenY - 8, 16, 16, color);
    return MenuTouch(touched, screenX, screenY);
}

int main(int argc, char **argv)
{
    // Overclock the Switch CPU
    ClkrstSession cpuSession;
    clkrstInitialize();
    clkrstOpenSession(&cpuSession, PcvModuleId_CpuBus, 0);
    clkrstSetClockRate(&cpuSession, 1785000000);

    // Initialize OpenGL
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, nullptr, nullptr);
    eglBindAPI(EGL_OPENGL_API);
    EGLConfig config;
    EGLint numConfigs;
    eglChooseConfig(display, {}, &config, 1, &numConfigs);
    surface = eglCreateWindowSurface(display, config, nwindowGetDefault(), nullptr);
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, {});
    eglMakeCurrent(display, surface, surface, context);
    gladLoadGL();

    // Compile the vertex and fragment shaders
    GLint vertShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertShader, 1, &vertexShader, nullptr);
    glCompileShader(vertShader);
    GLint fragShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragShader, 1, &fragmentShader, nullptr);
    glCompileShader(fragShader);

    // Attach shaders to the program
    program = glCreateProgram();
    glAttachShader(program, vertShader);
    glAttachShader(program, fragShader);
    glLinkProgram(program);
    glUseProgram(program);
    glDeleteShader(vertShader);
    glDeleteShader(fragShader);

    // Set up the vertex buffer
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, x));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, s));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, r));
    glEnableVertexAttribArray(2);

    // Enable alpha blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Initialize audio and buffers
    audoutInitialize();
    for (int i = 0; i < 2; i++)
    {
        int size = 1024 * sizeof(uint32_t);
        audioData[i] = (uint32_t*)memalign(0x1000, size);
        memset(audioData[i], 0, size);
        audioBuffers[i].next = nullptr;
        audioBuffers[i].buffer = audioData[i];
        audioBuffers[i].buffer_size = size;
        audioBuffers[i].data_size = size;
        audioBuffers[i].data_offset = 0;
        audoutAppendAudioOutBuffer(&audioBuffers[i]);
    }

    // Start audio output
    audoutStartAudioOut();
    std::thread audioThread(outputAudio);

    // Initialize input
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);
    hidInitializeTouchScreen();

    // Initialize motion sensors
    hidGetSixAxisSensorHandles(&sensors[0], 1, HidNpadIdType_No1, HidNpadStyleTag_NpadFullKey);
    hidGetSixAxisSensorHandles(&sensors[1], 2, HidNpadIdType_No1, HidNpadStyleTag_NpadJoyDual);
    for (int i = 0; i < 3; i++) hidStartSixAxisSensor(sensors[i]);

    // Get the system language
    uint64_t langCode;
    SetLanguage lang;
    setInitialize();
    setGetSystemLanguage(&langCode);
    setMakeLanguage(langCode, &lang);
    setExit();

    // Set the language for the generated firmware
    switch (lang)
    {
        case SetLanguage_JA: Spi::setLanguage(LG_JAPANESE); break;
        case SetLanguage_FRCA:
        case SetLanguage_FR: Spi::setLanguage(LG_FRENCH); break;
        case SetLanguage_DE: Spi::setLanguage(LG_GERMAN); break;
        case SetLanguage_IT: Spi::setLanguage(LG_ITALIAN); break;
        case SetLanguage_ES419:
        case SetLanguage_ES: Spi::setLanguage(LG_SPANISH); break;
        default: Spi::setLanguage(LG_ENGLISH); break;
    }

    // Initialize the UI and open the file browser if argument loading fails
    ConsoleUI::initialize(1280, 720, "sdmc:/", "sdmc:/switch/noods/");
    if (argc < 2 || ConsoleUI::setPath(argv[1]) < 2)
        ConsoleUI::fileBrowser();

    // Run the emulator until it exits
    ConsoleUI::mainLoop(gyroTouch);
    playing = false;
    audioThread.join();
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(display, context);
    eglDestroySurface(display, surface);
    eglTerminate(display);
    return 0;
}

#endif // __SWITCH__
