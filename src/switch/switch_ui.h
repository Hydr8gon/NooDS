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

#ifndef SWITCH_UI_H
#define SWITCH_UI_H

#include <string>
#include <vector>
#include <glad/glad.h>

struct ListItem
{
    ListItem(std::string name, std::string setting = "", uint32_t *icon = nullptr, int iconSize = 0):
        name(name), setting(setting), icon(icon), iconSize(iconSize) {}

    std::string name;
    std::string setting;
    uint32_t *icon;
    int iconSize;

    bool operator < (const ListItem &item) { return (name < item.name); }
};

struct Selection
{
    Selection(uint32_t pressed, int index): pressed(pressed), index(index) {}

    uint32_t pressed;
    int index;
};

struct Color
{
    Color(uint8_t r, uint8_t g, uint8_t b): r(r), g(g), b(b) {}
    Color(): r(0), g(0), b(0) {}

    uint8_t r, g, b;
};

class SwitchUI
{
    public:
        static void initialize();
        static void deinitialize();

        static uint32_t *bmpToTexture(std::string filename);

        static void drawImage(uint32_t *image, int width, int height, int x, int y, int scaleWidth, int scaleHeight, bool filter = true, int rotation = 0);
        static void drawString(std::string string, int x, int y, int size, Color color, bool alignRight = false);
        static void drawRectangle(int x, int y, int width, int height, Color color);
        static void clear(Color color);
        static void update();

        static Selection menu(std::string title, std::vector<ListItem> *items, unsigned int index = 0,
                              std::string actionX = "", std::string actionPlus = "");
        static bool message(std::string title, std::vector<std::string> text, bool cancel = false);

        static bool isDarkTheme() { return darkTheme; }
        static PadState *getPad() { return &pad;      }

    private:
        SwitchUI() {} // Private to prevent instantiation

        static bool shouldExit;

        static EGLDisplay display;
        static EGLContext context;
        static EGLSurface surface;

        static GLuint program;
        static GLuint vbo;
        static GLuint textures[3];

        static const char *vertexShader;
        static const char *fragmentShader;

        static const uint32_t *font;
        static const uint32_t empty;

        static const int charWidths[];

        static bool darkTheme;
        static Color palette[6];

        static PadState pad;
        static bool touchMode;

        static int stringWidth(std::string string);
};

#endif // SWITCH_UI_H
