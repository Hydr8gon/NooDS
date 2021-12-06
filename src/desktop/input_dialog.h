/*
    Copyright 2019-2021 Hydr8gon

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

#ifndef INPUT_DIALOG_H
#define INPUT_DIALOG_H

#include <vector>
#include <wx/wx.h>
#include <wx/joystick.h>

class InputDialog: public wxDialog
{
    public:
        InputDialog(wxJoystick *joystick);
        ~InputDialog();

    private:
        wxJoystick *joystick;
        wxTimer *timer;

        wxButton *keyA;
        wxButton *keyB;
        wxButton *keyX;
        wxButton *keyY;
        wxButton *keyStart;
        wxButton *keySelect;
        wxButton *keyUp;
        wxButton *keyDown;
        wxButton *keyLeft;
        wxButton *keyRight;
        wxButton *keyL;
        wxButton *keyR;
        wxButton *keyFastForward;
        wxButton *keyFullScreen;

        int keyBinds[14];
        std::vector<int> axisBases;
        wxButton *current = nullptr;
        int keyIndex = 0;

        std::string keyToString(int key);

        void resetLabels();

        void remapA(wxCommandEvent &event);
        void remapB(wxCommandEvent &event);
        void remapX(wxCommandEvent &event);
        void remapY(wxCommandEvent &event);
        void remapStart(wxCommandEvent &event);
        void remapSelect(wxCommandEvent &event);
        void remapUp(wxCommandEvent &event);
        void remapDown(wxCommandEvent &event);
        void remapLeft(wxCommandEvent &event);
        void remapRight(wxCommandEvent &event);
        void remapL(wxCommandEvent &event);
        void remapR(wxCommandEvent &event);
        void remapFastForward(wxCommandEvent &event);
        void remapFullScreen(wxCommandEvent &event);
        void updateJoystick(wxTimerEvent &event);
        void confirm(wxCommandEvent &event);
        void pressKey(wxKeyEvent &event);

        wxDECLARE_EVENT_TABLE();
};

#endif // INPUT_DIALOG_H
