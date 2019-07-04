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
#include "../gpu.h"

const char keyMap[] = { 'L', 'K', 'G', 'H', 'D', 'A', 'W', 'S', 'P', 'Q', 'O', 'I' };

std::thread *coreThread;
bool running;

void runCore()
{
    while (running)
        core::runScanline();
}

class DisplayPanel: public wxPanel
{
public:
    DisplayPanel(wxFrame *parent);
    void draw(bool clear);

private:
    int x, y;
    float scale;
    void resize(wxSizeEvent &event);
    void pressKey(wxKeyEvent &event);
    void releaseKey(wxKeyEvent &event);
    wxDECLARE_EVENT_TABLE();
};

class NooDS: public wxApp
{
private:
    DisplayPanel *panel;
    bool OnInit();
    void requestDraw(wxIdleEvent &event);
};

class EmuFrame: public wxFrame
{
public:
    EmuFrame();

private:
    void openRom(wxCommandEvent &event);
    void bootFirmware(wxCommandEvent &event);
    void exit(wxCommandEvent &event);
    wxDECLARE_EVENT_TABLE();
};

wxIMPLEMENT_APP(NooDS);

wxBEGIN_EVENT_TABLE(EmuFrame, wxFrame)
EVT_MENU(0,         EmuFrame::openRom)
EVT_MENU(1,         EmuFrame::bootFirmware)
EVT_MENU(wxID_EXIT, EmuFrame::exit)
wxEND_EVENT_TABLE()

wxBEGIN_EVENT_TABLE(DisplayPanel, wxPanel)
EVT_SIZE(DisplayPanel::resize)
EVT_KEY_DOWN(DisplayPanel::pressKey)
EVT_KEY_UP(DisplayPanel::releaseKey)
wxEND_EVENT_TABLE()

bool NooDS::OnInit()
{
#ifndef _WIN32
    if (argc == 1 || argv[1] != "-v")
        fclose(stdout);
#endif

    EmuFrame *frame = new EmuFrame();
    panel = new DisplayPanel(frame);
    wxBoxSizer *sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(panel, 1, wxEXPAND);     
    frame->SetSizer(sizer);
    Connect(wxID_ANY, wxEVT_IDLE, wxIdleEventHandler(NooDS::requestDraw));
    return true;
}

void NooDS::requestDraw(wxIdleEvent &event)
{
    panel->draw(false);
    event.RequestMore();
}

EmuFrame::EmuFrame(): wxFrame(NULL, wxID_ANY, "NooDS", wxPoint(50, 50), wxSize(256, 192 * 2), wxDEFAULT_FRAME_STYLE | wxWANTS_CHARS)
{
    wxMenu *fileMenu = new wxMenu();
    fileMenu->Append(0, "&Open ROM");
    fileMenu->Append(1, "&Boot Firmware");
    fileMenu->AppendSeparator();
    fileMenu->Append(wxID_EXIT);

    wxMenuBar *menuBar = new wxMenuBar();
    menuBar->Append(fileMenu, "&File");
    SetMenuBar(menuBar);

    SetClientSize(wxSize(256, 192 * 2));
    SetMinSize(GetSize());
    Centre();
    Show(true);
}

void EmuFrame::openRom(wxCommandEvent &event)
{
    wxFileDialog romSelect(this, "Open ROM File", "", "", "NDS ROM files (*.nds)|*.nds", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (romSelect.ShowModal() == wxID_CANCEL)
        return;

    if (coreThread)
    {
        running = false;
        coreThread->join();
        delete coreThread;
    }

    if (!core::init())
    {
        wxMessageBox("Initialization failed. Make sure you have BIOS files named 'bios9.bin' and 'bios7.bin' "
                     "and a firmware file named 'firmware.bin' placed in the same directory as the emulator.");
        return;
    }

    char path[1024];
    strncpy(path, (const char*)romSelect.GetPath().mb_str(wxConvUTF8), 1023);
    if (!core::loadRom(path))
    {
        wxMessageBox("ROM loading failed. Make sure the file is accessible.");
        return;
    }

    running = true;
    coreThread = new std::thread(runCore);
}

void EmuFrame::bootFirmware(wxCommandEvent &event)
{
    if (coreThread)
    {
        running = false;
        coreThread->join();
        delete coreThread;
    }

    if (!core::init())
    {
        wxMessageBox("Initialization failed. Make sure you have BIOS files named 'bios9.bin' and 'bios7.bin' "
                     "and a firmware file named 'firmware.bin' placed in the same directory as the emulator.");
        return;
    }

    running = true;
    coreThread = new std::thread(runCore);
}

void EmuFrame::exit(wxCommandEvent &event)
{
    Close(true);
}


DisplayPanel::DisplayPanel(wxFrame *parent): wxPanel(parent)
{
    SetSize(256, 192 * 2);
    SetFocus();
}

void DisplayPanel::draw(bool clear)
{
    wxBitmap bmp(256, 192 * 2, 24);
    wxNativePixelData data(bmp);
    wxNativePixelData::Iterator iter(data);

    for (int y = 0; y < 192 * 2; y++)
    {
        wxNativePixelData::Iterator pixel = iter;
        for (int x = 0; x < 256; x++, pixel++)
        {
            uint16_t color = gpu::displayBuffer[y * 256 + x];
            pixel.Red()   = ((color >>  0) & 0x1F) * 255 / 31;
            pixel.Green() = ((color >>  5) & 0x1F) * 255 / 31;
            pixel.Blue()  = ((color >> 10) & 0x1F) * 255 / 31;
        }
        iter.OffsetY(data, 1);
    }

    wxClientDC dc(this);
    if (clear) dc.Clear();
    dc.SetUserScale(scale, scale);
    dc.DrawBitmap(bmp, wxPoint(x, y));
}

void DisplayPanel::resize(wxSizeEvent &event)
{
    wxSize size = GetSize();
    float ratio = 256.0f / (192 * 2);
    float window = (float)GetSize().x / GetSize().y;
    scale = ((ratio >= window) ? (float)size.x / 256 : (float)size.y / (192 * 2));
    x = ((float)size.x / scale - 256)     / 2;
    y = ((float)size.y / scale - 192 * 2) / 2;
    draw(true);
}

void DisplayPanel::pressKey(wxKeyEvent &event)
{
    for (int i = 0; i < 12; i++)
    {
        if (event.GetKeyCode() == keyMap[i])
            core::pressKey(i);
    }
}

void DisplayPanel::releaseKey(wxKeyEvent &event)
{
    for (int i = 0; i < 12; i++)
    {
        if (event.GetKeyCode() == keyMap[i])
            core::releaseKey(i);
    }
}
