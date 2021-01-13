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

#ifndef INPUT_H
#define INPUT_H

#include <cstdint>

class Core;

class Input
{
    public:
        Input(Core *core): core(core) {}

        void pressKey(int key);
        void releaseKey(int key);
        void pressScreen();
        void releaseScreen();

        uint16_t readKeyInput() { return keyInput; }
        uint16_t readExtKeyIn() { return extKeyIn; }

    private:
        Core *core;

        uint16_t keyInput = 0x03FF;
        uint16_t extKeyIn = 0x007F;
};

#endif // INPUT_H
