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

#ifndef NOO_CANVAS_H
#define NOO_CANVAS_H

#include <chrono>
#include <wx/wx.h>

#include "../common/screen_layout.h"

class NooFrame;

#ifdef USE_GL_CANVAS
#include <wx/glcanvas.h>
#define CANVAS_CLASS wxGLCanvas
#define CANVAS_PARAM nullptr
#else
class wxGLContext;
#define CANVAS_CLASS wxPanel
#define CANVAS_PARAM wxDefaultPosition
#endif

class NooCanvas: public CANVAS_CLASS
{
    public:
        bool gbaMode = false;

        NooCanvas(NooFrame *frame);
        ~NooCanvas();

        void resetFrame() { sizeReset = 2; }
        void finish() { finished = true; }

    private:
        NooFrame *frame;
        wxGLContext *context;
        uint32_t *framebuffer;
        bool splitScreens;

        ScreenLayout layout;
        uint8_t sizeReset = 0;
        bool finished = false;

        int frameCount = 0;
        int swapInterval = 0;
        int refreshRate = 0;
        std::chrono::steady_clock::time_point lastRateTime;

        void drawScreen(int x, int y, int w, int h, int wb, int hb, uint32_t *buf);

        void draw(wxPaintEvent &event);
        void resize(wxSizeEvent &event);
        void pressKey(wxKeyEvent &event);
        void releaseKey(wxKeyEvent &event);
        void pressScreen(wxMouseEvent &event);
        void releaseScreen(wxMouseEvent &event);

        wxDECLARE_EVENT_TABLE();
};

#endif // NOO_CANVAS_H
