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

#ifndef GPU_H
#define GPU_H

#include <cstdint>

class Gpu2D;
class Interpreter;

class Gpu
{
    public:
        Gpu(Gpu2D *engineA, Gpu2D *engineB, Interpreter *arm9, Interpreter *arm7):
            engineA(engineA), engineB(engineB), arm9(arm9), arm7(arm7) {}

        void scanline256();
        void scanline355();

        uint16_t *getFramebuffer() { return framebuffer; }

        uint8_t readDispStat9(unsigned int byte) { return dispStat9 >> (byte * 8); }
        uint8_t readDispStat7(unsigned int byte) { return dispStat7 >> (byte * 8); }
        uint8_t readVCount(unsigned int byte)    { return vCount    >> (byte * 8); }
        uint8_t readPowCnt1(unsigned int byte)   { return powCnt1   >> (byte * 8); }

        void writeDispStat9(unsigned int byte, uint8_t value);
        void writeDispStat7(unsigned int byte, uint8_t value);
        void writePowCnt1(unsigned int byte, uint8_t value);

    private:
        uint16_t framebuffer[256 * 192 * 2] = {};

        uint16_t dispStat9 = 0, dispStat7 = 0;
        uint16_t vCount = 0;
        uint16_t powCnt1 = 0;

        Gpu2D *engineA, *engineB;
        Interpreter *arm9, *arm7;
};

#endif // GPU_H
