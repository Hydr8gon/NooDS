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

#include "action_replay.h"
#include "core.h"

void ActionReplay::setPath(std::string path)
{
    // Set the cheat path
    this->path = path;
}

void ActionReplay::setFd(int fd)
{
    // Set the cheat descriptor
    this->fd = fd;
}

FILE *ActionReplay::openFile(const char *mode)
{
    // Open the cheat file if one is set
    if (fd != -1)
        return fdopen(dup(fd), mode);
    else if (path != "")
        return fopen(path.c_str(), mode);
    return nullptr;
}

bool ActionReplay::loadCheats()
{
    // Attempt to open the cheat file
    FILE *file = openFile("r");
    if (!file) return false;
    char data[512];
    mutex.lock();

    // Reload cheats from the file
    cheats.clear();
    while (fgets(data, 512, file) != nullptr)
    {
        // Create a new cheat when one is found
        if (data[0] != '[') continue;
        cheats.push_back(ARCheat());
        ARCheat &cheat = cheats[cheats.size() - 1];

        // Parse the cheat name and enabled state from the file
        cheat.name = &data[1];
        cheat.enabled = (cheat.name[cheat.name.size() - 2] == '+');
        cheat.name = cheat.name.substr(0, cheat.name.size() - 3);
        LOG_INFO("Loaded cheat: %s (%s)\n", cheat.name.c_str(), cheat.enabled ? "enabled" : "disabled");

        // Load the cheat code up until an empty line
        while (fgets(data, 512, file) != nullptr && data[0] != '\n')
        {
            cheat.code.push_back(strtoll(&data[0], nullptr, 16));
            cheat.code.push_back(strtoll(&data[8], nullptr, 16));
        }
    }

    // Close the file after reading it
    mutex.unlock();
    fclose(file);
    return true;
}

bool ActionReplay::saveCheats()
{
    // Attempt to open the cheat file
    FILE *file = openFile("w");
    if (!file) return false;
    mutex.lock();

    // Write cheats back to the file
    for (uint32_t i = 0; i < cheats.size(); i++)
    {
        fprintf(file, "[%s]%c\n", cheats[i].name.c_str(), cheats[i].enabled ? '+' : '-');
        for (uint32_t j = 0; j < cheats[i].code.size(); j += 2)
            fprintf(file, "%08X %08X\n", cheats[i].code[j], cheats[i].code[j + 1]);
        fprintf(file, "\n");
    }

    // Close the file after writing it
    mutex.unlock();
    fclose(file);
    return true;
}

void ActionReplay::applyCheats()
{
    // Execute the code of enabled cheats
    mutex.lock();
    for (uint32_t i = 0; i < cheats.size(); i++)
    {
        // Define registers for executing a cheat
        if (!cheats[i].enabled) continue;
        uint32_t offset = 0;
        uint32_t dataReg = 0;
        uint32_t counter = 0;
        uint32_t loopCount = 0;
        uint32_t loopAddress = 0;
        bool condFlag = false;

        // Loop through lines of a cheat's code
        for (uint32_t address = 0; address < cheats[i].code.size(); address += 2)
        {
            // Check the condition flag
            uint32_t *line = &cheats[i].code[address];
            if (condFlag)
            {
                // Handle adjustments that happen regardless of condition
                uint8_t op = (line[0] >> 24);
                if ((op >> 4) == 0xE) // Parameter copy
                    address += ((line[1] + 0x7) & ~0x7) >> 2;
                else if (op == 0xC5) // If counter
                    counter++;

                // Skip non-control opcodes if the flag is set
                if (op != 0xD0 && op != 0xD1 && op != 0xD2)
                    continue;
            }

            // Interpret a line of the code
            switch (line[0] >> 28)
            {
                case 0x0: // Write word
                    // Write a word to memory
                    core->memory.write<uint32_t>(1, (line[0] & 0xFFFFFFF) + offset, line[1]);
                    continue;

                case 0x1: // Write half
                    // Write a half-word to memory
                    core->memory.write<uint16_t>(1, (line[0] & 0xFFFFFFF) + offset, line[1]);
                    continue;

                case 0x2: // Write byte
                    // Write a byte to memory
                    core->memory.write<uint8_t>(1, (line[0] & 0xFFFFFFF) + offset, line[1]);
                    continue;

                case 0x3: // If greater than word
                {
                    // Set the condition flag if a word isn't greater than a memory value
                    uint32_t addr = (line[0] & 0xFFFFFFF) ? (line[0] & 0xFFFFFFF) : offset;
                    condFlag = (line[1] <= core->memory.read<uint32_t>(1, addr));
                    continue;
                }

                case 0x4: // If less than word
                {
                    // Set the condition flag if a word isn't less than a memory value
                    uint32_t addr = (line[0] & 0xFFFFFFF) ? (line[0] & 0xFFFFFFF) : offset;
                    condFlag = (line[1] >= core->memory.read<uint32_t>(1, addr));
                    continue;
                }

                case 0x5: // If equal to word
                {
                    // Set the condition flag if a word isn't equal to a memory value
                    uint32_t addr = (line[0] & 0xFFFFFFF) ? (line[0] & 0xFFFFFFF) : offset;
                    condFlag = (line[1] != core->memory.read<uint32_t>(1, addr));
                    continue;
                }

                case 0x6: // If not equal to word
                {
                    // Set the condition flag if a word is equal to a memory value
                    uint32_t addr = (line[0] & 0xFFFFFFF) ? (line[0] & 0xFFFFFFF) : offset;
                    condFlag = (line[1] == core->memory.read<uint32_t>(1, addr));
                    continue;
                }

                case 0x7: // If greater than half
                {
                    // Set the condition flag if a half-word isn't greater than a masked memory value
                    uint32_t addr = (line[0] & 0xFFFFFFF) ? (line[0] & 0xFFFFFFF) : offset;
                    condFlag = (line[1] & 0xFFFF) <= (core->memory.read<uint16_t>(1, addr) & ~(line[1] >> 16));
                    continue;
                }

                case 0x8: // If less than half
                {
                    // Set the condition flag if a half-word isn't less than a masked memory value
                    uint32_t addr = (line[0] & 0xFFFFFFF) ? (line[0] & 0xFFFFFFF) : offset;
                    condFlag = (line[1] & 0xFFFF) >= (core->memory.read<uint16_t>(1, addr) & ~(line[1] >> 16));
                    continue;
                }

                case 0x9: // If equal to half
                {
                    // Set the condition flag if a half-word isn't equal to a masked memory value
                    uint32_t addr = (line[0] & 0xFFFFFFF) ? (line[0] & 0xFFFFFFF) : offset;
                    condFlag = (line[1] & 0xFFFF) != (core->memory.read<uint16_t>(1, addr) & ~(line[1] >> 16));
                    continue;
                }

                case 0xA: // If not equal to half
                {
                    // Set the condition flag if a half-word is equal to a masked memory value
                    uint32_t addr = (line[0] & 0xFFFFFFF) ? (line[0] & 0xFFFFFFF) : offset;
                    condFlag = (line[1] & 0xFFFF) == (core->memory.read<uint16_t>(1, addr) & ~(line[1] >> 16));
                    continue;
                }

                case 0xB: // Load offset
                    // Set the offset to a word from memory
                    offset = core->memory.read<uint32_t>(1, (line[0] & 0xFFFFFFF) + offset);
                    continue;

                case 0xC:
                    switch (line[0] >> 24)
                    {
                        case 0xC0: // For loop
                            // Set the loop count and address to loop to
                            loopCount = line[1];
                            loopAddress = address;
                            continue;

                        case 0xC5: // If counter
                            // Set the condition flag if the masked counter isn't equal to a half-word
                            condFlag = (++counter & line[1] & 0xFFFF) != (line[1] >> 16);
                            continue;

                        case 0xC6: // Write offset
                            // Write the offset value to a memory word
                            core->memory.write<uint32_t>(1, line[1], offset);
                            continue;
                    }
                    goto invalid;

                case 0xD:
                    switch (line[0] >> 24)
                    {
                        case 0xD0: // End if
                            // Clear the condition flag
                            condFlag = false;
                            continue;

                        case 0xD1: // Next loop
                            // Jump to the loop address until the loop count runs out
                            if (loopCount)
                            {
                                loopCount--;
                                address = loopAddress;
                                continue;
                            }
                            condFlag = false;
                            continue;

                        case 0xD2: // Next loop and flush
                            // Jump to the loop address and reset registers after looping
                            if (loopCount)
                            {
                                loopCount--;
                                address = loopAddress;
                                continue;
                            }
                            offset = 0;
                            dataReg = 0;
                            condFlag = false;
                            continue;

                        case 0xD3: // Set offset
                            // Set the offset to a word
                            offset = line[1];
                            continue;

                        case 0xD4: // Add data
                            // Add a word to the data register
                            dataReg += line[1];
                            continue;

                        case 0xD5: // Set data
                            // Set the data register to a word
                            dataReg = line[1];
                            continue;

                        case 0xD6: // Write data word
                            // Write the data register to a memory word and increment the offset
                            core->memory.write<uint32_t>(1, line[1] + offset, dataReg);
                            offset += 4;
                            continue;

                        case 0xD7: // Write data half
                            // Write the data register to a memory half-word and increment the offset
                            core->memory.write<uint16_t>(1, line[1] + offset, dataReg);
                            offset += 2;
                            continue;

                        case 0xD8: // Write data byte
                            // Write the data register to a memory byte and increment the offset
                            core->memory.write<uint8_t>(1, line[1] + offset, dataReg);
                            offset += 1;
                            continue;

                        case 0xD9: // Read data word
                            // Set the data register to a word from memory
                            dataReg = core->memory.read<uint32_t>(1, line[1] + offset);
                            continue;

                        case 0xDA: // Read data half
                            // Set the data register to a half-word from memory
                            dataReg = core->memory.read<uint16_t>(1, line[1] + offset);
                            continue;

                        case 0xDB: // Read data byte
                            // Set the data register to a byte from memory
                            dataReg = core->memory.read<uint8_t>(1, line[1] + offset);
                            continue;

                        case 0xDC: // Add offset
                            // Add a word to the offset
                            offset += line[1];
                            continue;
                    }
                    goto invalid;

                case 0xE: // Parameter copy
                    // Copy parameter bytes to memory and jump to the next opcode
                    for (uint32_t j = 0; j < line[1]; j++)
                    {
                        uint8_t value = line[(j >> 2) + 2] >> ((j & 0x3) * 8);
                        core->memory.write<uint8_t>(1, (line[0] & 0xFFFFFFF) + offset + j, value);
                    }
                    address += ((line[1] + 0x7) & ~0x7) >> 2;
                    continue;

                case 0xF: // Memory copy
                    // Copy bytes from one memory location to another
                    for (uint32_t j = 0; j < line[1]; j++)
                    {
                        uint8_t value = core->memory.read<uint8_t>(1, offset + j);
                        core->memory.write<uint8_t>(1, (line[0] & 0xFFFFFFF) + j, value);
                    }
                    continue;

                default:
                invalid:
                    LOG_CRIT("Invalid AR code: %08X %08X\n", line[0], line[1]);
                    continue;
            }
        }
    }
    mutex.unlock();
}
