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

wxBEGIN_EVENT_TABLE(NooCanvas, wxGLCanvas)
EVT_PAINT(NooCanvas::draw)
EVT_KEY_DOWN(NooCanvas::pressKey)
EVT_KEY_UP(NooCanvas::releaseKey)
EVT_LEFT_DOWN(NooCanvas::pressScreen)
EVT_MOTION(NooCanvas::pressScreen)
EVT_LEFT_UP(NooCanvas::releaseScreen)
wxEND_EVENT_TABLE()

NooCanvas::NooCanvas(wxFrame *frame, Emulator *emulator):
    wxGLCanvas(frame, wxID_ANY, nullptr, wxDefaultPosition, wxSize(256, 192 * 2)), emulator(emulator)
{
    // Prepare the OpenGL context
    context = new wxGLContext(this);
    wxGLCanvas::SetCurrent(*context);

    // Prepare a texture for the framebuffer
    GLuint texture;
    glEnable(GL_TEXTURE_2D);
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    // Set focus so that key presses will be registered
    SetFocus();
}

void NooCanvas::draw(wxPaintEvent &event)
{
    // Clear the screen
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Set filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, NooApp::getScreenFilter() ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, NooApp::getScreenFilter() ? GL_LINEAR : GL_NEAREST);

    uint32_t framebuffer[256 * 192 * 2];

    // Convert the framebuffer to RGBA8 format
    for (int i = 0; i < 256 * 192 * 2; i++)
    {
        uint32_t color = emulator->core ? emulator->core->getFramebuffer()[i] : 0;
        uint8_t r = ((color >>  0) & 0x3F) * 255 / 63;
        uint8_t g = ((color >>  6) & 0x3F) * 255 / 63;
        uint8_t b = ((color >> 12) & 0x3F) * 255 / 63;
        framebuffer[i] = (0xFF << 24) | (b << 16) | (g << 8) | r;
    }

    // Scale the display based on the aspect ratio of the window
    // If the window is wider than the DS ratio, scale to the height of the window
    // If the window is taller than the DS ratio, scale to the width of the window
    wxSize size = GetSize();
    float ratio = 256.0f / (192 * 2);
    float window = (float)size.x / size.y;
    scale = ((ratio >= window) ? (float)size.x / 256 : (float)size.y / (192 * 2));

    // Limit the scale to integer values if enabled
    if (NooApp::getIntegerScale())
        scale = (int)scale;

    // Center the display
    x = (size.x - scale * 256)     / 2;
    y = (size.y - scale * 192 * 2) / 2;

    // Update the display dimensions
    glViewport(x, y, scale * 256, scale * 192 * 2);

    // Draw the framebuffer
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 192 * 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, framebuffer);
    glBegin(GL_QUADS);
    glTexCoord2i(1, 1); glVertex2f( 1, -1);
    glTexCoord2i(0, 1); glVertex2f(-1, -1);
    glTexCoord2i(0, 0); glVertex2f(-1,  1);
    glTexCoord2i(1, 0); glVertex2f( 1,  1);
    glEnd();

    glFlush();
    SwapBuffers();
}

void NooCanvas::pressKey(wxKeyEvent &event)
{
    if (!emulator->running) return;

    // Send a key press to the core
    for (int i = 0; i < 12; i++)
    {
        if (event.GetKeyCode() == NooApp::getKeyMap(i))
            emulator->core->pressKey(i);
    }
}

void NooCanvas::releaseKey(wxKeyEvent &event)
{
    if (!emulator->running) return;

    // Send a key release to the core
    for (int i = 0; i < 12; i++)
    {
        if (event.GetKeyCode() == NooApp::getKeyMap(i))
            emulator->core->releaseKey(i);
    }
}

void NooCanvas::pressScreen(wxMouseEvent &event)
{
    // Ensure the left mouse button is clicked
    if (!emulator->running || !event.LeftIsDown()) return;

    // Determine the touch position relative to the emulated touch screen
    int touchX = (float)(event.GetX() - x) / scale;
    int touchY = (float)(event.GetY() - y) / scale - 192;

    // Send the touch coordinates to the core
    emulator->core->pressScreen(touchX, touchY);
}

void NooCanvas::releaseScreen(wxMouseEvent &event)
{
    // Send a touch release to the core
    if (emulator->running) emulator->core->releaseScreen();
}
