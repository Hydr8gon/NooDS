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
    KEY_ZR,    KEY_ZL,   KEY_X,     KEY_Y,
    (KEY_L | KEY_R)
};

int screenRotation    = 0;
int screenArrangement = 2;
int screenSizing      = 0;
int screenGap         = 0;
int screenFilter      = 1;
int integerScale      = 0;

std::string path;

bool running = false;

ClkrstSession cpuSession;

AudioOutBuffer audioBuffers[2];
AudioOutBuffer *audioReleasedBuffer;
int16_t *audioData[2];
uint32_t count;

Core *core;
Thread coreThread, audioThread;

int topX, botX;
int topY, botY;
int topWidth, botWidth;
int topHeight, botHeight;

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
        audoutWaitPlayFinish(&audioReleasedBuffer, &count, UINT64_MAX);
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

    // Start the audio thread
    threadCreate(&audioThread, outputAudio, NULL, NULL, 0x8000, 0x30, 2);
    threadStart(&audioThread);

    // Start the emulator thread
    threadCreate(&coreThread, runCore, NULL, NULL, 0x8000, 0x30, 1);
    threadStart(&coreThread);
}

void stopCore()
{
    if (!running) return;
    running = false;

    // Wait for the emulator thread to stop
    threadWaitForExit(&coreThread);
    threadClose(&coreThread);

    // Wait for the audio thread to stop
    threadWaitForExit(&audioThread);
    threadClose(&audioThread);

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

void updateLayout()
{
    // Determine the screen arrangement based on the current settings
    // In automatic mode, the arrangement is horizontal if rotated and vertical otherwise
    bool vertical = (screenArrangement == 1 || (screenArrangement == 0 && screenRotation == 0));

    // Determine the screen dimensions based on the current rotation
    int width  = (screenRotation ? 192 : 256);
    int height = (screenRotation ? 256 : 192);

    // Use constant window dimensions (the Switch's resolution)
    const int winWidth = 1280;
    const int winHeight = 720;

    float largeScale, smallScale;

    // Calculate the scale of each screen
    // When calculating scale, if the window is wider than the screen, the screen is scaled to the height of the window
    // If the window is taller than the screen, the screen is scaled to the width of the window
    // If gap is enabled, each screen is given half of the gap as extra weight for scaling
    // This results in a gap that is scaled with the screens, and averages if the screens are different scales
    if (vertical)
    {
        // Add the extra gap weight if enabled
        if (screenGap)
            height += 48;

        if (screenSizing == 0) // Even
        {
            // Scale both screens to the size of the window
            float baseRatio = (float)width / (height * 2);
            float screenRatio = (float)winWidth / winHeight;
            largeScale = ((baseRatio > screenRatio) ? ((float)winWidth / width) : ((float)winHeight / (height * 2)));
            if (integerScale) largeScale = (int)largeScale;
            smallScale = largeScale;
        }
        else // Enlarge Top/Bottom
        {
            float baseRatio = (float)width / height;

            // Scale the large screen to the size of the window minus room for the smaller screen
            float largeRatio = (float)winWidth / (winHeight - height);
            largeScale = ((baseRatio > largeRatio) ? ((float)winWidth / width) : ((float)(winHeight - height) / height));
            if (integerScale) largeScale = (int)largeScale;

            // Scale the small screen to the remaining window space
            float smallRatio = (float)winWidth / (winHeight - largeScale * height);
            smallScale = ((baseRatio > smallRatio) ? ((float)winWidth / width) : ((float)(winHeight - largeScale * height) / height));
            if (integerScale) smallScale = (int)smallScale;
        }

        // Remove the extra gap weight for the next calculations
        if (screenGap)
            height -= 48;
    }
    else // Horizontal
    {
        // Add the extra gap weight if enabled
        if (screenGap)
            width += 48;

        if (screenSizing == 0) // Even
        {
            // Scale both screens to the size of the window
            float baseRatio = (float)(width * 2) / height;
            float screenRatio = (float)winWidth / winHeight;
            largeScale = ((baseRatio > screenRatio) ? ((float)winWidth / (width * 2)) : ((float)winHeight / height));
            if (integerScale) largeScale = (int)largeScale;
            smallScale = largeScale;
        }
        else // Enlarge Top/Enlarge Bottom
        {
            float baseRatio = (float)width / height;

            // Scale the large screen to the size of the window minus room for the smaller screen
            float largeRatio = (float)(winWidth - width) / winHeight;
            largeScale = ((baseRatio > largeRatio) ? ((float)(winWidth - width) / width) : ((float)winHeight / height));
            if (integerScale) largeScale = (int)largeScale;

            // Scale the small screen to the remaining window space
            float smallRatio = (float)(winWidth - largeScale * width) / winHeight;
            smallScale = ((baseRatio > smallRatio) ? ((float)(winWidth - largeScale * width) / width) : ((float)winHeight / height));
            if (integerScale) smallScale = (int)smallScale;
        }

        // Remove the extra gap weight for the next calculations
        if (screenGap)
            width -= 48;
    }

    // Calculate the dimensions of each screen
    if (screenSizing == 1) // Enlarge Top
    {
        topWidth  = largeScale * width;
        botWidth  = smallScale * width;
        topHeight = largeScale * height;
        botHeight = smallScale * height;
    }
    else // Even/Enlarge Bottom
    {
        topWidth  = smallScale * width;
        botWidth  = largeScale * width;
        topHeight = smallScale * height;
        botHeight = largeScale * height;
    }

    // Calculate the positions of each screen
    // The screens are centered and placed next to each other either vertically or horizontally
    if (vertical)
    {
        topX = (winWidth - topWidth) / 2;
        botX = (winWidth - botWidth) / 2;

        // Swap the screens if rotated clockwise to keep the top above the bottom
        if (screenRotation == 1) // Clockwise
        {
            botY = (winHeight - botHeight - topHeight) / 2;
            topY = botY + botHeight;

            // Add the gap between the screens if enabled
            if (screenGap)
            {
                botY -= (largeScale * 48 + smallScale * 48) / 2;
                topY += (largeScale * 48 + smallScale * 48) / 2;
            }
        }
        else // None/Counter-Clockwise
        {
            topY = (winHeight - topHeight - botHeight) / 2;
            botY = topY + topHeight;

            // Add the gap between the screens if enabled
            if (screenGap)
            {
                topY -= (largeScale * 48 + smallScale * 48) / 2;
                botY += (largeScale * 48 + smallScale * 48) / 2;
            }
        }
    }
    else // Horizontal
    {
        topY = (winHeight - topHeight) / 2;
        botY = (winHeight - botHeight) / 2;

        // Swap the screens if rotated clockwise to keep the top above the bottom
        if (screenRotation == 1) // Clockwise
        {
            botX = (winWidth - botWidth - topWidth) / 2;
            topX = botX + botWidth;

            // Add the gap between the screens if enabled
            if (screenGap)
            {
                botX -= (largeScale * 48 + smallScale * 48) / 2;
                topX += (largeScale * 48 + smallScale * 48) / 2;
            }
        }
        else // None/Counter-Clockwise
        {
            topX = (winWidth - topWidth - botWidth) / 2;
            botX = topX + topWidth;

            // Add the gap between the screens if enabled
            if (screenGap)
            {
                topX -= (largeScale * 48 + smallScale * 48) / 2;
                botX += (largeScale * 48 + smallScale * 48) / 2;
            }
        }
    }
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
            ListItem("Threaded 3D",        toggle[Settings::getThreaded3D()]),
            ListItem("Limit FPS",          toggle[Settings::getLimitFps()]),
            ListItem("Screen Rotation",    rotation[screenRotation]),
            ListItem("Screen Arrangement", arrangement[screenArrangement]),
            ListItem("Screen Sizing",      sizing[screenSizing]),
            ListItem("Screen Gap",         toggle[screenGap]),
            ListItem("Screen Filter",      toggle[screenFilter]),
            ListItem("Integer Scale",      toggle[integerScale])
        };

        // Create the settings menu
        Selection menu = SwitchUI::menu("Settings", &settings, index);
        index = menu.index;

        // Handle menu input
        if (menu.pressed & KEY_A)
        {
            // Change the chosen setting to its next value
            switch (index)
            {
                case 0: Settings::setDirectBoot(!Settings::getDirectBoot()); break;
                case 1: Settings::setThreaded3D(!Settings::getThreaded3D()); break;
                case 2: Settings::setLimitFps(!Settings::getLimitFps());     break;
                case 3: screenRotation    = (screenRotation    + 1) % 3;     break;
                case 4: screenArrangement = (screenArrangement + 1) % 3;     break;
                case 5: screenSizing      = (screenSizing      + 1) % 3;     break;
                case 6: screenGap         = !screenGap;                      break;
                case 7: screenFilter      = !screenFilter;                   break;
                case 8: integerScale      = !integerScale;                   break;
            }
        }
        else
        {
            // Close the settings menu
            updateLayout();
            return;
        }
    }
}

void fileBrowser()
{
    path = "sdmc:/";
    unsigned int index = 0;

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
        if (menu.pressed & KEY_A)
        {
            // Do nothing if there are no files to select
            if (files.empty()) continue;

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

                            // Assume a save type of FLASH 512KB
                            Core::createSave(path, 5);

                            // Remove the ROM from the path and return to the file browser
                            path = path.substr(0, path.rfind("/"));
                            index = 0;
                            continue;
                        }
                    }
                }

                delete[] folder.texture;
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
    stopCore();

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
        Setting("screenRotation",    &screenRotation,    false),
        Setting("screenArrangement", &screenArrangement, false),
        Setting("screenSizing",      &screenSizing,      false),
        Setting("screenGap",         &screenGap,         false),
        Setting("screenFilter",      &screenFilter,      false),
        Setting("integerScale",      &integerScale,      false)
    };

    // Load the settings
    Settings::load(platformSettings);
    updateLayout();

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
                core->pressKey(i);
            else if (released & keyMap[i])
                core->releaseKey(i);
        }

        // Scan for touch input
        if (hidTouchCount() > 0)
        {
            touchPosition touch;
            hidTouchRead(&touch, 0);
            int touchX, touchY;

            // If the screen is being touched, determine the position relative to the emulated touch screen
            switch (screenRotation)
            {
                case 0: // None
                {
                    touchX = ((int)touch.px - botX) * 256 / botWidth;
                    touchY = ((int)touch.py - botY) * 192 / botHeight;
                    break;
                }

                case 1: // Clockwise
                {
                    touchX =       ((int)touch.py - botY) * 256 / botHeight;
                    touchY = 191 - ((int)touch.px - botX) * 192 / botWidth;
                    break;
                }

                case 2: // Counter-clockwise
                {
                    touchX = 255 - ((int)touch.py - botY) * 256 / botHeight;
                    touchY =       ((int)touch.px - botX) * 192 / botWidth;
                    break;
                }
            }

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

        SwitchUI::clear(Color(0, 0, 0));

        // Draw the screens
        SwitchUI::drawImage(&framebuffer[0],         256, 192, topX, topY, topWidth, topHeight, screenFilter, screenRotation);
        SwitchUI::drawImage(&framebuffer[256 * 192], 256, 192, botX, botY, botWidth, botHeight, screenFilter, screenRotation);

        // Draw the FPS counter
        SwitchUI::drawString(std::to_string(core->getFps()) + " FPS", 5, 0, 48, Color(255, 255, 255));

        SwitchUI::update();

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
