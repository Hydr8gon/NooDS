/*
    Copyright 2019-2023 Hydr8gon

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

#include "layout_dialog.h"
#include "noo_app.h"
#include "../common/screen_layout.h"
#include "../settings.h"

enum LayoutEvent
{
    POS_CENTER = 1,
    POS_TOP,
    POS_BOTTOM,
    POS_LEFT,
    POS_RIGHT,
    ROTATE_NONE,
    ROTATE_CW,
    ROTATE_CCW,
    ARRANGE_AUTO,
    ARRANGE_VERT,
    ARRANGE_HORI,
    ARRANGE_SING,
    SIZE_EVEN,
    SIZE_TOP,
    SIZE_BOT,
    GAP_NONE,
    GAP_QUART,
    GAP_HALF,
    GAP_FULL,
    INT_SCALE,
    GBA_CROP,
    FILTER
};

wxBEGIN_EVENT_TABLE(LayoutDialog, wxDialog)
EVT_RADIOBUTTON(POS_CENTER,   LayoutDialog::posCenter)
EVT_RADIOBUTTON(POS_TOP,      LayoutDialog::posTop)
EVT_RADIOBUTTON(POS_BOTTOM,   LayoutDialog::posBottom)
EVT_RADIOBUTTON(POS_LEFT,     LayoutDialog::posLeft)
EVT_RADIOBUTTON(POS_RIGHT,    LayoutDialog::posRight)
EVT_RADIOBUTTON(ROTATE_NONE,  LayoutDialog::rotateNone)
EVT_RADIOBUTTON(ROTATE_CW,    LayoutDialog::rotateCw)
EVT_RADIOBUTTON(ROTATE_CCW,   LayoutDialog::rotateCcw)
EVT_RADIOBUTTON(ARRANGE_AUTO, LayoutDialog::arrangeAuto)
EVT_RADIOBUTTON(ARRANGE_VERT, LayoutDialog::arrangeVert)
EVT_RADIOBUTTON(ARRANGE_HORI, LayoutDialog::arrangeHori)
EVT_RADIOBUTTON(ARRANGE_SING, LayoutDialog::arrangeSing)
EVT_RADIOBUTTON(SIZE_EVEN,    LayoutDialog::sizeEven)
EVT_RADIOBUTTON(SIZE_TOP,     LayoutDialog::sizeTop)
EVT_RADIOBUTTON(SIZE_BOT,     LayoutDialog::sizeBot)
EVT_RADIOBUTTON(GAP_NONE,     LayoutDialog::gapNone)
EVT_RADIOBUTTON(GAP_QUART,    LayoutDialog::gapQuart)
EVT_RADIOBUTTON(GAP_HALF,     LayoutDialog::gapHalf)
EVT_RADIOBUTTON(GAP_FULL,     LayoutDialog::gapFull)
EVT_CHECKBOX(INT_SCALE,       LayoutDialog::intScale)
EVT_CHECKBOX(GBA_CROP,        LayoutDialog::gbaCrop)
EVT_CHECKBOX(FILTER,          LayoutDialog::filter)
EVT_BUTTON(wxID_CANCEL,       LayoutDialog::cancel)
EVT_BUTTON(wxID_OK,           LayoutDialog::confirm)
wxEND_EVENT_TABLE()

LayoutDialog::LayoutDialog(NooApp *app): wxDialog(nullptr, wxID_ANY, "Screen Layout"), app(app)
{
    // Remember the previous settings in case the changes are discarded
    prevSettings[0] = ScreenLayout::screenRotation;
    prevSettings[1] = ScreenLayout::screenArrangement;
    prevSettings[2] = ScreenLayout::screenSizing;
    prevSettings[3] = ScreenLayout::screenGap;
    prevSettings[4] = ScreenLayout::integerScale;
    prevSettings[5] = ScreenLayout::gbaCrop;
    prevSettings[6] = NooApp::screenFilter;

    // Determine the height of a button
    // Borders are measured in pixels, so this value can be used to make values that scale with the DPI/font size
    wxButton *dummy = new wxButton(this, wxID_ANY, "");
    int size = dummy->GetSize().y;
    delete dummy;

    // Set up the position settings
    wxRadioButton *posBtns[5];
    wxBoxSizer *posSizer = new wxBoxSizer(wxHORIZONTAL);
    posSizer->Add(new wxStaticText(this, wxID_ANY, "Position:", wxDefaultPosition,
        wxSize(wxDefaultSize.GetWidth(), size)), 0, wxALIGN_CENTRE | wxRIGHT, size / 8);
    posSizer->Add(posBtns[0] = new wxRadioButton(this, POS_CENTER, "Center",
        wxDefaultPosition, wxDefaultSize, wxRB_GROUP), 0, wxLEFT, size / 8);
    posSizer->Add(posBtns[1] = new wxRadioButton(this, POS_TOP,    "Top"),    0, wxLEFT, size / 8);
    posSizer->Add(posBtns[2] = new wxRadioButton(this, POS_BOTTOM, "Bottom"), 0, wxLEFT, size / 8);
    posSizer->Add(posBtns[3] = new wxRadioButton(this, POS_LEFT,   "Left"),   0, wxLEFT, size / 8);
    posSizer->Add(posBtns[4] = new wxRadioButton(this, POS_RIGHT,  "Right"),  0, wxLEFT, size / 8);

    // Set up the rotation settings
    wxRadioButton *rotateBtns[3];
    wxBoxSizer *rotateSizer = new wxBoxSizer(wxHORIZONTAL);
    rotateSizer->Add(new wxStaticText(this, wxID_ANY, "Rotation:", wxDefaultPosition,
        wxSize(wxDefaultSize.GetWidth(), size)), 0, wxALIGN_CENTRE | wxRIGHT, size / 8);
    rotateSizer->Add(rotateBtns[0] = new wxRadioButton(this, ROTATE_NONE, "None",
        wxDefaultPosition, wxDefaultSize, wxRB_GROUP), 0, wxLEFT, size / 8);
    rotateSizer->Add(rotateBtns[1] = new wxRadioButton(this, ROTATE_CW,  "Clockwise"),         0, wxLEFT, size / 8);
    rotateSizer->Add(rotateBtns[2] = new wxRadioButton(this, ROTATE_CCW, "Counter-Clockwise"), 0, wxLEFT, size / 8);

    // Set up the arrangement settings
    wxRadioButton *arrangeBtns[4];
    wxBoxSizer *arrangeSizer = new wxBoxSizer(wxHORIZONTAL);
    arrangeSizer->Add(new wxStaticText(this, wxID_ANY, "Arrangement:", wxDefaultPosition,
        wxSize(wxDefaultSize.GetWidth(), size)), 0, wxALIGN_CENTRE | wxRIGHT, size / 8);
    arrangeSizer->Add(arrangeBtns[0] = new wxRadioButton(this, ARRANGE_AUTO, "Automatic",
        wxDefaultPosition, wxDefaultSize, wxRB_GROUP), 0, wxLEFT, size / 8);
    arrangeSizer->Add(arrangeBtns[1] = new wxRadioButton(this, ARRANGE_VERT, "Vertical"),   0, wxLEFT, size / 8);
    arrangeSizer->Add(arrangeBtns[2] = new wxRadioButton(this, ARRANGE_HORI, "Horizontal"), 0, wxLEFT, size / 8);
    arrangeSizer->Add(arrangeBtns[3] = new wxRadioButton(this, ARRANGE_SING, "Single Screen"), 0, wxLEFT, size / 8);

    // Set up the sizing settings
    wxRadioButton *sizeBtns[3];
    wxBoxSizer *sizeSizer = new wxBoxSizer(wxHORIZONTAL);
    sizeSizer->Add(new wxStaticText(this, wxID_ANY, "Sizing:", wxDefaultPosition,
        wxSize(wxDefaultSize.GetWidth(), size)), 0, wxALIGN_CENTRE | wxRIGHT, size / 8);
    sizeSizer->Add(sizeBtns[0] = new wxRadioButton(this, SIZE_EVEN, "Even",
        wxDefaultPosition, wxDefaultSize, wxRB_GROUP), 0, wxLEFT, size / 8);
    sizeSizer->Add(sizeBtns[1] = new wxRadioButton(this, SIZE_TOP, "Enlarge Top"),    0, wxLEFT, size / 8);
    sizeSizer->Add(sizeBtns[2] = new wxRadioButton(this, SIZE_BOT, "Enlarge Bottom"), 0, wxLEFT, size / 8);

    // Set up the gap settings
    wxRadioButton *gapBtns[4];
    wxBoxSizer *gapSizer = new wxBoxSizer(wxHORIZONTAL);
    gapSizer->Add(new wxStaticText(this, wxID_ANY, "Gap:", wxDefaultPosition,
        wxSize(wxDefaultSize.GetWidth(), size)), 0, wxALIGN_CENTRE | wxRIGHT, size / 8);
    gapSizer->Add(gapBtns[0] = new wxRadioButton(this, GAP_NONE, "None",
        wxDefaultPosition, wxDefaultSize, wxRB_GROUP), 0, wxLEFT, size / 8);
    gapSizer->Add(gapBtns[1] = new wxRadioButton(this, GAP_QUART, "Quarter"), 0, wxLEFT, size / 8);
    gapSizer->Add(gapBtns[2] = new wxRadioButton(this, GAP_HALF,  "Half"),    0, wxLEFT, size / 8);
    gapSizer->Add(gapBtns[3] = new wxRadioButton(this, GAP_FULL,  "Full"),    0, wxLEFT, size / 8);

    // Set up the checkbox settings
    wxCheckBox *boxes[3];
    wxBoxSizer *checkSizer = new wxBoxSizer(wxHORIZONTAL);
    checkSizer->Add(boxes[0] = new wxCheckBox(this, INT_SCALE, "Integer Scale"), 0, wxLEFT, size / 8);
    checkSizer->Add(boxes[1] = new wxCheckBox(this, GBA_CROP,  "GBA Crop"),      0, wxLEFT, size / 8);
    checkSizer->Add(boxes[2] = new wxCheckBox(this, FILTER,    "Filter"),        0, wxLEFT, size / 8);

    // Set the current values of the radio buttons
    if (ScreenLayout::screenPosition < 5)
        posBtns[ScreenLayout::screenPosition]->SetValue(true);
    if (ScreenLayout::screenRotation < 3)
        rotateBtns[ScreenLayout::screenRotation]->SetValue(true);
    if (ScreenLayout::screenArrangement < 4)
        arrangeBtns[ScreenLayout::screenArrangement]->SetValue(true);
    if (ScreenLayout::screenSizing < 3)
        sizeBtns[ScreenLayout::screenSizing]->SetValue(true);
    if (ScreenLayout::screenGap < 4)
        gapBtns[ScreenLayout::screenGap]->SetValue(true);

    // Set the current values of the checkboxes
    boxes[0]->SetValue(ScreenLayout::integerScale);
    boxes[1]->SetValue(ScreenLayout::gbaCrop);
    boxes[2]->SetValue(NooApp::screenFilter);

    // Set up the cancel and confirm buttons
    wxBoxSizer *buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    buttonSizer->Add(new wxStaticText(this, wxID_ANY, ""), 1);
    buttonSizer->Add(new wxButton(this, wxID_CANCEL, "Cancel"),  0, wxRIGHT, size / 16);
    buttonSizer->Add(new wxButton(this, wxID_OK,     "Confirm"), 0, wxLEFT,  size / 16);

    // Combine all of the contents
    wxBoxSizer *contents = new wxBoxSizer(wxVERTICAL);
    contents->Add(posSizer,     1, wxEXPAND);
    contents->Add(rotateSizer,  1, wxEXPAND);
    contents->Add(arrangeSizer, 1, wxEXPAND);
    contents->Add(sizeSizer,    1, wxEXPAND);
    contents->Add(gapSizer,     1, wxEXPAND);
    contents->Add(checkSizer,   1, wxEXPAND);
    contents->Add(buttonSizer,  1, wxEXPAND);

    // Add a final border around everything
    wxBoxSizer *sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(contents, 1, wxEXPAND | wxALL, size / 4);
    SetSizer(sizer);

    // Size the window to fit the contents and prevent resizing
    sizer->Fit(this);
    SetMinSize(GetSize());
    SetMaxSize(GetSize());
}

void LayoutDialog::posCenter(wxCommandEvent &event)
{
    // Set the screen position setting to center
    ScreenLayout::screenPosition = 0;
    app->updateLayouts();
}

void LayoutDialog::posTop(wxCommandEvent &event)
{
    // Set the screen position setting to top
    ScreenLayout::screenPosition = 1;
    app->updateLayouts();
}

void LayoutDialog::posBottom(wxCommandEvent &event)
{
    // Set the screen position setting to bottom
    ScreenLayout::screenPosition = 2;
    app->updateLayouts();
}

void LayoutDialog::posLeft(wxCommandEvent &event)
{
    // Set the screen position setting to left
    ScreenLayout::screenPosition = 3;
    app->updateLayouts();
}

void LayoutDialog::posRight(wxCommandEvent &event)
{
    // Set the screen position setting to right
    ScreenLayout::screenPosition = 4;
    app->updateLayouts();
}

void LayoutDialog::rotateNone(wxCommandEvent &event)
{
    // Set the screen rotation setting to none
    ScreenLayout::screenRotation = 0;
    app->updateLayouts();
}

void LayoutDialog::rotateCw(wxCommandEvent &event)
{
    // Set the screen rotation setting to clockwise
    ScreenLayout::screenRotation = 1;
    app->updateLayouts();
}

void LayoutDialog::rotateCcw(wxCommandEvent &event)
{
    // Set the screen rotation setting to counter-clockwise
    ScreenLayout::screenRotation = 2;
    app->updateLayouts();
}

void LayoutDialog::arrangeAuto(wxCommandEvent &event)
{
    // Set the screen arrangement setting to automatic
    ScreenLayout::screenArrangement = 0;
    app->updateLayouts();
}

void LayoutDialog::arrangeVert(wxCommandEvent &event)
{
    // Set the screen arrangement setting to vertical
    ScreenLayout::screenArrangement = 1;
    app->updateLayouts();
}

void LayoutDialog::arrangeHori(wxCommandEvent &event)
{
    // Set the screen arrangement setting to horizontal
    ScreenLayout::screenArrangement = 2;
    app->updateLayouts();
}

void LayoutDialog::arrangeSing(wxCommandEvent &event)
{
    // Set the screen arrangement setting to single screen
    ScreenLayout::screenArrangement = 3;
    app->updateLayouts();
}

void LayoutDialog::sizeEven(wxCommandEvent &event)
{
    // Set the screen sizing setting to even
    ScreenLayout::screenSizing = 0;
    app->updateLayouts();
}

void LayoutDialog::sizeTop(wxCommandEvent &event)
{
    // Set the screen sizing setting to enlarge top
    ScreenLayout::screenSizing = 1;
    app->updateLayouts();
}

void LayoutDialog::sizeBot(wxCommandEvent &event)
{
    // Set the screen sizing setting to enlarge bottom
    ScreenLayout::screenSizing = 2;
    app->updateLayouts();
}

void LayoutDialog::gapNone(wxCommandEvent &event)
{
    // Set the screen gap setting to none
    ScreenLayout::screenGap = 0;
    app->updateLayouts();
}

void LayoutDialog::gapQuart(wxCommandEvent &event)
{
    // Set the screen gap setting to quarter
    ScreenLayout::screenGap = 1;
    app->updateLayouts();
}

void LayoutDialog::gapHalf(wxCommandEvent &event)
{
    // Set the screen gap setting to half
    ScreenLayout::screenGap = 2;
    app->updateLayouts();
}

void LayoutDialog::gapFull(wxCommandEvent &event)
{
    // Set the screen gap setting to full
    ScreenLayout::screenGap = 3;
    app->updateLayouts();
}

void LayoutDialog::intScale(wxCommandEvent &event)
{
    // Toggle the integer scale setting
    ScreenLayout::integerScale = !ScreenLayout::integerScale;
    app->updateLayouts();
}

void LayoutDialog::gbaCrop(wxCommandEvent &event)
{
    // Toggle the GBA crop setting
    ScreenLayout::gbaCrop = !ScreenLayout::gbaCrop;
    app->updateLayouts();
}

void LayoutDialog::filter(wxCommandEvent &event)
{
    // Toggle the screen filter setting
    NooApp::screenFilter = !NooApp::screenFilter;
    app->updateLayouts();
}

void LayoutDialog::cancel(wxCommandEvent &event)
{
    // Reset the settings to their previous values
    ScreenLayout::screenRotation = prevSettings[0];
    ScreenLayout::screenArrangement = prevSettings[1];
    ScreenLayout::screenSizing = prevSettings[2];
    ScreenLayout::screenGap = prevSettings[3];
    ScreenLayout::integerScale = prevSettings[4];
    ScreenLayout::gbaCrop = prevSettings[5];
    NooApp::screenFilter = prevSettings[6];
    app->updateLayouts();

    event.Skip(true);
}

void LayoutDialog::confirm(wxCommandEvent &event)
{
    // Save the layout settings
    Settings::save();

    event.Skip(true);
}
