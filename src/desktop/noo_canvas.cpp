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

#include "noo_canvas.h"
#include "noo_app.h"

#ifdef _WIN32
#include <GL/gl.h>
#include <GL/glext.h>
#endif

wxBEGIN_EVENT_TABLE(NooCanvas, wxGLCanvas)
EVT_PAINT(NooCanvas::draw)
EVT_SIZE(NooCanvas::resize)
EVT_KEY_DOWN(NooCanvas::pressKey)
EVT_KEY_UP(NooCanvas::releaseKey)
EVT_LEFT_DOWN(NooCanvas::pressScreen)
EVT_MOTION(NooCanvas::pressScreen)
EVT_LEFT_UP(NooCanvas::releaseScreen)
wxEND_EVENT_TABLE()

NooCanvas::NooCanvas(NooFrame *frame, Emulator *emulator): wxGLCanvas(frame, wxID_ANY, nullptr), frame(frame), emulator(emulator)
{
    // Prepare the OpenGL context
    context = new wxGLContext(this);
    wxGLCanvas::SetCurrent(*context);

    // Prepare a texture for the framebuffer
    GLuint texture;
    glEnable(GL_TEXTURE_2D);
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Set focus so that key presses will be registered
    SetFocus();
}

void NooCanvas::draw(wxPaintEvent &event)
{
    // Continuous rendering can prevent the canvas from closing, so only render when needed
    if (!emulator->core && !display) return;

    // Clear the window
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (emulator->core)
    {
        uint32_t framebuffer[256 * 192 * 2];
        uint8_t texCoords;

        // Convert the framebuffer to RGBA8 format
        for (int i = 0; i < 256 * 192 * 2; i++)
        {
            uint32_t color = emulator->core->getFramebuffer()[i];
            uint8_t r = ((color >>  0) & 0x3F) * 255 / 63;
            uint8_t g = ((color >>  6) & 0x3F) * 255 / 63;
            uint8_t b = ((color >> 12) & 0x3F) * 255 / 63;
            framebuffer[i] = (0xFF << 24) | (b << 16) | (g << 8) | r;
        }

        // Rotate the texture coordinates
        switch (NooApp::getScreenRotation())
        {
            case 0: texCoords = 0x4B; break; // None
            case 1: texCoords = 0x2D; break; // Clockwise
            case 2: texCoords = 0xD2; break; // Counter-clockwise
        }

        // Draw the top screen
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 192, 0, GL_RGBA, GL_UNSIGNED_BYTE, &framebuffer[0]);
        glBegin(GL_QUADS);
        glTexCoord2i((texCoords >> 0) & 1, (texCoords >> 1) & 1); glVertex2i(topX + topWidth, topY + topHeight);
        glTexCoord2i((texCoords >> 2) & 1, (texCoords >> 3) & 1); glVertex2i(topX,            topY + topHeight);
        glTexCoord2i((texCoords >> 4) & 1, (texCoords >> 5) & 1); glVertex2i(topX,            topY);
        glTexCoord2i((texCoords >> 6) & 1, (texCoords >> 7) & 1); glVertex2i(topX + topWidth, topY);
        glEnd();

        // Draw the bottom screen
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 192, 0, GL_RGBA, GL_UNSIGNED_BYTE, &framebuffer[256 * 192]);
        glBegin(GL_QUADS);
        glTexCoord2i((texCoords >> 0) & 1, (texCoords >> 1) & 1); glVertex2i(botX + botWidth, botY + botHeight);
        glTexCoord2i((texCoords >> 2) & 1, (texCoords >> 3) & 1); glVertex2i(botX,            botY + botHeight);
        glTexCoord2i((texCoords >> 4) & 1, (texCoords >> 5) & 1); glVertex2i(botX,            botY);
        glTexCoord2i((texCoords >> 6) & 1, (texCoords >> 7) & 1); glVertex2i(botX + botWidth, botY);
        glEnd();

        display = true;
    }
    else
    {
        // Stop rendering until the core is running again
        // The current frame will clear the window
        display = false;
    }

    glFlush();
    SwapBuffers();
}

void NooCanvas::resize(wxSizeEvent &event)
{
    wxSize size = GetSize();

    // Update the display dimensions
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, size.x, size.y, 0, -1, 1);
    glViewport(0, 0, size.x, size.y);

    // Set filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, NooApp::getScreenFilter() ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, NooApp::getScreenFilter() ? GL_LINEAR : GL_NEAREST);

    // Determine the screen arrangement based on the current settings
    // In automatic mode, the arrangement is horizontal if rotated and vertical otherwise
    bool vertical = (NooApp::getScreenArrangement() == 1 ||
        (NooApp::getScreenArrangement() == 0 && NooApp::getScreenRotation() == 0));

    // Determine the screen dimensions based on the current rotation
    int width  = (NooApp::getScreenRotation() ? 192 : 256);
    int height = (NooApp::getScreenRotation() ? 256 : 192);

    float largeScale, smallScale;

    // Calculate the scale of each screen
    // When calculating scale, if the window is wider than the screen, the screen is scaled to the height of the window
    // If the window is taller than the screen, the screen is scaled to the width of the window
    // If gap is enabled, each screen is given half of the gap as extra weight for scaling
    // This results in a gap that is scaled with the screens, and averages if the screens are different scales
    if (vertical)
    {
        // Add the extra gap weight if enabled
        if (NooApp::getScreenGap())
            height += 48;

        frame->SetMinClientSize(wxSize(width, height * 2));

        if (NooApp::getScreenSizing() == 0) // Even
        {
            // Scale both screens to the size of the window
            float baseRatio = (float)width / (height * 2);
            float screenRatio = (float)size.x / size.y;
            largeScale = ((baseRatio > screenRatio) ? ((float)size.x / width) : ((float)size.y / (height * 2)));
            if (NooApp::getIntegerScale()) largeScale = (int)largeScale;
            smallScale = largeScale;
        }
        else // Enlarge Top/Bottom
        {
            float baseRatio = (float)width / height;

            // Scale the large screen to the size of the window minus room for the smaller screen
            float largeRatio = (float)size.x / (size.y - height);
            largeScale = ((baseRatio > largeRatio) ? ((float)size.x / width) : ((float)(size.y - height) / height));
            if (NooApp::getIntegerScale()) largeScale = (int)largeScale;

            // Scale the small screen to the remaining window space
            float smallRatio = (float)size.x / (size.y - largeScale * height);
            smallScale = ((baseRatio > smallRatio) ? ((float)size.x / width) : ((float)(size.y - largeScale * height) / height));
            if (NooApp::getIntegerScale()) smallScale = (int)smallScale;
        }

        // Remove the extra gap weight for the next calculations
        if (NooApp::getScreenGap())
            height -= 48;
    }
    else // Horizontal
    {
        // Add the extra gap weight if enabled
        if (NooApp::getScreenGap())
            width += 48;

        frame->SetMinClientSize(wxSize(width * 2, height));

        if (NooApp::getScreenSizing() == 0) // Even
        {
            // Scale both screens to the size of the window
            float baseRatio = (float)(width * 2) / height;
            float screenRatio = (float)size.x / size.y;
            largeScale = ((baseRatio > screenRatio) ? ((float)size.x / (width * 2)) : ((float)size.y / height));
            if (NooApp::getIntegerScale()) largeScale = (int)largeScale;
            smallScale = largeScale;
        }
        else // Enlarge Top/Enlarge Bottom
        {
            float baseRatio = (float)width / height;

            // Scale the large screen to the size of the window minus room for the smaller screen
            float largeRatio = (float)(size.x - width) / size.y;
            largeScale = ((baseRatio > largeRatio) ? ((float)(size.x - width) / width) : ((float)size.y / height));
            if (NooApp::getIntegerScale()) largeScale = (int)largeScale;

            // Scale the small screen to the remaining window space
            float smallRatio = (float)(size.x - largeScale * width) / size.y;
            smallScale = ((baseRatio > smallRatio) ? ((float)(size.x - largeScale * width) / width) : ((float)size.y / height));
            if (NooApp::getIntegerScale()) smallScale = (int)smallScale;
        }

        // Remove the extra gap weight for the next calculations
        if (NooApp::getScreenGap())
            width -= 48;
    }

    // Calculate the dimensions of each screen
    if (NooApp::getScreenSizing() == 1) // Enlarge Top
    {
        topWidth  = largeScale * width;
        botWidth  = smallScale * width;
        topHeight = largeScale * height;
        botHeight = smallScale * height;
    }
    else // Even/Enlarge Bottom
    {
        topWidth  = smallScale * width;
        botWidth  = largeScale * width;
        topHeight = smallScale * height;
        botHeight = largeScale * height;
    }

    // Calculate the positions of each screen
    // The screens are centered and placed next to each other either vertically or horizontally
    if (vertical)
    {
        topX = (size.x - topWidth) / 2;
        botX = (size.x - botWidth) / 2;

        // Swap the screens if rotated clockwise to keep the top above the bottom
        if (NooApp::getScreenRotation() == 1) // Clockwise
        {
            botY = (size.y - botHeight - topHeight) / 2;
            topY = botY + botHeight;

            // Add the gap between the screens if enabled
            if (NooApp::getScreenGap())
            {
                botY -= (largeScale * 48 + smallScale * 48) / 2;
                topY += (largeScale * 48 + smallScale * 48) / 2;
            }
        }
        else // None/Counter-Clockwise
        {
            topY = (size.y - topHeight - botHeight) / 2;
            botY = topY + topHeight;

            // Add the gap between the screens if enabled
            if (NooApp::getScreenGap())
            {
                topY -= (largeScale * 48 + smallScale * 48) / 2;
                botY += (largeScale * 48 + smallScale * 48) / 2;
            }
        }
    }
    else // Horizontal
    {
        topY = (size.y - topHeight) / 2;
        botY = (size.y - botHeight) / 2;

        // Swap the screens if rotated clockwise to keep the top above the bottom
        if (NooApp::getScreenRotation() == 1) // Clockwise
        {
            botX = (size.x - botWidth - topWidth) / 2;
            topX = botX + botWidth;

            // Add the gap between the screens if enabled
            if (NooApp::getScreenGap())
            {
                botX -= (largeScale * 48 + smallScale * 48) / 2;
                topX += (largeScale * 48 + smallScale * 48) / 2;
            }
        }
        else // None/Counter-Clockwise
        {
            topX = (size.x - topWidth - botWidth) / 2;
            botX = topX + topWidth;

            // Add the gap between the screens if enabled
            if (NooApp::getScreenGap())
            {
                topX -= (largeScale * 48 + smallScale * 48) / 2;
                botX += (largeScale * 48 + smallScale * 48) / 2;
            }
        }
    }
}

void NooCanvas::pressKey(wxKeyEvent &event)
{
    if (!emulator->running) return;

    // Send a key press to the core
    for (int i = 0; i < 12; i++)
    {
        if (event.GetKeyCode() == NooApp::getKeyBind(i))
            emulator->core->pressKey(i);
    }
}

void NooCanvas::releaseKey(wxKeyEvent &event)
{
    if (!emulator->running) return;

    // Send a key release to the core
    for (int i = 0; i < 12; i++)
    {
        if (event.GetKeyCode() == NooApp::getKeyBind(i))
            emulator->core->releaseKey(i);
    }
}

void NooCanvas::pressScreen(wxMouseEvent &event)
{
    // Ensure the left mouse button is clicked
    if (!emulator->running || !event.LeftIsDown()) return;

    int touchX, touchY;

    // Determine the touch position relative to the emulated touch screen
    switch (NooApp::getScreenRotation())
    {
        case 0: // None
        {
            touchX = (event.GetX() - botX) * 256 / botWidth;
            touchY = (event.GetY() - botY) * 192 / botHeight;
            break;
        }

        case 1: // Clockwise
        {
            touchX =       (event.GetY() - botY) * 256 / botHeight;
            touchY = 191 - (event.GetX() - botX) * 192 / botWidth;
            break;
        }

        case 2: // Counter-clockwise
        {
            touchX = 255 - (event.GetY() - botY) * 256 / botHeight;
            touchY =       (event.GetX() - botX) * 192 / botWidth;
            break;
        }
    }

    // Send the touch coordinates to the core
    emulator->core->pressScreen(touchX, touchY);
}

void NooCanvas::releaseScreen(wxMouseEvent &event)
{
    // Send a touch release to the core
    if (emulator->running) emulator->core->releaseScreen();
}
