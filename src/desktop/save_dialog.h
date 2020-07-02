/*
    Copyright 2020 Hydr8gon

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

#include <string>
#include <wx/wx.h>

class SaveDialog: public wxDialog
{
    public:
        SaveDialog(std::string path);

    private:
        std::string path;
        int selection = 0;

        void none(wxCommandEvent &event);
        void eeprom05(wxCommandEvent &event);
        void eeprom8(wxCommandEvent &event);
        void eeprom64(wxCommandEvent &event);
        void eeprom128(wxCommandEvent &event);
        void fram32(wxCommandEvent &event);
        void flash256(wxCommandEvent &event);
        void flash512(wxCommandEvent &event);
        void flash1024(wxCommandEvent &event);
        void flash8192(wxCommandEvent &event);
        void confirm(wxCommandEvent &event);

        wxDECLARE_EVENT_TABLE();
};

#endif // SAVE_DIALOG_H
