/*
    Copyright 2019-2024 Hydr8gon

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
        static int micEnable;
        static int keyBinds[MAX_KEYS];

        void createFrame();
        void removeFrame(int id);

        void connectCore(int id);
        void disconnCore(int id);

        void updateLayouts();
        void startStream(bool stream);
        void stopStream(bool stream);

    private:
        NooFrame *frames[MAX_FRAMES] = {};
        PaStream *streams[2] = {};

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
