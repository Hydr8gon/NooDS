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

#include "noo_frame.h"
#include "cheat_dialog.h"
#include "input_dialog.h"
#include "layout_dialog.h"
#include "noo_app.h"
#include "noo_canvas.h"
#include "path_dialog.h"
#include "save_dialog.h"
#include "../settings.h"
#include "../../icon/icon.xpm"

enum FrameEvent {
    LOAD_ROM = 1,
    BOOT_FIRMWARE,
    SAVE_STATE,
    LOAD_STATE,
    TRIM_ROM,
    CHANGE_SAVE,
    QUIT,
    PAUSE,
    RESTART,
    STOP,
    ACTION_REPLAY,
    ADD_SYSTEM,
    DSI_MODE,
    DIRECT_BOOT,
    ROM_IN_RAM,
    FPS_LIMITER,
    FRAMESKIP_0,
    FRAMESKIP_1,
    FRAMESKIP_2,
    FRAMESKIP_3,
    FRAMESKIP_4,
    FRAMESKIP_5,
    THREADED_2D,
    THREADED_3D_0,
    THREADED_3D_1,
    THREADED_3D_2,
    THREADED_3D_3,
    THREADED_3D_4,
    HIGH_RES_3D,
    SCREEN_GHOST,
    EMULATE_AUDIO,
    AUDIO_16_BIT,
    MIC_ENABLE,
    PATH_SETTINGS,
    SCREEN_LAYOUT,
    INPUT_BINDINGS,
    UPDATE_JOY
};

wxBEGIN_EVENT_TABLE(NooFrame, wxFrame)
EVT_MENU(LOAD_ROM, NooFrame::loadRom)
EVT_MENU(BOOT_FIRMWARE, NooFrame::bootFirmware)
EVT_MENU(SAVE_STATE, NooFrame::saveState)
EVT_MENU(LOAD_STATE, NooFrame::loadState)
EVT_MENU(TRIM_ROM, NooFrame::trimRom)
EVT_MENU(CHANGE_SAVE, NooFrame::changeSave)
EVT_MENU(QUIT, NooFrame::quit)
EVT_MENU(PAUSE, NooFrame::pause)
EVT_MENU(RESTART, NooFrame::restart)
EVT_MENU(STOP, NooFrame::stop)
EVT_MENU(ACTION_REPLAY, NooFrame::actionReplay)
EVT_MENU(ADD_SYSTEM, NooFrame::addSystem)
EVT_MENU(DSI_MODE, NooFrame::dsiModeToggle)
EVT_MENU(DIRECT_BOOT, NooFrame::directBootToggle)
EVT_MENU(ROM_IN_RAM, NooFrame::romInRam)
EVT_MENU(FPS_LIMITER, NooFrame::fpsLimiter)
EVT_MENU(FRAMESKIP_0, NooFrame::frameskip<0>)
EVT_MENU(FRAMESKIP_1, NooFrame::frameskip<1>)
EVT_MENU(FRAMESKIP_2, NooFrame::frameskip<2>)
EVT_MENU(FRAMESKIP_3, NooFrame::frameskip<3>)
EVT_MENU(FRAMESKIP_4, NooFrame::frameskip<4>)
EVT_MENU(FRAMESKIP_5, NooFrame::frameskip<5>)
EVT_MENU(THREADED_2D, NooFrame::threaded2D)
EVT_MENU(THREADED_3D_0, NooFrame::threaded3D<0>)
EVT_MENU(THREADED_3D_1, NooFrame::threaded3D<1>)
EVT_MENU(THREADED_3D_2, NooFrame::threaded3D<2>)
EVT_MENU(THREADED_3D_3, NooFrame::threaded3D<3>)
EVT_MENU(THREADED_3D_4, NooFrame::threaded3D<4>)
EVT_MENU(HIGH_RES_3D, NooFrame::highRes3D)
EVT_MENU(SCREEN_GHOST, NooFrame::screenGhost)
EVT_MENU(EMULATE_AUDIO, NooFrame::emulateAudio)
EVT_MENU(AUDIO_16_BIT, NooFrame::audio16Bit)
EVT_MENU(MIC_ENABLE, NooFrame::micEnable)
EVT_MENU(PATH_SETTINGS, NooFrame::pathSettings)
EVT_MENU(SCREEN_LAYOUT, NooFrame::layoutSettings)
EVT_MENU(INPUT_BINDINGS, NooFrame::inputSettings)
EVT_TIMER(UPDATE_JOY, NooFrame::updateJoystick)
EVT_DROP_FILES(NooFrame::dropFiles)
EVT_CLOSE(NooFrame::close)
wxEND_EVENT_TABLE()

NooFrame::NooFrame(NooApp *app, int id, std::string path, NooFrame *partner):
    wxFrame(nullptr, wxID_ANY, "NooDS"), app(app), id(id), partner(partner), mainFrame(!partner) {
    // Set the icon
    wxIcon icon(icon_xpm);
    SetIcon(icon);

    if (mainFrame) {
        // Set up the file menu
        fileMenu = new wxMenu();
        fileMenu->Append(LOAD_ROM, "&Load ROM");
        fileMenu->Append(BOOT_FIRMWARE, "&Boot Firmware");
        fileMenu->AppendSeparator();
        fileMenu->Append(SAVE_STATE, "&Save State");
        fileMenu->Append(LOAD_STATE, "&Load State");
        fileMenu->AppendSeparator();
        fileMenu->Append(TRIM_ROM, "&Trim ROM");
        fileMenu->Append(CHANGE_SAVE, "&Change Save Type");
        fileMenu->AppendSeparator();
        fileMenu->Append(QUIT, "&Quit");

        // Set up the system menu
        systemMenu = new wxMenu();
        systemMenu->Append(PAUSE, "&Resume");
        systemMenu->Append(RESTART, "&Restart");
        systemMenu->Append(STOP, "&Stop");
        systemMenu->AppendSeparator();
        systemMenu->Append(ACTION_REPLAY, "&Action Replay");
        systemMenu->Append(ADD_SYSTEM, "&Add System");
        systemMenu->AppendSeparator();
        systemMenu->AppendCheckItem(DSI_MODE, "&DSi Homebrew Mode");

        // Set the initial system checkbox states
        systemMenu->Check(DSI_MODE, Settings::dsiMode);

        // Disable some menu items until the core is running
        fileMenu->Enable(TRIM_ROM, false);
        fileMenu->Enable(CHANGE_SAVE, false);
        fileMenu->Enable(SAVE_STATE, false);
        fileMenu->Enable(LOAD_STATE, false);
        systemMenu->Enable(PAUSE, false);
        systemMenu->Enable(RESTART, false);
        systemMenu->Enable(STOP, false);
        systemMenu->Enable(ACTION_REPLAY, false);

        // Set up the skip frames submenu
        wxMenu *frameskip = new wxMenu();
        frameskip->AppendRadioItem(FRAMESKIP_0, "&None");
        frameskip->AppendRadioItem(FRAMESKIP_1, "&1 Frame");
        frameskip->AppendRadioItem(FRAMESKIP_2, "&2 Frames");
        frameskip->AppendRadioItem(FRAMESKIP_3, "&3 Frames");
        frameskip->AppendRadioItem(FRAMESKIP_4, "&4 Frames");
        frameskip->AppendRadioItem(FRAMESKIP_5, "&5 Frames");

        // Set up the threaded 3D submenu
        wxMenu *threaded3D = new wxMenu();
        threaded3D->AppendRadioItem(THREADED_3D_0, "&Disabled");
        threaded3D->AppendRadioItem(THREADED_3D_1, "&1 Thread");
        threaded3D->AppendRadioItem(THREADED_3D_2, "&2 Threads");
        threaded3D->AppendRadioItem(THREADED_3D_3, "&3 Threads");
        threaded3D->AppendRadioItem(THREADED_3D_4, "&4 Threads");

        // Set up the general settings submenu
        wxMenu *generalMenu = new wxMenu();
        generalMenu->AppendCheckItem(DIRECT_BOOT, "&Direct Boot");
        generalMenu->AppendCheckItem(ROM_IN_RAM, "&Keep ROM in RAM");
        generalMenu->AppendCheckItem(FPS_LIMITER, "&FPS Limiter");

        // Set up the graphics settings submenu
        wxMenu *graphicsMenu = new wxMenu();
        graphicsMenu->AppendSubMenu(frameskip, "&Skip Frames");
        graphicsMenu->AppendCheckItem(THREADED_2D, "&Threaded 2D");
        graphicsMenu->AppendSubMenu(threaded3D, "&Threaded 3D");
        graphicsMenu->AppendCheckItem(HIGH_RES_3D, "&High-Resolution 3D");
        graphicsMenu->AppendCheckItem(SCREEN_GHOST, "Simulate Ghosting");

        // Set up the audio settings submenu
        wxMenu *audioMenu = new wxMenu();
        audioMenu->AppendCheckItem(EMULATE_AUDIO, "&Audio Emulation");
        audioMenu->AppendCheckItem(AUDIO_16_BIT, "&16-bit Audio Output");
        audioMenu->AppendCheckItem(MIC_ENABLE, "&Use Microphone");

        // Set up the settings menu
        wxMenu *settingsMenu = new wxMenu();
        settingsMenu->AppendSubMenu(generalMenu, "&General Settings");
        settingsMenu->AppendSubMenu(graphicsMenu, "&Graphics Settings");
        settingsMenu->AppendSubMenu(audioMenu, "&Audio Settings");
        settingsMenu->AppendSeparator();
        settingsMenu->Append(PATH_SETTINGS, "&Path Settings");
        settingsMenu->Append(SCREEN_LAYOUT, "&Screen Layout");
        settingsMenu->Append(INPUT_BINDINGS, "&Input Bindings");

        // Set the initial settings checkbox states
        settingsMenu->Check(DIRECT_BOOT, Settings::directBoot);
        settingsMenu->Check(ROM_IN_RAM, Settings::romInRam);
        settingsMenu->Check(FPS_LIMITER, Settings::fpsLimiter);
        settingsMenu->Check(THREADED_2D, Settings::threaded2D);
        settingsMenu->Check(HIGH_RES_3D, Settings::highRes3D);
        settingsMenu->Check(SCREEN_GHOST, Settings::screenGhost);
        settingsMenu->Check(EMULATE_AUDIO, Settings::emulateAudio);
        settingsMenu->Check(AUDIO_16_BIT, Settings::audio16Bit);
        settingsMenu->Check(MIC_ENABLE, NooApp::micEnable);

        // Set the initial radio setting selections
        frameskip->Check(FRAMESKIP_0 + std::min<uint8_t>(Settings::frameskip, 5), true);
        threaded3D->Check(THREADED_3D_0 + std::min<uint8_t>(Settings::threaded3D, 4), true);

        // Set up the menu bar
        wxMenuBar *menuBar = new wxMenuBar();
        menuBar->Append(fileMenu, "&File");
        menuBar->Append(systemMenu, "&System");
        if (id == 0) // Only show settings in the main instance
            menuBar->Append(settingsMenu, "&Settings");
        SetMenuBar(menuBar);
    }

    // Set the initial window size based on the current screen layout
    ScreenLayout layout;
    layout.update(0, 0, false, NooApp::splitScreens && ScreenLayout::screenArrangement != 3);
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
    if (joystick->IsOk()) {
        // Save the initial axis values so inputs can be detected as offsets instead of raw values
        // This avoids issues with axes that have non-zero values in their resting positions
        for (int i = 0; i < joystick->GetNumberAxes(); i++)
            axisBases.push_back(joystick->GetPosition(i));

        // Start a timer to update joystick input, since wxJoystickEvents are unreliable
        timer = new wxTimer(this, UPDATE_JOY);
        timer->Start(10);
    }
    else {
        // Don't use a joystick if one isn't connected
        delete joystick;
        joystick = nullptr;
        timer = nullptr;
    }

    // Load a filename passed through the command line
    if (path != "")
        loadRomPath(path);
}

void NooFrame::Refresh() {
    // Override the refresh function to also update the FPS counter
    wxFrame::Refresh();
    wxString label = "NooDS";
    if (id > 0) label += wxString::Format(" (%d)", id + 1);
    if (running) label += wxString::Format(" - %d FPS", core->fps);
    SetLabel(label);

    // Manage the main frame's partner frame
    if (mainFrame) {
        bool split = NooApp::splitScreens && ScreenLayout::screenArrangement != 3 && !canvas->gbaMode;
        if (split && !partner) {
            // Create the partner frame if needed
            partner = new NooFrame(app, id, "", this);
            partner->core = core;
            partner->running = running;
        }
        else if (!split && partner) {
            // Remove the partner frame if not needed
            delete partner;
            partner = nullptr;
        }
        else if (partner) {
            // Update the partner frame
            partner->Refresh();
        }
    }
}

void NooFrame::runCore() {
    // Run the emulator
    while (running)
        core->runCore();
}

void NooFrame::checkSave() {
    while (running) {
        // Check save files every few seconds and update them if changed
        std::unique_lock<std::mutex> lock(mutex);
        cond.wait_for(lock, std::chrono::seconds(3), [&]{ return !running; });
        core->cartridgeNds.writeSave();
        core->cartridgeGba.writeSave();
    }
}

void NooFrame::startCore(bool full) {
    if (full) {
        // Ensure the core is shut down
        stopCore(true);

        try {
            // Attempt to boot the core
            core = new Core(ndsPath, gbaPath, id);
            if (partner) partner->core = core;
            app->connectCore(id);
        }
        catch (CoreError e) {
            // Inform the user of the error if loading wasn't successful
            switch (e) {
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

    if (core) {
        // Enable some file menu items if a ROM is loaded
        if (ndsPath != "" || gbaPath != "") {
            fileMenu->Enable(TRIM_ROM, true);
            fileMenu->Enable(CHANGE_SAVE, true);
            fileMenu->Enable(SAVE_STATE, true);
            fileMenu->Enable(LOAD_STATE, true);
        }

        // Update the system menu for running
        systemMenu->SetLabel(PAUSE, "&Pause");
        systemMenu->Enable(PAUSE, true);
        systemMenu->Enable(RESTART, true);
        systemMenu->Enable(STOP, true);
        systemMenu->Enable(ACTION_REPLAY, ndsPath != "");

        // Start the threads
        running = true;
        if (partner) partner->running = running;
        coreThread = new std::thread(&NooFrame::runCore, this);
        saveThread = new std::thread(&NooFrame::checkSave, this);
    }
}

void NooFrame::stopCore(bool full) {
    { // Signal for the threads to stop if the core is running
        std::lock_guard<std::mutex> guard(mutex);
        running = false;
        if (partner) partner->running = running;
        cond.notify_one();
    }

    // Wait for the core thread to stop
    if (coreThread) {
        coreThread->join();
        delete coreThread;
        coreThread = nullptr;
    }

    // Wait for the save thread to stop
    if (saveThread) {
        saveThread->join();
        delete saveThread;
        saveThread = nullptr;
    }

    // Update the system menu for being paused
    systemMenu->SetLabel(PAUSE, "&Resume");

    if (full) {
        // Disable some menu items
        fileMenu->Enable(TRIM_ROM, false);
        fileMenu->Enable(CHANGE_SAVE, false);
        fileMenu->Enable(SAVE_STATE, false);
        fileMenu->Enable(LOAD_STATE, false);
        systemMenu->Enable(PAUSE, false);
        systemMenu->Enable(RESTART, false);
        systemMenu->Enable(STOP, false);

        // Shut down the core
        if (core) {
            app->disconnCore(id);
            delete core;
            core = nullptr;
            if (partner) partner->core = core;
        }
    }
}

void NooFrame::pressKey(int key) {
    // Handle a key press separate from the key's actual mapping
    switch (key) {
        case 12: // Fast Forward Hold
            // Disable the FPS limiter
            if (Settings::fpsLimiter != 0) {
                fpsLimiterBackup = Settings::fpsLimiter;
                Settings::fpsLimiter = 0;
            }
            break;

        case 13: // Fast Forward Toggle
            // Toggle the FPS limiter on or off
            if (!(hotkeyToggles & BIT(0))) {
                if (Settings::fpsLimiter != 0) {
                    // Disable the FPS limiter
                    fpsLimiterBackup = Settings::fpsLimiter;
                    Settings::fpsLimiter = 0;
                }
                else if (fpsLimiterBackup != 0) {
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

        case 15: // Screen Swap Toggle
            // Toggle between favoring the top or bottom screen
            if (!(hotkeyToggles & BIT(2))) {
                ScreenLayout::screenSizing = (ScreenLayout::screenSizing == 1) ? 2 : 1;
                app->updateLayouts();
                hotkeyToggles |= BIT(2);
            }
            break;

        case 16: // System Pause Toggle
            // Toggle between pausing or resuming the core
            if (!(hotkeyToggles & BIT(3))) {
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

void NooFrame::releaseKey(int key) {
    // Handle a key release separate from the key's actual mapping
    switch (key) {
        case 12: // Fast Forward Hold
            // Restore the previous FPS limiter setting
            if (fpsLimiterBackup != 0) {
                Settings::fpsLimiter = fpsLimiterBackup;
                fpsLimiterBackup = 0;
            }
            break;

        case 13: // Fast Forward Toggle
        case 15: // Screen Swap Toggle
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

void NooFrame::loadRomPath(std::string path) {
    // Set the NDS or GBA ROM path depending on the extension of the given file
    // If a ROM of the other type is already loaded, ask if it should be loaded alongside the new ROM
    if (path.find(".nds", path.length() - 4) != std::string::npos) { // NDS ROM
        if (gbaPath != "") {
            wxMessageDialog dialog(this, "Load the current GBA ROM alongside this ROM?", "Loading NDS ROM", wxYES_NO | wxICON_NONE);
            if (dialog.ShowModal() != wxID_YES) gbaPath = "";
        }
        ndsPath = path;
    }
    else if (path.find(".gba", path.length() - 4) != std::string::npos) { // GBA ROM
        if (ndsPath != "") {
            wxMessageDialog dialog(this, "Load the current NDS ROM alongside this ROM?", "Loading GBA ROM", wxYES_NO | wxICON_NONE);
            if (dialog.ShowModal() != wxID_YES) ndsPath = "";
        }
        gbaPath = path;
    }
    else {
        return;
    }

    // Restart the core
    startCore(true);
}

void NooFrame::loadRom(wxCommandEvent &event) {
    // Show the file browser
    wxFileDialog romSelect(this, "Select ROM File", "", "", "NDS/GBA ROM files (*.nds, *.gba)|*.nds;*.gba", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (romSelect.ShowModal() != wxID_CANCEL)
        loadRomPath((const char*)romSelect.GetPath().mb_str(wxConvUTF8));
}

void NooFrame::bootFirmware(wxCommandEvent &event) {
    // Start the core with no ROM
    ndsPath = "";
    gbaPath = "";
    startCore(true);
}

void NooFrame::trimRom(wxCommandEvent &event) {
    bool gba = core->gbaMode;

    // Confirm that the current ROM should be trimmed
    wxMessageDialog dialog(this, "Trim the current ROM to save space?", "Trimming ROM", wxYES_NO | wxICON_NONE);
    if (dialog.ShowModal() == wxID_YES) {
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

void NooFrame::changeSave(wxCommandEvent &event) {
    // Show the save dialog
    SaveDialog saveDialog(this);
    saveDialog.ShowModal();
}

void NooFrame::saveState(wxCommandEvent &event) {
    // Create a confirmation dialog, with extra information if a state file doesn't exist yet
    wxMessageDialog *dialog;
    switch (core->saveStates.checkState()) {
        case STATE_FILE_FAIL:
            dialog = new wxMessageDialog(this, "Saving and loading states is dangerous and can lead to data "
                "loss. States are also not guaranteed to be compatible across emulator versions. Please "
                "rely on in-game saving to keep your progress, and back up .sav files before using this "
                "feature. Do you want to save the current state?", "Save State", wxYES_NO | wxICON_NONE);
            break;

        default:
            dialog = new wxMessageDialog(this, "Do you want to overwrite the saved state with the "
                "current state? This can't be undone!", "Save State", wxYES_NO | wxICON_NONE);
            break;
    }

    // Show the dialog and save the state if confirmed
    if (dialog->ShowModal() == wxID_YES) {
        stopCore(false);
        core->saveStates.saveState();
        startCore(false);
    }
    delete dialog;
}

void NooFrame::loadState(wxCommandEvent &event) {
    // Create a confirmation dialog, or an error if something went wrong
    wxMessageDialog *dialog;
    switch (core->saveStates.checkState()) {
        case STATE_SUCCESS:
            dialog = new wxMessageDialog(this, "Do you want to load the saved state and lose the "
                "current state? This can't be undone!", "Load State", wxYES_NO | wxICON_NONE);
            break;

        case STATE_FILE_FAIL:
            dialog = new wxMessageDialog(this, "The state file doesn't "
                "exist or couldn't be opened.", "Error", wxICON_NONE);
            break;

        case STATE_FORMAT_FAIL:
            dialog = new wxMessageDialog(this, "The state file "
                "doesn't have a valid format.", "Error", wxICON_NONE);
            break;

        case STATE_VERSION_FAIL:
            dialog = new wxMessageDialog(this, "The state file isn't "
                "compatible with this version of NooDS.", "Error", wxICON_NONE);
            break;
    }

    // Show the dialog and load the state if confirmed
    if (dialog->ShowModal() == wxID_YES) {
        stopCore(false);
        core->saveStates.loadState();
        startCore(false);
    }
    delete dialog;
}

void NooFrame::quit(wxCommandEvent &event) {
    // Close the program
    Close(true);
}

void NooFrame::pause(wxCommandEvent &event) {
    // Pause or resume the core
    if (running)
        stopCore(false);
    else
        startCore(false);
}

void NooFrame::restart(wxCommandEvent &event) {
    // Restart the core
    startCore(true);
}

void NooFrame::stop(wxCommandEvent &event) {
    // Stop the core
    stopCore(true);
}

void NooFrame::actionReplay(wxCommandEvent &event) {
    // Show the AR cheats dialog
    CheatDialog cheatDialog(core);
    cheatDialog.ShowModal();
}

void NooFrame::addSystem(wxCommandEvent &event) {
    // Create a new emulator instance
    app->createFrame();
}

void NooFrame::dsiModeToggle(wxCommandEvent &event) {
    // Toggle the DSi homebrew mode setting
    Settings::dsiMode = !Settings::dsiMode;
    Settings::save();
}

void NooFrame::directBootToggle(wxCommandEvent &event) {
    // Toggle the direct boot setting
    Settings::directBoot = !Settings::directBoot;
    Settings::save();
}

void NooFrame::romInRam(wxCommandEvent &event) {
    // Toggle the ROM in RAM setting
    Settings::romInRam = !Settings::romInRam;
    Settings::save();
}

void NooFrame::fpsLimiter(wxCommandEvent &event) {
    // Toggle the FPS limiter setting
    Settings::fpsLimiter = !Settings::fpsLimiter;
    Settings::save();
}

template <int value> void NooFrame::frameskip(wxCommandEvent &event) {
    // Set the skip frames setting
    Settings::frameskip = value;
    Settings::save();
}

void NooFrame::threaded2D(wxCommandEvent &event) {
    // Toggle the threaded 2D setting
    Settings::threaded2D = !Settings::threaded2D;
    Settings::save();
}

template <int value> void NooFrame::threaded3D(wxCommandEvent &event) {
    // Set the threaded 3D setting
    Settings::threaded3D = value;
    Settings::save();
}

void NooFrame::highRes3D(wxCommandEvent &event) {
    // Toggle the high-resolution 3D setting
    Settings::highRes3D = !Settings::highRes3D;
    Settings::save();
}

void NooFrame::screenGhost(wxCommandEvent &event) {
    // Toggle the simulate ghosting setting
    Settings::screenGhost = !Settings::screenGhost;
    app->updateLayouts();
}

void NooFrame::emulateAudio(wxCommandEvent &event) {
    // Toggle the audio emulation setting
    Settings::emulateAudio = !Settings::emulateAudio;
    Settings::save();
}

void NooFrame::audio16Bit(wxCommandEvent &event) {
    // Toggle the 16-bit audio output setting
    Settings::audio16Bit = !Settings::audio16Bit;
    Settings::save();
}

void NooFrame::micEnable(wxCommandEvent &event) {
    // Toggle the use microphone setting
    NooApp::micEnable = !NooApp::micEnable;
    NooApp::micEnable ? app->startStream(1) : app->stopStream(1);
    Settings::save();
}

void NooFrame::pathSettings(wxCommandEvent &event) {
    // Show the path settings dialog
    PathDialog pathDialog;
    pathDialog.ShowModal();
}

void NooFrame::layoutSettings(wxCommandEvent &event) {
    // Show the layout settings dialog
    LayoutDialog layoutDialog(app);
    layoutDialog.ShowModal();
}

void NooFrame::inputSettings(wxCommandEvent &event) {
    // Pause joystick updates and show the input settings dialog
    if (timer) timer->Stop();
    InputDialog inputDialog(joystick);
    inputDialog.ShowModal();
    if (timer) timer->Start(10);
}

void NooFrame::updateJoystick(wxTimerEvent &event) {
    // Check the status of mapped joystick inputs and trigger key presses and releases accordingly
    for (int i = 0; i < MAX_KEYS; i++) {
        if (NooApp::keyBinds[i] >= 3000 && joystick->GetNumberAxes() > NooApp::keyBinds[i] - 3000) { // Axis -
            if (joystick->GetPosition(NooApp::keyBinds[i] - 3000) - axisBases[NooApp::keyBinds[i] - 3000] < -16384)
                pressKey(i);
            else
                releaseKey(i);
        }
        else if (NooApp::keyBinds[i] >= 2000 && joystick->GetNumberAxes() > NooApp::keyBinds[i] - 2000) { // Axis +
            if (joystick->GetPosition(NooApp::keyBinds[i] - 2000) - axisBases[NooApp::keyBinds[i] - 2000] > 16384)
                pressKey(i);
            else
                releaseKey(i);
        }
        else if (NooApp::keyBinds[i] >= 1000 && joystick->GetNumberButtons() > NooApp::keyBinds[i] - 1000) { // Button
            if (joystick->GetButtonState(NooApp::keyBinds[i] - 1000))
                pressKey(i);
            else
                releaseKey(i);
        }
    }
}

void NooFrame::dropFiles(wxDropFilesEvent &event) {
    // Load a single dropped file
    if (event.GetNumberOfFiles() != 1) return;
    wxString path = event.GetFiles()[0];
    if (!wxFileExists(path)) return;
    (mainFrame ? this : partner)->loadRomPath((const char*)path.mb_str(wxConvUTF8));
}

void NooFrame::close(wxCloseEvent &event) {
    // Properly shut down the emulator
    (mainFrame ? this : partner)->stopCore(true);
    app->removeFrame(id);
    canvas->finish();
    if (partner) delete partner;
    event.Skip(true);
}
