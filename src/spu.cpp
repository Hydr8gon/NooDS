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

#include <chrono>
#include <cstring>

#include "core.h"

const int Spu::indexTable[] = {
    -1, -1, -1, -1, 2, 4, 6, 8
};

const int16_t Spu::adpcmTable[] = {
    0x0007, 0x0008, 0x0009, 0x000A, 0x000B, 0x000C, 0x000D, 0x000E, 0x0010, 0x0011, 0x0013, 0x0015,
    0x0017, 0x0019, 0x001C, 0x001F, 0x0022, 0x0025, 0x0029, 0x002D, 0x0032, 0x0037, 0x003C, 0x0042,
    0x0049, 0x0050, 0x0058, 0x0061, 0x006B, 0x0076, 0x0082, 0x008F, 0x009D, 0x00AD, 0x00BE, 0x00D1,
    0x00E6, 0x00FD, 0x0117, 0x0133, 0x0151, 0x0173, 0x0198, 0x01C1, 0x01EE, 0x0220, 0x0256, 0x0292,
    0x02D4, 0x031C, 0x036C, 0x03C3, 0x0424, 0x048E, 0x0502, 0x0583, 0x0610, 0x06AB, 0x0756, 0x0812,
    0x08E0, 0x09C3, 0x0ABD, 0x0BD0, 0x0CFF, 0x0E4C, 0x0FBA, 0x114C, 0x1307, 0x14EE, 0x1706, 0x1954,
    0x1BDC, 0x1EA5, 0x21B6, 0x2515, 0x28CA, 0x2CDF, 0x315B, 0x364B, 0x3BB9, 0x41B2, 0x4844, 0x4F7E,
    0x5771, 0x602F, 0x69CE, 0x7462, 0x7FFF
};

Spu::Spu(Core *core): core(core) {
    // Mark the buffer as not ready
    ready.store(false);
}

Spu::~Spu() {
    // Free the buffers
    delete[] bufferIn;
    delete[] bufferOut;
}

void Spu::saveState(FILE *file) {
    // Write state data to the file
    fwrite(&gbaFrameSequencer, sizeof(gbaFrameSequencer), 1, file);
    fwrite(gbaSoundTimers, 4, sizeof(gbaSoundTimers) / 4, file);
    fwrite(gbaEnvelopes, 1, sizeof(gbaEnvelopes), file);
    fwrite(gbaEnvTimers, 1, sizeof(gbaEnvTimers), file);
    fwrite(&gbaSweepTimer, sizeof(gbaSweepTimer), 1, file);
    fwrite(&gbaWaveDigit, sizeof(gbaWaveDigit), 1, file);
    fwrite(&gbaNoiseValue, sizeof(gbaNoiseValue), 1, file);
    fwrite(gbaWaveRam, 1, sizeof(gbaWaveRam), file);
    fwrite(&gbaSampleA, sizeof(gbaSampleA), 1, file);
    fwrite(&gbaSampleB, sizeof(gbaSampleB), 1, file);
    fwrite(&enabled, sizeof(enabled), 1, file);
    fwrite(adpcmValue, 4, sizeof(adpcmValue) / 4, file);
    fwrite(adpcmLoopValue, 4, sizeof(adpcmLoopValue) / 4, file);
    fwrite(adpcmIndex, 1, sizeof(adpcmIndex), file);
    fwrite(adpcmLoopIndex, 1, sizeof(adpcmLoopIndex), file);
    fwrite(adpcmToggle, sizeof(bool), sizeof(adpcmToggle) / sizeof(bool), file);
    fwrite(dutyCycles, 1, sizeof(dutyCycles), file);
    fwrite(noiseValues, 2, sizeof(noiseValues) / 2, file);
    fwrite(soundCurrent, 4, sizeof(soundCurrent) / 4, file);
    fwrite(soundTimers, 2, sizeof(soundTimers) / 2, file);
    fwrite(sndCapCurrent, 4, sizeof(sndCapCurrent) / 4, file);
    fwrite(sndCapTimers, 2, sizeof(sndCapTimers) / 2, file);
    fwrite(gbaSoundCntL, 1, sizeof(gbaSoundCntL), file);
    fwrite(gbaSoundCntH, 2, sizeof(gbaSoundCntH) / 2, file);
    fwrite(gbaSoundCntX, 2, sizeof(gbaSoundCntX) / 2, file);
    fwrite(&gbaMainSoundCntL, sizeof(gbaMainSoundCntL), 1, file);
    fwrite(&gbaMainSoundCntH, sizeof(gbaMainSoundCntH), 1, file);
    fwrite(&gbaMainSoundCntX, sizeof(gbaMainSoundCntX), 1, file);
    fwrite(&gbaSoundBias, sizeof(gbaSoundBias), 1, file);
    fwrite(soundCnt, 4, sizeof(soundCnt) / 4, file);
    fwrite(soundSad, 4, sizeof(soundSad) / 4, file);
    fwrite(soundTmr, 2, sizeof(soundTmr) / 2, file);
    fwrite(soundPnt, 2, sizeof(soundPnt) / 2, file);
    fwrite(soundLen, 4, sizeof(soundLen) / 4, file);
    fwrite(&mainSoundCnt, sizeof(mainSoundCnt), 1, file);
    fwrite(&soundBias, sizeof(soundBias), 1, file);
    fwrite(sndCapCnt, 1, sizeof(sndCapCnt), file);
    fwrite(sndCapDad, 4, sizeof(sndCapDad) / 4, file);
    fwrite(sndCapLen, 2, sizeof(sndCapLen) / 2, file);

    // Parse the FIFOs and save their values
    for (int i = 0; i < 2; i++) {
        uint32_t count = gbaFifos[i].size();
        fwrite(&count, sizeof(count), 1, file);
        for (uint32_t j = 0; j < count; j++)
            fwrite(&gbaFifos[i][j], sizeof(gbaFifos[i][j]), 1, file);
    }
}

void Spu::loadState(FILE *file) {
    // Read state data from the file
    fread(&gbaFrameSequencer, sizeof(gbaFrameSequencer), 1, file);
    fread(gbaSoundTimers, 4, sizeof(gbaSoundTimers) / 4, file);
    fread(gbaEnvelopes, 1, sizeof(gbaEnvelopes), file);
    fread(gbaEnvTimers, 1, sizeof(gbaEnvTimers), file);
    fread(&gbaSweepTimer, sizeof(gbaSweepTimer), 1, file);
    fread(&gbaWaveDigit, sizeof(gbaWaveDigit), 1, file);
    fread(&gbaNoiseValue, sizeof(gbaNoiseValue), 1, file);
    fread(gbaWaveRam, 1, sizeof(gbaWaveRam), file);
    fread(&gbaSampleA, sizeof(gbaSampleA), 1, file);
    fread(&gbaSampleB, sizeof(gbaSampleB), 1, file);
    fread(&enabled, sizeof(enabled), 1, file);
    fread(adpcmValue, 4, sizeof(adpcmValue) / 4, file);
    fread(adpcmLoopValue, 4, sizeof(adpcmLoopValue) / 4, file);
    fread(adpcmIndex, 1, sizeof(adpcmIndex), file);
    fread(adpcmLoopIndex, 1, sizeof(adpcmLoopIndex), file);
    fread(adpcmToggle, sizeof(bool), sizeof(adpcmToggle) / sizeof(bool), file);
    fread(dutyCycles, 1, sizeof(dutyCycles), file);
    fread(noiseValues, 2, sizeof(noiseValues) / 2, file);
    fread(soundCurrent, 4, sizeof(soundCurrent) / 4, file);
    fread(soundTimers, 2, sizeof(soundTimers) / 2, file);
    fread(sndCapCurrent, 4, sizeof(sndCapCurrent) / 4, file);
    fread(sndCapTimers, 2, sizeof(sndCapTimers) / 2, file);
    fread(gbaSoundCntL, 1, sizeof(gbaSoundCntL), file);
    fread(gbaSoundCntH, 2, sizeof(gbaSoundCntH) / 2, file);
    fread(gbaSoundCntX, 2, sizeof(gbaSoundCntX) / 2, file);
    fread(&gbaMainSoundCntL, sizeof(gbaMainSoundCntL), 1, file);
    fread(&gbaMainSoundCntH, sizeof(gbaMainSoundCntH), 1, file);
    fread(&gbaMainSoundCntX, sizeof(gbaMainSoundCntX), 1, file);
    fread(&gbaSoundBias, sizeof(gbaSoundBias), 1, file);
    fread(soundCnt, 4, sizeof(soundCnt) / 4, file);
    fread(soundSad, 4, sizeof(soundSad) / 4, file);
    fread(soundTmr, 2, sizeof(soundTmr) / 2, file);
    fread(soundPnt, 2, sizeof(soundPnt) / 2, file);
    fread(soundLen, 4, sizeof(soundLen) / 4, file);
    fread(&mainSoundCnt, sizeof(mainSoundCnt), 1, file);
    fread(&soundBias, sizeof(soundBias), 1, file);
    fread(sndCapCnt, 1, sizeof(sndCapCnt), file);
    fread(sndCapDad, 4, sizeof(sndCapDad) / 4, file);
    fread(sndCapLen, 2, sizeof(sndCapLen) / 2, file);

    // Reset the FIFOs and refill them with loaded values
    for (int i = 0; i < 2; i++) {
        gbaFifos[i].clear();
        uint32_t count;
        int8_t value;
        fread(&count, sizeof(count), 1, file);
        for (uint32_t j = 0; j < count; j++) {
            fread(&value, sizeof(value), 1, file);
            gbaFifos[i].push_back(value);
        }
    }
}

uint32_t *Spu::getSamples(int count) {
    // Initialize the buffers
    if (bufferSize != count) {
        delete[] bufferIn;
        delete[] bufferOut;
        bufferIn = new uint32_t[count];
        bufferOut = new uint32_t[count];
        bufferSize = count;
        bufferPointer = 0;
    }

    bool wait;

    // If FPS limit is enabled, try to wait until the buffer is filled
    // If the emulation isn't full speed, waiting would starve the audio buffer
    // So if it's taking too long, just let it play an empty buffer
    if (Settings::fpsLimiter == 2) { // Accurate
        std::chrono::steady_clock::time_point waitTime = std::chrono::steady_clock::now();
        wait = false;

        // Use a while loop to constantly check if the wait condition has been satisfied
        // This is wasteful, but ensures a swift break from the wait state
        while (!ready.load()) {
            if (std::chrono::steady_clock::now() - waitTime > std::chrono::microseconds(1000000 / 60)) {
                wait = true;
                break;
            }
        }
    }
    else { // Disabled/Light
        // Use a condition variable to save CPU cycles
        // This might take longer than expected due to the OS scheduler and other factors
        std::unique_lock<std::mutex> lock(mutex2);
        wait = !cond2.wait_for(lock, std::chrono::microseconds(1000000 / 60), [&]{ return ready.load(); });
    }

    uint32_t *out = new uint32_t[count];
    
    if (wait) {
        // Fill the output buffer with the last played sample to prevent crackles when running slow
        for (int i = 0; i < count; i++)
            out[i] = bufferOut[count - 1];
    }
    else {
        // Fill the output buffer with new data
        memcpy(out, bufferOut, count * sizeof(uint32_t));
    }

    { // Signal that the buffer was played
        std::lock_guard<std::mutex> guard(mutex1);
        ready.store(false);
        cond1.notify_one();
    }

    return out;
}

void Spu::runGbaSample() {
    // Push a dummy sample if disabled and schedule the next one
    core->schedule(GBA_SPU_SAMPLE, 512);
    if (!Settings::emulateAudio) return pushSample(0, 0);

    // Generate an audio sample
    int32_t sampleLeft = 0, sampleRight = 0;
    if (gbaMainSoundCntX & BIT(7)) {
        // Run the tone channels
        int32_t data[4] = {};
        for (int i = 0; i < 2; i++) {
            if (!(gbaMainSoundCntX & BIT(i)))
                continue;

            // Run the frequency sweeper at 128Hz when enabled (first channel only)
            if (i == 0 && gbaFrameSequencer % 256 == 128 && (gbaSoundCntL[i] & 0x70) && --gbaSweepTimer <= 0) {
                // Calculate the frequency change
                uint16_t frequency = (gbaSoundCntX[i] & 0x07FF);
                int sweep = frequency >> (gbaSoundCntL[i] & 0x07);
                if (gbaSoundCntL[i] & BIT(3)) sweep = -sweep;

                // Sweep the frequency
                frequency += sweep;

                if (frequency < 0x800) {
                    // Set the new frequency and reload the sweep timer
                    gbaSoundCntX[i] = (gbaSoundCntX[i] & ~0x07FF) | frequency;
                    gbaSweepTimer = (gbaSoundCntL[i] & 0x70) >> 4;
                }
                else {
                    // Disable the channel if the frequency is too high
                    gbaMainSoundCntX &= ~BIT(i);
                    continue;
                }
            }

            // Decrement and reload the sound timer
            gbaSoundTimers[i] -= 4;
            while ((gbaSoundTimers[i]) <= 0)
                gbaSoundTimers[i] += 2048 - (gbaSoundCntX[i] & 0x07FF);

            // Determine the point in the duty cycle where the sample switches from low to high
            int duty;
            switch ((gbaSoundCntH[i] & 0x00C0) >> 6) {
                case 0: duty = (2048 - (gbaSoundCntX[i] & 0x07FF)) * 7 / 8; break;
                case 1: duty = (2048 - (gbaSoundCntX[i] & 0x07FF)) * 6 / 8; break;
                case 2: duty = (2048 - (gbaSoundCntX[i] & 0x07FF)) * 4 / 8; break;
                case 3: duty = (2048 - (gbaSoundCntX[i] & 0x07FF)) * 2 / 8; break;
            }

            // Set the sample to low or high based on the position in the duty cycle
            data[i] = (gbaSoundTimers[i] < duty) ? -0x80 : 0x80;

            // Run the length counter at 256Hz when enabled
            if (gbaFrameSequencer % 128 == 0 && (gbaSoundCntX[i] & BIT(14)) && (gbaSoundCntH[i] & 0x003F)) {
                // Decrement the length counter
                gbaSoundCntH[i] = (gbaSoundCntH[i] & ~0x003F) | ((gbaSoundCntH[i] & 0x003F) - 1);

                // Disable the channel when the counter hits zero
                if ((gbaSoundCntH[i] & 0x003F) == 0)
                    gbaMainSoundCntX &= ~BIT(i);
            }

            // Run the envelope timer at 64Hz
            if (gbaFrameSequencer == 448 && --gbaEnvTimers[i] <= 0) {
                if (gbaEnvTimers[i] == 0) {
                    // Adjust the envelope volume if the timer period was non-zero
                    if ((gbaSoundCntH[i] & BIT(11)) && gbaEnvelopes[i] < 15)
                        gbaEnvelopes[i]++;
                    else if (!(gbaSoundCntH[i] & BIT(11)) && gbaEnvelopes[i] > 0)
                        gbaEnvelopes[i]--;
                }
                else {
                    // The envelope seems to reset with a period of zero?
                    gbaEnvelopes[i] = (gbaSoundCntH[i] & 0xF000) >> 12;
                }

                // Reload the envelope timer
                gbaEnvTimers[i] = (gbaSoundCntH[i] & 0x0700) >> 8;
            }

            // Apply the envelope volume
            data[i] = data[i] * gbaEnvelopes[i] / 15;
        }

        // Run the wave channel
        if ((gbaMainSoundCntX & BIT(2)) && (gbaSoundCntL[1] & BIT(7))) {
            // Decrement and reload the sound timer
            // Each reload increases the current wave digit
            gbaSoundTimers[2] -= 64;
            while ((gbaSoundTimers[2]) <= 0) {
                gbaSoundTimers[2] += (2048 - (gbaSoundCntX[2] & 0x07FF));
                gbaWaveDigit = (gbaWaveDigit + 1) % 64;
            }

            // Determine which wave RAM bank to read from
            // If the dimension is set to 2, samples from the other bank will play after the first 32 samples
            int bank = (gbaSoundCntL[1] & BIT(6)) >> 6;
            if ((gbaSoundCntL[1] & BIT(5)) && gbaWaveDigit >= 32)
                bank = !bank;

            // Read the current 4-bit sample from the wave RAM
            data[2] = gbaWaveRam[bank][(gbaWaveDigit % 32) / 2];
            if (gbaWaveDigit & 1)
                data[2] &= 0x0F;
            else
                data[2] >>= 4;

            // Run the length counter at 256Hz when enabled
            if (gbaFrameSequencer % 128 == 0 && (gbaSoundCntX[2] & BIT(14)) && (gbaSoundCntH[2] & 0x00FF)) {
                // Decrement the length counter
                gbaSoundCntH[2] = (gbaSoundCntH[2] & ~0x00FF) | ((gbaSoundCntH[2] & 0x00FF) - 1);

                // Disable the channel when the counter hits zero
                if ((gbaSoundCntH[2] & 0x00FF) == 0)
                    gbaMainSoundCntX &= ~BIT(2);
            }

            // Apply volume
            // If bit 15 is set, the volume shift is overridden and 75% is forced
            switch ((gbaSoundCntH[2] & 0xE000) >> 13) {
                case 0: data[2] >>= 4; break;
                case 1: data[2] >>= 0; break;
                case 2: data[2] >>= 1; break;
                case 3: data[2] >>= 2; break;
                default: data[2] = data[2] * 3 / 4; break;
            }

            // Convert the sample to an 8-bit value
            data[2] = (data[2] * 0x100 / 0xF);
        }

        // Run the noise channel
        if (gbaMainSoundCntX & BIT(3)) {
            // Decrement and reload the sound timer
            // Each reload advances the random generator
            gbaSoundTimers[3] -= 16;
            while ((gbaSoundTimers[3]) <= 0) {
                int divisor = (gbaSoundCntX[3] & 0x0007) * 16;
                if (divisor == 0) divisor = 8;
                gbaSoundTimers[3] += (divisor << ((gbaSoundCntX[3] & 0x00F0) >> 4));

                // Advance the random generator and save the carry bit to bit 15
                gbaNoiseValue &= ~BIT(15);
                if (gbaNoiseValue & BIT(0))
                    gbaNoiseValue = BIT(15) | ((gbaNoiseValue >> 1) ^ ((gbaSoundCntH[3] & BIT(3)) ? 0x60 : 0x6000));
                else
                    gbaNoiseValue >>= 1;
            }

            // Set the sample to low or high based on the last carry bits
            data[3] = (gbaNoiseValue & BIT(15)) ? 0x80 : -0x80;

            // Run the length counter at 256Hz when enabled
            if (gbaFrameSequencer % 128 == 0 && (gbaSoundCntX[3] & BIT(14)) && (gbaSoundCntH[3] & 0x003F)) {
                // Decrement the length counter
                gbaSoundCntH[3] = (gbaSoundCntH[3] & ~0x003F) | ((gbaSoundCntH[3] & 0x003F) - 1);

                // Disable the channel when the counter hits zero
                if ((gbaSoundCntH[3] & 0x003F) == 0)
                    gbaMainSoundCntX &= ~BIT(3);
            }

            // Run the envelope timer at 64Hz
            if (gbaFrameSequencer == 448 && --gbaEnvTimers[2] <= 0) {
                if (gbaEnvTimers[2] == 0) {
                    // Adjust the envelope volume if the timer period was non-zero
                    if ((gbaSoundCntH[3] & BIT(11)) && gbaEnvelopes[2] < 15)
                        gbaEnvelopes[2]++;
                    else if (!(gbaSoundCntH[3] & BIT(11)) && gbaEnvelopes[2] > 0)
                        gbaEnvelopes[2]--;
                }
                else {
                    // The envelope seems to reset with a period of zero?
                    gbaEnvelopes[2] = (gbaSoundCntH[3] & 0xF000) >> 12;
                }

                // Reload the envelope timer
                gbaEnvTimers[2] = (gbaSoundCntH[3] & 0x0700) >> 8;
            }

            // Apply the envelope volume
            data[3] = data[3] * gbaEnvelopes[2] / 15;
        }

        // Mix the PSG channels
        // The maximum volume is +/-0x80 per channel
        for (int i = 0; i < 4; i++) {
            // Apply the DMA mixing volume
            switch (gbaMainSoundCntH & 0x0003) {
                case 0: data[i] >>= 2; break;
                case 1: data[i] >>= 1; break;
                case 2: data[i] >>= 0; break;
            }

            // Add the data to the samples
            if (gbaMainSoundCntL & BIT(12 + i))
                sampleLeft += data[i] * ((gbaMainSoundCntL & 0x0070) >> 4) / 7;
            if (gbaMainSoundCntL & BIT(8 + i))
                sampleRight += data[i] * (gbaMainSoundCntL & 0x0007) / 7;
        }

        // Mix FIFO channel A
        // The maximum volume is +/-0x200, achieved by shifting the data left by 2
        if (gbaMainSoundCntH & BIT(9))
            sampleLeft += gbaSampleA << ((gbaMainSoundCntH & BIT(2)) ? 2 : 1);
        if (gbaMainSoundCntH & BIT(8))
            sampleRight += gbaSampleA << ((gbaMainSoundCntH & BIT(2)) ? 2 : 1);

        // Mix FIFO channel B
        // The maximum volume is +/-0x200, achieved by shifting the data left by 2
        if (gbaMainSoundCntH & BIT(13))
            sampleLeft += gbaSampleB << ((gbaMainSoundCntH & BIT(3)) ? 2 : 1);
        if (gbaMainSoundCntH & BIT(12))
            sampleRight += gbaSampleB << ((gbaMainSoundCntH & BIT(3)) ? 2 : 1);

        // Increment the frame sequencer
        // The frame sequencer runs at 512Hz, and has 8 steps before repeating
        // Audio is generated at 32768Hz, so every multiple of 64 is a new step
        gbaFrameSequencer = (gbaFrameSequencer + 1) % 512;
    }

    // Apply sound bias and clipping, and expand samples to signed 16-bit
    sampleLeft = (std::max(0, std::min<int32_t>(0x3FF, sampleLeft + (gbaSoundBias & 0x3FF))) - 0x200) << 6;
    sampleRight = (std::max(0, std::min<int32_t>(0x3FF, sampleRight + (gbaSoundBias & 0x3FF))) - 0x200) << 6;
    pushSample(sampleLeft, sampleRight);
}

void Spu::runSample() {
    // Push a dummy sample if disabled and schedule the next one
    core->schedule(NDS_SPU_SAMPLE, 512 * 2);
    if (!Settings::emulateAudio) return pushSample(0, 0);

    // Mix the sound channels
    int64_t mixerLeft = 0, mixerRight = 0;
    int64_t channelsLeft[2] = {}, channelsRight[2] = {};
    for (int i = 0; enabled >> i; i++) {
        // Skip disabled channels
        if (!(enabled & BIT(i))) continue;
        uint8_t format = (soundCnt[i] >> 29) & 0x3;
        int64_t data = 0;

        // Read the sample data
        switch (format) {
        case 0: // PCM8
            data = (int8_t)core->memory.read<uint8_t>(1, soundCurrent[i]) << 8;
            break;

        case 1: // PCM16
            data = (int16_t)core->memory.read<uint16_t>(1, soundCurrent[i]);
            break;

        case 2: // ADPCM
            data = adpcmValue[i];
            break;

        case 3: // Pulse/Noise
            if (i >= 8 && i <= 13) { // Pulse waves
                // Set the sample to low or high depending on the position in the duty cycle
                uint8_t duty = 7 - ((soundCnt[i] & 0x07000000) >> 24);
                data = (dutyCycles[i - 8] < duty) ? -0x7FFF : 0x7FFF;
            }
            else if (i >= 14) { // Noise
                // Set the sample to low or high depending on the carry bit (saved as bit 15)
                data = (noiseValues[i - 14] & BIT(15)) ? -0x7FFF : 0x7FFF;
            }
            break;
        }

        // Increment the timer for the length of a sample
        // The SPU runs at 16756991Hz with a sample rate of 32768Hz
        // 16756991 / 32768 = ~512 cycles per sample
        soundTimers[i] += 512;
        bool overflow = (soundTimers[i] < 512);

        // Handle timer overflow
        while (overflow) {
            // Reload the timer
            soundTimers[i] += soundTmr[i];
            overflow = (soundTimers[i] < soundTmr[i]);

            switch (format) {
            case 0: case 1: // PCM8/PCM16
                // Increment the data pointer by the size of one sample
                soundCurrent[i] += 1 + format;
                break;

            case 2: { // ADPCM
                // Save the ADPCM values at the loop position
                if (soundCurrent[i] == soundSad[i] + soundPnt[i] * 4 && !adpcmToggle[i]) {
                    adpcmLoopValue[i] = adpcmValue[i];
                    adpcmLoopIndex[i] = adpcmIndex[i];
                }

                // Get the 4-bit ADPCM data
                uint8_t adpcmData = core->memory.read<uint8_t>(1, soundCurrent[i]);
                adpcmData = adpcmToggle[i] ? ((adpcmData & 0xF0) >> 4) : (adpcmData & 0x0F);

                // Calculate the sample difference
                int32_t diff = adpcmTable[adpcmIndex[i]] / 8;
                if (adpcmData & BIT(0)) diff += adpcmTable[adpcmIndex[i]] / 4;
                if (adpcmData & BIT(1)) diff += adpcmTable[adpcmIndex[i]] / 2;
                if (adpcmData & BIT(2)) diff += adpcmTable[adpcmIndex[i]] / 1;

                // Apply the sample difference to the sample
                if (adpcmData & BIT(3)) {
                    adpcmValue[i] += diff;
                    if (adpcmValue[i] > 0x7FFF) adpcmValue[i] = 0x7FFF;
                }
                else {
                    adpcmValue[i] -= diff;
                    if (adpcmValue[i] < -0x7FFF) adpcmValue[i] = -0x7FFF;
                }

                // Calculate the next index
                adpcmIndex[i] += indexTable[adpcmData & 0x7];
                if (adpcmIndex[i] < 0) adpcmIndex[i] = 0;
                if (adpcmIndex[i] > 88) adpcmIndex[i] = 88;

                // Move to the next 4-bit ADPCM data
                adpcmToggle[i] = !adpcmToggle[i];
                if (!adpcmToggle[i]) soundCurrent[i]++;

                break;
            }

            case 3: // Pulse/Noise
                if (i >= 8 && i <= 13) { // Pulse waves
                    // Increment the duty cycle counter
                    dutyCycles[i - 8] = (dutyCycles[i - 8] + 1) % 8;
                }
                else if (i >= 14) { // Noise
                    // Clear the previous saved carry bit
                    noiseValues[i - 14] &= ~BIT(15);

                    // Advance the random generator and save the carry bit to bit 15
                    if (noiseValues[i - 14] & BIT(0))
                        noiseValues[i - 14] = BIT(15) | ((noiseValues[i - 14] >> 1) ^ 0x6000);
                    else
                        noiseValues[i - 14] >>= 1;
                }
                break;
            }

            // Repeat or end the sound if the end of the data is reached
            if (format != 3 && soundCurrent[i] >= soundSad[i] + (soundPnt[i] + soundLen[i]) * 4) {
                if ((soundCnt[i] & 0x18000000) >> 27 == 1) { // Loop infinite
                    soundCurrent[i] = soundSad[i] + soundPnt[i] * 4;

                    // Restore the ADPCM values from the loop position
                    if (format == 2) {
                        adpcmValue[i] = adpcmLoopValue[i];
                        adpcmIndex[i] = adpcmLoopIndex[i];
                        adpcmToggle[i] = false;
                    }
                }
                else { // One-shot
                    soundCnt[i] &= ~BIT(31);
                    enabled &= ~BIT(i);
                    break;
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
        // The samples are now rounded to 8 fractional bits
        int panValue = (soundCnt[i] & 0x007F0000) >> 16;
        if (panValue == 127) panValue++;
        int64_t dataLeft = (data * (128 - panValue) / 128) >> 3;
        int64_t dataRight = (data * panValue / 128) >> 3;

        // Redirect channels 1 and 3 if enabled
        if (i == 1 || i == 3) {
            channelsLeft[i >> 1] = dataLeft;
            channelsRight[i >> 1] = dataRight;
            if (mainSoundCnt & BIT(12 + (i >> 1)))
                continue;
        }

        // Add the channel to the mixer
        mixerLeft += dataLeft;
        mixerRight += dataRight;
    }

    // Capture sound
    for (int i = 0; i < 2; i++) {
        // Skip disabled capture channels
        if (!(sndCapCnt[i] & BIT(7)))
            continue;

        // Increment the timer for the length of a sample
        sndCapTimers[i] += 512;
        bool overflow = (sndCapTimers[i] < 512);

        // Handle timer overflow
        while (overflow) {
            // Reload the timer
            sndCapTimers[i] += soundTmr[1 + (i << 1)];
            overflow = (sndCapTimers[i] < soundTmr[1 + (i << 1)]);

            // Get a sample from the mixer, clamped to be within range
            int64_t sample = ((i == 0) ? mixerLeft : mixerRight);
            if (sample > 0x7FFFFF) sample = 0x7FFFFF;
            if (sample < -0x800000) sample = -0x800000;

            // Write a sample to the buffer
            if (sndCapCnt[i] & BIT(3)) { // PCM8
                core->memory.write<uint8_t>(1, sndCapCurrent[i], sample >> 16);
                sndCapCurrent[i]++;
            }
            else { // PCM16
                core->memory.write<uint16_t>(1, sndCapCurrent[i], sample >> 8);
                sndCapCurrent[i] += 2;
            }

            // Repeat or end the capture if the end of the buffer is reached
            if (sndCapCurrent[i] >= sndCapDad[i] + sndCapLen[i] * 4) {
                if (sndCapCnt[i] & BIT(2)) { // One-shot
                    sndCapCnt[i] &= ~BIT(7);
                    continue;
                }
                else { // Loop
                    sndCapCurrent[i] = sndCapDad[i];
                }
            }
        }
    }

    // Get the left output sample
    int64_t sampleLeft;
    switch ((mainSoundCnt & 0x0300) >> 8) { // Left output selection
        case 0: sampleLeft = mixerLeft; break; // Mixer
        case 1: sampleLeft = channelsLeft[0]; break; // Channel 1
        case 2: sampleLeft = channelsLeft[1]; break; // Channel 3
        case 3: sampleLeft = channelsLeft[0] + channelsLeft[1]; break; // Channel 1 + 3
    }

    // Get the right output sample
    int64_t sampleRight;
    switch ((mainSoundCnt & 0x0C00) >> 10) { // Right output selection
        case 0: sampleRight = mixerRight; break; // Mixer
        case 1: sampleRight = channelsRight[0]; break; // Channel 1
        case 2: sampleRight = channelsRight[1]; break; // Channel 3
        case 3: sampleRight = channelsRight[0] + channelsRight[1]; break; // Channel 1 + 3
    }

    // Apply the master volume
    // The samples are now rounded to no fractional bits
    int masterVol = (mainSoundCnt & 0x007F);
    if (masterVol == 127) masterVol++;
    sampleLeft = (sampleLeft * masterVol / 128) >> 8;
    sampleRight = (sampleRight * masterVol / 128) >> 8;

    // Process samples depending on audio settings
    if (Settings::audio16Bit) {
        // Apply sound bias and clipping, and convert to signed 16-bit
        sampleLeft = (std::max(0, std::min<int32_t>(0xFFFF, sampleLeft + (soundBias << 6))) - 0x8000);
        sampleRight = (std::max(0, std::min<int32_t>(0xFFFF, sampleRight + (soundBias << 6))) - 0x8000);
    }
    else {
        // Convert to 10-bit, apply sound bias and clipping, and expand to signed 16-bit
        sampleLeft = (std::max(0, std::min<int32_t>(0x3FF, (sampleLeft >> 6) + soundBias)) - 0x200) << 6;
        sampleRight = (std::max(0, std::min<int32_t>(0x3FF, (sampleRight >> 6) + soundBias)) - 0x200) << 6;
    }
    pushSample(sampleLeft, sampleRight);
}

void Spu::pushSample(int16_t sampleLeft, int16_t sampleRight) {
    // Write the samples to the buffer
    if (!bufferSize) return;
    bufferIn[bufferPointer++] = (sampleRight << 16) | (sampleLeft & 0xFFFF);
    if (bufferPointer != bufferSize) return;

    // Wait until the buffer has been played, keeping the emulator throttled to 60 FPS
    // Synchronizing to the audio eliminites the potential for nasty audio crackles
    if (Settings::fpsLimiter == 2) { // Accurate
        std::chrono::steady_clock::time_point waitTime = std::chrono::steady_clock::now();
        while (ready.load() && std::chrono::steady_clock::now() - waitTime <= std::chrono::microseconds(1000000));
    }
    else if (Settings::fpsLimiter == 1) { // Light
        std::unique_lock<std::mutex> lock(mutex1);
        cond1.wait_for(lock, std::chrono::microseconds(1000000), [&]{ return !ready.load(); });
    }

    // Swap the buffers
    SWAP(bufferOut, bufferIn);

    { // Signal that the buffer is ready to play
        std::lock_guard<std::mutex> guard(mutex2);
        ready.store(true);
        cond2.notify_one();
    }

    // Reset the buffer pointer
    bufferPointer = 0;
}

void Spu::startChannel(int channel) {
    // Reload the channel's internal registers
    soundCurrent[channel] = soundSad[channel];
    soundTimers[channel] = soundTmr[channel];

    switch ((soundCnt[channel] & 0x60000000) >> 29) { // Format
    case 2: { // ADPCM
        // Read the ADPCM header
        uint32_t header = core->memory.read<uint32_t>(1, soundSad[channel]);
        adpcmValue[channel] = (int16_t)header;
        adpcmIndex[channel] = (header & 0x007F0000) >> 16;
        if (adpcmIndex[channel] > 88) adpcmIndex[channel] = 88;
        adpcmToggle[channel] = false;
        soundCurrent[channel] += 4;
        break;
    }

    case 3: // Pulse/Noise
        // Reset the pulse or noise values
        if (channel >= 8 && channel <= 13) // Pulse waves
            dutyCycles[channel - 8] = 0;
        else if (channel >= 14) // Noise
            noiseValues[channel - 14] = 0x7FFF;
        break;
    }

    // Enable the channel
    enabled |= BIT(channel);
}

void Spu::gbaFifoTimer(int timer) {
    if (((gbaMainSoundCntH & BIT(10)) >> 10) == timer) { // FIFO A
        // Get a new sample
        if (!gbaFifos[0].empty()) {
            gbaSampleA = gbaFifos[0].front();
            gbaFifos[0].pop_front();
        }

        // Request more data from the DMA if half empty
        if (gbaFifos[0].size() <= 16)
            core->dma[1].trigger(3, 0x02);
    }

    if (((gbaMainSoundCntH & BIT(14)) >> 14) == timer) { // FIFO B
        // Get a new sample
        if (!gbaFifos[1].empty()) {
            gbaSampleB = gbaFifos[1].front();
            gbaFifos[1].pop_front();
        }

        // Request more data from the DMA if half empty
        if (gbaFifos[1].size() <= 16)
            core->dma[1].trigger(3, 0x04);
    }
}

void Spu::writeGbaSoundCntL(int channel, uint8_t value) {
    // Write to one of the GBA SOUNDCNT_L registers
    if (!(gbaMainSoundCntX & BIT(7))) return;
    uint8_t mask = (channel == 0) ? 0x7F : 0xE0;
    gbaSoundCntL[channel / 2] = (gbaSoundCntL[channel / 2] & ~mask) | (value & mask);
}

void Spu::writeGbaSoundCntH(int channel, uint16_t mask, uint16_t value) {
    // Write to one of the GBA SOUNDCNT_H registers
    if (!(gbaMainSoundCntX & BIT(7))) return;
    switch (channel) {
        case 2: mask &= 0xE0FF; break;
        case 3: mask &= 0xFF3F; break;
    }
    gbaSoundCntH[channel] = (gbaSoundCntH[channel] & ~mask) | (value & mask);
}

void Spu::writeGbaSoundCntX(int channel, uint16_t mask, uint16_t value) {
    // Write to one of the GBA SOUNDCNT_X registers
    if (!(gbaMainSoundCntX & BIT(7))) return;
    mask &= (channel == 3) ? 0x40FF : 0x47FF;
    gbaSoundCntX[channel] = (gbaSoundCntX[channel] & ~mask) | (value & mask);

    // Restart the channel if audio emulation is enabled
    if (!Settings::emulateAudio || !(value & BIT(15))) return;
    gbaMainSoundCntX |= BIT(channel);
    if (channel < 2) { // Tone
        if (channel == 0) gbaSweepTimer = (gbaSoundCntL[0] & 0x70) >> 4;
        gbaEnvelopes[channel] = (gbaSoundCntH[channel] & 0xF000) >> 12;
        gbaEnvTimers[channel] = (gbaSoundCntH[channel] & 0x0700) >> 8;
        gbaSoundTimers[channel] = 2048 - (gbaSoundCntX[channel] & 0x07FF);
    }
    else if (channel == 2) { // Wave
        gbaWaveDigit = 0;
        gbaSoundTimers[2] = 2048 - (gbaSoundCntX[2] & 0x07FF);
    }
    else { // Noise
        gbaNoiseValue = (gbaSoundCntH[3] & BIT(3)) ? 0x40 : 0x4000;
        gbaEnvelopes[2] = (gbaSoundCntH[3] & 0xF000) >> 12;
        gbaEnvTimers[2] = (gbaSoundCntH[3] & 0x0700) >> 8;

        int divisor = (gbaSoundCntX[3] & 0x0007) * 16;
        if (divisor == 0) divisor = 8;
        gbaSoundTimers[3] = (divisor << ((gbaSoundCntX[3] & 0x00F0) >> 4));
    }
}

void Spu::writeGbaMainSoundCntL(uint16_t mask, uint16_t value) {
    // Write to the main GBA SOUNDCNT_L register
    if (!(gbaMainSoundCntX & BIT(7))) return;
    mask &= 0xFF77;
    gbaMainSoundCntL = (gbaMainSoundCntL & ~mask) | (value & mask);
}

void Spu::writeGbaMainSoundCntH(uint16_t mask, uint16_t value) {
    // Write to the main GBA SOUNDCNT_H register
    mask &= 0x770F;
    gbaMainSoundCntH = (gbaMainSoundCntH & ~mask) | (value & mask);

    // Empty FIFO A if requested
    if (value & BIT(11)) {
        while (!gbaFifos[0].empty())
            gbaFifos[0].pop_front();
    }

    // Empty FIFO B if requested
    if (value & BIT(15)) {
        while (!gbaFifos[1].empty())
            gbaFifos[1].pop_front();
    }
}

void Spu::writeGbaMainSoundCntX(uint8_t value) {
    // Write to the main GBA SOUNDCNT_X register
    gbaMainSoundCntX = (gbaMainSoundCntX & ~0x80) | (value & 0x80);

    // Reset the PSG channels when disabled
    if (!(gbaMainSoundCntX & BIT(7))) {
        for (int i = 0; i < 4; i++) {
            if (i < 2) gbaSoundCntL[i] = 0;
            gbaSoundCntH[i] = 0;
            gbaSoundCntX[i] = 0;
        }
        gbaMainSoundCntL = 0;
        gbaMainSoundCntX &= ~0x0F;
        gbaFrameSequencer = 0;
    }
}

void Spu::writeGbaSoundBias(uint16_t mask, uint16_t value) {
    // Write to the GBA SOUNDBIAS register
    mask &= 0xC3FE;
    gbaSoundBias = (gbaSoundBias & ~mask) | (value & mask);
}

void Spu::writeGbaWaveRam(int index, uint8_t value) {
    // Write to the currently inactive GBA wave RAM bank
    gbaWaveRam[!(gbaSoundCntL[1] & BIT(6))][index] = value;
}

void Spu::writeGbaFifoA(uint32_t mask, uint32_t value) {
    // Push PCM8 data to the GBA sound FIFO A
    for (int i = 0; i < 32; i += 8)
        if (gbaFifos[0].size() < 32 && (mask & (0xFF << i)))
            gbaFifos[0].push_back(value >> i);
}

void Spu::writeGbaFifoB(uint32_t mask, uint32_t value) {
    // Push PCM8 data to the GBA sound FIFO B
    for (int i = 0; i < 32; i += 8)
        if (gbaFifos[1].size() < 32 && (mask & (0xFF << i)))
            gbaFifos[1].push_back(value >> i);
}

void Spu::writeSoundCnt(int channel, uint32_t mask, uint32_t value) {
    // Prevent channels from starting if audio emulation is disabled
    if (!Settings::emulateAudio) value &= ~BIT(31);
    bool enable = (!(soundCnt[channel] & BIT(31)) && (value & mask & BIT(31)));

    // Write to one of the SOUNDCNT registers
    mask &= 0xFF7F837F;
    soundCnt[channel] = (soundCnt[channel] & ~mask) | (value & mask);

    // Start the channel if the enable bit changes from 0 to 1 and the other conditions are met
    if (enable && (mainSoundCnt & BIT(15)) && (soundSad[channel] != 0 || ((soundCnt[channel] & 0x60000000) >> 29) == 3))
        startChannel(channel);
    else if (!(soundCnt[channel] & BIT(31)))
        enabled &= ~BIT(channel);
}

void Spu::writeSoundSad(int channel, uint32_t mask, uint32_t value) {
    // Write to one of the SOUNDSAD registers
    mask &= 0x07FFFFFC;
    soundSad[channel] = (soundSad[channel] & ~mask) | (value & mask);

    // Restart the channel if the source address is valid and the other conditions are met
    if (((soundCnt[channel] & 0x60000000) >> 29) != 3) { // Not pulse/noise
        if (soundSad[channel] != 0 && (mainSoundCnt & BIT(15)) && (soundCnt[channel] & BIT(31)))
            startChannel(channel);
        else
            enabled &= ~BIT(channel);
    }
}

void Spu::writeSoundTmr(int channel, uint16_t mask, uint16_t value) {
    // Write to one of the SOUNDTMR registers
    soundTmr[channel] = (soundTmr[channel] & ~mask) | (value & mask);
}

void Spu::writeSoundPnt(int channel, uint16_t mask, uint16_t value) {
    // Write to one of the SOUNDPNT registers
    soundPnt[channel] = (soundPnt[channel] & ~mask) | (value & mask);
}

void Spu::writeSoundLen(int channel, uint32_t mask, uint32_t value) {
    // Write to one of the SOUNDLEN registers
    mask &= 0x003FFFFF;
    soundLen[channel] = (soundLen[channel] & ~mask) | (value & mask);
}

void Spu::writeMainSoundCnt(uint16_t mask, uint16_t value) {
    // Write to the main SOUNDCNT register
    bool enable = (!(mainSoundCnt & BIT(15)) && (value & BIT(15)));
    mask &= 0xBF7F;
    mainSoundCnt = (mainSoundCnt & ~mask) | (value & mask);

    if (enable) {
        // Start the channels if the enable bit changes from 0 to 1 and the other conditions are met
        for (int i = 0; i < 16; i++) {
            if ((soundCnt[i] & BIT(31)) && (soundSad[i] != 0 || ((soundCnt[i] & 0x60000000) >> 29) == 3))
                startChannel(i);
        }
    }
    else if (!(mainSoundCnt & BIT(15))) {
        // Disable all channels if the master enable is turned off
        enabled = 0;
    }
}

void Spu::writeSoundBias(uint16_t mask, uint16_t value) {
    // Write to the SOUNDBIAS register
    mask &= 0x03FF;
    soundBias = (soundBias & ~mask) | (value & mask);
}

void Spu::writeSndCapCnt(int channel, uint8_t value) {
    // Start the capture if the enable bit changes from 0 to 1
    if (!(sndCapCnt[channel] & BIT(7)) && (value & BIT(7))) {
        sndCapCurrent[channel] = sndCapDad[channel];
        sndCapTimers[channel] = soundTmr[1 + (channel << 1)];
    }

    // Write to one of the SNDCAPCNT registers
    sndCapCnt[channel] = (value & 0x8F);
}

void Spu::writeSndCapDad(int channel, uint32_t mask, uint32_t value) {
    // Write to one of the SNDCAPDAD registers
    mask &= 0x07FFFFFC;
    sndCapDad[channel] = (sndCapDad[channel] & ~mask) | (value & mask);

    // Restart the capture
    sndCapCurrent[channel] = sndCapDad[channel];
    sndCapTimers[channel] = soundTmr[1 + (channel << 1)];
}

void Spu::writeSndCapLen(int channel, uint16_t mask, uint16_t value) {
    // Write to one of the SNDCAPLEN registers
    sndCapLen[channel] = (sndCapLen[channel] & ~mask) | (value & mask);
}

uint8_t Spu::readGbaSoundCntL(int channel) {
    // Read from one of the GBA SOUNDCNT_L registers
    // There are only two of these, on channels 0 and 2
    return gbaSoundCntL[channel / 2];
}

uint16_t Spu::readGbaSoundCntH(int channel) {
    // Read from one of the GBA SOUNDCNT_H registers
    // The sound length is write-only, so mask it out
    return gbaSoundCntH[channel] & ~((channel == 2) ? 0x00FF : 0x003F);

}
uint16_t Spu::readGbaSoundCntX(int channel) {
    // Read from one of the GBA SOUNDCNT_X registers
    // The frequency is write-only, so mask it out
    return gbaSoundCntX[channel] & ~((channel == 3) ? 0x0000 : 0x07FF);
}

uint8_t Spu::readGbaWaveRam(int index) {
    // Read from the currently inactive GBA wave RAM bank
    return gbaWaveRam[!(gbaSoundCntL[1] & BIT(6))][index];
}
