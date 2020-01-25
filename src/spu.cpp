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

#include <cstdio>

#include "spu.h"
#include "defines.h"
#include "memory.h"

const int Spu::indexTable[] =
{
    -1, -1, -1, -1, 2, 4, 6, 8
};

const int16_t Spu::adpcmTable[] =
{
    0x0007, 0x0008, 0x0009, 0x000A, 0x000B, 0x000C, 0x000D, 0x000E, 0x0010, 0x0011, 0x0013, 0x0015,
    0x0017, 0x0019, 0x001C, 0x001F, 0x0022, 0x0025, 0x0029, 0x002D, 0x0032, 0x0037, 0x003C, 0x0042,
    0x0049, 0x0050, 0x0058, 0x0061, 0x006B, 0x0076, 0x0082, 0x008F, 0x009D, 0x00AD, 0x00BE, 0x00D1,
    0x00E6, 0x00FD, 0x0117, 0x0133, 0x0151, 0x0173, 0x0198, 0x01C1, 0x01EE, 0x0220, 0x0256, 0x0292,
    0x02D4, 0x031C, 0x036C, 0x03C3, 0x0424, 0x048E, 0x0502, 0x0583, 0x0610, 0x06AB, 0x0756, 0x0812,
    0x08E0, 0x09C3, 0x0ABD, 0x0BD0, 0x0CFF, 0x0E4C, 0x0FBA, 0x114C, 0x1307, 0x14EE, 0x1706, 0x1954,
    0x1BDC, 0x1EA5, 0x21B6, 0x2515, 0x28CA, 0x2CDF, 0x315B, 0x364B, 0x3BB9, 0x41B2, 0x4844, 0x4F7E,
    0x5771, 0x602F, 0x69CE, 0x7462, 0x7FFF
};

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

            case 2: // ADPCM
            {
                data = adpcmValue[i];
                size = 0;
                break;
            }

            default:
            {
                printf("SPU channel %d: unimplemented format: %d\n", i, (soundCnt[i] & 0x60000000) >> 29);
                soundCnt[i] &= ~BIT(31);
                continue;
            }
        }

        // Increment the timer for the length of a sample
        // The SPU runs at 16756991Hz with a sample rate of 32768Hz
        // 16756991 / 32768 = ~512 cycles per sample
        soundTimers[i] += 512;
        bool overflow = (soundTimers[i] < 512);

        // Handle timer overflow
        while (overflow)
        {
            // Increment the data pointer and reload the timer
            soundCurrent[i] += size;
            soundTimers[i] += soundTmr[i];
            overflow = (soundTimers[i] < soundTmr[i]);

            // Decode the next ADPCM audio sample
            if (((soundCnt[i] & 0x60000000) >> 29) == 2)
            {
                // Get the 4-bit ADPCM data
                uint8_t adpcmData = memory->read<uint8_t>(false, soundCurrent[i]);
                adpcmData = adpcmToggle[i] ? ((adpcmData & 0xF0) >> 4) : (adpcmData & 0x0F);

                // Calculate the sample difference
                int32_t diff = adpcmTable[adpcmIndex[i]] / 8;
                if (adpcmData & BIT(0)) diff += adpcmTable[adpcmIndex[i]] / 4;
                if (adpcmData & BIT(1)) diff += adpcmTable[adpcmIndex[i]] / 2;
                if (adpcmData & BIT(2)) diff += adpcmTable[adpcmIndex[i]] / 1;

                // Apply the sample difference to the sample
                if (adpcmData & BIT(3))
                {
                    adpcmValue[i] += diff;
                    if (adpcmValue[i] > 0x7FFF) adpcmValue[i] = 0x7FFF;
                }
                else
                {
                    adpcmValue[i] -= diff;
                    if (adpcmValue[i] < -0x7FFF) adpcmValue[i] = -0x7FFF;
                }

                // Calculate the next index
                adpcmIndex[i] += indexTable[adpcmData & 0x7];
                if (adpcmIndex[i] <  0) adpcmIndex[i] =  0;
                if (adpcmIndex[i] > 88) adpcmIndex[i] = 88;

                // Move to the next 4-bit ADPCM data
                adpcmToggle[i] = !adpcmToggle[i];
                if (!adpcmToggle[i]) soundCurrent[i]++;

                // Save the ADPCM values from the loop position
                if (soundCurrent[i] == soundSad[i] + soundPnt[i] * 4 && !adpcmToggle[i])
                {
                    adpcmLoopValue[i] = adpcmValue[i];
                    adpcmLoopIndex[i] = adpcmIndex[i];
                }
            }

            // Repeat or end the sound if the end of the data is reached
            if (soundCurrent[i] == soundSad[i] + (soundPnt[i] + soundLen[i]) * 4)
            {
                if ((soundCnt[i] & 0x18000000) >> 27 == 1) // Loop infinite
                {
                    soundCurrent[i] = soundSad[i] + soundPnt[i] * 4;

                    // Restore the ADPCM values from the loop position
                    if (((soundCnt[i] & 0x60000000) >> 29) == 2)
                    {
                        adpcmValue[i] = adpcmLoopValue[i];
                        adpcmIndex[i] = adpcmLoopIndex[i];
                        adpcmToggle[i] = false;
                    }
                }
                else // One-shot
                {
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

        // Read the ADPCM header
        if (((soundCnt[channel] & 0x60000000) >> 29) == 2)
        {
            adpcmValue[channel] = (int16_t)soundSad[channel];
            adpcmIndex[channel] = soundSad[channel] >> 16;
            if (adpcmIndex[channel] <  0) adpcmIndex[channel] =  0;
            if (adpcmIndex[channel] > 88) adpcmIndex[channel] = 88;
            adpcmToggle[channel] = false;
            soundCurrent[channel] += 4;
        }
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
