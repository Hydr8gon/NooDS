/*
    Copyright 2019-2026 Hydr8gon

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

#pragma once

#include "../../core/core.h"
#include "../nds_icon.h"
#include "../screen_layout.h"

namespace CoreBridge {
    extern int showFpsCounter;
    extern int buttonScale;
    extern int buttonSpacing;
    extern int vibrateStrength;
    extern std::string keyBinds[15];

    bool loadSettings(const char *path);
    int loadRom(const char *ndsPath, const char *gbaPath);
    void resizeSave(int size);

    StateResult checkState();
    bool saveState();
    bool loadState();

    void start();
    void stop();

    void updateFrame();
    void getSamples(void *buffer, uint32_t count);
    bool getGbaMode();
    int getFps();

    void pressKey(int key);
    void releaseKey(int key);
    void pressScreen(int x, int y);
    void releaseScreen();

    int bytesCb(void *info, void *buffer, int count);
    int forwardCb(void *info, int count);
    void rewindCb(void *info);
};
