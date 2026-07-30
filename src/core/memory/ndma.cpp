/*
    Copyright 2019-2026 Hydr8gon

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

#include "../core.h"

void Ndma::saveState(FILE *file) {
    // Write DSi state data to the file
    if (!core->dsiMode) return;
    fwrite(srcAddrs, 4, sizeof(srcAddrs) / 4, file);
    fwrite(dstAddrs, 4, sizeof(dstAddrs) / 4, file);
    fwrite(&drqMask, sizeof(drqMask), 1, file);
    fwrite(&runMask, sizeof(runMask), 1, file);
    fwrite(ndmaSad, 4, sizeof(ndmaSad) / 4, file);
    fwrite(ndmaDad, 4, sizeof(ndmaDad) / 4, file);
    fwrite(ndmaTcnt, 4, sizeof(ndmaTcnt) / 4, file);
    fwrite(ndmaWcnt, 4, sizeof(ndmaWcnt) / 4, file);
    fwrite(ndmaFdata, 4, sizeof(ndmaFdata) / 4, file);
    fwrite(ndmaCnt, 4, sizeof(ndmaCnt) / 4, file);
}

void Ndma::loadState(FILE *file) {
    // Read DSi state data from the file
    if (!core->dsiMode) return;
    fread(srcAddrs, 4, sizeof(srcAddrs) / 4, file);
    fread(dstAddrs, 4, sizeof(dstAddrs) / 4, file);
    fread(&drqMask, sizeof(drqMask), 1, file);
    fread(&runMask, sizeof(runMask), 1, file);
    fread(ndmaSad, 4, sizeof(ndmaSad) / 4, file);
    fread(ndmaDad, 4, sizeof(ndmaDad) / 4, file);
    fread(ndmaTcnt, 4, sizeof(ndmaTcnt) / 4, file);
    fread(ndmaWcnt, 4, sizeof(ndmaWcnt) / 4, file);
    fread(ndmaFdata, 4, sizeof(ndmaFdata) / 4, file);
    fread(ndmaCnt, 4, sizeof(ndmaCnt) / 4, file);
}

bool Ndma::shouldTransfer(int i, uint8_t type) {
    // Check if a channel is enabled and using the triggered DRQ type
    if (~ndmaCnt[i] & BIT(31)) return false;
    return type == ((ndmaCnt[i] >> 24) & 0x1F);
}

void Ndma::setDrq(uint8_t type) {
    // Set a DRQ type's active bit
    drqMask |= BIT(type);
    bool scheduled = runMask;

    // Schedule transfers on newly activated channels
    for (int i = 0; i < 4; i++)
        if (shouldTransfer(i, type)) runMask |= BIT(i);
    if (!scheduled && runMask)
        core->schedule(SchedTask(NDMA9_UPDATE + arm7), 1);
}

void Ndma::clearDrq(uint8_t type) {
    // Clear a DRQ type's active bit
    drqMask &= ~BIT(type);
}

void Ndma::update() {
    // Perform scheduled transfers and acknowledge them
    for (int i = 0; runMask >> i; i++) {
        if (~runMask & BIT(i)) continue;
        transferBlock(i);
    }
    runMask = 0;
}

void Ndma::transferBlock(int i) {
    // Set the destination address step or handle special cases
    int dstStep;
    switch ((ndmaCnt[i] >> 10) & 0x3) {
        case 0: dstStep = 4; break; // Increment
        case 1: dstStep = -4; break; // Decrement
        case 2: dstStep = 0; break; // Fixed

    default: // Reserved
        LOG_CRIT("ARM%d NDMA channel %d triggered with unhandled destination step\n", arm7 ? 7 : 9, i);
        return;
    }

    // Set the source address step or handle special cases
    int srcStep;
    switch ((ndmaCnt[i] >> 13) & 0x3) {
        case 0: srcStep = 4; break; // Increment
        case 1: srcStep = -4; break; // Decrement
        case 2: srcStep = 0; break; // Fixed

    default: // Fill
        srcAddrs[i] = 0x4004118 + i * 0x1C;
        srcStep = 0;
        break;
    }

    // Perform an NDMA transfer based on the repeat mode
    uint32_t count = ndmaWcnt[i] ? ndmaWcnt[i] : 0x1000000;
    if (ndmaCnt[i] & (BIT(28) | BIT(29))) { // Immediate/infinite
        // Transfer a block and adjust source and destination addresses
        while (count--) {
            uint32_t value = core->memory.read<uint32_t>(arm7, srcAddrs[i], false);
            core->memory.write<uint32_t>(arm7, dstAddrs[i], value, false);
            srcAddrs[i] += srcStep;
            dstAddrs[i] += dstStep;
        }

        // Trigger an interrupt if enabled and end if immediate
        if (ndmaCnt[i] & BIT(30))
            core->interpreter[arm7].sendInterrupt(28 + i);
        if (ndmaCnt[i] & BIT(28))
            ndmaCnt[i] &= ~BIT(31);
    }
    else { // Repeat until total
        while (count--) {
            // Transfer a word and adjust source and destination addresses
            uint32_t value = core->memory.read<uint32_t>(arm7, srcAddrs[i], false);
            core->memory.write<uint32_t>(arm7, dstAddrs[i], value, false);
            srcAddrs[i] += srcStep;
            dstAddrs[i] += dstStep;

            // Trigger an interrupt and end if the word total is reached
            if (--ndmaTcnt[i] &= 0xFFFFFFF) continue;
            LOG_INFO("ARM%d NDMA channel %d finished transferring\n", arm7 ? 7 : 9, i);
            if (ndmaCnt[i] & BIT(30))
                core->interpreter[arm7].sendInterrupt(28 + i);
            ndmaCnt[i] &= ~BIT(31);
            break;
        }
    }

    // Reload destination and source addresses if enabled
    if (ndmaCnt[i] & BIT(12)) dstAddrs[i] = ndmaDad[i];
    if (ndmaCnt[i] & BIT(15)) srcAddrs[i] = ndmaSad[i];
}

void Ndma::writeSad(int i, uint32_t mask, uint32_t value) {
    // Write to one of the NDMAxSAD registers
    mask &= 0xFFFFFFFC;
    ndmaSad[i] = (ndmaSad[i] & ~mask) | (value & mask);
}

void Ndma::writeDad(int i, uint32_t mask, uint32_t value) {
    // Write to one of the NDMAxDAD registers
    mask &= 0xFFFFFFFC;
    ndmaDad[i] = (ndmaDad[i] & ~mask) | (value & mask);
}

void Ndma::writeTcnt(int i, uint32_t mask, uint32_t value) {
    // Write to one of the NDMAxTCNT registers
    mask &= 0xFFFFFFF;
    ndmaTcnt[i] = (ndmaTcnt[i] & ~mask) | (value & mask);
}

void Ndma::writeWcnt(int i, uint32_t mask, uint32_t value) {
    // Write to one of the NDMAxWCNT registers
    mask &= 0xFFFFFF;
    ndmaWcnt[i] = (ndmaWcnt[i] & ~mask) | (value & mask);
}

void Ndma::writeFdata(int i, uint32_t mask, uint32_t value) {
    // Write to one of the NDMAxFDATA registers
    ndmaFdata[i] = (ndmaFdata[i] & ~mask) | (value & mask);
}

void Ndma::writeCnt(int i, uint32_t mask, uint32_t value) {
    // Write to one of the NDMAxCNT registers and check if a transfer started
    mask &= 0xFF0FFFFF;
    uint32_t old = ndmaCnt[i];
    ndmaCnt[i] = (ndmaCnt[i] & ~mask) | (value & mask);
    if (!(~old & ndmaCnt[i] & BIT(31))) return;

    // Reload internal addresses and trigger immediate transfers right away
    srcAddrs[i] = ndmaSad[i];
    dstAddrs[i] = ndmaDad[i];
    uint8_t mode = (ndmaCnt[i] >> 24) & 0x1F;
    if (mode >= 0x10) setDrq(mode);

    // Log started channel information and catch unimplemented modes
    if ((!arm7 || (mode != 0x8 && mode != 0xA && mode != 0xB)) && mode < 0x10)
        LOG_CRIT("ARM%d NDMA channel %d started in unimplemented mode: 0x%X\n", arm7 ? 7 : 9, i, mode);
    else
        LOG_INFO("ARM%d NDMA channel %d starting in mode 0x%X, transferring from 0x%X to 0x%X with size 0x%X in "
            "blocks of 0x%X\n", arm7 ? 7 : 9, i, mode, ndmaSad[i], ndmaDad[i], ndmaTcnt[i] << 2, ndmaWcnt[i] << 2);
}
