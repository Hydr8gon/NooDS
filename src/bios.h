/*
    Copyright 2019-2022 Hydr8gon

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

#ifndef BIOS_H
#define BIOS_H

#include <cstdint>

class Core;

class Bios
{
    public:
        Bios(Core *core, int (Bios::**swiTable)(bool, uint32_t**)):
            core(core), swiTable(swiTable) {}

        int execute(uint8_t vector, bool cpu, uint32_t **registers);
        void checkWaitFlags(bool cpu);

        bool shouldCheck() { return waitFlags; }

        int swiWaitByLoop(bool cpu, uint32_t **registers);
        int swiInterruptWait(bool cpu, uint32_t **registers);
        int swiVBlankIntrWait(bool cpu, uint32_t **registers);
        int swiHalt(bool cpu, uint32_t **registers);
        int swiDivide(bool cpu, uint32_t **registers);
        int swiCpuSet(bool cpu, uint32_t **registers);
        int swiCpuFastSet(bool cpu, uint32_t **registers);
        int swiSquareRoot(bool cpu, uint32_t **registers);
        int swiGetCrc16(bool cpu, uint32_t **registers);
        int swiIsDebugger(bool cpu, uint32_t **registers);
        int swiBitUnpack(bool cpu, uint32_t **registers);
        int swiLz77Uncomp(bool cpu, uint32_t **registers);
        int swiRunlenUncomp(bool cpu, uint32_t **registers);
        int swiUnknown(bool cpu, uint32_t **registers);

    protected:
        Core *core;

    private:
        int (Bios::**swiTable)(bool, uint32_t**);
        uint32_t waitFlags = 0;
};

class Bios9: public Bios
{
    public:
        Bios9(Core *core):
            Bios(core, (int (Bios::**)(bool, uint32_t**))swiTable9) {}

    private:
        static int (Bios9::*swiTable9[0x21])(bool, uint32_t**);

        int swiDiffUnfilt8(bool cpu, uint32_t **registers);
        int swiDiffUnfilt16(bool cpu, uint32_t **registers);
};

class Bios7: public Bios
{
    public:
        Bios7(Core *core):
            Bios(core, (int (Bios::**)(bool, uint32_t**))swiTable7) {}

    private:
        static int (Bios7::*swiTable7[0x21])(bool, uint32_t**);

        int swiSleep(bool cpu, uint32_t **registers);
        int swiSoundBias(bool cpu, uint32_t **registers);
        int swiGetSineTable(bool cpu, uint32_t **registers);
        int swiGetPitchTable(bool cpu, uint32_t **registers);
        int swiGetVolumeTable(bool cpu, uint32_t **registers);
};


#endif // BIOS_H
