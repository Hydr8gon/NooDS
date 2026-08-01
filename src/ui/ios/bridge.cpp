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

#include "bridge.h"

int CoreBridge::showFpsCounter = 0;
int CoreBridge::buttonScale = 5;
int CoreBridge::buttonSpacing = 5;
int CoreBridge::vibrateStrength = 2;
std::string CoreBridge::keyBinds[15] = {};

static std::mutex mutex;
static Core *core = nullptr;
static std::thread *thread = nullptr;
static bool running = false;

static uint32_t framebuffer[256 * 192 * 8];
static uint32_t *fbCur[] = { framebuffer, framebuffer };

static void runCore() {
    // Run the emulator
    while (running)
        core->runCore();
}

static void prefixPath(std::string &setting, std::string &prefix, const char *reset) {
    // Update a path setting's prefix, or reset the value if invalid
    if (setting.size() > prefix.size())
        strncpy(&setting[0], &prefix[0], prefix.size() - 1);
    else
        setting = prefix + reset;
}

bool CoreBridge::loadSettings(const char *path) {
    // Define and add the platform settings
    std::vector<Setting> platformSettings = {
        Setting("showFpsCounter", &showFpsCounter, false),
        Setting("buttonScale", &buttonScale, false),
        Setting("buttonSpacing", &buttonSpacing, false),
        Setting("vibrateStrength", &vibrateStrength, false),
        Setting("keyA", &keyBinds[0], true),
        Setting("keyB", &keyBinds[1], true),
        Setting("keySelect", &keyBinds[2], true),
        Setting("keyStart", &keyBinds[3], true),
        Setting("keyRight", &keyBinds[4], true),
        Setting("keyLeft", &keyBinds[5], true),
        Setting("keyUp", &keyBinds[6], true),
        Setting("keyDown", &keyBinds[7], true),
        Setting("keyR", &keyBinds[8], true),
        Setting("keyL", &keyBinds[9], true),
        Setting("keyX", &keyBinds[10], true),
        Setting("keyY", &keyBinds[11], true),
        Setting("keyFastHold", &keyBinds[12], true),
        Setting("keyFastToggle", &keyBinds[13], true),
        Setting("keyScreenSwap", &keyBinds[14], true)
    };
    ScreenLayout::addSettings();
    Settings::add(platformSettings);

    // Load settings and update path prefixes in case the app UUID changed
    std::string path2 = path;
    if (!Settings::load(path2)) return false;
    prefixPath(Settings::gbaBiosPath, path2, "/gba_bios.bin");
    prefixPath(Settings::ndsBios9Path, path2, "/bios9.bin");
    prefixPath(Settings::ndsBios7Path, path2, "/bios7.bin");
    prefixPath(Settings::ndsFirmPath, path2, "/firmware.bin");
    prefixPath(Settings::dsiBios9Path, path2, "/bios9i.bin");
    prefixPath(Settings::dsiBios7Path, path2, "/bios7i.bin");
    prefixPath(Settings::dsiFirmPath, path2, "/firmwarei.bin");
    prefixPath(Settings::dsiNandPath, path2, "/nand.bin");
    prefixPath(Settings::sdImagePath, path2, "/sd.img");
    return true;
}

uint32_t CoreBridge::loadRom(const char *ndsPath, const char *gbaPath) {
    // Clean up the old core
    mutex.lock();
    delete core;
    core = nullptr;
    mutex.unlock();

    // Try to create a new core with the given paths
    try {
        core = new Core(ndsPath, gbaPath);
        return 0;
    }
    catch (CoreError e) {
        return e;
    }
}

void CoreBridge::resizeSave(int size) {
    // Resize a GBA or NDS save based on what's running
    if (core->gbaMode)
        core->cartridgeGba.resizeSave(size);
    else
        core->cartridgeNds.resizeSave(size);
}

StateResult CoreBridge::checkState() {
    // Get the status of a state file
    return core->saveStates.checkState();
}

bool CoreBridge::saveState() {
    // Save a state file
    return core->saveStates.saveState();
}

bool CoreBridge::loadState() {
    // Load a state file
    return core->saveStates.loadState();
}

void CoreBridge::start() {
    // Start the core thread if stopped
    if (running) return;
    running = true;
    thread = new std::thread(&runCore);
}

void CoreBridge::stop() {
    // Stop the core thread if started
    if (!running) return;
    running = false;
    thread->join();
    delete thread;
}

void CoreBridge::updateFrame() {
    // Update the framebuffer used by the image callbacks
    if (!core) return;
    bool gba = core->gbaMode && ScreenLayout::gbaCrop;
    core->gpu.getFrame(framebuffer, gba);
}

void CoreBridge::getSamples(void *buffer, uint32_t count) {
    // Return an empty buffer if the core isn't active
    mutex.lock();
    if (!core) {
        mutex.unlock();
        memset(buffer, 0, count * sizeof(uint32_t));
        return;
    }

    // Fill an audio buffer with core data resampled to 44100Hz
    uint32_t scale = (count & ~0x1) * 32768 / 44100; // Rounded for consistency
    uint32_t *samples = core->spu.getSamples(scale);
    mutex.unlock();
    for (int i = 0; i < count; i++)
        ((uint32_t*)buffer)[i] = samples[i * scale / count];
    delete[] samples;
}

bool CoreBridge::getGbaMode() {
    // Check if the core is in GBA mode
    return core ? core->gbaMode : false;
}

int CoreBridge::getFps() {
    // Read the core's FPS counter
    return core ? core->fps : 0;
}

void CoreBridge::pressKey(int key) {
    // Press a key
    core->input.pressKey(key);
}

void CoreBridge::releaseKey(int key) {
    // Release a key
    core->input.releaseKey(key);
}

void CoreBridge::pressScreen(int x, int y) {
    // Press the screen and set coordinates
    core->input.pressScreen();
    core->spi.setTouch(x, y);
}

void CoreBridge::releaseScreen() {
    // Release the screen and clear coordinates
    core->input.releaseScreen();
    core->spi.clearTouch();
}

int CoreBridge::bytesCb(void *info, void *buffer, int count) {
    // Copy top/bottom framebuffer bytes to an image and move the pointer
    uint8_t i = *(uint8_t*)info;
    int s = (Settings::highRes3D || Settings::screenFilter == 1) ? 2 : 0;
    uintptr_t end = uintptr_t(framebuffer + ((256 * 192 * (i + 1)) << s));
    count = std::max(0, std::min<int>(end - uintptr_t(fbCur[i]), count));
    memcpy(buffer, fbCur[i], count);
    fbCur[i] = (uint32_t*)((uint8_t*)fbCur[i] + count);
    return count;
}

int CoreBridge::forwardCb(void *info, int count) {
    // Move the top/bottom framebuffer pointer forward
    uint8_t i = *(uint8_t*)info;
    int s = (Settings::highRes3D || Settings::screenFilter == 1) ? 2 : 0;
    uintptr_t end = uintptr_t(framebuffer + ((256 * 192 * (i + 1)) << s));
    count = std::max(0, std::min<int>(end - uintptr_t(fbCur[i]), count));
    fbCur[i] = (uint32_t*)((uint8_t*)fbCur[i] + count);
    return count;
}

void CoreBridge::rewindCb(void *info) {
    // Reset the top/bottom framebuffer pointer
    uint8_t i = *(uint8_t*)info;
    int s = (Settings::highRes3D || Settings::screenFilter == 1) ? 2 : 0;
    fbCur[i] = framebuffer + ((256 * 192 * i) << s);
}
