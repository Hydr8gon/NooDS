/*
    Copyright 2019-2026 Hydr8gon

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

#pragma once

#include <wx/wx.h>

class PathDialog: public wxDialog {
public:
    PathDialog();

private:
    wxTextCtrl *gbaBiosPath;
    wxTextCtrl *ndsBios9Path;
    wxTextCtrl *ndsBios7Path;
    wxTextCtrl *firmwarePath;
    wxTextCtrl *dsiBios9Path;
    wxTextCtrl *dsiBios7Path;
    wxTextCtrl *dsiNandPath;
    wxTextCtrl *sdImagePath;
    wxCheckBox *boxes[3];

    void gbaBiosBrowse(wxCommandEvent &event);
    void ndsBios9Browse(wxCommandEvent &event);
    void ndsBios7Browse(wxCommandEvent &event);
    void firmwareBrowse(wxCommandEvent &event);
    void dsiBios9Browse(wxCommandEvent &event);
    void dsiBios7Browse(wxCommandEvent &event);
    void dsiNandBrowse(wxCommandEvent &event);
    void sdImageBrowse(wxCommandEvent &event);
    void openFolder(wxCommandEvent &event);
    void confirm(wxCommandEvent &event);
    wxDECLARE_EVENT_TABLE();
};
