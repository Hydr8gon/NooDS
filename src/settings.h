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

#ifndef SETTINGS_H
#define SETTINGS_H

#include <string>
#include <vector>

struct Setting
{
    Setting(std::string name, void *value, bool isString):
        name(name), value(value), isString(isString) {}

    std::string name;
    void *value;
    bool isString;
};

class Settings
{
    public:
        static int directBoot;
        static int fpsLimiter;
        static int threaded2D;
        static int threaded3D;
        static int highRes3D;
        static int screenFilter;
        static int screenGhost;
        static std::string bios9Path;
        static std::string bios7Path;
        static std::string firmwarePath;
        static std::string gbaBiosPath;
        static std::string sdImagePath;

        static void add(std::vector<Setting> platformSettings);
        static bool load(std::string filename = "noods.ini");
        static bool save();

    private:
        static std::string filename;
        static std::vector<Setting> settings;

        Settings() {} // Private to prevent instantiation
};

#endif // SETTINGS_H
