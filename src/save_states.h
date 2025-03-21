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

#ifndef SAVE_STATES_H
#define SAVE_STATES_H

#include <cstdint>
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

    private:
        Core *core;
        std::string ndsPath, gbaPath;
        int ndsFd = -1, gbaFd = -1;

        static const char *stateTag;
        static const uint32_t stateVersion;

        FILE *openFile(const char *mode);
};

#endif // SAVE_STATES_H
