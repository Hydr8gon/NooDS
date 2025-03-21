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

#include <wx/notebook.h>

#include "input_dialog.h"
#include "../settings.h"

enum InputEvent {
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
    REMAP_R,
    REMAP_FAST_HOLD,
    REMAP_FAST_TOGGLE,
    REMAP_FULL_SCREEN,
    REMAP_SCREEN_SWAP,
    REMAP_SYSTEM_PAUSE,
    CLEAR_MAP,
    UPDATE_JOY
};

wxBEGIN_EVENT_TABLE(InputDialog, wxDialog)
EVT_BUTTON(REMAP_A, InputDialog::remapA)
EVT_BUTTON(REMAP_B, InputDialog::remapB)
EVT_BUTTON(REMAP_X, InputDialog::remapX)
EVT_BUTTON(REMAP_Y, InputDialog::remapY)
EVT_BUTTON(REMAP_START, InputDialog::remapStart)
EVT_BUTTON(REMAP_SELECT, InputDialog::remapSelect)
EVT_BUTTON(REMAP_UP, InputDialog::remapUp)
EVT_BUTTON(REMAP_DOWN, InputDialog::remapDown)
EVT_BUTTON(REMAP_LEFT, InputDialog::remapLeft)
EVT_BUTTON(REMAP_RIGHT, InputDialog::remapRight)
EVT_BUTTON(REMAP_L, InputDialog::remapL)
EVT_BUTTON(REMAP_R, InputDialog::remapR)
EVT_BUTTON(REMAP_FAST_HOLD, InputDialog::remapFastHold)
EVT_BUTTON(REMAP_FAST_TOGGLE, InputDialog::remapFastToggle)
EVT_BUTTON(REMAP_FULL_SCREEN, InputDialog::remapFullScreen)
EVT_BUTTON(REMAP_SCREEN_SWAP, InputDialog::remapScreenSwap)
EVT_BUTTON(REMAP_SYSTEM_PAUSE, InputDialog::remapSystemPause)
EVT_BUTTON(CLEAR_MAP, InputDialog::clearMap)
EVT_TIMER(UPDATE_JOY, InputDialog::updateJoystick)
EVT_BUTTON(wxID_OK, InputDialog::confirm)
EVT_CHAR_HOOK(InputDialog::pressKey)
wxEND_EVENT_TABLE()

std::string InputDialog::keyToString(int key) {
    // Handle joystick keys
    // 1000, 2000, and 3000 are offsets assigned to these keys to identify them and hopefully avoid conflict
    if (key >= 3000)
        return "Axis " + std::to_string(key - 3000) + " -";
    else if (key >= 2000)
        return "Axis " + std::to_string(key - 2000) + " +";
    else if (key >= 1000)
        return "Button " + std::to_string(key - 1000);

    // Convert special keys to words representing their respective keys
    switch (key) {
        case 0: return "None";
        case WXK_BACK: return "Backspace";
        case WXK_TAB: return "Tab";
        case WXK_RETURN: return "Return";
        case WXK_ESCAPE: return "Escape";
        case WXK_SPACE: return "Space";
        case WXK_DELETE: return "Delete";
        case WXK_START: return "Start";
        case WXK_LBUTTON: return "Left Button";
        case WXK_RBUTTON: return "Right Button";
        case WXK_CANCEL: return "Cancel";
        case WXK_MBUTTON: return "Middle Button";
        case WXK_CLEAR: return "Clear";
        case WXK_SHIFT: return "Shift";
        case WXK_ALT: return "Alt";
        case WXK_RAW_CONTROL: return "Control";
        case WXK_MENU: return "Menu";
        case WXK_PAUSE: return "Pause";
        case WXK_CAPITAL: return "Caps Lock";
        case WXK_END: return "End";
        case WXK_HOME: return "Home";
        case WXK_LEFT: return "Left";
        case WXK_UP: return "Up";
        case WXK_RIGHT: return "Right";
        case WXK_DOWN: return "Down";
        case WXK_SELECT: return "Select";
        case WXK_PRINT: return "Print";
        case WXK_EXECUTE: return "Execute";
        case WXK_SNAPSHOT: return "Snapshot";
        case WXK_INSERT: return "Insert";
        case WXK_HELP: return "Help";
        case WXK_NUMPAD0: return "Numpad 0";
        case WXK_NUMPAD1: return "Numpad 1";
        case WXK_NUMPAD2: return "Numpad 2";
        case WXK_NUMPAD3: return "Numpad 3";
        case WXK_NUMPAD4: return "Numpad 4";
        case WXK_NUMPAD5: return "Numpad 5";
        case WXK_NUMPAD6: return "Numpad 6";
        case WXK_NUMPAD7: return "Numpad 7";
        case WXK_NUMPAD8: return "Numpad 8";
        case WXK_NUMPAD9: return "Numpad 9";
        case WXK_MULTIPLY: return "Multiply";
        case WXK_ADD: return "Add";
        case WXK_SEPARATOR: return "Separator";
        case WXK_SUBTRACT: return "Subtract";
        case WXK_DECIMAL: return "Decimal";
        case WXK_DIVIDE: return "Divide";
        case WXK_F1: return "F1";
        case WXK_F2: return "F2";
        case WXK_F3: return "F3";
        case WXK_F4: return "F4";
        case WXK_F5: return "F5";
        case WXK_F6: return "F6";
        case WXK_F7: return "F7";
        case WXK_F8: return "F8";
        case WXK_F9: return "F9";
        case WXK_F10: return "F10";
        case WXK_F11: return "F11";
        case WXK_F12: return "F12";
        case WXK_F13: return "F13";
        case WXK_F14: return "F14";
        case WXK_F15: return "F15";
        case WXK_F16: return "F16";
        case WXK_F17: return "F17";
        case WXK_F18: return "F18";
        case WXK_F19: return "F19";
        case WXK_F20: return "F20";
        case WXK_F21: return "F21";
        case WXK_F22: return "F22";
        case WXK_F23: return "F23";
        case WXK_F24: return "F24";
        case WXK_NUMLOCK: return "Numlock";
        case WXK_SCROLL: return "Scroll";
        case WXK_PAGEUP: return "Page Up";
        case WXK_PAGEDOWN: return "Page Down";
        case WXK_NUMPAD_SPACE: return "Numpad Space";
        case WXK_NUMPAD_TAB: return "Numpad Tab";
        case WXK_NUMPAD_ENTER: return "Numpad Enter";
        case WXK_NUMPAD_F1: return "Numpad F1";
        case WXK_NUMPAD_F2: return "Numpad F2";
        case WXK_NUMPAD_F3: return "Numpad F3";
        case WXK_NUMPAD_F4: return "Numpad F4";
        case WXK_NUMPAD_HOME: return "Numpad Home";
        case WXK_NUMPAD_LEFT: return "Numpad Left";
        case WXK_NUMPAD_UP: return "Numpad Up";
        case WXK_NUMPAD_RIGHT: return "Numpad Right";
        case WXK_NUMPAD_DOWN: return "Numpad Down";
        case WXK_NUMPAD_PAGEUP: return "Numpad Page Up";
        case WXK_NUMPAD_PAGEDOWN: return "Numpad Page Down";
        case WXK_NUMPAD_END: return "Numpad End";
        case WXK_NUMPAD_BEGIN: return "Numpad Begin";
        case WXK_NUMPAD_INSERT: return "Numpad Insert";
        case WXK_NUMPAD_DELETE: return "Numpad Delete";
        case WXK_NUMPAD_EQUAL: return "Numpad Equal";
        case WXK_NUMPAD_MULTIPLY: return "Numpad Multiply";
        case WXK_NUMPAD_ADD: return "Numpad Add";
        case WXK_NUMPAD_SEPARATOR: return "Numpad Separator";
        case WXK_NUMPAD_SUBTRACT: return "Numpad Subtract";
        case WXK_NUMPAD_DECIMAL: return "Numpad Decimal";
        case WXK_NUMPAD_DIVIDE: return "Numpad Divide";
    }

    // Directly use the key character for regular keys
    std::string regular;
    regular = (char)key;
    return regular;
}

InputDialog::InputDialog(wxJoystick *joystick): wxDialog(nullptr, wxID_ANY, "Input Bindings"), joystick(joystick) {
    // Load the key bindings
    memcpy(keyBinds, NooApp::keyBinds, sizeof(keyBinds));

    // Determine the height of a button
    // Borders are measured in pixels, so this value can be used to make values that scale with the DPI/font size
    wxButton *dummy = new wxButton(this, wxID_ANY, "");
    int size = dummy->GetSize().y;
    delete dummy;

    // Create separate tabs for buttons and hotkeys
    wxNotebook *notebook = new wxNotebook(this, wxID_ANY);
    wxPanel* buttonTab = new wxPanel(notebook, wxID_ANY);
    wxPanel* hotkeyTab = new wxPanel(notebook, wxID_ANY);
    notebook->AddPage(buttonTab, "&Buttons");
    notebook->AddPage(hotkeyTab, "&Hotkeys");

    // Set up the A button setting
    wxBoxSizer *aSizer = new wxBoxSizer(wxHORIZONTAL);
    aSizer->Add(new wxStaticText(buttonTab, wxID_ANY, "A:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 16);
    aSizer->Add(keyA = new wxButton(buttonTab, REMAP_A, keyToString(keyBinds[0]), wxDefaultPosition, wxSize(size * 4, size)), 0, wxLEFT, size / 16);

    // Set up the B button setting
    wxBoxSizer *bSizer = new wxBoxSizer(wxHORIZONTAL);
    bSizer->Add(new wxStaticText(buttonTab, wxID_ANY, "B:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 16);
    bSizer->Add(keyB = new wxButton(buttonTab, REMAP_B, keyToString(keyBinds[1]), wxDefaultPosition, wxSize(size * 4, size)), 0, wxLEFT, size / 16);

    // Set up the X button setting
    wxBoxSizer *xSizer = new wxBoxSizer(wxHORIZONTAL);
    xSizer->Add(new wxStaticText(buttonTab, wxID_ANY, "X:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 16);
    xSizer->Add(keyX = new wxButton(buttonTab, REMAP_X, keyToString(keyBinds[10]), wxDefaultPosition, wxSize(size * 4, size)), 0, wxLEFT, size / 16);

    // Set up the Y button setting
    wxBoxSizer *ySizer = new wxBoxSizer(wxHORIZONTAL);
    ySizer->Add(new wxStaticText(buttonTab, wxID_ANY, "Y:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 16);
    ySizer->Add(keyY = new wxButton(buttonTab, REMAP_Y, keyToString(keyBinds[11]), wxDefaultPosition, wxSize(size * 4, size)), 0, wxLEFT, size / 16);

    // Set up the Start button setting
    wxBoxSizer *startSizer = new wxBoxSizer(wxHORIZONTAL);
    startSizer->Add(new wxStaticText(buttonTab, wxID_ANY, "Start:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 16);
    startSizer->Add(keyStart = new wxButton(buttonTab, REMAP_START, keyToString(keyBinds[3]), wxDefaultPosition, wxSize(size * 4, size)), 0, wxLEFT, size / 16);

    // Set up the Select button setting
    wxBoxSizer *selectSizer = new wxBoxSizer(wxHORIZONTAL);
    selectSizer->Add(new wxStaticText(buttonTab, wxID_ANY, "Select:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 16);
    selectSizer->Add(keySelect = new wxButton(buttonTab, REMAP_SELECT, keyToString(keyBinds[2]), wxDefaultPosition, wxSize(size * 4, size)), 0, wxLEFT, size / 16);

    // Set up the Up button setting
    wxBoxSizer *upSizer = new wxBoxSizer(wxHORIZONTAL);
    upSizer->Add(new wxStaticText(buttonTab, wxID_ANY, "Up:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 16);
    upSizer->Add(keyUp = new wxButton(buttonTab, REMAP_UP, keyToString(keyBinds[6]), wxDefaultPosition, wxSize(size * 4, size)), 0, wxLEFT, size / 16);

    // Set up the Down button setting
    wxBoxSizer *downSizer = new wxBoxSizer(wxHORIZONTAL);
    downSizer->Add(new wxStaticText(buttonTab, wxID_ANY, "Down:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 16);
    downSizer->Add(keyDown = new wxButton(buttonTab, REMAP_DOWN, keyToString(keyBinds[7]), wxDefaultPosition, wxSize(size * 4, size)), 0, wxLEFT, size / 16);

    // Set up the Left button setting
    wxBoxSizer *leftSizer = new wxBoxSizer(wxHORIZONTAL);
    leftSizer->Add(new wxStaticText(buttonTab, wxID_ANY, "Left:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 16);
    leftSizer->Add(keyLeft = new wxButton(buttonTab, REMAP_LEFT, keyToString(keyBinds[5]), wxDefaultPosition, wxSize(size * 4, size)), 0, wxLEFT, size / 16);

    // Set up the Right button setting
    wxBoxSizer *rightSizer = new wxBoxSizer(wxHORIZONTAL);
    rightSizer->Add(new wxStaticText(buttonTab, wxID_ANY, "Right:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 16);
    rightSizer->Add(keyRight = new wxButton(buttonTab, REMAP_RIGHT, keyToString(keyBinds[4]), wxDefaultPosition, wxSize(size * 4, size)), 0, wxLEFT, size / 16);

    // Set up the L button setting
    wxBoxSizer *lSizer = new wxBoxSizer(wxHORIZONTAL);
    lSizer->Add(new wxStaticText(buttonTab, wxID_ANY, "L:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 16);
    lSizer->Add(keyL = new wxButton(buttonTab, REMAP_L, keyToString(keyBinds[9]), wxDefaultPosition, wxSize(size * 4, size)), 0, wxLEFT, size / 16);

    // Set up the R button setting
    wxBoxSizer *rSizer = new wxBoxSizer(wxHORIZONTAL);
    rSizer->Add(new wxStaticText(buttonTab, wxID_ANY, "R:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 16);
    rSizer->Add(keyR = new wxButton(buttonTab, REMAP_R, keyToString(keyBinds[8]), wxDefaultPosition, wxSize(size * 4, size)), 0, wxLEFT, size / 16);

    // Combine all of the left button tab contents
    wxBoxSizer *leftContents = new wxBoxSizer(wxVERTICAL);
    leftContents->Add(aSizer, 1, wxEXPAND | wxALL, size / 8);
    leftContents->Add(bSizer, 1, wxEXPAND | wxALL, size / 8);
    leftContents->Add(xSizer, 1, wxEXPAND | wxALL, size / 8);
    leftContents->Add(ySizer, 1, wxEXPAND | wxALL, size / 8);
    leftContents->Add(startSizer, 1, wxEXPAND | wxALL, size / 8);
    leftContents->Add(selectSizer, 1, wxEXPAND | wxALL, size / 8);

    // Combine all of the right button tab contents
    wxBoxSizer *rightContents = new wxBoxSizer(wxVERTICAL);
    rightContents->Add(upSizer, 1, wxEXPAND | wxALL, size / 8);
    rightContents->Add(downSizer, 1, wxEXPAND | wxALL, size / 8);
    rightContents->Add(leftSizer, 1, wxEXPAND | wxALL, size / 8);
    rightContents->Add(rightSizer, 1, wxEXPAND | wxALL, size / 8);
    rightContents->Add(lSizer, 1, wxEXPAND | wxALL, size / 8);
    rightContents->Add(rSizer, 1, wxEXPAND | wxALL, size / 8);

    // Combine the button tab contents and add a final border around it
    wxBoxSizer *buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    buttonSizer->Add(leftContents, 1, wxEXPAND | wxALL, size / 8);
    buttonSizer->Add(rightContents, 1, wxEXPAND | wxALL, size / 8);
    buttonTab->SetSizer(buttonSizer);

    // Set up the fast forward hold hotkey setting
    wxBoxSizer *fastHoldSizer = new wxBoxSizer(wxHORIZONTAL);
    fastHoldSizer->Add(new wxStaticText(hotkeyTab, wxID_ANY, "Fast Forward Hold:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 16);
    fastHoldSizer->Add(keyFastHold = new wxButton(hotkeyTab, REMAP_FAST_HOLD, keyToString(keyBinds[12]), wxDefaultPosition, wxSize(size * 4, size)), 0, wxLEFT, size / 16);

    // Set up the fast forward toggle hotkey setting
    wxBoxSizer *fastToggleSizer = new wxBoxSizer(wxHORIZONTAL);
    fastToggleSizer->Add(new wxStaticText(hotkeyTab, wxID_ANY, "Fast Forward Toggle:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 16);
    fastToggleSizer->Add(keyFastToggle = new wxButton(hotkeyTab, REMAP_FAST_TOGGLE, keyToString(keyBinds[13]), wxDefaultPosition, wxSize(size * 4, size)), 0, wxLEFT, size / 16);

    // Set up the full screen toggle hotkey setting
    wxBoxSizer *fullScreenSizer = new wxBoxSizer(wxHORIZONTAL);
    fullScreenSizer->Add(new wxStaticText(hotkeyTab, wxID_ANY, "Full Screen Toggle:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 16);
    fullScreenSizer->Add(keyFullScreen = new wxButton(hotkeyTab, REMAP_FULL_SCREEN, keyToString(keyBinds[14]), wxDefaultPosition, wxSize(size * 4, size)), 0, wxLEFT, size / 16);

    // Set up the screen swap toggle hotkey setting
    wxBoxSizer *screenSwapSizer = new wxBoxSizer(wxHORIZONTAL);
    screenSwapSizer->Add(new wxStaticText(hotkeyTab, wxID_ANY, "Screen Swap Toggle:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 16);
    screenSwapSizer->Add(keyScreenSwap = new wxButton(hotkeyTab, REMAP_SCREEN_SWAP, keyToString(keyBinds[15]), wxDefaultPosition, wxSize(size * 4, size)), 0, wxLEFT, size / 16);

    // Set up the system pause toggle hotkey setting
    wxBoxSizer *systemPauseSizer = new wxBoxSizer(wxHORIZONTAL);
    systemPauseSizer->Add(new wxStaticText(hotkeyTab, wxID_ANY, "System Pause Toggle:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 16);
    systemPauseSizer->Add(keySystemPause = new wxButton(hotkeyTab, REMAP_SYSTEM_PAUSE, keyToString(keyBinds[16]), wxDefaultPosition, wxSize(size * 4, size)), 0, wxLEFT, size / 16);

    // Combine all of the hotkey tab contents
    wxBoxSizer *hotkeyContents = new wxBoxSizer(wxVERTICAL);
    hotkeyContents->Add(fastHoldSizer, 1, wxEXPAND | wxALL, size / 8);
    hotkeyContents->Add(fastToggleSizer, 1, wxEXPAND | wxALL, size / 8);
    hotkeyContents->Add(fullScreenSizer, 1, wxEXPAND | wxALL, size / 8);
    hotkeyContents->Add(screenSwapSizer, 1, wxEXPAND | wxALL, size / 8);
    hotkeyContents->Add(systemPauseSizer, 1, wxEXPAND | wxALL, size / 8);
    hotkeyContents->Add(new wxStaticText(hotkeyTab, wxID_ANY, ""), 1);

    // Add a final border around the hotkey tab
    wxBoxSizer *hotkeySizer = new wxBoxSizer(wxHORIZONTAL);
    hotkeySizer->Add(hotkeyContents, 1, wxEXPAND | wxALL, size / 8);
    hotkeyTab->SetSizer(hotkeySizer);

    // Set up the common navigation buttons
    wxBoxSizer *naviSizer = new wxBoxSizer(wxHORIZONTAL);
    naviSizer->Add(new wxStaticText(this, wxID_ANY, ""), 1);
    naviSizer->Add(new wxButton(this, CLEAR_MAP, "Clear"), 0, wxRIGHT, size / 16);
    naviSizer->Add(new wxButton(this, wxID_CANCEL, "Cancel"), 0, wxLEFT | wxRIGHT, size / 16);
    naviSizer->Add(new wxButton(this, wxID_OK, "Confirm"), 0, wxLEFT, size / 16);

    // Populate the dialog
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(notebook, 1, wxEXPAND);
    sizer->Add(naviSizer, 0, wxEXPAND | wxALL, size / 8);
    SetSizerAndFit(sizer);

    // Size the window to prevent resizing
    SetMinSize(GetSize());
    SetMaxSize(GetSize());

    // Set up joystick input if a joystick is connected
    if (joystick) {
        // Save the initial axis values so inputs can be detected as offsets instead of raw values
        // This avoids issues with axes that have non-zero values in their resting positions
        for (int i = 0; i < joystick->GetNumberAxes(); i++)
            axisBases.push_back(joystick->GetPosition(i));

        // Start a timer to update joystick input, since wxJoystickEvents are unreliable
        timer = new wxTimer(this, UPDATE_JOY);
        timer->Start(10);
    }
}

InputDialog::~InputDialog() {
    // Clean up the joystick timer
    if (joystick)
        delete timer;
}

void InputDialog::resetLabels() {
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
    keyFastHold->SetLabel(keyToString(keyBinds[12]));
    keyFastToggle->SetLabel(keyToString(keyBinds[13]));
    keyFullScreen->SetLabel(keyToString(keyBinds[14]));
    keyScreenSwap->SetLabel(keyToString(keyBinds[15]));
    keySystemPause->SetLabel(keyToString(keyBinds[16]));
    current = nullptr;
}

void InputDialog::remapA(wxCommandEvent &event) {
    // Prepare the A button for remapping
    resetLabels();
    keyA->SetLabel("Press a key");
    current = keyA;
    keyIndex = 0;
}

void InputDialog::remapB(wxCommandEvent &event) {
    // Prepare the B button for remapping
    resetLabels();
    keyB->SetLabel("Press a key");
    current = keyB;
    keyIndex = 1;
}

void InputDialog::remapX(wxCommandEvent &event) {
    // Prepare the X button for remapping
    resetLabels();
    keyX->SetLabel("Press a key");
    current = keyX;
    keyIndex = 10;
}

void InputDialog::remapY(wxCommandEvent &event) {
    // Prepare the Y button for remapping
    resetLabels();
    keyY->SetLabel("Press a key");
    current = keyY;
    keyIndex = 11;
}

void InputDialog::remapStart(wxCommandEvent &event) {
    // Prepare the Start button for remapping
    resetLabels();
    keyStart->SetLabel("Press a key");
    current = keyStart;
    keyIndex = 3;
}

void InputDialog::remapSelect(wxCommandEvent &event) {
    // Prepare the Select button for remapping
    resetLabels();
    keySelect->SetLabel("Press a key");
    current = keySelect;
    keyIndex = 2;
}

void InputDialog::remapUp(wxCommandEvent &event) {
    // Prepare the Up button for remapping
    resetLabels();
    keyUp->SetLabel("Press a key");
    current = keyUp;
    keyIndex = 6;
}

void InputDialog::remapDown(wxCommandEvent &event) {
    // Prepare the Down button for remapping
    resetLabels();
    keyDown->SetLabel("Press a key");
    current = keyDown;
    keyIndex = 7;
}

void InputDialog::remapLeft(wxCommandEvent &event) {
    // Prepare the Left button for remapping
    resetLabels();
    keyLeft->SetLabel("Press a key");
    current = keyLeft;
    keyIndex = 5;
}

void InputDialog::remapRight(wxCommandEvent &event) {
    // Prepare the Right button for remapping
    resetLabels();
    keyRight->SetLabel("Press a key");
    current = keyRight;
    keyIndex = 4;
}

void InputDialog::remapL(wxCommandEvent &event) {
    // Prepare the L button for remapping
    resetLabels();
    keyL->SetLabel("Press a key");
    current = keyL;
    keyIndex = 9;
}

void InputDialog::remapR(wxCommandEvent &event) {
    // Prepare the R button for remapping
    resetLabels();
    keyR->SetLabel("Press a key");
    current = keyR;
    keyIndex = 8;
}

void InputDialog::remapFastHold(wxCommandEvent &event) {
    // Prepare the fast forward hold hotkey for remapping
    resetLabels();
    keyFastHold->SetLabel("Press a key");
    current = keyFastHold;
    keyIndex = 12;
}

void InputDialog::remapFastToggle(wxCommandEvent &event) {
    // Prepare the fast forward toggle hotkey for remapping
    resetLabels();
    keyFastToggle->SetLabel("Press a key");
    current = keyFastToggle;
    keyIndex = 13;
}

void InputDialog::remapFullScreen(wxCommandEvent &event) {
    // Prepare the full screen toggle hotkey for remapping
    resetLabels();
    keyFullScreen->SetLabel("Press a key");
    current = keyFullScreen;
    keyIndex = 14;
}

void InputDialog::remapScreenSwap(wxCommandEvent &event) {
    // Prepare the screen swap toggle hotkey for remapping
    resetLabels();
    keyScreenSwap->SetLabel("Press a key");
    current = keyScreenSwap;
    keyIndex = 15;
}

void InputDialog::remapSystemPause(wxCommandEvent &event) {
    // Prepare the system pause toggle hotkey for remapping
    resetLabels();
    keySystemPause->SetLabel("Press a key");
    current = keySystemPause;
    keyIndex = 16;
}

void InputDialog::clearMap(wxCommandEvent &event) {
    if (current) {
        // If a button is selected, clear only its mapping
        keyBinds[keyIndex] = 0;
        current->SetLabel(keyToString(keyBinds[keyIndex]));
        current = nullptr;
    }
    else {
        // If no button is selected, clear all mappings
        for (int i = 0; i < MAX_KEYS; i++)
            keyBinds[i] = 0;
        resetLabels();
    }
}

void InputDialog::updateJoystick(wxTimerEvent &event) {
    // Map the current button to a joystick button if one is pressed
    if (!current) return;
    for (int i = 0; i < joystick->GetNumberButtons(); i++) {
        if (joystick->GetButtonState(i)) {
            keyBinds[keyIndex] = 1000 + i;
            current->SetLabel(keyToString(keyBinds[keyIndex]));
            current = nullptr;
            return;
        }
    }

    // Map the current button to a joystick axis if one is held
    for (int i = 0; i < joystick->GetNumberAxes(); i++) {
        if (joystick->GetPosition(i) - axisBases[i] > 16384) { // Positive axis
            keyBinds[keyIndex] = 2000 + i;
            current->SetLabel(keyToString(keyBinds[keyIndex]));
            current = nullptr;
            return;
        }
        else if (joystick->GetPosition(i) - axisBases[i] < -16384) { // Negative axis
            keyBinds[keyIndex] = 3000 + i;
            current->SetLabel(keyToString(keyBinds[keyIndex]));
            current = nullptr;
            return;
        }
    }
}

void InputDialog::confirm(wxCommandEvent &event) {
    // Save the key mappings
    memcpy(NooApp::keyBinds, keyBinds, sizeof(keyBinds));
    Settings::save();
    event.Skip(true);
}

void InputDialog::pressKey(wxKeyEvent &event) {
    // Map the current button to the pressed key
    if (!current) return;
    keyBinds[keyIndex] = event.GetKeyCode();
    current->SetLabel(keyToString(keyBinds[keyIndex]));
    current = nullptr;
}
