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

#include "save_dialog.h"
#include "../core.h"

wxBEGIN_EVENT_TABLE(SaveDialog, wxDialog)
EVT_RADIOBUTTON(1, SaveDialog::eeprom05)
EVT_RADIOBUTTON(2, SaveDialog::eeprom8)
EVT_RADIOBUTTON(3, SaveDialog::eeprom64)
EVT_RADIOBUTTON(4, SaveDialog::fram32)
EVT_RADIOBUTTON(5, SaveDialog::flash256)
EVT_RADIOBUTTON(6, SaveDialog::flash512)
EVT_RADIOBUTTON(7, SaveDialog::flash1024)
EVT_RADIOBUTTON(8, SaveDialog::flash8192)
EVT_BUTTON(wxID_OK, SaveDialog::confirm)
wxEND_EVENT_TABLE()

SaveDialog::SaveDialog(std::string path): wxDialog(nullptr, wxID_ANY, "Select Save Type"), path(path)
{
    // Determine the height of a button
    // Borders are measured in pixels, so this value can be used to make values that scale with the DPI/font size
    wxButton *dummy = new wxButton(this, wxID_ANY, "");
    int size = dummy->GetSize().y;
    delete dummy;

    // Set up the radio buttons on the left side
    wxBoxSizer *leftRadio = new wxBoxSizer(wxVERTICAL);
    leftRadio->Add(new wxRadioButton(this, 1, "EEPROM 0.5KB"), 1);
    leftRadio->Add(new wxRadioButton(this, 2, "EEPROM 8KB"), 1);
    leftRadio->Add(new wxRadioButton(this, 3, "EEPROM 64KB"), 1);
    leftRadio->Add(new wxRadioButton(this, 4, "FRAM 32KB"), 1);

    // Set up the radio buttons on the right side
    wxBoxSizer *rightRadio = new wxBoxSizer(wxVERTICAL);
    rightRadio->Add(new wxRadioButton(this, 5, "FLASH 256KB"), 1);
    rightRadio->Add(new wxRadioButton(this, 6, "FLASH 512KB"), 1);
    rightRadio->Add(new wxRadioButton(this, 7, "FLASH 1024KB"), 1);
    rightRadio->Add(new wxRadioButton(this, 8, "FLASH 8192KB"), 1);

    // Combine all of the radio buttons
    wxBoxSizer *radioSizer = new wxBoxSizer(wxHORIZONTAL);
    radioSizer->Add(leftRadio, 1, wxEXPAND | wxRIGHT, size / 8);
    radioSizer->Add(rightRadio, 1, wxEXPAND | wxLEFT, size / 8);

    // Set up the cancel and confirm buttons
    wxBoxSizer *buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    buttonSizer->Add(new wxStaticText(this, wxID_ANY, ""), 1);
    buttonSizer->Add(new wxButton(this, wxID_CANCEL, "Cancel"), 0, wxRIGHT, size / 16);
    buttonSizer->Add(new wxButton(this, wxID_OK, "Confirm"), 0, wxLEFT, size / 16);

    // Combine all of the contents
    wxBoxSizer *contents = new wxBoxSizer(wxVERTICAL);
    contents->Add(radioSizer, 1, wxEXPAND | wxALL, size / 8);
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

void SaveDialog::eeprom05(wxCommandEvent &event)
{
    // Set the selection to EEPROM 0.5KB
    selection = 0;
}

void SaveDialog::eeprom8(wxCommandEvent &event)
{
    // Set the selection to EEPROM 8KB
    selection = 1;
}

void SaveDialog::eeprom64(wxCommandEvent &event)
{
    // Set the selection to EEPROM 64KB
    selection = 3;
}

void SaveDialog::fram32(wxCommandEvent &event)
{
    // Set the selection to FRAM 32KB
    selection = 2;
}

void SaveDialog::flash256(wxCommandEvent &event)
{
    // Set the selection to FLASH 256KB
    selection = 4;
}

void SaveDialog::flash512(wxCommandEvent &event)
{
    // Set the selection to FLASH 512KB
    selection = 5;
}

void SaveDialog::flash1024(wxCommandEvent &event)
{
    // Set the selection to FLASH 1024KB
    selection = 6;
}

void SaveDialog::flash8192(wxCommandEvent &event)
{
    // Set the selection to FLASH 8192KB
    selection = 7;
}

void SaveDialog::confirm(wxCommandEvent &event)
{
    // Create a save of the selected type
    Core::createSave(path, selection);

    event.Skip(true);
}
