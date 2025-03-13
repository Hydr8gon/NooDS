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

#include "cheat_dialog.h"

enum CheatEvent
{
    ADD_CHEAT = 1,
    REMOVE_CHEAT
};

wxBEGIN_EVENT_TABLE(CheatDialog, wxDialog)
EVT_CHECKLISTBOX(wxID_ANY, CheatDialog::checkCheat)
EVT_LISTBOX(wxID_ANY, CheatDialog::selectCheat)
EVT_BUTTON(ADD_CHEAT, CheatDialog::addCheat)
EVT_BUTTON(REMOVE_CHEAT, CheatDialog::removeCheat)
EVT_BUTTON(wxID_CANCEL, CheatDialog::cancel)
EVT_BUTTON(wxID_OK, CheatDialog::confirm)
wxEND_EVENT_TABLE()

CheatDialog::CheatDialog(Core *core): wxDialog(nullptr, wxID_ANY, "Action Replay Cheats"), core(core)
{
    // Use the height of a button as a unit to scale pixel values based on DPI/font
    wxButton *dummy = new wxButton(this, wxID_ANY, "");
    int size = dummy->GetSize().y;
    delete dummy;

    // Set up the cheat name and code editors
    wxBoxSizer *editSizer = new wxBoxSizer(wxVERTICAL);
    nameEditor = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(size * 8, size));
    editSizer->Add(nameEditor, 0, wxEXPAND | wxBOTTOM, size / 16);
    codeEditor = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE);
    editSizer->Add(codeEditor, 1, wxEXPAND | wxTOP, size / 16);

    // Set up the cheat list and combine it with the editors
    wxBoxSizer *cheatSizer = new wxBoxSizer(wxHORIZONTAL);
    cheatList = new wxCheckListBox();
    cheatList->Create(this, wxID_ANY, wxDefaultPosition, wxSize(size * 8, size * 12));
    cheatSizer->Add(cheatList, 1, wxEXPAND | wxRIGHT, size / 16);
    cheatSizer->Add(editSizer, 1, wxEXPAND | wxLEFT, size / 16);

    // Disable editors and populate the cheat list
    nameEditor->Disable();
    codeEditor->Disable();
    std::vector<ARCheat> &cheats = core->actionReplay.cheats;
    for (uint32_t i = 0; i < cheats.size(); i++)
    {
        cheatList->Append(cheats[i].name);
        cheatList->Check(i, cheats[i].enabled);
    }

    // Set up the add, remove, cancel, and confirm buttons
    wxBoxSizer *buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    buttonSizer->Add(new wxButton(this, ADD_CHEAT, "Add"), 0, wxRIGHT, size / 16);
    buttonSizer->Add(new wxButton(this, REMOVE_CHEAT, "Remove"), 0, wxLEFT, size / 16);
    buttonSizer->Add(new wxStaticText(this, wxID_ANY, ""), 1);
    buttonSizer->Add(new wxButton(this, wxID_CANCEL, "Cancel"), 0, wxRIGHT, size / 16);
    buttonSizer->Add(new wxButton(this, wxID_OK, "Confirm"), 0, wxLEFT, size / 16);

    // Combine all of the contents
    wxBoxSizer *contents = new wxBoxSizer(wxVERTICAL);
    contents->Add(cheatSizer, 1, wxEXPAND | wxBOTTOM, size / 16);
    contents->Add(buttonSizer, 0, wxEXPAND | wxTOP, size / 16);

    // Add a final border around everything
    wxBoxSizer *sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(contents, 1, wxEXPAND | wxALL, size / 8);
    SetSizer(sizer);

    // Size the window to fit the contents and prevent resizing
    sizer->Fit(this);
    SetMinSize(GetSize());
    SetMaxSize(GetSize());
}

void CheatDialog::updateCheat()
{
    // Get the current cheat and update its name
    ARCheat *cheat = &core->actionReplay.cheats[curCheat];
    cheatList->SetString(curCheat, cheat->name = nameEditor->GetValue());
    cheat->code.clear();

    // Parse the cheat's code from the editor string
    std::string code = codeEditor->GetValue().ToStdString();
    for (size_t i = 0, p; i < code.size(); i = p + 1)
    {
        p = code.find_first_of(" \n", i);
        if (p == std::string::npos) p = code.size();
        cheat->code.push_back(strtoll(code.substr(i, p).c_str(), nullptr, 16));
    }

    // Ensure the code's word count is a multiple of 2
    if (cheat->code.size() & 0x1)
        cheat->code.push_back(0);
}

void CheatDialog::checkCheat(wxCommandEvent &event)
{
    // Enable or disable a cheat
    ARCheat &cheat = core->actionReplay.cheats[event.GetInt()];
    cheat.enabled = !cheat.enabled;
}

void CheatDialog::selectCheat(wxCommandEvent &event)
{
    // Select a new cheat and put its name in the editor
    if (curCheat >= 0) updateCheat();
    ARCheat *cheat = &core->actionReplay.cheats[curCheat = event.GetInt()];
    nameEditor->Clear();
    nameEditor->AppendText(cheat->name);

    // Write the code to the editor as a string
    codeEditor->Clear();
    for (uint32_t i = 0; i < cheat->code.size(); i += 2)
    {
        char line[19];
        sprintf(line, "%08X %08X\n", cheat->code[i], cheat->code[i + 1]);
        codeEditor->AppendText(line);
    }

    // Enable the cheat editors
    nameEditor->Enable();
    codeEditor->Enable();
}

void CheatDialog::addCheat(wxCommandEvent &event)
{
    // Create a new cheat
    ARCheat cheat;
    cheatList->Append(cheat.name = "New Cheat");
    cheat.enabled = false;
    core->actionReplay.cheats.push_back(cheat);
}

void CheatDialog::removeCheat(wxCommandEvent &event)
{
    // Remove a cheat and reset the list
    if (curCheat < 0) return;
    std::vector<ARCheat> &cheats = core->actionReplay.cheats;
    cheats.erase(cheats.begin() + curCheat);
    cheatList->Clear();

    // Repopulate the cheat list
    for (uint32_t i = 0; i < cheats.size(); i++)
    {
        cheatList->Append(cheats[i].name);
        cheatList->Check(i, cheats[i].enabled);
    }

    // Reset and disable the editors
    curCheat = -1;
    nameEditor->Clear();
    codeEditor->Clear();
    nameEditor->Disable();
    codeEditor->Disable();
}

void CheatDialog::cancel(wxCommandEvent &event)
{
    // Reload cheats to discard changes
    core->actionReplay.loadCheats();
    event.Skip(true);
}

void CheatDialog::confirm(wxCommandEvent &event)
{
    // Update the current cheat and save changes
    if (curCheat >= 0) updateCheat();
    core->actionReplay.saveCheats();
    event.Skip(true);
}
