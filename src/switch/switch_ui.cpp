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

#include <chrono>
#include <switch.h>

#include "switch_ui.h"

struct VertexData
{
    VertexData(float x, float y, float s, float t, float r, float g, float b):
        x(x), y(y), s(s), t(t), r(r), g(g), b(b) {}

    float x, y;
    float s, t;
    float r, g, b;
};

bool SwitchUI::shouldExit = false;

EGLDisplay SwitchUI::display;
EGLContext SwitchUI::context;
EGLSurface SwitchUI::surface;

GLuint SwitchUI::program;
GLuint SwitchUI::vbo;
GLuint SwitchUI::textures[3];

const char *SwitchUI::vertexShader =
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

const char *SwitchUI::fragmentShader =
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

const uint32_t *SwitchUI::font;
const uint32_t SwitchUI::empty = 0xFFFFFFFF;

const int SwitchUI::charWidths[] =
{
    11,  9, 11, 20, 18, 28, 24,  7, 12, 12,
    14, 24,  9, 12,  9, 16, 21, 21, 21, 21,
    21, 21, 21, 21, 21, 21,  9,  9, 26, 24,
    26, 18, 28, 24, 21, 24, 26, 20, 20, 27,
    23,  9, 17, 21, 16, 31, 27, 29, 19, 29,
    20, 18, 21, 26, 24, 37, 21, 21, 24, 12,
    16, 12, 18, 16,  9, 20, 21, 18, 21, 20,
    10, 20, 20,  8, 12, 19,  9, 30, 20, 21,
    21, 21, 12, 16, 12, 20, 17, 29, 17, 17,
    16,  9,  8,  9, 12,  0, 40, 40, 40, 40
};

bool SwitchUI::darkTheme = false;
Color SwitchUI::palette[6];

bool SwitchUI::touchMode = false;

int SwitchUI::stringWidth(std::string string)
{
    int width = 0;

    // Add the widths of each character in the string
    for (unsigned int i = 0; i < string.size(); i++)
        width += charWidths[string[i] - 32];

    return width;
}

void SwitchUI::initialize()
{
    // Initialize EGL
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, NULL, NULL);
    eglBindAPI(EGL_OPENGL_API);
    EGLConfig config;
    EGLint numConfigs;
    eglChooseConfig(display, {}, &config, 1, &numConfigs);
    surface = eglCreateWindowSurface(display, config, nwindowGetDefault(), NULL);
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, {});
    eglMakeCurrent(display, surface, surface, context);

    gladLoadGL();

    // Compile the vertex shader
    GLint vertShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertShader, 1, &vertexShader, NULL);
    glCompileShader(vertShader);

    // Compile the fragment shader
    GLint fragShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragShader, 1, &fragmentShader, NULL);
    glCompileShader(fragShader);

    // Attach the shaders to the program
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

    // Load the font bitmap
    romfsInit();
    font = bmpToTexture("romfs:/font.bmp");
    romfsExit();

    glGenTextures(3, textures);

    // Prepare a texture for image drawing
    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Prepare the font texture
    glBindTexture(GL_TEXTURE_2D, textures[1]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 512, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, font);

    // Prepare an empty texture for drawing only the vertex color
    glBindTexture(GL_TEXTURE_2D, textures[2]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &empty);

    // Enable alpha blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Determine the system theme
    setsysInitialize();
    ColorSetId theme;
    setsysGetColorSetId(&theme);
    darkTheme = (theme == ColorSetId_Dark);
    setsysExit();

    // Set the theme palette
    if (darkTheme)
    {
        palette[0] = Color( 45,  45,  45);
        palette[1] = Color(255, 255, 255);
        palette[2] = Color( 75,  75,  75);
        palette[3] = Color( 35,  35,  35);
        palette[4] = Color( 85, 185, 225);
        palette[5] = Color(  0, 255, 200);
    }
    else
    {
        palette[0] = Color(235, 235, 235);
        palette[1] = Color( 45,  45,  45);
        palette[2] = Color(205, 205, 205);
        palette[3] = Color(255, 255, 255);
        palette[4] = Color( 50, 215, 210);
        palette[5] = Color( 50,  80, 240);
    }
}

void SwitchUI::deinitialize()
{
    // Clean up
    glDeleteProgram(program);
    glDeleteBuffers(1, &vbo);
    glDeleteTextures(3, textures);
    delete[] font;

    // Deinitialize EGL
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(display, context);
    eglDestroySurface(display, surface);
    eglTerminate(display);
}

uint32_t *SwitchUI::bmpToTexture(std::string filename)
{
    // Attempt to load the file
    FILE *bmp = fopen(filename.c_str(), "rb");
    if (!bmp) return nullptr;

    // Read the bitmap header
    uint8_t header[70];
    fread(header, sizeof(uint8_t), 70, bmp);
    int width = *(int*)&header[18];
    int height = *(int*)&header[22];

    // Read the bitmap data
    uint32_t *data = new uint32_t[width * height];
    fread(data, sizeof(uint32_t), width * height, bmp);
    fclose(bmp);

    // Convert the bitmap data to RGBA8 format
    uint32_t *texture = new uint32_t[width * height];
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            uint32_t color = data[(height - y - 1) * width + x];
            uint8_t b = (color >>  0) & 0xFF;
            uint8_t g = (color >>  8) & 0xFF;
            uint8_t r = (color >> 16) & 0xFF;
            uint8_t a = (color >> 24) & 0xFF;
            texture[y * width + x] = (a << 24) | (b << 16) | (g << 8) | r;
        }
    }

    delete[] data;
    return texture;
}

void SwitchUI::drawImage(uint32_t *image, int width, int height, int x, int y, int scaleWidth, int scaleHeight, bool filter, int rotation)
{
    // Rotate the texture coordinates
    uint8_t texCoords;
    switch (rotation)
    {
        case 0:  texCoords = 0x4B; break; // None
        case 1:  texCoords = 0x2D; break; // Clockwise
        case 2:  texCoords = 0xD2; break; // Counter-clockwise
        default: texCoords = 0xB4; break; // Flipped
    }

    VertexData vertices[] =
    {
        VertexData(x + scaleWidth, y + scaleHeight, (texCoords >> 0) & 1, (texCoords >> 1) & 1, 255, 255, 255),
        VertexData(x,              y + scaleHeight, (texCoords >> 2) & 1, (texCoords >> 3) & 1, 255, 255, 255),
        VertexData(x,              y,               (texCoords >> 4) & 1, (texCoords >> 5) & 1, 255, 255, 255),
        VertexData(x + scaleWidth, y,               (texCoords >> 6) & 1, (texCoords >> 7) & 1, 255, 255, 255)
    };

    // Load the image as a texture and set filtering
    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter ? GL_LINEAR : GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);

    // Load the vertex data and draw the image
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void SwitchUI::drawString(std::string string, int x, int y, int size, Color color, bool alignRight)
{
    float curWidth = 0;

    // Align the string to the right if enabled
    if (alignRight)
        curWidth -= stringWidth(string);

    // Draw each character of the string
    for (unsigned int i = 0; i < string.size(); i++)
    {
        float x1 = x + curWidth * size / 48;
        float x2 = x + (curWidth + 48) * size / 48;
        float s = 48.0f * ((string[i] - 32) % 10) / 512;
        float t = 48.0f * ((string[i] - 32) / 10) / 512;

        VertexData vertices[] =
        {
            VertexData(x1, y + size, s,                 t + (47.0f / 512), color.r, color.g, color.b),
            VertexData(x2, y + size, s + (47.0f / 512), t + (47.0f / 512), color.r, color.g, color.b),
            VertexData(x2, y,        s + (47.0f / 512), t,                 color.r, color.g, color.b),
            VertexData(x1, y,        s,                 t,                 color.r, color.g, color.b)
        };

        // Load the vertex and texture data and draw a character
        glBindTexture(GL_TEXTURE_2D, textures[1]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

        // Adjust position for the next character
        curWidth += charWidths[string[i] - 32];
    }
}

void SwitchUI::drawRectangle(int x, int y, int width, int height, Color color)
{
    VertexData vertices[] =
    {
        VertexData(x + width, y + height, 1.0f, 1.0f, color.r, color.g, color.b),
        VertexData(x,         y + height, 0.0f, 1.0f, color.r, color.g, color.b),
        VertexData(x,         y,          0.0f, 0.0f, color.r, color.g, color.b),
        VertexData(x + width, y,          1.0f, 0.0f, color.r, color.g, color.b)
    };

    // Load the vertex and texture data and draw the rectangle
    glBindTexture(GL_TEXTURE_2D, textures[2]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void SwitchUI::clear(Color color)
{
    // Clear the canvas with the specified color
    glClearColor((float)color.r / 0xFF, (float)color.g / 0xFF, (float)color.b / 0xFF, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void SwitchUI::update()
{
    // Display a new frame
    glFinish();
    eglSwapBuffers(display, surface);
}

Selection SwitchUI::menu(std::string title, std::vector<ListItem> *items, unsigned int index, std::string actionX, std::string actionPlus)
{
    // Format the action strings
    if (actionPlus != "") actionPlus = "\x83 " + actionPlus + "     ";
    if (actionX    != "") actionX    = "\x82 " + actionX    + "     ";
    std::string actionB = "\x81 Back     ";
    std::string actionA = "\x80 OK";

    // Calculate the touch bounds for the action buttons
    unsigned int boundsAB    = 1218 - (stringWidth(actionA) + 2.5f * charWidths[0]) * 34 / 48;
    unsigned int boundsBX    = boundsAB    - stringWidth(actionB)    * 34 / 48;
    unsigned int boundsXPlus = boundsBX    - stringWidth(actionX)    * 34 / 48;
    unsigned int boundsPlus  = boundsXPlus - stringWidth(actionPlus) * 34 / 48;

    bool upHeld = false;
    bool downHeld = false;
    bool scroll = false;
    std::chrono::steady_clock::time_point timeHeld;

    unsigned int touchIndex = 0;
    bool touchStarted = false;
    bool touchScroll = false;
    touchPosition touch, touchMove;

    while (appletMainLoop() && !shouldExit)
    {
        clear(palette[0]);

        // Draw the borders
        drawString(title, 72, 30, 42, palette[1]);
        drawRectangle(30,  88, 1220, 1, palette[1]);
        drawRectangle(30, 648, 1220, 1, palette[1]);
        drawString(actionPlus + actionX + actionB + actionA, 1218, 667, 34, palette[1], true);

        // Scan for key input
        hidScanInput();
        u32 pressed = hidKeysDown(CONTROLLER_P1_AUTO);
        u32 released = hidKeysUp(CONTROLLER_P1_AUTO);

        // Handle up input presses
        if ((pressed & KEY_UP) && !(pressed & KEY_DOWN))
        {
            // If touch mode is active, disable it to make the selection box visible
            // If the selection box is visible, move it up one item
            if (touchMode)
                touchMode = false;
            else if (index > 0)
                index--;

            // Remember when the up input started
            upHeld = true;
            timeHeld = std::chrono::steady_clock::now();
        }

        // Handle down input presses
        if ((pressed & KEY_DOWN) && !(pressed & KEY_UP))
        {
            // If touch mode is active, disable it to make the selection box visible
            // If the selection box is visible, move it down one item
            if (touchMode)
                touchMode = false;
            else if (index < items->size() - 1)
                index++;

            // Remember when the down input started
            downHeld = true;
            timeHeld = std::chrono::steady_clock::now();
        }

        // Return button presses so they can be handled externally
        if (((pressed & KEY_A) && !touchMode) || (pressed & KEY_B) ||
            (actionX != "" && (pressed & KEY_X)) || (actionPlus != "" && (pressed & KEY_PLUS)))
        {
            touchMode = false;
            return Selection(pressed, index);
        }

        // Disable touch mode before allowing A presses so that the selector can be seen
        if ((pressed & KEY_A) && touchMode)
            touchMode = false;

        // Cancel up input if it was released
        if (released & KEY_UP)
        {
            upHeld = false;
            scroll = false;
        }

        // Cancel down input if it was released
        if (released & KEY_DOWN)
        {
            downHeld = false;
            scroll = false;
        }

        // Scroll continuously when a directional input is held
        if ((upHeld && index > 0) || (downHeld && index < items->size() - 1))
        {
            // Wait a bit after the input before starting to scroll
            std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - timeHeld;
            if (!scroll && elapsed.count() > 0.5f)
                scroll = true;

            // Scroll up or down one item at a fixed interval
            if (scroll && elapsed.count() > 0.1f)
            {
                index += upHeld ? -1 : 1;
                timeHeld = std::chrono::steady_clock::now();
            }
        }

        // Handle touch input
        if (hidTouchCount() > 0) // Pressed
        {
            // Track the beginning of a touch input
            if (!touchStarted)
            {
                hidTouchRead(&touch, 0);
                touchStarted = true;
                touchScroll = false;
                touchMode = true;
            }

            // Track the current state of a touch input
            hidTouchRead(&touchMove, 0);

            if (touchScroll)
            {
                // Calculate how far the drag has gone
                int newIndex = touchIndex + (int)(touch.py - touchMove.py) / 70;

                // Scroll the list by adjusting the index
                // At the edges of the list, adjust the index to stay centered
                if (items->size() > 7 && newIndex != (int)touchIndex)
                {
                    if (newIndex < 3)
                        index = 3;
                    else if (newIndex > (int)items->size() - 4)
                        index = items->size() - 4;
                    else
                        index = newIndex;
                }
            }
            else if (touchMove.px > touch.px + 25 || touchMove.px < touch.px - 25 ||
                     touchMove.py > touch.py + 25 || touchMove.py < touch.py - 25)
            {
                // Activate touch scrolling if a touch starts dragging
                touchScroll = true;

                // Remember the initial index before dragging for reference
                // At the edges of the list, adjust the index to stay centered
                if (index < 3)
                    touchIndex = 3;
                else if (index > items->size() - 4)
                    touchIndex = items->size() - 4;
                else
                    touchIndex = index;
            }
        }
        else // Released
        {
            // Simulate a button press if its action text was touched
            // If the touch is dragged instead of just pressed, this won't register
            if (!touchScroll && touch.py >= 650)
            {
                if (touch.px >= boundsBX && touch.px < boundsAB)
                    return Selection(KEY_B | KEY_TOUCH, index);
                else if (touch.px >= boundsXPlus && touch.px < boundsBX)
                    return Selection(KEY_X | KEY_TOUCH, index);
                else if (touch.px >= boundsPlus && touch.px < boundsXPlus)
                    return Selection(KEY_PLUS | KEY_TOUCH, index);
            }

            touchStarted = false;
        }

        // Draw the first item separator
        if (items->size() > 0)
            drawRectangle(90, 124, 1100, 1, palette[2]);

        // Draw the list items
        for (unsigned int i = 0; i < 7; i++)
        {
            if (i < items->size())
            {
                // Determine the scroll offset
                unsigned int offset;
                if (index < 4 || items->size() <= 7)
                    offset = i;
                else if (index > items->size() - 4)
                    offset = items->size() - 7 + i;
                else
                    offset = i + index - 3;

                // Simulate an A press on a selection if it was touched
                // If the touch is dragged instead of just pressed, this won't register
                if (!touchStarted && !touchScroll && touch.px >= 90 && touch.px < 1190 && touch.py >= 124 + i * 70 && touch.py < 194 + i * 70)
                    return Selection(KEY_A | KEY_TOUCH, offset);

                if (!touchMode && offset == index)
                {
                    // Draw the selection box around the selected item if not in touch mode
                    drawRectangle(  90, 125 + i * 70, 1100, 69, palette[3]);
                    drawRectangle(  89, 121 + i * 70, 1103,  5, palette[4]);
                    drawRectangle(  89, 191 + i * 70, 1103,  5, palette[4]);
                    drawRectangle(  88, 122 + i * 70,    5, 73, palette[4]);
                    drawRectangle(1188, 122 + i * 70,    5, 73, palette[4]);
                }
                else
                {
                    // Draw the item separators
                    drawRectangle(90, 194 + i * 70, 1100, 1, palette[2]);
                }

                if ((*items)[offset].iconSize > 0)
                {
                    // Draw the item's icon and its name beside it
                    drawImage((*items)[offset].icon, (*items)[offset].iconSize, (*items)[offset].iconSize, 105, 127 + i * 70, 64, 64);
                    drawString((*items)[offset].name, 184, 140 + i * 70, 38, palette[1]);
                }
                else
                {
                    // Draw the item's name
                    drawString((*items)[offset].name, 105, 140 + i * 70, 38, palette[1]);
                }

                // Draw the item's setting name
                if ((*items)[offset].setting != "")
                    drawString((*items)[offset].setting, 1175, 143 + i * 70, 32, palette[5], true);
            }
        }

        update();
    }

    // appletMainLoop only seems to return false once, so remember when it does
    shouldExit = true;
    return Selection(0, 0);
}

bool SwitchUI::message(std::string title, std::vector<std::string> text, bool cancel)
{
    clear(palette[0]);

    std::string actionB = "\x81 Back     ";
    std::string actionA = "\x80 OK";

    // Calculate the touch bounds for the action buttons
    unsigned int boundsA  = 1218 + (2.5f * charWidths[0]) * 34 / 48;
    unsigned int boundsAB = 1218 - (stringWidth(actionA) + 2.5f * charWidths[0]) * 34 / 48;
    unsigned int boundsB  = boundsAB - stringWidth(actionB) * 34 / 48;

    // Draw the borders
    drawString(title, 72, 30, 42, palette[1]);
    drawRectangle(30,  88, 1220, 1, palette[1]);
    drawRectangle(30, 648, 1220, 1, palette[1]);
    drawString((cancel ? actionB : "") + actionA, 1218, 667, 34, palette[1], true);

    // Draw the message contents
    // Each string in the array is drawn on a separate line
    for (unsigned int i = 0; i < text.size(); i++)
        drawString(text[i], 90, 124 + i * 38, 38, palette[1]);

    update();

    bool touchStarted = false;
    bool touchScroll = false;
    touchPosition touch, touchMove;

    while (appletMainLoop() && !shouldExit)
    {
        // Scan for key input
        hidScanInput();
        u32 pressed = hidKeysDown(CONTROLLER_P1_AUTO);

        // Dismiss the message and return the result if an action is pressed
        if (pressed & KEY_A)
            return true;
        else if ((pressed & KEY_B) && cancel)
            return false;

        // Handle touch input
        if (hidTouchCount() > 0) // Pressed
        {
            // Track the beginning of a touch input
            if (!touchStarted)
            {
                hidTouchRead(&touch, 0);
                touchStarted = true;
                touchScroll = false;
                touchMode = true;
            }

            // Track the current state of a touch input
            hidTouchRead(&touchMove, 0);

            // Activate touch scrolling if a touch starts dragging
            if (touchMove.px > touch.px + 25 || touchMove.px < touch.px - 25 ||
                touchMove.py > touch.py + 25 || touchMove.py < touch.py - 25)
                touchScroll = true;
        }
        else // Released
        {
            // Simulate a button press if its action text was touched
            // If the touch is dragged instead of just pressed, this won't register
            if (!touchScroll && touch.py >= 650)
            {
                if (touch.px >= boundsAB && touch.px < boundsA)
                    return true;
                else if (touch.px >= boundsB && touch.px < boundsAB && cancel)
                    return false;
            }

            touchStarted = false;
        }
    }

    // appletMainLoop only seems to return false once, so remember when it does
    shouldExit = true;
    return false;
}
