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

#include "path_dialog.h"
#include "../settings.h"

wxBEGIN_EVENT_TABLE(PathDialog, wxDialog)
EVT_BUTTON(1, PathDialog::bios9Browse)
EVT_BUTTON(2, PathDialog::bios7Browse)
EVT_BUTTON(3, PathDialog::firmwareBrowse)
EVT_BUTTON(wxID_OK, PathDialog::confirm)
wxEND_EVENT_TABLE()

PathDialog::PathDialog(): wxDialog(nullptr, wxID_ANY, "Path Settings")
{
    // Determine the height of a button
    // Borders are measured in pixels, so this value can be used to make values that scale with the DPI/font size
    wxButton *dummy = new wxButton(this, wxID_ANY, "");
    int size = dummy->GetSize().y;
    delete dummy;

    // Set up the ARM9 BIOS path setting
    wxBoxSizer *arm9Sizer = new wxBoxSizer(wxHORIZONTAL);
    arm9Sizer->Add(new wxStaticText(this, wxID_ANY, "ARM9 BIOS:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 8);
    arm9Sizer->Add(bios9Path = new wxTextCtrl(this, wxID_ANY, Settings::getBios9Path(), wxDefaultPosition, wxSize(size * 6, size)), 0, wxALIGN_CENTRE);
    arm9Sizer->Add(new wxButton(this, 1, "Browse"), 0, wxLEFT, size / 8);

    // Set up the ARM7 BIOS path setting
    wxBoxSizer *arm7Sizer = new wxBoxSizer(wxHORIZONTAL);
    arm7Sizer->Add(new wxStaticText(this, wxID_ANY, "ARM7 BIOS:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 8);
    arm7Sizer->Add(bios7Path = new wxTextCtrl(this, wxID_ANY, Settings::getBios7Path(), wxDefaultPosition, wxSize(size * 6, size)), 0, wxALIGN_CENTRE);
    arm7Sizer->Add(new wxButton(this, 2, "Browse"), 0, wxLEFT, size / 8);

    // Set up the firmware path setting
    wxBoxSizer *firmSizer = new wxBoxSizer(wxHORIZONTAL);
    firmSizer->Add(new wxStaticText(this, wxID_ANY, "Firmware:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 8);
    firmSizer->Add(firmwarePath = new wxTextCtrl(this, wxID_ANY, Settings::getFirmwarePath(), wxDefaultPosition, wxSize(size * 6, size)), 0, wxALIGN_CENTRE);
    firmSizer->Add(new wxButton(this, 3, "Browse"), 0, wxLEFT, size / 8);

    // Set up the cancel and confirm buttons
    wxBoxSizer *buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    buttonSizer->Add(new wxStaticText(this, wxID_ANY, ""), 1);
    buttonSizer->Add(new wxButton(this, wxID_CANCEL, "Cancel"), 0, wxRIGHT, size / 16);
    buttonSizer->Add(new wxButton(this, wxID_OK, "Confirm"), 0, wxLEFT, size / 16);

    // Combine all of the contents
    wxBoxSizer *contents = new wxBoxSizer(wxVERTICAL);
    contents->Add(arm9Sizer, 1, wxEXPAND | wxALL, size / 8);
    contents->Add(arm7Sizer, 1, wxEXPAND | wxALL, size / 8);
    contents->Add(firmSizer, 1, wxEXPAND | wxALL, size / 8);
    contents->Add(buttonSizer, 1, wxEXPAND | wxALL, size / 8);

    // Add a final border around everything
    wxBoxSizer *sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(contents, 1, wxEXPAND | wxALL, size / 8);
    SetSizer(sizer);

    // Size the window to fit the contents and prevent resizing
    sizer->Fit(this);
    SetMinSize(GetSize());
    SetMaxSize(GetSize());
}

void PathDialog::bios9Browse(wxCommandEvent &event)
{
    // Show the file browser
    wxFileDialog bios9Select(this, "Select ARM9 BIOS File", "", "", "Binary files (*.bin)|*.bin", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (bios9Select.ShowModal() == wxID_CANCEL)
        return;

    // Update the path
    bios9Path->Clear();
    *bios9Path << bios9Select.GetPath();
}

void PathDialog::bios7Browse(wxCommandEvent &event)
{
    // Show the file browser
    wxFileDialog bios7Select(this, "Select ARM7 BIOS File", "", "", "Binary files (*.bin)|*.bin", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (bios7Select.ShowModal() == wxID_CANCEL)
        return;

    // Update the path
    bios7Path->Clear();
    *bios7Path << bios7Select.GetPath();
}

void PathDialog::firmwareBrowse(wxCommandEvent &event)
{
    // Show the file browser
    wxFileDialog firmwareSelect(this, "Select Firmware File", "", "", "Binary files (*.bin)|*.bin", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (firmwareSelect.ShowModal() == wxID_CANCEL)
        return;

    // Update the path
    firmwarePath->Clear();
    *firmwarePath << firmwareSelect.GetPath();
}

void PathDialog::confirm(wxCommandEvent &event)
{
    char path[1024];

    // Save the ARM9 BIOS path
    strncpy(path, (const char*)bios9Path->GetValue().mb_str(wxConvUTF8), 1023);
    Settings::setBios9Path(path);

    // Save the ARM7 BIOS path
    strncpy(path, (const char*)bios7Path->GetValue().mb_str(wxConvUTF8), 1023);
    Settings::setBios7Path(path);

    // Save the firmware path
    strncpy(path, (const char*)firmwarePath->GetValue().mb_str(wxConvUTF8), 1023);
    Settings::setFirmwarePath(path);

    event.Skip(true);
}
