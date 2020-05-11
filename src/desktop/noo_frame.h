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

#ifndef NOO_FRAME_H
#define NOO_FRAME_H

#include <thread>
#include <wx/wx.h>

#include "../core.h"

struct Emulator
{
    Core *core = nullptr;
    bool running = false;
};

class NooFrame: public wxFrame
{
    public:
        NooFrame(Emulator *emulator, std::string path);

    private:
        std::thread *coreThread = nullptr;
        Emulator *emulator;
        std::string path;

        wxMenu *systemMenu;

        void runCore();
        void startCore();
        void stopCore();

        void loadRom(wxCommandEvent &event);
        void bootFirmware(wxCommandEvent &event);
        void pause(wxCommandEvent &event);
        void restart(wxCommandEvent &event);
        void stop(wxCommandEvent &event);
        void pathSettings(wxCommandEvent &event);
        void inputSettings(wxCommandEvent &event);
        void layoutSettings(wxCommandEvent &event);
        void directBootToggle(wxCommandEvent &event);
        void threaded3D0(wxCommandEvent &event);
        void threaded3D1(wxCommandEvent &event);
        void threaded3D2(wxCommandEvent &event);
        void threaded3D3(wxCommandEvent &event);
        void threaded3D4(wxCommandEvent &event);
        void limitFpsToggle(wxCommandEvent &event);
        void exit(wxCommandEvent &event);

        void close(wxCloseEvent &event);

        wxDECLARE_EVENT_TABLE();
};

#endif // NOO_FRAME_H
