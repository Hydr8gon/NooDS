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

#ifndef CP15_H
#define CP15_H

#include <cstdint>

class Interpreter;

class Cp15
{
    public:
        Cp15(Interpreter *arm9): arm9(arm9) {}

        uint32_t read(unsigned int cn, unsigned int cm, unsigned int cp);
        void write(unsigned int cn, unsigned int cm, unsigned int cp, uint32_t value);

        uint32_t getExceptionAddr() { return exceptionAddr; }
        bool getDtcmEnabled()       { return dtcmEnabled;   }
        bool getItcmEnabled()       { return itcmEnabled;   }
        uint32_t getDtcmAddr()      { return dtcmAddr;      }
        uint32_t getDtcmSize()      { return dtcmSize;      }
        uint32_t getItcmSize()      { return itcmSize;      }

    private:
        uint32_t ctrlReg = 0x00000078;
        uint32_t dtcmReg = 0x00000000;
        uint32_t itcmReg = 0x00000000;

        uint32_t exceptionAddr = 0;
        bool dtcmEnabled = false;
        bool itcmEnabled = false;
        uint32_t dtcmAddr = 0, dtcmSize = 0;
        uint32_t itcmSize = 0;

        Interpreter *arm9;
};

#endif // CP15_H
