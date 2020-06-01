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

#include "noo_frame.h"
#include "input_dialog.h"
#include "layout_dialog.h"
#include "path_dialog.h"
#include "save_dialog.h"
#include "../common/screen_layout.h"
#include "../settings.h"

enum Event
{
    LOAD_ROM = 1,
    BOOT_FIRMWARE,
    PAUSE,
    RESTART,
    STOP,
    PATH_SETTINGS,
    INPUT_BINDINGS,
    SCREEN_LAYOUT,
    FPS_DISABLED,
    FPS_LIGHT,
    FPS_ACCURATE,
    THREADED_3D_0,
    THREADED_3D_1,
    THREADED_3D_2,
    THREADED_3D_3,
    THREADED_3D_4,
    DIRECT_BOOT,
    UPDATE_FPS
};

wxBEGIN_EVENT_TABLE(NooFrame, wxFrame)
EVT_MENU(LOAD_ROM,       NooFrame::loadRom)
EVT_MENU(BOOT_FIRMWARE,  NooFrame::bootFirmware)
EVT_MENU(PAUSE,          NooFrame::pause)
EVT_MENU(RESTART,        NooFrame::restart)
EVT_MENU(STOP,           NooFrame::stop)
EVT_MENU(PATH_SETTINGS,  NooFrame::pathSettings)
EVT_MENU(INPUT_BINDINGS, NooFrame::inputSettings)
EVT_MENU(SCREEN_LAYOUT,  NooFrame::layoutSettings)
EVT_MENU(FPS_DISABLED,   NooFrame::fpsDisabled)
EVT_MENU(FPS_LIGHT,      NooFrame::fpsLight)
EVT_MENU(FPS_ACCURATE,   NooFrame::fpsAccurate)
EVT_MENU(THREADED_3D_0,  NooFrame::threaded3D0)
EVT_MENU(THREADED_3D_1,  NooFrame::threaded3D1)
EVT_MENU(THREADED_3D_2,  NooFrame::threaded3D2)
EVT_MENU(THREADED_3D_3,  NooFrame::threaded3D3)
EVT_MENU(THREADED_3D_4,  NooFrame::threaded3D4)
EVT_MENU(DIRECT_BOOT,    NooFrame::directBootToggle)
EVT_MENU(wxID_EXIT,      NooFrame::exit)
EVT_CLOSE(NooFrame::close)
wxEND_EVENT_TABLE()

NooFrame::NooFrame(Emulator *emulator, std::string path): wxFrame(nullptr, wxID_ANY, "NooDS"), emulator(emulator), path(path)
{
    // Set up the File menu
    wxMenu *fileMenu = new wxMenu();
    fileMenu->Append(LOAD_ROM,      "&Load ROM");
    fileMenu->Append(BOOT_FIRMWARE, "&Boot Firmware");
    fileMenu->AppendSeparator();
    fileMenu->Append(wxID_EXIT);

    // Set up the System menu
    systemMenu = new wxMenu();
    systemMenu->Append(PAUSE,   "&Resume");
    systemMenu->Append(RESTART, "&Restart");
    systemMenu->Append(STOP,    "&Stop");

    // Disable the System menu items until the core is running
    systemMenu->Enable(PAUSE,   false);
    systemMenu->Enable(RESTART, false);
    systemMenu->Enable(STOP,    false);

    // Set up the FPS Limiter submenu
    wxMenu *fpsLimiter = new wxMenu();
    fpsLimiter->AppendRadioItem(FPS_DISABLED, "&Disabled");
    fpsLimiter->AppendRadioItem(FPS_LIGHT,    "&Light");
    fpsLimiter->AppendRadioItem(FPS_ACCURATE, "&Accurate");

    // Set the current value of the FPS limiter setting
    switch (Settings::getFpsLimiter())
    {
        case 0:  fpsLimiter->Check(FPS_DISABLED, true); break;
        case 1:  fpsLimiter->Check(FPS_LIGHT,    true); break;
        default: fpsLimiter->Check(FPS_ACCURATE, true); break;
    }

    // Set up the Threaded 3D submenu
    wxMenu *threaded3D = new wxMenu();
    threaded3D->AppendRadioItem(THREADED_3D_0, "&Disabled");
    threaded3D->AppendRadioItem(THREADED_3D_1, "&1 Thread");
    threaded3D->AppendRadioItem(THREADED_3D_2, "&2 Threads");
    threaded3D->AppendRadioItem(THREADED_3D_3, "&3 Threads");
    threaded3D->AppendRadioItem(THREADED_3D_4, "&4 Threads");

    // Set the current value of the threaded 3D setting
    switch (Settings::getThreaded3D())
    {
        case 0:  threaded3D->Check(THREADED_3D_0, true); break;
        case 1:  threaded3D->Check(THREADED_3D_1, true); break;
        case 2:  threaded3D->Check(THREADED_3D_2, true); break;
        case 3:  threaded3D->Check(THREADED_3D_3, true); break;
        default: threaded3D->Check(THREADED_3D_4, true); break;
    }

    // Set up the Settings menu
    wxMenu *settingsMenu = new wxMenu();
    settingsMenu->Append(PATH_SETTINGS,  "&Path Settings");
    settingsMenu->Append(INPUT_BINDINGS, "&Input Bindings");
    settingsMenu->Append(SCREEN_LAYOUT,  "&Screen Layout");
    settingsMenu->AppendSeparator();
    settingsMenu->AppendSubMenu(fpsLimiter, "&FPS Limiter");
    settingsMenu->AppendSubMenu(threaded3D, "&Threaded 3D");
    settingsMenu->AppendSeparator();
    settingsMenu->AppendCheckItem(DIRECT_BOOT, "&Direct Boot");

    // Set the current value of the direct boot setting
    settingsMenu->Check(DIRECT_BOOT, Settings::getDirectBoot());

    // Set up the menu bar
    wxMenuBar *menuBar = new wxMenuBar();
    menuBar->Append(fileMenu,     "&File");
    menuBar->Append(systemMenu,   "&System");
    menuBar->Append(settingsMenu, "&Settings");
    SetMenuBar(menuBar);

    // Set the initial window size based on the current screen layout
    ScreenLayout layout;
    layout.update(0, 0, false);
    SetClientSize(wxSize(layout.getMinWidth(), layout.getMinHeight()));

    SetBackgroundColour(*wxBLACK);

    Centre();
    Show(true);

    // Start the core right away if a filename was passed through the command line
    if (path != "") startCore();
}

void NooFrame::runCore()
{
    // Run the emulator
    while (emulator->running)
        emulator->core->runFrame();
}

void NooFrame::startCore()
{
    // Ensure the core is stopped
    stopCore();

    try
    {
        // Attempt to boot the ROM
        emulator->core = new Core(path);
    }
    catch (int e)
    {
        // Handle errors during ROM boot
        switch (e)
        {
            case 1: // Missing BIOS and/or firmware files
            {
                // Inform the user of the error
                wxMessageBox("Initialization failed. Make sure the path settings point to valid BIOS and firmware files and try again.");
                return;
            }

            case 2: // Unreadable ROM file
            {
                // Inform the user of the error
                wxMessageBox("Initialization failed. Make sure the ROM file is accessible and try again.");
                return;
            }

            case 3: // Missing save file
            {
                // Show the save dialog and boot the ROM again if a save was created
                SaveDialog saveDialog(path);
                if (saveDialog.ShowModal() == wxID_OK)
                {
                    emulator->core = new Core(path);
                    break;
                }
                return;
            }
        }
    }

    // Start the core and enable the System menu items
    emulator->running = true;
    systemMenu->Enable(PAUSE,   true);
    systemMenu->Enable(RESTART, true);
    systemMenu->Enable(STOP,    true);
    systemMenu->SetLabel(PAUSE, "&Pause");

    // Start the core thread
    coreThread = new std::thread(&NooFrame::runCore, this);
}

void NooFrame::stopCore()
{
    // Stop the core and disable the System menu items
    emulator->running = false;
    systemMenu->Enable(PAUSE,   false);
    systemMenu->Enable(RESTART, false);
    systemMenu->Enable(STOP,    false);
    systemMenu->SetLabel(PAUSE, "&Resume");

    // Ensure the core thread is stopped
    if (coreThread)
    {
        coreThread->join();
        delete coreThread;
        coreThread = nullptr;
    }

    // Close the core to ensure the save gets written
    if (emulator->core) delete emulator->core;
    emulator->core = nullptr;
}

void NooFrame::loadRom(wxCommandEvent &event)
{
    // Show the file browser
    wxFileDialog romSelect(this, "Select ROM File", "", "", "NDS/GBA ROM files (*.nds, *.gba)|*.nds;*.gba", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (romSelect.ShowModal() == wxID_CANCEL)
        return;

    // Set the ROM path and start the core
    path = (const char*)romSelect.GetPath().mb_str(wxConvUTF8);
    startCore();
}

void NooFrame::bootFirmware(wxCommandEvent &event)
{
    // Start the core with no ROM
    path = "";
    startCore();
}

void NooFrame::pause(wxCommandEvent &event)
{
    if (emulator->running) // Pause
    {
        // Stop the core thread without closing the core
        emulator->running = false;
        coreThread->join();
        delete coreThread;
        coreThread = nullptr;

        // Update the menu item
        systemMenu->SetLabel(PAUSE, "&Resume");
    }
    else if (emulator->core) // Resume
    {
        // Start the core thread
        emulator->running = true;
        coreThread = new std::thread(&NooFrame::runCore, this);

        // Update the menu item
        systemMenu->SetLabel(PAUSE, "&Pause");
    }
}

void NooFrame::restart(wxCommandEvent &event)
{
    // Restart the core
    startCore();
}

void NooFrame::stop(wxCommandEvent &event)
{
    // Stop the core
    stopCore();
}

void NooFrame::pathSettings(wxCommandEvent &event)
{
    // Show the path settings dialog
    PathDialog pathDialog;
    pathDialog.ShowModal();
}

void NooFrame::inputSettings(wxCommandEvent &event)
{
    // Show the input settings dialog
    InputDialog inputDialog;
    inputDialog.ShowModal();
}

void NooFrame::layoutSettings(wxCommandEvent &event)
{
    // Show the layout settings dialog
    LayoutDialog layoutDialog(this);
    layoutDialog.ShowModal();
}

void NooFrame::directBootToggle(wxCommandEvent &event)
{
    // Toggle the direct boot setting
    Settings::setDirectBoot(!Settings::getDirectBoot());
}

void NooFrame::fpsDisabled(wxCommandEvent &event)
{
    // Set the FPS limiter setting to disabled
    Settings::setFpsLimiter(0);
}

void NooFrame::fpsLight(wxCommandEvent &event)
{
    // Set the FPS limiter setting to light
    Settings::setFpsLimiter(1);
}

void NooFrame::fpsAccurate(wxCommandEvent &event)
{
    // Set the FPS limiter setting to accurate
    Settings::setFpsLimiter(2);
}

void NooFrame::threaded3D0(wxCommandEvent &event)
{
    // Set the threaded 3D setting to disabled
    Settings::setThreaded3D(0);
}

void NooFrame::threaded3D1(wxCommandEvent &event)
{
    // Set the threaded 3D setting to 1 thread
    Settings::setThreaded3D(1);
}

void NooFrame::threaded3D2(wxCommandEvent &event)
{
    // Set the threaded 3D setting to 2 threads
    Settings::setThreaded3D(2);
}

void NooFrame::threaded3D3(wxCommandEvent &event)
{
    // Set the threaded 3D setting to 3 threads
    Settings::setThreaded3D(3);
}

void NooFrame::threaded3D4(wxCommandEvent &event)
{
    // Set the threaded 3D setting to 4 threads
    Settings::setThreaded3D(4);
}

void NooFrame::exit(wxCommandEvent &event)
{
    // Close the program
    Close(true);
}

void NooFrame::close(wxCloseEvent &event)
{
    // Properly shut down the emulator
    stopCore();
    Settings::save();

    event.Skip(true);
}
