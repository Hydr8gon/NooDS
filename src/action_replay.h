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

#ifndef ACTION_REPLAY_H
#define ACTION_REPLAY_H

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

class Core;

struct ARCheat {
    std::string name;
    std::vector<uint32_t> code;
    bool enabled;
};

class ActionReplay {
    public:
        std::vector<ARCheat> cheats;

        ActionReplay(Core *core): core(core) {}
        void setPath(std::string path);
        void setFd(int fd);

        bool loadCheats();
        bool saveCheats();
        void applyCheats();

    private:
        Core *core;
        std::mutex mutex;
        std::string path;
        int fd = -1;

        FILE *openFile(const char *mode);
};

#endif // ACTION_REPLAY_H
