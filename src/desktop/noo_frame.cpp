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

#include "noo_frame.h"
#include "input_dialog.h"
#include "layout_dialog.h"
#include "noo_app.h"
#include "noo_canvas.h"
#include "path_dialog.h"
#include "save_dialog.h"
#include "../common/screen_layout.h"
#include "../settings.h"
#include "../../icon/icon.xpm"

enum FrameEvent
{
    LOAD_ROM = 1,
    BOOT_FIRMWARE,
    TRIM_ROM,
    CHANGE_SAVE,
    QUIT,
    PAUSE,
    RESTART,
    STOP,
    ADD_SYSTEM,
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
    HIGH_RES_3D,
    MIC_ENABLE,
    UPDATE_JOY
};

wxBEGIN_EVENT_TABLE(NooFrame, wxFrame)
EVT_MENU(LOAD_ROM,       NooFrame::loadRom)
EVT_MENU(BOOT_FIRMWARE,  NooFrame::bootFirmware)
EVT_MENU(TRIM_ROM,       NooFrame::trimRom)
EVT_MENU(CHANGE_SAVE,    NooFrame::changeSave)
EVT_MENU(QUIT,           NooFrame::quit)
EVT_MENU(PAUSE,          NooFrame::pause)
EVT_MENU(RESTART,        NooFrame::restart)
EVT_MENU(STOP,           NooFrame::stop)
EVT_MENU(ADD_SYSTEM,     NooFrame::addSystem)
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
EVT_MENU(HIGH_RES_3D,    NooFrame::highRes3D)
EVT_MENU(MIC_ENABLE,     NooFrame::micEnable)
EVT_TIMER(UPDATE_JOY,    NooFrame::updateJoystick)
EVT_DROP_FILES(NooFrame::dropFiles)
EVT_CLOSE(NooFrame::close)
wxEND_EVENT_TABLE()

NooFrame::NooFrame(NooApp *app, int id, std::string path):
    wxFrame(nullptr, wxID_ANY, "NooDS"), app(app), id(id)
{
    // Set the icon
    wxIcon icon(icon_xpm);
    SetIcon(icon);

    // Set up the File menu
    fileMenu = new wxMenu();
    fileMenu->Append(LOAD_ROM,      "&Load ROM");
    fileMenu->Append(BOOT_FIRMWARE, "&Boot Firmware");
    fileMenu->AppendSeparator();
    fileMenu->Append(TRIM_ROM,      "&Trim ROM");
    fileMenu->Append(CHANGE_SAVE,   "&Change Save Type");
    fileMenu->AppendSeparator();
    fileMenu->Append(QUIT,          "&Quit");

    // Set up the System menu
    systemMenu = new wxMenu();
    systemMenu->Append(PAUSE,      "&Resume");
    systemMenu->Append(RESTART,    "&Restart");
    systemMenu->Append(STOP,       "&Stop");
    systemMenu->AppendSeparator();
    systemMenu->Append(ADD_SYSTEM, "&Add System");

    // Disable some menu items until the core is running
    fileMenu->Enable(TRIM_ROM,    false);
    fileMenu->Enable(CHANGE_SAVE, false);
    systemMenu->Enable(PAUSE,     false);
    systemMenu->Enable(RESTART,   false);
    systemMenu->Enable(STOP,      false);

    // Set up the FPS Limiter submenu
    wxMenu *fpsLimiter = new wxMenu();
    fpsLimiter->AppendRadioItem(FPS_DISABLED, "&Disabled");
    fpsLimiter->AppendRadioItem(FPS_LIGHT,    "&Light");
    fpsLimiter->AppendRadioItem(FPS_ACCURATE, "&Accurate");

    // Set the current value of the FPS limiter setting
    switch (Settings::fpsLimiter)
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
    switch (Settings::threaded3D)
    {
        case 0:  threaded3D->Check(THREADED_3D_0, true); break;
        case 1:  threaded3D->Check(THREADED_3D_1, true); break;
        case 2:  threaded3D->Check(THREADED_3D_2, true); break;
        default: threaded3D->Check(THREADED_3D_3, true); break;
    }

    // Set up the Settings menu
    wxMenu *settingsMenu = new wxMenu();
    settingsMenu->Append(PATH_SETTINGS,        "&Path Settings");
    settingsMenu->Append(INPUT_BINDINGS,       "&Input Bindings");
    settingsMenu->Append(SCREEN_LAYOUT,        "&Screen Layout");
    settingsMenu->AppendSeparator();
    settingsMenu->AppendCheckItem(DIRECT_BOOT, "&Direct Boot");
    settingsMenu->AppendSubMenu(fpsLimiter,    "&FPS Limiter");
    settingsMenu->AppendSeparator();
    settingsMenu->AppendCheckItem(THREADED_2D, "&Threaded 2D");
    settingsMenu->AppendSubMenu(threaded3D,    "&Threaded 3D");
    settingsMenu->AppendSeparator();
    settingsMenu->AppendCheckItem(HIGH_RES_3D, "&High-Resolution 3D");
    settingsMenu->AppendSeparator();
    settingsMenu->AppendCheckItem(MIC_ENABLE,  "&Use Microphone");

    // Set the current values of the checkboxes
    settingsMenu->Check(DIRECT_BOOT, Settings::directBoot);
    settingsMenu->Check(THREADED_2D, Settings::threaded2D);
    settingsMenu->Check(HIGH_RES_3D, Settings::highRes3D);
    settingsMenu->Check(MIC_ENABLE,  NooApp::micEnable);

    // Set up the menu bar
    wxMenuBar *menuBar = new wxMenuBar();
    menuBar->Append(fileMenu,   "&File");
    menuBar->Append(systemMenu, "&System");
    if (id == 0) // Only show settings in the main instance
        menuBar->Append(settingsMenu, "&Settings");
    SetMenuBar(menuBar);

    // Set the initial window size based on the current screen layout
    ScreenLayout layout;
    layout.update(0, 0, false);
    SetClientSize(wxSize(layout.minWidth, layout.minHeight));

    // Prepare and show the window
    DragAcceptFiles(true);
    SetBackgroundColour(*wxBLACK);
    Centre();
    Show(true);

    // Create and add a canvas for drawing the framebuffer
    canvas = new NooCanvas(this);
    wxBoxSizer *sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(canvas, 1, wxEXPAND);
    SetSizer(sizer);

    // Prepare a joystick if one is connected
    joystick = new wxJoystick();
    if (joystick->IsOk())
    {
        // Save the initial axis values so inputs can be detected as offsets instead of raw values
        // This avoids issues with axes that have non-zero values in their resting positions
        for (int i = 0; i < joystick->GetNumberAxes(); i++)
            axisBases.push_back(joystick->GetPosition(i));

        // Start a timer to update joystick input, since wxJoystickEvents are unreliable
        timer = new wxTimer(this, UPDATE_JOY);
        timer->Start(10);
    }
    else
    {
        // Don't use a joystick if one isn't connected
        delete joystick;
        joystick = nullptr;
        timer = nullptr;
    }

    // Load a filename passed through the command line
    if (path != "")
        loadRomPath(path);
}

void NooFrame::Refresh()
{
    wxFrame::Refresh();

    // Override the refresh function to also update the FPS counter
    wxString label = "NooDS";
    if (id > 0)  label += wxString::Format(" (%d)", id + 1);
    if (running) label += wxString::Format(" - %d FPS", core->fps);
    SetLabel(label);
}

void NooFrame::runCore()
{
    // Run the emulator
    while (running)
        core->runFrame();
}

void NooFrame::checkSave()
{
    while (running)
    {
        // Check save files every few seconds and update them if changed
        std::unique_lock<std::mutex> lock(mutex);
        cond.wait_for(lock, std::chrono::seconds(3), [&]{ return !running; });
        core->cartridgeNds.writeSave();
        core->cartridgeGba.writeSave();
    }
}

void NooFrame::startCore(bool full)
{
    if (full)
    {
        // Ensure the core is shut down
        stopCore(true);

        try
        {
            // Attempt to boot the core
            core = new Core(ndsPath, gbaPath, "", "", id);
            app->connectCore(id);
        }
        catch (CoreError e)
        {
            // Inform the user of the error if loading wasn't successful
            switch (e)
            {
                case ERROR_BIOS: // Missing BIOS files
                    wxMessageDialog(this, "Make sure the path settings point to valid BIOS files and try again.",
                        "Error Loading BIOS", wxICON_NONE).ShowModal();
                    return;

                case ERROR_FIRM: // Non-bootable firmware file
                    wxMessageDialog(this, "Make sure the path settings point to a bootable firmware file or try another boot method.",
                        "Error Loading Firmware", wxICON_NONE).ShowModal();
                    return;

                case ERROR_ROM: // Unreadable ROM file
                    wxMessageDialog(this, "Make sure the ROM file is accessible and try again.",
                        "Error Loading ROM", wxICON_NONE).ShowModal();
                    return;
            }
        }
    }

    if (core)
    {
        systemMenu->SetLabel(PAUSE, "&Pause");

        // Enable some menu items
        if (ndsPath != "" || gbaPath != "")
        {
            fileMenu->Enable(TRIM_ROM,    true);
            fileMenu->Enable(CHANGE_SAVE, true);
        }
        systemMenu->Enable(PAUSE,   true);
        systemMenu->Enable(RESTART, true);
        systemMenu->Enable(STOP,    true);

        // Start the threads
        running = true;
        coreThread = new std::thread(&NooFrame::runCore, this);
        saveThread = new std::thread(&NooFrame::checkSave, this);
    }
}

void NooFrame::stopCore(bool full)
{
    // Signal for the threads to stop if the core is running
    {
        std::lock_guard<std::mutex> guard(mutex);
        running = false;
        cond.notify_one();
    }

    // Wait for the core thread to stop
    if (coreThread)
    {
        coreThread->join();
        delete coreThread;
        coreThread = nullptr;
    }

    // Wait for the save thread to stop
    if (saveThread)
    {
        saveThread->join();
        delete saveThread;
        saveThread = nullptr;
    }

    systemMenu->SetLabel(PAUSE, "&Resume");

    if (full)
    {
        // Disable some menu items
        fileMenu->Enable(TRIM_ROM,    false);
        fileMenu->Enable(CHANGE_SAVE, false);
        systemMenu->Enable(PAUSE,     false);
        systemMenu->Enable(RESTART,   false);
        systemMenu->Enable(STOP,      false);

        // Shut down the core
        if (core)
        {
            app->disconnCore(id);
            delete core;
            core = nullptr;
        }
    }
}

void NooFrame::pressKey(int key)
{
    // Handle a key press separate from the key's actual mapping
    switch (key)
    {
        case 12: // Fast Forward Hold
            // Disable the FPS limiter
            if (Settings::fpsLimiter != 0)
            {
                fpsLimiterBackup = Settings::fpsLimiter;
                Settings::fpsLimiter = 0;
            }
            break;

        case 13: // Fast Forward Toggle
            // Toggle the FPS limiter on or off
            if (!(hotkeyToggles & BIT(0)))
            {
                if (Settings::fpsLimiter != 0)
                {
                    // Disable the FPS limiter
                    fpsLimiterBackup = Settings::fpsLimiter;
                    Settings::fpsLimiter = 0;
                }
                else if (fpsLimiterBackup != 0)
                {
                    // Restore the previous FPS limiter setting
                    Settings::fpsLimiter = fpsLimiterBackup;
                    fpsLimiterBackup = 0;
                }

                hotkeyToggles |= BIT(0);
            }
            break;

        case 14: // Full Screen Toggle
            // Toggle full screen mode
            ShowFullScreen(fullScreen = !fullScreen);
            if (!fullScreen) canvas->resetFrame();
            break;

        case 15: // Enlarge Swap Toggle
            // Toggle between enlarging the top or bottom screen
            if (!(hotkeyToggles & BIT(2)))
            {
                ScreenLayout::screenSizing = (ScreenLayout::screenSizing == 1) ? 2 : 1;
                app->updateLayouts();
                hotkeyToggles |= BIT(2);
            }
            break;

        case 16: // System Pause Toggle
            // Toggle between pausing or resuming the core
            if (!(hotkeyToggles & BIT(3)))
            {
                running ? stopCore(false) : startCore(false);
                hotkeyToggles |= BIT(3);
            }
            break;

        default: // Core input
            // Send a key press to the core
            if (running)
                core->input.pressKey(key);
            break;
    }
}

void NooFrame::releaseKey(int key)
{
    // Handle a key release separate from the key's actual mapping
    switch (key)
    {
        case 12: // Fast Forward Hold
            // Restore the previous FPS limiter setting
            if (fpsLimiterBackup != 0)
            {
                Settings::fpsLimiter = fpsLimiterBackup;
                fpsLimiterBackup = 0;
            }
            break;

        case 13: // Fast Forward Toggle
        case 15: // Enlarge Swap Toggle
        case 16: // System Pause Toggle
            // Clear a toggle bit so a hotkey can be used again
            hotkeyToggles &= ~BIT(key - 13);
            break;

        default: // Core input
            // Send a key release to the core
            if (running)
                core->input.releaseKey(key);
            break;
    }
}

void NooFrame::loadRomPath(std::string path)
{
    // Set the NDS or GBA ROM path depending on the extension of the given file
    // If a ROM of the other type is already loaded, ask if it should be loaded alongside the new ROM
    if (path.find(".nds", path.length() - 4) != std::string::npos) // NDS ROM
    {
        if (gbaPath != "")
        {
            wxMessageDialog dialog(this, "Load the current GBA ROM alongside this ROM?", "Loading NDS ROM", wxYES_NO | wxICON_NONE);
            if (dialog.ShowModal() != wxID_YES) gbaPath = "";
        }
        ndsPath = path;
    }
    else if (path.find(".gba", path.length() - 4) != std::string::npos) // GBA ROM
    {
        if (ndsPath != "")
        {
            wxMessageDialog dialog(this, "Load the current NDS ROM alongside this ROM?", "Loading GBA ROM", wxYES_NO | wxICON_NONE);
            if (dialog.ShowModal() != wxID_YES) ndsPath = "";
        }
        gbaPath = path;
    }
    else
    {
        return;
    }

    // Restart the core
    startCore(true);
}

void NooFrame::loadRom(wxCommandEvent &event)
{
    // Show the file browser
    wxFileDialog romSelect(this, "Select ROM File", "", "", "NDS/GBA ROM files (*.nds, *.gba)|*.nds;*.gba", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (romSelect.ShowModal() != wxID_CANCEL)
        loadRomPath((const char*)romSelect.GetPath().mb_str(wxConvUTF8));
}

void NooFrame::bootFirmware(wxCommandEvent &event)
{
    // Start the core with no ROM
    ndsPath = "";
    gbaPath = "";
    startCore(true);
}

void NooFrame::trimRom(wxCommandEvent &event)
{
    bool gba = core->gbaMode;

    // Confirm that the current ROM should be trimmed
    wxMessageDialog dialog(this, "Trim the current ROM to save space?", "Trimming ROM", wxYES_NO | wxICON_NONE);
    if (dialog.ShowModal() == wxID_YES)
    {
        int oldSize, newSize;

        // Pause the core for safety and trim the ROM
        stopCore(false);
        Cartridge *cartridge = gba ? (Cartridge*)&core->cartridgeGba : (Cartridge*)&core->cartridgeNds;
        oldSize = cartridge->getRomSize();
        cartridge->trimRom();
        newSize = cartridge->getRomSize();
        startCore(false);

        // Show the results
        wxString str;
        if (oldSize != newSize)
            str.Printf("ROM trimmed from %.2fMB to %.2fMB!", (float)oldSize / 1024 / 1024, (float)newSize / 1024 / 1024);
        else
            str.Printf("This ROM is already trimmed!");
        wxMessageDialog(this, str, "ROM Trimmed", wxICON_NONE).ShowModal();
    }
}

void NooFrame::changeSave(wxCommandEvent &event)
{
    // Show the save dialog
    SaveDialog saveDialog(this);
    saveDialog.ShowModal();
}

void NooFrame::quit(wxCommandEvent &event)
{
    // Close the program
    Close(true);
}

void NooFrame::pause(wxCommandEvent &event)
{
    // Pause or resume the core
    if (running)
        stopCore(false);
    else
        startCore(false);
}

void NooFrame::restart(wxCommandEvent &event)
{
    // Restart the core
    startCore(true);
}

void NooFrame::stop(wxCommandEvent &event)
{
    // Stop the core
    stopCore(true);
}

void NooFrame::addSystem(wxCommandEvent &event)
{
    // Create a new emulator instance
    app->createFrame();
}

void NooFrame::pathSettings(wxCommandEvent &event)
{
    // Show the path settings dialog
    PathDialog pathDialog;
    pathDialog.ShowModal();
}

void NooFrame::inputSettings(wxCommandEvent &event)
{
    // Pause joystick updates and show the input settings dialog
    if (timer) timer->Stop();
    InputDialog inputDialog(joystick);
    inputDialog.ShowModal();
    if (timer) timer->Start(10);
}

void NooFrame::layoutSettings(wxCommandEvent &event)
{
    // Show the layout settings dialog
    LayoutDialog layoutDialog(app);
    layoutDialog.ShowModal();
}

void NooFrame::directBootToggle(wxCommandEvent &event)
{
    // Toggle the direct boot setting
    Settings::directBoot = !Settings::directBoot;
    Settings::save();
}

void NooFrame::fpsDisabled(wxCommandEvent &event)
{
    // Set the FPS limiter setting to disabled
    Settings::fpsLimiter = 0;
    Settings::save();
}

void NooFrame::fpsLight(wxCommandEvent &event)
{
    // Set the FPS limiter setting to light
    Settings::fpsLimiter = 1;
    Settings::save();
}

void NooFrame::fpsAccurate(wxCommandEvent &event)
{
    // Set the FPS limiter setting to accurate
    Settings::fpsLimiter = 2;
    Settings::save();
}

void NooFrame::threaded2D(wxCommandEvent &event)
{
    // Toggle the threaded 2D setting
    Settings::threaded2D = !Settings::threaded2D;
    Settings::save();
}

void NooFrame::threaded3D0(wxCommandEvent &event)
{
    // Set the threaded 3D setting to disabled
    Settings::threaded3D = 0;
    Settings::save();
}

void NooFrame::threaded3D1(wxCommandEvent &event)
{
    // Set the threaded 3D setting to 1 thread
    Settings::threaded3D = 1;
    Settings::save();
}

void NooFrame::threaded3D2(wxCommandEvent &event)
{
    // Set the threaded 3D setting to 2 threads
    Settings::threaded3D = 2;
    Settings::save();
}

void NooFrame::threaded3D3(wxCommandEvent &event)
{
    // Set the threaded 3D setting to 3 threads
    Settings::threaded3D = 3;
    Settings::save();
}

void NooFrame::highRes3D(wxCommandEvent &event)
{
    // Toggle the high-resolution 3D setting
    Settings::highRes3D = !Settings::highRes3D;
    Settings::save();
}

void NooFrame::micEnable(wxCommandEvent &event)
{
    // Toggle the use microphone setting
    NooApp::micEnable = !NooApp::micEnable;
    NooApp::micEnable ? app->startStream(1) : app->stopStream(1);
    Settings::save();
}

void NooFrame::updateJoystick(wxTimerEvent &event)
{
    // Check the status of mapped joystick inputs and trigger key presses and releases accordingly
    for (int i = 0; i < MAX_KEYS; i++)
    {
        if (NooApp::keyBinds[i] >= 3000 && joystick->GetNumberAxes() > NooApp::keyBinds[i] - 3000) // Axis -
        {
            if (joystick->GetPosition(NooApp::keyBinds[i] - 3000) - axisBases[NooApp::keyBinds[i] - 3000] < -16384)
                pressKey(i);
            else
                releaseKey(i);
        }
        else if (NooApp::keyBinds[i] >= 2000 && joystick->GetNumberAxes() > NooApp::keyBinds[i] - 2000) // Axis +
        {
            if (joystick->GetPosition(NooApp::keyBinds[i] - 2000) - axisBases[NooApp::keyBinds[i] - 2000] > 16384)
                pressKey(i);
            else
                releaseKey(i);
        }
        else if (NooApp::keyBinds[i] >= 1000 && joystick->GetNumberButtons() > NooApp::keyBinds[i] - 1000) // Button
        {
            if (joystick->GetButtonState(NooApp::keyBinds[i] - 1000))
                pressKey(i);
            else
                releaseKey(i);
        }
    }
}

void NooFrame::dropFiles(wxDropFilesEvent &event)
{
    // Load a single dropped file
    if (event.GetNumberOfFiles() != 1) return;
    wxString path = event.GetFiles()[0];
    if (!wxFileExists(path)) return;
    loadRomPath((const char*)path.mb_str(wxConvUTF8));
}

void NooFrame::close(wxCloseEvent &event)
{
    // Properly shut down the emulator
    stopCore(true);
    app->removeFrame(id);
    canvas->finish();
    event.Skip(true);
}
