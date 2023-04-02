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

#ifndef SAVE_DIALOG_H
#define SAVE_DIALOG_H

#include <wx/wx.h>

class NooFrame;
class Cartridge;

class SaveDialog: public wxDialog
{
    public:
        SaveDialog(NooFrame *frame);

    private:
        NooFrame *frame;
        Cartridge *cartridge;

        bool gba = false;
        int selection = 0;

        int selectionToSize(int selection);
        int sizeToSelection(int size);

        void selection0(wxCommandEvent &event);
        void selection1(wxCommandEvent &event);
        void selection2(wxCommandEvent &event);
        void selection3(wxCommandEvent &event);
        void selection4(wxCommandEvent &event);
        void selection5(wxCommandEvent &event);
        void selection6(wxCommandEvent &event);
        void selection7(wxCommandEvent &event);
        void selection8(wxCommandEvent &event);
        void selection9(wxCommandEvent &event);
        void confirm(wxCommandEvent &event);

        wxDECLARE_EVENT_TABLE();
};

#endif // SAVE_DIALOG_H
