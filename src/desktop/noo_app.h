/*
    Copyright 2019-2023 Hydr8gon

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
#define MAX_KEYS  17

class NooFrame;

class NooApp: public wxApp
{
    public:
        void createFrame();
        void removeFrame(int id);

        void connectCore(int id);
        void disconnCore(int id);

        void updateLayouts();
        void startStream(bool stream);
        void stopStream(bool stream);

        static int getScreenFilter()     { return screenFilter;    }
        static int getMicEnable()        { return micEnable;       }
        static int getKeyBind(int index) { return keyBinds[index]; }

        static void setScreenFilter(int value)       { screenFilter    = value; }
        static void setMicEnable(int value)          { micEnable       = value; }
        static void setKeyBind(int index, int value) { keyBinds[index] = value; }

    private:
        NooFrame *frames[MAX_FRAMES] = {};
        PaStream *streams[2] = {};

        static int screenFilter;
        static int micEnable;
        static int keyBinds[MAX_KEYS];

        bool OnInit();
        int  OnExit();

        void update(wxTimerEvent &event);

        static int audioCallback(const void *in, void *out, unsigned long count,
            const PaStreamCallbackTimeInfo *info, PaStreamCallbackFlags flags, void *data);
        static int micCallback(const void *in, void *out, unsigned long count,
            const PaStreamCallbackTimeInfo *info, PaStreamCallbackFlags flags, void *data);

        wxDECLARE_EVENT_TABLE();
};

#endif // NOO_APP_H
