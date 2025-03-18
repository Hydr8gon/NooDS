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

#include <sys/stat.h>
#include "settings.h"
#include "defines.h"

int Settings::directBoot = 1;
int Settings::fpsLimiter = 1;
int Settings::romInRam = 0;
int Settings::threaded2D = 1;
int Settings::threaded3D = 1;
int Settings::highRes3D = 0;
int Settings::emulateAudio = 1;
int Settings::audio16Bit = 1;
int Settings::screenFilter = 2;
int Settings::screenGhost = 0;
int Settings::savesFolder = 0;
int Settings::statesFolder = 1;
int Settings::cheatsFolder = 1;
int Settings::dsiMode = 0;

std::string Settings::bios9Path = "bios9.bin";
std::string Settings::bios7Path = "bios7.bin";
std::string Settings::firmwarePath = "firmware.bin";
std::string Settings::gbaBiosPath = "gba_bios.bin";
std::string Settings::sdImagePath = "sd.img";
std::string Settings::basePath = ".";

std::vector<Setting> Settings::settings =
{
    Setting("directBoot", &directBoot, false),
    Setting("fpsLimiter", &fpsLimiter, false),
    Setting("romInRam", &romInRam, false),
    Setting("threaded2D", &threaded2D, false),
    Setting("threaded3D", &threaded3D, false),
    Setting("highRes3D", &highRes3D, false),
    Setting("emulateAudio", &emulateAudio, false),
    Setting("audio16Bit", &audio16Bit, false),
    Setting("screenFilter", &screenFilter, false),
    Setting("screenGhost", &screenGhost, false),
    Setting("savesFolder", &savesFolder, false),
    Setting("statesFolder", &statesFolder, false),
    Setting("cheatsFolder", &cheatsFolder, false),
    Setting("dsiMode", &dsiMode, false),
    Setting("bios9Path", &bios9Path, true),
    Setting("bios7Path", &bios7Path, true),
    Setting("firmwarePath", &firmwarePath, true),
    Setting("gbaBiosPath", &gbaBiosPath, true),
    Setting("sdImagePath", &sdImagePath, true)
};

void Settings::add(std::vector<Setting> &settings)
{
    // Add additional settings to be loaded from the settings file
    Settings::settings.insert(Settings::settings.end(), settings.begin(), settings.end());
}

bool Settings::load(std::string path)
{
    // Set the base path and ensure all folders exist
    mkdir((basePath = path).c_str() MKDIR_ARGS);
    mkdir((basePath + "/saves").c_str() MKDIR_ARGS);
    mkdir((basePath + "/states").c_str() MKDIR_ARGS);
    mkdir((basePath + "/cheats").c_str() MKDIR_ARGS);

    // Open the settings file or set defaults if it doesn't exist
    FILE *file = fopen((basePath + "/noods.ini").c_str(), "r");
    if (!file)
    {
        Settings::bios9Path = basePath + "/bios9.bin";
        Settings::bios7Path = basePath + "/bios7.bin";
        Settings::firmwarePath = basePath + "/firmware.bin";
        Settings::gbaBiosPath = basePath + "/gba_bios.bin";
        Settings::sdImagePath = basePath + "/sd.img";
        Settings::save();
        return false;
    }

    // Read each line of the settings file and load values from them
    char data[512];
    while (fgets(data, 512, file) != nullptr)
    {
        std::string line = data;
        size_t split = line.find('=');
        std::string name = line.substr(0, split);
        for (size_t i = 0; i < settings.size(); i++)
        {
            if (name != settings[i].name) continue;
            std::string value = line.substr(split + 1, line.size() - split - 2);
            if (settings[i].isString)
                *(std::string*)settings[i].value = value;
            else if (value[0] >= '0' && value[0] <= '9')
                *(int*)settings[i].value = stoi(value);
            break;
        }
    }

    // Close the file after reading it
    fclose(file);
    return true;
}

bool Settings::save()
{
    // Attempt to open the settings file
    FILE *file = fopen((basePath + "/noods.ini").c_str(), "w");
    if (!file) return false;

    // Write each setting to the settings file
    for (size_t i = 0; i < settings.size(); i++)
    {
        std::string value = settings[i].isString ?
            *(std::string*)settings[i].value : std::to_string(*(int*)settings[i].value);
        fprintf(file, "%s=%s\n", settings[i].name.c_str(), value.c_str());
    }

    // Close the file after writing it
    fclose(file);
    return true;
}
