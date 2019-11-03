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

#ifndef MATH_H
#define MATH_H

#include <cstdint>

class Math
{
    public:
        uint8_t readDivCnt(unsigned int byte)       { return divCnt       >> (byte * 8); }
        uint8_t readDivNumer(unsigned int byte)     { return divNumer     >> (byte * 8); }
        uint8_t readDivDenom(unsigned int byte)     { return divDenom     >> (byte * 8); }
        uint8_t readDivResult(unsigned int byte)    { return divResult    >> (byte * 8); }
        uint8_t readDivRemResult(unsigned int byte) { return divRemResult >> (byte * 8); }
        uint8_t readSqrtCnt(unsigned int byte)      { return sqrtCnt      >> (byte * 8); }
        uint8_t readSqrtResult(unsigned int byte)   { return sqrtResult   >> (byte * 8); }
        uint8_t readSqrtParam(unsigned int byte)    { return sqrtParam    >> (byte * 8); }

        void writeDivCnt(uint8_t value);
        void writeDivNumer(unsigned int byte, uint8_t value);
        void writeDivDenom(unsigned int byte, uint8_t value);
        void writeSqrtCnt(uint8_t value);
        void writeSqrtParam(unsigned int byte, uint8_t value);

    private:
        uint16_t divCnt = 0;
        int64_t divNumer = 0;
        int64_t divDenom = 0;
        int64_t divResult = 0;
        int64_t divRemResult = 0;

        uint16_t sqrtCnt = 0;
        uint32_t sqrtResult = 0;
        uint64_t sqrtParam = 0;

        void divide();
        void squareRoot();
};

#endif // MATH_H
