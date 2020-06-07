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

    frame->SendSizeEvent();
}

NooCanvas::~NooCanvas()
{
    // Free the framebuffer
    if (framebuffer) delete[] framebuffer;
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
        // Request a new frame
        bool gba = (emulator->core->isGbaMode() && ScreenLayout::getGbaCrop());
        uint32_t *fb = emulator->core->getFrame(gba);

        if (fb)
        {
            // Update the frame if a new one was ready
            // If not, the old frame will be drawn so the screen layout can still update
            if (framebuffer) delete[] framebuffer;
            framebuffer = fb;

            // Update GBA mode status to match the new frame
            if (gbaMode != gba)
            {
                gbaMode = gba;
                frame->SendSizeEvent();
            }
        }

        // Rotate the texture coordinates
        uint8_t texCoords;
        switch (ScreenLayout::getScreenRotation())
        {
            case 0: texCoords = 0x4B; break; // None
            case 1: texCoords = 0x2D; break; // Clockwise
            case 2: texCoords = 0xD2; break; // Counter-clockwise
        }

        if (gbaMode)
        {
            // Draw the GBA screen
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 240, 160, 0, GL_RGBA, GL_UNSIGNED_BYTE, &framebuffer[0]);
            glBegin(GL_QUADS);
            glTexCoord2i((texCoords >> 0) & 1, (texCoords >> 1) & 1);
            glVertex2i(layout.getTopX() + layout.getTopWidth(), layout.getTopY() + layout.getTopHeight());
            glTexCoord2i((texCoords >> 2) & 1, (texCoords >> 3) & 1);
            glVertex2i(layout.getTopX(), layout.getTopY() + layout.getTopHeight());
            glTexCoord2i((texCoords >> 4) & 1, (texCoords >> 5) & 1);
            glVertex2i(layout.getTopX(), layout.getTopY());
            glTexCoord2i((texCoords >> 6) & 1, (texCoords >> 7) & 1);
            glVertex2i(layout.getTopX() + layout.getTopWidth(), layout.getTopY());
            glEnd();
        }
        else // NDS mode
        {
            // Draw the DS top screen
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 192, 0, GL_RGBA, GL_UNSIGNED_BYTE, &framebuffer[0]);
            glBegin(GL_QUADS);
            glTexCoord2i((texCoords >> 0) & 1, (texCoords >> 1) & 1);
            glVertex2i(layout.getTopX() + layout.getTopWidth(), layout.getTopY() + layout.getTopHeight());
            glTexCoord2i((texCoords >> 2) & 1, (texCoords >> 3) & 1);
            glVertex2i(layout.getTopX(), layout.getTopY() + layout.getTopHeight());
            glTexCoord2i((texCoords >> 4) & 1, (texCoords >> 5) & 1);
            glVertex2i(layout.getTopX(), layout.getTopY());
            glTexCoord2i((texCoords >> 6) & 1, (texCoords >> 7) & 1);
            glVertex2i(layout.getTopX() + layout.getTopWidth(), layout.getTopY());
            glEnd();

            // Draw the DS bottom screen
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 192, 0, GL_RGBA, GL_UNSIGNED_BYTE, &framebuffer[256 * 192]);
            glBegin(GL_QUADS);
            glTexCoord2i((texCoords >> 0) & 1, (texCoords >> 1) & 1);
            glVertex2i(layout.getBotX() + layout.getBotWidth(), layout.getBotY() + layout.getBotHeight());
            glTexCoord2i((texCoords >> 2) & 1, (texCoords >> 3) & 1);
            glVertex2i(layout.getBotX(), layout.getBotY() + layout.getBotHeight());
            glTexCoord2i((texCoords >> 4) & 1, (texCoords >> 5) & 1);
            glVertex2i(layout.getBotX(), layout.getBotY());
            glTexCoord2i((texCoords >> 6) & 1, (texCoords >> 7) & 1);
            glVertex2i(layout.getBotX() + layout.getBotWidth(), layout.getBotY());
            glEnd();
        }

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
    // Update the screen layout
    wxSize size = GetSize();
    layout.update(size.x, size.y, gbaMode);
    frame->SetMinClientSize(wxSize(layout.getMinWidth(), layout.getMinHeight()));

    // Update the display dimensions
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, size.x, size.y, 0, -1, 1);
    glViewport(0, 0, size.x, size.y);

    // Set filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, NooApp::getScreenFilter() ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, NooApp::getScreenFilter() ? GL_LINEAR : GL_NEAREST);
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

    // Determine the touch position relative to the emulated touch screen
    int touchX = layout.getTouchX(event.GetX(), event.GetY());
    int touchY = layout.getTouchY(event.GetX(), event.GetY());

    // Send the touch coordinates to the core
    emulator->core->pressScreen(touchX, touchY);
}

void NooCanvas::releaseScreen(wxMouseEvent &event)
{
    // Send a touch release to the core
    if (emulator->running) emulator->core->releaseScreen();
}
