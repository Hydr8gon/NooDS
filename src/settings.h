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

#ifndef SETTINGS_H
#define SETTINGS_H

#include <string>
#include <vector>

struct Setting
{
    Setting(std::string name, void *value, bool isString): name(name), value(value), isString(isString) {}

    std::string name;
    void *value;
    bool isString;
};

class Settings
{
    public:
        static void add(std::vector<Setting> platformSettings);
        static bool load(std::string filename = "noods.ini");
        static bool save();

        static int         getDirectBoot()   { return directBoot;   }
        static int         getFpsLimiter()   { return fpsLimiter;   }
        static int         getThreaded2D()   { return threaded2D;   }
        static int         getThreaded3D()   { return threaded3D;   }
        static std::string getBios9Path()    { return bios9Path;    }
        static std::string getBios7Path()    { return bios7Path;    }
        static std::string getFirmwarePath() { return firmwarePath; }
        static std::string getGbaBiosPath()  { return gbaBiosPath;  }
        static std::string getSdImagePath()  { return sdImagePath;  }

        static void setDirectBoot(int value)           { directBoot   = value; }
        static void setFpsLimiter(int value)           { fpsLimiter   = value; }
        static void setThreaded2D(int value)           { threaded2D   = value; }
        static void setThreaded3D(int value)           { threaded3D   = value; }
        static void setBios9Path(std::string value)    { bios9Path    = value; }
        static void setBios7Path(std::string value)    { bios7Path    = value; }
        static void setFirmwarePath(std::string value) { firmwarePath = value; }
        static void setGbaBiosPath(std::string value)  { gbaBiosPath  = value; }
        static void setSdImagePath(std::string value)  { sdImagePath  = value; }

    private:
        Settings() {} // Private to prevent instantiation

        static std::string filename;

        static int directBoot;
        static int fpsLimiter;
        static int threaded2D;
        static int threaded3D;
        static std::string bios9Path;
        static std::string bios7Path;
        static std::string firmwarePath;
        static std::string gbaBiosPath;
        static std::string sdImagePath;

        static std::vector<Setting> settings;
};

#endif // SETTINGS_H
