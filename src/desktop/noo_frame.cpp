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
#include "noo_app.h"
#include "path_dialog.h"
#include "save_dialog.h"
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
    DIRECT_BOOT,
    THREADED_3D,
    LIMIT_FPS,
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
EVT_MENU(DIRECT_BOOT,    NooFrame::directBootToggle)
EVT_MENU(THREADED_3D,    NooFrame::threaded3DToggle)
EVT_MENU(LIMIT_FPS,      NooFrame::limitFpsToggle)
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

    // Set up the Settings menu
    wxMenu *settingsMenu = new wxMenu();
    settingsMenu->Append(PATH_SETTINGS,  "&Path Settings");
    settingsMenu->Append(INPUT_BINDINGS, "&Input Bindings");
    settingsMenu->Append(SCREEN_LAYOUT,  "&Screen Layout");
    settingsMenu->AppendSeparator();
    settingsMenu->AppendCheckItem(DIRECT_BOOT, "&Direct Boot");
    settingsMenu->AppendCheckItem(THREADED_3D, "&Threaded 3D");
    settingsMenu->AppendCheckItem(LIMIT_FPS,   "&Limit FPS");

    // Set the current values of the checkboxes
    settingsMenu->Check(DIRECT_BOOT, Settings::getDirectBoot());
    settingsMenu->Check(THREADED_3D, Settings::getThreaded3D());
    settingsMenu->Check(LIMIT_FPS,   Settings::getLimitFps());

    // Set up the menu bar
    wxMenuBar *menuBar = new wxMenuBar();
    menuBar->Append(fileMenu,     "&File");
    menuBar->Append(systemMenu,   "&System");
    menuBar->Append(settingsMenu, "&Settings");
    SetMenuBar(menuBar);

    // Set the initial window size based on the current screen layout
    int width  = (NooApp::getScreenRotation() ? 192 : 256);
    int height = (NooApp::getScreenRotation() ? 256 : 192);
    if (NooApp::getScreenArrangement() == 1 || (NooApp::getScreenArrangement() == 0 && NooApp::getScreenRotation() == 0))
        SetClientSize(wxSize(width, height * 2));
    else
        SetClientSize(wxSize(width * 2, height));

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
        if (path == "")
            emulator->core = new Core();
        else
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
    wxFileDialog romSelect(this, "Select ROM File", "", "", "NDS ROM files (*.nds)|*.nds", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
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

void NooFrame::threaded3DToggle(wxCommandEvent &event)
{
    // Toggle the threaded 3D setting
    Settings::setThreaded3D(!Settings::getThreaded3D());
}

void NooFrame::limitFpsToggle(wxCommandEvent &event)
{
    // Toggle the limit FPS setting
    Settings::setLimitFps(!Settings::getLimitFps());
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
