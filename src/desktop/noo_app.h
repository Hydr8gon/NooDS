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

#ifndef NOO_APP_H
#define NOO_APP_H

#include <portaudio.h>
#include <wx/wx.h>

#define MAX_FRAMES 8

class NooFrame;

class NooApp: public wxApp
{
    public:
        void createFrame();
        void removeFrame(int number);

        void connectCore(int number);
        void disconnCore(int number);

        static int getScreenFilter()     { return screenFilter;    }
        static int getKeyBind(int index) { return keyBinds[index]; }

        static void setScreenFilter(int value)       { screenFilter    = value; }
        static void setKeyBind(int index, int value) { keyBinds[index] = value; }

    private:
        NooFrame *frames[MAX_FRAMES] = {};

        static int screenFilter;
        static int keyBinds[14];

        bool OnInit();

        void update(wxTimerEvent &event);

        static int audioCallback(const void *in, void *out, unsigned long count,
                                 const PaStreamCallbackTimeInfo *info, PaStreamCallbackFlags flags, void *data);

        wxDECLARE_EVENT_TABLE();
};

#endif // NOO_APP_H
