/*
    Copyright 2019-2025 Hydr8gon

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

#ifndef SETTINGS_H
#define SETTINGS_H

#include <string>
#include <vector>

struct Setting
{
    std::string name;
    void *value;
    bool isString;

    Setting(std::string name, void *value, bool isString):
        name(name), value(value), isString(isString) {}
};

class Settings
{
    public:
        static int directBoot;
        static int fpsLimiter;
        static int romInRam;
        static int threaded2D;
        static int threaded3D;
        static int highRes3D;
        static int screenFilter;
        static int screenGhost;
        static int savesFolder;
        static int statesFolder;
        static int cheatsFolder;
        static int dsiMode;

        static std::string bios9Path;
        static std::string bios7Path;
        static std::string firmwarePath;
        static std::string gbaBiosPath;
        static std::string sdImagePath;
        static std::string basePath;

        static void add(std::vector<Setting> &settings);
        static bool load(std::string path = ".");
        static bool save();

    private:
        static std::vector<Setting> settings;
        Settings() {} // Private to prevent instantiation
};

#endif // SETTINGS_H
