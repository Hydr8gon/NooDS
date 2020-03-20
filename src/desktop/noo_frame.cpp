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
#include "path_dialog.h"
#include "save_dialog.h"
#include "../settings.h"

wxBEGIN_EVENT_TABLE(NooFrame, wxFrame)
EVT_MENU(1,  NooFrame::loadRom)
EVT_MENU(2,  NooFrame::bootFirmware)
EVT_MENU(3,  NooFrame::pause)
EVT_MENU(4,  NooFrame::restart)
EVT_MENU(5,  NooFrame::stop)
EVT_MENU(6,  NooFrame::pathSettings)
EVT_MENU(7,  NooFrame::inputSettings)
EVT_MENU(8,  NooFrame::directBootToggle)
EVT_MENU(9,  NooFrame::threaded3DToggle)
EVT_MENU(10, NooFrame::limitFpsToggle)
EVT_MENU(wxID_EXIT, NooFrame::exit)
EVT_CLOSE(NooFrame::close)
wxEND_EVENT_TABLE()

NooFrame::NooFrame(Emulator *emulator): wxFrame(nullptr, wxID_ANY, "NooDS"), emulator(emulator)
{
    // Set up the File menu
    wxMenu *fileMenu = new wxMenu();
    fileMenu->Append(1, "&Load ROM");
    fileMenu->Append(2, "&Boot Firmware");
    fileMenu->AppendSeparator();
    fileMenu->Append(wxID_EXIT);

    // Set up the System menu
    systemMenu = new wxMenu();
    systemMenu->Append(3, "&Resume");
    systemMenu->Append(4, "&Restart");
    systemMenu->Append(5, "&Stop");

    // Disable the System menu items until the core is running
    systemMenu->Enable(3, false);
    systemMenu->Enable(4, false);
    systemMenu->Enable(5, false);

    // Set up the Settings menu
    wxMenu *settingsMenu = new wxMenu();
    settingsMenu->Append(6, "&Path Settings");
    settingsMenu->Append(7, "&Input Settings");
    settingsMenu->AppendSeparator();
    settingsMenu->AppendCheckItem(8,  "&Direct Boot");
    settingsMenu->AppendCheckItem(9,  "&Threaded 3D");
    settingsMenu->AppendCheckItem(10, "&Limit FPS");

    // Set the current values of the checkboxes
    settingsMenu->Check(8,  Settings::getDirectBoot());
    settingsMenu->Check(9,  Settings::getThreaded3D());
    settingsMenu->Check(10, Settings::getLimitFps());

    // Set up the menu bar
    wxMenuBar *menuBar = new wxMenuBar();
    menuBar->Append(fileMenu, "&File");
    menuBar->Append(systemMenu, "&System");
    menuBar->Append(settingsMenu, "&Settings");
    SetMenuBar(menuBar);

    // Prevent resizing smaller than the DS resolution
    SetClientSize(wxSize(256, 192 * 2));
    SetMinSize(GetSize());

    Centre();
    Show(true);
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
    systemMenu->Enable(3, true);
    systemMenu->Enable(4, true);
    systemMenu->Enable(5, true);
    systemMenu->SetLabel(3, "&Pause");

    // Start the core thread
    coreThread = new std::thread(&NooFrame::runCore, this);
}

void NooFrame::stopCore()
{
    // Stop the core and disable the System menu items
    emulator->running = false;
    systemMenu->Enable(3, false);
    systemMenu->Enable(4, false);
    systemMenu->Enable(5, false);
    systemMenu->SetLabel(3, "&Resume");

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
        systemMenu->SetLabel(3, "&Resume");
    }
    else if (emulator->core) // Resume
    {
        // Start the core thread
        emulator->running = true;
        coreThread = new std::thread(&NooFrame::runCore, this);

        // Update the menu item
        systemMenu->SetLabel(3, "&Pause");
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

void NooFrame::directBootToggle(wxCommandEvent &event)
{
    // Toggle the "Direct Boot" option
    Settings::setDirectBoot(!Settings::getDirectBoot());
}

void NooFrame::threaded3DToggle(wxCommandEvent &event)
{
    // Toggle the "Threaded 3D" option
    Settings::setThreaded3D(!Settings::getThreaded3D());
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

void NooFrame::close(wxCloseEvent &event)
{
    // Properly shut down the emulator
    stopCore();
    Settings::save();

    event.Skip(true);
}
