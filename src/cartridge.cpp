/*
    Copyright 2019-2020 Hydr8gon

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
#include "defines.h"
#include "dma.h"
#include "interpreter.h"
#include "memory.h"

void Cartridge::setRom(uint8_t *rom, uint32_t romSize, uint8_t *save, uint32_t saveSize)
{
    this->rom = rom;
    this->romSize = romSize;
    this->save = save;
    this->saveSize = saveSize;
}

void Cartridge::setGbaRom(uint8_t *gbaRom, uint32_t gbaRomSize, uint8_t *gbaSave, uint32_t gbaSaveSize)
{
    this->gbaRom = gbaRom;
    this->gbaRomSize = gbaRomSize;
    this->gbaSave = gbaSave;
    this->gbaSaveSize = gbaSaveSize;
}

template int8_t   Cartridge::gbaRomRead(uint32_t address);
template int16_t  Cartridge::gbaRomRead(uint32_t address);
template uint8_t  Cartridge::gbaRomRead(uint32_t address);
template uint16_t Cartridge::gbaRomRead(uint32_t address);
template uint32_t Cartridge::gbaRomRead(uint32_t address);
template <typename T> T Cartridge::gbaRomRead(uint32_t address)
{
    // If nothing is inserted in the GBA slot, return endless 0xFFs
    if (gbaRomSize == 0)
        return (T)0xFFFFFFFF;

    // Read a value from the GBA cartridge
    if ((address & 0xFF000000) == 0x0E000000)
    {
        if (gbaSaveSize == 0x8000 && address < 0xE008000) // SRAM
        {
            // Read a single byte because the data bus is only 8 bits
            return gbaSave[address - 0xE000000];
        }
        else if ((gbaSaveSize == 0x10000 || gbaSaveSize == 0x20000) && address < 0xE010000) // FLASH
        {
            // Run a FLASH command
            if (gbaFlashCmd == 0x90 && address == 0xE000000)
            {
                // Read the chip manufacturer ID
                return 0xC2;
            }
            else if (gbaFlashCmd == 0x90 && address == 0xE000001)
            {
                // Read the chip device ID
                return (gbaSaveSize == 0x10000) ? 0x1C : 0x09;
            }
            else
            {
                // Read a single byte
                if (gbaBankSwap) address += 0x10000;
                return gbaSave[address - 0xE000000];
            }
        }
    }
    else
    {
        // EEPROM can be accesssed at the top 256 bytes of the 32MB ROM address block
        // If the ROM is 16MB or smaller, it can be accessed in the full upper 16MB
        if (gbaSaveSize == 0x2000 && ((gbaRomSize <= 0x1000000 && (address & 0xFF000000) == 0x0D000000) ||
            (gbaRomSize > 0x1000000 && address >= 0x0DFFFF00 && address < 0x0E000000))) // EEPROM
        {
            if (((gbaEepromCmd & 0xC000) >> 14) == 0x3 && gbaEepromCount >= 17) // Read
            {
                if (++gbaEepromCount >= 22)
                {
                    // Read the data bits, MSB first
                    int bit = 63 - (gbaEepromCount - 22);
                    T value = (gbaSave[(gbaEepromCmd & 0x03FF) * 8 + bit / 8] & BIT(bit % 8)) >> (bit % 8);

                    // Reset the transfer at the end
                    if (gbaEepromCount >= 85)
                    {
                        gbaEepromCount = 0;
                        gbaEepromCmd = 0;
                        gbaEepromData = 0;
                    }

                    return value;
                }
            }
            else if (gbaEepromDone)
            {
                // Signal that a write has finished
                return 1;
            }

            return 0;
        }
        else if ((address & 0x01FFFFFF) < gbaRomSize) // ROM
        {
            // Form an LSB-first value from the data at the ROM address
            T value = 0;
            for (unsigned int i = 0; i < sizeof(T); i++)
                value |= gbaRom[(address & 0x01FFFFFF) + i] << (i * 8);
            return value;
        }
    }

    return 0;
}

template void Cartridge::gbaRomWrite(uint32_t address, uint8_t value);
template void Cartridge::gbaRomWrite(uint32_t address, uint16_t value);
template void Cartridge::gbaRomWrite(uint32_t address, uint32_t value);
template <typename T> void Cartridge::gbaRomWrite(uint32_t address, T value)
{
    // Write a value to the GBA cartridge
    if ((address & 0xFF000000) == 0x0E000000)
    {
        if (gbaSaveSize == 0x8000 && address < 0xE008000) // SRAM
        {
            // Write a single byte because the data bus is only 8 bits
            gbaSave[address - 0xE000000] = value;
        }
        else if ((gbaSaveSize == 0x10000 || gbaSaveSize == 0x20000) && address < 0xE010000) // FLASH
        {
            // Run a FLASH command
            if (gbaFlashCmd == 0xA0)
            {
                // Write a single byte
                if (gbaBankSwap) address += 0x10000;
                gbaSave[address - 0xE000000] = value;
                gbaFlashCmd = 0xF0;
            }
            else if (gbaFlashErase && (address & ~0x000F000) == 0xE000000 && (value & 0xFF) == 0x30)
            {
                // Erase a sector
                if (gbaBankSwap) address += 0x10000;
                memset(&gbaSave[address - 0xE000000], 0xFF, 0x1000 * sizeof(uint8_t));
                gbaFlashErase = false;
            }
            else if (gbaSaveSize == 0x20000 && gbaFlashCmd == 0xB0 && address == 0xE000000)
            {
                // Swap the ROM banks on 128KB carts
                gbaBankSwap = value;
                gbaFlashCmd = 0xF0;
            }
            else if (address == 0xE005555)
            {
                // Write the FLASH command byte
                gbaFlashCmd = value;

                // Handle erase commands
                if (gbaFlashCmd == 0x80)
                    gbaFlashErase = true;
                else if (gbaFlashCmd != 0xAA)
                    gbaFlashErase = false;
                else if (gbaFlashErase && gbaFlashCmd == 0x10)
                    memset(gbaSave, 0xFF, gbaSaveSize * sizeof(uint8_t));
            }
        }
    }
    else if (gbaSaveSize == 0x2000 && ((gbaRomSize <= 0x1000000 && (address & 0xFF000000) == 0x0D000000) ||
            (gbaRomSize > 0x1000000 && address >= 0x0DFFFF00 && address < 0x0E000000))) // EEPROM
    {
        gbaEepromDone = false;

        if (gbaEepromCount < 16)
        {
            // Get the command bits
            gbaEepromCmd |= (value & BIT(0)) << (16 - ++gbaEepromCount);
        }
        else if (((gbaEepromCmd & 0xC000) >> 14) == 0x3) // Read
        {
            // Accept the last bit to finish the read command
            if (gbaEepromCount < 17)
                gbaEepromCount++;
        }
        else if (((gbaEepromCmd & 0xC000) >> 14) == 0x2) // Write
        {
            // Get the data bits, MSB first
            if (++gbaEepromCount <= 80)
                gbaEepromData |= (uint64_t)(value & BIT(0)) << (80 - gbaEepromCount);

            if (gbaEepromCount >= 81)
            {
                // Write the data after all the bits have been received
                for (unsigned int i = 0; i < 8; i++)
                    gbaSave[(gbaEepromCmd & 0x03FF) * 8 + i] = gbaEepromData >> (i * 8);

                // Reset the transfer
                gbaEepromCount = 0;
                gbaEepromCmd = 0;
                gbaEepromData = 0;
                gbaEepromDone = true;
            }
        }

        return;
    }
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

void Cartridge::initKeycode(int level)
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

void Cartridge::writeAuxSpiCnt(uint16_t mask, uint16_t value)
{
    // Write to the AUXSPICNT register
    mask &= 0xE043;
    auxSpiCnt = (auxSpiCnt & ~mask) | (value & mask);
}

void Cartridge::writeRomCmdOutL(uint32_t mask, uint32_t value)
{
    // Write to the ROMCMDOUT register
    romCmdOut = (romCmdOut & ~((uint64_t)mask)) | (value & mask);
}

void Cartridge::writeRomCmdOutH(uint32_t mask, uint32_t value)
{
    // Write to the ROMCMDOUT register
    romCmdOut = (romCmdOut & ~((uint64_t)mask << 32)) | ((uint64_t)(value & mask) << 32);
}

void Cartridge::writeAuxSpiData(uint8_t value)
{
    if (auxWriteCount == 0)
    {
        // On the first write, set the command byte
        if (value == 0) return;
        auxCommand = value;
        auxAddress = 0;
        auxSpiData = 0;
    }
    else
    {
        switch (saveSize)
        {
            case 0x200: // EEPROM 0.5KB
            {
                switch (auxCommand)
                {
                    case 0x03: // Read from lower memory
                    {
                        if (auxWriteCount < 2)
                        {
                            // On the second write, set the 1 byte address to read from
                            auxAddress = value;
                            auxSpiData = 0;
                        }
                        else
                        {
                            // On writes 3+, read data from the save and send it back
                            auxSpiData = (auxAddress < 0x100) ? save[auxAddress] : 0;
                            auxAddress++;
                        }
                        break;
                    }

                    case 0x0B: // Read from upper memory
                    {
                        if (auxWriteCount < 2)
                        {
                            // On the second write, set the 1 byte address to read from
                            auxAddress = 0x100 + value;
                            auxSpiData = 0;
                        }
                        else
                        {
                            // On writes 3+, read data from the save and send it back
                            auxSpiData = (auxAddress < 0x200) ? save[auxAddress] : 0;
                            auxAddress++;
                        }
                        break;
                    }

                    case 0x02: // Write to lower memory
                    {
                        if (auxWriteCount < 2)
                        {
                            // On the second write, set the 1 byte address to write to
                            auxAddress = value;
                            auxSpiData = 0;
                        }
                        else
                        {
                            // On writes 3+, write data to the save
                            if (auxAddress < 0x100) save[auxAddress] = value;
                            auxAddress++;
                            auxSpiData = 0;
                        }
                        break;
                    }

                    case 0x0A: // Write to upper memory
                    {
                        if (auxWriteCount < 2)
                        {
                            // On the second write, set the 1 byte address to write to
                            auxAddress = 0x100 + value;
                            auxSpiData = 0;
                        }
                        else
                        {
                            // On writes 3+, write data to the save
                            if (auxAddress < 0x200) save[auxAddress] = value;
                            auxAddress++;
                            auxSpiData = 0;
                        }
                        break;
                    }

                    default:
                    {
                        printf("Write to AUX SPI with unknown EEPROM 0.5KB command: 0x%X\n", auxCommand);
                        auxSpiData = 0;
                        break;
                    }
                }
                break;
            }

            case 0x2000: case 0x8000: case 0x10000: // EEPROM/FRAM 8KB, 32KB, 64KB
            {
                switch (auxCommand)
                {
                    case 0x03: // Read from memory
                    {
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
                    }

                    case 0x02: // Write to memory
                    {
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
                    }

                    default:
                    {
                        printf("Write to AUX SPI with unknown EEPROM/FRAM command: 0x%X\n", auxCommand);
                        auxSpiData = 0;
                        break;
                    }
                }
                break;
            }

            case 0x40000: case 0x80000: case 0x100000: case 0x800000: // FLASH 256KB, 512KB, 1024KB, 8192KB
            {
                switch (auxCommand)
                {
                    case 0x03: // Read data bytes
                    {
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
                    }

                    case 0x0A: // Page write
                    case 0x02: // Page program
                    {
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
                    }

                    case 0x08: // IR-related
                    {
                        // If a gamecode starts with 'I', the game has an infrared port in its cartridge
                        // This shares the same SPI as FLASH memory
                        // Some games check this command as an anti-piracy measure
                        auxSpiData = (rom[0xC] == 'I') ? 0xAA : 0;
                        break;
                    }

                    default:
                    {
                        printf("Write to AUX SPI with unknown FLASH command: 0x%X\n", auxCommand);
                        auxSpiData = 0;
                        break;
                    }
                }
                break;
            }

            default:
            {
                printf("Write to AUX SPI with unknown save type\n");
                break;
            }
        }
    }

    // Keep track of the write count
    if (auxSpiCnt & BIT(6)) // Keep chip selected
        auxWriteCount++;
    else // Deselect chip
        auxWriteCount = 0;
}

void Cartridge::writeRomCtrl(uint32_t mask, uint32_t value)
{
    bool transfer = false;

    // Set the release reset bit, but never clear it
    romCtrl |= (value & BIT(29));

    // Start a transfer if the start bit changes from 0 to 1
    if (!(romCtrl & BIT(31)) && (value & BIT(31)))
        transfer = true;

    // Write to the ROMCTRL register
    mask &= 0xDF7F7FFF;
    romCtrl = (romCtrl & ~mask) | (value & mask);

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

        // Disable DS cartridge DMA transfers
        dma->setMode(5, false);

        // Trigger a block ready IRQ if enabled
        if (auxSpiCnt & BIT(14))
            cpu->sendInterrupt(19);
    }
    else
    {
        // Indicate that a word is ready
        romCtrl |= BIT(23);

        // Enable DS cartridge DMA transfers
        dma->setMode(5, true);

        readCount = 0;
    }
}

uint32_t Cartridge::readRomDataIn()
{
    // Don't transfer if the word ready bit isn't set
    if (!(romCtrl & BIT(23)))
        return 0;

    // Endless 0xFFs are returned on a dummy command or when no cart is inserted
    uint32_t value = 0xFFFFFFFF;

    if (rom)
    {
        // Interpret the current ROM command
        if (command == 0x0000000000000000) // Get header
        {
            // Return the ROM header, repeated every 0x1000 bytes
            value = U8TO32(rom, readCount % 0x1000);
        }
        else if (command == 0x9000000000000000 || ((command >> 56) & 0xF0) == 0x10 || command == 0xB800000000000000) // Get chip ID
        {
            // Return the chip ID, repeated every 4 bytes
            // ROM dumps don't provide a chip ID, so use a fake one
            value = 0x00001FC2;
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

                value = data >> (((address + readCount) & 4) ? 32 : 0);
            }
            else
            {
                value = U8TO32(rom, address + readCount);
            }
        }
        else if ((command & 0xFF00000000FFFFFF) == 0xB700000000000000) // Get data
        {
            // Return ROM data from the given address
            // This command can't read the first 32KB of a ROM, so it redirects the address
            // Some games verify that the first 32KB are unreadable as an anti-piracy measure
            uint32_t address = (command & 0x00FFFFFFFF000000) >> 24;
            if (address < 0x8000) address = 0x8000 + (address & 0x1FF);
            value = (address + readCount < romSize) ? U8TO32(rom, address + readCount) : 0;
        }
        else if (command != 0x9F00000000000000) // Unknown (not dummy)
        {
            printf("ROM transfer with unknown command: 0x%.8X%.8X\n", (uint32_t)(command >> 32), (uint32_t)command);
            value = 0;
        }
    }

    readCount += 4;

    if (readCount == blockSize)
    {
        // End the transfer when the block size has been reached
        romCtrl &= ~BIT(23); // Word not ready
        romCtrl &= ~BIT(31); // Block ready

        // Disable DS cartridge DMA transfers
        dma->setMode(5, false);

        // Trigger a block ready IRQ if enabled
        if (auxSpiCnt & BIT(14))
            cpu->sendInterrupt(19);
    }

    return value;
}
