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
#include <malloc.h>

#include "switch_ui.h"
#include "../core.h"
#include "../settings.h"

const uint32_t keyMap[] =
{
    KEY_A,     KEY_B,    KEY_MINUS, KEY_PLUS,
    KEY_RIGHT, KEY_LEFT, KEY_UP,    KEY_DOWN,
    KEY_ZR,    KEY_ZL,   KEY_X,     KEY_Y
};

std::string path;

Core *core;
bool running = true;
Thread coreThread, audioThread;

AudioOutBuffer audioBuffers[2];
AudioOutBuffer *audioReleasedBuffer;
int16_t *audioData[2];
uint32_t count;

void runCore(void *args)
{
    // Run the emulator
    while (running)
        core->runFrame();
}

void outputAudio(void *args)
{
    while (running)
    {
        audoutWaitPlayFinish(&audioReleasedBuffer, &count, U64_MAX);
        int16_t *buffer = (int16_t*)audioReleasedBuffer->buffer;

        // The NDS sample rate is 32768Hz, but audout uses 48000Hz
        // Get 699 samples at 32768Hz, which is equal to approximately 1024 samples at 48000Hz
        uint32_t *original = core->getSamples(699);

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
    unsigned int index = 0;

    while (true)
    {
        // Get the list of settings and current values
        std::vector<ListItem> settings =
        {
            ListItem("Direct Boot", Settings::getDirectBoot() ? "On" : "Off"),
            ListItem("Threaded 3D", Settings::getThreaded3D() ? "On" : "Off"),
            ListItem("Limit FPS",   Settings::getLimitFps()   ? "On" : "Off")
        };

        // Create the settings menu
        Selection menu = SwitchUI::menu("Settings", &settings, index);
        index = menu.index;

        // Handle menu input
        if (menu.pressed & KEY_A)
        {
            // Toggle the chosen setting
            switch (index)
            {
                case 0: Settings::setDirectBoot(!Settings::getDirectBoot()); break;
                case 1: Settings::setThreaded3D(!Settings::getThreaded3D()); break;
                case 2: Settings::setLimitFps(!Settings::getLimitFps());     break;
            }
        }
        else if (menu.pressed & KEY_B)
        {
            // Close the settings menu
            Settings::save();
            return;
        }
    }
}

bool fileBrowser()
{
    path = "sdmc:/";
    unsigned int index = 0;
    bool exit = false;

    // Load the appropriate folder icon for the current theme
    romfsInit();
    std::string folderName = SwitchUI::isDarkTheme() ? "romfs:/folder-dark.bmp" : "romfs:/folder-light.bmp";
    Icon folder(SwitchUI::bmpToTexture(folderName), 64);
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
                // Add a ROM with its decoded icon to the list
                icons.push_back(new Icon(getRomIcon(path + "/" + name), 32));
                files.push_back(ListItem(name, "", icons[icons.size() - 1]));
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
        if ((menu.pressed & KEY_A) && files.size() > 0)
        {
            // Navigate to the selected directory
            path += "/" + files[menu.index].name;
            index = 0;

            // If a ROM was selected, attempt to boot it
            if (path.find(".nds", path.length() - 4) != std::string::npos)
            {
                try
                {
                    core = new Core(path);
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

                            // Assume a save type of FLASH 512KB and boot the ROM again
                            Core::createSave(path, 5);
                            core = new Core(path);
                            break;
                        }
                    }
                }

                break;
            }
        }
        else if ((menu.pressed & KEY_B) && path != "sdmc:/")
        {
            // Navigate to the previous directory
            path = path.substr(0, path.rfind("/"));
            index = 0;
        }
        else if (menu.pressed & KEY_X)
        {
            // Open the settings menu   
            settingsMenu();
        }
        else if (menu.pressed & KEY_PLUS)
        {
            // Request to exit the program
            exit = true;
            break;
        }
    }

    delete[] folder.texture;
    return !exit;
}

int main()
{
    SwitchUI::initialize();
    Settings::load();

    // Open the file browser
    if (!fileBrowser())
    {
        // Exit the program if the file browser requested it
        Settings::save();
        SwitchUI::deinitialize();
        return 0;
    }

    appletLockExit();

    // Overclock the Switch CPU
    ClkrstSession cpuSession;
    clkrstInitialize();
    clkrstOpenSession(&cpuSession, PcvModuleId_CpuBus, 0);
    clkrstSetClockRate(&cpuSession, 1785000000);

    audoutInitialize();
    audoutStartAudioOut();

    // Set up the audio buffer
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

    // Start the audio thread
    threadCreate(&audioThread, outputAudio, NULL, NULL, 0x8000, 0x30, 2);
    threadStart(&audioThread);

    // Start the emulator thread
    threadCreate(&coreThread, runCore, NULL, NULL, 0x8000, 0x30, 1);
    threadStart(&coreThread);

    while (appletMainLoop())
    {
        // Scan for key input
        hidScanInput();
        uint32_t pressed = hidKeysDown(CONTROLLER_P1_AUTO);
        uint32_t released = hidKeysUp(CONTROLLER_P1_AUTO);

        // Send input to the core
        for (int i = 0; i < 12; i++)
        {
            if (pressed & keyMap[i])
                core->pressKey(i);
            else if (released & keyMap[i])
                core->releaseKey(i);
        }

        // Scan for touch input
        if (hidTouchCount() > 0)
        {
            // If the screen is being touched, determine the position relative to the emulated touch screen
            touchPosition touch;
            hidTouchRead(&touch, 0);
            int touchX = (touch.px - 640) / 2;
            int touchY = (touch.py - 168) / 2;

            // Send the touch coordinates to the core
            core->pressScreen(touchX, touchY);
        }
        else
        {
            // If the screen isn't being touched, release the touch screen press
            core->releaseScreen();
        }

        uint32_t framebuffer[256 * 192 * 2];

        // Convert the framebuffer to RGBA8 format
        for (int i = 0; i < 256 * 192 * 2; i++)
        {
            uint32_t color = core->getFramebuffer()[i];
            uint8_t r = ((color >>  0) & 0x3F) * 255 / 63;
            uint8_t g = ((color >>  6) & 0x3F) * 255 / 63;
            uint8_t b = ((color >> 12) & 0x3F) * 255 / 63;
            framebuffer[i] = (0xFF << 24) | (b << 16) | (g << 8) | r;
        }

        // Draw the screens
        SwitchUI::clear(Color(0, 0, 0));
        SwitchUI::drawImage(&framebuffer[0],         256, 192, 128, 168, 256 * 2, 192 * 2, false);
        SwitchUI::drawImage(&framebuffer[256 * 192], 256, 192, 640, 168, 256 * 2, 192 * 2, false);
        SwitchUI::drawString(std::to_string(core->getFps()) + " FPS", 5, 0, 48, Color(255, 255, 255));
        SwitchUI::update();
    }

    // Clean up
    running = false;
    threadWaitForExit(&coreThread);
    threadClose(&coreThread);
    threadWaitForExit(&audioThread);
    threadClose(&audioThread);
    audoutStopAudioOut();
    audoutExit();
    clkrstSetClockRate(&cpuSession, 1020000000);
    clkrstExit();
    delete core;
    Settings::save();
    SwitchUI::deinitialize();
    appletUnlockExit();
    return 0;
}
