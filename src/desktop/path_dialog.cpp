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
    SD_IMAGE_BROWSE,
    SAVES_FOLDER,
    STATES_FOLDER,
    CHEATS_FOLDER,
    OPEN_FOLDER
};

wxBEGIN_EVENT_TABLE(PathDialog, wxDialog)
EVT_BUTTON(BIOS9_BROWSE, PathDialog::bios9Browse)
EVT_BUTTON(BIOS7_BROWSE, PathDialog::bios7Browse)
EVT_BUTTON(FIRMWARE_BROWSE, PathDialog::firmwareBrowse)
EVT_BUTTON(GBA_BIOS_BROWSE, PathDialog::gbaBiosBrowse)
EVT_BUTTON(SD_IMAGE_BROWSE, PathDialog::sdImageBrowse)
EVT_BUTTON(OPEN_FOLDER, PathDialog::openFolder)
EVT_BUTTON(wxID_OK, PathDialog::confirm)
wxEND_EVENT_TABLE()

PathDialog::PathDialog(): wxDialog(nullptr, wxID_ANY, "Path Settings")
{
    // Use the height of a button as a unit to scale pixel values based on DPI/font
    wxButton *dummy = new wxButton(this, wxID_ANY, "");
    int size = dummy->GetSize().y;
    delete dummy;

    // Set up the ARM9 BIOS path setting
    wxBoxSizer *arm9Sizer = new wxBoxSizer(wxHORIZONTAL);
    arm9Sizer->Add(new wxStaticText(this, wxID_ANY, "ARM9 BIOS:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 8);
    bios9Path = new wxTextCtrl(this, wxID_ANY, Settings::bios9Path, wxDefaultPosition, wxSize(size * 8, size));
    arm9Sizer->Add(bios9Path);
    arm9Sizer->Add(new wxButton(this, BIOS9_BROWSE, "Browse"), 0, wxLEFT, size / 8);

    // Set up the ARM7 BIOS path setting
    wxBoxSizer *arm7Sizer = new wxBoxSizer(wxHORIZONTAL);
    arm7Sizer->Add(new wxStaticText(this, wxID_ANY, "ARM7 BIOS:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 8);
    bios7Path = new wxTextCtrl(this, wxID_ANY, Settings::bios7Path, wxDefaultPosition, wxSize(size * 8, size));
    arm7Sizer->Add(bios7Path);
    arm7Sizer->Add(new wxButton(this, BIOS7_BROWSE, "Browse"), 0, wxLEFT, size / 8);

    // Set up the firmware path setting
    wxBoxSizer *firmSizer = new wxBoxSizer(wxHORIZONTAL);
    firmSizer->Add(new wxStaticText(this, wxID_ANY, "Firmware:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 8);
    firmwarePath = new wxTextCtrl(this, wxID_ANY, Settings::firmwarePath, wxDefaultPosition, wxSize(size * 8, size));
    firmSizer->Add(firmwarePath);
    firmSizer->Add(new wxButton(this, FIRMWARE_BROWSE, "Browse"), 0, wxLEFT, size / 8);

    // Set up the GBA BIOS path setting
    wxBoxSizer *gbaSizer = new wxBoxSizer(wxHORIZONTAL);
    gbaSizer->Add(new wxStaticText(this, wxID_ANY, "GBA BIOS:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 8);
    gbaBiosPath = new wxTextCtrl(this, wxID_ANY, Settings::gbaBiosPath, wxDefaultPosition, wxSize(size * 8, size));
    gbaSizer->Add(gbaBiosPath);
    gbaSizer->Add(new wxButton(this, GBA_BIOS_BROWSE, "Browse"), 0, wxLEFT, size / 8);

    // Set up the SD image path setting
    wxBoxSizer *sdSizer = new wxBoxSizer(wxHORIZONTAL);
    sdSizer->Add(new wxStaticText(this, wxID_ANY, "SD Image:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 8);
    sdImagePath = new wxTextCtrl(this, wxID_ANY, Settings::sdImagePath, wxDefaultPosition, wxSize(size * 8, size));
    sdSizer->Add(sdImagePath);
    sdSizer->Add(new wxButton(this, SD_IMAGE_BROWSE, "Browse"), 0, wxLEFT, size / 8);

    // Set up the separate folder checkboxes
    wxBoxSizer *folderSizer = new wxBoxSizer(wxHORIZONTAL);
    folderSizer->Add(new wxStaticText(this, wxID_ANY, "Separate Folders For:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 8);
    folderSizer->Add(boxes[0] = new wxCheckBox(this, SAVES_FOLDER, "Saves"), 0, wxALIGN_CENTRE | wxLEFT, size / 8);
    folderSizer->Add(boxes[1] = new wxCheckBox(this, STATES_FOLDER, "States"), 0, wxALIGN_CENTRE | wxLEFT, size / 8);
    folderSizer->Add(boxes[2] = new wxCheckBox(this, CHEATS_FOLDER, "Cheats"), 0, wxALIGN_CENTRE | wxLEFT, size / 8);

    // Set the current values of the checkboxes
    boxes[0]->SetValue(Settings::savesFolder);
    boxes[1]->SetValue(Settings::statesFolder);
    boxes[2]->SetValue(Settings::cheatsFolder);

    // Set up the open folder, cancel, and confirm buttons
    wxBoxSizer *buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    buttonSizer->Add(new wxButton(this, OPEN_FOLDER, "Open Folder"));
    buttonSizer->Add(new wxStaticText(this, wxID_ANY, ""), 1);
    buttonSizer->Add(new wxButton(this, wxID_CANCEL, "Cancel"), 0, wxRIGHT, size / 16);
    buttonSizer->Add(new wxButton(this, wxID_OK, "Confirm"), 0, wxLEFT, size / 16);

    // Combine all of the contents
    wxBoxSizer *contents = new wxBoxSizer(wxVERTICAL);
    contents->Add(arm9Sizer, 1, wxEXPAND | wxALL, size / 8);
    contents->Add(arm7Sizer, 1, wxEXPAND | wxALL, size / 8);
    contents->Add(firmSizer, 1, wxEXPAND | wxALL, size / 8);
    contents->Add(gbaSizer, 1, wxEXPAND | wxALL, size / 8);
    contents->Add(sdSizer, 1, wxEXPAND | wxALL, size / 8);
    contents->Add(folderSizer, 1, wxEXPAND | wxALL, size / 8);
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
    wxFileDialog bios9Select(this, "Select ARM9 BIOS File", "", "",
        "Binary files (*.bin)|*.bin", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (bios9Select.ShowModal() == wxID_CANCEL) return;

    // Update the path
    bios9Path->Clear();
    *bios9Path << bios9Select.GetPath();
}

void PathDialog::bios7Browse(wxCommandEvent &event)
{
    // Show the file browser
    wxFileDialog bios7Select(this, "Select ARM7 BIOS File", "", "",
        "Binary files (*.bin)|*.bin", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (bios7Select.ShowModal() == wxID_CANCEL) return;

    // Update the path
    bios7Path->Clear();
    *bios7Path << bios7Select.GetPath();
}

void PathDialog::firmwareBrowse(wxCommandEvent &event)
{
    // Show the file browser
    wxFileDialog firmwareSelect(this, "Select Firmware File", "", "",
        "Binary files (*.bin)|*.bin", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (firmwareSelect.ShowModal() == wxID_CANCEL) return;

    // Update the path
    firmwarePath->Clear();
    *firmwarePath << firmwareSelect.GetPath();
}

void PathDialog::gbaBiosBrowse(wxCommandEvent &event)
{
    // Show the file browser
    wxFileDialog gbaBiosSelect(this, "Select GBA BIOS File", "", "",
        "Binary files (*.bin)|*.bin", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (gbaBiosSelect.ShowModal() == wxID_CANCEL) return;

    // Update the path
    gbaBiosPath->Clear();
    *gbaBiosPath << gbaBiosSelect.GetPath();
}

void PathDialog::sdImageBrowse(wxCommandEvent &event)
{
    // Show the file browser
    wxFileDialog sdImageSelect(this, "Select SD Image File", "", "",
        "Image files (*.img)|*.img", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (sdImageSelect.ShowModal() == wxID_CANCEL) return;

    // Update the path
    sdImagePath->Clear();
    *sdImagePath << sdImageSelect.GetPath();
}

void PathDialog::openFolder(wxCommandEvent &event)
{
    // Open the folder containing settings and other files
    wxLaunchDefaultApplication(Settings::basePath);
}

void PathDialog::confirm(wxCommandEvent &event)
{
    // Update and save the path settings
    Settings::bios9Path = bios9Path->GetValue().ToStdString();
    Settings::bios7Path = bios7Path->GetValue().ToStdString();
    Settings::firmwarePath = firmwarePath->GetValue().ToStdString();
    Settings::gbaBiosPath = gbaBiosPath->GetValue().ToStdString();
    Settings::sdImagePath = sdImagePath->GetValue().ToStdString();
    Settings::savesFolder = boxes[0]->GetValue();
    Settings::statesFolder = boxes[1]->GetValue();
    Settings::cheatsFolder = boxes[2]->GetValue();
    Settings::save();
    event.Skip(true);
}
