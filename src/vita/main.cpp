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

Core *core;

ScreenLayout layout;
uint32_t framebuffer[256 * 192 * 2];
bool gbaMode = false;

uint32_t audioBuffer[1024];
int audioPort = 0;

int nds_save_size_values[10] = {
	0,
    0x200,
    0x2000,
    0x10000,
    0x20000,
    0x8000,
    0x40000,
    0x80000,
    0x100000,
    0x800000
};

std::string nds_save_size_labels[10] {
	"None",
	"EEPROM 0.5KB",
	"EEPROM 8KB",
	"EEPROM 64KB",
	"EEPROM 128KB",
	"FRAM 32KB",
	"FLASH 256KB",
	"FLASH 512KB",
	"FLASH 1024KB",
	"FLASH 8192KB"
};

std::string gba_save_size_labels[6] {
	"None",
	"EEPROM 0.5KB",
	"EEPROM 8KB",
	"SRAM 32KB",
	"FLASH 64KB",
	"FLASH 128KB"
};

int gba_save_size_values[6] {
	0,
	0x200,
	0x2000,
	0x8000,
	0x10000,
	0x20000
};

void runCore()
{
    // Run the emulator
    while (true)
        core->runFrame();
}

void outputAudio()
{
    while (true)
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
    while (true)
    {
        // Check save files every second and update them if changed
        sceKernelDelayThread(1000000);
        core->cartridge.writeSave();
    }
}


void fileBrowser()
{
    std::string path = "ux0:";
    std::string ndsPath, gbaPath;
    unsigned int selection = 0;
    uint32_t buttons = 0;

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

        vita2d_start_drawing();
        vita2d_clear_screen();

        // Draw the title and current path
        vita2d_pgf_draw_text(pgf, 5, 20, COLOR_TEXT1, 1.0f, "NooDS");
        vita2d_pgf_draw_text(pgf, 5, 40, COLOR_TEXT2, 1.0f, path.c_str());

        // Draw the list of files at the current path
        for (size_t i = 0; i < files.size(); i++)
            vita2d_pgf_draw_text(pgf, 5, 80 + i * 20, (selection == i ? COLOR_TEXT3 : COLOR_TEXT1), 1.0f, files[i].c_str());

        vita2d_end_drawing();
        vita2d_swap_buffers();

        while (true)
        {
            // Scan for newly-pressed buttons
            SceCtrlData held;
            sceCtrlPeekBufferPositive(0, &held, 1);
            uint32_t pressed = held.buttons & ~buttons;
            buttons = held.buttons;

            if ((pressed & confirmButton) && files.size() > 0)
            {
                // Handle the current selection
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
                break;
            }

            if ((pressed & cancelButton) && path != "ux0:")
            {
                // Navigate to the previous directory
                path = path.substr(0, path.rfind("/"));
                selection = 0;
                break;
            }

            if ((pressed & SCE_CTRL_UP) && selection > 0)
            {
                // Move the current selection up
                selection--;
                break;
            }

            if ((pressed & SCE_CTRL_DOWN) && selection < files.size() - 1)
            {
                // Move the current selection down
                selection++;
                break;
            }

            sceDisplayWaitVblankStart();
        }

        // Try to load a ROM if one was set
        if (ndsPath != "" || gbaPath != "")
        {
            try
            {
                // Prepare the core and close the file browser
                core = new Core(ndsPath, gbaPath);
				/*
				// Skip save selector if gba rom
				if (gbaPath != ""); break;
				// Check for existing NDS save file, skip if present
				std::string ndsSaveName = ndsPath.substr(0, ndsPath.rfind(".")) + ".sav";
   				FILE *ndsSaveFile = fopen(ndsSaveName.c_str(), "rb");
				if (ndsSaveFile) {fclose(ndsSaveFile); break;}
				*/
            }
            catch (int e)
            {
                vita2d_start_drawing();
                vita2d_clear_screen();

                // Loading probably failed because of missing BIOS/firmware, so inform the user
                vita2d_pgf_draw_text(pgf, 5, 20, COLOR_TEXT1, 1.0f, "Initialization failed.");
                vita2d_pgf_draw_text(pgf, 5, 40, COLOR_TEXT1, 1.0f, "Make sure the path settings point to valid BIOS and firmware files and try again.");
                vita2d_pgf_draw_text(pgf, 5, 60, COLOR_TEXT1, 1.0f, "You can modify the path settings in ux0:/data/noods/noods.ini.");

                vita2d_end_drawing();
                vita2d_swap_buffers();

                while (true)
                {
                    // Scan for newly-pressed buttons
                    SceCtrlData held;
                    sceCtrlPeekBufferPositive(0, &held, 1);
                    uint32_t pressed = held.buttons & ~buttons;
                    buttons = held.buttons;

                    // Stay on the error screen until dismissed
                    if (pressed & confirmButton)
                        break;

                    sceDisplayWaitVblankStart();
                }

                ndsPath = gbaPath = "";
            }

			std::string* save_size_labels = nds_save_size_labels;
			int* save_size_values = nds_save_size_values;
			int save_type_count = 10;

			if ( core->isGbaMode() ) {
				save_size_labels = gba_save_size_labels;
				save_size_values = gba_save_size_values;
				save_type_count = 6;
			}
			selection = 0;
			while (true) {
				
				vita2d_start_drawing();
				vita2d_clear_screen();

				vita2d_pgf_draw_text(pgf, 5, 20, COLOR_TEXT1, 1.0f, "Select a save type for the game");
        		vita2d_pgf_draw_text(pgf, 5, 40, COLOR_TEXT2, 1.0f, "If not sure, select FLASH 512KB");
				
				vita2d_pgf_draw_text(pgf, 5, 80, (selection == 0 ? COLOR_TEXT3 : COLOR_TEXT1), 1.0f, "Skip");
				// List all posible save types
				for (size_t i = 0; i < save_type_count; i++)
					vita2d_pgf_draw_text(pgf, 5, 100 + i * 20, (selection == i+1 ? COLOR_TEXT3 : COLOR_TEXT1), 1.0f, save_size_labels[i].c_str());

				
				vita2d_end_drawing();
				vita2d_swap_buffers();
				while (true) {
					SceCtrlData held;
					sceCtrlPeekBufferPositive(0, &held, 1);
					uint32_t pressed = held.buttons & ~buttons;
					buttons = held.buttons;
	
					if ((pressed & SCE_CTRL_UP) && selection > 0)
					{
						// Move the current selection up
						selection--;
						break;
					}

					if ((pressed & SCE_CTRL_DOWN) && selection < save_type_count - 1)
					{
						// Move the current selection down
						selection++;
						break;
					}

					if (pressed & confirmButton) {
						if (selection == 0) return;
						core->cartridge.resizeNdsSave(save_size_values[selection]);
						return;
					}
				}

				sceDisplayWaitVblankStart();
				
			}
		}
    }
}

void drawScreen(vita2d_texture *texture, uint32_t *data, int width, int height, int scrX, int scrY, int scrWidth, int scrHeight)
{
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

    // Initialize graphics and textures
    vita2d_init();
    vita2d_set_clear_color(COLOR_CLEAR);
    pgf = vita2d_load_default_pgf();
    vita2d_texture *top = vita2d_create_empty_texture(256, 192);
    vita2d_texture *bot = vita2d_create_empty_texture(256, 192);

    // Set texture filtering
    SceGxmTextureFilter filter = screenFilter ? SCE_GXM_TEXTURE_FILTER_LINEAR : SCE_GXM_TEXTURE_FILTER_POINT;
    vita2d_texture_set_filters(top, filter, filter);
    vita2d_texture_set_filters(bot, filter, filter);

    // Open the file browser
    fileBrowser();

    // Set up touch sampling and audio output
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
    audioPort = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM, 1024, 48000, SCE_AUDIO_OUT_MODE_STEREO);

    // Start the core
    std::thread coreThread(runCore);
    std::thread audioThread(outputAudio);
    std::thread saveThread(checkSave);

    layout.update(960, 544, gbaMode);

    while (true)
    {
        // Scan for button input
        SceCtrlData pressed;
        sceCtrlPeekBufferPositive(0, &pressed, 1);

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
