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

#include "core.h"

void HleArm7::init() {
    // Permanently halt the ARM7 and set initial IPC state
    core->interpreter[1].halt(2);
    core->ipc.writeIpcSync(1, -1, 0x700);
    core->ipc.writeIpcFifoCnt(1, -1, 0x8000);
}

void HleArm7::saveState(FILE *file) {
    // Write state data to the file
    fwrite(&inited, 1, sizeof(inited), file);
    fwrite(&autoTouch, 1, sizeof(autoTouch), file);
}

void HleArm7::loadState(FILE *file) {
    // Read state data from the file
    fread(&inited, 1, sizeof(inited), file);
    fread(&autoTouch, 1, sizeof(autoTouch), file);
}

void HleArm7::ipcSync(uint8_t value) {
    // Catch unhandled HLE IPC sync requests
    if (inited) {
        LOG_CRIT("Unhandled HLE IPC sync sent after initialization\n");
        return;
    }

    // During init, decrement the sync value and send it back
    if (value > 0)
        return core->ipc.writeIpcSync(1, -1, (value - 1) << 8);

    // Set subsystem init flags and finish the init process
    core->memory.write<uint32_t>(1, 0x27FFF8C, 0x3FFF0);
    inited = true;
}

void HleArm7::ipcFifo(uint32_t value) {
    // Handle FIFO commands based on the subsystem tag
    if (!inited) return;
    switch (value & 0x1F) { // Subsystem
    case 0x6: // Touch screen
        // Poll touch input manually or enable auto-polling
        if ((value & 0xC0000000) == 0xC0000000) {
            pollTouch(value | BIT(21));
        }
        else if ((value & 0xC0000000) == 0x40000000) {
            autoTouch = (value & BIT(22));
            pollTouch(0xC0204006);
        }
        return;

    default:
        // Stub unknown FIFO commands by replying with the same value
        LOG_CRIT("Unknown HLE IPC FIFO command: 0x%X\n", value);
        return core->ipc.writeIpcFifoSend(1, -1, value);
    }
}

void HleArm7::runFrame() {
    // Automatically poll extra keys and touch if enabled
    if (!inited) return;
    core->memory.write<uint16_t>(1, 0x27FFFA8, (core->input.readExtKeyIn() & 0xB) << 10);
    if (autoTouch) pollTouch(0xC0240006);
}

void HleArm7::pollTouch(uint32_t value) {
    // Update touch values in shared memory and send a FIFO reply
    if (core->input.readExtKeyIn() & BIT(6)) { // Released
        core->memory.write<uint16_t>(1, 0x27FFFAA, 0x000);
        core->memory.write<uint16_t>(1, 0x27FFFAC, 0x600);
    }
    else { // Pressed
        core->memory.write<uint16_t>(1, 0x27FFFAA, (core->spi.touchX >> 0));
        core->memory.write<uint16_t>(1, 0x27FFFAC, (core->spi.touchY >> 4) | 0x100);
    }
    core->ipc.writeIpcFifoSend(1, -1, value);
}
