/*
    Copyright 2019-2020 Hydr8gon

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

#include "portaudio.h"
#include <thread>
#include <wx/wx.h>
#include <wx/rawbmp.h>

#include "../core.h"
#include "../settings.h"

const char keyMap[] = { 'L', 'K', 'G', 'H', 'D', 'A', 'W', 'S', 'P', 'Q', 'O', 'I' };

Core *core;
std::thread *coreThread;
PaStream *stream;
bool running;

void runCore()
{
    while (running)
        core->runFrame();
}

int audioCallback(const void *in, void *out, unsigned long frames,
                  const PaStreamCallbackTimeInfo *info, PaStreamCallbackFlags flags, void *data)
{
    int16_t *curOut = (int16_t*)out;
    for (int i = 0; i < frames; i++)
    {
        uint32_t sample = running ? core->getSample() : 0;
        curOut[i * 2 + 0] = sample >>  0;
        curOut[i * 2 + 1] = sample >> 16;
    }
    return 0;
}

class PathDialog: public wxDialog
{
    public:
        PathDialog();

    private:
        wxTextCtrl *bios9Path;
        wxTextCtrl *bios7Path;
        wxTextCtrl *firmwarePath;

        void bios9Browse(wxCommandEvent &event);
        void bios7Browse(wxCommandEvent &event);
        void firmwareBrowse(wxCommandEvent &event);
        void confirm(wxCommandEvent &event);

        wxDECLARE_EVENT_TABLE();
};

class NooFrame: public wxFrame
{
    public:
        NooFrame();

    private:
        PathDialog pathDialog;

        void loadRom(wxCommandEvent &event);
        void bootFirmware(wxCommandEvent &event);
        void pathSettings(wxCommandEvent &event);
        void directBootToggle(wxCommandEvent &event);
        void limitFpsToggle(wxCommandEvent &event);
        void exit(wxCommandEvent &event);
        void stop(wxCloseEvent &event);

        wxDECLARE_EVENT_TABLE();
};

class NooPanel: public wxPanel
{
    public:
        NooPanel(wxFrame *parent);

    private:
        int x = 0, y = 0;
        float scale = 1.0f;
        bool needsClear = false;

        void clear(wxEraseEvent &event);
        void draw(wxPaintEvent &event);
        void resize(wxSizeEvent &event);
        void pressKey(wxKeyEvent &event);
        void releaseKey(wxKeyEvent &event);
        void pressScreen(wxMouseEvent &event);
        void releaseScreen(wxMouseEvent &event);

        wxDECLARE_EVENT_TABLE();
};

class NooApp: public wxApp
{
    private:
        NooFrame *frame;
        NooPanel *panel;

        bool OnInit();
        void requestDraw(wxIdleEvent &event);
};

wxIMPLEMENT_APP(NooApp);

wxBEGIN_EVENT_TABLE(NooFrame, wxFrame)
EVT_MENU(1, NooFrame::loadRom)
EVT_MENU(2, NooFrame::bootFirmware)
EVT_MENU(3, NooFrame::pathSettings)
EVT_MENU(4, NooFrame::directBootToggle)
EVT_MENU(5, NooFrame::limitFpsToggle)
EVT_MENU(wxID_EXIT, NooFrame::exit)
EVT_CLOSE(NooFrame::stop)
wxEND_EVENT_TABLE()

wxBEGIN_EVENT_TABLE(NooPanel, wxPanel)
EVT_ERASE_BACKGROUND(NooPanel::clear)
EVT_PAINT(NooPanel::draw)
EVT_SIZE(NooPanel::resize)
EVT_KEY_DOWN(NooPanel::pressKey)
EVT_KEY_UP(NooPanel::releaseKey)
EVT_LEFT_DOWN(NooPanel::pressScreen)
EVT_MOTION(NooPanel::pressScreen)
EVT_LEFT_UP(NooPanel::releaseScreen)
wxEND_EVENT_TABLE()

wxBEGIN_EVENT_TABLE(PathDialog, wxDialog)
EVT_BUTTON(1, PathDialog::bios9Browse)
EVT_BUTTON(2, PathDialog::bios7Browse)
EVT_BUTTON(3, PathDialog::firmwareBrowse)
EVT_BUTTON(wxID_OK, PathDialog::confirm)
wxEND_EVENT_TABLE()

bool NooApp::OnInit()
{
    // Load the settings
    Settings::load();

    // Set up the window
    frame = new NooFrame();
    panel = new NooPanel(frame);
    wxBoxSizer *sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(panel, 1, wxEXPAND);
    frame->SetSizer(sizer);
    Connect(wxID_ANY, wxEVT_IDLE, wxIdleEventHandler(NooApp::requestDraw));

    // Start the audio service
    Pa_Initialize();
    Pa_OpenDefaultStream(&stream, 0, 2, paInt16, 32768, 1024, audioCallback, NULL);
    Pa_StartStream(stream);

    return true;
}

void NooApp::requestDraw(wxIdleEvent &event)
{
    // Refresh the display
    panel->Refresh();
    event.RequestMore();

    // Update the FPS in the window title if the core is running
    frame->SetLabel(running ? wxString::Format(wxT("NooDS - %d FPS"), core->getFps()) : wxT("NooDS"));
}

NooPanel::NooPanel(wxFrame *parent): wxPanel(parent)
{
    // Set the panel size and set focus for reading input
    SetSize(256, 192 * 2);
    SetFocus();
}

void NooPanel::clear(wxEraseEvent &event)
{
    // Clearing the screen can cause flickering, so only do it when necessary (on resize)
    if (needsClear)
    {
        needsClear = false;
        event.Skip(true);
    }
}

void NooPanel::draw(wxPaintEvent &event)
{
    wxBitmap bmp(256, 192 * 2, 24);
    wxNativePixelData data(bmp);
    wxNativePixelData::Iterator iter(data);

    // Copy the framebuffer to a bitmap
    for (int y = 0; y < 192 * 2; y++)
    {
        wxNativePixelData::Iterator pixel = iter;
        for (int x = 0; x < 256; x++, pixel++)
        {
            // Convert the color values from 6-bit to 8-bit
            uint32_t color = core ? core->getFramebuffer()[y * 256 + x] : 0;
            pixel.Red()   = ((color >>  0) & 0x3F) * 255 / 63;
            pixel.Green() = ((color >>  6) & 0x3F) * 255 / 63;
            pixel.Blue()  = ((color >> 12) & 0x3F) * 255 / 63;
        }
        iter.OffsetY(data, 1);
    }

    // Draw the bitmap
    wxPaintDC dc(this);
    dc.SetUserScale(scale, scale);
    dc.DrawBitmap(bmp, wxPoint(x, y));
}

void NooPanel::resize(wxSizeEvent &event)
{
    // Scale the display based on the aspect ratio of the window
    // If the window is wider than the DS ratio, scale to the height of the window
    // If the window is taller than the DS ratio, scale to the width of the window
    wxSize size = GetSize();
    float ratio = 256.0f / (192 * 2);
    float window = (float)size.x / size.y;
    scale = ((ratio >= window) ? (float)size.x / 256 : (float)size.y / (192 * 2));

    // Center the display
    x = ((float)size.x / scale - 256)     / 2;
    y = ((float)size.y / scale - 192 * 2) / 2;

    needsClear = true;
}

void NooPanel::pressKey(wxKeyEvent &event)
{
    if (!core) return;

    // Send a key press to the core
    for (int i = 0; i < 12; i++)
    {
        if (event.GetKeyCode() == keyMap[i])
            core->pressKey(i);
    }
}

void NooPanel::releaseKey(wxKeyEvent &event)
{
    if (!core) return;

    // Send a key release to the core
    for (int i = 0; i < 12; i++)
    {
        if (event.GetKeyCode() == keyMap[i])
            core->releaseKey(i);
    }
}

void NooPanel::pressScreen(wxMouseEvent &event)
{
    // Ensure the left mouse button is clicked
    if (!core || !event.LeftIsDown()) return;

    // Determine the touch position relative to the emulated touch screen
    int touchX = (float)event.GetX() / scale - x;
    int touchY = (float)event.GetY() / scale - y - 192;

    // Send the touch coordinates to the core
    core->pressScreen(touchX, touchY);
}

void NooPanel::releaseScreen(wxMouseEvent &event)
{
    // Send a touch release to the core
    if (core) core->releaseScreen();
}

NooFrame::NooFrame(): wxFrame(NULL, wxID_ANY, "", wxDefaultPosition, wxSize(256, 192 * 2), wxDEFAULT_FRAME_STYLE | wxWANTS_CHARS)
{
    // Set up the File menu
    wxMenu *fileMenu = new wxMenu();
    fileMenu->Append(1, "&Load ROM");
    fileMenu->Append(2, "&Boot Firmware");
    fileMenu->AppendSeparator();
    fileMenu->Append(wxID_EXIT);

    // Set up the Settings menu
    wxMenu *settingsMenu = new wxMenu();
    settingsMenu->Append(3, "&Path Settings");
    settingsMenu->AppendSeparator();
    settingsMenu->AppendCheckItem(4, "&Direct Boot");
    settingsMenu->Check(4, Settings::getDirectBoot());
    settingsMenu->AppendCheckItem(5, "&Limit FPS");
    settingsMenu->Check(5, Settings::getLimitFps());

    // Set up the menu bar
    wxMenuBar *menuBar = new wxMenuBar();
    menuBar->Append(fileMenu, "&File");
    menuBar->Append(settingsMenu, "&Settings");
    SetMenuBar(menuBar);

    // Prevent resizing smaller than the DS resolution
    SetClientSize(wxSize(256, 192 * 2));
    SetMinSize(GetSize());

    Centre();
    Show(true);
}

void NooFrame::loadRom(wxCommandEvent &event)
{
    // Show the file browser
    wxFileDialog romSelect(this, "Select ROM File", "", "", "NDS ROM files (*.nds)|*.nds", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (romSelect.ShowModal() == wxID_CANCEL)
        return;

    // Ensure the core thread is stopped
    if (coreThread)
    {
        running = false;
        coreThread->join();
        delete coreThread;
    }

    // Get the ROM path
    char path[1024];
    strncpy(path, (const char*)romSelect.GetPath().mb_str(wxConvUTF8), 1023);

    // Attempt to boot the ROM
    try
    {
        if (core) delete core;
        core = new Core(path);
    }
    catch (std::exception *e)
    {
        wxMessageBox("Initialization failed. Make sure you have BIOS files named 'bios9.bin' and 'bios7.bin' "
                     "and a firmware file named 'firmware.bin' placed in the same directory as the emulator.");
        return;
    }

    // Start the core thread
    running = true;
    coreThread = new std::thread(runCore);
}

void NooFrame::bootFirmware(wxCommandEvent &event)
{
    // Ensure the core thread is stopped
    if (coreThread)
    {
        running = false;
        coreThread->join();
        delete coreThread;
    }

    // Attempt to boot the firmware
    try
    {
        if (core) delete core;
        core = new Core();
    }
    catch (std::exception *e)
    {
        wxMessageBox("Initialization failed. Make sure you have BIOS files named 'bios9.bin' and 'bios7.bin' "
                     "and a firmware file named 'firmware.bin' placed in the same directory as the emulator.");
        return;
    }

    // Start the core thread
    running = true;
    coreThread = new std::thread(runCore);
}

void NooFrame::pathSettings(wxCommandEvent &event)
{
    // Show the path settings dialog
    pathDialog.Centre();
    pathDialog.Show(true);
}

void NooFrame::directBootToggle(wxCommandEvent &event)
{
    // Toggle the "Direct Boot" option
    Settings::setDirectBoot(!Settings::getDirectBoot());
}

void NooFrame::limitFpsToggle(wxCommandEvent &event)
{
    // Toggle the "Limit FPS" option
    Settings::setLimitFps(!Settings::getLimitFps());
}

void NooFrame::exit(wxCommandEvent &event)
{
    // Close the program
    Close(true);
}

void NooFrame::stop(wxCloseEvent &event)
{
    // Ensure the core thread is stopped
    if (coreThread)
    {
        running = false;
        coreThread->join();
        delete coreThread;
    }

    // Close the core to ensure the save gets written
    if (core) delete core;
    core = nullptr;

    // Save the settings
    Settings::save();

    event.Skip(true);
}

PathDialog::PathDialog(): wxDialog(NULL, wxID_ANY, "Path Settings", wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE)
{
    // Determine the height of a text box
    // Borders are measured in pixels, so this value can be used to make values that scale with the DPI/font size
    wxTextCtrl *dummy = new wxTextCtrl(this, wxID_ANY, "");
    int size = dummy->GetSize().y;
    delete dummy;

    // Set up the ARM9 BIOS path setting
    wxBoxSizer *arm9Sizer = new wxBoxSizer(wxHORIZONTAL);
    arm9Sizer->Add(new wxStaticText(this, wxID_ANY, "ARM9 BIOS:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 8);
    arm9Sizer->Add(bios9Path = new wxTextCtrl(this, wxID_ANY, Settings::getBios9Path(), wxDefaultPosition, wxSize(size * 5, size)), 0);
    arm9Sizer->Add(new wxButton(this, 1, "Browse"), 0, wxLEFT, size / 8);

    // Set up the ARM7 BIOS path setting
    wxBoxSizer *arm7Sizer = new wxBoxSizer(wxHORIZONTAL);
    arm7Sizer->Add(new wxStaticText(this, wxID_ANY, "ARM7 BIOS:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 8);
    arm7Sizer->Add(bios7Path = new wxTextCtrl(this, wxID_ANY, Settings::getBios7Path(), wxDefaultPosition, wxSize(size * 5, size)), 0);
    arm7Sizer->Add(new wxButton(this, 2, "Browse"), 0, wxLEFT, size / 8);

    // Set up the firmware path setting
    wxBoxSizer *firmSizer = new wxBoxSizer(wxHORIZONTAL);
    firmSizer->Add(new wxStaticText(this, wxID_ANY, "Firmware:"), 1, wxALIGN_CENTRE | wxRIGHT, size / 8);
    firmSizer->Add(firmwarePath = new wxTextCtrl(this, wxID_ANY, Settings::getFirmwarePath(), wxDefaultPosition, wxSize(size * 5, size)), 0);
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

    Show(false);
}
