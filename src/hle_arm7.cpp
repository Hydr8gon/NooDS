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

#include "hle_arm7.h"
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
}

void HleArm7::loadState(FILE *file) {
    // Read state data from the file
    fread(&inited, 1, sizeof(inited), file);
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
    // Stub FIFO commands by replying with the same value
    if (!inited) return;
    LOG_CRIT("Unknown HLE IPC FIFO command: 0x%X\n", value);
    core->ipc.writeIpcFifoSend(1, -1, value);
}
