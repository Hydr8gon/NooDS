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
    DIRECT_BOOT,
    FPS_DISABLED,
    FPS_LIGHT,
    FPS_ACCURATE,
    THREADED_2D,
    THREADED_3D_0,
    THREADED_3D_1,
    THREADED_3D_2,
    THREADED_3D_3,
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
EVT_MENU(FPS_DISABLED,   NooFrame::fpsDisabled)
EVT_MENU(FPS_LIGHT,      NooFrame::fpsLight)
EVT_MENU(FPS_ACCURATE,   NooFrame::fpsAccurate)
EVT_MENU(THREADED_2D,    NooFrame::threaded2D)
EVT_MENU(THREADED_3D_0,  NooFrame::threaded3D0)
EVT_MENU(THREADED_3D_1,  NooFrame::threaded3D1)
EVT_MENU(THREADED_3D_2,  NooFrame::threaded3D2)
EVT_MENU(THREADED_3D_3,  NooFrame::threaded3D3)
EVT_MENU(wxID_EXIT,      NooFrame::exit)
EVT_JOYSTICK_EVENTS(NooFrame::joystickInput)
EVT_CLOSE(NooFrame::close)
wxEND_EVENT_TABLE()

NooFrame::NooFrame(wxJoystick *joystick, Emulator *emulator, std::string path): wxFrame(nullptr, wxID_ANY, "NooDS"), joystick(joystick), emulator(emulator)
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

    // Set the current value of the threaded 3D setting
    switch (Settings::getThreaded3D())
    {
        case 0:  threaded3D->Check(THREADED_3D_0, true); break;
        case 1:  threaded3D->Check(THREADED_3D_1, true); break;
        case 2:  threaded3D->Check(THREADED_3D_2, true); break;
        default: threaded3D->Check(THREADED_3D_3, true); break;
    }

    // Set up the Settings menu
    wxMenu *settingsMenu = new wxMenu();
    settingsMenu->Append(PATH_SETTINGS,  "&Path Settings");
    settingsMenu->Append(INPUT_BINDINGS, "&Input Bindings");
    settingsMenu->Append(SCREEN_LAYOUT,  "&Screen Layout");
    settingsMenu->AppendSeparator();
    settingsMenu->AppendCheckItem(DIRECT_BOOT, "&Direct Boot");
    settingsMenu->AppendSubMenu(fpsLimiter, "&FPS Limiter");
    settingsMenu->AppendSeparator();
    settingsMenu->AppendCheckItem(THREADED_2D, "&Threaded 2D");
    settingsMenu->AppendSubMenu(threaded3D, "&Threaded 3D");

    // Set the current values of the checkboxes
    settingsMenu->Check(DIRECT_BOOT, Settings::getDirectBoot());
    settingsMenu->Check(THREADED_2D, Settings::getThreaded2D());

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

    // Capture joystick input if a joystick is connected
    // The initial axis values are saved so that inputs can be detected as offsets instead of raw values
    // This avoids issues with axes that have non-zero values in their resting position
    // Just make sure you aren't touching the controller when the program starts!
    if (joystick)
    {
        joystick->SetCapture(this, 10);
        for (int i = 0; i < joystick->GetNumberAxes(); i++)
            axisBases.push_back(joystick->GetPosition(i));
    }

    // Start the core right away if a filename was passed through the command line
    if (path != "")
    {
        if (path.find(".nds", path.length() - 4) != std::string::npos)
            ndsPath = path;
        else if (path.find(".gba", path.length() - 4) != std::string::npos)
            gbaPath = path;
        startCore();
    }
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
        emulator->core = new Core(ndsPath, gbaPath);
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
                SaveDialog saveDialog(ndsPath);
                if (saveDialog.ShowModal() == wxID_OK)
                {
                    emulator->core = new Core(ndsPath, gbaPath);
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

void NooFrame::pressKey(int key)
{
    // Handle a key press separate from the key's actual mapping
    switch (key)
    {
        case 12: // Fast Forward
        {
            // Disable the FPS limiter
            if (Settings::getFpsLimiter() != 0)
            {
                fpsLimiterBackup = Settings::getFpsLimiter();
                Settings::setFpsLimiter(0);
            }
            break;
        }

        case 13: // Full Screen
        {
            // Toggle full screen mode
            ShowFullScreen(fullScreen = !fullScreen);
            if (!fullScreen) emulator->frameReset = true;
            break;
        }

        default: // Core input
        {
            // Send a key press to the core
            if (emulator->running)
                emulator->core->input.pressKey(key);
            break;
        }
    }
}

void NooFrame::releaseKey(int key)
{
    // Handle a key release separate from the key's actual mapping
    switch (key)
    {
        case 12: // Fast Forward
        {
            // Restore the previous FPS limiter setting
            if (fpsLimiterBackup != 0)
            {
                Settings::setFpsLimiter(fpsLimiterBackup);
                fpsLimiterBackup = 0;
            }
            break;
        }

        default: // Core input
        {
            // Send a key release to the core
            if (emulator->running)
                emulator->core->input.releaseKey(key);
            break;
        }
    }
}

void NooFrame::loadRom(wxCommandEvent &event)
{
    // Show the file browser
    wxFileDialog romSelect(this, "Select ROM File", "", "", "NDS/GBA ROM files (*.nds, *.gba)|*.nds;*.gba", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (romSelect.ShowModal() == wxID_CANCEL)
        return;

    std::string path = (const char*)romSelect.GetPath().mb_str(wxConvUTF8);

    // Set the NDS or GBA ROM path depending on the extension of the selected file
    // If a ROM of the other type is already loaded, ask if the new ROM should be loaded alongside it
    if (path.find(".nds", path.length() - 4) != std::string::npos) // NDS ROM
    {
        if (gbaPath != "")
        {
            wxMessageDialog dialog(this, "Load this ROM alongside the current GBA ROM?", "Loading NDS ROM", wxYES_NO | wxICON_NONE);
            if (dialog.ShowModal() == wxID_NO) gbaPath = "";
        }
        ndsPath = path;
    }
    else // GBA ROM
    {
        if (ndsPath != "")
        {
            wxMessageDialog dialog(this, "Load this ROM alongside the current NDS ROM?", "Loading GBA ROM", wxYES_NO | wxICON_NONE);
            if (dialog.ShowModal() == wxID_NO) ndsPath = "";
        }
        gbaPath = path;
    }

    startCore();
}

void NooFrame::bootFirmware(wxCommandEvent &event)
{
    // Start the core with no ROM
    ndsPath = "";
    gbaPath = "";
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

        // Write the save as an extra precaution
        emulator->core->cartridge.writeSave();

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
    // Show the input settings dialog and recapture joystick input afterwards
    InputDialog inputDialog(joystick);
    inputDialog.ShowModal();
    if (joystick) joystick->SetCapture(this, 10);
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

void NooFrame::threaded2D(wxCommandEvent &event)
{
    // Toggle the threaded 2D setting
    Settings::setThreaded2D(!Settings::getThreaded2D());
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

void NooFrame::exit(wxCommandEvent &event)
{
    // Close the program
    Close(true);
}

void NooFrame::joystickInput(wxJoystickEvent &event)
{
    // Check the status of mapped joystick inputs and trigger key presses and releases accordingly
    for (int i = 0; i < 14; i++)
    {
        if (NooApp::getKeyBind(i) >= 3000 && joystick->GetNumberAxes() > NooApp::getKeyBind(i) - 3000) // Axis -
        {
            if (joystick->GetPosition(NooApp::getKeyBind(i) - 3000) - axisBases[NooApp::getKeyBind(i) - 3000] < -16384)
                pressKey(i);
            else
                releaseKey(i);
        }
        else if (NooApp::getKeyBind(i) >= 2000 && joystick->GetNumberAxes() > NooApp::getKeyBind(i) - 2000) // Axis +
        {
            if (joystick->GetPosition(NooApp::getKeyBind(i) - 2000) - axisBases[NooApp::getKeyBind(i) - 2000] > 16384)
                pressKey(i);
            else
                releaseKey(i);
        }
        else if (NooApp::getKeyBind(i) >= 1000 && joystick->GetNumberButtons() > NooApp::getKeyBind(i) - 1000) // Button
        {
            if (joystick->GetButtonState(NooApp::getKeyBind(i) - 1000))
                pressKey(i);
            else
                releaseKey(i);
        }
    }
}

void NooFrame::close(wxCloseEvent &event)
{
    // Free the joystick if one was connected
    if (joystick)
    {
        joystick->ReleaseCapture();
        delete joystick;
    }

    // Properly shut down the emulator
    stopCore();
    Settings::save();

    event.Skip(true);
}
