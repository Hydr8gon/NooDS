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

#ifndef CHEAT_DIALOG_H
#define CHEAT_DIALOG_H

#include "noo_frame.h"

class CheatDialog: public wxDialog {
    public:
        CheatDialog(Core *core);

    private:
        Core *core;
        wxCheckListBox *cheatList;
        wxTextCtrl *nameEditor;
        wxTextCtrl *codeEditor;
        int curCheat = -1;

        void updateCheat();
        void checkCheat(wxCommandEvent &event);
        void selectCheat(wxCommandEvent &event);
        void addCheat(wxCommandEvent &event);
        void removeCheat(wxCommandEvent &event);
        void cancel(wxCommandEvent &event);
        void confirm(wxCommandEvent &event);

        wxDECLARE_EVENT_TABLE();
};

#endif // CHEAT_DIALOG_H
