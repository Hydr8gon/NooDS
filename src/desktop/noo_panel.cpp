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

#include <wx/rawbmp.h>

#include "noo_panel.h"
#include "noo_app.h"

wxBEGIN_EVENT_TABLE(NooPanel, wxPanel)
EVT_ERASE_BACKGROUND(NooPanel::clear)
EVT_PAINT(NooPanel::draw)
EVT_SIZE(NooPanel::resize)
EVT_KEY_DOWN(NooPanel::pressKey)
EVT_KEY_UP(NooPanel::releaseKey)
EVT_LEFT_DOWN(NooPanel::pressScreen)
EVT_MOTION(NooPanel::pressScreen)
EVT_LEFT_UP(NooPanel::releaseScreen)
wxEND_EVENT_TABLE()

NooPanel::NooPanel(wxFrame *frame, Emulator *emulator): wxPanel(frame, wxID_ANY, wxDefaultPosition, wxSize(256, 192 * 2)), emulator(emulator)
{
    // Set focus so that key presses will be registered
    SetFocus();
}

void NooPanel::clear(wxEraseEvent &event)
{
    // Clearing the screen can cause flickering, so only do it when necessary (on resize)
    if (needsClear)
    {
        needsClear = false;
        event.Skip(true);
    }
}

void NooPanel::draw(wxPaintEvent &event)
{
    wxBitmap bmp(256, 192 * 2, 24);
    wxNativePixelData data(bmp);
    wxNativePixelData::Iterator iter(data);

    // Copy the framebuffer to a bitmap
    for (int y = 0; y < 192 * 2; y++)
    {
        wxNativePixelData::Iterator pixel = iter;

        // Convert the color values from 6-bit to 8-bit
        for (int x = 0; x < 256; x++, pixel++)
        {
            uint32_t color = emulator->core ? emulator->core->getFramebuffer()[y * 256 + x] : 0;
            pixel.Red()   = ((color >>  0) & 0x3F) * 255 / 63;
            pixel.Green() = ((color >>  6) & 0x3F) * 255 / 63;
            pixel.Blue()  = ((color >> 12) & 0x3F) * 255 / 63;
        }

        iter.OffsetY(data, 1);
    }

    // Draw the bitmap
    wxPaintDC dc(this);
    dc.SetUserScale(scale, scale);
    dc.DrawBitmap(bmp, wxPoint(x, y));
}

void NooPanel::resize(wxSizeEvent &event)
{
    // Scale the display based on the aspect ratio of the window
    // If the window is wider than the DS ratio, scale to the height of the window
    // If the window is taller than the DS ratio, scale to the width of the window
    wxSize size = GetSize();
    float ratio = 256.0f / (192 * 2);
    float window = (float)size.x / size.y;
    scale = ((ratio >= window) ? (float)size.x / 256 : (float)size.y / (192 * 2));

    // Center the display
    x = ((float)size.x / scale - 256)     / 2;
    y = ((float)size.y / scale - 192 * 2) / 2;

    needsClear = true;
}

void NooPanel::pressKey(wxKeyEvent &event)
{
    if (!emulator->running) return;

    // Send a key press to the core
    for (int i = 0; i < 12; i++)
    {
        if (event.GetKeyCode() == NooApp::getKeyMap(i))
            emulator->core->pressKey(i);
    }
}

void NooPanel::releaseKey(wxKeyEvent &event)
{
    if (!emulator->running) return;

    // Send a key release to the core
    for (int i = 0; i < 12; i++)
    {
        if (event.GetKeyCode() == NooApp::getKeyMap(i))
            emulator->core->releaseKey(i);
    }
}

void NooPanel::pressScreen(wxMouseEvent &event)
{
    // Ensure the left mouse button is clicked
    if (!emulator->running || !event.LeftIsDown()) return;

    // Determine the touch position relative to the emulated touch screen
    int touchX = (float)event.GetX() / scale - x;
    int touchY = (float)event.GetY() / scale - y - 192;

    // Send the touch coordinates to the core
    emulator->core->pressScreen(touchX, touchY);
}

void NooPanel::releaseScreen(wxMouseEvent &event)
{
    // Send a touch release to the core
    if (emulator->running) emulator->core->releaseScreen();
}
