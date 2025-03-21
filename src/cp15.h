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

#ifndef CP15_H
#define CP15_H

#include <cstdint>
#include <cstdio>

class Core;

class Cp15 {
    public:
        uint32_t exceptionAddr = 0;
        bool dtcmCanRead = false, dtcmCanWrite = false;
        bool itcmCanRead = false, itcmCanWrite = false;
        uint32_t dtcmAddr = 0, dtcmSize = 0;
        uint32_t itcmSize = 0;

        Cp15(Core *core): core(core) {}
        void saveState(FILE *file);
        void loadState(FILE *file);

        uint32_t read(uint8_t cn, uint8_t cm, uint8_t cp);
        void write(uint8_t cn, uint8_t cm, uint8_t cp, uint32_t value);

    private:
        Core *core;
        uint32_t ctrlReg = 0x78;
        uint32_t dtcmReg = 0x00;
        uint32_t itcmReg = 0x00;
        uint32_t procId = 0x00;
};

#endif // CP15_H
