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

#include <algorithm>
#include <cstring>
#include <dirent.h>
#include <switch.h>
#include <thread>
#include <malloc.h>

#include "switch_ui.h"
#include "../common/screen_layout.h"
#include "../core.h"
#include "../settings.h"

const uint32_t keyMap[] =
{
    KEY_A,     KEY_B,    KEY_MINUS, KEY_PLUS,
    KEY_RIGHT, KEY_LEFT, KEY_UP,    KEY_DOWN,
    KEY_ZR,    KEY_ZL,   KEY_X,     KEY_Y,
    (KEY_L | KEY_R)
};

int screenFilter = 1;
int showFpsCounter = 0;

std::string path;

bool running = false;

ClkrstSession cpuSession;

AudioOutBuffer audioBuffers[2];
AudioOutBuffer *audioReleasedBuffer;
int16_t *audioData[2];
uint32_t count;

Core *core;
std::thread *coreThread, *audioThread;

ScreenLayout layout;
bool gbaMode = false;

void runCore()
{
    // Run the emulator
    while (running)
        core->runFrame();
}

void outputAudio()
{
    while (running)
    {
        audoutWaitPlayFinish(&audioReleasedBuffer, &count, UINT64_MAX);
        int16_t *buffer = (int16_t*)audioReleasedBuffer->buffer;

        // The NDS sample rate is 32768Hz, but audout uses 48000Hz
        // Get 699 samples at 32768Hz, which is equal to approximately 1024 samples at 48000Hz
        uint32_t *original = core->spu.getSamples(699);

        // Stretch the 699 samples out to 1024 samples in the audio buffer
        for (int i = 0; i < 1024; i++)
        {
            uint32_t sample = original[i * 699 / 1024];
            buffer[i * 2 + 0] = sample >>  0;
            buffer[i * 2 + 1] = sample >> 16;
        }

        audoutAppendAudioOutBuffer(audioReleasedBuffer);
    }
}

void startCore()
{
    if (running) return;
    running = true;

    // Overclock the Switch CPU
    clkrstInitialize();
    clkrstOpenSession(&cpuSession, PcvModuleId_CpuBus, 0);
    clkrstSetClockRate(&cpuSession, 1785000000);

    // Start audio output
    audoutInitialize();
    audoutStartAudioOut();

    // Set up the audio buffers
    for (int i = 0; i < 2; i++)
    {
        int size = 1024 * 2 * sizeof(int16_t);
        int alignedSize = (size + 0xFFF) & ~0xFFF;
        audioData[i] = (int16_t*)memalign(0x1000, size);
        memset(audioData[i], 0, alignedSize);
        audioBuffers[i].next = NULL;
        audioBuffers[i].buffer = audioData[i];
        audioBuffers[i].buffer_size = alignedSize;
        audioBuffers[i].data_size = size;
        audioBuffers[i].data_offset = 0;
        audoutAppendAudioOutBuffer(&audioBuffers[i]);
    }

    // Start the threads
    audioThread = new std::thread(outputAudio);
    coreThread = new std::thread(runCore);
}

void stopCore()
{
    if (!running) return;
    running = false;

    // Wait for the threads to stop
    coreThread->join();
    delete coreThread;
    audioThread->join();
    delete audioThread;

    // Free the audio buffers
    delete[] audioData[0];
    delete[] audioData[1];

    // Stop audio output
    audoutStopAudioOut();
    audoutExit();

    // Disable the overclock
    clkrstSetClockRate(&cpuSession, 1020000000);
    clkrstExit();
}

uint32_t *getRomIcon(std::string filename)
{
    // Attempt to open the ROM
    FILE *rom = fopen(filename.c_str(), "rb");
    if (!rom) return nullptr;

    // Get the icon offset
    uint32_t offset;
    fseek(rom, 0x68, SEEK_SET);
    fread(&offset, sizeof(uint32_t), 1, rom);

    // Get the icon data
    uint8_t data[512];
    fseek(rom, 0x20 + offset, SEEK_SET);
    fread(data, sizeof(uint8_t), 512, rom);

    // Get the icon palette
    uint16_t palette[16];
    fseek(rom, 0x220 + offset, SEEK_SET);
    fread(palette, sizeof(uint16_t), 16, rom);

    fclose(rom);

    // Get each pixel's 5-bit palette color and convert it to 8-bit
    uint32_t tiles[32 * 32];
    for (int i = 0; i < 32 * 32; i++)
    {
        uint8_t index = (i & 1) ? ((data[i / 2] & 0xF0) >> 4) : (data[i / 2] & 0x0F);
        uint8_t r = index ? (((palette[index] >>  0) & 0x1F) * 255 / 31) : 0xFFFFFFFF;
        uint8_t g = index ? (((palette[index] >>  5) & 0x1F) * 255 / 31) : 0xFFFFFFFF;
        uint8_t b = index ? (((palette[index] >> 10) & 0x1F) * 255 / 31) : 0xFFFFFFFF;
        tiles[i] = (0xFF << 24) | (b << 16) | (g << 8) | r;
    }

    uint32_t *texture = new uint32_t[32 * 32];

    // Rearrange the pixels from 8x8 tiles to a 32x32 texture
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            for (int k = 0; k < 4; k++)
                memcpy(&texture[256 * i + 32 * j + 8 * k], &tiles[256 * i + 8 * j + 64 * k], 8 * sizeof(uint32_t));
        }
    }

    return texture;
}

void settingsMenu()
{
    const std::vector<std::string> toggle      = { "Off", "On"                              };
    const std::vector<std::string> rotation    = { "None", "Clockwise", "Counter-Clockwise" };
    const std::vector<std::string> arrangement = { "Automatic", "Vertical", "Horizontal"    };
    const std::vector<std::string> sizing      = { "Even", "Enlarge Top", "Enlarge Bottom"  };

    unsigned int index = 0;

    while (true)
    {
        // Get the list of settings and current values
        std::vector<ListItem> settings =
        {
            ListItem("Direct Boot",        toggle[Settings::getDirectBoot()]),
            ListItem("FPS Limiter",        toggle[Settings::getFpsLimiter()]),
            ListItem("Threaded 2D",        toggle[Settings::getThreaded2D()]),
            ListItem("Threaded 3D",        toggle[(bool)Settings::getThreaded3D()]),
            ListItem("Screen Rotation",    rotation[ScreenLayout::getScreenRotation()]),
            ListItem("Screen Arrangement", arrangement[ScreenLayout::getScreenArrangement()]),
            ListItem("Screen Sizing",      sizing[ScreenLayout::getScreenSizing()]),
            ListItem("Screen Gap",         toggle[ScreenLayout::getScreenGap()]),
            ListItem("Integer Scale",      toggle[ScreenLayout::getIntegerScale()]),
            ListItem("GBA Crop",           toggle[ScreenLayout::getGbaCrop()]),
            ListItem("Screen Filter",      toggle[screenFilter]),
            ListItem("Show FPS Counter",   toggle[showFpsCounter])
        };

        // Create the settings menu
        Selection menu = SwitchUI::menu("Settings", &settings, index);
        index = menu.index;

        // Handle menu input
        if (menu.pressed & KEY_A)
        {
            // Change the chosen setting to its next value
            // Light FPS limiter doesn't seem to have issues, so there's no need for advanced selection
            // 1 thread for 3D seems to work best, so there's no need for advanced selection
            switch (index)
            {
                case  0: Settings::setDirectBoot(!Settings::getDirectBoot());                                break;
                case  1: Settings::setFpsLimiter(!Settings::getFpsLimiter());                                break;
                case  2: Settings::setThreaded2D(!Settings::getThreaded2D());                                break;
                case  3: Settings::setThreaded3D(!Settings::getThreaded3D());                                break;
                case  4: ScreenLayout::setScreenRotation((ScreenLayout::getScreenRotation()       + 1) % 3); break;
                case  5: ScreenLayout::setScreenArrangement((ScreenLayout::getScreenArrangement() + 1) % 3); break;
                case  6: ScreenLayout::setScreenSizing((ScreenLayout::getScreenSizing()           + 1) % 3); break;
                case  7: ScreenLayout::setScreenGap(!ScreenLayout::getScreenGap());                          break;
                case  8: ScreenLayout::setIntegerScale(!ScreenLayout::getIntegerScale());                    break;
                case  9: ScreenLayout::setGbaCrop(!ScreenLayout::getGbaCrop());                              break;
                case 10: screenFilter   = !screenFilter;                                                     break;
                case 11: showFpsCounter = !showFpsCounter;                                                   break;
            }
        }
        else
        {
            // Close the settings menu
            layout.update(1280, 720, gbaMode);
            return;
        }
    }
}

void fileBrowser()
{
    path = "sdmc:/";
    unsigned int index = 0;

    // Load the appropriate icons for the current theme
    romfsInit();
    Icon folder(SwitchUI::bmpToTexture(SwitchUI::isDarkTheme() ? "romfs:/folder-dark.bmp" : "romfs:/folder-light.bmp"), 64);
    Icon file(SwitchUI::bmpToTexture(SwitchUI::isDarkTheme() ? "romfs:/file-dark.bmp" : "romfs:/file-light.bmp"), 64);
    romfsExit();

    while (true)
    {
        std::vector<ListItem> files;
        std::vector<Icon*> icons;
        DIR *dir = opendir(path.c_str());
        dirent *entry;

        // Get all the folders and ROMs at the current path
        while ((entry = readdir(dir)))
        {
            std::string name = entry->d_name;

            if (entry->d_type == DT_DIR)
            {
                // Add a directory with a generic icon to the list
                files.push_back(ListItem(name, "", &folder));
            }
            else if (name.find(".nds", name.length() - 4) != std::string::npos)
            {
                // Add an NDS ROM with its decoded icon to the list
                icons.push_back(new Icon(getRomIcon(path + "/" + name), 32));
                files.push_back(ListItem(name, "", icons[icons.size() - 1]));
            }
            else if (name.find(".gba", name.length() - 4) != std::string::npos)
            {
                // Add a GBA ROM with a generic icon to the list
                files.push_back(ListItem(name, "", &file));
            }
        }

        closedir(dir);
        sort(files.begin(), files.end());

        // Create the file browser menu
        Selection menu = SwitchUI::menu("NooDS", &files, index, "Settings", "Exit");
        index = menu.index;

        // Free the ROM icon memory
        for (unsigned int i = 0; i < icons.size(); i++)
        {
            delete[] icons[i]->texture;
            delete[] icons[i];
        }

        // Handle menu input
        if (menu.pressed & KEY_A)
        {
            // Do nothing if there are no files to select
            if (files.empty()) continue;

            // Navigate to the selected directory
            path += "/" + files[menu.index].name;
            index = 0;

            bool gba = (path.find(".gba", path.length() - 4) != std::string::npos);

            // If a ROM was selected, attempt to boot it
            if (path.find(".nds", path.length() - 4) != std::string::npos || gba)
            {
                try
                {
                    core = new Core(path, gba);
                }
                catch (int e)
                {
                    // Handle errors during ROM boot
                    switch (e)
                    {
                        case 1: // Missing BIOS and/or firmware files
                        {
                            // Inform the user of the error
                            std::vector<std::string> message =
                            {
                                "Initialization failed.",
                                "Make sure the path settings point to valid BIOS and firmware files and try again.",
                                "You can modify the path settings in the noods.ini file."
                            };
                            SwitchUI::message("Missing BIOS/Firmware", message);

                            // Remove the ROM from the path and return to the file browser
                            path = path.substr(0, path.rfind("/"));
                            index = 0;
                            continue;
                        }

                        case 2: // Unreadable ROM file
                        {
                            // Inform the user of the error
                            std::vector<std::string> message =
                            {
                                "Initialization failed.",
                                "Make sure the ROM file is accessible and try again."
                            };
                            SwitchUI::message("Unreadable ROM", message);

                            // Remove the ROM from the path and return to the file browser
                            path = path.substr(0, path.rfind("/"));
                            index = 0;
                            continue;
                        }

                        case 3: // Missing save file
                        {
                            // Inform the user of the error
                            std::vector<std::string> message =
                            {
                                "This ROM does not have a save file.",
                                "NooDS cannot automatically detect save type, so FLASH 512KB will be assumed.",
                                "If the game fails to save, provide a save with the correct file size."
                            };
                            SwitchUI::message("Missing Save", message);

                            // Assume a save type of FLASH 512KB
                            Core::createSave(path, 7);

                            // Boot the ROM again
                            core = new Core(path, gba);
                            break;
                        }
                    }
                }

                delete[] folder.texture;
                delete[] file.texture;
                startCore();
                return;
            }
        }
        else if (menu.pressed & KEY_B)
        {
            // Navigate to the previous directory
            if (path != "sdmc:/")
            {
                path = path.substr(0, path.rfind("/"));
                index = 0;
            }
        }
        else if (menu.pressed & KEY_X)
        {
            // Open the settings menu   
            settingsMenu();
        }
        else
        {
            // Close the file browser
            return;
        }
    }
}

void pauseMenu()
{
    // Stop the core and write the save as an extra precaution
    stopCore();
    core->cartridge.writeSave();

    unsigned int index = 0;

    std::vector<ListItem> items =
    {
        ListItem("Resume"),
        ListItem("Restart"),
        ListItem("Settings"),
        ListItem("File Browser")
    };

    while (true)
    {
        // Create the pause menu
        Selection menu = SwitchUI::menu("NooDS", &items, index);
        index = menu.index;

        // Handle menu input
        if (menu.pressed & KEY_A)
        {
            // Handle the selected item
            switch (index)
            {
                case 0: // Resume
                {
                    // Return to the emulator
                    startCore();
                    return;
                }

                case 1: // Restart
                {
                    // Restart and return to the emulator
                    delete core;
                    core = new Core(path);
                    startCore();
                    return;
                }

                case 2: // Settings
                {
                    // Open the settings menu
                    settingsMenu();
                    break;
                }

                case 3: // File Browser
                {
                    // Open the file browser and close the pause menu
                    fileBrowser();
                    return;
                }
            }
        }
        else if (menu.pressed & KEY_B)
        {
            // Return to the emulator
            startCore();
            return;
        }
        else
        {
            // Close the pause menu
            return;
        }
    }
}

int main()
{
    appletLockExit();
    SwitchUI::initialize();

    // Define the platform settings
    std::vector<Setting> platformSettings =
    {
        Setting("screenFilter",   &screenFilter,   false),
        Setting("showFpsCounter", &showFpsCounter, false)
    };

    // Load the settings
    ScreenLayout::addSettings();
    Settings::add(platformSettings);
    Settings::load();

    layout.update(1280, 720, gbaMode);

    // Open the file browser
    fileBrowser();

    while (appletMainLoop() && running)
    {
        // Scan for key input
        hidScanInput();
        uint32_t pressed = hidKeysDown(CONTROLLER_P1_AUTO);
        uint32_t released = hidKeysUp(CONTROLLER_P1_AUTO);

        // Send input to the core
        for (int i = 0; i < 12; i++)
        {
            if (pressed & keyMap[i])
                core->input.pressKey(i);
            else if (released & keyMap[i])
                core->input.releaseKey(i);
        }

        // Scan for touch input
        if (hidTouchCount() > 0)
        {
            touchPosition touch;
            hidTouchRead(&touch, 0);

            // Determine the touch position relative to the emulated touch screen
            int touchX = layout.getTouchX(touch.px, touch.py);
            int touchY = layout.getTouchY(touch.px, touch.py);

            // Send the touch coordinates to the core
            core->input.pressScreen();
            core->spi.setTouch(touchX, touchY);
        }
        else
        {
            // If the screen isn't being touched, release the touch screen press
            core->input.releaseScreen();
            core->spi.clearTouch();
        }

        // Request a new frame
        bool gba = (core->isGbaMode() && ScreenLayout::getGbaCrop());
        uint32_t *framebuffer = core->gpu.getFrame(gba);

        // Update GBA mode status to match the new frame
        if (gbaMode != gba)
        {
            gbaMode = gba;
            layout.update(1280, 720, gbaMode);
        }

        // Draw the frame if it's ready
        if (framebuffer)
        {
            SwitchUI::clear(Color(0, 0, 0));

            if (gbaMode)
            {
                // Draw the GBA screen
                SwitchUI::drawImage(&framebuffer[0], 240, 160, layout.getTopX(), layout.getTopY(),
                    layout.getTopWidth(), layout.getTopHeight(), screenFilter, ScreenLayout::getScreenRotation());
            }
            else // NDS mode
            {
                // Draw the DS top screen
                SwitchUI::drawImage(&framebuffer[0], 256, 192, layout.getTopX(), layout.getTopY(),
                    layout.getTopWidth(), layout.getTopHeight(), screenFilter, ScreenLayout::getScreenRotation());

                // Draw the DS bottom screen
                SwitchUI::drawImage(&framebuffer[256 * 192], 256, 192, layout.getBotX(), layout.getBotY(),
                    layout.getBotWidth(), layout.getBotHeight(), screenFilter, ScreenLayout::getScreenRotation());
            }

            // Draw the FPS counter if enabled
            if (showFpsCounter)
                SwitchUI::drawString(std::to_string(core->getFps()) + " FPS", 5, 0, 48, Color(255, 255, 255));

            SwitchUI::update();
            delete[] framebuffer;
        }

        // Open the pause menu if requested
        if (pressed & keyMap[12])
            pauseMenu();
    }

    // Clean up
    stopCore();
    delete core;
    Settings::save();
    SwitchUI::deinitialize();
    appletUnlockExit();
    return 0;
}
