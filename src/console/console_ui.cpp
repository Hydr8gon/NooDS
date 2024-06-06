/*
    Copyright 2019-2024 Hydr8gon

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
#include <chrono>
#include <dirent.h>
#include <sys/stat.h>

#include "console_ui.h"
#include "../common/nds_icon.h"
#include "../settings.h"

#define SCALEH(x, h) (((x) * (h)) / 720)
#define SCALE(x) SCALEH(x, uiHeight)

extern uint8_t _binary_src_console_images_file_dark_bmp_start;
extern uint8_t _binary_src_console_images_file_light_bmp_start;
extern uint8_t _binary_src_console_images_folder_dark_bmp_start;
extern uint8_t _binary_src_console_images_folder_light_bmp_start;
extern uint8_t _binary_src_console_images_font_bmp_start;

void *ConsoleUI::fileTextures[2];
void *ConsoleUI::folderTextures[2];
void *ConsoleUI::fontTexture;

int ConsoleUI::showFpsCounter = 0;
int ConsoleUI::menuTheme = 0;

const uint32_t *ConsoleUI::palette;
uint32_t ConsoleUI::uiWidth, ConsoleUI::uiHeight;
uint32_t ConsoleUI::lineHeight;
bool ConsoleUI::touchMode;

Core *ConsoleUI::core;
bool ConsoleUI::running;
std::string ConsoleUI::ndsPath, ConsoleUI::gbaPath;
std::string ConsoleUI::basePath, ConsoleUI::curPath;

uint32_t ConsoleUI::framebuffer[256 * 192 * 8];
ScreenLayout ConsoleUI::layout;
bool ConsoleUI::gbaMode;
bool ConsoleUI::changed;

std::thread *ConsoleUI::coreThread, *ConsoleUI::saveThread;
std::condition_variable ConsoleUI::cond;
std::mutex ConsoleUI::mutex;

const uint32_t ConsoleUI::themeColors[] =
{
    0xFF2D2D2D, 0xFFFFFFFF, 0xFF4B4B4B, 0xFF232323, 0xFFE1B955, 0xFFC8FF00, // Dark
    0xFFEBEBEB, 0xFF2D2D2D, 0xFFCDCDCD, 0xFFFFFFFF, 0xFFD2D732, 0xFFF05032 // Light
};

const uint8_t ConsoleUI::charWidths[] =
{
    11, 9, 11, 20, 18, 28, 24, 7, 12, 12,
    14, 24, 9, 12, 9, 16, 21, 21, 21, 21,
    21, 21, 21, 21, 21, 21, 9, 9, 26, 24,
    26, 18, 28, 24, 21, 24, 26, 20, 20, 27,
    23, 9, 17, 21, 16, 31, 27, 29, 19, 29,
    20, 18, 21, 26, 24, 37, 21, 21, 24, 12,
    16, 12, 18, 16, 9, 20, 21, 18, 21, 20,
    10, 20, 20, 8, 12, 19, 9, 30, 20, 21,
    21, 21, 12, 16, 12, 20, 17, 29, 17, 17,
    16, 9, 8, 9, 12, 0, 40, 40, 40, 40
};

void ConsoleUI::drawRectangle(float x, float y, float w, float h, uint32_t color)
{
    // Draw a rectangle using a blank texture
    static uint32_t data = 0xFFFFFFFF;
    static void *texture = createTexture(&data, 1, 1);
    drawTexture(texture, 0, 0, 1, 1, x, y, w, h, false, 0, color);
}

void ConsoleUI::drawString(std::string string, float x, float y, float size, uint32_t color, bool alignRight)
{
    // Set the initial offset based on alignment
    float offset = alignRight ? -stringWidth(string) : 0;

    // Move along a string and draw each character
    for (uint32_t i = 0; i < string.size(); i++)
    {
        float x1 = x + offset * size / 48;
        float tx = 48.0f * (((uint8_t)string[i] - 32) % 10);
        float ty = 48.0f * (((uint8_t)string[i] - 32) / 10);
        drawTexture(fontTexture, tx, ty, 47, 47, x1, y, size, size, true, 0, color);
        offset += charWidths[(uint8_t)string[i] - 32];
    }
}

void ConsoleUI::fillAudioBuffer(uint32_t *buffer, int count, int rate)
{
    // Fill the buffer with the last played sample if not running
    static uint32_t lastSample = 0;
    if (!running)
    {
        for (int i = 0; i < count; i++)
            buffer[i] = lastSample;
        return;
    }

    // Fill the buffer with resampled output from the core
    int scaled = count * 32768 / rate;
    uint32_t *original = core->spu.getSamples(scaled);
    lastSample = original[scaled - 1];
    for (int i = 0; i < 1024; i++)
        buffer[i] = original[i * scaled / 1024];
    delete[] original;
}

uint32_t ConsoleUI::getInputPress()
{
    // Scan for newly-pressed buttons
    static uint32_t buttons = 0;
    uint32_t held = getInputHeld();
    uint32_t pressed = held & ~buttons;
    buttons = held;
    return pressed;
}

void *ConsoleUI::bmpToTexture(uint8_t *bmp)
{
    // Allocate data based on bitmap measurements
    int width = U8TO32(bmp, 0x12);
    int height = U8TO32(bmp, 0x16);
    uint32_t *data = new uint32_t[width * height];

    // Convert the bitmap to RGBA8 texture data
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            uint8_t *color = &bmp[0x46 + (((height - y - 1) * width + x) << 2)];
            data[y * width + x] = (color[3] << 24) | (color[0] << 16) | (color[1] << 8) | color[2];
        }
    }

    // Create a texture from the data
    void *texture = createTexture(data, width, height);
    delete[] data;
    return texture;
}

int ConsoleUI::stringWidth(std::string &string)
{
    // Add the widths of each character in a string
    int width = 0;
    for (uint32_t i = 0; i < string.size(); i++)
        width += charWidths[(uint8_t)string[i] - 32];
    return width;
}

void ConsoleUI::initialize(int width, int height, std::string root, std::string prefix)
{
    // Initialize bitmap textures
    fileTextures[0] = bmpToTexture(&_binary_src_console_images_file_dark_bmp_start);
    fileTextures[1] = bmpToTexture(&_binary_src_console_images_file_light_bmp_start);
    folderTextures[0] = bmpToTexture(&_binary_src_console_images_folder_dark_bmp_start);
    folderTextures[1] = bmpToTexture(&_binary_src_console_images_folder_light_bmp_start);
    fontTexture = bmpToTexture(&_binary_src_console_images_font_bmp_start);

    // Create the settings folder if it doesn't exist
    mkdir(prefix.c_str(), 0777);

    // Define the platform settings
    std::vector<Setting> platformSettings =
    {
        Setting("showFpsCounter", &showFpsCounter, false),
        Setting("menuTheme", &menuTheme, false)
    };

    // Add the platform settings
    ScreenLayout::addSettings();
    Settings::add(platformSettings);

    // Load the settings or set defaults if this is the first time
    if (!Settings::load(prefix + "noods.ini"))
    {
        Settings::bios9Path = prefix + "bios9.bin";
        Settings::bios7Path = prefix + "bios7.bin";
        Settings::firmwarePath = prefix + "firmware.bin";
        Settings::gbaBiosPath = prefix + "gba_bios.bin";
        Settings::sdImagePath = prefix + "sd.img";
        ScreenLayout::screenArrangement = 2;
        Settings::save();
    }

    // Initialize some values
    palette = &themeColors[menuTheme * 6];
    uiWidth = width;
    uiHeight = height;
    lineHeight = height / 480;
    basePath = curPath = root;
    changed = true;
}

void ConsoleUI::mainLoop(MenuTouch (*specialTouch)(), ScreenLayout *touchLayout)
{
    while (running)
    {
        // Check if GBA mode changed
        if (gbaMode != (core->gbaMode && ScreenLayout::gbaCrop))
        {
            gbaMode = !gbaMode;
            changed = true;
        }

        // Update the screen layout if it changed
        if (changed)
        {
            layout.update(uiWidth, uiHeight, gbaMode);
            if (touchLayout)
                touchLayout->update(touchLayout->winWidth, touchLayout->winHeight, gbaMode);
            changed = false;
        }

        // Update the framebuffer and start rendering
        void *gbaTexture = nullptr, *topTexture = nullptr, *botTexture = nullptr;
        bool shift = (Settings::highRes3D || Settings::screenFilter == 1);
        core->gpu.getFrame(framebuffer, gbaMode);
        startFrame(0);

        if (gbaMode)
        {
            // Draw the GBA screen
            gbaTexture = createTexture(&framebuffer[0], 240 << shift, 160 << shift);
            drawTexture(gbaTexture, 0, 0, 240 << shift, 160 << shift, layout.topX, layout.topY,
                layout.topWidth, layout.topHeight, Settings::screenFilter, ScreenLayout::screenRotation);
        }
        else // DS mode
        {
            // Draw the DS top screen
            if (ScreenLayout::screenArrangement != 3 || ScreenLayout::screenSizing < 2)
            {
                topTexture = createTexture(&framebuffer[0], 256 << shift, 192 << shift);
                drawTexture(topTexture, 0, 0, 256 << shift, 192 << shift, layout.topX, layout.topY,
                    layout.topWidth, layout.topHeight, Settings::screenFilter, ScreenLayout::screenRotation);
            }

            // Draw the DS bottom screen
            if (ScreenLayout::screenArrangement != 3 || ScreenLayout::screenSizing == 2)
            {
                botTexture = createTexture(&framebuffer[(256 * 192) << (shift * 2)], 256 << shift, 192 << shift);
                drawTexture(botTexture, 0, 0, 256 << shift, 192 << shift, layout.botX, layout.botY,
                    layout.botWidth, layout.botHeight, Settings::screenFilter, ScreenLayout::screenRotation);
            }
        }

        // Draw the FPS counter if enabled
        if (showFpsCounter)
            drawString(std::to_string(core->fps) + " FPS", SCALE(5), 0, SCALE(48));

        // Scan for key input
        uint32_t pressed = getInputPress();
        uint32_t held = getInputHeld();

        // Send input to the core
        for (int i = 0; i < 12; i++)
        {
            if (pressed & BIT(i))
                core->input.pressKey(i);
            else if (~held & BIT(i))
                core->input.releaseKey(i);
        }

        // Scan for touch input, falling back to a special function if provided
        MenuTouch touch = getInputTouch();
        if (!touch.pressed && specialTouch)
            touch = (*specialTouch)();

        if (touch.pressed)
        {
            // Determine the touch position relative to the emulated touch screen
            ScreenLayout *sl = touchLayout ? touchLayout : &layout;
            int touchX = sl->getTouchX(SCALEH(touch.x, sl->winHeight), SCALEH(touch.y, sl->winHeight));
            int touchY = sl->getTouchY(SCALEH(touch.x, sl->winHeight), SCALEH(touch.y, sl->winHeight));

            // Send the touch coordinates to the core
            core->input.pressScreen();
            core->spi.setTouch(touchX, touchY);
        }
        else // Released
        {
            // Release the touch screen press
            core->input.releaseScreen();
            core->spi.clearTouch();
        }

        // Finish drawing and free textures
        endFrame();
        if (gbaTexture) destroyTexture(gbaTexture);
        if (topTexture) destroyTexture(topTexture);
        if (botTexture) destroyTexture(botTexture);

        // Open the pause menu if requested
        if (held & INPUT_PAUSE)
            pauseMenu();
    }
}

int ConsoleUI::setPath(std::string path)
{
    // Set the ROM path if the extension matches
    if (path.find(".nds", path.length() - 4) != std::string::npos) // NDS ROM
    {
        // If a GBA path is set, allow clearing it
        if (gbaPath != "")
        {
            if (!message("Loading NDS ROM", "Load the previous GBA ROM alongside this ROM?", true))
                gbaPath = "";
        }

        // Set the NDS ROM path
        ndsPath = path;

        // Attempt to boot the core with the set ROMs
        if (createCore())
        {
            startCore();
            return 2;
        }

        // Clear the NDS ROM path if booting failed
        ndsPath = "";
        return 1;
    }
    else if (path.find(".gba", path.length() - 4) != std::string::npos) // GBA ROM
    {
        // If an NDS path is set, allow clearing it
        if (ndsPath != "")
        {
            if (!message("Loading GBA ROM", "Load the previous NDS ROM alongside this ROM?", true))
                ndsPath = "";
        }

        // Set the GBA ROM path
        gbaPath = path;

        // Attempt to boot the core with the set ROMs
        if (createCore())
        {
            startCore();
            return 2;
        }

        // Clear the GBA ROM path if booting failed
        gbaPath = "";
        return 1;
    }
    return 0;
}

uint32_t ConsoleUI::menu(std::string title, std::vector<MenuItem> &items,
    int &index, std::string actionX, std::string actionPlus)
{
    // Define the action strings
    if (actionPlus != "") actionPlus = "\x83 " + actionPlus + "     ";
    if (actionX != "") actionX = "\x82 " + actionX + "     ";
    std::string actionB = "\x81 Back     ";
    std::string actionA = "\x80 OK";

    // Calculate touch bounds for the action buttons
    int boundsAB = 1218 - (stringWidth(actionA) + 2.5f * charWidths[0]) * 34 / 48;
    int boundsBX = boundsAB - stringWidth(actionB) * 34 / 48;
    int boundsXPlus = boundsBX - stringWidth(actionX) * 34 / 48;
    int boundsPlus = boundsXPlus - stringWidth(actionPlus) * 34 / 48;

    // Define variables for button input
    bool upHeld = false;
    bool downHeld = false;
    bool scroll = false;
    std::chrono::steady_clock::time_point timeHeld;

    // Define variables for touch input
    int touchIndex = 0;
    bool touchStarted = false;
    bool touchScroll = false;
    MenuTouch touchStart(false, 0, 0);

    while (true)
    {
        // Draw the borders
        startFrame(palette[0]);
        drawString(title, SCALE(72), SCALE(30), SCALE(42), palette[1]);
        drawRectangle(SCALE(30), SCALE(88), SCALE(1220), lineHeight, palette[1]);
        drawRectangle(SCALE(30), SCALE(648), SCALE(1220), lineHeight, palette[1]);
        drawString(actionPlus + actionX + actionB + actionA, SCALE(1218), SCALE(667), SCALE(34), palette[1], true);

        // Scan for key input
        uint32_t pressed = getInputPress();
        uint32_t held = getInputHeld();

        // Handle up input presses
        if ((pressed & INPUT_UP) && !(pressed & INPUT_DOWN))
        {
            // Disable touch mode or move the selection box up
            if (touchMode)
                touchMode = false;
            else if (index > 0)
                index--;

            // Remember when the up input started
            upHeld = true;
            timeHeld = std::chrono::steady_clock::now();
        }

        // Handle down input presses
        if ((pressed & INPUT_DOWN) && !(pressed & INPUT_UP))
        {
            // Disable touch mode or move the selection box down
            if (touchMode)
                touchMode = false;
            else if (index < items.size() - 1)
                index++;

            // Remember when the down input started
            downHeld = true;
            timeHeld = std::chrono::steady_clock::now();
        }

        // Return button presses so they can be handled externally
        if (((pressed & INPUT_A) && !touchMode) || (pressed & INPUT_B) || (actionX != ""
            && (pressed & INPUT_X)) || (actionPlus != "" && (pressed & INPUT_START)))
        {
            touchMode = false;
            return pressed;
        }

        // Disable touch mode before allowing A presses so the selector is visible
        if ((pressed & INPUT_A) && touchMode)
            touchMode = false;

        // Cancel up input if it was released
        if (upHeld && !(held & INPUT_UP))
        {
            upHeld = false;
            scroll = false;
        }

        // Cancel down input if it was released
        if (downHeld && !(held & INPUT_DOWN))
        {
            downHeld = false;
            scroll = false;
        }

        // Scroll continuously while a directional input is held
        if ((upHeld && index > 0) || (downHeld && index < items.size() - 1))
        {
            // When the input starts, wait a bit before scrolling
            std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - timeHeld;
            if (!scroll && elapsed.count() > 0.5f)
                scroll = true;

            // Scroll up or down at a fixed time interval
            if (scroll && elapsed.count() > 0.1f)
            {
                index += upHeld ? -1 : 1;
                timeHeld = std::chrono::steady_clock::now();
            }
        }

        // Scan for touch input
        MenuTouch touch = getInputTouch();

        // Handle touch input
        if (touch.pressed)
        {
            // Remember where a touch started
            if (!touchStarted)
            {
                touchStart = touch;
                touchStarted = true;
                touchScroll = false;
                touchMode = true;
            }

            // Handle touch scrolling
            if (touchScroll)
            {
                // Scroll the list based on how far it's been dragged
                int newIndex = touchIndex + (touchStart.y - touch.y) / 70;
                if (items.size() > 7 && newIndex != touchIndex)
                    index = std::max(3, std::min<int>(items.size() - 4, newIndex));
            }
            else if (touch.x > touchStart.x + 25 || touch.x < touchStart.x - 25
                || touch.y > touchStart.y + 25 || touch.y < touchStart.y - 25)
            {
                // Start scrolling from the current index if a touch is dragged
                touchScroll = true;
                touchIndex = std::max(3, std::min<int>(items.size() - 4, index));
            }
        }
        else // Released
        {
            // Simulate a button press if its action text was tapped
            if (!touchScroll && touchStart.y >= 650)
            {
                if (touchStart.x >= boundsBX && touchStart.x < boundsAB)
                    return INPUT_B;
                else if (touchStart.x >= boundsXPlus && touchStart.x < boundsBX)
                    return INPUT_X;
                else if (touchStart.x >= boundsPlus && touchStart.x < boundsXPlus)
                    return INPUT_START;
            }
            touchStarted = false;
        }

        // Draw the first item separator
        if (items.size() > 0)
            drawRectangle(SCALE(90), SCALE(124), SCALE(1100), lineHeight, palette[2]);

        // Draw the list items
        int size = std::min<int>(7, items.size());
        for (int i = 0, offset; i < size; i++)
        {
            // Determine the scroll offset
            if (index < 4 || items.size() <= 7)
                offset = i;
            else if (index > items.size() - 4)
                offset = items.size() - 7 + i;
            else
                offset = i + index - 3;

            // Simulate an A press on a selection if it was tapped
            if (!touchStarted && !touchScroll && touchStart.x >= 90 && touchStart.x <
                1190 && touchStart.y >= 124 + i * 70 && touchStart.y < 194 + i * 70)
            {
                index = offset;
                return INPUT_A;
            }

            // Draw UI elements around the list items
            if (!touchMode && offset == index)
            {
                // Draw a box behind the selected item if not in touch mode
                drawRectangle(SCALE(90), SCALE(125 + i * 70), SCALE(1100), SCALE(69), palette[3]);
                drawRectangle(SCALE(89), SCALE(121 + i * 70), SCALE(1103), SCALE(5), palette[4]);
                drawRectangle(SCALE(89), SCALE(191 + i * 70), SCALE(1103), SCALE(5), palette[4]);
                drawRectangle(SCALE(88), SCALE(122 + i * 70), SCALE(5), SCALE(73), palette[4]);
                drawRectangle(SCALE(1188), SCALE(122 + i * 70), SCALE(5), SCALE(73), palette[4]);
            }
            else
            {
                // Draw separators between the items
                drawRectangle(SCALE(90), SCALE(194 + i * 70), SCALE(1100), lineHeight, palette[2]);
            }

            // Draw the current item's name
            int x = (items[offset].iconSize > 0) ? 184 : 105;
            drawString(items[offset].name, SCALE(x), SCALE(140 + i * 70), SCALE(38), palette[1]);

            // Draw the current item's icon if it has one
            if (items[offset].iconSize > 0)
                drawTexture(items[offset].iconTex, 0, 0, items[offset].iconSize,
                    items[offset].iconSize, SCALE(105), SCALE(127 + i * 70), SCALE(64), SCALE(64));

            // Draw the current item's setting if it has one
            if (items[offset].setting != "")
                drawString(items[offset].setting, SCALE(1175), SCALE(143 + i * 70), SCALE(32), palette[5], true);
        }

        // Finish drawing
        endFrame();
    }
}

bool ConsoleUI::message(std::string title, std::string text, bool cancel)
{
    // Define the action strings
    std::string actionB = "\x81 Back     ";
    std::string actionA = "\x80 OK";

    // Calculate touch bounds for the action buttons
    int boundsA = 1218 + (2.5f * charWidths[0]) * 34 / 48;
    int boundsAB = 1218 - (stringWidth(actionA) + 2.5f * charWidths[0]) * 34 / 48;
    int boundsB = boundsAB - stringWidth(actionB) * 34 / 48;

    // Define variables for touch input
    bool touchStarted = false;
    bool touchScroll = false;
    MenuTouch touchStart(false, 0, 0);

    while (true)
    {
        // Draw the borders
        startFrame(palette[0]);
        drawString(title, SCALE(72), SCALE(30), SCALE(42), palette[1]);
        drawRectangle(SCALE(30), SCALE(88), SCALE(1220), lineHeight, palette[1]);
        drawRectangle(SCALE(30), SCALE(648), SCALE(1220), lineHeight, palette[1]);
        drawString((cancel ? actionB : "") + actionA, SCALE(1218), SCALE(667), SCALE(34), palette[1], true);

        // Draw each line of text, separated by newline characters
        for (int i = 0, j = 0, y = 0; j != std::string::npos; y += 38)
        {
            j = text.find("\n", i);
            drawString(text.substr(i, j - i), SCALE(90), SCALE(124 + y), SCALE(38), palette[1]);
            i = j + 1;
        }

        // Scan for key input
        uint32_t pressed = getInputPress();

        // Dismiss the message and return the result if an action is pressed
        if (pressed & INPUT_A)
            return true;
        else if ((pressed & INPUT_B) && cancel)
            return false;

        // Scan for touch input
        MenuTouch touch = getInputTouch();

        // Handle touch input
        if (touch.pressed)
        {
            // Remember where a touch started
            if (!touchStarted)
            {
                touchStart = touch;
                touchStarted = true;
                touchScroll = false;
                touchMode = true;
            }

            // Track if a touch starts dragging instead of tapping
            if (touch.x > touchStart.x + 25 || touch.x < touchStart.x - 25 ||
                touch.y > touchStart.y + 25 || touch.y < touchStart.y - 25)
                touchScroll = true;
        }
        else // Released
        {
            // Simulate a button press if its action text was tapped
            if (!touchScroll && touchStart.y >= 650)
            {
                if (touchStart.x >= boundsAB && touchStart.x < boundsA)
                    return true;
                else if (touchStart.x >= boundsB && touchStart.x < boundsAB && cancel)
                    return false;
            }
            touchStarted = false;
        }

        // Finish drawing
        endFrame();
    }
}

void ConsoleUI::fileBrowser()
{
    int index = 0;
    while (true)
    {
        // Open the current directory to list files from
        std::vector<MenuItem> files;
        DIR *dir = opendir(curPath.c_str());
        dirent *entry;

        // Build a list of directories and ROMs
        while ((entry = readdir(dir)))
        {
            // Get information on a file
            std::string name = entry->d_name;
            std::string subpath = curPath + "/" + name;
            struct stat substat;
            stat(subpath.c_str(), &substat);

            // Add a list entry if appropriate
            if (S_ISDIR(substat.st_mode))
            {
                // Add a directory with a generic icon to the list
                files.push_back(MenuItem(name, "", folderTextures[menuTheme], 64));
            }
            else if (name.find(".nds", name.length() - 4) != std::string::npos)
            {
                // Add an NDS ROM with its decoded icon to the list
                void *texture = createTexture(NdsIcon(subpath).getIcon(), 32, 32);
                files.push_back(MenuItem(name, "", texture, 32));
            }
            else if (name.find(".gba", name.length() - 4) != std::string::npos)
            {
                // Add a GBA ROM with a generic icon to the list
                files.push_back(MenuItem(name, "", fileTextures[menuTheme], 64));
            }
        }

        // Sort the files alphabetically
        sort(files.begin(), files.end());
        closedir(dir);

        // Create the file browser menu
        uint32_t pressed = menu("NooDS", files, index, "Settings", "Exit");

        // Handle menu input
        if (pressed & INPUT_A)
        {
            // Navigate to the selected file if any exist
            if (files.empty()) continue;
            curPath += "/" + files[index].name;
            index = 0;

            // Try to set a ROM path
            switch (setPath(curPath))
            {
                case 1: // ROM failed to load
                    // Remove the ROM from the path and continue browsing
                    curPath = curPath.substr(0, curPath.rfind("/"));
                    index = 0;
                case 0: // ROM not selected
                    continue;

                case 2: // ROM loaded
                    // Save the previous directory and close the file browser
                    curPath = curPath.substr(0, curPath.rfind("/"));
                    return;
            }
        }
        else if (pressed & INPUT_B)
        {
            // Navigate to the previous directory
            if (curPath != basePath)
            {
                curPath = curPath.substr(0, curPath.rfind("/"));
                index = 0;
            }
        }
        else if (pressed & INPUT_X)
        {
            // Open the settings menu
            settingsMenu();
        }
        else if (pressed & INPUT_START)
        {
            // Close the file browser
            return;
        }
    }
}

void ConsoleUI::settingsMenu()
{
    // Define possible values for settings
    const std::vector<std::string> toggle = { "Off", "On" };
    const std::vector<std::string> position = { "Center", "Top", "Bottom", "Left", "Right" };
    const std::vector<std::string> rotation = { "None", "Clockwise", "Counter-Clockwise" };
    const std::vector<std::string> arrangement = { "Automatic", "Vertical", "Horizontal", "Single Screen" };
    const std::vector<std::string> sizing = { "Even", "Enlarge Top", "Enlarge Bottom" };
    const std::vector<std::string> gap = { "None", "Quarter", "Half", "Full" };
    const std::vector<std::string> filter = { "Nearest", "Upscaled", "Linear" };
    const std::vector<std::string> theme = { "Dark", "Light" };

    int index = 0;
    while (true)
    {
        // Create a list of settings and current values
        std::vector<MenuItem> settings =
        {
            MenuItem("Direct Boot", toggle[Settings::directBoot]),
            MenuItem("FPS Limiter", toggle[Settings::fpsLimiter]),
            MenuItem("Threaded 2D", toggle[Settings::threaded2D]),
            MenuItem("Threaded 3D", toggle[(bool)Settings::threaded3D]),
            MenuItem("High-Resolution 3D", toggle[Settings::highRes3D]),
            MenuItem("Screen Position", position[ScreenLayout::screenPosition]),
            MenuItem("Screen Rotation", rotation[ScreenLayout::screenRotation]),
            MenuItem("Screen Arrangement", arrangement[ScreenLayout::screenArrangement]),
            MenuItem("Screen Sizing", sizing[ScreenLayout::screenSizing]),
            MenuItem("Screen Gap", gap[ScreenLayout::screenGap]),
            MenuItem("Screen Filter", filter[Settings::screenFilter]),
            MenuItem("Integer Scale", toggle[ScreenLayout::integerScale]),
            MenuItem("GBA Crop", toggle[ScreenLayout::gbaCrop]),
            MenuItem("Simulate Ghosting", toggle[Settings::screenGhost]),
            MenuItem("Show FPS Counter", toggle[showFpsCounter]),
            MenuItem("Menu Theme", theme[menuTheme])
        };

        // Create the settings menu
        uint32_t pressed = menu("Settings", settings, index);

        // Handle menu input
        if (pressed & INPUT_A)
        {
            // Change the chosen setting to its next value
            switch (index)
            {
                case 0: Settings::directBoot = (Settings::directBoot + 1) % 2; break;
                case 1: Settings::fpsLimiter = (Settings::fpsLimiter + 1) % 2; break;
                case 2: Settings::threaded2D = (Settings::threaded2D + 1) % 2; break;
                case 3: Settings::threaded3D = (Settings::threaded3D + 1) % 2; break;
                case 4: Settings::highRes3D = (Settings::highRes3D + 1) % 2; break;
                case 5: ScreenLayout::screenPosition = (ScreenLayout::screenPosition + 1) % 5; break;
                case 6: ScreenLayout::screenRotation = (ScreenLayout::screenRotation + 1) % 3; break;
                case 7: ScreenLayout::screenArrangement = (ScreenLayout::screenArrangement + 1) % 4; break;
                case 8: ScreenLayout::screenSizing = (ScreenLayout::screenSizing + 1) % 3; break;
                case 9: ScreenLayout::screenGap = (ScreenLayout::screenGap + 1) % 4; break;
                case 10: Settings::screenFilter = (Settings::screenFilter + 1) % 3; break;
                case 11: ScreenLayout::integerScale = (ScreenLayout::integerScale + 1) % 2; break;
                case 12: ScreenLayout::gbaCrop = (ScreenLayout::gbaCrop + 1) % 2; break;
                case 13: Settings::screenGhost = (Settings::screenGhost + 1) % 2; break;
                case 14: showFpsCounter = (showFpsCounter + 1) % 2; break;

                case 15:
                    // Update the palette when changing themes
                    menuTheme = (menuTheme + 1) % 2;
                    palette = &themeColors[menuTheme * 6];
                    break;
            }
        }
        else if (pressed & INPUT_B)
        {
            // Close the settings menu
            changed = true;
            Settings::save();
            return;
        }
    }
}

void ConsoleUI::pauseMenu()
{
    // Pause the emulator
    stopCore();

    // Define the pause menu list
    std::vector<MenuItem> items =
    {
        MenuItem("Resume"),
        MenuItem("Restart"),
        MenuItem("Save State"),
        MenuItem("Load State"),
        MenuItem("Change Save Type"),
        MenuItem("Settings"),
        MenuItem("File Browser")
    };

    int index = 0;
    while (true)
    {
        // Create the pause menu
        uint32_t pressed = menu("NooDS", items, index);

        // Handle menu input
        if (pressed & INPUT_A)
        {
            // Handle the selected item
            switch (index)
            {
                case 0: // Resume
                    // Return to the emulator
                    startCore();
                    return;

                case 1: // Restart
                    // Restart and return to the emulator
                    createCore() ? startCore() : fileBrowser();
                    return;

                case 2: // Save State
                    // Show a confirmation message, with extra information if a state file doesn't exist yet
                    if (!message("Save State", (core->saveStates.checkState() == STATE_FILE_FAIL) ? "Saving and "
                        "loading states is dangerous and can lead to data loss.\nStates are also not guaranteed to "
                        "be compatible across emulator versions.\nPlease rely on in-game saving to keep your progress, "
                        "and back up .sav files\nbefore using this feature. Do you want to save the current state?" :
                        "Do you want to overwrite the saved state with the current state? This can't be undone!", true))
                        break;

                    // Save the state and return to emulation if confirmed
                    core->saveStates.saveState();
                    startCore();
                    return;

                case 3: // Load State
                {
                    // Show a confirmation message, or an error if something went wrong
                    bool error = true;
                    std::string title, text;
                    switch (core->saveStates.checkState())
                    {
                        case STATE_SUCCESS:
                            error = false;
                            title = "Load State";
                            text = "Do you want to load the saved state and "
                                "lose the current state? This can't be undone!";
                            break;

                        case STATE_FILE_FAIL:
                            title = "Error";
                            text = "The state file doesn't exist or couldn't be opened.";
                            break;

                        case STATE_FORMAT_FAIL:
                            title = "Error";
                            text = "The state file doesn't have a valid format.";
                            break;

                        case STATE_VERSION_FAIL:
                            title = "Error";
                            text = "The state file isn't compatible with this version of NooDS.";
                            break;
                    }

                    // Load the state and return to emulation if confirmed
                    if (!message(title, text, !error) || error) break;
                    core->saveStates.loadState();
                    startCore();
                    return;
                }

                case 4: // Change Save Type
                    // Open the save type menu and restart if the save changed
                    if (saveTypeMenu())
                        return createCore() ? startCore() : fileBrowser();
                    break;

                case 5: // Settings
                    // Open the settings menu
                    settingsMenu();
                    break;

                case 6: // File Browser
                    // Open the file browser and close the pause menu
                    fileBrowser();
                    return;
            }
        }
        else if (pressed & INPUT_B)
        {
            // Return to the emulator
            startCore();
            return;
        }
    }
}

bool ConsoleUI::saveTypeMenu()
{
    // Build a save type list for the current mode
    std::vector<MenuItem> items;
    if (core->gbaMode)
    {
        // Add list items for GBA save types
        items.push_back(MenuItem("None"));
        items.push_back(MenuItem("EEPROM 0.5KB"));
        items.push_back(MenuItem("EEPROM 8KB"));
        items.push_back(MenuItem("SRAM 32KB"));
        items.push_back(MenuItem("FLASH 64KB"));
        items.push_back(MenuItem("FLASH 128KB"));
    }
    else
    {
        // Add list items for NDS save types
        items.push_back(MenuItem("None"));
        items.push_back(MenuItem("EEPROM 0.5KB"));
        items.push_back(MenuItem("EEPROM 8KB"));
        items.push_back(MenuItem("EEPROM 64KB"));
        items.push_back(MenuItem("EEPROM 128KB"));
        items.push_back(MenuItem("FRAM 32KB"));
        items.push_back(MenuItem("FLASH 256KB"));
        items.push_back(MenuItem("FLASH 512KB"));
        items.push_back(MenuItem("FLASH 1024KB"));
        items.push_back(MenuItem("FLASH 8192KB"));
    }

    int index = 0;
    while (true)
    {
        // Create the save type menu
        uint32_t pressed = menu("Change Save Type", items, index);

        // Handle menu input
        if (pressed & INPUT_A)
        {
            // Confirm the change because doing this accidentally could be bad
            if (!message("Changing Save Type", "Are you sure? This may result in data loss!", true))
                continue;

            // Apply the change for the current mode
            if (core->gbaMode)
            {
                // Resize a GBA save file
                switch (index)
                {
                    case 0: core->cartridgeGba.resizeSave(0x00000); break; // None
                    case 1: core->cartridgeGba.resizeSave(0x00200); break; // EEPROM 0.5KB
                    case 2: core->cartridgeGba.resizeSave(0x02000); break; // EEPROM 8KB
                    case 3: core->cartridgeGba.resizeSave(0x08000); break; // SRAM 32KB
                    case 4: core->cartridgeGba.resizeSave(0x10000); break; // FLASH 64KB
                    case 5: core->cartridgeGba.resizeSave(0x20000); break; // FLASH 128KB
                }
            }
            else
            {
                // Resize an NDS save file
                switch (index)
                {
                    case 0: core->cartridgeNds.resizeSave(0x000000); break; // None
                    case 1: core->cartridgeNds.resizeSave(0x000200); break; // EEPROM 0.5KB
                    case 2: core->cartridgeNds.resizeSave(0x002000); break; // EEPROM 8KB
                    case 3: core->cartridgeNds.resizeSave(0x010000); break; // EEPROM 64KB
                    case 4: core->cartridgeNds.resizeSave(0x020000); break; // EEPROM 128KB
                    case 5: core->cartridgeNds.resizeSave(0x008000); break; // FRAM 32KB
                    case 6: core->cartridgeNds.resizeSave(0x040000); break; // FLASH 256KB
                    case 7: core->cartridgeNds.resizeSave(0x080000); break; // FLASH 512KB
                    case 8: core->cartridgeNds.resizeSave(0x100000); break; // FLASH 1024KB
                    case 9: core->cartridgeNds.resizeSave(0x800000); break; // FLASH 8192KB
                }
            }
            return true;
        }
        else if (pressed & INPUT_B)
        {
            // Close the save type menu
            return false;
        }
    }
}

bool ConsoleUI::createCore()
{
    try
    {
        // Attempt to create the core
        if (core) delete core;
        core = new Core(ndsPath, gbaPath);
        return true;
    }
    catch (CoreError e)
    {
        // Inform the user of an error if loading wasn't successful
        std::string text;
        switch (e)
        {
            case ERROR_BIOS: // Missing BIOS files
                text = "Make sure the path settings point to valid BIOS files and try again.\n"
                    "You can modify the path settings in the noods.ini file.";
                message("Error Loading BIOS", text);
                break;

            case ERROR_FIRM: // Non-bootable firmware file
                text = "Make sure the path settings point to a bootable firmware file or try another boot method.\n"
                    "You can modify the path settings in the noods.ini file.";
                message("Error Loading Firmware", text);
                break;

            case ERROR_ROM: // Unreadable ROM file
                text = "Make sure the ROM file is accessible and try again.";
                message("Error Loading ROM", text);
                break;
        }

        // Throw out the incomplete core
        core = nullptr;
        return false;
    }
}

void ConsoleUI::startCore()
{
    // Tell the threads to run if stopped
    if (running) return;
    running = true;

    // Start the threads
    coreThread = new std::thread(runCore);
    saveThread = new std::thread(checkSave);
}

void ConsoleUI::stopCore()
{
    // Tell the threads to stop if running
    if (running)
    {
        std::lock_guard<std::mutex> guard(mutex);
        running = false;
        cond.notify_one();
    }
    else return;

    // Wait for the threads to stop
    coreThread->join();
    delete coreThread;
    saveThread->join();
    delete saveThread;
}

void ConsoleUI::runCore()
{
    // Run the emulator
    while (running)
        core->runFrame();
}

void ConsoleUI::checkSave()
{
    while (running)
    {
        // Check save files every few seconds and update them if changed
        std::unique_lock<std::mutex> lock(mutex);
        cond.wait_for(lock, std::chrono::seconds(3), [&]{ return !running; });
        core->cartridgeNds.writeSave();
        core->cartridgeGba.writeSave();
    }
}
