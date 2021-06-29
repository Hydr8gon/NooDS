/*
    Copyright 2019-2021 Hydr8gon

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
#include "../common/nds_icon.h"
#include "../common/screen_layout.h"
#include "../core.h"
#include "../settings.h"

const uint32_t keyMap[] =
{
    HidNpadButton_A,        HidNpadButton_B,       HidNpadButton_Minus,    HidNpadButton_Plus,
    HidNpadButton_AnyRight, HidNpadButton_AnyLeft, HidNpadButton_AnyUp,    HidNpadButton_AnyDown,
    HidNpadButton_ZR,       HidNpadButton_ZL,      HidNpadButton_X,        HidNpadButton_Y,
    (HidNpadButton_L | HidNpadButton_R)
};

const int clockSpeeds[] = { 1020000000, 1224000000, 1581000000, 1785000000 };

int screenFilter = 1;
int showFpsCounter = 0;
int switchOverclock = 3;

std::string ndsPath, gbaPath;

bool running = false;

ClkrstSession cpuSession;

AudioOutBuffer audioBuffers[2];
AudioOutBuffer *audioReleasedBuffer;
int16_t *audioData[2];
uint32_t count;

Core *core;
std::thread *coreThread, *audioThread;

ScreenLayout layout;
uint32_t framebuffer[256 * 192 * 2] = {};
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

        delete[] original;
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
    clkrstSetClockRate(&cpuSession, clockSpeeds[switchOverclock]);

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

void settingsMenu()
{
    const std::vector<std::string> toggle      = { "Off", "On"                                    };
    const std::vector<std::string> rotation    = { "None", "Clockwise", "Counter-Clockwise"       };
    const std::vector<std::string> arrangement = { "Automatic", "Vertical", "Horizontal"          };
    const std::vector<std::string> sizing      = { "Even", "Enlarge Top", "Enlarge Bottom"        };
    const std::vector<std::string> gap         = { "None", "Quarter", "Half", "Full"              };
    const std::vector<std::string> overclock   = { "1020 MHz", "1224 MHz", "1581 MHz", "1785 MHz" };

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
            ListItem("Screen Gap",         gap[ScreenLayout::getScreenGap()]),
            ListItem("Integer Scale",      toggle[ScreenLayout::getIntegerScale()]),
            ListItem("GBA Crop",           toggle[ScreenLayout::getGbaCrop()]),
            ListItem("Screen Filter",      toggle[screenFilter]),
            ListItem("Show FPS Counter",   toggle[showFpsCounter]),
            ListItem("Switch Overclock",   overclock[switchOverclock])
        };

        // Create the settings menu
        Selection menu = SwitchUI::menu("Settings", &settings, index);
        index = menu.index;

        // Handle menu input
        if (menu.pressed & HidNpadButton_A)
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
                case  7: ScreenLayout::setScreenGap((ScreenLayout::getScreenGap()                 + 1) % 4); break;
                case  8: ScreenLayout::setIntegerScale(!ScreenLayout::getIntegerScale());                    break;
                case  9: ScreenLayout::setGbaCrop(!ScreenLayout::getGbaCrop());                              break;
                case 10: screenFilter   = !screenFilter;                                                     break;
                case 11: showFpsCounter = !showFpsCounter;                                                   break;
                case 12: switchOverclock = (switchOverclock + 1) % 4;                                        break;
            }
        }
        else
        {
            // Close the settings menu
            layout.update(1280, 720, gbaMode);
            Settings::save();
            return;
        }
    }
}

void fileBrowser()
{
    std::string path = "sdmc:/";
    unsigned int index = 0;

    // Load the appropriate icons for the current theme
    romfsInit();
    uint32_t *file   = SwitchUI::bmpToTexture(SwitchUI::isDarkTheme() ? "romfs:/file-dark.bmp"   : "romfs:/file-light.bmp");
    uint32_t *folder = SwitchUI::bmpToTexture(SwitchUI::isDarkTheme() ? "romfs:/folder-dark.bmp" : "romfs:/folder-light.bmp");
    romfsExit();

    while (true)
    {
        std::vector<ListItem> files;
        std::vector<NdsIcon*> icons;
        DIR *dir = opendir(path.c_str());
        dirent *entry;

        // Get all the folders and ROMs at the current path
        while ((entry = readdir(dir)))
        {
            std::string name = entry->d_name;

            if (entry->d_type == DT_DIR)
            {
                // Add a directory with a generic icon to the list
                files.push_back(ListItem(name, "", folder, 64));
            }
            else if (name.find(".nds", name.length() - 4) != std::string::npos)
            {
                // Add an NDS ROM with its decoded icon to the list
                icons.push_back(new NdsIcon(path + "/" + name));
                files.push_back(ListItem(name, "", icons[icons.size() - 1]->getIcon(), 32));
            }
            else if (name.find(".gba", name.length() - 4) != std::string::npos)
            {
                // Add a GBA ROM with a generic icon to the list
                files.push_back(ListItem(name, "", file, 64));
            }
        }

        closedir(dir);
        sort(files.begin(), files.end());

        // Create the file browser menu
        Selection menu = SwitchUI::menu("NooDS", &files, index, "Settings", "Exit");
        index = menu.index;

        // Free the NDS icon memory
        for (unsigned int i = 0; i < icons.size(); i++)
            delete[] icons[i];

        // Handle menu input
        if (menu.pressed & HidNpadButton_A)
        {
            // Do nothing if there are no files to select
            if (files.empty()) continue;

            // Navigate to the selected directory
            path += "/" + files[menu.index].name;
            index = 0;

            // Check if a ROM was selected, and set the NDS or GBA ROM path depending on the file extension
            // If a ROM of the other type is already loaded, ask if it should be loaded alongside the new ROM
            if (path.find(".nds", path.length() - 4) != std::string::npos) // NDS ROM
            {
                if (gbaPath != "")
                {
                    if (!SwitchUI::message("Loading NDS ROM", std::vector<std::string>{"Load the previous GBA ROM alongside this ROM?"}, true))
                        gbaPath = "";
                }
                ndsPath = path;
            }
            else if (path.find(".gba", path.length() - 4) != std::string::npos) // GBA ROM
            {
                if (ndsPath != "")
                {
                    if (!SwitchUI::message("Loading GBA ROM", std::vector<std::string>{"Load the previous NDS ROM alongside this ROM?"}, true))
                        ndsPath = "";
                }
                gbaPath = path;
            }
            else
            {
                continue;
            }

            // If a ROM was selected, attempt to boot it
            try
            {
                core = new Core(ndsPath, gbaPath);
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
                }
            }

            delete[] folder;
            delete[] file;
            startCore();
            return;
        }
        else if (menu.pressed & HidNpadButton_B)
        {
            // Navigate to the previous directory
            if (path != "sdmc:/")
            {
                path = path.substr(0, path.rfind("/"));
                index = 0;
            }
        }
        else if (menu.pressed & HidNpadButton_X)
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

void saveTypeMenu()
{
    unsigned int index = 0;
    std::vector<ListItem> items;

    if (core->isGbaMode())
    {
        // Set up list items for GBA save types
        items.push_back(ListItem("None"));
        items.push_back(ListItem("EEPROM 0.5KB"));
        items.push_back(ListItem("EEPROM 8KB"));
        items.push_back(ListItem("SRAM 32KB"));
        items.push_back(ListItem("FLASH 64KB"));
        items.push_back(ListItem("FLASH 128KB"));
    }
    else
    {
        // Set up list items for NDS save types
        items.push_back(ListItem("None"));
        items.push_back(ListItem("EEPROM 0.5KB"));
        items.push_back(ListItem("EEPROM 8KB"));
        items.push_back(ListItem("EEPROM 64KB"));
        items.push_back(ListItem("EEPROM 128KB"));
        items.push_back(ListItem("FRAM 32KB"));
        items.push_back(ListItem("FLASH 256KB"));
        items.push_back(ListItem("FLASH 512KB"));
        items.push_back(ListItem("FLASH 1024KB"));
        items.push_back(ListItem("FLASH 8192KB"));
    }

    while (true)
    {
        // Create the save type menu
        Selection menu = SwitchUI::menu("Change Save Type", &items, index);
        index = menu.index;

        // Handle menu input
        if (menu.pressed & HidNpadButton_A)
        {
            // Confirm the change because accidentally resizing a working save file could be bad!
            if (!SwitchUI::message("Changing Save Type", std::vector<std::string>{"Are you sure? This may result in data loss!"}, true))
                continue;

            // Apply the change
            if (core->isGbaMode())
            {
                switch (index)
                {
                    case 0: core->cartridgeGba.resizeSave(0);       break; // None
                    case 1: core->cartridgeGba.resizeSave(0x200);   break; // EEPROM 0.5KB
                    case 2: core->cartridgeGba.resizeSave(0x2000);  break; // EEPROM 8KB
                    case 3: core->cartridgeGba.resizeSave(0x8000);  break; // SRAM 32KB
                    case 4: core->cartridgeGba.resizeSave(0x10000); break; // FLASH 64KB
                    case 5: core->cartridgeGba.resizeSave(0x20000); break; // FLASH 128KB
                }
            }
            else
            {
                switch (index)
                {
                    case 0: core->cartridgeNds.resizeSave(0);        break; // None
                    case 1: core->cartridgeNds.resizeSave(0x200);    break; // EEPROM 0.5KB
                    case 2: core->cartridgeNds.resizeSave(0x2000);   break; // EEPROM 8KB
                    case 3: core->cartridgeNds.resizeSave(0x10000);  break; // EEPROM 64KB
                    case 4: core->cartridgeNds.resizeSave(0x20000);  break; // EEPROM 128KB
                    case 5: core->cartridgeNds.resizeSave(0x8000);   break; // FRAM 32KB
                    case 6: core->cartridgeNds.resizeSave(0x40000);  break; // FLASH 256KB
                    case 7: core->cartridgeNds.resizeSave(0x80000);  break; // FLASH 512KB
                    case 8: core->cartridgeNds.resizeSave(0x100000); break; // FLASH 1024KB
                    case 9: core->cartridgeNds.resizeSave(0x800000); break; // FLASH 8192KB
                }
            }

            // Restart the core
            delete core;
            core = new Core(ndsPath, gbaPath);
        }

        // Return to the pause menu
        return;
    }
}

void pauseMenu()
{
    // Stop the core and write the save as an extra precaution
    stopCore();
    core->cartridgeNds.writeSave();
    core->cartridgeGba.writeSave();

    unsigned int index = 0;

    std::vector<ListItem> items =
    {
        ListItem("Resume"),
        ListItem("Restart"),
        ListItem("Change Save Type"),
        ListItem("Settings"),
        ListItem("File Browser")
    };

    while (true)
    {
        // Create the pause menu
        Selection menu = SwitchUI::menu("NooDS", &items, index);
        index = menu.index;

        // Handle menu input
        if (menu.pressed & HidNpadButton_A)
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
                    core = new Core(ndsPath, gbaPath);
                    startCore();
                    return;
                }

                case 2: // Change Save Type
                {
                    // Open the save type menu
                    saveTypeMenu();
                    break;
                }

                case 3: // Settings
                {
                    // Open the settings menu
                    settingsMenu();
                    break;
                }

                case 4: // File Browser
                {
                    // Open the file browser and close the pause menu
                    fileBrowser();
                    return;
                }
            }
        }
        else if (menu.pressed & HidNpadButton_B)
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
        Setting("screenFilter",    &screenFilter,    false),
        Setting("showFpsCounter",  &showFpsCounter,  false),
        Setting("switchOverclock", &switchOverclock, false)
    };

    // Load the settings
    ScreenLayout::addSettings();
    Settings::add(platformSettings);
    if (!Settings::load()) Settings::save();

    layout.update(1280, 720, gbaMode);

    // Open the file browser
    fileBrowser();

    PadState pad;
    padInitializeDefault(&pad);

    while (appletMainLoop() && running)
    {
        // Scan for key input
        padUpdate(&pad);
        uint32_t pressed = padGetButtonsDown(&pad);
        uint32_t released = padGetButtonsUp(&pad);

        // Send input to the core
        for (int i = 0; i < 12; i++)
        {
            if (pressed & keyMap[i])
                core->input.pressKey(i);
            else if (released & keyMap[i])
                core->input.releaseKey(i);
        }

        // Scan for touch input
        HidTouchScreenState touch;
        hidGetTouchScreenStates(&touch, 1);

        if (touch.count > 0) // Pressed
        {
            // Determine the touch position relative to the emulated touch screen
            int touchX = layout.getTouchX(touch.touches[0].x, touch.touches[0].y);
            int touchY = layout.getTouchY(touch.touches[0].x, touch.touches[0].y);

            // Send the touch coordinates to the core
            core->input.pressScreen();
            core->spi.setTouch(touchX, touchY);
        }
        else // Released
        {
            // If the screen isn't being touched, release the touch screen press
            core->input.releaseScreen();
            core->spi.clearTouch();
        }

        // Draw a new frame if one is ready
        bool gba = (core->isGbaMode() && ScreenLayout::getGbaCrop());
        if (core->gpu.getFrame(framebuffer, gba))
        {
            // Update the layout if GBA mode changed
            if (gbaMode != gba)
            {
                gbaMode = gba;
                layout.update(1280, 720, gbaMode);
            }

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
        }

        // Open the pause menu if requested
        if (pressed & keyMap[12])
            pauseMenu();
    }

    // Clean up
    stopCore();
    delete core;
    SwitchUI::deinitialize();
    appletUnlockExit();
    return 0;
}
