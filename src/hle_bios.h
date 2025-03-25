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

#ifndef HLE_BIOS_H
#define HLE_BIOS_H

#include <cstdint>
#include <cstdio>

class Core;

class HleBios {
    public:
        static int (HleBios::*swiTable9[0x21])(uint32_t**);
        static int (HleBios::*swiTable7[0x21])(uint32_t**);
        static int (HleBios::*swiTableGba[0x21])(uint32_t**);

        HleBios(Core *core, bool arm7, int (HleBios::**swiTable)(uint32_t**)):
            core(core), arm7(arm7), swiTable(swiTable) {}
        void saveState(FILE *file);
        void loadState(FILE *file);

        int execute(uint8_t vector, uint32_t **registers);
        void checkWaitFlags();
        bool shouldCheck() { return waitFlags; }

        int swiRegRamReset(uint32_t **registers);
        int swiWaitByLoop(uint32_t **registers);
        int swiInterruptWait(uint32_t **registers);
        int swiVBlankIntrWait(uint32_t **registers);
        int swiHalt(uint32_t **registers);
        int swiSleep(uint32_t **registers);
        int swiSoundBias(uint32_t **registers);
        int swiDivide(uint32_t **registers);
        int swiDivArm(uint32_t **registers);
        int swiSquareRoot(uint32_t **registers);
        int swiArcTan(uint32_t **registers);
        int swiArcTan2(uint32_t **registers);
        int swiCpuSet(uint32_t **registers);
        int swiCpuFastSet(uint32_t **registers);
        int swiGetCrc16(uint32_t **registers);
        int swiIsDebugger(uint32_t **registers);
        int swiBgAffineSet(uint32_t **registers);
        int swiObjAffineSet(uint32_t **registers);
        int swiBitUnpack(uint32_t **registers);
        int swiLz77Uncomp(uint32_t **registers);
        int swiHuffUncomp(uint32_t **registers);
        int swiRunlenUncomp(uint32_t **registers);
        int swiDiffUnfilt8(uint32_t **registers);
        int swiDiffUnfilt16(uint32_t **registers);
        int swiGetSineTable(uint32_t **registers);
        int swiGetPitchTable(uint32_t **registers);
        int swiGetVolumeTable(uint32_t **registers);
        int swiUnknown(uint32_t **registers);

    private:
        Core *core;
        bool arm7;
        int (HleBios::**swiTable)(uint32_t**);

        static const uint16_t affineTable[0x100];
        uint32_t waitFlags = 0;
};

#endif // HLE_BIOS_H
