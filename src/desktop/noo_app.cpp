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

#include <wx/filename.h>
#include <wx/stdpaths.h>

#include "noo_app.h"
#include "noo_frame.h"
#include "../common/screen_layout.h"
#include "../settings.h"

enum AppEvent {
    UPDATE = 1
};

wxBEGIN_EVENT_TABLE(NooApp, wxApp)
EVT_TIMER(UPDATE, NooApp::update)
wxEND_EVENT_TABLE()

int NooApp::micEnable = 0;
int NooApp::splitScreens = 0;
int NooApp::keyBinds[] = { 'L', 'K', 'G', 'H', 'D', 'A', 'W', 'S', 'P', 'Q', 'O', 'I', WXK_TAB, 0, WXK_ESCAPE, 0, WXK_BACK };

bool NooApp::OnInit() {
    // Define the platform settings
    std::vector<Setting> platformSettings = {
        Setting("micEnable", &micEnable, false),
        Setting("splitScreens", &splitScreens, false),
        Setting("keyA", &keyBinds[0], false),
        Setting("keyB", &keyBinds[1], false),
        Setting("keySelect", &keyBinds[2], false),
        Setting("keyStart", &keyBinds[3], false),
        Setting("keyRight", &keyBinds[4], false),
        Setting("keyLeft", &keyBinds[5], false),
        Setting("keyUp", &keyBinds[6], false),
        Setting("keyDown", &keyBinds[7], false),
        Setting("keyR", &keyBinds[8], false),
        Setting("keyL", &keyBinds[9], false),
        Setting("keyX", &keyBinds[10], false),
        Setting("keyY", &keyBinds[11], false),
        Setting("keyFastHold", &keyBinds[12], false),
        Setting("keyFastToggle", &keyBinds[13], false),
        Setting("keyFullScreen", &keyBinds[14], false),
        Setting("keyScreenSwap", &keyBinds[15], false),
        Setting("keySystemPause", &keyBinds[16], false)
    };

    // Add the platform settings
    ScreenLayout::addSettings();
    Settings::add(platformSettings);

    // Try to load the settings
    if (FILE *file = fopen("noods.ini", "r")) {
        // Load from the working directory if a file exists
        fclose(file);
        Settings::load();
    }
    else {
        // Load from the system-specific application settings directory
        std::string settingsDir;
        wxStandardPaths &paths = wxStandardPaths::Get();
#if defined(WINDOWS) || defined(MACOS) || !wxCHECK_VERSION(3, 1, 0)
        settingsDir = paths.GetUserDataDir().mb_str(wxConvUTF8);
#else
        paths.SetFileLayout(wxStandardPaths::FileLayout_XDG);
        settingsDir = paths.GetUserConfigDir().mb_str(wxConvUTF8);
        settingsDir += "/noods";
#endif
        Settings::load(settingsDir);
    }

    // Create the initial frame, passing along a command line filename if given
    SetAppName("NooDS");
    frames[0] = new NooFrame(this, 0, (argc > 1) ? argv[1].ToStdString() : "");

    // Set up the update timer
    wxTimer *timer = new wxTimer(this, UPDATE);
    timer->Start(6);

    // Start the audio service and output/input streams
    Pa_Initialize();
    startStream(0);
    startStream(1);
    return true;
}

int NooApp::OnExit() {
    // Clean up the audio service and streams
    stopStream(1);
    stopStream(0);
    Pa_Terminate();

    return wxApp::OnExit();
}

void NooApp::createFrame() {
    // Create a new frame using the lowest free instance ID
    for (int i = 0; i < MAX_FRAMES; i++) {
        if (!frames[i]) {
            frames[i] = new NooFrame(this, i);
            return;
        }
    }
}

void NooApp::removeFrame(int id) {
    // Free an instance ID; this should be done on frame destruction
    frames[id] = nullptr;
}

void NooApp::connectCore(int id) {
    // Connect a frame's core to all other active cores
    for (int i = 0; i < MAX_FRAMES; i++) {
        if (!frames[i] || i == id) continue;
        if (Core *core = frames[i]->core)
            core->wifi.addConnection(frames[id]->core);
    }
}

void NooApp::disconnCore(int id) {
    // Disconnect a frame's core from all other active cores
    for (int i = 0; i < MAX_FRAMES; i++) {
        if (!frames[i] || i == id) continue;
        if (Core *core = frames[i]->core)
            core->wifi.remConnection(frames[id]->core);
    }
}

void NooApp::updateLayouts() {
    // Trigger resize events for frames to update screen layouts
    for (size_t i = 0; i < MAX_FRAMES; i++) {
        if (frames[i])
            frames[i]->SendSizeEvent();
    }
}

void NooApp::startStream(bool stream) {
    if (stream == 0) {
        if (Pa_GetDefaultOutputDevice() != paNoDevice) {
            // Initialize the audio output stream
            Pa_OpenDefaultStream(&streams[0], 0, 2, paInt16, 48000, 1024, audioCallback, frames);
            Pa_StartStream(streams[0]);
        }
    }
    else if (micEnable && Pa_GetDefaultInputDevice() != paNoDevice) {
        // Initialize the audio input stream
        Pa_OpenDefaultStream(&streams[1], 1, 0, paInt16, 48000, 1024, micCallback, frames);
        Pa_StartStream(streams[1]);
    }
}

void NooApp::stopStream(bool stream) {
    // Stop a stream if it was running
    if (streams[stream]) {
        Pa_StopStream(streams[stream]);
        Pa_CloseStream(streams[stream]);
        streams[stream] = nullptr;
    }
}

void NooApp::update(wxTimerEvent &event) {
    // Continuously refresh the frames
    for (size_t i = 0; i < MAX_FRAMES; i++) {
        if (frames[i])
            frames[i]->Refresh();
    }
}

int NooApp::audioCallback(const void *in, void *out, unsigned long count,
    const PaStreamCallbackTimeInfo *info, PaStreamCallbackFlags flags, void *data) {
    int16_t *buffer = (int16_t*)out;
    NooFrame **frames = (NooFrame**)data;
    uint32_t *original = nullptr;

    // Get samples from each instance so frame limiting is enforced
    // Only the lowest instance ID's samples are played; the rest are discarded
    for (size_t i = 0; i < MAX_FRAMES; i++) {
        if (!frames[i]) continue;
        if (Core *core = frames[i]->core) {
            uint32_t *samples = core->spu.getSamples(699);
            if (!original)
                original = samples;
            else
                delete[] samples;
        }
    }

    if (original) {
        // The NDS sample rate is 32768Hz, but it causes issues on some systems, so 48000Hz is used instead
        // Stretch 699 samples at 32768Hz to approximately 1024 samples at 48000Hz
        for (int i = 0; i < count; i++) {
            uint32_t sample = original[i * 699 / 1024];
            buffer[i * 2 + 0] = sample >>  0;
            buffer[i * 2 + 1] = sample >> 16;
        }
        delete[] original;
    }
    else {
        // Play silence if the emulator isn't running
        memset(buffer, 0, count * sizeof(uint32_t));
    }

    return paContinue;
}

int NooApp::micCallback(const void *in, void *out, unsigned long count,
    const PaStreamCallbackTimeInfo *info, PaStreamCallbackFlags flags, void *data) {
    const int16_t *buffer = (const int16_t*)in;
    NooFrame **frames = (NooFrame**)data;
    Core *core = nullptr;

    // Find the core with the lowest instance ID
    for (size_t i = 0; i < MAX_FRAMES; i++) {
        if (frames[i] && (core = frames[i]->core))
            break;
    }

    // Send microphone samples to the core
    if (core)
        core->spi.sendMicData(buffer, count, 48000);

    return paContinue;
}
