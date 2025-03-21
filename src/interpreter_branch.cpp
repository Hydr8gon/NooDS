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

#include "interpreter.h"
#include "core.h"

int Interpreter::bx(uint32_t opcode) { // BX Rn
    // Branch to address and switch to THUMB if bit 0 is set
    uint32_t op0 = *registers[opcode & 0xF];
    cpsr |= (op0 & BIT(0)) << 5;
    *registers[15] = op0;
    flushPipeline();
    return 3;
}

int Interpreter::blxReg(uint32_t opcode) { // BLX Rn
    // Branch to address with link and switch to THUMB if bit 0 is set
    if (arm7) return 1; // ARM9 exclusive
    uint32_t op0 = *registers[opcode & 0xF];
    cpsr |= (op0 & BIT(0)) << 5;
    *registers[14] = *registers[15] - 4;
    *registers[15] = op0;
    flushPipeline();
    return 3;
}

int Interpreter::b(uint32_t opcode) { // B label
    // Branch to offset
    int32_t op0 = (int32_t)(opcode << 8) >> 6;
    *registers[15] += op0;
    flushPipeline();
    return 3;
}

int Interpreter::bl(uint32_t opcode) { // BL label
    // Branch to offset with link
    int32_t op0 = (int32_t)(opcode << 8) >> 6;
    *registers[14] = *registers[15] - 4;
    *registers[15] += op0;
    flushPipeline();
    return 3;
}

int Interpreter::blx(uint32_t opcode) { // BLX label
    // Branch to offset with link and switch to THUMB
    if (arm7) return 1; // ARM9 exclusive
    int32_t op0 = ((int32_t)(opcode << 8) >> 6) | ((opcode & BIT(24)) >> 23);
    cpsr |= BIT(5);
    *registers[14] = *registers[15] - 4;
    *registers[15] += op0;
    flushPipeline();
    return 3;
}

int Interpreter::swi(uint32_t opcode) { // SWI #i
    // Software interrupt
    LOG_INFO("Triggering ARM%d software interrupt: 0x%X\n", arm7 ? 7 : 9, opcode & 0xFFFFFF);
    *registers[15] -= 4;
    return exception(0x08);
}

int Interpreter::bxRegT(uint16_t opcode) { // BX Rs
    // Branch to address and switch to ARM mode if bit 0 is cleared (THUMB)
    uint32_t op0 = *registers[(opcode >> 3) & 0xF];
    cpsr &= ~((~op0 & BIT(0)) << 5);
    *registers[15] = op0;
    flushPipeline();
    return 3;
}

int Interpreter::blxRegT(uint16_t opcode) { // BLX Rs
    // Branch to address with link and switch to ARM mode if bit 0 is cleared (THUMB)
    if (arm7) return 1; // ARM9 exclusive
    uint32_t op0 = *registers[(opcode >> 3) & 0xF];
    cpsr &= ~((~op0 & BIT(0)) << 5);
    *registers[14] = *registers[15] - 1;
    *registers[15] = op0;
    flushPipeline();
    return 3;
}

int Interpreter::beqT(uint16_t opcode) { // BEQ label
    // Branch to offset if equal (THUMB)
    int32_t op0 = (int8_t)opcode << 1;
    if (~cpsr & BIT(30)) return 1;
    *registers[15] += op0;
    flushPipeline();
    return 3;
}

int Interpreter::bneT(uint16_t opcode) { // BNE label
    // Branch to offset if not equal (THUMB)
    int32_t op0 = (int8_t)opcode << 1;
    if (cpsr & BIT(30)) return 1;
    *registers[15] += op0;
    flushPipeline();
    return 3;
}

int Interpreter::bcsT(uint16_t opcode) { // BCS label
    // Branch to offset if carry set (THUMB)
    int32_t op0 = (int8_t)opcode << 1;
    if (~cpsr & BIT(29)) return 1;
    *registers[15] += op0;
    flushPipeline();
    return 3;
}

int Interpreter::bccT(uint16_t opcode) { // BCC label
    // Branch to offset if carry clear (THUMB)
    int32_t op0 = (int8_t)opcode << 1;
    if (cpsr & BIT(29)) return 1;
    *registers[15] += op0;
    flushPipeline();
    return 3;
}

int Interpreter::bmiT(uint16_t opcode) { // BMI label
    // Branch to offset if negative (THUMB)
    int32_t op0 = (int8_t)opcode << 1;
    if (~cpsr & BIT(31)) return 1;
    *registers[15] += op0;
    flushPipeline();
    return 3;
}

int Interpreter::bplT(uint16_t opcode) { // BPL label
    // Branch to offset if positive (THUMB)
    int32_t op0 = (int8_t)opcode << 1;
    if (cpsr & BIT(31)) return 1;
    *registers[15] += op0;
    flushPipeline();
    return 3;
}

int Interpreter::bvsT(uint16_t opcode) { // BVS label
    // Branch to offset if overflow set (THUMB)
    int32_t op0 = (int8_t)opcode << 1;
    if (~cpsr & BIT(28)) return 1;
    *registers[15] += op0;
    flushPipeline();
    return 3;
}

int Interpreter::bvcT(uint16_t opcode) { // BVC label
    // Branch to offset if overflow clear (THUMB)
    int32_t op0 = (int8_t)opcode << 1;
    if (cpsr & BIT(28)) return 1;
    *registers[15] += op0;
    flushPipeline();
    return 3;
}

int Interpreter::bhiT(uint16_t opcode) { // BHI label
    // Branch to offset if higher (THUMB)
    int32_t op0 = (int8_t)opcode << 1;
    if ((cpsr & 0x60000000) != 0x20000000) return 1;
    *registers[15] += op0;
    flushPipeline();
    return 3;
}

int Interpreter::blsT(uint16_t opcode) { // BLS label
    // Branch to offset if lower or same (THUMB)
    int32_t op0 = (int8_t)opcode << 1;
    if ((cpsr & 0x60000000) == 0x20000000) return 1;
    *registers[15] += op0;
    flushPipeline();
    return 3;
}

int Interpreter::bgeT(uint16_t opcode) { // BGE label
    // Branch to offset if signed greater or equal (THUMB)
    int32_t op0 = (int8_t)opcode << 1;
    if ((cpsr ^ (cpsr << 3)) & BIT(31)) return 1;
    *registers[15] += op0;
    flushPipeline();
    return 3;
}

int Interpreter::bltT(uint16_t opcode) { // BLT label
    // Branch to offset if signed less than (THUMB)
    int32_t op0 = (int8_t)opcode << 1;
    if (~(cpsr ^ (cpsr << 3)) & BIT(31)) return 1;
    *registers[15] += op0;
    flushPipeline();
    return 3;
}

int Interpreter::bgtT(uint16_t opcode) { // BGT label
    // Branch to offset if signed greater than (THUMB)
    int32_t op0 = (int8_t)opcode << 1;
    if (((cpsr ^ (cpsr << 3)) | (cpsr << 1)) & BIT(31)) return 1;
    *registers[15] += op0;
    flushPipeline();
    return 3;
}

int Interpreter::bleT(uint16_t opcode) { // BLE label
    // Branch to offset if signed less or equal (THUMB)
    int32_t op0 = (int8_t)opcode << 1;
    if (~((cpsr ^ (cpsr << 3)) | (cpsr << 1)) & BIT(31)) return 1;
    *registers[15] += op0;
    flushPipeline();
    return 3;
}

int Interpreter::bT(uint16_t opcode) { // B label
    // Branch to offset (THUMB)
    int32_t op0 = (int16_t)(opcode << 5) >> 4;
    *registers[15] += op0;
    flushPipeline();
    return 3;
}

int Interpreter::blSetupT(uint16_t opcode) { // BL/BLX label
    // Set the upper 11 bits of the target address for a long BL/BLX (THUMB)
    int32_t op0 = (int16_t)(opcode << 5) >> 4;
    *registers[14] = *registers[15] + (op0 << 11);
    return 1;
}

int Interpreter::blOffT(uint16_t opcode) { // BL label
    // Long branch to offset with link (THUMB)
    uint32_t op0 = (opcode & 0x7FF) << 1;
    uint32_t ret = *registers[15] - 1;
    *registers[15] = *registers[14] + op0;
    *registers[14] = ret;
    flushPipeline();
    return 3;
}

int Interpreter::blxOffT(uint16_t opcode) { // BLX label
    // Long branch to offset with link and switch to ARM mode (THUMB)
    if (arm7) return 1; // ARM9 exclusive
    uint32_t op0 = (opcode & 0x7FF) << 1;
    cpsr &= ~BIT(5);
    uint32_t ret = *registers[15] - 1;
    *registers[15] = *registers[14] + op0;
    *registers[14] = ret;
    flushPipeline();
    return 3;
}

int Interpreter::swiT(uint16_t opcode) { // SWI #i
    // Software interrupt (THUMB)
    LOG_INFO("Triggering ARM%d software interrupt: 0x%X\n", arm7 ? 7 : 9, opcode & 0xFF);
    *registers[15] -= 4;
    return exception(0x08);
}
