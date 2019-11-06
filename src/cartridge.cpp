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

#include "cartridge.h"
#include "defines.h"
#include "interpreter.h"
#include "memory.h"

void Cartridge::setRom(uint8_t *rom, unsigned int romSize, uint8_t *save, unsigned int saveSize)
{
    this->rom = rom;
    this->romSize = romSize;
    this->save = save;
    this->saveSize = saveSize;
}

uint64_t Cartridge::encrypt64(uint64_t value)
{
    // Encrypt a 64-bit value using the Blowfish algorithm
    // This is a translation of the pseudocode from GBATEK to C++

    uint32_t y = value;
    uint32_t x = value >> 32;

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

    return ((uint64_t)(y ^ encTable[0x11]) << 32) | (x ^ encTable[0x10]);
}

uint64_t Cartridge::decrypt64(uint64_t value)
{
    // Decrypt a 64-bit value using the Blowfish algorithm
    // This is a translation of the pseudocode from GBATEK to C++

    uint32_t y = value;
    uint32_t x = value >> 32;

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

    return ((uint64_t)(y ^ encTable[0x00]) << 32) | (x ^ encTable[0x01]);
}

void Cartridge::initKeycode(unsigned int level)
{
    // Initialize the Blowfish encryption table
    // This is a translation of the pseudocode from GBATEK to C++

    for (int i = 0; i < 0x412; i++)
        encTable[i] = memory->read<uint32_t>(false, 0x30 + i * 4);

    uint32_t code = U8TO32(rom, 0x0C);

    encCode[0] = code;
    encCode[1] = code / 2;
    encCode[2] = code * 2;

    if (level >= 1) applyKeycode();
    if (level >= 2) applyKeycode();

    encCode[1] *= 2;
    encCode[2] /= 2;

    if (level >= 3) applyKeycode();
}

void Cartridge::applyKeycode()
{
    // Apply a keycode to the Blowfish encryption table
    // This is a translation of the pseudocode from GBATEK to C++
    
    uint64_t enc1 = encrypt64(((uint64_t)encCode[2] << 32) | encCode[1]);
    encCode[1] = enc1;
    encCode[2] = enc1 >> 32;

    uint64_t enc2 = encrypt64(((uint64_t)encCode[1] << 32) | encCode[0]);
    encCode[0] = enc2;
    encCode[1] = enc2 >> 32;

    for (int i = 0; i <= 0x11; i++)
    {
        uint32_t byteReverse = 0;
        for (int j = 0; j < 4; j++)
            byteReverse |= ((encCode[i % 2] >> (j * 8)) & 0xFF) << ((3 - j) * 8);

        encTable[i] ^= byteReverse;
    }

    uint64_t scratch = 0;

    for (int i = 0; i <= 0x410; i += 2)
    {
        scratch = encrypt64(scratch);
        encTable[i + 0] = scratch >> 32;
        encTable[i + 1] = scratch;
    }
}

void Cartridge::writeAuxSpiCnt(unsigned int byte, uint8_t value)
{
    // Write to the AUXSPICNT register
    uint16_t mask = 0xE043 & (0xFF << (byte * 8));
    auxSpiCnt = (auxSpiCnt & ~mask) | ((value << (byte * 8)) & mask);
}

void Cartridge::writeRomCmdOut(unsigned int byte, uint8_t value)
{
    // Write to the ROMCMDOUT register
    romCmdOut = (romCmdOut & ~((uint64_t)0xFF << (byte * 8))) | ((uint64_t)value << (byte * 8));
}

void Cartridge::writeAuxSpiData(uint8_t value)
{
    if (auxWriteCount == 0)
    {
        // On the first write, set the command byte
        auxCommand = value;
        auxAddress = 0;
        auxSpiData = 0;
    }
    else
    {
        switch (saveSize)
        {
            case 0x2000: case 0x10000: // EEPROM 8KB, 64KB
                switch (auxCommand)
                {
                    case 0x03: // Read from memory
                        if (auxWriteCount < 3)
                        {
                            // On writes 2-3, set the 2 byte address to read from
                            auxAddress |= value << ((2 - auxWriteCount) * 8);
                            auxSpiData = 0;
                        }
                        else
                        {
                            // On writes 4+, read data from the save and send it back
                            auxSpiData = (auxAddress < saveSize) ? save[auxAddress] : 0;
                            auxAddress++;
                        }
                        break;

                    case 0x02: // Write to memory
                        if (auxWriteCount < 3)
                        {
                            // On writes 2-3, set the 2 byte address to write to
                            auxAddress |= value << ((2 - auxWriteCount) * 8);
                            auxSpiData = 0;
                        }
                        else
                        {
                            // On writes 4+, write data to the save
                            if (auxAddress < saveSize) save[auxAddress] = value;
                            auxAddress++;
                            auxSpiData = 0;
                        }
                        break;

                    default:
                        printf("Write to AUX SPI with unknown EEPROM command: 0x%X\n", auxCommand);
                        auxSpiData = 0;
                        break;
                }
                break;

            case 0x40000: case 0x80000: // FLASH 256KB, 512KB
                switch (auxCommand)
                {
                    case 0x03: // Read data bytes
                        if (auxWriteCount < 4)
                        {
                            // On writes 2-4, set the 3 byte address to read from
                            auxAddress |= value << ((3 - auxWriteCount) * 8);
                            auxSpiData = 0;
                        }
                        else
                        {
                            // On writes 5+, read data from the save and send it back
                            auxSpiData = (auxAddress < saveSize) ? save[auxAddress] : 0;
                            auxAddress++;
                        }
                        break;

                    case 0x0A: // Page write
                    case 0x02: // Page program
                        if (auxWriteCount < 4)
                        {
                            // On writes 2-4, set the 3 byte address to write to
                            auxAddress |= value << ((3 - auxWriteCount) * 8);
                            auxSpiData = 0;
                        }
                        else
                        {
                            // On writes 5+, write data to the save
                            if (auxAddress < saveSize) save[auxAddress] = value;
                            auxAddress++;
                            auxSpiData = 0;
                        }
                        break;

                    default:
                        printf("Write to AUX SPI with unknown FLASH command: 0x%X\n", auxCommand);
                        auxSpiData = 0;
                        break;
                }
                break;

            default:
                printf("Write to AUX SPI with unknown save type\n");
                break;
        }
    }

    // Keep track of the write count
    if (auxSpiCnt & BIT(6)) // Keep chip selected
        auxWriteCount++;
    else // Deselect chip
        auxWriteCount = 0;
}

void Cartridge::writeRomCtrl(unsigned int byte, uint8_t value)
{
    bool transfer = false;

    if (byte == 3)
    {
        // Set the release reset bit, but never clear it
        romCtrl |= (value & BIT(5)) << 24;

        // Start a transfer if the start bit changes from 0 to 1
        if (!(romCtrl & BIT(31)) && (value & BIT(7)))
            transfer = true;
    }

    // Write to the ROMCTRL register
    uint32_t mask = 0xDF7F7FFF & (0xFF << (byte * 8));
    romCtrl = (romCtrl & ~mask) | ((value << (byte * 8)) & mask);

    if (!transfer) return;

    // Determine the size of the block to transfer
    uint8_t size = (romCtrl & 0x07000000) >> 24;
    switch (size)
    {
        case 0:  blockSize = 0;             break;
        case 7:  blockSize = 4;             break;
        default: blockSize = 0x100 << size; break;
    }

    // Reverse the byte order of the ROM command to make it easier to work with
    command = 0;
    for (int i = 0; i < 8; i++)
        command |= ((romCmdOut >> (i * 8)) & 0xFF) << ((7 - i) * 8);

    // Decrypt the ROM command if encryption is enabled
    if (encrypted)
    {
        initKeycode(2);
        command = decrypt64(command);
    }

    // Handle encryption commands
    if (rom)
    {
        if ((command >> 56) == 0x3C) // Activate KEY1 encryption mode
        {
            // Initialize KEY1 encryption
            encrypted = true;
        }
        else if (((command >> 56) & 0xF0) == 0xA0) // Enter main data mode
        {
            // Disable KEY1 encryption
            // On hardware, this is where KEY2 encryption would start
            encrypted = false;
        }
    }

    if (blockSize == 0)
    {
        // End the transfer right away if the block size is 0
        romCtrl &= ~BIT(23); // Word not ready
        romCtrl &= ~BIT(31); // Block ready

        // Trigger a block ready IRQ if enabled
        if (auxSpiCnt & BIT(14))
            cpu->sendInterrupt(19);
    }
    else
    {
        // Indicate that a word is ready
        romCtrl |= BIT(23);
        readCount = 0;
    }
}

uint8_t Cartridge::readRomDataIn(unsigned int byte)
{
    if (byte == 0)
    {
        // Don't transfer if the word ready bit isn't set
        if (!(romCtrl & BIT(23)))
        {
            romDataIn = 0;
            return 0;
        }

        if (rom)
        {
            // Interpret the current ROM command
            if (command == 0x9F00000000000000) // Dummy
            {
                // Return endless 0xFFs
                romDataIn = 0xFFFFFFFF;
            }
            else if (command == 0x0000000000000000) // Get header
            {
                // Return the ROM header, repeated every 0x1000 bytes
                romDataIn = U8TO32(rom, readCount % 0x1000);
            }
            else if (command == 0x9000000000000000 || ((command >> 56) & 0xF0) == 0x10 || command == 0xB800000000000000) // Get chip ID
            {
                // Return the chip ID, repeated every 4 bytes
                // ROM dumps don't provide a chip ID, so use a fake one
                romDataIn = 0x00001FC2;
            }
            else if (((command >> 56) & 0xF0) == 0x20) // Get secure area
            {
                uint32_t address = ((command & 0x0FFFF00000000000) >> 44) * 0x1000;

                // Return data from the selected secure area block
                if (address == 0x4000 && readCount < 0x800)
                {
                    // Encrypt the first 2KB of the first block
                    // The first 8 bytes of this block should contain the double-encrypted string 'encryObj'
                    // This string isn't included in ROM dumps, so manually supply it
                    uint64_t data;
                    if (readCount < 8)
                    {
                        data = 0x6A624F7972636E65; // encryObj
                    }
                    else
                    {
                        data = (uint64_t)U8TO32(rom, ((address + readCount) & ~7) + 4) << 32;
                        data |= (uint32_t)U8TO32(rom, (address + readCount) & ~7);
                    }

                    // Encrypt the data
                    initKeycode(3);
                    data = encrypt64(data);

                    // Double-encrypt the 'encryObj' string
                    if (readCount < 8)
                    {
                        initKeycode(2);
                        data = encrypt64(data);
                    }

                    romDataIn = data >> (((address + readCount) & 4) ? 32 : 0);
                }
                else
                {
                    romDataIn = U8TO32(rom, address + readCount);
                }
            }
            else if ((command & 0xFF00000000FFFFFF) == 0xB700000000000000) // Get data
            {
                // Return ROM data from the given address
                uint32_t address = (command & 0x00FFFFFFFF000000) >> 24;
                romDataIn = (address + readCount < romSize) ? U8TO32(rom, address + readCount) : 0;
            }
            else
            {
                printf("ROM transfer with unknown command: 0x%.8X%.8X\n", (uint32_t)(command >> 32), (uint32_t)command);
            }
        }
        else
        {
            // When a cart isn't inserted, endless 0xFFs are returned
            romDataIn = 0xFFFFFFFF;
        }

        readCount += 4;

        if (readCount == blockSize)
        {
            // End the transfer when the block size has been reached
            romCtrl &= ~BIT(23); // Word not ready
            romCtrl &= ~BIT(31); // Block ready

            // Trigger a block ready IRQ if enabled
            if (auxSpiCnt & BIT(14))
                cpu->sendInterrupt(19);
        }
    }

    return romDataIn >> (byte * 8);
}
