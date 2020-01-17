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

#ifndef SPU_H
#define SPU_H

#include <cstdint>

class Memory;

class Spu
{
    public:
        Spu(Memory *memory): memory(memory) {}

        uint32_t getSample();

        uint32_t readSoundCnt(int channel) { return soundCnt[channel]; }
        uint16_t readMainSoundCnt()        { return mainSoundCnt;      }
        uint16_t readSoundBias()           { return soundBias;         }

        void writeSoundCnt(int channel, uint32_t mask, uint32_t value);
        void writeSoundSad(int channel, uint32_t mask, uint32_t value);
        void writeSoundTmr(int channel, uint16_t mask, uint16_t value);
        void writeSoundPnt(int channel, uint16_t mask, uint16_t value);
        void writeSoundLen(int channel, uint32_t mask, uint32_t value);
        void writeMainSoundCnt(uint16_t mask, uint16_t value);
        void writeSoundBias(uint16_t mask, uint16_t value);

    private:
        static const int indexTable[8];
        static const int16_t adpcmTable[89];

        int32_t adpcmValue[16] = {}, adpcmLoopValue[16] = {};
        int adpcmIndex[16] = {}, adpcmLoopIndex[16] = {};
        bool adpcmToggle[16] = {};

        uint32_t soundCurrent[16] = {};
        uint16_t soundTimers[16] = {};

        uint32_t soundCnt[16] = {};
        uint32_t soundSad[16] = {};
        uint16_t soundTmr[16] = {};
        uint16_t soundPnt[16] = {};
        uint32_t soundLen[16] = {};
        uint16_t mainSoundCnt = 0;
        uint16_t soundBias = 0;

        Memory *memory;
};

#endif // SPU_H
