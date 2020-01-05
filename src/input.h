/*
    Copyright 2019 Hydr8gon

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

class Input
{
    public:
        void pressKey(unsigned int key);
        void releaseKey(unsigned int key);
        void pressScreen();
        void releaseScreen();

        uint16_t readKeyInput() { return keyInput; }
        uint16_t readExtKeyIn() { return extKeyIn; }

    private:
        uint16_t keyInput = 0x03FF;
        uint16_t extKeyIn = 0x007F;
};

#endif // INPUT_H
