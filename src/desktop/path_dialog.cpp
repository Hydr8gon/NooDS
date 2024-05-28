/*
    Copyright 2019-2024 Hydr8gon

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

enum PathEvent
{
    BIOS9_BROWSE = 1,
    BIOS7_BROWSE,
    FIRMWARE_BROWSE,
    GBA_BIOS_BROWSE,
    SD_IMAGE_BROWSE
};

wxBEGIN_EVENT_TABLE(PathDialog, wxDialog)
EVT_BUTTON(BIOS9_BROWSE,    PathDialog::bios9Browse)
EVT_BUTTON(BIOS7_BROWSE,    PathDialog::bios7Browse)
EVT_BUTTON(FIRMWARE_BROWSE, PathDialog::firmwareBrowse)
EVT_BUTTON(GBA_BIOS_BROWSE, PathDialog::gbaBiosBrowse)
EVT_BUTTON(SD_IMAGE_BROWSE, PathDialog::sdImageBrowse)
EVT_BUTTON(wxID_OK,         PathDialog::confirm)
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
    arm9Sizer->Add(bios9Path = new wxTextCtrl(this, wxID_ANY, Settings::bios9Path, wxDefaultPosition, wxSize(size * 6, size)), 0, wxALIGN_CENTRE);
    arm9Sizer->Add(new wxButton(this, BIOS9_BROWSE, "Browse"), 0, wxLEFT, size / 8);

    // Set up the ARM7 BIOS path setting
    wxBoxSizer *arm7Sizer = new wxBoxSizer(wxHORIZONTAL);
    arm7Sizer->Add(new wxStaticText(this, wxID_ANY, "ARM7 BIOS:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 8);
    arm7Sizer->Add(bios7Path = new wxTextCtrl(this, wxID_ANY, Settings::bios7Path, wxDefaultPosition, wxSize(size * 6, size)), 0, wxALIGN_CENTRE);
    arm7Sizer->Add(new wxButton(this, BIOS7_BROWSE, "Browse"), 0, wxLEFT, size / 8);

    // Set up the firmware path setting
    wxBoxSizer *firmSizer = new wxBoxSizer(wxHORIZONTAL);
    firmSizer->Add(new wxStaticText(this, wxID_ANY, "Firmware:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 8);
    firmSizer->Add(firmwarePath = new wxTextCtrl(this, wxID_ANY, Settings::firmwarePath, wxDefaultPosition, wxSize(size * 6, size)), 0, wxALIGN_CENTRE);
    firmSizer->Add(new wxButton(this, FIRMWARE_BROWSE, "Browse"), 0, wxLEFT, size / 8);

    // Set up the GBA BIOS path setting
    wxBoxSizer *gbaSizer = new wxBoxSizer(wxHORIZONTAL);
    gbaSizer->Add(new wxStaticText(this, wxID_ANY, "GBA BIOS:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 8);
    gbaSizer->Add(gbaBiosPath = new wxTextCtrl(this, wxID_ANY, Settings::gbaBiosPath, wxDefaultPosition, wxSize(size * 6, size)), 0, wxALIGN_CENTRE);
    gbaSizer->Add(new wxButton(this, GBA_BIOS_BROWSE, "Browse"), 0, wxLEFT, size / 8);

    // Set up the SD image path setting
    wxBoxSizer *sdSizer = new wxBoxSizer(wxHORIZONTAL);
    sdSizer->Add(new wxStaticText(this, wxID_ANY, "SD Image:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 8);
    sdSizer->Add(sdImagePath = new wxTextCtrl(this, wxID_ANY, Settings::sdImagePath, wxDefaultPosition, wxSize(size * 6, size)), 0, wxALIGN_CENTRE);
    sdSizer->Add(new wxButton(this, SD_IMAGE_BROWSE, "Browse"), 0, wxLEFT, size / 8);

    // Set up the cancel and confirm buttons
    wxBoxSizer *buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    buttonSizer->Add(new wxStaticText(this, wxID_ANY, ""), 1);
    buttonSizer->Add(new wxButton(this, wxID_CANCEL, "Cancel"),  0, wxRIGHT, size / 16);
    buttonSizer->Add(new wxButton(this, wxID_OK,     "Confirm"), 0, wxLEFT,  size / 16);

    // Combine all of the contents
    wxBoxSizer *contents = new wxBoxSizer(wxVERTICAL);
    contents->Add(arm9Sizer,   1, wxEXPAND | wxALL, size / 8);
    contents->Add(arm7Sizer,   1, wxEXPAND | wxALL, size / 8);
    contents->Add(firmSizer,   1, wxEXPAND | wxALL, size / 8);
    contents->Add(gbaSizer,    1, wxEXPAND | wxALL, size / 8);
    contents->Add(sdSizer,     1, wxEXPAND | wxALL, size / 8);
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

void PathDialog::gbaBiosBrowse(wxCommandEvent &event)
{
    // Show the file browser
    wxFileDialog gbaBiosSelect(this, "Select GBA BIOS File", "", "", "Binary files (*.bin)|*.bin", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (gbaBiosSelect.ShowModal() == wxID_CANCEL)
        return;

    // Update the path
    gbaBiosPath->Clear();
    *gbaBiosPath << gbaBiosSelect.GetPath();
}

void PathDialog::sdImageBrowse(wxCommandEvent &event)
{
    // Show the file browser
    wxFileDialog sdImageSelect(this, "Select SD Image File", "", "", "Image files (*.img)|*.img", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (sdImageSelect.ShowModal() == wxID_CANCEL)
        return;

    // Update the path
    sdImagePath->Clear();
    *sdImagePath << sdImageSelect.GetPath();
}

void PathDialog::confirm(wxCommandEvent &event)
{
    std::string path;

    // Save the ARM9 BIOS path
    path = (const char*)bios9Path->GetValue().mb_str(wxConvUTF8);
    Settings::bios9Path = path;

    // Save the ARM7 BIOS path
    path = (const char*)bios7Path->GetValue().mb_str(wxConvUTF8);
    Settings::bios7Path = path;

    // Save the firmware path
    path = (const char*)firmwarePath->GetValue().mb_str(wxConvUTF8);
    Settings::firmwarePath = path;

    // Save the GBA BIOS path
    path = (const char*)gbaBiosPath->GetValue().mb_str(wxConvUTF8);
    Settings::gbaBiosPath = path;

    // Save the SD image path
    path = (const char*)sdImagePath->GetValue().mb_str(wxConvUTF8);
    Settings::sdImagePath = path;

    Settings::save();

    event.Skip(true);
}
