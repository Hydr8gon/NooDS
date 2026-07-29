/*
    Copyright 2019-2026 Hydr8gon

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

#pragma once

#include <cstdint>
#include <deque>
#include <string>

class Core;

enum StateResult {
    STATE_SUCCESS,
    STATE_FILE_FAIL,
    STATE_FORMAT_FAIL,
    STATE_VERSION_FAIL
};

class SaveStates {
public:
    SaveStates(Core *core): core(core) {}
    void setPath(std::string path, bool gba);
    void setFd(int fd, bool gba);

    StateResult checkState();
    bool saveState();
    bool loadState();

    template <typename T> static void writeFifo(std::deque<T> &fifo, FILE *file);
    template <typename T> static void readFifo(std::deque<T> &fifo, FILE *file);

private:
    Core *core;
    std::string ndsPath, gbaPath;
    int ndsFd = -1, gbaFd = -1;

    static const char *stateTag;
    static const uint32_t stateVersion;

    FILE *openFile(const char *mode);
};

template <typename T> void SaveStates::writeFifo(std::deque<T> &fifo, FILE *file) {
    // Parse a FIFO and save its values
    uint32_t count = fifo.size();
    fwrite(&count, sizeof(count), 1, file);
    for (uint32_t i = 0; i < count; i++)
        fwrite(&fifo[i], sizeof(fifo[i]), 1, file);
}

template <typename T> void SaveStates::readFifo(std::deque<T> &fifo, FILE *file) {
    // Reset and reload a FIFO with saved values
    fifo.clear();
    uint32_t count; T value;
    fread(&count, sizeof(count), 1, file);
    for (uint32_t i = 0; i < count; i++) {
        fread(&value, sizeof(value), 1, file);
        fifo.push_back(value);
    }
}
