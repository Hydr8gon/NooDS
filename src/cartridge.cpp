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

#include <cstring>

#include "cartridge.h"
#include "core.h"
#include "settings.h"

Cartridge::~Cartridge()
{
    // Update the save file before exiting
    writeSave();

    // Free the ROM and save memory
    if (romFile) fclose(romFile);
    if (rom) delete[] rom;
    if (save) delete[] save;
}

bool Cartridge::setRom(std::string romPath, int romFd, int saveFd, int stateFd, int cheatFd)
{
    // Derive file names with extensions based on instance ID
    std::string basePath = romPath.substr(0, romPath.rfind('.'));
    std::string savePath = basePath + (core->id ? (".sv" + std::to_string(core->id + 1)) : ".sav");
    std::string statePath = basePath + (core->id ? (".no" + std::to_string(core->id + 1)) : ".noo");
    std::string cheatPath = basePath + ".cht";

    // Relocate files to separate folders if enabled
    if (Settings::savesFolder)
        savePath = Settings::basePath + "/saves" + savePath.substr(savePath.find_last_of("/\\"));
    if (Settings::statesFolder)
        statePath = Settings::basePath + "/states" + statePath.substr(statePath.find_last_of("/\\"));
    if (Settings::cheatsFolder)
        cheatPath = Settings::basePath + "/cheats" + cheatPath.substr(cheatPath.find_last_of("/\\"));

    // Load files using paths or descriptors if provided
    bool gba = (this == &core->cartridgeGba);
    if (romFd == -1) this->romPath = romPath; else this->romFd = romFd;
    if (saveFd == -1) this->savePath = savePath; else this->saveFd = saveFd;
    (stateFd == -1) ? core->saveStates.setPath(statePath, gba) : core->saveStates.setFd(stateFd, gba);
    if (!gba) (cheatFd == -1) ? core->actionReplay.setPath(cheatPath) : core->actionReplay.setFd(cheatFd);
    return loadRom();
}

bool Cartridge::loadRom()
{
    // Attempt to open a ROM file
    romFile = (romFd == -1) ? fopen(romPath.c_str(), "rb") : fdopen(dup(romFd), "rb");
    if (!romFile) return false;
    fseek(romFile, 0, SEEK_END);
    romSize = ftell(romFile);
    fseek(romFile, 0, SEEK_SET);

    // Attempt to load the ROM's save into memory
    if (FILE *saveFile = (saveFd == -1) ? fopen(savePath.c_str(), "rb") : fdopen(dup(saveFd), "rb"))
    {
        fseek(saveFile, 0, SEEK_END);
        saveSize = ftell(saveFile);
        fseek(saveFile, 0, SEEK_SET);
        save = new uint8_t[saveSize];
        fread(save, sizeof(uint8_t), saveSize, saveFile);
        fclose(saveFile);
    }

    // Verify the save size; invalid sizes fall back to auto-detection
    for (size_t i = 0; i < saveSizes.size(); i++)
        if (saveSize == saveSizes[i]) return true;
    saveSize = -1;
    return true;
}

void Cartridge::loadRomSection(size_t offset, size_t size)
{
    // Load a section of the current ROM file into memory
    if (rom) delete[] rom;
    rom = new uint8_t[size];
    fseek(romFile, offset, SEEK_SET);
    fread(rom, sizeof(uint8_t), size, romFile);
    core->dldi.patchRom(rom, offset, size);
}

void Cartridge::writeSave()
{
    // Update the save file if the data changed
    mutex.lock();
    if (saveDirty)
    {
        if (FILE *saveFile = (saveFd == -1) ? fopen(savePath.c_str(), "wb") : fdopen(dup(saveFd), "wb"))
        {
            // Overwrite and resize without closing the file descriptor
            if (saveFd != -1)
            {
                fseek(saveFile, 0, SEEK_SET);
                ftruncate(saveFd, saveSize);
            }

            LOG_INFO("Writing save file to disk\n");
            fwrite(save, sizeof(uint8_t), saveSize, saveFile);
            fclose(saveFile);
            saveDirty = false;
        }
    }
    mutex.unlock();
}

void Cartridge::trimRom()
{
    // Starting from the end, reduce the ROM size until a non-filler word is found
    int newSize;
    for (newSize = romSize & ~3; newSize > 0; newSize -= 4)
    {
        if (U8TO32(rom, newSize - 4) != 0xFFFFFFFF)
            break;
    }

    if (newSize < romSize)
    {
        // Update the ROM in memory
        romSize = newSize;
        uint8_t *newRom = new uint8_t[newSize];
        memcpy(newRom, rom, newSize * sizeof(uint8_t));
        delete[] rom;
        rom = newRom;

        // Update the ROM file
        FILE *romFile = (romFd == -1) ? fopen(romPath.c_str(), "wb") : fdopen(dup(romFd), "wb");
        if (romFile)
        {
            if (newSize > 0)
                fwrite(rom, sizeof(uint8_t), newSize, romFile);
            fclose(romFile);
        }
    }
}

void Cartridge::resizeSave(int newSize, bool dirty)
{
    mutex.lock();
    uint8_t *newSave = new uint8_t[newSize];

    // Resize the save
    if (saveSize < newSize) // New save is larger
    {
        // Copy all of the old save and fill the rest with 0xFF
        if (saveSize < 0) saveSize = 0;
        memcpy(newSave, save, saveSize * sizeof(uint8_t));
        memset(&newSave[saveSize], 0xFF, (newSize - saveSize) * sizeof(uint8_t));
    }
    else // New save is smaller
    {
        // Copy as much of the old save as possible
        memcpy(newSave, save, newSize * sizeof(uint8_t));
    }

    // Swap the old save for the new one
    delete[] save;
    save = newSave;
    saveSize = newSize;
    if (dirty) saveDirty = true;
    mutex.unlock();
}

void CartridgeNds::saveState(FILE *file)
{
    // Write state data to the file
    fwrite(&saveSize, sizeof(saveSize), 1, file);
    if (saveSize > 0) fwrite(save, 1, saveSize, file);
    fwrite(&cmdMode, sizeof(cmdMode), 1, file);
    fwrite(encTable, 4, sizeof(encTable) / 4, file);
    fwrite(encCode, 4, sizeof(encCode) / 4, file);
    fwrite(romAddrReal, 4, sizeof(romAddrReal) / 4, file);
    fwrite(romAddrVirt, 4, sizeof(romAddrVirt) / 4, file);
    fwrite(blockSize, 2, sizeof(blockSize) / 2, file);
    fwrite(readCount, 2, sizeof(readCount) / 2, file);
    fwrite(wordCycles, 4, sizeof(wordCycles) / 4, file);
    fwrite(encrypted, sizeof(bool), sizeof(encrypted) / sizeof(bool), file);
    fwrite(auxCommand, 1, sizeof(auxCommand), file);
    fwrite(auxAddress, 4, sizeof(auxAddress) / 4, file);
    fwrite(auxWriteCount, 4, sizeof(auxWriteCount) / 4, file);
    fwrite(auxSpiCnt, 2, sizeof(auxSpiCnt) / 2, file);
    fwrite(auxSpiData, 1, sizeof(auxSpiData), file);
    fwrite(romCtrl, 4, sizeof(romCtrl) / 4, file);
    fwrite(romCmdOut, 8, sizeof(romCmdOut) / 8, file);
}

void CartridgeNds::loadState(FILE *file)
{
    // Read state data from the file
    fread(&saveSize, sizeof(saveSize), 1, file);
    if (saveSize > 0) fread(save, 1, saveSize, file);
    fread(&cmdMode, sizeof(cmdMode), 1, file);
    fread(encTable, 4, sizeof(encTable) / 4, file);
    fread(encCode, 4, sizeof(encCode) / 4, file);
    fread(romAddrReal, 4, sizeof(romAddrReal) / 4, file);
    fread(romAddrVirt, 4, sizeof(romAddrVirt) / 4, file);
    fread(blockSize, 2, sizeof(blockSize) / 2, file);
    fread(readCount, 2, sizeof(readCount) / 2, file);
    fread(wordCycles, 4, sizeof(wordCycles) / 4, file);
    fread(encrypted, sizeof(bool), sizeof(encrypted) / sizeof(bool), file);
    fread(auxCommand, 1, sizeof(auxCommand), file);
    fread(auxAddress, 4, sizeof(auxAddress) / 4, file);
    fread(auxWriteCount, 4, sizeof(auxWriteCount) / 4, file);
    fread(auxSpiCnt, 2, sizeof(auxSpiCnt) / 2, file);
    fread(auxSpiData, 1, sizeof(auxSpiData), file);
    fread(romCtrl, 4, sizeof(romCtrl) / 4, file);
    fread(romCmdOut, 8, sizeof(romCmdOut) / 8, file);

    // Don't overwrite the save file right away; wait until it's modified
    saveDirty = false;
}

bool CartridgeNds::loadRom()
{
    // Set the valid NDS save sizes
    if (saveSizes.empty())
    {
        saveSizes.push_back(0x000000); // None
        saveSizes.push_back(0x000200); // EEPROM 0.5KB
        saveSizes.push_back(0x002000); // EEPROM 8KB
        saveSizes.push_back(0x008000); // FRAM 32KB
        saveSizes.push_back(0x010000); // EEPROM 64KB
        saveSizes.push_back(0x020000); // EEPROM 128KB
        saveSizes.push_back(0x040000); // FLASH 256KB
        saveSizes.push_back(0x080000); // FLASH 512KB
        saveSizes.push_back(0x100000); // FLASH 1024KB
        saveSizes.push_back(0x800000); // FLASH 8192KB
    }

    // Try to load the ROM into RAM if enabled; otherwise fall back to file-based loading
    if (!Cartridge::loadRom())
    {
        return false;
    }
    else if (Settings::romInRam)
    {
        try
        {
            loadRomSection(0, romSize);
            fclose(romFile);
            romFile = nullptr;
        }
        catch (std::bad_alloc &ba)
        {
            loadRomSection(0, 0x5000);
        }
    }
    else
    {
        loadRomSection(0, 0x5000);
    }

    // Get logo data from the ROM header
    core->memory.copyBiosLogo(&rom[0xC0]);

    // Calculate the mask for ROM mirroring
    for (romMask = 1; romMask < romSize; romMask <<= 1);
    romMask -= 1;

    // Save the ROM code, which is mainly used for encryption
    romCode = U8TO32(rom, 0x0C);

    // Check if the ROM is encrypted
    if (romSize >= 0x8000) // ROM has secure area
    {
        // Decrypt the 'encryObj' string
        uint64_t data = U8TO64(rom, 0x4000);
        initKeycode(2);
        data = decrypt64(data);
        initKeycode(3);
        data = decrypt64(data);

        // If decryption was successful, the ROM is encrypted
        if (data == 0x6A624F7972636E65) // encryObj
        {
            LOG_INFO("Detected an encrypted ROM!\n");
            romEncrypted = true;
        }
    }
    return true;
}

void CartridgeNds::directBoot()
{
    // Load the ROM header from file if needed
    if (romFile)
        loadRomSection(0, 0x170);

    // Extract some information about the initial ARM9 code from the header
    uint32_t offset9 = U8TO32(rom, 0x20);
    core->interpreter[0].entryAddr = U8TO32(rom, 0x24);
    uint32_t ramAddr9 = U8TO32(rom, 0x28);
    uint32_t size9 = U8TO32(rom, 0x2C);
    LOG_INFO("ARM9 code ROM offset: 0x%X\n", offset9);
    LOG_INFO("ARM9 code entry address: 0x%X\n", core->interpreter[0].entryAddr);
    LOG_INFO("ARM9 RAM address: 0x%X\n", ramAddr9);
    LOG_INFO("ARM9 code size: 0x%X\n", size9);

    // Extract some information about the initial ARM7 code from the header
    uint32_t offset7 = U8TO32(rom, 0x30);
    core->interpreter[1].entryAddr = U8TO32(rom, 0x34);
    uint32_t ramAddr7 = U8TO32(rom, 0x38);
    uint32_t size7 = U8TO32(rom, 0x3C);
    LOG_INFO("ARM7 code ROM offset: 0x%X\n", offset7);
    LOG_INFO("ARM7 code entry address: 0x%X\n", core->interpreter[1].entryAddr);
    LOG_INFO("ARM7 RAM address: 0x%X\n", ramAddr7);
    LOG_INFO("ARM7 code size: 0x%X\n", size7);

    // Load the ROM header into memory
    for (uint32_t i = 0; i < 0x170; i += 4)
        core->memory.write<uint32_t>(0, 0x27FFE00 + i, U8TO32(rom, i));

    // Load the initial ARM9 code from file if needed
    uint32_t offset;
    if (romFile)
    {
        loadRomSection(offset9, size9);
        offset = 0;
    }
    else
    {
        offset = offset9;
    }

    // Load the initial ARM9 code into memory
    for (uint32_t i = 0; i < size9; i += 4)
    {
        if (romEncrypted && offset9 + i >= 0x4000 && offset9 + i < 0x4800)
        {
            if (offset9 + i < 0x4008)
            {
                // Overwrite the 'encryObj' string
                core->memory.write<uint32_t>(0, ramAddr9 + i, 0xE7FFDEFF);
            }
            else
            {
                // Decrypt the first 2KB of the secure area
                initKeycode(3);
                uint64_t data = decrypt64(U8TO64(rom, (offset + i) & ~7));
                core->memory.write<uint32_t>(0, ramAddr9 + i, data >> (((offset + i) & 4) ? 32 : 0));
            }
        }
        else
        {
            core->memory.write<uint32_t>(0, ramAddr9 + i, U8TO32(rom, offset + i));
        }
    }

    // Load the initial ARM7 code from file if needed
    if (romFile)
    {
        loadRomSection(offset7, size7);
        offset = 0;
    }
    else
    {
        offset = offset7;
    }

    // Load the initial ARM7 code into memory
    for (uint32_t i = 0; i < size7; i += 4)
    {
        if (romEncrypted && offset7 + i >= 0x4000 && offset7 + i < 0x4800)
        {
            if (offset7 + i < 0x4008)
            {
                // Overwrite the 'encryObj' string
                core->memory.write<uint32_t>(1, ramAddr7 + i, 0xE7FFDEFF);
            }
            else
            {
                // Decrypt the first 2KB of the secure area
                initKeycode(3);
                uint64_t data = decrypt64(U8TO64(rom, (offset + i) & ~7));
                core->memory.write<uint32_t>(1, ramAddr7 + i, data >> (((offset + i) & 4) ? 32 : 0));
            }
        }
        else
        {
            core->memory.write<uint32_t>(1, ramAddr7 + i, U8TO32(rom, offset + i));
        }
    }
}

uint64_t CartridgeNds::encrypt64(uint64_t value)
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
        x = encTable[0x212 + ((z >> 8) & 0xFF)] ^ x;
        x = encTable[0x312 + ((z >> 0) & 0xFF)] + x;
        x ^= y;
        y = z;
    }

    return ((uint64_t)(y ^ encTable[0x11]) << 32) | (x ^ encTable[0x10]);
}

uint64_t CartridgeNds::decrypt64(uint64_t value)
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
        x = encTable[0x212 + ((z >> 8) & 0xFF)] ^ x;
        x = encTable[0x312 + ((z >> 0) & 0xFF)] + x;
        x ^= y;
        y = z;
    }

    return ((uint64_t)(y ^ encTable[0x00]) << 32) | (x ^ encTable[0x01]);
}

void CartridgeNds::initKeycode(int level)
{
    // Initialize the Blowfish encryption table
    // This is a translation of the pseudocode from GBATEK to C++

    for (int i = 0; i < 0x412; i++)
        encTable[i] = core->memory.read<uint32_t>(1, 0x30 + i * 4);

    encCode[0] = romCode;
    encCode[1] = romCode / 2;
    encCode[2] = romCode * 2;

    if (level >= 1) applyKeycode();
    if (level >= 2) applyKeycode();

    encCode[1] *= 2;
    encCode[2] /= 2;

    if (level >= 3) applyKeycode();
}

void CartridgeNds::applyKeycode()
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

void CartridgeNds::wordReady(bool cpu)
{
    // Indicate that a word is ready
    romCtrl[cpu] |= BIT(23);

    // Trigger DS cartridge DMA transfers
    core->dma[cpu].trigger((cpu == 0) ? 5 : 2);
}

void CartridgeNds::writeAuxSpiCnt(bool cpu, uint16_t mask, uint16_t value)
{
    // Write to one of the AUXSPICNT registers
    mask &= 0xE043;
    auxSpiCnt[cpu] = (auxSpiCnt[cpu] & ~mask) | (value & mask);
}

void CartridgeNds::writeRomCmdOutL(bool cpu, uint32_t mask, uint32_t value)
{
    // Write to one of the ROMCMDOUT registers (low)
    romCmdOut[cpu] = (romCmdOut[cpu] & ~((uint64_t)mask)) | (value & mask);
}

void CartridgeNds::writeRomCmdOutH(bool cpu, uint32_t mask, uint32_t value)
{
    // Write to one of the ROMCMDOUT registers (high)
    romCmdOut[cpu] = (romCmdOut[cpu] & ~((uint64_t)mask << 32)) | ((uint64_t)(value & mask) << 32);
}

void CartridgeNds::writeAuxSpiData(bool cpu, uint8_t value)
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
        // Incredibly naive save type detection, based on commands that might be sent
        if (saveSize == -1)
        {
            switch (auxCommand[cpu])
            {
                case 0x0B: // EEPROM 0.5KB: Read from upper memory
                    LOG_INFO("Detected EEPROM 0.5KB save type\n");
                    resizeSave(0x200, false);
                    break;

                case 0x02: // EEPROM 64KB: Write to memory
                    LOG_INFO("Detected EEPROM 64KB save type\n");
                    resizeSave(0x10000, false);
                    break;

                case 0x0A: // FLASH 512KB: Page write
                    LOG_INFO("Detected FLASH 512KB save type\n");
                    resizeSave(0x80000, false);
                    break;

                default:
                    // Deselect chip
                    if (!(auxSpiCnt[cpu] & BIT(6)))
                        auxWriteCount[cpu] = 0;
                    return;
            }
        }

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
                            auxSpiData[cpu] = (auxAddress[cpu] < 0x200) ? save[auxAddress[cpu]] : 0;
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
                            if (auxAddress[cpu] < 0x200)
                            {
                                mutex.lock();
                                save[auxAddress[cpu]] = value;
                                saveDirty = true;
                                mutex.unlock();
                            }

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
                            if (auxAddress[cpu] < 0x200)
                            {
                                mutex.lock();
                                save[auxAddress[cpu]] = value;
                                saveDirty = true;
                                mutex.unlock();
                            }

                            auxAddress[cpu]++;
                            auxSpiData[cpu] = 0;
                        }
                        break;
                    }

                    default:
                    {
                        LOG_CRIT("Write to AUX SPI with unknown EEPROM 0.5KB command: 0x%X\n", auxCommand[cpu]);
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
                            if (auxAddress[cpu] < saveSize)
                            {
                                mutex.lock();
                                save[auxAddress[cpu]] = value;
                                saveDirty = true;
                                mutex.unlock();
                            }

                            auxAddress[cpu]++;
                            auxSpiData[cpu] = 0;
                        }
                        break;
                    }

                    default:
                    {
                        LOG_CRIT("Write to AUX SPI with unknown EEPROM/FRAM command: 0x%X\n", auxCommand[cpu]);
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
                            if (auxAddress[cpu] < saveSize)
                            {
                                mutex.lock();
                                save[auxAddress[cpu]] = value;
                                saveDirty = true;
                                mutex.unlock();
                            }

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
                        auxSpiData[cpu] = ((romCode & 0xFF) == 'I') ? 0xAA : 0;
                        break;
                    }

                    default:
                    {
                        LOG_CRIT("Write to AUX SPI with unknown FLASH command: 0x%X\n", auxCommand[cpu]);
                        auxSpiData[cpu] = 0;
                        break;
                    }
                }
                break;
            }

            default:
            {
                LOG_CRIT("Write to AUX SPI with unknown save size: 0x%X\n", saveSize);
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

void CartridgeNds::writeRomCtrl(bool cpu, uint32_t mask, uint32_t value)
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

    // Set the time to transfer a word: 4 cycles at either 4.2MHz or 6.7MHz
    wordCycles[cpu] = (romCtrl[cpu] & BIT(27)) ? (4 * 8) : (4 * 5);

    if (!transfer) return;

    // Determine the size of the block to transfer
    uint8_t size = (romCtrl[cpu] & 0x07000000) >> 24;
    switch (size)
    {
        case 0: blockSize[cpu] = 0; break;
        case 7: blockSize[cpu] = 4; break;
        default: blockSize[cpu] = 0x100 << size; break;
    }

    // Reverse the byte order of the ROM command
    uint64_t command = 0;
    for (int i = 0; i < 8; i++)
        command |= ((romCmdOut[cpu] >> (i * 8)) & 0xFF) << ((7 - i) * 8);

    // Decrypt the ROM command if encryption is enabled
    if (encrypted[cpu])
    {
        initKeycode(2);
        command = decrypt64(command);
    }

    cmdMode = CMD_NONE;

    // Interpret the ROM command
    if (rom)
    {
        if (command == 0x0000000000000000) // Get header
        {
            cmdMode = CMD_HEADER;

            // Load the header from file if needed
            if (romFile)
                loadRomSection(0, blockSize[cpu]);
        }
        else if (command == 0x9000000000000000 || (command >> 60) == 0x1 || command == 0xB800000000000000) // Get chip ID
        {
            cmdMode = CMD_CHIP;
        }
        else if ((command >> 56) == 0x3C) // Activate KEY1 encryption mode
        {
            // Initialize KEY1 encryption
            encrypted[cpu] = true;
        }
        else if ((command >> 60) == 0x2) // Get secure area
        {
            cmdMode = CMD_SECURE;
            romAddrReal[cpu] = ((command & 0x0FFFF00000000000) >> 44) * 0x1000;

            // Load the secure area block from file if needed
            if (romFile)
            {
                loadRomSection(romAddrReal[cpu], blockSize[cpu]);
                romAddrVirt[cpu] = 0;
            }
            else
            {
                romAddrVirt[cpu] = romAddrReal[cpu];
            }
        }
        else if ((command >> 60) == 0xA) // Enter main data mode
        {
            // Disable KEY1 encryption
            // On hardware, this is where KEY2 encryption would start
            encrypted[cpu] = false;
        }
        else if ((command >> 56) == 0xB7) // Get data
        {
            cmdMode = CMD_DATA;
            romAddrReal[cpu] = (command >> 24) & romMask;

            // Load the ROM data from file if needed
            if (romFile)
            {
                if (romAddrReal[cpu] < 0x8000)
                {
                    // Workaround for address redirection when loading from file
                    loadRomSection(0, 0x8200 + blockSize[cpu]);
                    romAddrVirt[cpu] = romAddrReal[cpu];
                }
                else
                {
                    loadRomSection(romAddrReal[cpu], blockSize[cpu]);
                    romAddrVirt[cpu] = 0;
                }
            }
            else
            {
                romAddrVirt[cpu] = romAddrReal[cpu];
            }
        }
        else if (command != 0x9F00000000000000) // Unknown (not dummy)
        {
            LOG_CRIT("ROM transfer with unknown command: 0x%llX\n", command);
        }
    }

    if (blockSize[cpu] == 0)
    {
        // End the transfer right away if the block size is 0
        romCtrl[cpu] &= ~BIT(23); // Word not ready
        romCtrl[cpu] &= ~BIT(31); // Block ready

        // Trigger a block ready IRQ if enabled
        if (auxSpiCnt[cpu] & BIT(14))
            core->interpreter[cpu].sendInterrupt(19);
    }
    else
    {
        // Schedule the first word to be ready
        core->schedule(SchedTask(CART9_WORD_READY + cpu), wordCycles[cpu]);
        readCount[cpu] = 0;
    }
}

uint32_t CartridgeNds::readRomDataIn(bool cpu)
{
    // Don't transfer if the word ready bit isn't set
    if (!(romCtrl[cpu] & BIT(23)))
        return 0;

    // Mark the next word as not ready
    romCtrl[cpu] &= ~BIT(23);

    // Increment the read counter
    if ((readCount[cpu] += 4) == blockSize[cpu])
    {
        // End the transfer when the block size has been reached
        romCtrl[cpu] &= ~BIT(31); // Block ready

        // Trigger a block ready IRQ if enabled
        if (auxSpiCnt[cpu] & BIT(14))
            core->interpreter[cpu].sendInterrupt(19);
    }
    else
    {
        // Schedule the next word to be ready
        core->schedule(SchedTask(CART9_WORD_READY + cpu), wordCycles[cpu]);
    }

    // Return a value from the cart depending on the current command
    switch (cmdMode)
    {
        case CMD_HEADER:
        {
            // Read the ROM header, repeated every 0x1000 bytes
            return U8TO32(rom, (readCount[cpu] - 4) & 0xFFF);
        }

        case CMD_CHIP:
        {
            // Read the chip ID, repeated every 4 bytes
            // ROM dumps don't provide a chip ID, so use a fake one
            return 0x00001FC2;
        }

        case CMD_SECURE:
        {
            // Encrypt the first 2KB of the secure area
            if (!romEncrypted && romAddrReal[cpu] == 0x4000 && readCount[cpu] <= 0x800)
            {
                // Supply the 'encryObj' string for the first 8 bytes (overwritten during decryption)
                uint64_t data = (readCount[cpu] <= 8) ? 0x6A624F7972636E65 :
                    U8TO64(rom, (romAddrVirt[cpu] + readCount[cpu] - 4) & ~7);

                // Encrypt the data
                initKeycode(3);
                data = encrypt64(data);

                // Double-encrypt the 'encryObj' string
                if (readCount[cpu] <= 8)
                {
                    initKeycode(2);
                    data = encrypt64(data);
                }

                return data >> (((romAddrReal[cpu] + readCount[cpu]) & 4) ? 0 : 32);
            }

            // Read data from the selected secure area block
            return U8TO32(rom, romAddrVirt[cpu] + readCount[cpu] - 4);
        }

        case CMD_DATA:
        {
            // Read ROM data from the given address
            // This command can't read the first 32KB of a ROM, so it redirects the address
            // Some games verify that the first 32KB are unreadable as an anti-piracy measure
            uint32_t address = romAddrVirt[cpu] + readCount[cpu] - 4;
            if (romAddrReal[cpu] + readCount[cpu] <= 0x8000) address = 0x8000 + (address & 0x1FF);
            if (address < romSize) return U8TO32(rom, address);
        }
    }

    // Default to endless 0xFFs if there's no actual data to read
    return 0xFFFFFFFF;
}

void CartridgeGba::saveState(FILE *file)
{
    // Write state data to the file
    fwrite(&saveSize, sizeof(saveSize), 1, file);
    if (saveSize > 0) fwrite(save, 1, saveSize, file);
    fwrite(&eepromCount, sizeof(eepromCount), 1, file);
    fwrite(&eepromCmd, sizeof(eepromCmd), 1, file);
    fwrite(&eepromData, sizeof(eepromData), 1, file);
    fwrite(&eepromDone, sizeof(eepromDone), 1, file);
    fwrite(&flashCmd, sizeof(flashCmd), 1, file);
    fwrite(&bankSwap, sizeof(bankSwap), 1, file);
    fwrite(&flashErase, sizeof(flashErase), 1, file);
}

void CartridgeGba::loadState(FILE *file)
{
    // Read state data from the file
    fread(&saveSize, sizeof(saveSize), 1, file);
    if (saveSize > 0) fread(save, 1, saveSize, file);
    fread(&eepromCount, sizeof(eepromCount), 1, file);
    fread(&eepromCmd, sizeof(eepromCmd), 1, file);
    fread(&eepromData, sizeof(eepromData), 1, file);
    fread(&eepromDone, sizeof(eepromDone), 1, file);
    fread(&flashCmd, sizeof(flashCmd), 1, file);
    fread(&bankSwap, sizeof(bankSwap), 1, file);
    fread(&flashErase, sizeof(flashErase), 1, file);

    // Don't overwrite the save file right away; wait until it's modified
    saveDirty = false;
}

bool CartridgeGba::findString(std::string string)
{
    // Scan a GBA ROM for a string and report if it was found
    for (int i = 0; i < romSize; i += 4)
    {
        bool found = true;

        for (size_t j = 0; j < string.length(); j++)
        {
            if (i + j >= romSize || rom[i + j] != string[j])
            {
                found = false;
                break;
            }
        }

        if (found)
            return true;
    }

    return false;
}

bool CartridgeGba::loadRom()
{
    // Set the valid GBA save sizes
    if (saveSizes.empty())
    {
        saveSizes.push_back(0x00000); // None
        saveSizes.push_back(0x00200); // EEPROM 0.5KB
        saveSizes.push_back(0x02000); // EEPROM 8KB
        saveSizes.push_back(0x08000); // SRAM 32KB
        saveSizes.push_back(0x10000); // FLASH 64KB
        saveSizes.push_back(0x20000); // FLASH 128KB
    }

    // Load the ROM into memory
    if (!Cartridge::loadRom()) return false;
    loadRomSection(0, romSize);
    fclose(romFile);
    romFile = nullptr;

    // Calculate the mask for ROM mirroring
    if (romSize > 0xAC && rom[0xAC] == 'F') // NES classic
    {
        // NES classic ROMs are mirrored based on their size
        for (romMask = 1; romMask < romSize; romMask <<= 1);
        romMask--;
    }
    else
    {
        // Most ROMs are only mirrored each 32MB wait state area
        romMask = 0x1FFFFFF;
    }

    // Detect if the cart has an RTC by searching for the relevant string
    if (findString("SIIRTC_V"))
        core->rtc.enableGpRtc();

    // Update the memory maps at the GBA ROM locations
    core->memory.updateMap9(0x08000000, 0x0A000000);
    core->memory.updateMap7(0x08000000, 0x0D000000);

    // If the save size is unknown, try to detect it
    if (saveSize == -1)
    {
        const std::string saveStrs[] = { "EEPROM_V", "SRAM_V", "FLASH_V", "FLASH512_V", "FLASH1M_V" };

        // Unlike the DS, a GBA cart's save type can be detected by searching for strings in the ROM
        // Search the ROM for a save string so a new save of that type can be created
        for (int i = 0; i < 5; i++)
        {
            if (!findString(saveStrs[i]))
                continue;

            // Create a new GBA save of the detected type
            switch (i)
            {
                case 0: // EEPROM
                    // EEPROM can be either 0.5KB or 8KB, so it must be guessed based on how the game uses it
                    return true;

                case 1: // SRAM 32KB
                    LOG_INFO("Detected SRAM 32KB save type\n");
                    resizeSave(0x8000, false);
                    return true;

                case 2: case 3: // FLASH 64KB
                    LOG_INFO("Detected FLASH 64KB save type\n");
                    resizeSave(0x10000, false);
                    return true;

                case 4: // FLASH 128KB
                    LOG_INFO("Detected FLASH 128KB save type\n");
                    resizeSave(0x20000, false);
                    return true;
            }
        }
    }
    return true;
}

uint8_t CartridgeGba::eepromRead()
{
    if (saveSize == -1)
    {
        // Detect the save size based on how many command bits were sent before reading
        if (eepromCount == 9)
        {
            LOG_INFO("Detected EEPROM 0.5KB save type\n");
            resizeSave(0x200, false);
        }
        else
        {
            LOG_INFO("Detected EEPROM 8KB save type\n");
            resizeSave(0x2000, false);
        }
    }

    // EEPROM 0.5KB uses 8-bit commands, and EEPROM 8KB uses 16-bit commands
    uint8_t length = (saveSize == 0x200) ? 8 : 16;

    if (((eepromCmd & 0xC000) >> 14) == 0x3 && eepromCount >= length + 1) // Read
    {
        if (++eepromCount >= length + 6)
        {
            // Read the data bits, MSB first
            int bit = 63 - (eepromCount - (length + 6));
            uint16_t addr = (saveSize == 0x200) ? ((eepromCmd & 0x3F00) >> 8) : (eepromCmd & 0x03FF);
            uint8_t value = (save[addr * 8 + bit / 8] & BIT(bit % 8)) >> (bit % 8);

            // Reset the transfer at the end
            if (eepromCount >= length + 69)
            {
                eepromCount = 0;
                eepromCmd = 0;
                eepromData = 0;
            }

            return value;
        }
    }
    else if (eepromDone)
    {
        // Signal that a write has finished
        return 1;
    }

    return 0;
}

void CartridgeGba::eepromWrite(uint8_t value)
{
    eepromDone = false;

    // EEPROM 0.5KB uses 8-bit commands, and EEPROM 8KB uses 16-bit commands
    uint8_t length = (saveSize == 0x200) ? 8 : 16;

    if (eepromCount < length)
    {
        // Get the command bits
        eepromCmd |= (value & BIT(0)) << (16 - ++eepromCount);
    }
    else if (((eepromCmd & 0xC000) >> 14) == 0x3) // Read
    {
        // Accept the last bit to finish the read command
        if (eepromCount < length + 1)
            eepromCount++;
    }
    else if (((eepromCmd & 0xC000) >> 14) == 0x2) // Write
    {
        // Get the data bits, MSB first
        if (++eepromCount <= length + 64)
            eepromData |= (uint64_t)(value & BIT(0)) << (length + 64 - eepromCount);

        if (eepromCount >= length + 65)
        {
            // Games will probably read first, so the save size can be detected from that
            // If something decides to write first, it becomes a lot harder to detect the size
            // In this case, just assume EEPROM 8KB and create an empty save
            if (saveSize == -1)
            {
                LOG_INFO("Detected EEPROM 8KB save type\n");
                resizeSave(0x2000, false);
            }

            // Write the data after all the bits have been received
            mutex.lock();
            uint16_t addr = (saveSize == 0x200) ? ((eepromCmd & 0x3F00) >> 8) : (eepromCmd & 0x03FF);
            for (unsigned int i = 0; i < 8; i++)
                save[addr * 8 + i] = eepromData >> (i * 8);
            saveDirty = true;
            mutex.unlock();

            // Reset the transfer
            eepromCount = 0;
            eepromCmd = 0;
            eepromData = 0;
            eepromDone = true;
        }
    }
}

uint8_t CartridgeGba::sramRead(uint32_t address)
{
    if (saveSize == 0x8000 && address < 0xE008000) // SRAM
    {
        // Read a single byte because the data bus is only 8 bits
        return save[address - 0xE000000];
    }
    else if ((saveSize == 0x10000 || saveSize == 0x20000) && address < 0xE010000) // FLASH
    {
        // Run a FLASH command
        if (flashCmd == 0x90 && address == 0xE000000)
        {
            // Read the chip manufacturer ID
            return 0xC2;
        }
        else if (flashCmd == 0x90 && address == 0xE000001)
        {
            // Read the chip device ID
            return (saveSize == 0x10000) ? 0x1C : 0x09;
        }
        else
        {
            // Read a single byte
            if (bankSwap) address += 0x10000;
            return save[address - 0xE000000];
        }
    }

    return 0xFF;
}

void CartridgeGba::sramWrite(uint32_t address, uint8_t value)
{
    if (saveSize == 0x8000 && address < 0xE008000) // SRAM
    {
        // Write a single byte because the data bus is only 8 bits
        mutex.lock();
        save[address - 0xE000000] = value;
        saveDirty = true;
        mutex.unlock();
    }
    else if ((saveSize == 0x10000 || saveSize == 0x20000) && address < 0xE010000) // FLASH
    {
        // Run a FLASH command
        if (flashCmd == 0xA0)
        {
            // Write a single byte
            if (bankSwap) address += 0x10000;
            mutex.lock();
            save[address - 0xE000000] = value;
            saveDirty = true;
            mutex.unlock();
            flashCmd = 0xF0;
        }
        else if (flashErase && (address & ~0x000F000) == 0xE000000 && (value & 0xFF) == 0x30)
        {
            // Erase a sector
            if (bankSwap) address += 0x10000;
            mutex.lock();
            memset(&save[address - 0xE000000], 0xFF, 0x1000 * sizeof(uint8_t));
            saveDirty = true;
            mutex.unlock();
            flashErase = false;
        }
        else if (saveSize == 0x20000 && flashCmd == 0xB0 && address == 0xE000000)
        {
            // Swap the ROM banks on 128KB carts
            bankSwap = value;
            flashCmd = 0xF0;
        }
        else if (address == 0xE005555)
        {
            // Write the FLASH command byte
            flashCmd = value;

            // Handle erase commands
            if (flashCmd == 0x80)
            {
                flashErase = true;
            }
            else if (flashCmd != 0xAA)
            {
                flashErase = false;
            }
            else if (flashErase && flashCmd == 0x10)
            {
                mutex.lock();
                memset(save, 0xFF, saveSize * sizeof(uint8_t));
                saveDirty = true;
                mutex.unlock();
            }
        }
    }
}
