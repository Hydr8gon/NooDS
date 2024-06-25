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

#include "save_states.h"
#include "core.h"

const char *SaveStates::stateTag = "NOOD";
const uint32_t SaveStates::stateVersion = 2;

void SaveStates::setState(std::string path, bool gba)
{
    // Set the NDS or GBA state path
    gba ? (gbaPath = path) : (ndsPath = path);
}

void SaveStates::setState(int fd, bool gba)
{
    // Set the NDS or GBA state descriptor
    gba ? (gbaFd = fd) : (ndsFd = fd);
}

FILE *SaveStates::openFile(const char *mode)
{
    // Open the NDS or GBA state file based on what's running
    if (gbaFd != -1 && (core->gbaMode || ndsFd == -1))
        return fdopen(dup(gbaFd), mode);
    else if (ndsFd != -1)
        return fdopen(dup(ndsFd), mode);
    else if (gbaPath != "" && (core->gbaMode || ndsPath == ""))
        return fopen(gbaPath.c_str(), mode);
    else if (ndsPath != "")
        return fopen(ndsPath.c_str(), mode);
    return nullptr;
}

StateResult SaveStates::checkState()
{
    // Try to open the state file, if it exists
    FILE *file = openFile("rb");
    if (!file) return STATE_FILE_FAIL;
    fseek(file, 0, SEEK_END);
    uint32_t size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size == 0) return STATE_FILE_FAIL;

    // Get header values from the file for comparison
    uint8_t tag[4];
    uint32_t version;
    fread(tag, sizeof(uint8_t), 4, file);
    fread(&version, sizeof(uint32_t), 1, file);
    fclose(file);

    // Check if the format tag matches
    for (int i = 0; i < 4; i++)
        if (tag[i] != stateTag[i])
            return STATE_FORMAT_FAIL;

    // Check if the state version matches
    if (version != stateVersion)
        return STATE_VERSION_FAIL;
    return STATE_SUCCESS;
}

bool SaveStates::saveState()
{
    // Open the state file and write the header
    FILE *file = openFile("wb");
    if (!file) return false;
    fwrite(stateTag, sizeof(uint8_t), 4, file);
    fwrite(&stateVersion, sizeof(uint32_t), 1, file);

    // Save the state of every component
    core->saveState(file);
    core->bios[0].saveState(file);
    core->bios[1].saveState(file);
    core->bios[2].saveState(file);
    core->cartridgeGba.saveState(file);
    core->cartridgeNds.saveState(file);
    core->cp15.saveState(file);
    core->divSqrt.saveState(file);
    core->dma[0].saveState(file);
    core->dma[1].saveState(file);
    core->gpu.saveState(file);
    core->gpu2D[0].saveState(file);
    core->gpu2D[1].saveState(file);
    core->gpu3D.saveState(file);
    core->gpu3DRenderer.saveState(file);
    core->interpreter[0].saveState(file);
    core->interpreter[1].saveState(file);
    core->ipc.saveState(file);
    core->memory.saveState(file);
    core->rtc.saveState(file);
    core->spi.saveState(file);
    core->spu.saveState(file);
    core->timers[0].saveState(file);
    core->timers[1].saveState(file);
    core->wifi.saveState(file);
    fclose(file);
    return true;
}

bool SaveStates::loadState()
{
    // Open the state file and read past the header
    FILE *file = openFile("rb");
    if (!file) return false;
    fseek(file, 8, SEEK_SET);

    // Load the state of every component
    core->loadState(file);
    core->bios[0].loadState(file);
    core->bios[1].loadState(file);
    core->bios[2].loadState(file);
    core->cartridgeGba.loadState(file);
    core->cartridgeNds.loadState(file);
    core->cp15.loadState(file);
    core->divSqrt.loadState(file);
    core->dma[0].loadState(file);
    core->dma[1].loadState(file);
    core->gpu.loadState(file);
    core->gpu2D[0].loadState(file);
    core->gpu2D[1].loadState(file);
    core->gpu3D.loadState(file);
    core->gpu3DRenderer.loadState(file);
    core->interpreter[0].loadState(file);
    core->interpreter[1].loadState(file);
    core->ipc.loadState(file);
    core->memory.loadState(file);
    core->rtc.loadState(file);
    core->spi.loadState(file);
    core->spu.loadState(file);
    core->timers[0].loadState(file);
    core->timers[1].loadState(file);
    core->wifi.loadState(file);
    fclose(file);
    return true;
}
