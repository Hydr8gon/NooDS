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

void I2c::saveState(FILE *file) {
    // Write DSi state data to the file
    if (!core->dsiMode) return;
    fwrite(&writeCount, sizeof(writeCount), 1, file);
    fwrite(&devAddr, sizeof(devAddr), 1, file);
    fwrite(&regAddr, sizeof(regAddr), 1, file);
    fwrite(&i2cData, sizeof(i2cData), 1, file);
    fwrite(&i2cCnt, sizeof(i2cCnt), 1, file);
}

void I2c::loadState(FILE *file) {
    // Read DSi state data from the file
    if (!core->dsiMode) return;
    fread(&writeCount, sizeof(writeCount), 1, file);
    fread(&devAddr, sizeof(devAddr), 1, file);
    fread(&regAddr, sizeof(regAddr), 1, file);
    fread(&i2cData, sizeof(i2cData), 1, file);
    fread(&i2cCnt, sizeof(i2cCnt), 1, file);
}

uint8_t I2c::readMcu() {
    // Read from an MCU register and increment the address
    switch (regAddr++) {
        case 0x20: return 0xF; // Battery (full)

    default:
        // Catch reads from unknown MCU registers
        LOG_WARN("Unknown I2C MCU read from register 0x%X\n", regAddr - 1);
        return 0;
    }
}

void I2c::writeMcu(uint8_t value) {
    // Set the 8-bit MCU address on second write
    if (writeCount == 2) {
        regAddr = value;
        return;
    }

    // Write to an MCU register and increment the address
    switch (regAddr++) {
    default:
        // Catch writes to unknown MCU registers
        LOG_WARN("Unknown I2C MCU write to register 0x%X\n", regAddr - 1);
        return;
    }
}

void I2c::writeData(uint8_t value) {
    // Write to the I2C_DATA register
    i2cData = value;
}

void I2c::writeCnt(uint8_t value) {
    // Write to the I2C_CNT register and check some bits
    i2cCnt = value & 0x7F;
    if (~value & BIT(7)) return; // Enable
    if (value & BIT(1)) writeCount = 0; // Start

    // Trigger an I2C interrupt if enabled
    if (value & BIT(6))
        core->interpreter[1].sendInterrupt(45);

    // Forward I2C reads to the addressed device
    if (value & BIT(5)) { // Direction
        switch (devAddr) {
            case 0x4B: i2cData = readMcu(); return;

        default:
            // Catch reads from unknown devices
            LOG_WARN("Unknown I2C read from device 0x%X\n", devAddr);
            i2cData = 0;
            return;
        }
    }

    // Set the device address on first write
    i2cCnt |= BIT(4); // Acknowledge
    if (++writeCount == 1) {
        devAddr = i2cData;
        return;
    }

    // Forward I2C writes to the addressed device
    switch (devAddr) {
        case 0x4A: return writeMcu(i2cData);

    default:
        // Catch writes to unknown devices
        LOG_WARN("Unknown I2C write to device 0x%X\n", devAddr);
        return;
    }
}
