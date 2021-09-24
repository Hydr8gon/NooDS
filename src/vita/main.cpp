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
#include <thread>

#include <psp2/audioout.h>
#include <psp2/ctrl.h>
#include <psp2/display.h>
#include <psp2/registrymgr.h> 
#include <psp2/touch.h>
#include <psp2/io/dirent.h> 
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/power.h>

#include <vita2d.h>

#include "../core.h"
#include "../settings.h"
#include "../common/screen_layout.h"

#define COLOR_CLEAR RGBA8(  0,   0,   0, 255)
#define COLOR_TEXT1 RGBA8(255, 255, 255, 255)
#define COLOR_TEXT2 RGBA8(200, 200, 200, 255)
#define COLOR_TEXT3 RGBA8(200, 200, 255, 255)

// Reserve 128MB of allocatable memory (can do more, but loading larger ROMs into RAM is slow)
int _newlib_heap_size_user = 128 * 1024 * 1024;

const uint32_t keyMap[] =
{
    SCE_CTRL_CIRCLE,   SCE_CTRL_CROSS,    SCE_CTRL_SELECT,   SCE_CTRL_START,
    SCE_CTRL_RIGHT,    SCE_CTRL_LEFT,     SCE_CTRL_UP,       SCE_CTRL_DOWN,
    SCE_CTRL_RTRIGGER, SCE_CTRL_LTRIGGER, SCE_CTRL_TRIANGLE, SCE_CTRL_SQUARE
};

int screenFilter = 1;
int showFpsCounter = 0;

uint32_t confirmButton, cancelButton;
vita2d_pgf *pgf;

std::string ndsPath, gbaPath;
Core *core;

bool running = false;
SceUID eventFlag;
std::thread *coreThread, *audioThread, *saveThread;

ScreenLayout layout;
uint32_t framebuffer[256 * 192 * 2];
bool gbaMode = false;

uint32_t audioBuffer[1024];
int audioPort = 0;

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
        // The NDS sample rate is 32768Hz, but the Vita doesn't support this, so 48000Hz is used
        // Get 699 samples at 32768Hz, which is equal to approximately 1024 samples at 48000Hz
        uint32_t *original = core->spu.getSamples(699);

        // Stretch the 699 samples out to 1024 samples in the audio buffer
        for (int i = 0; i < 1024; i++)
            audioBuffer[i] = original[i * 699 / 1024];

        delete[] original;
        sceAudioOutOutput(audioPort, audioBuffer);
    }
}

void checkSave()
{
    while (running)
    {
        // Check save files every second and update them if changed
        SceUInt timeout = 1000000;
        sceKernelWaitEventFlag(eventFlag, 1, SCE_EVENT_WAITOR | SCE_EVENT_WAITCLEAR_PAT, nullptr, &timeout);
        core->cartridgeNds.writeSave();
        core->cartridgeGba.writeSave();
    }
}

void startCore()
{
    scePowerSetArmClockFrequency(444);

    // Start the threads
    running = true;
    coreThread  = new std::thread(runCore);
    audioThread = new std::thread(outputAudio);
    saveThread  = new std::thread(checkSave);
}

void stopCore()
{
    running = false;
    sceKernelSetEventFlag(eventFlag, 1);

    // Wait for the threads to stop
    coreThread->join();
    delete coreThread;
    audioThread->join();
    delete audioThread;
    saveThread->join();
    delete saveThread;

    scePowerSetArmClockFrequency(333);
}

uint32_t menu(std::string title, std::string subtitle, std::vector<std::string> *items,
    std::vector<std::string> *subitems, unsigned int *selection, uint32_t buttonMask)
{
    // Ignore any buttons that were already pressed
    uint32_t buttons = 0xFFFFFFFF;

    while (true)
    {
        vita2d_start_drawing();
        vita2d_clear_screen();

        // Draw the title
        vita2d_pgf_draw_text(pgf, 5, 20, COLOR_TEXT1, 1.0f, title.c_str());

        // If there's a subtitle, draw it and offset the item list
        unsigned int y = 60;
        if (subtitle != "")
        {
            vita2d_pgf_draw_text(pgf, 5, 40, COLOR_TEXT2, 1.0f, subtitle.c_str());
            y = 80;
        }

        // Draw the menu items, highlighting the current selection
        for (size_t i = 0; i < items->size(); i++)
            vita2d_pgf_draw_text(pgf, 5, y + i * 20, (*selection == i ? COLOR_TEXT3 : COLOR_TEXT1), 1.0f, (*items)[i].c_str());

        // If there are subitems, draw them right-aligned across from the main items
        if (subitems)
        {
            for (size_t i = 0; i < subitems->size(); i++)
            {
                int width = vita2d_pgf_text_width(pgf, 1.0f, (*subitems)[i].c_str());
                vita2d_pgf_draw_text(pgf, 955 - width, y + i * 20, (*selection == i ? COLOR_TEXT3 : COLOR_TEXT1), 1.0f, (*subitems)[i].c_str());
            }
        }

        vita2d_end_drawing();
        vita2d_swap_buffers();

        // Scan for newly-pressed buttons
        SceCtrlData held;
        sceCtrlPeekBufferPositive(0, &held, 1);
        uint32_t pressed = held.buttons & ~buttons;
        buttons = held.buttons;

        // Handle menu input
        if (pressed & buttonMask)
        {
            // Return the pressed buttons so they can be handled
            return pressed;
        }
        else if ((pressed & SCE_CTRL_UP) && *selection > 0)
        {
            // Move the current selection up
            (*selection)--;
        }
        else if ((pressed & SCE_CTRL_DOWN) && *selection < items->size() - 1)
        {
            // Move the current selection down
            (*selection)++;
        }

        sceDisplayWaitVblankStart();
    }
}

uint32_t message(std::string text, uint32_t buttonMask)
{
    // Ignore any buttons that were already pressed
    uint32_t buttons = 0xFFFFFFFF;

    while (true)
    {
        vita2d_start_drawing();
        vita2d_clear_screen();

        size_t i = 0;
        unsigned int y = 0;

        // Draw the text, handling newline characters appropriately
        while (true)
        {
            size_t j = text.find("\n", i);
            vita2d_pgf_draw_text(pgf, 5, (y += 20), COLOR_TEXT1, 1.0f, text.substr(i, j - i).c_str());
            if (j == std::string::npos) break;
            i = j + 1;
        }

        vita2d_end_drawing();
        vita2d_swap_buffers();

        // Scan for newly-pressed buttons
        SceCtrlData held;
        sceCtrlPeekBufferPositive(0, &held, 1);
        uint32_t pressed = held.buttons & ~buttons;
        buttons = held.buttons;

        // Return the pressed buttons so they can be handled
        if (pressed & buttonMask)
            return pressed;

        sceDisplayWaitVblankStart();
    }
}

void settingsMenu()
{
    unsigned int selection = 0;

    std::vector<std::string> items =
    {
        "Direct Boot",
        "FPS Limiter",
        "Threaded 2D",
        "Threaded 3D",
        "Screen Rotation",
        "Screen Arrangement",
        "Screen Sizing",
        "Screen Gap",
        "Integer Scale",
        "GBA Crop",
        "Screen Filter",
        "Show FPS Counter"
    };

    std::vector<std::string> toggle      = { "Off", "On"                              };
    std::vector<std::string> rotation    = { "None", "Clockwise", "Counter-Clockwise" };
    std::vector<std::string> arrangement = { "Automatic", "Vertical", "Horizontal"    };
    std::vector<std::string> sizing      = { "Even", "Enlarge Top", "Enlarge Bottom"  };
    std::vector<std::string> gap         = { "None", "Quarter", "Half", "Full"        };

    while (true)
    {
        // Make a list of strings for the current setting values
        std::vector<std::string> subitems =
        {
            toggle[Settings::getDirectBoot()],
            toggle[Settings::getFpsLimiter()],
            toggle[Settings::getThreaded2D()],
            toggle[(bool)Settings::getThreaded3D()],
            rotation[ScreenLayout::getScreenRotation()],
            arrangement[ScreenLayout::getScreenArrangement()],
            sizing[ScreenLayout::getScreenSizing()],
            gap[ScreenLayout::getScreenGap()],
            toggle[ScreenLayout::getIntegerScale()],
            toggle[ScreenLayout::getGbaCrop()],
            toggle[screenFilter],
            toggle[showFpsCounter]
        };

        // Show the settings menu
        uint32_t pressed = menu("Settings", "", &items, &subitems, &selection, confirmButton | cancelButton);

        // Handle special menu input
        if (pressed & confirmButton)
        {
            // Change the chosen setting to its next value
            // Light FPS limiter doesn't seem to have issues, so there's no need for advanced selection
            // 1 thread for 3D seems to work best, so there's no need for advanced selection
            switch (selection)
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
            }
        }
        else if (pressed & cancelButton)
        {
            // Apply settings and close the menu
            layout.update(960, 544, gbaMode);
            Settings::save();
            return;
        }
    }
}

void fileBrowser()
{
    ndsPath = gbaPath = "";

    std::string path = "ux0:";
    unsigned int selection = 0;

    while (true)
    {
        std::vector<std::string> files;
        SceUID dir = sceIoDopen(path.c_str());
        SceIoDirent entry;

        // Get all folders and ROMs at the current path
        while (sceIoDread(dir, &entry) > 0)
        {
            std::string name = entry.d_name;
            if (SCE_S_ISDIR(entry.d_stat.st_mode) || name.find(".nds", name.length() - 4) !=
                std::string::npos || name.find(".gba", name.length() - 4) != std::string::npos)
                files.push_back(name);
        }

        sceIoDclose(dir);
        sort(files.begin(), files.end());

        // Show the file browser
        uint32_t pressed = menu("NooDS", path.c_str(), &files, nullptr, &selection, confirmButton | cancelButton | SCE_CTRL_TRIANGLE);

        // Handle special menu input
        if ((pressed & confirmButton) && files.size() > 0)
        {
            if (files[selection].find(".nds", files[selection].length() - 4) != std::string::npos)
            {
                // Set an NDS ROM to load
                ndsPath = path + "/" + files[selection];
            }
            else if (files[selection].find(".gba", files[selection].length() - 4) != std::string::npos)
            {
                // Set a GBA ROM to load
                gbaPath = path + "/" + files[selection];
            }
            else
            {
                // Navigate to the selected directory
                path += "/" + files[selection];
                selection = 0;
            }
        }
        else if ((pressed & cancelButton) && path != "ux0:")
        {
            // Navigate to the previous directory
            path = path.substr(0, path.rfind("/"));
            selection = 0;
        }
        else if (pressed & SCE_CTRL_TRIANGLE)
        {
            // Open the settings menu
            settingsMenu();
        }

        // Try to load a ROM if one was set
        if (ndsPath != "" || gbaPath != "")
        {
            try
            {
                // Restart the core and close the menu
                if (core) delete core;
                core = new Core(ndsPath, gbaPath);
                return;
            }
            catch (int e)
            {
                // Loading probably failed because of missing BIOS/firmware, so inform the user
                std::string text =
                    "Initialization failed.\n"
                    "Make sure the path settings point to valid BIOS and firmware files and try again.\n"
                    "You can modify the path settings in ux0:/data/noods/noods.ini.";
                message(text, confirmButton);

                ndsPath = gbaPath = "";
            }
        }
    }
}

void saveTypeMenu()
{
    unsigned int selection = 0;

    std::vector<std::string> items;
    if (core->isGbaMode())
    {
        // Set up list items for GBA save types
        items.push_back("None");
        items.push_back("EEPROM 0.5KB");
        items.push_back("EEPROM 8KB");
        items.push_back("SRAM 32KB");
        items.push_back("FLASH 64KB");
        items.push_back("FLASH 128KB");
    }
    else
    {
        // Set up list items for NDS save types
        items.push_back("None");
        items.push_back("EEPROM 0.5KB");
        items.push_back("EEPROM 8KB");
        items.push_back("EEPROM 64KB");
        items.push_back("EEPROM 128KB");
        items.push_back("FRAM 32KB");
        items.push_back("FLASH 256KB");
        items.push_back("FLASH 512KB");
        items.push_back("FLASH 1024KB");
        items.push_back("FLASH 8192KB");
    }

    while (true)
    {
        // Show the save type menu
        uint32_t pressed = menu("Change Save Type", "", &items, nullptr, &selection, confirmButton | cancelButton);

        // Handle special menu input
        if (pressed & confirmButton)
        {
            // Confirm the change because accidentally resizing a working save file could be bad!
            if (!(message("Are you sure? This may result in data loss!", confirmButton | cancelButton) & confirmButton))
                continue;

            // Apply the change
            if (core->isGbaMode())
            {
                switch (selection)
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
                switch (selection)
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

            // Restart the core and close the menu
            if (core) delete core;
            core = new Core(ndsPath, gbaPath);
            return;
        }
        else if (pressed & cancelButton)
        {
            // Close the menu
            return;
        }
    }
}

void pauseMenu()
{
    stopCore();

    unsigned int selection = 0;

    std::vector<std::string> items =
    {
        "Resume",
        "Restart",
        "Change Save Type",
        "Settings",
        "File Browser"
    };

    while (true)
    {
        // Show the pause menu
        uint32_t pressed = menu("NooDS", "", &items, nullptr, &selection, confirmButton | cancelButton);

        // Handle special menu input
        if (pressed & confirmButton)
        {
            switch (selection)
            {
                case 0: // Resume
                    startCore();
                    return;

                case 1: // Restart
                    if (core) delete core;
                    core = new Core(ndsPath, gbaPath);
                    startCore();
                    return;

                case 2: // Change Save Type
                    saveTypeMenu();
                    break;

                case 3: // Settings
                    settingsMenu();
                    break;

                case 4: // File Browser
                    fileBrowser();
                    startCore();
                    return;
            }
        }
        else if (pressed & cancelButton)
        {
            // Resume and close the menu
            startCore();
            return;
        }
    }
}

void drawScreen(vita2d_texture *texture, uint32_t *data, int width, int height, int scrX, int scrY, int scrWidth, int scrHeight)
{
    // Set texture filtering
    SceGxmTextureFilter filter = screenFilter ? SCE_GXM_TEXTURE_FILTER_LINEAR : SCE_GXM_TEXTURE_FILTER_POINT;
    vita2d_texture_set_filters(texture, filter, filter);

    unsigned int stride = vita2d_texture_get_stride(texture) / 4;
    uint32_t *texData = (uint32_t*)vita2d_texture_get_datap(texture);

    // Copy the screen data to the texture
    for (unsigned int y = 0; y < height; y++)
         memcpy(&texData[y * stride], &data[y * width], width * sizeof(uint32_t));

    if (ScreenLayout::getScreenRotation() == 0)
    {
        // Draw the screen without rotation
        vita2d_draw_texture_part_scale(texture, scrX, scrY, 0, 0, width, height, (float)scrWidth / width, (float)scrHeight / height);
    }
    else
    {
        // Draw the screen with rotation
        float rotation = 3.14159f * ((ScreenLayout::getScreenRotation() == 1) ? 0.5f : -0.5f);
        vita2d_draw_texture_part_scale_rotate(texture, scrX + scrWidth / 2, scrY + scrHeight / 2,
            0, 0, width, height, (float)scrWidth / height, (float)scrHeight / width, rotation);
    }
}

int main()
{
    // Create the noods folder if it doesn't exist
    sceIoMkdir("ux0:/data/noods", 0777);

    // Define the platform settings
    std::vector<Setting> platformSettings =
    {
        Setting("screenFilter",   &screenFilter,   false),
        Setting("showFpsCounter", &showFpsCounter, false)
    };

    // Add the platform settings
    ScreenLayout::addSettings();
    Settings::add(platformSettings);

    // Load the settings
    // If this is the first time, set the default Vita path settings
    if (!Settings::load("ux0:/data/noods/noods.ini"))
    {
        Settings::setBios9Path("ux0:/data/noods/bios9.bin");
        Settings::setBios7Path("ux0:/data/noods/bios7.bin");
        Settings::setFirmwarePath("ux0:/data/noods/firmware.bin");
        Settings::setGbaBiosPath("ux0:/data/noods/gba_bios.bin");
        Settings::setSdImagePath("ux0:/data/noods/sd.img");
        Settings::save();
    }

    // Set the cancel and confirm buttons based on the system registry value
    int assign;
    sceRegMgrGetKeyInt("/CONFIG/SYSTEM", "button_assign", &assign);
    confirmButton = (assign ? SCE_CTRL_CROSS  : SCE_CTRL_CIRCLE);
    cancelButton  = (assign ? SCE_CTRL_CIRCLE : SCE_CTRL_CROSS);

    // Set up button and touch controls
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);

    // Set up an event flag for the save thread
    eventFlag = sceKernelCreateEventFlag("noods_eventflag", 0, 0, nullptr);

    // Initialize graphics and textures
    vita2d_init();
    vita2d_set_clear_color(COLOR_CLEAR);
    pgf = vita2d_load_default_pgf();
    vita2d_texture *top = vita2d_create_empty_texture(256, 192);
    vita2d_texture *bot = vita2d_create_empty_texture(256, 192);

    // Initialize audio output
    audioPort = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM, 1024, 48000, SCE_AUDIO_OUT_MODE_STEREO);

    // Open the file browser
    fileBrowser();

    // Set the screen layout and start the core
    layout.update(960, 544, gbaMode);
    startCore();

    while (true)
    {
        // Scan for button input
        SceCtrlData pressed;
        sceCtrlPeekBufferPositive(0, &pressed, 1);

        // Open the pause menu if the right stick is flicked down
        if (pressed.ry >= 192)
            pauseMenu();

        // Send input to the core
        for (int i = 0; i < 12; i++)
        {
            if (pressed.buttons & keyMap[i])
                core->input.pressKey(i);
            else
                core->input.releaseKey(i);
        }

        // Scan for touch input
        SceTouchData touch;
        sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1);

        if (touch.reportNum > 0)
        {
            // Determine the touch position relative to the emulated touch screen
            int touchX = layout.getTouchX(touch.report[0].x * 960 / 1920, touch.report[0].y * 544 / 1080);
            int touchY = layout.getTouchY(touch.report[0].x * 960 / 1920, touch.report[0].y * 544 / 1080);

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

        // Draw a new frame if one is ready
        bool gba = (core->isGbaMode() && ScreenLayout::getGbaCrop());
        if (core->gpu.getFrame(framebuffer, gba))
        {
            // Update the layout if GBA mode changed
            if (gbaMode != gba)
            {
                gbaMode = gba;
                layout.update(960, 544, gbaMode);
            }

    		vita2d_start_drawing();
    		vita2d_clear_screen();

            if (gbaMode)
            {
                // Draw the GBA screen
                drawScreen(top, &framebuffer[0], 240, 160, layout.getTopX(), layout.getTopY(), layout.getTopWidth(), layout.getTopHeight());
            }
            else
            {
                // Draw the DS top and bottom screens
                drawScreen(top, &framebuffer[0],         256, 192, layout.getTopX(), layout.getTopY(), layout.getTopWidth(), layout.getTopHeight());
                drawScreen(bot, &framebuffer[256 * 192], 256, 192, layout.getBotX(), layout.getBotY(), layout.getBotWidth(), layout.getBotHeight());
            }

            // Draw the FPS counter if enabled
            if (showFpsCounter)
            {
                std::string fps = std::to_string(core->getFps()) + " FPS";
                vita2d_pgf_draw_text(pgf, 5, 20, COLOR_TEXT1, 1.0f, fps.c_str());
            }

            vita2d_end_drawing();
            vita2d_swap_buffers();
        }

        sceDisplayWaitVblankStart();
    }

    return 0;
}
