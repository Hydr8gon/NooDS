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

#include <cstring>

#include "cartridge.h"
#include "core.h"
#include "settings.h"

Cartridge::~Cartridge()
{
    // Write the save before exiting
    writeSave();

    // Free the ROM and save memory
    if (rom)     delete[] rom;
    if (save)    delete[] save;
    if (gbaRom)  delete[] gbaRom;
    if (gbaSave) delete[] gbaSave;
}

void Cartridge::loadRom(std::string filename)
{
    // Attempt to load an NDS ROM
    FILE *romFile = fopen(filename.c_str(), "rb");
    if (!romFile) throw 2;
    fseek(romFile, 0, SEEK_END);
    romSize = ftell(romFile);
    fseek(romFile, 0, SEEK_SET);
    rom = new uint8_t[romSize];
    fread(rom, sizeof(uint8_t), romSize, romFile);
    fclose(romFile);

    // Attempt to load the ROM's save file
    saveName = filename.substr(0, filename.rfind(".")) + ".sav";
    FILE *saveFile = fopen(saveName.c_str(), "rb");
    if (!saveFile) throw 3;
    fseek(saveFile, 0, SEEK_END);
    saveSize = ftell(saveFile);
    fseek(saveFile, 0, SEEK_SET);
    save = new uint8_t[saveSize];
    fread(save, sizeof(uint8_t), saveSize, saveFile);
    fclose(saveFile);
}

void Cartridge::loadGbaRom(std::string filename)
{
    // Attempt to load a GBA ROM
    FILE *gbaRomFile = fopen(filename.c_str(), "rb");
    if (!gbaRomFile) throw 2;
    fseek(gbaRomFile, 0, SEEK_END);
    gbaRomSize = ftell(gbaRomFile);
    fseek(gbaRomFile, 0, SEEK_SET);
    gbaRom = new uint8_t[gbaRomSize];
    fread(gbaRom, sizeof(uint8_t), gbaRomSize, gbaRomFile);
    fclose(gbaRomFile);

    // Attempt to load the ROM's save file
    gbaSaveName = filename.substr(0, filename.rfind(".")) + ".sav";
    FILE *gbaSaveFile = fopen(gbaSaveName.c_str(), "rb");
    if (gbaSaveFile)
    {
        fseek(gbaSaveFile, 0, SEEK_END);
        gbaSaveSize = ftell(gbaSaveFile);
        fseek(gbaSaveFile, 0, SEEK_SET);
        gbaSave = new uint8_t[gbaSaveSize];
        fread(gbaSave, sizeof(uint8_t), gbaSaveSize, gbaSaveFile);
        fclose(gbaSaveFile);
    }
    else
    {
        const std::string saveStrs[] = { "EEPROM_V", "SRAM_V", "FLASH_V", "FLASH512_V", "FLASH1M_V" };
        int match = -1;

        // Unlike the DS, a GBA cart's save type can be reliably detected by searching for strings in the ROM
        // Search the ROM for a save string so a new save of that type can be created
        for (unsigned int i = 0; i < gbaRomSize; i += 4)
        {
            for (unsigned int j = 0; j < 5; j++)
            {
                match = j;
                for (unsigned int k = 0; k < saveStrs[j].length(); k++)
                {
                    if (i + k >= gbaRomSize || gbaRom[i + k] != saveStrs[j][k])
                    {
                        match = -1;
                        break;
                    }
                }
                if (match != -1) break;
            }
            if (match != -1) break;
        }

        // Create a new GBA save of the detected type
        if (match != -1)
        {
            switch (match)
            {
                case 0: // EEPROM
                {
                    // EEPROM can be either 0.5KB or 8KB, so it will have to be determined by how the ROM interacts with it
                    gbaSaveSize = -1;
                    return;
                }

                case 1: // SRAM 32KB
                {
                    gbaSaveSize = 0x8000;
                    printf("Detected SRAM 32KB save type\n");
                    break;
                }

                case 2: case 3: // FLASH 64KB
                {
                    gbaSaveSize = 0x10000;
                    printf("Detected FLASH 64KB save type\n");
                    break;
                }

                case 4: // FLASH 128KB
                {
                    gbaSaveSize = 0x20000;
                    printf("Detected FLASH 128KB save type\n");
                    break;
                }
            }

            gbaSave = new uint8_t[gbaSaveSize];
            memset(gbaSave, 0xFF, gbaSaveSize * sizeof(uint8_t));
        }
    }
}

void Cartridge::directBoot()
{
    // Extract some information about the initial ARM9 code from the header
    uint32_t offset9    = U8TO32(rom, 0x20);
    uint32_t entryAddr9 = U8TO32(rom, 0x24);
    uint32_t ramAddr9   = U8TO32(rom, 0x28);
    uint32_t size9      = U8TO32(rom, 0x2C);
    printf("ARM9 code ROM offset:    0x%X\n", offset9);
    printf("ARM9 code entry address: 0x%X\n", entryAddr9);
    printf("ARM9 RAM address:        0x%X\n", ramAddr9);
    printf("ARM9 code size:          0x%X\n", size9);

    // Extract some information about the initial ARM7 code from the header
    uint32_t offset7    = U8TO32(rom, 0x30);
    uint32_t entryAddr7 = U8TO32(rom, 0x34);
    uint32_t ramAddr7   = U8TO32(rom, 0x38);
    uint32_t size7      = U8TO32(rom, 0x3C);
    printf("ARM7 code ROM offset:    0x%X\n", offset7);
    printf("ARM7 code entry address: 0x%X\n", entryAddr7);
    printf("ARM7 RAM address:        0x%X\n", ramAddr7);
    printf("ARM7 code size:          0x%X\n", size7);

    // Load the ROM header into memory
    for (uint32_t i = 0; i < 0x170; i++)
        core->memory.write<uint8_t>(0, 0x27FFE00 + i, rom[i]);

    // Load the initial ARM9 code into memory
    for (uint32_t i = 0; i < size9; i++)
        core->memory.write<uint8_t>(0, ramAddr9 + i, rom[offset9 + i]);

    // Load the initial ARM7 code into memory
    for (uint32_t i = 0; i < size7; i++)
        core->memory.write<uint8_t>(1, ramAddr7 + i, rom[offset7 + i]);
}

void Cartridge::writeSave()
{
    // Write an NDS save file if one is open
    if (saveSize > 0)
    {
        FILE *saveFile = fopen(saveName.c_str(), "wb");
        if (saveFile)
        {
            fwrite(save, sizeof(uint8_t), saveSize, saveFile);
            fclose(saveFile);
        }
    }

    // Write a GBA save file if one is open
    if (gbaSaveSize > 0)
    {
        FILE *gbaSaveFile = fopen(gbaSaveName.c_str(), "wb");
        if (gbaSaveFile)
        {
            fwrite(gbaSave, sizeof(uint8_t), gbaSaveSize, gbaSaveFile);
            fclose(gbaSaveFile);
        }
    }
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
        if ((gbaSaveSize == -1 || gbaSaveSize == 0x200 || gbaSaveSize == 0x2000) &&
            ((gbaRomSize <= 0x1000000 && (address & 0xFF000000) == 0x0D000000) ||
            (gbaRomSize > 0x1000000 && address >= 0x0DFFFF00 && address < 0x0E000000))) // EEPROM
        {
            if (gbaSaveSize == -1)
            {
                // Detect the save size based on how many command bits were sent before reading
                if (gbaEepromCount == 9)
                {
                    gbaSaveSize = 0x200;
                    printf("Detected EEPROM 0.5KB save type\n");
                }
                else
                {
                    gbaSaveSize = 0x2000;
                    printf("Detected EEPROM 8KB save type\n");
                }

                // Create a new save
                gbaSave = new uint8_t[gbaSaveSize];
                memset(gbaSave, 0xFF, gbaSaveSize * sizeof(uint8_t));
            }

            // EEPROM 0.5KB uses 8-bit commands, and EEPROM 8KB uses 16-bit commands
            int length = (gbaSaveSize == 0x200) ? 8 : 16;

            if (((gbaEepromCmd & 0xC000) >> 14) == 0x3 && gbaEepromCount >= length + 1) // Read
            {
                if (++gbaEepromCount >= length + 6)
                {
                    // Read the data bits, MSB first
                    int bit = 63 - (gbaEepromCount - (length + 6));
                    uint16_t addr = (gbaSaveSize == 0x200) ? ((gbaEepromCmd & 0x3F00) >> 8) : (gbaEepromCmd & 0x03FF);
                    T value = (gbaSave[addr * 8 + bit / 8] & BIT(bit % 8)) >> (bit % 8);

                    // Reset the transfer at the end
                    if (gbaEepromCount >= length + 69)
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
    else if ((gbaSaveSize == -1 || gbaSaveSize == 0x200 || gbaSaveSize == 0x2000) &&
        ((gbaRomSize <= 0x1000000 && (address & 0xFF000000) == 0x0D000000) ||
        (gbaRomSize > 0x1000000 && address >= 0x0DFFFF00 && address < 0x0E000000))) // EEPROM
    {
        gbaEepromDone = false;

        // EEPROM 0.5KB uses 8-bit commands, and EEPROM 8KB uses 16-bit commands
        int length = (gbaSaveSize == 0x200) ? 8 : 16;

        if (gbaEepromCount < length)
        {
            // Get the command bits
            gbaEepromCmd |= (value & BIT(0)) << (16 - ++gbaEepromCount);
        }
        else if (((gbaEepromCmd & 0xC000) >> 14) == 0x3) // Read
        {
            // Accept the last bit to finish the read command
            if (gbaEepromCount < length + 1)
                gbaEepromCount++;
        }
        else if (((gbaEepromCmd & 0xC000) >> 14) == 0x2) // Write
        {
            // Get the data bits, MSB first
            if (++gbaEepromCount <= length + 64)
                gbaEepromData |= (uint64_t)(value & BIT(0)) << (length + 64 - gbaEepromCount);

            if (gbaEepromCount >= length + 65)
            {
                // Games will probably read first, so the save size can be detected from that
                // If something decides to write first, it becomes a lot harder to detect the size
                // In this case, just assume EEPROM 8KB and create an empty save
                if (gbaSaveSize == -1)
                {
                    gbaSaveSize = 0x2000;
                    printf("Detected EEPROM 8KB save type\n");
                    gbaSave = new uint8_t[gbaSaveSize];
                    memset(gbaSave, 0xFF, gbaSaveSize * sizeof(uint8_t));
                }

                // Write the data after all the bits have been received
                uint16_t addr = (gbaSaveSize == 0x200) ? ((gbaEepromCmd & 0x3F00) >> 8) : (gbaEepromCmd & 0x03FF);
                for (unsigned int i = 0; i < 8; i++)
                    gbaSave[addr * 8 + i] = gbaEepromData >> (i * 8);

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
        encTable[i] = core->memory.read<uint32_t>(1, 0x30 + i * 4);

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

void Cartridge::writeAuxSpiCnt(bool cpu, uint16_t mask, uint16_t value)
{
    // Write to one of the AUXSPICNT registers
    mask &= 0xE043;
    auxSpiCnt[cpu] = (auxSpiCnt[cpu] & ~mask) | (value & mask);
}

void Cartridge::writeRomCmdOutL(bool cpu, uint32_t mask, uint32_t value)
{
    // Write to one of the ROMCMDOUT registers (low)
    romCmdOut[cpu] = (romCmdOut[cpu] & ~((uint64_t)mask)) | (value & mask);
}

void Cartridge::writeRomCmdOutH(bool cpu, uint32_t mask, uint32_t value)
{
    // Write to one of the ROMCMDOUT registers (high)
    romCmdOut[cpu] = (romCmdOut[cpu] & ~((uint64_t)mask << 32)) | ((uint64_t)(value & mask) << 32);
}

void Cartridge::writeAuxSpiData(bool cpu, uint8_t value)
{
    // Do nothing if there is no save
    if (saveSize == 0) return;

    if (auxWriteCount[cpu] == 0)
    {
        // On the first write, set the command byte
        if (value == 0) return;
        auxCommand[cpu] = value;
        auxAddress[cpu] = 0;
        auxSpiData[cpu] = 0;
    }
    else
    {
        switch (saveSize)
        {
            case 0x200: // EEPROM 0.5KB
            {
                switch (auxCommand[cpu])
                {
                    case 0x03: // Read from lower memory
                    {
                        if (auxWriteCount[cpu] < 2)
                        {
                            // On the second write, set the 1 byte address to read from
                            auxAddress[cpu] = value;
                            auxSpiData[cpu] = 0;
                        }
                        else
                        {
                            // On writes 3+, read data from the save and send it back
                            auxSpiData[cpu] = (auxAddress[cpu] < 0x100) ? save[auxAddress[cpu]] : 0;
                            auxAddress[cpu]++;
                        }
                        break;
                    }

                    case 0x0B: // Read from upper memory
                    {
                        if (auxWriteCount[cpu] < 2)
                        {
                            // On the second write, set the 1 byte address to read from
                            auxAddress[cpu] = 0x100 + value;
                            auxSpiData[cpu] = 0;
                        }
                        else
                        {
                            // On writes 3+, read data from the save and send it back
                            auxSpiData[cpu] = (auxAddress[cpu] < 0x200) ? save[auxAddress[cpu]] : 0;
                            auxAddress[cpu]++;
                        }
                        break;
                    }

                    case 0x02: // Write to lower memory
                    {
                        if (auxWriteCount[cpu] < 2)
                        {
                            // On the second write, set the 1 byte address to write to
                            auxAddress[cpu] = value;
                            auxSpiData[cpu] = 0;
                        }
                        else
                        {
                            // On writes 3+, write data to the save
                            if (auxAddress[cpu] < 0x100) save[auxAddress[cpu]] = value;
                            auxAddress[cpu]++;
                            auxSpiData[cpu] = 0;
                        }
                        break;
                    }

                    case 0x0A: // Write to upper memory
                    {
                        if (auxWriteCount[cpu] < 2)
                        {
                            // On the second write, set the 1 byte address to write to
                            auxAddress[cpu] = 0x100 + value;
                            auxSpiData[cpu] = 0;
                        }
                        else
                        {
                            // On writes 3+, write data to the save
                            if (auxAddress[cpu] < 0x200) save[auxAddress[cpu]] = value;
                            auxAddress[cpu]++;
                            auxSpiData[cpu] = 0;
                        }
                        break;
                    }

                    default:
                    {
                        printf("Write to AUX SPI with unknown EEPROM 0.5KB command: 0x%X\n", auxCommand[cpu]);
                        auxSpiData[cpu] = 0;
                        break;
                    }
                }
                break;
            }

            case 0x2000: case 0x8000: case 0x10000: case 0x20000: // EEPROM 8KB, 64KB, 128KB; FRAM 32KB
            {
                switch (auxCommand[cpu])
                {
                    case 0x03: // Read from memory
                    {
                        if (auxWriteCount[cpu] < ((saveSize == 0x20000) ? 4 : 3))
                        {
                            // On writes 2-3, set the 2 byte address to read from (not EEPROM 128KB)
                            // EEPROM 128KB uses a 3 byte address, so it's set on writes 2-4
                            auxAddress[cpu] |= value << ((((saveSize == 0x20000) ? 3 : 2) - auxWriteCount[cpu]) * 8);
                            auxSpiData[cpu] = 0;
                        }
                        else
                        {
                            // On writes 4+, read data from the save and send it back
                            auxSpiData[cpu] = (auxAddress[cpu] < saveSize) ? save[auxAddress[cpu]] : 0;
                            auxAddress[cpu]++;
                        }
                        break;
                    }

                    case 0x02: // Write to memory
                    {
                        if (auxWriteCount[cpu] < ((saveSize == 0x20000) ? 4 : 3))
                        {
                            // On writes 2-3, set the 2 byte address to write to (not EEPROM 128KB)
                            // EEPROM 128KB uses a 3 byte address, so it's set on writes 2-4
                            auxAddress[cpu] |= value << ((((saveSize == 0x20000) ? 3 : 2) - auxWriteCount[cpu]) * 8);
                            auxSpiData[cpu] = 0;
                        }
                        else
                        {
                            // On writes 4+, write data to the save
                            if (auxAddress[cpu] < saveSize) save[auxAddress[cpu]] = value;
                            auxAddress[cpu]++;
                            auxSpiData[cpu] = 0;
                        }
                        break;
                    }

                    default:
                    {
                        printf("Write to AUX SPI with unknown EEPROM/FRAM command: 0x%X\n", auxCommand[cpu]);
                        auxSpiData[cpu] = 0;
                        break;
                    }
                }
                break;
            }

            case 0x40000: case 0x80000: case 0x100000: case 0x800000: // FLASH 256KB, 512KB, 1024KB, 8192KB
            {
                switch (auxCommand[cpu])
                {
                    case 0x03: // Read data bytes
                    {
                        if (auxWriteCount[cpu] < 4)
                        {
                            // On writes 2-4, set the 3 byte address to read from
                            auxAddress[cpu] |= value << ((3 - auxWriteCount[cpu]) * 8);
                            auxSpiData[cpu] = 0;
                        }
                        else
                        {
                            // On writes 5+, read data from the save and send it back
                            auxSpiData[cpu] = (auxAddress[cpu] < saveSize) ? save[auxAddress[cpu]] : 0;
                            auxAddress[cpu]++;
                        }
                        break;
                    }

                    case 0x0A: // Page write
                    {
                        if (auxWriteCount[cpu] < 4)
                        {
                            // On writes 2-4, set the 3 byte address to write to
                            auxAddress[cpu] |= value << ((3 - auxWriteCount[cpu]) * 8);
                            auxSpiData[cpu] = 0;
                        }
                        else
                        {
                            // On writes 5+, write data to the save
                            if (auxAddress[cpu] < saveSize) save[auxAddress[cpu]] = value;
                            auxAddress[cpu]++;
                            auxSpiData[cpu] = 0;
                        }
                        break;
                    }

                    case 0x08: // IR-related
                    {
                        // If a gamecode starts with 'I', the game has an infrared port in its cartridge
                        // This shares the same SPI as FLASH memory
                        // Some games check this command as an anti-piracy measure
                        auxSpiData[cpu] = (rom[0xC] == 'I') ? 0xAA : 0;
                        break;
                    }

                    default:
                    {
                        printf("Write to AUX SPI with unknown FLASH command: 0x%X\n", auxCommand[cpu]);
                        auxSpiData[cpu] = 0;
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
    if (auxSpiCnt[cpu] & BIT(6)) // Keep chip selected
        auxWriteCount[cpu]++;
    else // Deselect chip
        auxWriteCount[cpu] = 0;
}

void Cartridge::writeRomCtrl(bool cpu, uint32_t mask, uint32_t value)
{
    bool transfer = false;

    // Set the release reset bit, but never clear it
    romCtrl[cpu] |= (value & BIT(29));

    // Start a transfer if the start bit changes from 0 to 1
    if (!(romCtrl[cpu] & BIT(31)) && (value & BIT(31)))
        transfer = true;

    // Write to one of the ROMCTRL registers
    mask &= 0xDF7F7FFF;
    romCtrl[cpu] = (romCtrl[cpu] & ~mask) | (value & mask);

    if (!transfer) return;

    // Determine the size of the block to transfer
    uint8_t size = (romCtrl[cpu] & 0x07000000) >> 24;
    switch (size)
    {
        case 0:  blockSize[cpu] = 0;             break;
        case 7:  blockSize[cpu] = 4;             break;
        default: blockSize[cpu] = 0x100 << size; break;
    }

    // Reverse the byte order of the ROM command
    command[cpu] = 0;
    for (int i = 0; i < 8; i++)
        command[cpu] |= ((romCmdOut[cpu] >> (i * 8)) & 0xFF) << ((7 - i) * 8);

    // Decrypt the ROM command if encryption is enabled
    if (encrypted[cpu])
    {
        initKeycode(2);
        command[cpu] = decrypt64(command[cpu]);
    }

    // Handle encryption commands
    if (rom)
    {
        if ((command[cpu] >> 56) == 0x3C) // Activate KEY1 encryption mode
        {
            // Initialize KEY1 encryption
            encrypted[cpu] = true;
        }
        else if (((command[cpu] >> 56) & 0xF0) == 0xA0) // Enter main data mode
        {
            // Disable KEY1 encryption
            // On hardware, this is where KEY2 encryption would start
            encrypted[cpu] = false;
        }
    }

    if (blockSize[cpu] == 0)
    {
        // End the transfer right away if the block size is 0
        romCtrl[cpu] &= ~BIT(23); // Word not ready
        romCtrl[cpu] &= ~BIT(31); // Block ready

        // Disable DS cartridge DMA transfers
        core->dma[cpu].disable((cpu == 0) ? 5 : 2);

        // Trigger a block ready IRQ if enabled
        if (auxSpiCnt[cpu] & BIT(14))
            core->interpreter[cpu].sendInterrupt(19);
    }
    else
    {
        // Indicate that a word is ready
        romCtrl[cpu] |= BIT(23);

        // Trigger DS cartridge DMA transfers
        core->dma[cpu].trigger((cpu == 0) ? 5 : 2);

        readCount[cpu] = 0;
    }
}

uint32_t Cartridge::readRomDataIn(bool cpu)
{
    // Don't transfer if the word ready bit isn't set
    if (!(romCtrl[cpu] & BIT(23)))
        return 0;

    // Endless 0xFFs are returned on a dummy command or when no cart is inserted
    uint32_t value = 0xFFFFFFFF;

    if (rom)
    {
        // Interpret the current ROM command
        if (command[cpu] == 0x0000000000000000) // Get header
        {
            // Return the ROM header, repeated every 0x1000 bytes
            value = U8TO32(rom, readCount[cpu] % 0x1000);
        }
        else if (command[cpu] == 0x9000000000000000 || ((command[cpu] >> 56) & 0xF0) == 0x10 ||
                 command[cpu] == 0xB800000000000000) // Get chip ID
        {
            // Return the chip ID, repeated every 4 bytes
            // ROM dumps don't provide a chip ID, so use a fake one
            value = 0x00001FC2;
        }
        else if (((command[cpu] >> 56) & 0xF0) == 0x20) // Get secure area
        {
            uint32_t address = ((command[cpu] & 0x0FFFF00000000000) >> 44) * 0x1000;

            // Return data from the selected secure area block
            if (address == 0x4000 && readCount[cpu] < 0x800)
            {
                // Encrypt the first 2KB of the first block
                // The first 8 bytes of this block should contain the double-encrypted string 'encryObj'
                // This string isn't included in ROM dumps, so manually supply it
                uint64_t data;
                if (readCount[cpu] < 8)
                {
                    data = 0x6A624F7972636E65; // encryObj
                }
                else
                {
                    data = (uint64_t)U8TO32(rom, ((address + readCount[cpu]) & ~7) + 4) << 32;
                    data |= (uint32_t)U8TO32(rom, (address + readCount[cpu]) & ~7);
                }

                // Encrypt the data
                initKeycode(3);
                data = encrypt64(data);

                // Double-encrypt the 'encryObj' string
                if (readCount[cpu] < 8)
                {
                    initKeycode(2);
                    data = encrypt64(data);
                }

                value = data >> (((address + readCount[cpu]) & 4) ? 32 : 0);
            }
            else
            {
                value = U8TO32(rom, address + readCount[cpu]);
            }
        }
        else if ((command[cpu] & 0xFF00000000FFFFFF) == 0xB700000000000000) // Get data
        {
            // Return ROM data from the given address
            // This command can't read the first 32KB of a ROM, so it redirects the address
            // Some games verify that the first 32KB are unreadable as an anti-piracy measure
            uint32_t address = (command[cpu] & 0x00FFFFFFFF000000) >> 24;
            if (address < 0x8000) address = 0x8000 + (address & 0x1FF);
            if (address + readCount[cpu] < romSize) value = U8TO32(rom, address + readCount[cpu]);
        }
        else if (command[cpu] != 0x9F00000000000000) // Unknown (not dummy)
        {
            printf("ROM transfer with unknown command: 0x%.8X%.8X\n", (uint32_t)(command[cpu] >> 32), (uint32_t)command[cpu]);
            value = 0;
        }
    }

    readCount[cpu] += 4;

    if (readCount[cpu] == blockSize[cpu])
    {
        // End the transfer when the block size has been reached
        romCtrl[cpu] &= ~BIT(23); // Word not ready
        romCtrl[cpu] &= ~BIT(31); // Block ready

        // Disable DS cartridge DMA transfers
        core->dma[cpu].disable((cpu == 0) ? 5 : 2);

        // Trigger a block ready IRQ if enabled
        if (auxSpiCnt[cpu] & BIT(14))
            core->interpreter[cpu].sendInterrupt(19);
    }
    else
    {
        // Trigger DS cartridge DMA transfers until the block size is reached
        core->dma[cpu].trigger((cpu == 0) ? 5 : 2);
    }

    return value;
}
