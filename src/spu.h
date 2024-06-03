/*
    Copyright 2019-2024 Hydr8gon

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

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <queue>
#include <mutex>

class Core;

class Spu
{
    public:
        Spu(Core *core);
        ~Spu();

        void saveState(FILE *file);
        void loadState(FILE *file);

        uint32_t *getSamples(int count);
        void runGbaSample();
        void runSample();
        void gbaFifoTimer(int timer);

        uint8_t  readGbaSoundCntL(int channel);
        uint16_t readGbaSoundCntH(int channel);
        uint16_t readGbaSoundCntX(int channel);
        uint16_t readGbaMainSoundCntL() { return gbaMainSoundCntL; }
        uint16_t readGbaMainSoundCntH() { return gbaMainSoundCntH; }
        uint8_t  readGbaMainSoundCntX() { return gbaMainSoundCntX; }
        uint16_t readGbaSoundBias()     { return gbaSoundBias;     }
        uint8_t  readGbaWaveRam(int index);

        uint32_t readSoundCnt(int channel)  { return soundCnt[channel];  }
        uint16_t readMainSoundCnt()         { return mainSoundCnt;       }
        uint16_t readSoundBias()            { return soundBias;          }
        uint8_t  readSndCapCnt(int channel) { return sndCapCnt[channel]; }
        uint32_t readSndCapDad(int channel) { return sndCapDad[channel]; }

        void writeGbaSoundCntL(int channel, uint8_t value);
        void writeGbaSoundCntH(int channel, uint16_t mask, uint16_t value);
        void writeGbaSoundCntX(int channel, uint16_t mask, uint16_t value);
        void writeGbaMainSoundCntL(uint16_t mask, uint16_t value);
        void writeGbaMainSoundCntH(uint16_t mask, uint16_t value);
        void writeGbaMainSoundCntX(uint8_t value);
        void writeGbaSoundBias(uint16_t mask, uint16_t value);
        void writeGbaWaveRam(int index, uint8_t value);
        void writeGbaFifoA(uint32_t mask, uint32_t value);
        void writeGbaFifoB(uint32_t mask, uint32_t value);

        void writeSoundCnt(int channel, uint32_t mask, uint32_t value);
        void writeSoundSad(int channel, uint32_t mask, uint32_t value);
        void writeSoundTmr(int channel, uint16_t mask, uint16_t value);
        void writeSoundPnt(int channel, uint16_t mask, uint16_t value);
        void writeSoundLen(int channel, uint32_t mask, uint32_t value);
        void writeMainSoundCnt(uint16_t mask, uint16_t value);
        void writeSoundBias(uint16_t mask, uint16_t value);
        void writeSndCapCnt(int channel, uint8_t value);
        void writeSndCapDad(int channel, uint32_t mask, uint32_t value);
        void writeSndCapLen(int channel, uint16_t mask, uint16_t value);

    private:
        Core *core;

        uint32_t *bufferIn = nullptr, *bufferOut = nullptr;
        uint32_t bufferSize = 0, bufferPointer = 0;

        std::condition_variable cond1, cond2;
        std::mutex mutex1, mutex2;
        std::atomic<bool> ready;

        int16_t gbaFrameSequencer = 0;
        int32_t gbaSoundTimers[4] = {};
        int8_t gbaEnvelopes[3] = {};
        int8_t gbaEnvTimers[3] = {};
        int8_t gbaSweepTimer = 0;
        int8_t gbaWaveDigit = 0;
        uint16_t gbaNoiseValue = 0;

        uint8_t gbaWaveRam[2][16] = {};
        std::deque<int8_t> gbaFifos[2];
        int8_t gbaSampleA = 0, gbaSampleB = 0;

        uint16_t enabled = 0;

        static const int indexTable[8];
        static const int16_t adpcmTable[89];

        int32_t adpcmValue[16] = {}, adpcmLoopValue[16] = {};
        int8_t adpcmIndex[16] = {}, adpcmLoopIndex[16] = {};
        bool adpcmToggle[16] = {};

        uint8_t dutyCycles[6] = {};
        uint16_t noiseValues[2] = {};
        uint32_t soundCurrent[16] = {};
        uint16_t soundTimers[16] = {};
        uint32_t sndCapCurrent[2] = {};
        uint16_t sndCapTimers[2] = {};

        uint8_t gbaSoundCntL[2] = {};
        uint16_t gbaSoundCntH[4] = {};
        uint16_t gbaSoundCntX[4] = {};
        uint16_t gbaMainSoundCntL = 0;
        uint16_t gbaMainSoundCntH = 0;
        uint8_t gbaMainSoundCntX = 0;
        uint16_t gbaSoundBias = 0;

        uint32_t soundCnt[16] = {};
        uint32_t soundSad[16] = {};
        uint16_t soundTmr[16] = {};
        uint16_t soundPnt[16] = {};
        uint32_t soundLen[16] = {};
        uint16_t mainSoundCnt = 0;
        uint16_t soundBias = 0;
        uint8_t sndCapCnt[2] = {};
        uint32_t sndCapDad[2] = {};
        uint16_t sndCapLen[2] = {};

        void swapBuffers();
        void startChannel(int channel);
};

#endif // SPU_H
