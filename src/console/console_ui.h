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

#ifndef CONSOLE_UI_H
#define CONSOLE_UI_H

#include <cstdint>
#include <string>
#include <vector>

#include "../common/screen_layout.h"
#include "../core.h"
#include "../defines.h"

enum MenuInputs
{
    INPUT_A = BIT(0),
    INPUT_B = BIT(1),
    INPUT_SELECT = BIT(2),
    INPUT_START = BIT(3),
    INPUT_RIGHT = BIT(4),
    INPUT_LEFT = BIT(5),
    INPUT_UP = BIT(6),
    INPUT_DOWN = BIT(7),
    INPUT_R = BIT(8),
    INPUT_L = BIT(9),
    INPUT_X = BIT(10),
    INPUT_Y = BIT(11),
    INPUT_PAUSE = BIT(12)
};

struct MenuTouch
{
    bool pressed;
    float x, y;

    MenuTouch(bool pressed, float x, float y):
        pressed(pressed), x(x), y(y) {}
};

struct MenuItem
{
    std::string name;
    std::string setting;
    void *iconTex;
    uint8_t iconSize;

    MenuItem(std::string name, std::string setting = "", void *iconTex = nullptr, uint8_t iconSize = 0):
        name(name), setting(setting), iconTex(iconTex), iconSize(iconSize) {}
    bool operator<(const MenuItem &item) { return (name < item.name); }
};

class ConsoleUI
{
    public:
        static Core *core;
        static bool running;
        static uint32_t framebuffer[256 * 192 * 8];
        static ScreenLayout layout;
        static bool gbaMode;

        // Functions that are implemented per-platform
        static void startFrame(uint32_t color);
        static void endFrame();
        static void *createTexture(uint32_t *data, int width, int height);
        static void destroyTexture(void *texture);
        static void drawTexture(void *texture, float tx, float ty, float tw, float th, float x, float y,
            float w, float h, bool filter = true, int rotation = 0, uint32_t color = 0xFFFFFFFF);
        static uint32_t getInputHeld();
        static MenuTouch getInputTouch();

        static void drawRectangle(float x, float y, float w, float h, uint32_t color = 0xFFFFFFFF);
        static void drawString(std::string string, float x, float y,
            float size, uint32_t color = 0xFFFFFFFF, bool alignRight = false);
        static void fillAudioBuffer(uint32_t *buffer, int count, int rate);
        static uint32_t getInputPress();

        static void initialize(int width, int height, std::string root, std::string prefix);
        static void mainLoop(MenuTouch (*specialTouch)() = nullptr, ScreenLayout *touchLayout = nullptr);
        static int setPath(std::string path);
        static void fileBrowser();

    private:
        static void *fileTextures[2];
        static void *folderTextures[2];
        static void *fontTexture;

        static int screenFilter;
        static int showFpsCounter;
        static int menuTheme;

        static const uint32_t *palette;
        static uint32_t uiWidth, uiHeight;
        static uint32_t lineHeight;
        static bool touchMode;

        static std::string ndsPath, gbaPath;
        static std::string basePath, curPath;
        static bool changed;

        static std::thread *coreThread, *saveThread;
        static std::condition_variable cond;
        static std::mutex mutex;

        static const uint32_t themeColors[];
        static const uint8_t charWidths[];

        ConsoleUI() {} // Private to prevent instantiation
        static void *bmpToTexture(uint8_t *bmp);
        static int stringWidth(std::string &string);

        static uint32_t menu(std::string title, std::vector<MenuItem> &items,
            int &index, std::string actionX = "", std::string actionPlus = "");
        static bool message(std::string title, std::string text, bool cancel = false);

        static void settingsMenu();
        static void pauseMenu();
        static bool saveTypeMenu();

        static bool createCore();
        static void startCore();
        static void stopCore();
        static void runCore();
        static void checkSave();
};

#endif // CONSOLE_UI_H
