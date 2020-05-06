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

#include "input_dialog.h"
#include "noo_app.h"
#include "../settings.h"

enum Event
{
    REMAP_A = 1,
    REMAP_B,
    REMAP_X,
    REMAP_Y,
    REMAP_START,
    REMAP_SELECT,
    REMAP_UP,
    REMAP_DOWN,
    REMAP_LEFT,
    REMAP_RIGHT,
    REMAP_L,
    REMAP_R
};

wxBEGIN_EVENT_TABLE(InputDialog, wxDialog)
EVT_BUTTON(REMAP_A,      InputDialog::remapA)
EVT_BUTTON(REMAP_B,      InputDialog::remapB)
EVT_BUTTON(REMAP_X,      InputDialog::remapX)
EVT_BUTTON(REMAP_Y,      InputDialog::remapY)
EVT_BUTTON(REMAP_START,  InputDialog::remapStart)
EVT_BUTTON(REMAP_SELECT, InputDialog::remapSelect)
EVT_BUTTON(REMAP_UP,     InputDialog::remapUp)
EVT_BUTTON(REMAP_DOWN,   InputDialog::remapDown)
EVT_BUTTON(REMAP_LEFT,   InputDialog::remapLeft)
EVT_BUTTON(REMAP_RIGHT,  InputDialog::remapRight)
EVT_BUTTON(REMAP_L,      InputDialog::remapL)
EVT_BUTTON(REMAP_R,      InputDialog::remapR)
EVT_BUTTON(wxID_OK,      InputDialog::confirm)
EVT_CHAR_HOOK(InputDialog::pressKey)
wxEND_EVENT_TABLE()

std::string InputDialog::keyToString(int key)
{
    // Convert special keys to words representing their respective keys
    switch (key)
    {
        case WXK_BACK:             return "Backspace";
        case WXK_TAB:              return "Tab";
        case WXK_RETURN:           return "Return";
        case WXK_ESCAPE:           return "Escape";
        case WXK_SPACE:            return "Space";
        case WXK_DELETE:           return "Delete";
        case WXK_START:            return "Start";
        case WXK_LBUTTON:          return "Left Button";
        case WXK_RBUTTON:          return "Right Button";
        case WXK_CANCEL:           return "Cancel";
        case WXK_MBUTTON:          return "Middle Button";
        case WXK_CLEAR:            return "Clear";
        case WXK_SHIFT:            return "Shift";
        case WXK_ALT:              return "Alt";
        case WXK_RAW_CONTROL:      return "Control";
        case WXK_MENU:             return "Menu";
        case WXK_PAUSE:            return "Pause";
        case WXK_CAPITAL:          return "Caps Lock";
        case WXK_END:              return "End";
        case WXK_HOME:             return "Home";
        case WXK_LEFT:             return "Left";
        case WXK_UP:               return "Up";
        case WXK_RIGHT:            return "Right";
        case WXK_DOWN:             return "Down";
        case WXK_SELECT:           return "Select";
        case WXK_PRINT:            return "Print";
        case WXK_EXECUTE:          return "Execute";
        case WXK_SNAPSHOT:         return "Snapshot";
        case WXK_INSERT:           return "Insert";
        case WXK_HELP:             return "Help";
        case WXK_NUMPAD0:          return "Numpad 0";
        case WXK_NUMPAD1:          return "Numpad 1";
        case WXK_NUMPAD2:          return "Numpad 2";
        case WXK_NUMPAD3:          return "Numpad 3";
        case WXK_NUMPAD4:          return "Numpad 4";
        case WXK_NUMPAD5:          return "Numpad 5";
        case WXK_NUMPAD6:          return "Numpad 6";
        case WXK_NUMPAD7:          return "Numpad 7";
        case WXK_NUMPAD8:          return "Numpad 8";
        case WXK_NUMPAD9:          return "Numpad 9";
        case WXK_MULTIPLY:         return "Multiply";
        case WXK_ADD:              return "Add";
        case WXK_SEPARATOR:        return "Separator";
        case WXK_SUBTRACT:         return "Subtract";
        case WXK_DECIMAL:          return "Decimal";
        case WXK_DIVIDE:           return "Divide";
        case WXK_F1:               return "F1";
        case WXK_F2:               return "F2";
        case WXK_F3:               return "F3";
        case WXK_F4:               return "F4";
        case WXK_F5:               return "F5";
        case WXK_F6:               return "F6";
        case WXK_F7:               return "F7";
        case WXK_F8:               return "F8";
        case WXK_F9:               return "F9";
        case WXK_F10:              return "F10";
        case WXK_F11:              return "F11";
        case WXK_F12:              return "F12";
        case WXK_F13:              return "F13";
        case WXK_F14:              return "F14";
        case WXK_F15:              return "F15";
        case WXK_F16:              return "F16";
        case WXK_F17:              return "F17";
        case WXK_F18:              return "F18";
        case WXK_F19:              return "F19";
        case WXK_F20:              return "F20";
        case WXK_F21:              return "F21";
        case WXK_F22:              return "F22";
        case WXK_F23:              return "F23";
        case WXK_F24:              return "F24";
        case WXK_NUMLOCK:          return "Numlock";
        case WXK_SCROLL:           return "Scroll";
        case WXK_PAGEUP:           return "Page Up";
        case WXK_PAGEDOWN:         return "Page Down";
        case WXK_NUMPAD_SPACE:     return "Numpad Space";
        case WXK_NUMPAD_TAB:       return "Numpad Tab";
        case WXK_NUMPAD_ENTER:     return "Numpad Enter";
        case WXK_NUMPAD_F1:        return "Numpad F1";
        case WXK_NUMPAD_F2:        return "Numpad F2";
        case WXK_NUMPAD_F3:        return "Numpad F3";
        case WXK_NUMPAD_F4:        return "Numpad F4";
        case WXK_NUMPAD_HOME:      return "Numpad Home";
        case WXK_NUMPAD_LEFT:      return "Numpad Left";
        case WXK_NUMPAD_UP:        return "Numpad Up";
        case WXK_NUMPAD_RIGHT:     return "Numpad Right";
        case WXK_NUMPAD_DOWN:      return "Numpad Down";
        case WXK_NUMPAD_PAGEUP:    return "Numpad Page Up";
        case WXK_NUMPAD_PAGEDOWN:  return "Numpad Page Down";
        case WXK_NUMPAD_END:       return "Numpad End";
        case WXK_NUMPAD_BEGIN:     return "Numpad Begin";
        case WXK_NUMPAD_INSERT:    return "Numpad Insert";
        case WXK_NUMPAD_DELETE:    return "Numpad Delete";
        case WXK_NUMPAD_EQUAL:     return "Numpad Equal";
        case WXK_NUMPAD_MULTIPLY:  return "Numpad Multiply";
        case WXK_NUMPAD_ADD:       return "Numpad Add";
        case WXK_NUMPAD_SEPARATOR: return "Numpad Separator";
        case WXK_NUMPAD_SUBTRACT:  return "Numpad Subtract";
        case WXK_NUMPAD_DECIMAL:   return "Numpad Decimal";
        case WXK_NUMPAD_DIVIDE:    return "Numpad Divide";
    }

    // Directly use the key character for regular keys
    std::string regular;
    regular = (char)key;
    return regular;
}

InputDialog::InputDialog(): wxDialog(nullptr, wxID_ANY, "Input Bindings")
{
    // Load the key bindings
    for (int i = 0; i < 12; i++)
        keyBinds[i] = NooApp::getKeyBind(i);

    // Determine the height of a button
    // Borders are measured in pixels, so this value can be used to make values that scale with the DPI/font size
    wxButton *dummy = new wxButton(this, wxID_ANY, "");
    int size = dummy->GetSize().y;
    delete dummy;

    // Set up the A button setting
    wxBoxSizer *aSizer = new wxBoxSizer(wxHORIZONTAL);
    aSizer->Add(new wxStaticText(this, wxID_ANY, "A:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 16);
    aSizer->Add(keyA = new wxButton(this, REMAP_A, keyToString(keyBinds[0]), wxDefaultPosition, wxSize(size * 4, size)), 0, wxLEFT, size / 16);

    // Set up the B button setting
    wxBoxSizer *bSizer = new wxBoxSizer(wxHORIZONTAL);
    bSizer->Add(new wxStaticText(this, wxID_ANY, "B:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 16);
    bSizer->Add(keyB = new wxButton(this, REMAP_B, keyToString(keyBinds[1]), wxDefaultPosition, wxSize(size * 4, size)), 0, wxLEFT, size / 16);

    // Set up the X button setting
    wxBoxSizer *xSizer = new wxBoxSizer(wxHORIZONTAL);
    xSizer->Add(new wxStaticText(this, wxID_ANY, "X:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 16);
    xSizer->Add(keyX = new wxButton(this, REMAP_X, keyToString(keyBinds[10]), wxDefaultPosition, wxSize(size * 4, size)), 0, wxLEFT, size / 16);

    // Set up the Y button setting
    wxBoxSizer *ySizer = new wxBoxSizer(wxHORIZONTAL);
    ySizer->Add(new wxStaticText(this, wxID_ANY, "Y:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 16);
    ySizer->Add(keyY = new wxButton(this, REMAP_Y, keyToString(keyBinds[11]), wxDefaultPosition, wxSize(size * 4, size)), 0, wxLEFT, size / 16);

    // Set up the Start button setting
    wxBoxSizer *startSizer = new wxBoxSizer(wxHORIZONTAL);
    startSizer->Add(new wxStaticText(this, wxID_ANY, "Start:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 16);
    startSizer->Add(keyStart = new wxButton(this, REMAP_START, keyToString(keyBinds[3]), wxDefaultPosition, wxSize(size * 4, size)), 0, wxLEFT, size / 16);

    // Set up the Select button setting
    wxBoxSizer *selectSizer = new wxBoxSizer(wxHORIZONTAL);
    selectSizer->Add(new wxStaticText(this, wxID_ANY, "Select:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 16);
    selectSizer->Add(keySelect = new wxButton(this, REMAP_SELECT, keyToString(keyBinds[2]), wxDefaultPosition, wxSize(size * 4, size)), 0, wxLEFT, size / 16);

    // Set up the Up button setting
    wxBoxSizer *upSizer = new wxBoxSizer(wxHORIZONTAL);
    upSizer->Add(new wxStaticText(this, wxID_ANY, "Up:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 16);
    upSizer->Add(keyUp = new wxButton(this, REMAP_UP, keyToString(keyBinds[6]), wxDefaultPosition, wxSize(size * 4, size)), 0, wxLEFT, size / 16);

    // Set up the Down button setting
    wxBoxSizer *downSizer = new wxBoxSizer(wxHORIZONTAL);
    downSizer->Add(new wxStaticText(this, wxID_ANY, "Down:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 16);
    downSizer->Add(keyDown = new wxButton(this, REMAP_DOWN, keyToString(keyBinds[7]), wxDefaultPosition, wxSize(size * 4, size)), 0, wxLEFT, size / 16);

    // Set up the Left button setting
    wxBoxSizer *leftSizer = new wxBoxSizer(wxHORIZONTAL);
    leftSizer->Add(new wxStaticText(this, wxID_ANY, "Left:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 16);
    leftSizer->Add(keyLeft = new wxButton(this, REMAP_LEFT, keyToString(keyBinds[5]), wxDefaultPosition, wxSize(size * 4, size)), 0, wxLEFT, size / 16);

    // Set up the Right button setting
    wxBoxSizer *rightSizer = new wxBoxSizer(wxHORIZONTAL);
    rightSizer->Add(new wxStaticText(this, wxID_ANY, "Right:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 16);
    rightSizer->Add(keyRight = new wxButton(this, REMAP_RIGHT, keyToString(keyBinds[4]), wxDefaultPosition, wxSize(size * 4, size)), 0, wxLEFT, size / 16);

    // Set up the L button setting
    wxBoxSizer *lSizer = new wxBoxSizer(wxHORIZONTAL);
    lSizer->Add(new wxStaticText(this, wxID_ANY, "L:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 16);
    lSizer->Add(keyL = new wxButton(this, REMAP_L, keyToString(keyBinds[9]), wxDefaultPosition, wxSize(size * 4, size)), 0, wxLEFT, size / 16);

    // Set up the R button setting
    wxBoxSizer *rSizer = new wxBoxSizer(wxHORIZONTAL);
    rSizer->Add(new wxStaticText(this, wxID_ANY, "R:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 16);
    rSizer->Add(keyR = new wxButton(this, REMAP_R, keyToString(keyBinds[8]), wxDefaultPosition, wxSize(size * 4, size)), 0, wxLEFT, size / 16);

    // Set up the cancel and confirm buttons
    wxBoxSizer *buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    buttonSizer->Add(new wxStaticText(this, wxID_ANY, ""), 1);
    buttonSizer->Add(new wxButton(this, wxID_CANCEL, "Cancel"),  0, wxRIGHT, size / 16);
    buttonSizer->Add(new wxButton(this, wxID_OK,     "Confirm"), 0, wxLEFT,  size / 16);

    // Combine all of the left contents
    wxBoxSizer *leftContents = new wxBoxSizer(wxVERTICAL);
    leftContents->Add(aSizer,      1, wxEXPAND | wxALL, size / 8);
    leftContents->Add(bSizer,      1, wxEXPAND | wxALL, size / 8);
    leftContents->Add(xSizer,      1, wxEXPAND | wxALL, size / 8);
    leftContents->Add(ySizer,      1, wxEXPAND | wxALL, size / 8);
    leftContents->Add(startSizer,  1, wxEXPAND | wxALL, size / 8);
    leftContents->Add(selectSizer, 1, wxEXPAND | wxALL, size / 8);
    leftContents->Add(new wxStaticText(this, wxID_ANY, ""), 1, wxEXPAND | wxALL, size / 8);

    // Combine all of the right contents
    wxBoxSizer *rightContents = new wxBoxSizer(wxVERTICAL);
    rightContents->Add(upSizer,     1, wxEXPAND | wxALL, size / 8);
    rightContents->Add(downSizer,   1, wxEXPAND | wxALL, size / 8);
    rightContents->Add(leftSizer,   1, wxEXPAND | wxALL, size / 8);
    rightContents->Add(rightSizer,  1, wxEXPAND | wxALL, size / 8);
    rightContents->Add(lSizer,      1, wxEXPAND | wxALL, size / 8);
    rightContents->Add(rSizer,      1, wxEXPAND | wxALL, size / 8);
    rightContents->Add(buttonSizer, 1, wxEXPAND | wxALL, size / 8);

    // Add a final border around everything
    wxBoxSizer *sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(leftContents,  1, wxEXPAND | wxALL, size / 8);
    sizer->Add(rightContents, 1, wxEXPAND | wxALL, size / 8);
    SetSizer(sizer);

    // Size the window to fit the contents and prevent resizing
    sizer->Fit(this);
    SetMinSize(GetSize());
    SetMaxSize(GetSize());
}

void InputDialog::resetLabels()
{
    // Reset the button labels
    keyA->SetLabel(keyToString(keyBinds[0]));
    keyB->SetLabel(keyToString(keyBinds[1]));
    keyX->SetLabel(keyToString(keyBinds[10]));
    keyY->SetLabel(keyToString(keyBinds[11]));
    keyStart->SetLabel(keyToString(keyBinds[3]));
    keySelect->SetLabel(keyToString(keyBinds[2]));
    keyUp->SetLabel(keyToString(keyBinds[6]));
    keyDown->SetLabel(keyToString(keyBinds[7]));
    keyLeft->SetLabel(keyToString(keyBinds[5]));
    keyRight->SetLabel(keyToString(keyBinds[4]));
    keyL->SetLabel(keyToString(keyBinds[9]));
    keyR->SetLabel(keyToString(keyBinds[8]));
    current = nullptr;
}

void InputDialog::remapA(wxCommandEvent &event)
{
    // Prepare the A button for remapping
    resetLabels();
    keyA->SetLabel("Press a key");
    current = keyA;
    keyIndex = 0;
}

void InputDialog::remapB(wxCommandEvent &event)
{
    // Prepare the B button for remapping
    resetLabels();
    keyB->SetLabel("Press a key");
    current = keyB;
    keyIndex = 1;
}

void InputDialog::remapX(wxCommandEvent &event)
{
    // Prepare the X button for remapping
    resetLabels();
    keyX->SetLabel("Press a key");
    current = keyX;
    keyIndex = 10;
}

void InputDialog::remapY(wxCommandEvent &event)
{
    // Prepare the Y button for remapping
    resetLabels();
    keyY->SetLabel("Press a key");
    current = keyY;
    keyIndex = 11;
}

void InputDialog::remapStart(wxCommandEvent &event)
{
    // Prepare the Start button for remapping
    resetLabels();
    keyStart->SetLabel("Press a key");
    current = keyStart;
    keyIndex = 3;
}

void InputDialog::remapSelect(wxCommandEvent &event)
{
    // Prepare the Select button for remapping
    resetLabels();
    keySelect->SetLabel("Press a key");
    current = keySelect;
    keyIndex = 2;
}

void InputDialog::remapUp(wxCommandEvent &event)
{
    // Prepare the Up button for remapping
    resetLabels();
    keyUp->SetLabel("Press a key");
    current = keyUp;
    keyIndex = 6;
}

void InputDialog::remapDown(wxCommandEvent &event)
{
    // Prepare the Down button for remapping
    resetLabels();
    keyDown->SetLabel("Press a key");
    current = keyDown;
    keyIndex = 7;
}

void InputDialog::remapLeft(wxCommandEvent &event)
{
    // Prepare the Left button for remapping
    resetLabels();
    keyLeft->SetLabel("Press a key");
    current = keyLeft;
    keyIndex = 5;
}

void InputDialog::remapRight(wxCommandEvent &event)
{
    // Prepare the Right button for remapping
    resetLabels();
    keyRight->SetLabel("Press a key");
    current = keyRight;
    keyIndex = 4;
}

void InputDialog::remapL(wxCommandEvent &event)
{
    // Prepare the L button for remapping
    resetLabels();
    keyL->SetLabel("Press a key");
    current = keyL;
    keyIndex = 9;
}

void InputDialog::remapR(wxCommandEvent &event)
{
    // Prepare the R button for remapping
    resetLabels();
    keyR->SetLabel("Press a key");
    current = keyR;
    keyIndex = 8;
}

void InputDialog::confirm(wxCommandEvent &event)
{
    // Save the key mappings
    for (int i = 0; i < 12; i++)
        NooApp::setKeyBind(i, keyBinds[i]);

    event.Skip(true);
}

void InputDialog::pressKey(wxKeyEvent &event)
{
    if (!current) return;

    // Update the key mapping of the current button
    keyBinds[keyIndex] = event.GetKeyCode();
    current->SetLabel(keyToString(keyBinds[keyIndex]));
    current = nullptr;
}
