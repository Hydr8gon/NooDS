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

#include "path_dialog.h"
#include "../../core/settings.h"

enum PathEvent {
    GBA_BIOS_BROWSE = 1,
    NDS_BIOS9_BROWSE,
    NDS_BIOS7_BROWSE,
    FIRMWARE_BROWSE,
    DSI_BIOS9_BROWSE,
    DSI_BIOS7_BROWSE,
    DSI_NAND_BROWSE,
    SD_IMAGE_BROWSE,
    SAVES_FOLDER,
    STATES_FOLDER,
    CHEATS_FOLDER,
    OPEN_FOLDER
};

wxBEGIN_EVENT_TABLE(PathDialog, wxDialog)
EVT_BUTTON(GBA_BIOS_BROWSE, PathDialog::gbaBiosBrowse)
EVT_BUTTON(NDS_BIOS9_BROWSE, PathDialog::ndsBios9Browse)
EVT_BUTTON(NDS_BIOS7_BROWSE, PathDialog::ndsBios7Browse)
EVT_BUTTON(FIRMWARE_BROWSE, PathDialog::firmwareBrowse)
EVT_BUTTON(DSI_BIOS9_BROWSE, PathDialog::dsiBios9Browse)
EVT_BUTTON(DSI_BIOS7_BROWSE, PathDialog::dsiBios7Browse)
EVT_BUTTON(DSI_NAND_BROWSE, PathDialog::dsiNandBrowse)
EVT_BUTTON(SD_IMAGE_BROWSE, PathDialog::sdImageBrowse)
EVT_BUTTON(OPEN_FOLDER, PathDialog::openFolder)
EVT_BUTTON(wxID_OK, PathDialog::confirm)
wxEND_EVENT_TABLE()

PathDialog::PathDialog(): wxDialog(nullptr, wxID_ANY, "Path Settings") {
    // Use the height of a button as a unit to scale pixel values based on DPI/font
    wxButton *dummy = new wxButton(this, wxID_ANY, "");
    int size = dummy->GetSize().y;
    delete dummy;

    // Set up the GBA BIOS path setting
    wxBoxSizer *gbaSizer = new wxBoxSizer(wxHORIZONTAL);
    gbaSizer->Add(new wxStaticText(this, wxID_ANY, "GBA BIOS:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 8);
    gbaBiosPath = new wxTextCtrl(this, wxID_ANY, Settings::gbaBiosPath, wxDefaultPosition, wxSize(size * 10, size));
    gbaSizer->Add(gbaBiosPath);
    gbaSizer->Add(new wxButton(this, GBA_BIOS_BROWSE, "Browse"), 0, wxLEFT, size / 8);

    // Set up the NDS BIOS9 path setting
    wxBoxSizer *nds9Sizer = new wxBoxSizer(wxHORIZONTAL);
    nds9Sizer->Add(new wxStaticText(this, wxID_ANY, "NDS BIOS9:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 8);
    ndsBios9Path = new wxTextCtrl(this, wxID_ANY, Settings::ndsBios9Path, wxDefaultPosition, wxSize(size * 10, size));
    nds9Sizer->Add(ndsBios9Path);
    nds9Sizer->Add(new wxButton(this, NDS_BIOS9_BROWSE, "Browse"), 0, wxLEFT, size / 8);

    // Set up the NDS BIOS7 path setting
    wxBoxSizer *nds7Sizer = new wxBoxSizer(wxHORIZONTAL);
    nds7Sizer->Add(new wxStaticText(this, wxID_ANY, "NDS BIOS7:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 8);
    ndsBios7Path = new wxTextCtrl(this, wxID_ANY, Settings::ndsBios7Path, wxDefaultPosition, wxSize(size * 10, size));
    nds7Sizer->Add(ndsBios7Path);
    nds7Sizer->Add(new wxButton(this, NDS_BIOS7_BROWSE, "Browse"), 0, wxLEFT, size / 8);

    // Set up the firmware path setting
    wxBoxSizer *firmSizer = new wxBoxSizer(wxHORIZONTAL);
    firmSizer->Add(new wxStaticText(this, wxID_ANY, "Firmware:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 8);
    firmwarePath = new wxTextCtrl(this, wxID_ANY, Settings::firmwarePath, wxDefaultPosition, wxSize(size * 10, size));
    firmSizer->Add(firmwarePath);
    firmSizer->Add(new wxButton(this, FIRMWARE_BROWSE, "Browse"), 0, wxLEFT, size / 8);

    // Set up the DSi BIOS9 path setting
    wxBoxSizer *dsi9Sizer = new wxBoxSizer(wxHORIZONTAL);
    dsi9Sizer->Add(new wxStaticText(this, wxID_ANY, "DSi BIOS9:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 8);
    dsiBios9Path = new wxTextCtrl(this, wxID_ANY, Settings::dsiBios9Path, wxDefaultPosition, wxSize(size * 10, size));
    dsi9Sizer->Add(dsiBios9Path);
    dsi9Sizer->Add(new wxButton(this, DSI_BIOS9_BROWSE, "Browse"), 0, wxLEFT, size / 8);

    // Set up the DSi BIOS7 path setting
    wxBoxSizer *dsi7Sizer = new wxBoxSizer(wxHORIZONTAL);
    dsi7Sizer->Add(new wxStaticText(this, wxID_ANY, "DSi BIOS7:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 8);
    dsiBios7Path = new wxTextCtrl(this, wxID_ANY, Settings::dsiBios7Path, wxDefaultPosition, wxSize(size * 10, size));
    dsi7Sizer->Add(dsiBios7Path);
    dsi7Sizer->Add(new wxButton(this, DSI_BIOS7_BROWSE, "Browse"), 0, wxLEFT, size / 8);

    // Set up the DSi NAND path setting
    wxBoxSizer *nandSizer = new wxBoxSizer(wxHORIZONTAL);
    nandSizer->Add(new wxStaticText(this, wxID_ANY, "DSi NAND:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 8);
    dsiNandPath = new wxTextCtrl(this, wxID_ANY, Settings::dsiNandPath, wxDefaultPosition, wxSize(size * 10, size));
    nandSizer->Add(dsiNandPath);
    nandSizer->Add(new wxButton(this, DSI_NAND_BROWSE, "Browse"), 0, wxLEFT, size / 8);

    // Set up the SD image path setting
    wxBoxSizer *sdSizer = new wxBoxSizer(wxHORIZONTAL);
    sdSizer->Add(new wxStaticText(this, wxID_ANY, "SD Image:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 8);
    sdImagePath = new wxTextCtrl(this, wxID_ANY, Settings::sdImagePath, wxDefaultPosition, wxSize(size * 10, size));
    sdSizer->Add(sdImagePath);
    sdSizer->Add(new wxButton(this, SD_IMAGE_BROWSE, "Browse"), 0, wxLEFT, size / 8);

    // Set up the separate folder checkboxes
    wxBoxSizer *folderSizer = new wxBoxSizer(wxHORIZONTAL);
    folderSizer->Add(new wxStaticText(this, wxID_ANY, "Separate Folders For:"), 0, wxALIGN_CENTRE | wxRIGHT, size / 8);
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

    // Combine all the contents with added labels
    wxBoxSizer *contents = new wxBoxSizer(wxVERTICAL);
    wxStaticText *gbaText = new wxStaticText(this, wxID_ANY, "");
    gbaText->SetLabelMarkup("<b>GBA Files</b>\n"
        "Optional, but can improve compatibility and accuracy.");
    contents->Add(gbaText, 0, wxEXPAND | wxALL, size / 8);
    contents->Add(gbaSizer, 0, wxEXPAND | wxALL, size / 8);
    wxStaticText *ndsText = new wxStaticText(this, wxID_ANY, "");
    ndsText->SetLabelMarkup("<b>NDS Files</b>\n"
        "Optional, but can improve compatibility and accuracy.\n"
        "Required for non-direct boots.");
    contents->Add(ndsText, 0, wxEXPAND | wxALL, size / 8);
    contents->Add(nds9Sizer, 0, wxEXPAND | wxALL, size / 8);
    contents->Add(nds7Sizer, 0, wxEXPAND | wxALL, size / 8);
    contents->Add(firmSizer, 0, wxEXPAND | wxALL, size / 8);
    wxStaticText *dsiText = new wxStaticText(this, wxID_ANY, "");
    dsiText->SetLabelMarkup("<b>DSi Files</b>\n"
        "Required for the experimental DSi mode.");
    contents->Add(dsiText, 0, wxEXPAND | wxALL, size / 8);
    contents->Add(dsi9Sizer, 0, wxEXPAND | wxALL, size / 8);
    contents->Add(dsi7Sizer, 0, wxEXPAND | wxALL, size / 8);
    contents->Add(nandSizer, 0, wxEXPAND | wxALL, size / 8);
    wxStaticText *stgText = new wxStaticText(this, wxID_ANY, "");
    stgText->SetLabelMarkup("<b>Storage Files</b>\n"
        "Required for some homebrew and DSi software.");
    contents->Add(stgText, 0, wxEXPAND | wxALL, size / 8);
    contents->Add(sdSizer, 0, wxEXPAND | wxALL, size / 8);
    contents->Add(folderSizer, 0, wxEXPAND | wxALL, size / 4);
    contents->Add(buttonSizer, 0, wxEXPAND | wxALL, size / 8);

    // Add a final border around everything
    wxBoxSizer *sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(contents, 1, wxEXPAND | wxALL, size / 8);
    SetSizer(sizer);

    // Size the window to fit the contents and prevent resizing
    sizer->Fit(this);
    SetMinSize(GetSize());
    SetMaxSize(GetSize());
}

void PathDialog::gbaBiosBrowse(wxCommandEvent &event) {
    // Show the file browser
    wxFileDialog gbaBiosSelect(this, "Select GBA BIOS File", "", "",
        "Binary files (*.bin)|*.bin", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (gbaBiosSelect.ShowModal() == wxID_CANCEL) return;

    // Update the path
    gbaBiosPath->Clear();
    *gbaBiosPath << gbaBiosSelect.GetPath();
}

void PathDialog::ndsBios9Browse(wxCommandEvent &event) {
    // Show the file browser
    wxFileDialog bios9Select(this, "Select NDS BIOS9 File", "", "",
        "Binary files (*.bin)|*.bin", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (bios9Select.ShowModal() == wxID_CANCEL) return;

    // Update the path
    ndsBios9Path->Clear();
    *ndsBios9Path << bios9Select.GetPath();
}

void PathDialog::ndsBios7Browse(wxCommandEvent &event) {
    // Show the file browser
    wxFileDialog bios7Select(this, "Select NDS BIOS7 File", "", "",
        "Binary files (*.bin)|*.bin", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (bios7Select.ShowModal() == wxID_CANCEL) return;

    // Update the path
    ndsBios7Path->Clear();
    *ndsBios7Path << bios7Select.GetPath();
}

void PathDialog::firmwareBrowse(wxCommandEvent &event) {
    // Show the file browser
    wxFileDialog firmwareSelect(this, "Select Firmware File", "", "",
        "Binary files (*.bin)|*.bin", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (firmwareSelect.ShowModal() == wxID_CANCEL) return;

    // Update the path
    firmwarePath->Clear();
    *firmwarePath << firmwareSelect.GetPath();
}

void PathDialog::dsiBios9Browse(wxCommandEvent &event) {
    // Show the file browser
    wxFileDialog bios9Select(this, "Select DSi BIOS9 File", "", "",
        "Binary files (*.bin)|*.bin", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (bios9Select.ShowModal() == wxID_CANCEL) return;

    // Update the path
    dsiBios9Path->Clear();
    *dsiBios9Path << bios9Select.GetPath();
}

void PathDialog::dsiBios7Browse(wxCommandEvent &event) {
    // Show the file browser
    wxFileDialog bios7Select(this, "Select DSi BIOS7 File", "", "",
        "Binary files (*.bin)|*.bin", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (bios7Select.ShowModal() == wxID_CANCEL) return;

    // Update the path
    dsiBios7Path->Clear();
    *dsiBios7Path << bios7Select.GetPath();
}

void PathDialog::dsiNandBrowse(wxCommandEvent &event) {
    // Show the file browser
    wxFileDialog nandSelect(this, "Select DSi NAND File", "", "",
        "Binary files (*.bin)|*.bin", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (nandSelect.ShowModal() == wxID_CANCEL) return;

    // Update the path
    dsiNandPath->Clear();
    *dsiNandPath << nandSelect.GetPath();
}

void PathDialog::sdImageBrowse(wxCommandEvent &event) {
    // Show the file browser
    wxFileDialog sdImageSelect(this, "Select SD Image File", "", "",
        "Image files (*.img)|*.img", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (sdImageSelect.ShowModal() == wxID_CANCEL) return;

    // Update the path
    sdImagePath->Clear();
    *sdImagePath << sdImageSelect.GetPath();
}

void PathDialog::openFolder(wxCommandEvent &event) {
    // Open the folder containing settings and other files
    wxLaunchDefaultApplication(Settings::basePath);
}

void PathDialog::confirm(wxCommandEvent &event) {
    // Update and save the path settings
    Settings::gbaBiosPath = gbaBiosPath->GetLineText(0).ToStdString();
    Settings::ndsBios9Path = ndsBios9Path->GetLineText(0).ToStdString();
    Settings::ndsBios7Path = ndsBios7Path->GetLineText(0).ToStdString();
    Settings::firmwarePath = firmwarePath->GetLineText(0).ToStdString();
    Settings::dsiBios9Path = dsiBios9Path->GetLineText(0).ToStdString();
    Settings::dsiBios7Path = dsiBios7Path->GetLineText(0).ToStdString();
    Settings::dsiNandPath = dsiNandPath->GetLineText(0).ToStdString();
    Settings::sdImagePath = sdImagePath->GetLineText(0).ToStdString();
    Settings::savesFolder = boxes[0]->GetValue();
    Settings::statesFolder = boxes[1]->GetValue();
    Settings::cheatsFolder = boxes[2]->GetValue();
    Settings::save();
    event.Skip(true);
}
