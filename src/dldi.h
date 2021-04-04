/*
    Copyright 2019-2021 Hydr8gon

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

#ifndef DLDI_H
#define DLDI_H

#include <cstdint>
#include <cstdio>

class Core;

enum DldiFunc
{
    DLDI_START = 0xF0000000,
    DLDI_INSERT,
    DLDI_READ,
    DLDI_WRITE,
    DLDI_CLEAR,
    DLDI_STOP
};

class Dldi
{
    public:
        Dldi(Core *core): core(core) {}
        ~Dldi();

        void patchDriver(uint32_t address);
        bool isFunction(uint32_t address);

        int startup();
        int isInserted();
        int readSectors(int sector, int numSectors, uint32_t buf);
        int writeSectors(int sector, int numSectors, uint32_t buf);
        int clearStatus();
        int shutdown();

    private:
        Core *core;

        uint32_t funcAddress = 0;
        FILE *sdImage = nullptr;
};

#endif // DLDI_H
