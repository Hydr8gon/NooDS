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

#include "save_dialog.h"
#include "noo_frame.h"

enum SaveEvent
{
    SELECTION_0 = 1,
    SELECTION_1,
    SELECTION_2,
    SELECTION_3,
    SELECTION_4,
    SELECTION_5,
    SELECTION_6,
    SELECTION_7,
    SELECTION_8,
    SELECTION_9
};

wxBEGIN_EVENT_TABLE(SaveDialog, wxDialog)
EVT_RADIOBUTTON(SELECTION_0, SaveDialog::selection0)
EVT_RADIOBUTTON(SELECTION_1, SaveDialog::selection1)
EVT_RADIOBUTTON(SELECTION_2, SaveDialog::selection2)
EVT_RADIOBUTTON(SELECTION_3, SaveDialog::selection3)
EVT_RADIOBUTTON(SELECTION_4, SaveDialog::selection4)
EVT_RADIOBUTTON(SELECTION_5, SaveDialog::selection5)
EVT_RADIOBUTTON(SELECTION_6, SaveDialog::selection6)
EVT_RADIOBUTTON(SELECTION_7, SaveDialog::selection7)
EVT_RADIOBUTTON(SELECTION_8, SaveDialog::selection8)
EVT_RADIOBUTTON(SELECTION_9, SaveDialog::selection9)
EVT_BUTTON(wxID_OK, SaveDialog::confirm)
wxEND_EVENT_TABLE()

int SaveDialog::selectionToSize(int selection)
{
    switch (selection)
    {
        case 1: return 0x200; // 0.5KB
        case 2: return 0x2000; // 8KB
        case 3: return 0x8000; // 32KB
        case 4: return 0x10000; // 64KB
        case 5: return 0x20000; // 128KB
        case 6: return 0x40000; // 256KB
        case 7: return 0x80000; // 512KB
        case 8: return 0x100000; // 1024KB
        case 9: return 0x800000; // 8192KB
        default: return 0; // None
    }
}

int SaveDialog::sizeToSelection(int size)
{
    switch (size)
    {
        case 0x200: return 1; // 0.5KB
        case 0x2000: return 2; // 8KB
        case 0x8000: return 3; // 32KB
        case 0x10000: return 4; // 64KB
        case 0x20000: return 5; // 128KB
        case 0x40000: return 6; // 256KB
        case 0x80000: return 7; // 512KB
        case 0x100000: return 8; // 1024KB
        case 0x800000: return 9; // 8192KB
        default: return 0; // None
    }
}

SaveDialog::SaveDialog(NooFrame *frame): wxDialog(nullptr, wxID_ANY, "Change Save Type"), frame(frame)
{
    // Check the current emulation mode and get the corresponding cartridge
    gba = frame->core->gbaMode;
    cartridge = gba ? (Cartridge*)&frame->core->cartridgeGba : (Cartridge*)&frame->core->cartridgeNds;

    // Determine the height of a button
    // Borders are measured in pixels, so this value can be used to make values that scale with the DPI/font size
    wxButton *dummy = new wxButton(this, wxID_ANY, "");
    int size = dummy->GetSize().y;
    delete dummy;

    wxBoxSizer *leftRadio = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer *rightRadio = new wxBoxSizer(wxVERTICAL);
    wxRadioButton *buttons[10];

    if (gba)
    {
        // Set up radio buttons for GBA save types
        leftRadio->Add(buttons[0] = new wxRadioButton(this, SELECTION_0, "None"), 1);
        leftRadio->Add(buttons[1] = new wxRadioButton(this, SELECTION_1, "EEPROM 0.5KB"), 1);
        leftRadio->Add(buttons[2] = new wxRadioButton(this, SELECTION_2, "EEPROM 8KB"), 1);
        rightRadio->Add(buttons[3] = new wxRadioButton(this, SELECTION_3, "SRAM 32KB"), 1);
        rightRadio->Add(buttons[4] = new wxRadioButton(this, SELECTION_4, "FLASH 64KB"), 1);
        rightRadio->Add(buttons[5] = new wxRadioButton(this, SELECTION_5, "FLASH 128KB"), 1);
    }
    else
    {
        // Set up radio buttons for NDS save types
        leftRadio->Add(buttons[0] = new wxRadioButton(this, SELECTION_0, "None"), 1);
        leftRadio->Add(buttons[1] = new wxRadioButton(this, SELECTION_1, "EEPROM 0.5KB"), 1);
        leftRadio->Add(buttons[2] = new wxRadioButton(this, SELECTION_2, "EEPROM 8KB"), 1);
        leftRadio->Add(buttons[4] = new wxRadioButton(this, SELECTION_4, "EEPROM 64KB"), 1);
        leftRadio->Add(buttons[5] = new wxRadioButton(this, SELECTION_5, "EEPROM 128KB"), 1);
        rightRadio->Add(buttons[3] = new wxRadioButton(this, SELECTION_3, "FRAM 32KB"), 1);
        rightRadio->Add(buttons[6] = new wxRadioButton(this, SELECTION_6, "FLASH 256KB"), 1);
        rightRadio->Add(buttons[7] = new wxRadioButton(this, SELECTION_7, "FLASH 512KB"), 1);
        rightRadio->Add(buttons[8] = new wxRadioButton(this, SELECTION_8, "FLASH 1024KB"), 1);
        rightRadio->Add(buttons[9] = new wxRadioButton(this, SELECTION_9, "FLASH 8192KB"), 1);
    }

    // Select the current save type by default
    selection = sizeToSelection(cartridge->getSaveSize());
    buttons[selection]->SetValue(true);

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

void SaveDialog::selection0(wxCommandEvent &event)
{
    // Set the selection to 0
    selection = 0;
}

void SaveDialog::selection1(wxCommandEvent &event)
{
    // Set the selection to 1
    selection = 1;
}

void SaveDialog::selection2(wxCommandEvent &event)
{
    // Set the selection to 2
    selection = 2;
}

void SaveDialog::selection3(wxCommandEvent &event)
{
    // Set the selection to 3
    selection = 3;
}

void SaveDialog::selection4(wxCommandEvent &event)
{
    // Set the selection to 4
    selection = 4;
}

void SaveDialog::selection5(wxCommandEvent &event)
{
    // Set the selection to 5
    selection = 5;
}

void SaveDialog::selection6(wxCommandEvent &event)
{
    // Set the selection to 6
    selection = 6;
}

void SaveDialog::selection7(wxCommandEvent &event)
{
    // Set the selection to 7
    selection = 7;
}

void SaveDialog::selection8(wxCommandEvent &event)
{
    // Set the selection to 8
    selection = 8;
}

void SaveDialog::selection9(wxCommandEvent &event)
{
    // Set the selection to 9
    selection = 9;
}

void SaveDialog::confirm(wxCommandEvent &event)
{
    // Confirm the change because accidentally resizing a working save file could be bad!
    // On confirmation, apply the change and restart the core
    wxMessageDialog dialog(this, "Are you sure? This may result in data loss!", "Changing Save Type", wxYES_NO | wxICON_NONE);
    if (dialog.ShowModal() == wxID_YES)
    {
        frame->stopCore(false);
        cartridge->resizeSave(selectionToSize(selection));
        frame->startCore(true);
        event.Skip(true);
    }
}
