/*
    Copyright 2020 Hydr8gon

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

#include <condition_variable>
#include <cstdint>
#include <mutex>

class Memory;

class Spu
{
    public:
        Spu(Memory *memory): memory(memory) {}
        ~Spu();

        uint32_t *getSamples(int count);

        void runGbaSample();
        void runSample();

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
        uint32_t *bufferIn = nullptr, *bufferOut = nullptr;
        int bufferSize = 0, bufferPointer = 0;

        std::condition_variable cond1, cond2;
        std::mutex mutex1, mutex2;
        bool ready = false;

        static const int indexTable[8];
        static const int16_t adpcmTable[89];

        int32_t adpcmValue[16] = {}, adpcmLoopValue[16] = {};
        int adpcmIndex[16] = {}, adpcmLoopIndex[16] = {};
        bool adpcmToggle[16] = {};

        int dutyCycles[6] = {};
        uint16_t noiseValues[2] = {};

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

        bool shouldPlay() { return  ready; }
        bool shouldFill() { return !ready; }
};

#endif // SPU_H
