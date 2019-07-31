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

#include "memory_transfer.h"
#include "core.h"
#include "memory.h"

namespace memory_transfer
{

uint8_t firmware[0x40000];
uint8_t *rom;

uint8_t rtcWriteCount;
uint8_t rtcCommand;
uint8_t rtcLast;

std::queue<uint32_t> fifo9, fifo7;

uint64_t romCommand;
uint16_t romBlockSize, romReadCount;
bool romEncrypt;

uint32_t encTable[0x412];
uint32_t encCode[3];

uint32_t spiWriteCount;
uint32_t spiAddr;
uint8_t spiInstr;

void dmaTransfer(interpreter::Cpu *cpu, uint8_t channel)
{
    // Don't transfer if the channel isn't enabled
    if (!(*cpu->dmacnt[channel] & BIT(31)) || channel > 3)
        return;

    uint8_t mode = (*cpu->dmacnt[channel] & 0x38000000) >> 27;

    if (mode == 0) // Start immediately
    {
        // Get some values from the registers
        uint8_t dstAddrCnt = (*cpu->dmacnt[channel] & 0x00600000) >> 21;
        uint8_t srcAddrCnt = (*cpu->dmacnt[channel] & 0x01800000) >> 23;
        uint32_t dstAddr = *cpu->dmadad[channel];
        uint32_t srcAddr = *cpu->dmasad[channel];
        uint32_t size = (*cpu->dmacnt[channel] & 0x001FFFFF);

        if (*cpu->dmacnt[channel] & BIT(26)) // Whole word transfer
        {
            for (unsigned int i = 0; i < size; i++)
            {
                // Transfer a word
                memory::write<uint32_t>(cpu, dstAddr, memory::read<uint32_t>(cpu, srcAddr));

                // Adjust the destination address
                if (dstAddrCnt == 0 || dstAddrCnt == 3) // Increment
                    dstAddr += 4;
                else if (dstAddrCnt == 1) // Decrement
                    dstAddr -= 4;

                // Adjust the source address
                if (srcAddrCnt == 0) // Increment
                    srcAddr += 4;
                else if (srcAddrCnt == 1) // Decrement
                    srcAddr -= 4;
            }
        }
        else // Halfword transfer
        {
            for (unsigned int i = 0; i < size; i++)
            {
                // Transfer a halfword
                memory::write<uint16_t>(cpu, dstAddr, memory::read<uint16_t>(cpu, srcAddr));

                // Adjust the destination address
                if (dstAddrCnt == 0 || dstAddrCnt == 3) // Increment
                    dstAddr += 2;
                else if (dstAddrCnt == 1) // Decrement
                    dstAddr -= 2;

                // Adjust the source address
                if (srcAddrCnt == 0) // Increment
                    srcAddr += 2;
                else if (srcAddrCnt == 1) // Decrement
                    srcAddr -= 2;
            }
        }
    }
    else
    {
        printf("Unknown ARM%d DMA transfer mode: %d\n", cpu->type, mode);
    }

    // Trigger an end of transfer IRQ if enabled
    if (*cpu->dmacnt[channel] & BIT(30))
        *cpu->irf |= BIT(8 + channel);

    // Clear the enable bit to indicate transfer completion
    // Transfers shouldn't complete instantly like this, but there's no timing system yet
    *cpu->dmacnt[channel] &= ~BIT(31);
}

void rtcWrite(uint8_t *value)
{
    // I find GBATEK's RTC documentation confusing, so here's a quick summary of how it works
    //
    // Bits 2 and 6 are connected to the CS pinout
    // Bit 6 should always be set, so bit 2 being 1 or 0 causes CS to be high or low, respectively
    //
    // Bits 1 and 5 are connected to the SCK pinout
    // Bit 5 should always be set, so bit 1 being 1 or 0 causes SCK to be high or low, respectively
    //
    // Bits 0 and 4 are connected to the SIO pinout
    // Bit 4 indicates data direction; 0 is read, and 1 is write
    // Bit 0 is where data sent from the RTC is read, and where data sent to the RTC is written
    //
    // To start a transfer, switch CS from low to high
    // To end a transfer, switch CS from high to low
    //
    // To transfer a bit, set SCK to low and then high (it should be high when the transfer starts)
    // When writing a bit to the RTC, you should set bit 0 at the same time as setting SCK to low
    // When reading a bit from the RTC, you should read bit 0 after setting SCK to low (or high?)
    //
    // Start by writing the 8 bits of the command register, LSB first
    // After that, read or write the registers selected with the command register, MSB first
    // RTC registers at least are documented nicely on GBATEK

    if (*value & BIT(2)) // CS high
    {
        if ((rtcLast & BIT(1)) && !(*value & BIT(1))) // SCK high to low
        {
            if (rtcWriteCount < 8)
            {
                // Write the first 8 bits to the command register, reversing the bit order
                rtcCommand |= (*value & BIT(0)) << (7 - rtcWriteCount);
                rtcWriteCount++;
            }
            else
            {
                uint8_t regSelect = (rtcCommand & 0x0E) >> 1;

                // Transfer all following bits to or from the registers selected by the command register
                if (*value & BIT(4)) // Write
                {
                    // For now, writes aren't supported
                    printf("Unhandled write to RTC registers: %d\n", regSelect);
                }
                else // Read
                {
                    *value &= ~BIT(0);

                    switch (regSelect)
                    {
                        case 2: // Date and time
                        {
                            // The bytes here (from low to high) represent:
                            // Year, month, day, day of week, hours, minutes, seconds
                            // For now it's just a hardcoded value
                            *value |= ((0x00301100250519 >> (rtcWriteCount - 8)) & BIT(0));
                            break;
                        }

                        case 3: // Time
                        {
                            // The bytes here (from low to high) represent:
                            // Hours, minutes, seconds
                            // For now it's just a hardcoded value
                            *value |= ((0x003011 >> (rtcWriteCount - 8)) & BIT(0));
                            break;
                        }

                        default:
                        {
                            printf("Read from unknown RTC registers: %d\n", regSelect);
                            break;
                        }
                    }
                }

                rtcWriteCount++;
            }
        }
    }
    else // CS low
    {
        // Reset the transfer
        rtcWriteCount = 0;
        rtcCommand = 0;
    }

    // Save the current RTC register so it can be compared to the next write
    rtcLast = *value;
}

void fifoClear(interpreter::Cpu *cpuSend, interpreter::Cpu *cpuRecv)
{
    // Nothing needs to be done if the FIFO is already empty
    if (cpuSend->fifo->empty())
        return;

    // Empty the FIFO
    while (!cpuSend->fifo->empty())
        cpuSend->fifo->pop();
    cpuSend->ipcfiforecv = 0;

    // Set the FIFO empty bits and clear the FIFO full bits
    *cpuSend->ipcfifocnt |=  BIT(0);
    *cpuSend->ipcfifocnt &= ~BIT(1);
    *cpuRecv->ipcfifocnt |=  BIT(8);
    *cpuRecv->ipcfifocnt &= ~BIT(9);

    // Trigger a send FIFO empty IRQ if enabled
    if (*cpuSend->ipcfifocnt & BIT(2))
        *cpuSend->irf |= BIT(17);
}

void fifoSend(interpreter::Cpu *cpuSend, interpreter::Cpu *cpuRecv)
{
    if (*cpuSend->ipcfifocnt & BIT(15)) // FIFO enabled
    {
        if (cpuSend->fifo->size() < 16) // FIFO not full
        {
            // Push a word to the back of the send FIFO
            cpuSend->fifo->push(*cpuSend->ipcfifosend);

            if (cpuSend->fifo->size() == 1)
            {
                // If the send FIFO isn't empty now, clear the FIFO empty bits
                *cpuSend->ipcfifocnt &= ~BIT(0);
                *cpuRecv->ipcfifocnt &= ~BIT(8);

                // Trigger a receive FIFO not empty IRQ if enabled
                if (*cpuRecv->ipcfifocnt & BIT(10))
                    *cpuRecv->irf |= BIT(18);
            }
            else if (cpuSend->fifo->size() == 16)
            {
                // If the send FIFO is full now, set the FIFO full bits
                *cpuSend->ipcfifocnt |= BIT(1);
                *cpuRecv->ipcfifocnt |= BIT(9);
            }
        }
        else
        {
            // The FIFO can only hold 16 words, so indicate a send full error
            *cpuSend->ipcfifocnt |= BIT(14);
        }
    }
}

uint32_t fifoReceive(interpreter::Cpu *cpuSend, interpreter::Cpu *cpuRecv)
{
    if (!cpuRecv->fifo->empty())
    {
        // The front word is received even if the FIFO isn't enabled
        cpuSend->ipcfiforecv = cpuRecv->fifo->front();

        if (*cpuSend->ipcfifocnt & BIT(15)) // FIFO enabled
        {
            // Remove a word from the front of receive FIFO
            cpuRecv->fifo->pop();

            if (cpuRecv->fifo->empty())
            {
                // If the receive FIFO is empty now, set the FIFO empty bits
                *cpuSend->ipcfifocnt |= BIT(8);
                *cpuRecv->ipcfifocnt |= BIT(0);

                // Trigger a receive FIFO empty IRQ if enabled
                if (*cpuRecv->ipcfifocnt & BIT(2))
                    *cpuRecv->irf |= BIT(17);
            }
            else if (cpuRecv->fifo->size() == 15)
            {
                // If the receive FIFO isn't full now, clear the FIFO full bits
                *cpuSend->ipcfifocnt &= ~BIT(9);
                *cpuRecv->ipcfifocnt &= ~BIT(1);
            }
        }
    }
    else
    {
        // If the receive FIFO is empty, indicate a receive empty error
        *cpuSend->ipcfifocnt |= BIT(14);
    }

    return cpuSend->ipcfiforecv;
}

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

void romTransferStart(interpreter::Cpu *cpu)
{
    // Get the size of the block to transfer
    uint8_t size = (*cpu->romctrl & 0x07000000) >> 24;
    switch (size)
    {
        case 0:  romBlockSize = 0;             break;
        case 7:  romBlockSize = 4;             break;
        default: romBlockSize = 0x100 << size; break;
    }

    // Reverse the byte order of the ROM command to make it easier to work with
    for (int i = 0; i < 8; i++)
       ((uint8_t*)&romCommand)[i] = ((uint8_t*)cpu->romcmdout)[7 - i];

    // Decrypt the ROM command if encryption is enabled
    if (romEncrypt)
        decrypt64(&romCommand);

    // Handle encryption commands
    if (rom)
    {
        if (((uint8_t*)&romCommand)[7] == 0x3C) // Activate KEY1 encryption mode
        {
            // Initialize KEY1 encryption
            initKeycode(*(uint32_t*)&rom[0x0C], 2);
            romEncrypt = true;
        }
        else if ((((uint8_t*)&romCommand)[7] & 0xF0) == 0xA0) // Enter main data mode
        {
            // Disable KEY1 encryption
            // On hardware, this is where KEY2 encryption would start
            romEncrypt = false;
        }
    }

    // Indicate transfer completion right away when the block size is 0
    if (romBlockSize == 0)
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
    romReadCount = 0;
}

uint32_t romTransfer(interpreter::Cpu *cpu)
{
    // Don't transfer if the word bit isn't set
    if (!(*cpu->romctrl & BIT(23)))
        return 0;

    // When a cart isn't inserted, endless 0xFFs are returned
    uint32_t value = 0xFFFFFFFF;

    // Interpret the current ROM command and change the return value if needed
    if (rom)
    {
        if (romCommand == 0x0000000000000000) // Get header
        {
            // Return the ROM header, repeated every 0x1000 bytes
            value = *(uint32_t*)&rom[romReadCount % 0x1000];
        }
        else if ((romCommand & 0xFF00000000FFFFFF) == 0xB700000000000000) // Get data
        {
            // Return ROM data from the given address
            uint32_t address = (romCommand & 0x00FFFFFFFF000000) >> 24;
            value = *(uint32_t*)&rom[address + romReadCount];
        }
        else if (romCommand != 0x9F00000000000000) // Dummy
        {
            // Split the ROM command into 2 32-bit parts for easier printing
            printf("ROM transfer with unknown command: 0x%.8X%.8X\n", ((uint32_t*)&romCommand)[1], ((uint32_t*)&romCommand)[0]);
        }
    }

    romReadCount += 4;

    // Indicate transfer completion when the block size has been reached
    if (romReadCount == romBlockSize)
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

void spiWrite(uint8_t value)
{
    // Don't do anything if the SPI isn't enabled
    if (!(*memory::spicnt & BIT(15)))
        return;

    // Receive the value written by the CPU and send a value back
    uint8_t device = (*memory::spicnt & 0x0300) >> 8;
    switch (device)
    {
        case 1: // Firmware
        {
            if (spiWriteCount == 0)
            {
                // On the first write, set the instruction byte
                spiInstr = value;
                spiAddr = 0;
                *memory::spidata = 0;
            }
            else if (spiInstr == 0x03) // READ
            {
                if (spiWriteCount < 4)
                {
                    // On writes 2-4, set the 3 byte address to read from
                    ((uint8_t*)&spiAddr)[3 - spiWriteCount] = value;
                }
                else
                {
                    // On writes 5+, read from the firmware and send it back
                    if (spiAddr < sizeof(firmware))
                        *memory::spidata = firmware[spiAddr];

                    // 16-bit mode is bugged; the address is incremented accordingly, but only the lower 8 bits are sent
                    spiAddr += (*memory::spicnt & BIT(10)) ? 2 : 1;
                }
            }
            else
            {
                *memory::spidata = 0;
                printf("Unknown firmware SPI instruction: 0x%X\n", spiInstr);
            }
            break;
        }

        default:
        {
            *memory::spidata = 0;
            printf("Write to unknown SPI device: %d\n", device);
            break;
        }
    }

    // Keep track of the write count
    if (*memory::spicnt & BIT(11)) // Keep chip selected
        spiWriteCount++;
    else // Deselect chip
        spiWriteCount = 0;

    // Trigger a transfer finished IRQ if enabled
    // Transfers shouldn't complete instantly like this, but there's no timing system yet
    if (*memory::spicnt & BIT(14))
        *interpreter::arm7.irf |= BIT(23);
}

void init()
{
    rtcWriteCount = 0;
    rtcCommand = 0;
    rtcLast = 0;

    // Empty the FIFOs
    while (!fifo9.empty()) fifo9.pop();
    while (!fifo7.empty()) fifo7.pop();

    romEncrypt = false;
    spiWriteCount = 0;

    interpreter::arm9.fifo = &fifo9;
    interpreter::arm7.fifo = &fifo7;
}

}
