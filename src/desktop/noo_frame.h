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

#ifndef NOO_FRAME_H
#define NOO_FRAME_H

#include <wx/wx.h>
#include <wx/joystick.h>

#include "../core.h"

class NooApp;
class NooCanvas;

class NooFrame: public wxFrame
{
    public:
        NooFrame(NooApp *app, int id = 0, std::string path = "");

        void Refresh();

        void startCore(bool full);
        void stopCore(bool full);

        void pressKey(int key);
        void releaseKey(int key);

        Core *getCore()  { return core;    }
        bool isRunning() { return running; }

    private:
        NooApp *app;
        NooCanvas *canvas;
        wxMenu *fileMenu, *systemMenu;
        wxJoystick *joystick;
        wxTimer *timer;

        int id = 0;
        Core *core = nullptr;
        bool running = false;

        std::string ndsPath, gbaPath;
        std::thread *coreThread = nullptr, *saveThread = nullptr;
        std::condition_variable cond;
        std::mutex mutex;

        std::vector<int> axisBases;
        uint8_t hotkeyToggles = 0;
        int fpsLimiterBackup = 0;
        bool fastForward = false;
        bool fullScreen = false;

        void runCore();
        void checkSave();
        void loadRomPath(std::string path);

        void loadRom(wxCommandEvent &event);
        void bootFirmware(wxCommandEvent &event);
        void trimRom(wxCommandEvent &event);
        void changeSave(wxCommandEvent &event);
        void quit(wxCommandEvent &event);
        void pause(wxCommandEvent &event);
        void restart(wxCommandEvent &event);
        void stop(wxCommandEvent &event);
        void addSystem(wxCommandEvent &event);
        void pathSettings(wxCommandEvent &event);
        void inputSettings(wxCommandEvent &event);
        void layoutSettings(wxCommandEvent &event);
        void directBootToggle(wxCommandEvent &event);
        void fpsDisabled(wxCommandEvent &event);
        void fpsLight(wxCommandEvent &event);
        void fpsAccurate(wxCommandEvent &event);
        void threaded2D(wxCommandEvent &event);
        void threaded3D0(wxCommandEvent &event);
        void threaded3D1(wxCommandEvent &event);
        void threaded3D2(wxCommandEvent &event);
        void threaded3D3(wxCommandEvent &event);
        void micEnable(wxCommandEvent &event);
        void updateJoystick(wxTimerEvent &event);
        void dropFiles(wxDropFilesEvent &event);
        void close(wxCloseEvent &event);

        wxDECLARE_EVENT_TABLE();
};

#endif // NOO_FRAME_H
