/*
    Copyright 2019 Hydr8gon

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

#include <thread>
#include <wx/wx.h>
#include <wx/rawbmp.h>

#include "../core.h"

const char keyMap[] = { 'L', 'K', 'G', 'H', 'D', 'A', 'W', 'S', 'P', 'Q', 'O', 'I' };

Core *core;
std::thread *coreThread;
bool running;

void runCore()
{
    while (running)
        core->runFrame();
}

class NooFrame: public wxFrame
{
    public:
        NooFrame();

    private:
        void loadRom(wxCommandEvent &event);
        void bootFirmware(wxCommandEvent &event);
        void exit(wxCommandEvent &event);
        void stop(wxCloseEvent &event);

        wxDECLARE_EVENT_TABLE();
};

class NooPanel: public wxPanel
{
    public:
        NooPanel(wxFrame *parent);

        void draw(bool clear);

    private:
        int x, y;
        float scale;

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
EVT_MENU(0, NooFrame::loadRom)
EVT_MENU(1, NooFrame::bootFirmware)
EVT_MENU(wxID_EXIT, NooFrame::exit)
EVT_CLOSE(NooFrame::stop)
wxEND_EVENT_TABLE()

wxBEGIN_EVENT_TABLE(NooPanel, wxPanel)
EVT_SIZE(NooPanel::resize)
EVT_KEY_DOWN(NooPanel::pressKey)
EVT_KEY_UP(NooPanel::releaseKey)
EVT_LEFT_DOWN(NooPanel::pressScreen)
EVT_MOTION(NooPanel::pressScreen)
EVT_LEFT_UP(NooPanel::releaseScreen)
wxEND_EVENT_TABLE()

bool NooApp::OnInit()
{
    // Set up the window
    frame = new NooFrame();
    panel = new NooPanel(frame);
    wxBoxSizer *sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(panel, 1, wxEXPAND);     
    frame->SetSizer(sizer);
    Connect(wxID_ANY, wxEVT_IDLE, wxIdleEventHandler(NooApp::requestDraw));

    return true;
}

void NooApp::requestDraw(wxIdleEvent &event)
{
    // Refresh the display
    panel->draw(false);
    event.RequestMore();

    // Update the FPS in the window title if the core is running
    frame->SetLabel(running ? wxString::Format(wxT("NooDS - %d FPS"), core->getFps()) : wxT("NooDS"));
}

NooFrame::NooFrame(): wxFrame(NULL, wxID_ANY, "", wxPoint(50, 50), wxSize(256, 192 * 2), wxDEFAULT_FRAME_STYLE | wxWANTS_CHARS)
{
    // Set up the File menu
    wxMenu *fileMenu = new wxMenu();
    fileMenu->Append(0, "&Load ROM");
    fileMenu->Append(1, "&Boot Firmware");
    fileMenu->AppendSeparator();
    fileMenu->Append(wxID_EXIT);

    // Set up the menu bar
    wxMenuBar *menuBar = new wxMenuBar();
    menuBar->Append(fileMenu, "&File");
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

    event.Skip(true);
}

NooPanel::NooPanel(wxFrame *parent): wxPanel(parent)
{
    // Set the panel size and set focus for reading input
    SetSize(256, 192 * 2);
    SetFocus();
}

void NooPanel::draw(bool clear)
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
            // Convert the color values from 5-bit to 8-bit
            uint16_t color = core ? core->getFramebuffer()[y * 256 + x] : 0;
            pixel.Red()   = ((color >>  0) & 0x1F) * 255 / 31;
            pixel.Green() = ((color >>  5) & 0x1F) * 255 / 31;
            pixel.Blue()  = ((color >> 10) & 0x1F) * 255 / 31;
        }
        iter.OffsetY(data, 1);
    }

    // Draw the bitmap
    // Clearing can cause flickering, so only do it when necessary (on resize)
    wxClientDC dc(this);
    if (clear) dc.Clear();
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

    draw(true);
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
    if (!core || (!event.LeftDown() && !event.Dragging()))
        return;

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
