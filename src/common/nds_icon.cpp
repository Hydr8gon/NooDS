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

#include <cstring>

#include "nds_icon.h"
#include "../defines.h"

NdsIcon::NdsIcon(std::string romPath)
{
    // Attempt to open the ROM
    FILE *rom = fopen(romPath.c_str(), "rb");

    // Create an empty icon if ROM loading failed
    if (!rom)
    {
        LOG("Failed to open ROM for icon decoding!\n");
        memset(icon, 0, 32 * 32 * sizeof(uint32_t));
        return;
    }

    // Get the icon offset
    uint32_t offset;
    fseek(rom, 0x68, SEEK_SET);
    fread(&offset, sizeof(uint32_t), 1, rom);

    // Get the icon data
    uint8_t data[512];
    fseek(rom, offset + 0x20, SEEK_SET);
    fread(data, sizeof(uint8_t), 512, rom);

    // Get the icon palette
    uint16_t palette[16];
    fseek(rom, offset + 0x220, SEEK_SET);
    fread(palette, sizeof(uint16_t), 16, rom);

    fclose(rom);

    // Get each pixel's 5-bit palette color and convert it to 8-bit
    uint32_t tiles[32 * 32];
    for (int i = 0; i < 32 * 32; i++)
    {
        uint8_t index = (i & 1) ? ((data[i / 2] & 0xF0) >> 4) : (data[i / 2] & 0x0F);
        uint8_t r = index ? (((palette[index] >>  0) & 0x1F) * 255 / 31) : 0xFF;
        uint8_t g = index ? (((palette[index] >>  5) & 0x1F) * 255 / 31) : 0xFF;
        uint8_t b = index ? (((palette[index] >> 10) & 0x1F) * 255 / 31) : 0xFF;
        tiles[i] = (0xFF << 24) | (b << 16) | (g << 8) | r;
    }

    // Rearrange the pixels from 8x8 tiles to a 32x32 icon
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            for (int k = 0; k < 4; k++)
                memcpy(&icon[256 * i + 32 * j + 8 * k], &tiles[256 * i + 8 * j + 64 * k], 8 * sizeof(uint32_t));
        }
    }
}
