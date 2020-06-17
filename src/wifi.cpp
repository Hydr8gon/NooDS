/*
    Copyright 2020 Hydr8gon

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

#include "wifi.h"
#include "defines.h"
#include "memory.h"

Wifi::Wifi(Memory *memory): memory(memory)
{
    // Set some default BB register values
    bbRegisters[0x00] = 0x6D;
    bbRegisters[0x5D] = 0x01;
    bbRegisters[0x64] = 0xFF;
}

void Wifi::writeWMacaddr(int index, uint16_t mask, uint16_t value)
{
    // Write to one of the the W_MACADDR registers
    wMacaddr[index] = (wMacaddr[index] & ~mask) | (value & mask);
}

void Wifi::writeWBssid(int index, uint16_t mask, uint16_t value)
{
    // Write to one of the the W_BSSID registers
    wBssid[index] = (wBssid[index] & ~mask) | (value & mask);
}

void Wifi::writeWRxbufBegin(uint16_t mask, uint16_t value)
{
    // Write to the W_RXBUF_BEGIN register
    wRxbufBegin = (wRxbufBegin & ~mask) | (value & mask);
}

void Wifi::writeWRxbufEnd(uint16_t mask, uint16_t value)
{
    // Write to the W_RXBUF_END register
    wRxbufEnd = (wRxbufEnd & ~mask) | (value & mask);
}

void Wifi::writeWRxbufRdAddr(uint16_t mask, uint16_t value)
{
    // Write to the W_RXBUF_RD_ADDR register
    mask &= 0x1FFE;
    wRxbufRdAddr = (wRxbufRdAddr & ~mask) | (value & mask);
}

void Wifi::writeWRxbufReadcsr(uint16_t mask, uint16_t value)
{
    // Write to the W_RXBUF_RDCSR register
    mask &= 0x0FFF;
    wRxbufReadcsr = (wRxbufReadcsr & ~mask) | (value & mask);
}

void Wifi::writeWRxbufGap(uint16_t mask, uint16_t value)
{
    // Write to the W_RXBUF_GAP register
    mask &= 0x1FFE;
    wRxbufGap = (wRxbufGap & ~mask) | (value & mask);
}

void Wifi::writeWRxbufGapdisp(uint16_t mask, uint16_t value)
{
    // Write to the W_RXBUF_GAPDISP register
    mask &= 0x0FFF;
    wRxbufGapdisp = (wRxbufGapdisp & ~mask) | (value & mask);
}

void Wifi::writeWRxbufCount(uint16_t mask, uint16_t value)
{
    // Write to the W_RXBUF_COUNT register
    mask &= 0x0FFF;
    wRxbufCount = (wRxbufCount & ~mask) | (value & mask);
}

void Wifi::writeWTxbufWrAddr(uint16_t mask, uint16_t value)
{
    // Write to the W_TXBUF_WR_ADDR register
    mask &= 0x1FFE;
    wTxbufWrAddr = (wTxbufWrAddr & ~mask) | (value & mask);
}

void Wifi::writeWTxbufWrData(uint16_t mask, uint16_t value)
{
    // Write a value to WiFi RAM
    memory->write<uint16_t>(false, 0x4804000 + wTxbufWrAddr, value & mask);

    // Increment the write address
    wTxbufWrAddr += 2;
    if (wTxbufWrAddr == wTxbufGap)
        wTxbufWrAddr += wTxbufGapdisp << 1;
    wTxbufWrAddr %= 0x2000;
}

void Wifi::writeWTxbufGap(uint16_t mask, uint16_t value)
{
    // Write to the W_TXBUF_GAP register
    mask &= 0x1FFE;
    wTxbufGap = (wTxbufGap & ~mask) | (value & mask);
}

void Wifi::writeWTxbufGapdisp(uint16_t mask, uint16_t value)
{
    // Write to the W_TXBUF_GAPDISP register
    mask &= 0x0FFF;
    wTxbufGapdisp = (wTxbufGapdisp & ~mask) | (value & mask);
}

void Wifi::writeWConfig(int index, uint16_t mask, uint16_t value)
{
    const uint16_t masks[] =
    {
        0x81FF, 0xFFFF, 0xFFFF, 0xFFFF, 0x0FFF,
        0x8FFF, 0xFFFF, 0xFFFF, 0x00FF, 0x00FF,
        0x00FF, 0x00FF, 0xFFFF, 0xFF3F, 0x7A7F
    };

    // Write to one of the W_CONFIG registers
    mask &= masks[index];
    wConfig[index] = (wConfig[index] & ~mask) | (value & mask);
}

void Wifi::writeWBbCnt(uint16_t mask, uint16_t value)
{
    int index = value & 0x00FF;

    // Perform a BB register transfer
    switch ((value & 0xF000) >> 12)
    {
        case 5: // Write
        {
            if ((index >= 0x01 && index <= 0x0C) || (index >= 0x13 && index <= 0x15) || (index >= 0x1B && index <= 0x26) ||
                (index >= 0x28 && index <= 0x4C) || (index >= 0x4E && index <= 0x5C) || (index >= 0x62 && index <= 0x63) ||
                (index == 0x65) || (index >= 0x67 && index <= 0x68)) // Writable registers
                bbRegisters[index] = wBbWrite;
            break;
        }

        case 6: // Read
        {
            wBbRead = bbRegisters[index];
            break;
        }
    }

    // Trigger the busy flag
    if (bbCount == 0)
        bbCount++;
}

void Wifi::writeWBbWrite(uint16_t mask, uint16_t value)
{
    // Write to the W_BB_WRITE register
    wBbWrite = (wBbWrite & ~mask) | (value & mask);
}

uint16_t Wifi::readWRxbufRdData()
{
    // Read a value from WiFi RAM
    uint16_t value = memory->read<uint16_t>(false, 0x4804000 + wRxbufRdAddr);

    // Increment the read address
    wRxbufRdAddr += 2;
    if (wRxbufRdAddr == wRxbufGap)
        wRxbufRdAddr += wRxbufGapdisp << 1;
    if ((wRxbufBegin & 0x1FFE) != (wRxbufEnd & 0x1FFE))
        wRxbufRdAddr = (wRxbufBegin & 0x1FFE) + (wRxbufRdAddr - (wRxbufBegin & 0x1FFE)) % ((wRxbufEnd & 0x1FFE) - (wRxbufBegin & 0x1FFE));
    wRxbufRdAddr %= 0x2000;

    // Decrement the read counter
    if (wRxbufCount > 0)
        wRxbufCount--;

    return value;
}

uint16_t Wifi::readWBbBusy()
{
    // If a BB transfer was triggered, set the busy flag for an arbitrary amount of reads
    // This is a gross hack meant to stall WiFi initialization in the 4th gen Pokémon games
    // The games give an error message on the save select screen, preventing save loading
    // This allows enough time to load a save, and shouldn't break anything since it eventually clears
    // This is temporary; a proper fix will likely require a good chunk of the WiFi to be implemented
    if (bbCount > 0)
    {
        if (++bbCount == 3000)
            bbCount = 0;
        return 1;
    }
    else
    {
        return 0;
    }
}
