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

#include "spu.h"
#include "defines.h"
#include "memory.h"

uint32_t Spu::getSample()
{
    int64_t sampleLeft = 0;
    int64_t sampleRight = 0;

    // Mix the sound channels
    for (int i = 0; i < 16; i++)
    {
        // Skip disabled channels
        if (!(soundCnt[i] & BIT(31)))
            continue;

        int64_t data;
        int size;

        // Read the sample data
        switch ((soundCnt[i] & 0x60000000) >> 29) // Format
        {
            case 0: // PCM8
            {
                data = memory->read<int8_t>(false, soundCurrent[i]) << 8;
                size = 1;
                break;
            }

            case 1: // PCM16
            {
                data = memory->read<int16_t>(false, soundCurrent[i]);
                size = 2;
                break;
            }

            default:
            {
                printf("SPU channel %d: unimplemented format: %d\n", i, (soundCnt[i] & 0x60000000) >> 29);
                soundCnt[i] &= ~BIT(31);
                continue;
            }
        }

        // Increment the timers for the length of a sample
        // The SPU runs at 16756991Hz with a sample rate of 32768Hz
        // 16756991 / 32768 = ~512 cycles per sample
        for (int j = 0; j < 512; j++)
        {
            soundTimers[i]++;

            // Handle timer overflow
            if (soundTimers[i] == 0)
            {
                // Increment the data pointer and reload the timer
                soundCurrent[i] += size;
                soundTimers[i] = soundTmr[i];

                // Repeat or end the sound if the end of the data is reached
                if (soundCurrent[i] == soundSad[i] + (soundPnt[i] + soundLen[i]) * 4)
                {
                   if ((soundCnt[i] & 0x18000000) >> 27 == 1) // Loop infinite
                        soundCurrent[i] = soundSad[i] + soundPnt[i] * 4;
                    else // One-shot
                        soundCnt[i] &= ~BIT(31);
                }
            }
        }

        // Apply the volume divider
        // The sample now has 4 fractional bits
        int divShift = (soundCnt[i] & 0x00000300) >> 8;
        if (divShift == 3) divShift++;
        data <<= 4 - divShift;

        // Apply the volume factor
        // The sample now has 11 fractional bits
        int mulFactor = (soundCnt[i] & 0x0000007F);
        if (mulFactor == 127) mulFactor++;
        data = (data << 7) * mulFactor / 128;

        // Apply panning
        // The sample has 18 fractional bits after panning, but is then rounded to 8 fractional bits
        int panValue = (soundCnt[i] & 0x007F0000) >> 16;
        if (panValue == 127) panValue++;
        sampleLeft  += ((data << 7) * (128 - panValue) / 128) >> 10;
        sampleRight += ((data << 7) *        panValue  / 128) >> 10;
    }

    // Apply the master volume
    // The samples now have 21 fractional bits
    int masterVol = (mainSoundCnt & 0x007F);
    if (masterVol == 127) masterVol++;
    sampleLeft  = (sampleLeft  << 13) * masterVol / 128 / 64;
    sampleRight = (sampleRight << 13) * masterVol / 128 / 64;

    // Round to 0 fractional bits and apply the sound bias
    sampleLeft  = (sampleLeft  >> 21) + soundBias;
    sampleRight = (sampleRight >> 21) + soundBias;

    // Apply clipping
    if (sampleLeft  < 0x000) sampleLeft  = 0x000;
    if (sampleLeft  > 0x3FF) sampleLeft  = 0x3FF;
    if (sampleRight < 0x000) sampleRight = 0x000;
    if (sampleRight > 0x3FF) sampleRight = 0x3FF;

    // Expanded the samples to signed 16-bit values and return them
    sampleLeft  = (sampleLeft  - 0x200) << 6;
    sampleRight = (sampleRight - 0x200) << 6;
    return (sampleRight << 16) | (sampleLeft & 0xFFFF);
}

void Spu::writeSoundCnt(int channel, uint32_t mask, uint32_t value)
{
    // Reload the internal registers if the enable bit changes from 0 to 1
    if (!(soundCnt[channel] & BIT(31)) && (value & BIT(31)))
    {
        soundCurrent[channel] = soundSad[channel];
        soundTimers[channel] = soundTmr[channel];
    }

    // Write to one of the SOUNDCNT registers
    mask &= 0xFF7F837F;
    soundCnt[channel] = (soundCnt[channel] & ~mask) | (value & mask);
}

void Spu::writeSoundSad(int channel, uint32_t mask, uint32_t value)
{
    // Write to one of the SOUNDSAD registers
    mask &= 0x07FFFFFC;
    soundSad[channel] = (soundSad[channel] & ~mask) | (value & mask);
}

void Spu::writeSoundTmr(int channel, uint16_t mask, uint16_t value)
{
    // Write to one of the SOUNDTMR registers
    soundTmr[channel] = (soundTmr[channel] & ~mask) | (value & mask);
}

void Spu::writeSoundPnt(int channel, uint16_t mask, uint16_t value)
{
    // Write to one of the SOUNDPNT registers
    soundPnt[channel] = (soundPnt[channel] & ~mask) | (value & mask);
}

void Spu::writeSoundLen(int channel, uint32_t mask, uint32_t value)
{
    // Write to one of the SOUNDLEN registers
    mask &= 0x003FFFFF;
    soundLen[channel] = (soundLen[channel] & ~mask) | (value & mask);
}

void Spu::writeMainSoundCnt(uint16_t mask, uint16_t value)
{
    // Write to the main SOUNDCNT register
    mask &= 0xBF7F;
    mainSoundCnt = (mainSoundCnt & ~mask) | (value & mask);
}

void Spu::writeSoundBias(uint16_t mask, uint16_t value)
{
    // Write to the SOUNDBIAS register
    mask &= 0x03FF;
    soundBias = (soundBias & ~mask) | (value & mask);
}
