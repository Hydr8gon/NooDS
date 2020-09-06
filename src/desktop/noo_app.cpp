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

#include "noo_app.h"
#include "../common/screen_layout.h"
#include "../settings.h"

enum Event
{
    UPDATE = 1
};

wxBEGIN_EVENT_TABLE(NooApp, wxApp)
EVT_TIMER(UPDATE, NooApp::update)
wxEND_EVENT_TABLE()

int NooApp::screenFilter = 1;
int NooApp::keyBinds[] = { 'L', 'K', 'G', 'H', 'D', 'A', 'W', 'S', 'P', 'Q', 'O', 'I', WXK_TAB, WXK_ESCAPE };

bool NooApp::OnInit()
{
    // Define the platform settings
    std::vector<Setting> platformSettings =
    {
        Setting("screenFilter",   &screenFilter, false),
        Setting("keyA",           &keyBinds[0],  false),
        Setting("keyB",           &keyBinds[1],  false),
        Setting("keySelect",      &keyBinds[2],  false),
        Setting("keyStart",       &keyBinds[3],  false),
        Setting("keyRight",       &keyBinds[4],  false),
        Setting("keyLeft",        &keyBinds[5],  false),
        Setting("keyUp",          &keyBinds[6],  false),
        Setting("keyDown",        &keyBinds[7],  false),
        Setting("keyR",           &keyBinds[8],  false),
        Setting("keyL",           &keyBinds[9],  false),
        Setting("keyX",           &keyBinds[10], false),
        Setting("keyY",           &keyBinds[11], false),
        Setting("keyFastForward", &keyBinds[12], false),
        Setting("keyFullScreen",  &keyBinds[13], false)
    };

    // Load the settings
    ScreenLayout::addSettings();
    Settings::add(platformSettings);
    Settings::load();

    // Prepare a joystick if one is connected
    joystick = new wxJoystick();
    if (!joystick->IsOk())
    {
        delete joystick;
        joystick = nullptr;
    }

    // Set up the window
    // If a filename is passed through the command line, pass it along
    frame = new NooFrame(joystick, &emulator, ((argc > 1) ? argv[1].ToStdString() : ""));
    canvas = new NooCanvas(frame, &emulator);
    wxBoxSizer *sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(canvas, 1, wxEXPAND);
    frame->SetSizer(sizer);

    // Set up the update timer
    wxTimer *timer = new wxTimer(this, UPDATE);
    timer->Start(10);

    // Start the audio service
    PaStream *stream;
    Pa_Initialize();
    Pa_OpenDefaultStream(&stream, 0, 2, paInt16, 48000, 1024, audioCallback, &emulator);
    Pa_StartStream(stream);

    return true;
}

void NooApp::update(wxTimerEvent &event)
{
    // Refresh the display and update the FPS counter
    canvas->Refresh();
    frame->SetLabel(emulator.running ? wxString::Format("NooDS - %d FPS", emulator.core->getFps()) : "NooDS");
}

int NooApp::audioCallback(const void *in, void *out, unsigned long frames,
                          const PaStreamCallbackTimeInfo *info, PaStreamCallbackFlags flags, void *data)
{
    int16_t *buffer = (int16_t*)out;
    Emulator *emulator = (Emulator*)data;

    if (emulator->core)
    {
        // The NDS sample rate is 32768Hz, and PortAudio supports this frequency directly
        // It causes issues on some systems though, so a more standard frequency of 48000Hz is used instead
        // Get 699 samples at 32768Hz, which is equal to approximately 1024 samples at 48000Hz
        uint32_t *original = emulator->core->spu.getSamples(699);

        // Stretch the 699 samples out to 1024 samples in the audio buffer
        for (int i = 0; i < frames; i++)
        {
            uint32_t sample = original[i * 699 / 1024];
            buffer[i * 2 + 0] = sample >>  0;
            buffer[i * 2 + 1] = sample >> 16;
        }

        delete[] original;
    }
    else
    {
        // Play silence if the emulator isn't running
        memset(buffer, 0, frames * sizeof(uint32_t));
    }

    return 0;
}
