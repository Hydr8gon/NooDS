/*
    Copyright 2019-2025 Hydr8gon

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

#ifndef PATH_DIALOG_H
#define PATH_DIALOG_H

#include <wx/wx.h>

class PathDialog: public wxDialog
{
    public:
        PathDialog();

    private:
        wxTextCtrl *bios9Path;
        wxTextCtrl *bios7Path;
        wxTextCtrl *firmwarePath;
        wxTextCtrl *gbaBiosPath;
        wxTextCtrl *sdImagePath;
        wxCheckBox *boxes[3];

        void bios9Browse(wxCommandEvent &event);
        void bios7Browse(wxCommandEvent &event);
        void firmwareBrowse(wxCommandEvent &event);
        void gbaBiosBrowse(wxCommandEvent &event);
        void sdImageBrowse(wxCommandEvent &event);
        void openFolder(wxCommandEvent &event);
        void confirm(wxCommandEvent &event);

        wxDECLARE_EVENT_TABLE();
};

#endif // PATH_DIALOG_H
