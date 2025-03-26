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

#ifndef HLE_ARM7_H
#define HLE_ARM7_H

#include <cstdint>
#include <cstdio>

class Core;

class HleArm7 {
    public:
        HleArm7(Core *core): core(core) {}
        void init();

        void saveState(FILE *file);
        void loadState(FILE *file);

        void ipcSync(uint8_t value);
        void ipcFifo(uint32_t value);
        void runFrame();

    private:
        Core *core;
        bool inited = false;
        bool autoTouch = false;

        void pollTouch(uint32_t value);
};

#endif // HLE_ARM7_H
