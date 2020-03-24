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

#ifndef NOO_CANVAS_H
#define NOO_CANVAS_H

#include <wx/wx.h>
#include <wx/glcanvas.h>

#include "noo_frame.h"

class NooCanvas: public wxGLCanvas
{
    public:
        NooCanvas(wxFrame *frame, Emulator *emulator);

    private:
        Emulator *emulator;
        wxGLContext *context;

        int x = 0, y = 0;
        float scale = 1.0f;

        void draw(wxPaintEvent &event);
        void pressKey(wxKeyEvent &event);
        void releaseKey(wxKeyEvent &event);
        void pressScreen(wxMouseEvent &event);
        void releaseScreen(wxMouseEvent &event);

        wxDECLARE_EVENT_TABLE();
};

#endif // NOO_CANVAS_H
