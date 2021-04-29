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

#include "dldi.h"
#include "core.h"
#include "settings.h"

Dldi::~Dldi()
{
    // Ensure the SD image is closed
    if (sdImage)
        fclose(sdImage);
}

void Dldi::patchDriver(uint32_t address)
{
    // Patch the DLDI driver to use the HLE functions
    core->memory.write<uint8_t> (0, address + 0x0F, 0x0E);           // Size of driver in terms of 1 << n (16KB)
    core->memory.write<uint32_t>(0, address + 0x40, address);        // Address of driver
    core->memory.write<uint32_t>(0, address + 0x64, 0x00000023);     // Feature flags (read, write, NDS slot)
    core->memory.write<uint32_t>(0, address + 0x68, address + 0x80); // Address of startup()
    core->memory.write<uint32_t>(0, address + 0x6C, address + 0x84); // Address of isInserted()
    core->memory.write<uint32_t>(0, address + 0x70, address + 0x88); // Address of readSectors(sector, numSectors, buf)
    core->memory.write<uint32_t>(0, address + 0x74, address + 0x8C); // Address of writeSectors(sector, numSectors, buf)
    core->memory.write<uint32_t>(0, address + 0x78, address + 0x90); // Address of clearStatus()
    core->memory.write<uint32_t>(0, address + 0x7C, address + 0x94); // Address of shutdown()
    core->memory.write<uint32_t>(0, address + 0x80, DLDI_START);     // startup()
    core->memory.write<uint32_t>(0, address + 0x84, DLDI_INSERT);    // isInserted()
    core->memory.write<uint32_t>(0, address + 0x88, DLDI_READ);      // readSectors(sector, numSectors, buf)
    core->memory.write<uint32_t>(0, address + 0x8C, DLDI_WRITE);     // writeSectors(sector, numSectors, buf)
    core->memory.write<uint32_t>(0, address + 0x90, DLDI_CLEAR);     // clearStatus()
    core->memory.write<uint32_t>(0, address + 0x94, DLDI_STOP);      // shutdown()
    funcAddress = address + 0x80;

    LOG("Patched DLDI driver at 0x%X\n", address);
}

bool Dldi::isFunction(uint32_t address)
{
    // Check if an address points to a DLDI function
    return (funcAddress != 0 && address >= funcAddress && address < funcAddress + 0x18);
}

int Dldi::startup()
{
    // Try to open the SD image
    sdImage = fopen(Settings::getSdImagePath().c_str(), "rb+");
    return (sdImage ? 1 : 0);
}

int Dldi::isInserted()
{
    // Check if the SD image is opened
    return (sdImage ? 1 : 0);
}

int Dldi::readSectors(int sector, int numSectors, uint32_t buf)
{
    if (!sdImage) return 0;

    const int offset = sector * 0x200;
    const int size = numSectors * 0x200;

    // Read data from the SD image
    uint8_t *data = new uint8_t[size];
    fseek(sdImage, offset, SEEK_SET);
    fread(data, sizeof(uint8_t), size, sdImage);

    // Write the data to memory
    for (int i = 0; i < size; i++)
        core->memory.write<uint8_t>(0, buf + i, data[i]);

    delete[] data;
    return 1;
}

int Dldi::writeSectors(int sector, int numSectors, uint32_t buf)
{
    if (!sdImage) return 0;

    const int offset = sector * 0x200;
    const int size = numSectors * 0x200;

    // Read data from memory
    uint8_t *data = new uint8_t[size];
    for (int i = 0; i < size; i++)
        data[i] = core->memory.read<uint8_t>(0, buf + i);

    // Write the data to the SD image
    fseek(sdImage, offset, SEEK_SET);
    fwrite(data, sizeof(uint8_t), size, sdImage);

    delete[] data;
    return 1;
}

int Dldi::clearStatus()
{
    // Dummy function
    return (sdImage ? 1 : 0);
}

int Dldi::shutdown()
{
    if (!sdImage) return 0;

    // Close the SD image
    fclose(sdImage);
    sdImage = nullptr;
    return 1;
}
