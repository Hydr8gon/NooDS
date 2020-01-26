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

#ifndef NOO_APP_H
#define NOO_APP_H

#include "portaudio.h"
#include <wx/wx.h>

#include "noo_panel.h"

class NooApp: public wxApp
{
    private:
        NooFrame *frame;
        NooPanel *panel;
        Emulator emulator;

        bool OnInit();
        void requestDraw(wxIdleEvent &event);

        static int audioCallback(const void *in, void *out, unsigned long frames,
                                 const PaStreamCallbackTimeInfo *info, PaStreamCallbackFlags flags, void *data);
};

#endif // NOO_APP_H
