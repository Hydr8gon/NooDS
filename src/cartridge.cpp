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

#include <cstdio>
#include <cstring>

#include "cartridge.h"
#include "core.h"
#include "memory.h"

namespace cartridge
{

uint8_t *rom;

uint64_t command;
uint16_t blockSize, readCount;
bool encrypt;

uint32_t encTable[0x412];
uint32_t encCode[3];

void decrypt64(uint64_t *value)
{
    // Decrypt a 64-bit value using the Blowfish algorithm
    // This is a translation of the pseudocode from GBATEK to C++

    uint32_t y = ((uint32_t*)value)[0];
    uint32_t x = ((uint32_t*)value)[1];

    for (int i = 0x11; i >= 0x02; i--)
    {
        uint32_t z = encTable[i] ^ x;
        x = encTable[0x012 + ((z >> 24) & 0xFF)];
        x = encTable[0x112 + ((z >> 16) & 0xFF)] + x;
        x = encTable[0x212 + ((z >>  8) & 0xFF)] ^ x;
        x = encTable[0x312 + ((z >>  0) & 0xFF)] + x;
        x ^= y;
        y = z;
    }

    ((uint32_t*)value)[0] = x ^ encTable[1];
    ((uint32_t*)value)[1] = y ^ encTable[0];
}

void encrypt64(uint64_t *value)
{
    // Encrypt a 64-bit value using the Blowfish algorithm
    // This is a translation of the pseudocode from GBATEK to C++

    uint32_t y = ((uint32_t*)value)[0];
    uint32_t x = ((uint32_t*)value)[1];

    for (int i = 0x00; i <= 0x0F; i++)
    {
        uint32_t z = encTable[i] ^ x;
        x = encTable[0x012 + ((z >> 24) & 0xFF)];
        x = encTable[0x112 + ((z >> 16) & 0xFF)] + x;
        x = encTable[0x212 + ((z >>  8) & 0xFF)] ^ x;
        x = encTable[0x312 + ((z >>  0) & 0xFF)] + x;
        x ^= y;
        y = z;
    }

    ((uint32_t*)value)[0] = x ^ encTable[0x10];
    ((uint32_t*)value)[1] = y ^ encTable[0x11];
}

uint32_t byteReverse32(uint32_t value)
{
    // Reverse the byte order of a 32-bit value
    uint32_t reverse = 0;
    for (int i = 0; i < 4; i++)
        ((uint8_t*)&reverse)[i] = ((uint8_t*)&value)[3 - i];
    return reverse;
}

void applyKeycode()
{
    // Apply a keycode to the Blowfish encryption table
    // This is a translation of the pseudocode from GBATEK to C++

    encrypt64((uint64_t*)&encCode[1]);
    encrypt64((uint64_t*)&encCode[0]);

    uint64_t scratch = 0;

    for (int i = 0; i <= 0x11; i++)
        encTable[i] ^= byteReverse32(encCode[i % 2]);

    for (int i = 0; i <= 0x410; i += 2)
    {
        encrypt64(&scratch);
        encTable[i + 0] = ((uint32_t*)&scratch)[1];
        encTable[i + 1] = ((uint32_t*)&scratch)[0];
    }
}

void initKeycode(uint32_t idCode, uint8_t level)
{
    // Initialize the Blowfish encryption table
    // This is a translation of the pseudocode from GBATEK to C++

    memcpy(encTable, &memory::bios7[0x30], sizeof(encTable));

    encCode[0] = idCode;
    encCode[1] = idCode / 2;
    encCode[2] = idCode * 2;

    if (level >= 1) applyKeycode();
    if (level >= 2) applyKeycode();

    encCode[1] *= 2;
    encCode[2] /= 2;

    if (level >= 3) applyKeycode();
}

void transferStart(interpreter::Cpu *cpu)
{
    // Get the size of the block to transfer
    uint8_t size = (*cpu->romctrl & 0x07000000) >> 24;
    switch (size)
    {
        case 0:  blockSize = 0;             break;
        case 7:  blockSize = 4;             break;
        default: blockSize = 0x100 << size; break;
    }

    // Reverse the byte order of the ROM command to make it easier to work with
    for (int i = 0; i < 8; i++)
       ((uint8_t*)&command)[i] = ((uint8_t*)cpu->romcmdout)[7 - i];

    // Decrypt the ROM command if encryption is enabled
    if (encrypt)
    {
        initKeycode(*(uint32_t*)&rom[0x0C], 2);
        decrypt64(&command);
    }

    // Handle encryption commands
    if (rom)
    {
        if (((uint8_t*)&command)[7] == 0x3C) // Activate KEY1 encryption mode
        {
            // Initialize KEY1 encryption
            encrypt = true;
        }
        else if ((((uint8_t*)&command)[7] & 0xF0) == 0xA0) // Enter main data mode
        {
            // Disable KEY1 encryption
            // On hardware, this is where KEY2 encryption would start
            encrypt = false;
        }
    }

    // Indicate transfer completion right away when the block size is 0
    if (blockSize == 0)
    {
        // Set the ready bits
        *cpu->romctrl &= ~BIT(23); // Word not ready
        *cpu->romctrl &= ~BIT(31); // Block ready

        // Trigger a block ready IRQ if enabled
        if (*cpu->auxspicnt & BIT(14))
            *cpu->irf |= BIT(19);

        return;
    }

    // Indicate that a word is ready
    // Transfers shouldn't complete instantly like this, but there's no timing system yet
    *cpu->romctrl |= BIT(23);
    readCount = 0;
}

uint32_t transfer(interpreter::Cpu *cpu)
{
    // Don't transfer if the word bit isn't set
    if (!(*cpu->romctrl & BIT(23)))
        return 0;

    // When a cart isn't inserted, endless 0xFFs are returned
    uint32_t value = 0xFFFFFFFF;

    // Interpret the current ROM command and change the return value if needed
    if (rom)
    {
        if (command == 0x0000000000000000) // Get header
        {
            // Return the ROM header, repeated every 0x1000 bytes
            value = *(uint32_t*)&rom[readCount % 0x1000];
        }
        else if (command == 0x9000000000000000 || (((uint8_t*)&command)[7] & 0xF0) == 0x10 || command == 0xB800000000000000) // Get chip ID
        {
            // Return the chip ID, repeated every 4 bytes
            // ROM dumps don't provide a chip ID, so use a fake one
            value = 0x00001FC2;
        }
        else if ((((uint8_t*)&command)[7] & 0xF0) == 0x20) // Get secure area
        {
            uint32_t address = ((command & 0x0FFFF00000000000) >> 44) * 0x1000;

            // Return data from the selected secure area block
            if (address == 0x4000 && readCount < 0x800)
            {
                // Get a 64-bit value for use with the encryption functions
                // The first 8 bytes of the first block should contain the double-encrypted string 'encryObj'
                // This string isn't included in ROM dumps, so manually supply it
                uint64_t data;
                if (readCount < 8)
                    data = 0x6A624F7972636E65; // encryObj
                else
                    data = *(uint64_t*)&rom[(address + readCount) & ~BIT(2)];

                // Encrypt the first 2KB of the first block
                initKeycode(*(uint32_t*)&rom[0x0C], 3);
                encrypt64(&data);

                // Double-encrypt the first 8 bytes of the first block
                if (readCount < 8)
                {
                    initKeycode(*(uint32_t*)&rom[0x0C], 2);
                    encrypt64(&data);
                }

                value = ((uint32_t*)&data)[((address + readCount) & BIT(2)) >> 2];
            }
            else
            {
                value = *(uint32_t*)&rom[address + readCount];
            }
        }
        else if ((command & 0xFF00000000FFFFFF) == 0xB700000000000000) // Get data
        {
            // Return ROM data from the given address
            uint32_t address = (command & 0x00FFFFFFFF000000) >> 24;
            value = *(uint32_t*)&rom[address + readCount];
        }
        else if (command != 0x9F00000000000000) // Dummy
        {
            // Split the ROM command into 2 32-bit parts for easier printing
            printf("ROM transfer with unknown command: 0x%.8X%.8X\n", ((uint32_t*)&command)[1], ((uint32_t*)&command)[0]);
        }
    }

    readCount += 4;

    // Indicate transfer completion when the block size has been reached
    if (readCount == blockSize)
    {
        // Set the ready bits
        *cpu->romctrl &= ~BIT(23); // Word not ready
        *cpu->romctrl &= ~BIT(31); // Block ready

        // Trigger a block ready IRQ if enabled
        if (*cpu->auxspicnt & BIT(14))
            *cpu->irf |= BIT(19);
    }

    return value;
}

void init()
{
    command = 0;
    blockSize = 0;
    readCount = 0;
    encrypt = false;
}

}
