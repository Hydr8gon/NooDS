/*
    Copyright 2019-2021 Hydr8gon

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

#ifndef INTERPRETER_TRANSFER
#define INTERPRETER_TRANSFER

#include "core.h"

FORCE_INLINE uint32_t Interpreter::ip(uint32_t opcode) // #i (B/_)
{
    // Immediate offset for byte and word transfers
    return opcode & 0x00000FFF;
}

FORCE_INLINE uint32_t Interpreter::ipH(uint32_t opcode) // #i (SB/SH/H/D)
{
    // Immediate offset for signed, half-word, and double word transfers
    return ((opcode & 0x00000F00) >> 4) | (opcode & 0x0000000F);
}

FORCE_INLINE uint32_t Interpreter::rp(uint32_t opcode) // Rm
{
    // Register offset for signed and half-word transfers
    return *registers[opcode & 0x0000000F];
}

FORCE_INLINE uint32_t Interpreter::rpll(uint32_t opcode) // Rm,LSL #i
{
    // Decode the operands
    uint32_t value = *registers[opcode & 0x0000000F];
    uint8_t shift = (opcode & 0x00000F80) >> 7;

    // Logical shift left by immediate
    return value << shift;
}

FORCE_INLINE uint32_t Interpreter::rplr(uint32_t opcode) // Rm,LSR #i
{
    // Decode the operands
    uint32_t value = *registers[opcode & 0x0000000F];
    uint8_t shift = (opcode & 0x00000F80) >> 7;

    // Logical shift right by immediate
    // A shift of 0 translates to a shift of 32
    return shift ? (value >> shift) : 0;
}

FORCE_INLINE uint32_t Interpreter::rpar(uint32_t opcode) // Rm,ASR #i
{
    // Decode the operands
    uint32_t value = *registers[opcode & 0x0000000F];
    uint8_t shift = (opcode & 0x00000F80) >> 7;

    // Arithmetic shift right by immediate
    // A shift of 0 translates to a shift of 32
    return shift ? ((int32_t)value >> shift) : ((value & BIT(31)) ? 0xFFFFFFFF : 0);
}

FORCE_INLINE uint32_t Interpreter::rprr(uint32_t opcode) // Rm,ROR #i
{
    // Decode the operands
    uint32_t value = *registers[opcode & 0x0000000F];
    uint8_t shift = (opcode & 0x00000F80) >> 7;

    // Rotate right by immediate
    // A shift of 0 translates to a rotate with carry of 1
    return shift ? ((value << (32 - shift)) | (value >> shift)) : (((cpsr & BIT(29)) << 2) | (value >> 1));
}

FORCE_INLINE int Interpreter::ldrsbOf(uint32_t opcode, uint32_t op2) // LDRSB Rd,[Rn,op2]
{
    // Decode the other operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16];

    // Signed byte load, pre-adjust without writeback
    *op0 = core->memory.read<int8_t>(cpu, op1 + op2);

    // Handle pipelining
    if (op0 != registers[15]) return ((cpu == 0) ? 1 : 3);
    *registers[15] = (*registers[15] & ~3) + 4;
    return 5;
}

FORCE_INLINE int Interpreter::ldrshOf(uint32_t opcode, uint32_t op2) // LDRSH Rd,[Rn,op2]
{
    // Decode the other operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16];

    // Signed half-word load, pre-adjust without writeback
    *op0 = core->memory.read<int16_t>(cpu, op1 += op2);

    // Shift misaligned reads on ARM7
    if (cpu == 1 && (op1 & 1))
        *op0 = (int16_t)*op0 >> 8;

    // Handle pipelining
    if (op0 != registers[15]) return ((cpu == 0) ? 1 : 3);
    *registers[15] = (*registers[15] & ~3) + 4;
    return 5;
}

FORCE_INLINE int Interpreter::ldrbOf(uint32_t opcode, uint32_t op2) // LDRB Rd,[Rn,op2]
{
    // Decode the other operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16];

    // Byte load, pre-adjust without writeback
    *op0 = core->memory.read<uint8_t>(cpu, op1 + op2);

    if (op0 != registers[15])
        return ((cpu == 0) ? 1 : 3);

    // Handle pipelining and THUMB switching
    if (cpu == 0 && (*op0 & BIT(0)))
    {
        cpsr |= BIT(5);
        *registers[15] = (*registers[15] & ~1) + 2;
    }
    else
    {
        *registers[15] = (*registers[15] & ~3) + 4;
    }

    return 5;
}

FORCE_INLINE int Interpreter::strbOf(uint32_t opcode, uint32_t op2) // STRB Rd,[Rn,op2]
{
    // Decode the other operands
    // When used as Rd, the program counter is read with +4
    uint32_t op0 = *registers[(opcode & 0x0000F000) >> 12] + (((opcode & 0x0000F000) == 0x0000F000) ? 4 : 0);
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16];

    // Byte store, pre-adjust without writeback
    core->memory.write<uint8_t>(cpu, op1 + op2, op0);

    return ((cpu == 0) ? 1 : 2);
}

FORCE_INLINE int Interpreter::ldrhOf(uint32_t opcode, uint32_t op2) // LDRH Rd,[Rn,op2]
{
    // Decode the other operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16];

    // Half-word load, pre-adjust without writeback
    *op0 = core->memory.read<uint16_t>(cpu, op1 += op2);

    // Rotate misaligned reads on ARM7
    if (cpu == 1 && (op1 & 1))
        *op0 = (*op0 << 24) | (*op0 >> 8);

    // Handle pipelining
    if (op0 != registers[15]) return ((cpu == 0) ? 1 : 3);
    *registers[15] = (*registers[15] & ~3) + 4;
    return 5;
}

FORCE_INLINE int Interpreter::strhOf(uint32_t opcode, uint32_t op2) // STRH Rd,[Rn,op2]
{
    // Decode the other operands
    // When used as Rd, the program counter is read with +4
    uint32_t op0 = *registers[(opcode & 0x0000F000) >> 12] + (((opcode & 0x0000F000) == 0x0000F000) ? 4 : 0);
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16];

    // Half-word store, pre-adjust without writeback
    core->memory.write<uint16_t>(cpu, op1 + op2, op0);

    return ((cpu == 0) ? 1 : 2);
}

FORCE_INLINE int Interpreter::ldrOf(uint32_t opcode, uint32_t op2) // LDR Rd,[Rn,op2]
{
    // Decode the other operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16];

    // Word load, pre-adjust without writeback
    *op0 = core->memory.read<uint32_t>(cpu, op1 += op2);

    // Rotate misaligned reads
    if (op1 & 3)
    {
        int shift = (op1 & 3) * 8;
        *op0 = (*op0 << (32 - shift)) | (*op0 >> shift);
    }

    if (op0 != registers[15])
        return ((cpu == 0) ? 1 : 3);

    // Handle pipelining and THUMB switching
    if (cpu == 0 && (*op0 & BIT(0)))
    {
        cpsr |= BIT(5);
        *registers[15] = (*registers[15] & ~1) + 2;
    }
    else
    {
        *registers[15] = (*registers[15] & ~3) + 4;
    }

    return 5;
}

FORCE_INLINE int Interpreter::strOf(uint32_t opcode, uint32_t op2) // STR Rd,[Rn,op2]
{
    // Decode the other operands
    // When used as Rd, the program counter is read with +4
    uint32_t op0 = *registers[(opcode & 0x0000F000) >> 12] + (((opcode & 0x0000F000) == 0x0000F000) ? 4 : 0);
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16];

    // Word store, pre-adjust without writeback
    core->memory.write<uint32_t>(cpu, op1 + op2, op0);

    return ((cpu == 0) ? 1 : 2);
}

FORCE_INLINE int Interpreter::ldrdOf(uint32_t opcode, uint32_t op2) // LDRD Rd,[Rn,op2]
{
    if (cpu == 1) return 1; // ARM9 exclusive

    // Decode the other operands
    uint8_t op0 = (opcode & 0x0000F000) >> 12;
    if (op0 == 15) return 1;
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16];

    // Double word load, pre-adjust without writeback
    *registers[op0]     = core->memory.read<uint32_t>(cpu, op1 + op2);
    *registers[op0 + 1] = core->memory.read<uint32_t>(cpu, op1 + op2 + 4);

    return 2;
}

FORCE_INLINE int Interpreter::strdOf(uint32_t opcode, uint32_t op2) // STRD Rd,[Rn,op2]
{
    if (cpu == 1) return 1; // ARM9 exclusive

    // Decode the other operands
    uint8_t op0 = (opcode & 0x0000F000) >> 12;
    if (op0 == 15) return 1;
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16];

    // Double word store, pre-adjust without writeback
    core->memory.write<uint32_t>(cpu, op1 + op2,     *registers[op0]);
    core->memory.write<uint32_t>(cpu, op1 + op2 + 4, *registers[op0 + 1]);

    return 2;
}

FORCE_INLINE int Interpreter::ldrsbPr(uint32_t opcode, uint32_t op2) // LDRSB Rd,[Rn,op2]!
{
    // Decode the other operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];

    // Signed byte load, pre-adjust with writeback
    *op1 += op2;
    *op0 = core->memory.read<int8_t>(cpu, *op1);

    // Handle pipelining
    if (op0 != registers[15]) return ((cpu == 0) ? 1 : 3);
    *registers[15] = (*registers[15] & ~3) + 4;
    return 5;
}

FORCE_INLINE int Interpreter::ldrshPr(uint32_t opcode, uint32_t op2) // LDRSH Rd,[Rn,op2]!
{
    // Decode the other operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t address;

    // Signed half-word load, pre-adjust with writeback
    *op1 += op2;
    *op0 = core->memory.read<int16_t>(cpu, address = *op1);

    // Shift misaligned reads on ARM7
    if (cpu == 1 && (address & 1))
        *op0 = (int16_t)*op0 >> 8;

    // Handle pipelining
    if (op0 != registers[15]) return ((cpu == 0) ? 1 : 3);
    *registers[15] = (*registers[15] & ~3) + 4;
    return 5;
}

FORCE_INLINE int Interpreter::ldrbPr(uint32_t opcode, uint32_t op2) // LDRB Rd,[Rn,op2]!
{
    // Decode the other operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];

    // Byte load, pre-adjust with writeback
    *op1 += op2;
    *op0 = core->memory.read<uint8_t>(cpu, *op1);

    if (op0 != registers[15])
        return ((cpu == 0) ? 1 : 3);

    // Handle pipelining and THUMB switching
    if (cpu == 0 && (*op0 & BIT(0)))
    {
        cpsr |= BIT(5);
        *registers[15] = (*registers[15] & ~1) + 2;
    }
    else
    {
        *registers[15] = (*registers[15] & ~3) + 4;
    }

    return 5;
}

FORCE_INLINE int Interpreter::strbPr(uint32_t opcode, uint32_t op2) // STRB Rd,[Rn,op2]!
{
    // Decode the other operands
    // When used as Rd, the program counter is read with +4
    uint32_t op0 = *registers[(opcode & 0x0000F000) >> 12] + (((opcode & 0x0000F000) == 0x0000F000) ? 4 : 0);
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];

    // Byte store, pre-adjust with writeback
    *op1 += op2;
    core->memory.write<uint8_t>(cpu, *op1, op0);

    return ((cpu == 0) ? 1 : 2);
}

FORCE_INLINE int Interpreter::ldrhPr(uint32_t opcode, uint32_t op2) // LDRH Rd,[Rn,op2]!
{
    // Decode the other operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t address;

    // Half-word load, pre-adjust with writeback
    *op1 += op2;
    *op0 = core->memory.read<uint16_t>(cpu, address = *op1);

    // Rotate misaligned reads on ARM7
    if (cpu == 1 && (address & 1))
        *op0 = (*op0 << 24) | (*op0 >> 8);

    // Handle pipelining
    if (op0 != registers[15]) return ((cpu == 0) ? 1 : 3);
    *registers[15] = (*registers[15] & ~3) + 4;
    return 5;
}

FORCE_INLINE int Interpreter::strhPr(uint32_t opcode, uint32_t op2) // STRH Rd,[Rn,op2]!
{
    // Decode the other operands
    // When used as Rd, the program counter is read with +4
    uint32_t op0 = *registers[(opcode & 0x0000F000) >> 12] + (((opcode & 0x0000F000) == 0x0000F000) ? 4 : 0);
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];

    // Half-word store, pre-adjust with writeback
    *op1 += op2;
    core->memory.write<uint16_t>(cpu, *op1, op0);

    return ((cpu == 0) ? 1 : 2);
}

FORCE_INLINE int Interpreter::ldrPr(uint32_t opcode, uint32_t op2) // LDR Rd,[Rn,op2]!
{
    // Decode the other operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t address;

    // Word load, pre-adjust with writeback
    *op1 += op2;
    *op0 = core->memory.read<uint32_t>(cpu, address = *op1);

    // Rotate misaligned reads
    if (address & 3)
    {
        int shift = (address & 3) * 8;
        *op0 = (*op0 << (32 - shift)) | (*op0 >> shift);
    }

    if (op0 != registers[15])
        return ((cpu == 0) ? 1 : 3);

    // Handle pipelining and THUMB switching
    if (cpu == 0 && (*op0 & BIT(0)))
    {
        cpsr |= BIT(5);
        *registers[15] = (*registers[15] & ~1) + 2;
    }
    else
    {
        *registers[15] = (*registers[15] & ~3) + 4;
    }

    return 5;
}

FORCE_INLINE int Interpreter::strPr(uint32_t opcode, uint32_t op2) // STR Rd,[Rn,op2]!
{
    // Decode the other operands
    // When used as Rd, the program counter is read with +4
    uint32_t op0 = *registers[(opcode & 0x0000F000) >> 12] + (((opcode & 0x0000F000) == 0x0000F000) ? 4 : 0);
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];

    // Word store, pre-adjust with writeback
    *op1 += op2;
    core->memory.write<uint32_t>(cpu, *op1, op0);

    return ((cpu == 0) ? 1 : 2);
}

FORCE_INLINE int Interpreter::ldrdPr(uint32_t opcode, uint32_t op2) // LDRD Rd,[Rn,op2]!
{
    if (cpu == 1) return 1; // ARM9 exclusive

    // Decode the other operands
    uint8_t op0 = (opcode & 0x0000F000) >> 12;
    if (op0 == 15) return 1;
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];

    // Double word load, pre-adjust with writeback
    *op1 += op2;
    *registers[op0]     = core->memory.read<uint32_t>(cpu, *op1);
    *registers[op0 + 1] = core->memory.read<uint32_t>(cpu, *op1 + 4);

    return 2;
}

FORCE_INLINE int Interpreter::strdPr(uint32_t opcode, uint32_t op2) // STRD Rd,[Rn,op2]!
{
    if (cpu == 1) return 1; // ARM9 exclusive

    // Decode the other operands
    uint8_t op0 = (opcode & 0x0000F000) >> 12;
    if (op0 == 15) return 1;
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];

    // Double word store, pre-adjust with writeback
    *op1 += op2;
    core->memory.write<uint32_t>(cpu, *op1,     *registers[op0]);
    core->memory.write<uint32_t>(cpu, *op1 + 4, *registers[op0 + 1]);

    return 2;
}

FORCE_INLINE int Interpreter::ldrsbPt(uint32_t opcode, uint32_t op2) // LDRSB Rd,[Rn],op2
{
    // Decode the other operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];

    // Signed byte load, post-adjust
    *op0 = core->memory.read<int8_t>(cpu, *op1);
    if (op0 != op1) *op1 += op2;

    // Handle pipelining
    if (op0 != registers[15]) return ((cpu == 0) ? 1 : 3);
    *registers[15] = (*registers[15] & ~3) + 4;
    return 5;
}

FORCE_INLINE int Interpreter::ldrshPt(uint32_t opcode, uint32_t op2) // LDRSH Rd,[Rn],op2
{
    // Decode the other operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t address;

    // Signed half-word load, post-adjust
    *op0 = core->memory.read<int16_t>(cpu, address = *op1);
    if (op0 != op1) *op1 += op2;

    // Shift misaligned reads on ARM7
    if (cpu == 1 && (address & 1))
        *op0 = (int16_t)*op0 >> 8;

    // Handle pipelining
    if (op0 != registers[15]) return ((cpu == 0) ? 1 : 3);
    *registers[15] = (*registers[15] & ~3) + 4;
    return 5;
}

FORCE_INLINE int Interpreter::ldrbPt(uint32_t opcode, uint32_t op2) // LDRB Rd,[Rn],op2
{
    // Decode the other operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];

    // Byte load, post-adjust
    *op0 = core->memory.read<uint8_t>(cpu, *op1);
    if (op0 != op1) *op1 += op2;

    if (op0 != registers[15])
        return ((cpu == 0) ? 1 : 3);

    // Handle pipelining and THUMB switching
    if (cpu == 0 && (*op0 & BIT(0)))
    {
        cpsr |= BIT(5);
        *registers[15] = (*registers[15] & ~1) + 2;
    }
    else
    {
        *registers[15] = (*registers[15] & ~3) + 4;
    }

    return 5;
}

FORCE_INLINE int Interpreter::strbPt(uint32_t opcode, uint32_t op2) // STRB Rd,[Rn],op2
{
    // Decode the other operands
    // When used as Rd, the program counter is read with +4
    uint32_t op0 = *registers[(opcode & 0x0000F000) >> 12] + (((opcode & 0x0000F000) == 0x0000F000) ? 4 : 0);
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];

    // Byte store, post-adjust
    core->memory.write<uint8_t>(cpu, *op1, op0);
    *op1 += op2;

    return ((cpu == 0) ? 1 : 2);
}

FORCE_INLINE int Interpreter::ldrhPt(uint32_t opcode, uint32_t op2) // LDRH Rd,[Rn],op2
{
    // Decode the other operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t address;

    // Half-word load, post-adjust
    *op0 = core->memory.read<uint16_t>(cpu, address = *op1);
    *op1 += op2;

    // Rotate misaligned reads on ARM7
    if (cpu == 1 && (address & 1))
        *op0 = (*op0 << 24) | (*op0 >> 8);

    // Handle pipelining
    if (op0 != registers[15]) return ((cpu == 0) ? 1 : 3);
    *registers[15] = (*registers[15] & ~3) + 4;
    return 5;
}

FORCE_INLINE int Interpreter::strhPt(uint32_t opcode, uint32_t op2) // STRH Rd,[Rn],op2
{
    // Decode the other operands
    // When used as Rd, the program counter is read with +4
    uint32_t op0 = *registers[(opcode & 0x0000F000) >> 12] + (((opcode & 0x0000F000) == 0x0000F000) ? 4 : 0);
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];

    // Half-word store, post-adjust
    core->memory.write<uint16_t>(cpu, *op1, op0);
    *op1 += op2;

    return ((cpu == 0) ? 1 : 2);
}

FORCE_INLINE int Interpreter::ldrPt(uint32_t opcode, uint32_t op2) // LDR Rd,[Rn],op2
{
    // Decode the other operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t address;

    // Word load
    *op0 = core->memory.read<uint32_t>(cpu, address = *op1);

    // Rotate misaligned reads
    if (address & 3)
    {
        int shift = (address & 3) * 8;
        *op0 = (*op0 << (32 - shift)) | (*op0 >> shift);
    }

    // Post-adjust
    if (op0 != op1) *op1 += op2;

    if (op0 != registers[15])
        return ((cpu == 0) ? 1 : 3);

    // Handle pipelining and THUMB switching
    if (cpu == 0 && (*op0 & BIT(0)))
    {
        cpsr |= BIT(5);
        *registers[15] = (*registers[15] & ~1) + 2;
    }
    else
    {
        *registers[15] = (*registers[15] & ~3) + 4;
    }

    return 5;
}

FORCE_INLINE int Interpreter::strPt(uint32_t opcode, uint32_t op2) // STR Rd,[Rn],op2
{
    // Decode the other operands
    // When used as Rd, the program counter is read with +4
    uint32_t op0 = *registers[(opcode & 0x0000F000) >> 12] + (((opcode & 0x0000F000) == 0x0000F000) ? 4 : 0);
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];

    // Word store, post-adjust
    core->memory.write<uint32_t>(cpu, *op1, op0);
    *op1 += op2;

    return ((cpu == 0) ? 1 : 2);
}

FORCE_INLINE int Interpreter::ldrdPt(uint32_t opcode, uint32_t op2) // LDRD Rd,[Rn],op2
{
    if (cpu == 1) return 1; // ARM9 exclusive

    // Decode the other operands
    uint8_t op0 = (opcode & 0x0000F000) >> 12;
    if (op0 == 15) return 1;
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];

    // Double word load, post-adjust
    *registers[op0]     = core->memory.read<uint32_t>(cpu, *op1);
    *registers[op0 + 1] = core->memory.read<uint32_t>(cpu, *op1 + 4);
    *op1 += op2;

    return 2;
}

FORCE_INLINE int Interpreter::strdPt(uint32_t opcode, uint32_t op2) // STRD Rd,[Rn],op2
{
    if (cpu == 1) return 1; // ARM9 exclusive

    // Decode the other operands
    uint8_t op0 = (opcode & 0x0000F000) >> 12;
    if (op0 == 15) return 1;
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];

    // Double word store, post-adjust
    core->memory.write<uint32_t>(cpu, *op1,     *registers[op0]);
    core->memory.write<uint32_t>(cpu, *op1 + 4, *registers[op0 + 1]);
    *op1 += op2;

    return 2;
}

FORCE_INLINE int Interpreter::swpb(uint32_t opcode) // SWPB Rd,Rm,[Rn]
{
    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[opcode & 0x0000000F];
    uint32_t op2 = *registers[(opcode & 0x000F0000) >> 16];

    // Swap
    *op0 = core->memory.read<uint8_t>(cpu, op2);
    core->memory.write<uint8_t>(cpu, op2, op1);

    return (cpu == 0) ? 2 : 4;
}

FORCE_INLINE int Interpreter::swp(uint32_t opcode) // SWP Rd,Rm,[Rn]
{
    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[opcode & 0x0000000F];
    uint32_t op2 = *registers[(opcode & 0x000F0000) >> 16];

    // Swap
    *op0 = core->memory.read<uint32_t>(cpu, op2);
    core->memory.write<uint32_t>(cpu, op2, op1);

    // Rotate misaligned reads
    if (op2 & 3)
    {
        int shift = (op2 & 3) * 8;
        *op0 = (*op0 << (32 - shift)) | (*op0 >> shift);
    }

    return (cpu == 0) ? 2 : 4;
}

FORCE_INLINE int Interpreter::ldmda(uint32_t opcode) // LDMDA Rn, <Rlist>
{
    // Decode the operand
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16];
    int n = 0;

    // Decrement the address beforehand because transfers are always done in increasing order
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            op0 -= 4;
            n++;
        }
    }

    // Block load, post-decrement
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            op0 += 4;
            *registers[i] = core->memory.read<uint32_t>(cpu, op0);
        }
    }

    if (!(opcode & BIT(15))) // PC not in Rlist
        return n + ((cpu == 0) ? ((n > 1) ? 0 : 1) : 2);

    // Handle pipelining and THUMB switching
    if (cpu == 0 && (*registers[15] & BIT(0)))
    {
        cpsr |= BIT(5);
        *registers[15] = (*registers[15] & ~1) + 2;
    }
    else
    {
        *registers[15] = (*registers[15] & ~3) + 4;
    }

    return n + 4;
}

FORCE_INLINE int Interpreter::stmda(uint32_t opcode) // STMDA Rn, <Rlist>
{
    // Decode the operand
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16];
    int n = 0;

    // Decrement the address beforehand because transfers are always done in increasing order
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            op0 -= 4;
            n++;
        }
    }

    // Block store, post-decrement
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            op0 += 4;
            core->memory.write<uint32_t>(cpu, op0, *registers[i]);
        }
    }

    return n + ((cpu == 0 && n > 1) ? 0 : 1);
}

FORCE_INLINE int Interpreter::ldmia(uint32_t opcode) // LDMIA Rn, <Rlist>
{
    // Decode the operand
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16];
    int n = 0;

    // Block load, post-increment
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            *registers[i] = core->memory.read<uint32_t>(cpu, op0);
            op0 += 4;
            n++;
        }
    }

    if (!(opcode & BIT(15))) // PC not in Rlist
        return n + ((cpu == 0) ? ((n > 1) ? 0 : 1) : 2);

    // Handle pipelining and THUMB switching
    if (cpu == 0 && (*registers[15] & BIT(0)))
    {
        cpsr |= BIT(5);
        *registers[15] = (*registers[15] & ~1) + 2;
    }
    else
    {
        *registers[15] = (*registers[15] & ~3) + 4;
    }

    return n + 4;
}

FORCE_INLINE int Interpreter::stmia(uint32_t opcode) // STMIA Rn, <Rlist>
{
    // Decode the operand
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16];
    int n = 0;

    // Block store, post-increment
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            core->memory.write<uint32_t>(cpu, op0, *registers[i]);
            op0 += 4;
            n++;
        }
    }

    return n + ((cpu == 0 && n > 1) ? 0 : 1);
}

FORCE_INLINE int Interpreter::ldmdb(uint32_t opcode) // LDMDB Rn, <Rlist>
{
    // Decode the operand
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16];
    int n = 0;

    // Decrement the address beforehand because transfers are always done in increasing order
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            op0 -= 4;
            n++;
        }
    }

    // Block load, pre-decrement
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            *registers[i] = core->memory.read<uint32_t>(cpu, op0);
            op0 += 4;
        }
    }

    if (!(opcode & BIT(15))) // PC not in Rlist
        return n + ((cpu == 0) ? ((n > 1) ? 0 : 1) : 2);

    // Handle pipelining and THUMB switching
    if (cpu == 0 && (*registers[15] & BIT(0)))
    {
        cpsr |= BIT(5);
        *registers[15] = (*registers[15] & ~1) + 2;
    }
    else
    {
        *registers[15] = (*registers[15] & ~3) + 4;
    }

    return n + 4;
}

FORCE_INLINE int Interpreter::stmdb(uint32_t opcode) // STMDB Rn, <Rlist>
{
    // Decode the operand
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16];
    int n = 0;

    // Decrement the address beforehand because transfers are always done in increasing order
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            op0 -= 4;
            n++;
        }
    }

    // Block store, pre-decrement
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            core->memory.write<uint32_t>(cpu, op0, *registers[i]);
            op0 += 4;
        }
    }

    return n + ((cpu == 0 && n > 1) ? 0 : 1);
}

FORCE_INLINE int Interpreter::ldmib(uint32_t opcode) // LDMIB Rn, <Rlist>
{
    // Decode the operand
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16];
    int n = 0;

    // Block load, pre-increment
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            op0 += 4;
            *registers[i] = core->memory.read<uint32_t>(cpu, op0);
            n++;
        }
    }

    if (!(opcode & BIT(15))) // PC not in Rlist
        return n + ((cpu == 0) ? ((n > 1) ? 0 : 1) : 2);

    // Handle pipelining and THUMB switching
    if (cpu == 0 && (*registers[15] & BIT(0)))
    {
        cpsr |= BIT(5);
        *registers[15] = (*registers[15] & ~1) + 2;
    }
    else
    {
        *registers[15] = (*registers[15] & ~3) + 4;
    }

    return n + 4;
}

FORCE_INLINE int Interpreter::stmib(uint32_t opcode) // STMIB Rn, <Rlist>
{
    // Decode the operand
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16];
    int n = 0;

    // Block store, pre-increment
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            op0 += 4;
            core->memory.write<uint32_t>(cpu, op0, *registers[i]);
            n++;
        }
    }

    return n + ((cpu == 0 && n > 1) ? 0 : 1);
}

FORCE_INLINE int Interpreter::ldmdaW(uint32_t opcode) // LDMDA Rn!, <Rlist>
{
    // Decode the operand
    int m = (opcode & 0x000F0000) >> 16;
    uint32_t op0 = *registers[m];
    int n = 0;

    // Decrement the address beforehand because transfers are always done in increasing order
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            op0 -= 4;
            n++;
        }
    }

    uint32_t writeback = op0;

    // Block load, post-decrement
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            op0 += 4;
            *registers[i] = core->memory.read<uint32_t>(cpu, op0);
        }
    }

    // Writeback
    // On ARM9, if Rn is in Rlist, writeback only happens if Rn is the only register, or not the last
    // On ARM7, if Rn is in Rlist, writeback never happens
    if (!(opcode & BIT(m)) || (cpu == 0 && ((opcode & 0x0000FFFF) == BIT(m) || (opcode & 0x0000FFFF & ~(BIT(m + 1) - 1)))))
        *registers[m] = writeback;

    if (!(opcode & BIT(15))) // PC not in Rlist
        return n + ((cpu == 0) ? ((n > 1) ? 0 : 1) : 2);

    // Handle pipelining and THUMB switching
    if (cpu == 0 && (*registers[15] & BIT(0)))
    {
        cpsr |= BIT(5);
        *registers[15] = (*registers[15] & ~1) + 2;
    }
    else
    {
        *registers[15] = (*registers[15] & ~3) + 4;
    }

    return n + 4;
}

FORCE_INLINE int Interpreter::stmdaW(uint32_t opcode) // STMDA Rn!, <Rlist>
{
    // Decode the operand
    int m = (opcode & 0x000F0000) >> 16;
    uint32_t op0 = *registers[m];
    int n = 0;

    // Decrement the address beforehand because transfers are always done in increasing order
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            op0 -= 4;
            n++;
        }
    }

    uint32_t writeback = op0;

    // On ARM9, if Rn is in Rlist, the writeback value is never stored
    // On ARM7, if Rn is in Rlist, the writeback value is stored if Rn is not the first register
    if (cpu == 1 && (opcode & BIT(m)) && (opcode & (BIT(m) - 1)))
        *registers[m] = writeback;

    // Block store, post-decrement
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            op0 += 4;
            core->memory.write<uint32_t>(cpu, op0, *registers[i]);
        }
    }

    // Writeback
    *registers[m] = writeback;

    return n + ((cpu == 0 && n > 1) ? 0 : 1);
}

FORCE_INLINE int Interpreter::ldmiaW(uint32_t opcode) // LDMIA Rn!, <Rlist>
{
    // Decode the operand
    int m = (opcode & 0x000F0000) >> 16;
    uint32_t op0 = *registers[m];
    int n = 0;

    // Block load, post-increment
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            *registers[i] = core->memory.read<uint32_t>(cpu, op0);
            op0 += 4;
            n++;
        }
    }

    // Writeback
    // On ARM9, if Rn is in Rlist, writeback only happens if Rn is the only register, or not the last
    // On ARM7, if Rn is in Rlist, writeback never happens
    if (!(opcode & BIT(m)) || (cpu == 0 && ((opcode & 0x0000FFFF) == BIT(m) || (opcode & 0x0000FFFF & ~(BIT(m + 1) - 1)))))
        *registers[m] = op0;

    if (!(opcode & BIT(15))) // PC not in Rlist
        return n + ((cpu == 0) ? ((n > 1) ? 0 : 1) : 2);

    // Handle pipelining and THUMB switching
    if (cpu == 0 && (*registers[15] & BIT(0)))
    {
        cpsr |= BIT(5);
        *registers[15] = (*registers[15] & ~1) + 2;
    }
    else
    {
        *registers[15] = (*registers[15] & ~3) + 4;
    }

    return n + 4;
}

FORCE_INLINE int Interpreter::stmiaW(uint32_t opcode) // STMIA Rn!, <Rlist>
{
    // Decode the operand
    int m = (opcode & 0x000F0000) >> 16;
    uint32_t op0 = *registers[m];
    int n = 0;

    // On ARM9, if Rn is in Rlist, the writeback value is never stored
    // On ARM7, if Rn is in Rlist, the writeback value is stored if Rn is not the first register
    if (cpu == 1 && (opcode & BIT(m)) && (opcode & (BIT(m) - 1)))
    {
        uint32_t writeback = op0;
        for (int i = 0; i < 16; i++)
        {
            if (opcode & BIT(i))
                writeback += 4;
        }
        *registers[m] = writeback;
    }

    // Block store, post-increment
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            core->memory.write<uint32_t>(cpu, op0, *registers[i]);
            op0 += 4;
            n++;
        }
    }

    // Writeback
    *registers[m] = op0;

    return n + ((cpu == 0 && n > 1) ? 0 : 1);
}

FORCE_INLINE int Interpreter::ldmdbW(uint32_t opcode) // LDMDB Rn!, <Rlist>
{
    // Decode the operand
    int m = (opcode & 0x000F0000) >> 16;
    uint32_t op0 = *registers[m];
    int n = 0;

    // Decrement the address beforehand because transfers are always done in increasing order
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            op0 -= 4;
            n++;
        }
    }

    uint32_t writeback = op0;

    // Block load, pre-decrement
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            *registers[i] = core->memory.read<uint32_t>(cpu, op0);
            op0 += 4;
        }
    }

    // Writeback
    // On ARM9, if Rn is in Rlist, writeback only happens if Rn is the only register, or not the last
    // On ARM7, if Rn is in Rlist, writeback never happens
    if (!(opcode & BIT(m)) || (cpu == 0 && ((opcode & 0x0000FFFF) == BIT(m) || (opcode & 0x0000FFFF & ~(BIT(m + 1) - 1)))))
        *registers[m] = writeback;

    if (!(opcode & BIT(15))) // PC not in Rlist
        return n + ((cpu == 0) ? ((n > 1) ? 0 : 1) : 2);

    // Handle pipelining and THUMB switching
    if (cpu == 0 && (*registers[15] & BIT(0)))
    {
        cpsr |= BIT(5);
        *registers[15] = (*registers[15] & ~1) + 2;
    }
    else
    {
        *registers[15] = (*registers[15] & ~3) + 4;
    }

    return n + 4;
}

FORCE_INLINE int Interpreter::stmdbW(uint32_t opcode) // STMDB Rn!, <Rlist>
{
    // Decode the operand
    int m = (opcode & 0x000F0000) >> 16;
    uint32_t op0 = *registers[m];
    int n = 0;

    // Decrement the address beforehand because transfers are always done in increasing order
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            op0 -= 4;
            n++;
        }
    }

    uint32_t writeback = op0;

    // On ARM9, if Rn is in Rlist, the writeback value is never stored
    // On ARM7, if Rn is in Rlist, the writeback value is stored if Rn is not the first register
    if (cpu == 1 && (opcode & BIT(m)) && (opcode & (BIT(m) - 1)))
        *registers[m] = writeback;

    // Block store, pre-decrement
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            core->memory.write<uint32_t>(cpu, op0, *registers[i]);
            op0 += 4;
        }
    }

    // Writeback
    *registers[m] = writeback;

    return n + ((cpu == 0 && n > 1) ? 0 : 1);
}

FORCE_INLINE int Interpreter::ldmibW(uint32_t opcode) // LDMIB Rn!, <Rlist>
{
    // Decode the operand
    int m = (opcode & 0x000F0000) >> 16;
    uint32_t op0 = *registers[m];
    int n = 0;

    // Block load, pre-increment
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            op0 += 4;
            *registers[i] = core->memory.read<uint32_t>(cpu, op0);
            n++;
        }
    }

    // Writeback
    // On ARM9, if Rn is in Rlist, writeback only happens if Rn is the only register, or not the last
    // On ARM7, if Rn is in Rlist, writeback never happens
    if (!(opcode & BIT(m)) || (cpu == 0 && ((opcode & 0x0000FFFF) == BIT(m) || (opcode & 0x0000FFFF & ~(BIT(m + 1) - 1)))))
        *registers[m] = op0;

    if (!(opcode & BIT(15))) // PC not in Rlist
        return n + ((cpu == 0) ? ((n > 1) ? 0 : 1) : 2);

    // Handle pipelining and THUMB switching
    if (cpu == 0 && (*registers[15] & BIT(0)))
    {
        cpsr |= BIT(5);
        *registers[15] = (*registers[15] & ~1) + 2;
    }
    else
    {
        *registers[15] = (*registers[15] & ~3) + 4;
    }

    return n + 4;
}

FORCE_INLINE int Interpreter::stmibW(uint32_t opcode) // STMIB Rn!, <Rlist>
{
    // Decode the operand
    int m = (opcode & 0x000F0000) >> 16;
    uint32_t op0 = *registers[m];
    int n = 0;

    // On ARM9, if Rn is in Rlist, the writeback value is never stored
    // On ARM7, if Rn is in Rlist, the writeback value is stored if Rn is not the first register
    if (cpu == 1 && (opcode & BIT(m)) && (opcode & (BIT(m) - 1)))
    {
        uint32_t writeback = op0;
        for (int i = 0; i < 16; i++)
        {
            if (opcode & BIT(i))
                writeback += 4;
        }
        *registers[m] = writeback;
    }

    // Block store, pre-increment
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            op0 += 4;
            core->memory.write<uint32_t>(cpu, op0, *registers[i]);
            n++;
        }
    }

    // Writeback
    *registers[m] = op0;

    return n + ((cpu == 0 && n > 1) ? 0 : 1);
}

FORCE_INLINE int Interpreter::ldmdaU(uint32_t opcode) // LDMDA Rn, <Rlist>^
{
    // Decode the operand
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16];
    int n = 0;

    // Decrement the address beforehand because transfers are always done in increasing order
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            op0 -= 4;
            n++;
        }
    }

    if (!(opcode & BIT(15))) // PC not in Rlist
    {
        // Block load, post-decrement (user registers)
        for (int i = 0; i < 16; i++)
        {
            if (opcode & BIT(i))
            {
                op0 += 4;
                registersUsr[i] = core->memory.read<uint32_t>(cpu, op0);
            }
        }

        return n + ((cpu == 0) ? ((n > 1) ? 0 : 1) : 2);
    }

    // Block load, post-decrement
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            op0 += 4;
            *registers[i] = core->memory.read<uint32_t>(cpu, op0);
        }
    }

    // Restore the SPSR
    if (spsr)
        setCpsr(*spsr);

    // Handle pipelining and THUMB switching
    if (cpu == 0 && (*registers[15] & BIT(0)))
    {
        cpsr |= BIT(5);
        *registers[15] = (*registers[15] & ~1) + 2;
    }
    else
    {
        *registers[15] = (*registers[15] & ~3) + 4;
    }

    return n + 4;
}

FORCE_INLINE int Interpreter::stmdaU(uint32_t opcode) // STMDA Rn, <Rlist>^
{
    // Decode the operand
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16];
    int n = 0;

    // Decrement the address beforehand because transfers are always done in increasing order
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            op0 -= 4;
            n++;
        }
    }

    // Block store, post-decrement (user registers)
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            op0 += 4;
            core->memory.write<uint32_t>(cpu, op0, registersUsr[i]);
        }
    }

    return n + ((cpu == 0 && n > 1) ? 0 : 1);
}

FORCE_INLINE int Interpreter::ldmiaU(uint32_t opcode) // LDMIA Rn, <Rlist>^
{
    // Decode the operand
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16];
    int n = 0;

    if (!(opcode & BIT(15))) // PC not in Rlist
    {
        // Block load, post-increment (user registers)
        for (int i = 0; i < 16; i++)
        {
            if (opcode & BIT(i))
            {
                registersUsr[i] = core->memory.read<uint32_t>(cpu, op0);
                op0 += 4;
                n++;
            }
        }

        return n + ((cpu == 0) ? ((n > 1) ? 0 : 1) : 2);
    }

    // Block load, post-increment
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            *registers[i] = core->memory.read<uint32_t>(cpu, op0);
            op0 += 4;
            n++;
        }
    }

    // Restore the SPSR
    if (spsr)
        setCpsr(*spsr);

    // Handle pipelining and THUMB switching
    if (cpu == 0 && (*registers[15] & BIT(0)))
    {
        cpsr |= BIT(5);
        *registers[15] = (*registers[15] & ~1) + 2;
    }
    else
    {
        *registers[15] = (*registers[15] & ~3) + 4;
    }

    return n + 4;
}

FORCE_INLINE int Interpreter::stmiaU(uint32_t opcode) // STMIA Rn, <Rlist>^
{
    // Decode the operand
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16];
    int n = 0;

    // Block store, post-increment (user registers)
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            core->memory.write<uint32_t>(cpu, op0, registersUsr[i]);
            op0 += 4;
            n++;
        }
    }

    return n + ((cpu == 0 && n > 1) ? 0 : 1);
}

FORCE_INLINE int Interpreter::ldmdbU(uint32_t opcode) // LDMDB Rn, <Rlist>^
{
    // Decode the operand
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16];
    int n = 0;

    // Decrement the address beforehand because transfers are always done in increasing order
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            op0 -= 4;
            n++;
        }
    }

    if (!(opcode & BIT(15))) // PC not in Rlist
    {
        // Block load, pre-decrement (user registers)
        for (int i = 0; i < 16; i++)
        {
            if (opcode & BIT(i))
            {
                registersUsr[i] = core->memory.read<uint32_t>(cpu, op0);
                op0 += 4;
            }
        }

        return n + ((cpu == 0) ? ((n > 1) ? 0 : 1) : 2);
    }

    // Block load, pre-decrement
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            *registers[i] = core->memory.read<uint32_t>(cpu, op0);
            op0 += 4;
        }
    }

    // Restore the SPSR
    if (spsr)
        setCpsr(*spsr);

    // Handle pipelining and THUMB switching
    if (cpu == 0 && (*registers[15] & BIT(0)))
    {
        cpsr |= BIT(5);
        *registers[15] = (*registers[15] & ~1) + 2;
    }
    else
    {
        *registers[15] = (*registers[15] & ~3) + 4;
    }

    return n + 4;
}

FORCE_INLINE int Interpreter::stmdbU(uint32_t opcode) // STMDB Rn, <Rlist>^
{
    // Decode the operand
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16];
    int n = 0;

    // Decrement the address beforehand because transfers are always done in increasing order
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            op0 -= 4;
            n++;
        }
    }

    // Block store, pre-decrement (user registers)
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            core->memory.write<uint32_t>(cpu, op0, registersUsr[i]);
            op0 += 4;
        }
    }

    return n + ((cpu == 0 && n > 1) ? 0 : 1);
}

FORCE_INLINE int Interpreter::ldmibU(uint32_t opcode) // LDMIB Rn, <Rlist>^
{
    // Decode the operand
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16];
    int n = 0;

    if (!(opcode & BIT(15))) // PC not in Rlist
    {
        // Block load, pre-increment (user registers)
        for (int i = 0; i < 16; i++)
        {
            if (opcode & BIT(i))
            {
                op0 += 4;
                registersUsr[i] = core->memory.read<uint32_t>(cpu, op0);
                n++;
            }
        }

        return n + ((cpu == 0) ? ((n > 1) ? 0 : 1) : 2);
    }

    // Block load, pre-increment
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            op0 += 4;
            *registers[i] = core->memory.read<uint32_t>(cpu, op0);
            n++;
        }
    }

    // Restore the SPSR
    if (spsr)
        setCpsr(*spsr);

    // Handle pipelining and THUMB switching
    if (cpu == 0 && (*registers[15] & BIT(0)))
    {
        cpsr |= BIT(5);
        *registers[15] = (*registers[15] & ~1) + 2;
    }
    else
    {
        *registers[15] = (*registers[15] & ~3) + 4;
    }

    return n + 4;
}

FORCE_INLINE int Interpreter::stmibU(uint32_t opcode) // STMIB Rn, <Rlist>^
{
    // Decode the operand
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16];
    int n = 0;

    // Block store, pre-increment (user registers)
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            op0 += 4;
            core->memory.write<uint32_t>(cpu, op0, registersUsr[i]);
            n++;
        }
    }

    return n + ((cpu == 0 && n > 1) ? 0 : 1);
}

FORCE_INLINE int Interpreter::ldmdaUW(uint32_t opcode) // LDMDA Rn!, <Rlist>^
{
    // Decode the operand
    int m = (opcode & 0x000F0000) >> 16;
    uint32_t op0 = *registers[m];
    int n = 0;

    // Decrement the address beforehand because transfers are always done in increasing order
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            op0 -= 4;
            n++;
        }
    }

    uint32_t writeback = op0;

    if (!(opcode & BIT(15))) // PC not in Rlist
    {
        // Block load, post-decrement (user registers)
        for (int i = 0; i < 16; i++)
        {
            if (opcode & BIT(i))
            {
                op0 += 4;
                registersUsr[i] = core->memory.read<uint32_t>(cpu, op0);
            }
        }

        // Writeback
        // On ARM9, if Rn is in Rlist, writeback only happens if Rn is the only register, or not the last
        // On ARM7, if Rn is in Rlist, writeback never happens
        if (!(opcode & BIT(m)) || (cpu == 0 && ((opcode & 0x0000FFFF) == BIT(m) || (opcode & 0x0000FFFF & ~(BIT(m + 1) - 1)))))
            *registers[m] = writeback;

        return n + ((cpu == 0) ? ((n > 1) ? 0 : 1) : 2);
    }

    // Block load, post-decrement
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            op0 += 4;
            *registers[i] = core->memory.read<uint32_t>(cpu, op0);
        }
    }

    // Writeback
    // On ARM9, if Rn is in Rlist, writeback only happens if Rn is the only register, or not the last
    // On ARM7, if Rn is in Rlist, writeback never happens
    if (!(opcode & BIT(m)) || (cpu == 0 && ((opcode & 0x0000FFFF) == BIT(m) || (opcode & 0x0000FFFF & ~(BIT(m + 1) - 1)))))
        *registers[m] = writeback;

    // Restore the SPSR
    if (spsr)
        setCpsr(*spsr);

    // Handle pipelining and THUMB switching
    if (cpu == 0 && (*registers[15] & BIT(0)))
    {
        cpsr |= BIT(5);
        *registers[15] = (*registers[15] & ~1) + 2;
    }
    else
    {
        *registers[15] = (*registers[15] & ~3) + 4;
    }

    return n + 4;
}

FORCE_INLINE int Interpreter::stmdaUW(uint32_t opcode) // STMDA Rn!, <Rlist>^
{
    // Decode the operand
    int m = (opcode & 0x000F0000) >> 16;
    uint32_t op0 = *registers[m];
    int n = 0;

    // Decrement the address beforehand because transfers are always done in increasing order
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            op0 -= 4;
            n++;
        }
    }

    uint32_t writeback = op0;

    // On ARM9, if Rn is in Rlist, the writeback value is never stored
    // On ARM7, if Rn is in Rlist, the writeback value is stored if Rn is not the first register
    if (cpu == 1 && (opcode & BIT(m)) && (opcode & (BIT(m) - 1)))
        *registers[m] = writeback;

    // Block store, post-decrement (user registers)
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            op0 += 4;
            core->memory.write<uint32_t>(cpu, op0, registersUsr[i]);
        }
    }

    // Writeback
    *registers[m] = writeback;

    return n + ((cpu == 0 && n > 1) ? 0 : 1);
}

FORCE_INLINE int Interpreter::ldmiaUW(uint32_t opcode) // LDMIA Rn!, <Rlist>^
{
    // Decode the operand
    int m = (opcode & 0x000F0000) >> 16;
    uint32_t op0 = *registers[m];
    int n = 0;

    if (!(opcode & BIT(15))) // PC not in Rlist
    {
        // Block load, post-increment (user registers)
        for (int i = 0; i < 16; i++)
        {
            if (opcode & BIT(i))
            {
                registersUsr[i] = core->memory.read<uint32_t>(cpu, op0);
                op0 += 4;
                n++;
            }
        }

        // Writeback
        // On ARM9, if Rn is in Rlist, writeback only happens if Rn is the only register, or not the last
        // On ARM7, if Rn is in Rlist, writeback never happens
        if (!(opcode & BIT(m)) || (cpu == 0 && ((opcode & 0x0000FFFF) == BIT(m) || (opcode & 0x0000FFFF & ~(BIT(m + 1) - 1)))))
            *registers[m] = op0;

        return n + ((cpu == 0) ? ((n > 1) ? 0 : 1) : 2);
    }

    // Block load, post-increment
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            *registers[i] = core->memory.read<uint32_t>(cpu, op0);
            op0 += 4;
            n++;
        }
    }

    // Writeback
    // On ARM9, if Rn is in Rlist, writeback only happens if Rn is the only register, or not the last
    // On ARM7, if Rn is in Rlist, writeback never happens
    if (!(opcode & BIT(m)) || (cpu == 0 && ((opcode & 0x0000FFFF) == BIT(m) || (opcode & 0x0000FFFF & ~(BIT(m + 1) - 1)))))
        *registers[m] = op0;

    // Restore the SPSR
    if (spsr)
        setCpsr(*spsr);

    // Handle pipelining and THUMB switching
    if (cpu == 0 && (*registers[15] & BIT(0)))
    {
        cpsr |= BIT(5);
        *registers[15] = (*registers[15] & ~1) + 2;
    }
    else
    {
        *registers[15] = (*registers[15] & ~3) + 4;
    }

    return n + 4;
}

FORCE_INLINE int Interpreter::stmiaUW(uint32_t opcode) // STMIA Rn!, <Rlist>^
{
    // Decode the operand
    int m = (opcode & 0x000F0000) >> 16;
    uint32_t op0 = *registers[m];
    int n = 0;

    // On ARM9, if Rn is in Rlist, the writeback value is never stored
    // On ARM7, if Rn is in Rlist, the writeback value is stored if Rn is not the first register
    if (cpu == 1 && (opcode & BIT(m)) && (opcode & (BIT(m) - 1)))
    {
        uint32_t writeback = op0;
        for (int i = 0; i < 16; i++)
        {
            if (opcode & BIT(i))
                writeback += 4;
        }
        *registers[m] = writeback;
    }

    // Block store, post-increment (user registers)
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            core->memory.write<uint32_t>(cpu, op0, registersUsr[i]);
            op0 += 4;
            n++;
        }
    }

    // Writeback
    *registers[m] = op0;

    return n + ((cpu == 0 && n > 1) ? 0 : 1);
}

FORCE_INLINE int Interpreter::ldmdbUW(uint32_t opcode) // LDMDB Rn!, <Rlist>^
{
    // Decode the operand
    int m = (opcode & 0x000F0000) >> 16;
    uint32_t op0 = *registers[m];
    int n = 0;

    // Decrement the address beforehand because transfers are always done in increasing order
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            op0 -= 4;
            n++;
        }
    }

    uint32_t writeback = op0;

    if (!(opcode & BIT(15))) // PC not in Rlist
    {
        // Block load, pre-decrement (user registers)
        for (int i = 0; i < 16; i++)
        {
            if (opcode & BIT(i))
            {
                registersUsr[i] = core->memory.read<uint32_t>(cpu, op0);
                op0 += 4;
            }
        }

        // Writeback
        // On ARM9, if Rn is in Rlist, writeback only happens if Rn is the only register, or not the last
        // On ARM7, if Rn is in Rlist, writeback never happens
        if (!(opcode & BIT(m)) || (cpu == 0 && ((opcode & 0x0000FFFF) == BIT(m) || (opcode & 0x0000FFFF & ~(BIT(m + 1) - 1)))))
            *registers[m] = writeback;

        return n + ((cpu == 0) ? ((n > 1) ? 0 : 1) : 2);
    }

    // Block load, pre-decrement
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            *registers[i] = core->memory.read<uint32_t>(cpu, op0);
            op0 += 4;
        }
    }

    // Writeback
    // On ARM9, if Rn is in Rlist, writeback only happens if Rn is the only register, or not the last
    // On ARM7, if Rn is in Rlist, writeback never happens
    if (!(opcode & BIT(m)) || (cpu == 0 && ((opcode & 0x0000FFFF) == BIT(m) || (opcode & 0x0000FFFF & ~(BIT(m + 1) - 1)))))
        *registers[m] = writeback;

    // Restore the SPSR
    if (spsr)
        setCpsr(*spsr);

    // Handle pipelining and THUMB switching
    if (cpu == 0 && (*registers[15] & BIT(0)))
    {
        cpsr |= BIT(5);
        *registers[15] = (*registers[15] & ~1) + 2;
    }
    else
    {
        *registers[15] = (*registers[15] & ~3) + 4;
    }

    return n + 4;
}

FORCE_INLINE int Interpreter::stmdbUW(uint32_t opcode) // STMDB Rn!, <Rlist>^
{
    // Decode the operand
    int m = (opcode & 0x000F0000) >> 16;
    uint32_t op0 = *registers[m];
    int n = 0;

    // Decrement the address beforehand because transfers are always done in increasing order
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            op0 -= 4;
            n++;
        }
    }

    uint32_t writeback = op0;

    // On ARM9, if Rn is in Rlist, the writeback value is never stored
    // On ARM7, if Rn is in Rlist, the writeback value is stored if Rn is not the first register
    if (cpu == 1 && (opcode & BIT(m)) && (opcode & (BIT(m) - 1)))
        *registers[m] = writeback;

    // Block store, pre-decrement (user registers)
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            core->memory.write<uint32_t>(cpu, op0, registersUsr[i]);
            op0 += 4;
        }
    }

    // Writeback
    *registers[m] = writeback;

    return n + ((cpu == 0 && n > 1) ? 0 : 1);
}

FORCE_INLINE int Interpreter::ldmibUW(uint32_t opcode) // LDMIB Rn!, <Rlist>^
{
    // Decode the operand
    int m = (opcode & 0x000F0000) >> 16;
    uint32_t op0 = *registers[m];
    int n = 0;

    if (!(opcode & BIT(15))) // PC not in Rlist
    {
        // Block load, pre-increment (user registers)
        for (int i = 0; i < 16; i++)
        {
            if (opcode & BIT(i))
            {
                op0 += 4;
                registersUsr[i] = core->memory.read<uint32_t>(cpu, op0);
                n++;
            }
        }

        // Writeback
        // On ARM9, if Rn is in Rlist, writeback only happens if Rn is the only register, or not the last
        // On ARM7, if Rn is in Rlist, writeback never happens
        if (!(opcode & BIT(m)) || (cpu == 0 && ((opcode & 0x0000FFFF) == BIT(m) || (opcode & 0x0000FFFF & ~(BIT(m + 1) - 1)))))
            *registers[m] = op0;

        return n + ((cpu == 0) ? ((n > 1) ? 0 : 1) : 2);
    }

    // Block load, pre-increment
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            op0 += 4;
            *registers[i] = core->memory.read<uint32_t>(cpu, op0);
            n++;
        }
    }

    // Writeback
    // On ARM9, if Rn is in Rlist, writeback only happens if Rn is the only register, or not the last
    // On ARM7, if Rn is in Rlist, writeback never happens
    if (!(opcode & BIT(m)) || (cpu == 0 && ((opcode & 0x0000FFFF) == BIT(m) || (opcode & 0x0000FFFF & ~(BIT(m + 1) - 1)))))
        *registers[m] = op0;

    // Restore the SPSR
    if (spsr)
        setCpsr(*spsr);

    // Handle pipelining and THUMB switching
    if (cpu == 0 && (*registers[15] & BIT(0)))
    {
        cpsr |= BIT(5);
        *registers[15] = (*registers[15] & ~1) + 2;
    }
    else
    {
        *registers[15] = (*registers[15] & ~3) + 4;
    }

    return n + 4;
}

FORCE_INLINE int Interpreter::stmibUW(uint32_t opcode) // STMIB Rn!, <Rlist>^
{
    // Decode the operand
    int m = (opcode & 0x000F0000) >> 16;
    uint32_t op0 = *registers[m];
    int n = 0;

    // On ARM9, if Rn is in Rlist, the writeback value is never stored
    // On ARM7, if Rn is in Rlist, the writeback value is stored if Rn is not the first register
    if (cpu == 1 && (opcode & BIT(m)) && (opcode & (BIT(m) - 1)))
    {
        uint32_t writeback = op0;
        for (int i = 0; i < 16; i++)
        {
            if (opcode & BIT(i))
                writeback += 4;
        }
        *registers[m] = writeback;
    }

    // Block store, pre-increment (user registers)
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            op0 += 4;
            core->memory.write<uint32_t>(cpu, op0, registersUsr[i]);
            n++;
        }
    }

    // Writeback
    *registers[m] = op0;

    return n + ((cpu == 0 && n > 1) ? 0 : 1);
}

FORCE_INLINE int Interpreter::msrRc(uint32_t opcode) // MSR CPSR,Rm
{
    // Decode the operand
    uint32_t op1 = *registers[opcode & 0x0000000F];

    // Write the first 8 bits of the status flags, but only allow changing the CPU mode when not in user mode
    if (opcode & BIT(16))
    {
        uint8_t mask = ((cpsr & 0x1F) == 0x10) ? 0xE0 : 0xFF;
        setCpsr((cpsr & ~mask) | (op1 & mask));
    }

    // Write the remaining 8-bit blocks of the status flags
    for (int i = 0; i < 3; i++)
    {
        if (opcode & BIT(17 + i))
            cpsr = (cpsr & ~(0x0000FF00 << (i * 8))) | (op1 & (0x0000FF00 << (i * 8)));
    }

    return 1;
}

FORCE_INLINE int Interpreter::msrRs(uint32_t opcode) // MSR SPSR,Rm
{
    // Decode the operand
    uint32_t op1 = *registers[opcode & 0x0000000F];

    // Write the saved status flags in 8-bit blocks
    if (spsr)
    {
        for (int i = 0; i < 4; i++)
        {
            if (opcode & BIT(16 + i))
                *spsr = (*spsr & ~(0x000000FF << (i * 8))) | (op1 & (0x000000FF << (i * 8)));
        }
    }

    return 1;
}

FORCE_INLINE int Interpreter::msrIc(uint32_t opcode) // MSR CPSR,#i
{
    // Decode the operand
    // Can be any 8 bits rotated right by a multiple of 2
    uint32_t value = opcode & 0x000000FF;
    uint8_t shift = (opcode & 0x00000F00) >> 7;
    uint32_t op1 = (value << (32 - shift)) | (value >> shift);

    // Write the first 8 bits of the status flags, but only allow changing the CPU mode when not in user mode
    if (opcode & BIT(16))
    {
        uint8_t mask = ((cpsr & 0x1F) == 0x10) ? 0xE0 : 0xFF;
        setCpsr((cpsr & ~mask) | (op1 & mask));
    }

    // Write the remaining 8-bit blocks of the status flags
    for (int i = 0; i < 3; i++)
    {
        if (opcode & BIT(17 + i))
            cpsr = (cpsr & ~(0x0000FF00 << (i * 8))) | (op1 & (0x0000FF00 << (i * 8)));
    }

    return 1;
}

FORCE_INLINE int Interpreter::msrIs(uint32_t opcode) // MSR SPSR,#i
{
    // Decode the operand
    // Can be any 8 bits rotated right by a multiple of 2
    uint32_t value = opcode & 0x000000FF;
    uint8_t shift = (opcode & 0x00000F00) >> 7;
    uint32_t op1 = (value << (32 - shift)) | (value >> shift);

    // Write the saved status flags in 8-bit blocks
    if (spsr)
    {
        for (int i = 0; i < 4; i++)
        {
            if (opcode & BIT(16 + i))
                *spsr = (*spsr & ~(0x000000FF << (i * 8))) | (op1 & (0x000000FF << (i * 8)));
        }
    }

    return 1;
}

FORCE_INLINE int Interpreter::mrsRc(uint32_t opcode) // MRS Rd,CPSR
{
    // Decode the operand
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];

    // Copy the status flags to a register
    *op0 = cpsr;

    return (cpu == 0 ? 2 : 1);
}

FORCE_INLINE int Interpreter::mrsRs(uint32_t opcode) // MRS Rd,SPSR
{
    // Decode the operand
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];

    // Copy the saved status flags to a register
    if (spsr) *op0 = *spsr;

    return (cpu == 0 ? 2 : 1);
}

FORCE_INLINE int Interpreter::mrc(uint32_t opcode) // MRC Pn,<cpopc>,Rd,Cn,Cm,<cp>
{
    if (cpu == 1) return 1; // ARM9 exclusive

    // Decode the operands
    uint32_t *op2 = registers[(opcode & 0x0000F000) >> 12];
    int op3 = (opcode & 0x000F0000) >> 16;
    int op4 = opcode & 0x0000000F;
    int op5 = (opcode & 0x000000E0) >> 5;

    // Read from a CP15 register
    *op2 = core->cp15.read(op3, op4, op5);

    return 1;
}

FORCE_INLINE int Interpreter::mcr(uint32_t opcode) // MCR Pn,<cpopc>,Rd,Cn,Cm,<cp>
{
    if (cpu == 1) return 1; // ARM9 exclusive

    // Decode the operands
    uint32_t op2 = *registers[(opcode & 0x0000F000) >> 12];
    int op3 = (opcode & 0x000F0000) >> 16;
    int op4 = opcode & 0x0000000F;
    int op5 = (opcode & 0x000000E0) >> 5;

    // Write to a CP15 register
    core->cp15.write(op3, op4, op5, op2);

    return 1;
}

FORCE_INLINE int Interpreter::ldrsbRegT(uint16_t opcode) // LDRSB Rd,[Rb,Ro]
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = *registers[(opcode & 0x01C0) >> 6];

    // Signed byte load, pre-adjust without writeback
    *op0 = core->memory.read<int8_t>(cpu, op1 + op2);

    return ((cpu == 0) ? 1 : 3);
}

FORCE_INLINE int Interpreter::ldrshRegT(uint16_t opcode) // LDRSH Rd,[Rb,Ro]
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = *registers[(opcode & 0x01C0) >> 6];

    // Signed half-word load, pre-adjust without writeback
    *op0 = core->memory.read<int16_t>(cpu, op1 += op2);

    // Shift misaligned reads on ARM7
    if (cpu == 1 && (op1 & 1))
        *op0 = (int16_t)*op0 >> 8;

    return ((cpu == 0) ? 1 : 3);
}

FORCE_INLINE int Interpreter::ldrbRegT(uint16_t opcode) // LDRB Rd,[Rb,Ro]
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = *registers[(opcode & 0x01C0) >> 6];

    // Byte load, pre-adjust without writeback
    *op0 = core->memory.read<uint8_t>(cpu, op1 + op2);

    return ((cpu == 0) ? 1 : 3);
}

FORCE_INLINE int Interpreter::strbRegT(uint16_t opcode) // STRB Rd,[Rb,Ro]
{
    // Decode the operands
    uint32_t op0 = *registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = *registers[(opcode & 0x01C0) >> 6];

    // Byte write, pre-adjust without writeback
    core->memory.write<uint8_t>(cpu, op1 + op2, op0);

    return ((cpu == 0) ? 1 : 2);
}

FORCE_INLINE int Interpreter::ldrhRegT(uint16_t opcode) // LDRH Rd,[Rb,Ro]
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = *registers[(opcode & 0x01C0) >> 6];

    // Half-word load, pre-adjust without writeback
    *op0 = core->memory.read<uint16_t>(cpu, op1 += op2);

    // Rotate misaligned reads on ARM7
    if (cpu == 1 && (op1 & 1))
        *op0 = (*op0 << 24) | (*op0 >> 8);

    return ((cpu == 0) ? 1 : 3);
}

FORCE_INLINE int Interpreter::strhRegT(uint16_t opcode) // STRH Rd,[Rb,Ro]
{
    // Decode the operands
    uint32_t op0 = *registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = *registers[(opcode & 0x01C0) >> 6];

    // Half-word write, pre-adjust without writeback
    core->memory.write<uint16_t>(cpu, op1 + op2, op0);

    return ((cpu == 0) ? 1 : 2);
}

FORCE_INLINE int Interpreter::ldrRegT(uint16_t opcode) // LDR Rd,[Rb,Ro]
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = *registers[(opcode & 0x01C0) >> 6];

    // Word load, pre-adjust without writeback
    *op0 = core->memory.read<uint32_t>(cpu, op1 += op2);

    // Rotate misaligned reads
    if (op1 & 3)
    {
        int shift = (op1 & 3) * 8;
        *op0 = (*op0 << (32 - shift)) | (*op0 >> shift);
    }

    return ((cpu == 0) ? 1 : 3);
}

FORCE_INLINE int Interpreter::strRegT(uint16_t opcode) // STR Rd,[Rb,Ro]
{
    // Decode the operands
    uint32_t op0 = *registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = *registers[(opcode & 0x01C0) >> 6];

    // Word write, pre-adjust without writeback
    core->memory.write<uint32_t>(cpu, op1 + op2, op0);

    return ((cpu == 0) ? 1 : 2);
}

FORCE_INLINE int Interpreter::ldrbImm5T(uint16_t opcode) // LDRB Rd,[Rb,#i]
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = (opcode & 0x07C0) >> 6;

    // Byte load, pre-adjust without writeback
    *op0 = core->memory.read<uint8_t>(cpu, op1 + op2);

    return ((cpu == 0) ? 1 : 3);
}

FORCE_INLINE int Interpreter::strbImm5T(uint16_t opcode) // STRB Rd,[Rb,#i]
{
    // Decode the operands
    uint32_t op0 = *registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = (opcode & 0x07C0) >> 6;

    // Byte store, pre-adjust without writeback
    core->memory.write<uint8_t>(cpu, op1 + op2, op0);

    return ((cpu == 0) ? 1 : 2);
}

FORCE_INLINE int Interpreter::ldrhImm5T(uint16_t opcode) // LDRH Rd,[Rb,#i]
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = (opcode & 0x07C0) >> 5;

    // Half-word load, pre-adjust without writeback
    *op0 = core->memory.read<uint16_t>(cpu, op1 += op2);

    // Rotate misaligned reads on ARM7
    if (cpu == 1 && (op1 & 1))
        *op0 = (*op0 << 24) | (*op0 >> 8);

    return ((cpu == 0) ? 1 : 3);
}

FORCE_INLINE int Interpreter::strhImm5T(uint16_t opcode) // STRH Rd,[Rb,#i]
{
    // Decode the operands
    uint32_t op0 = *registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = (opcode & 0x07C0) >> 5;

    // Half-word store, pre-adjust without writeback
    core->memory.write<uint16_t>(cpu, op1 + op2, op0);

    return ((cpu == 0) ? 1 : 2);
}

FORCE_INLINE int Interpreter::ldrImm5T(uint16_t opcode) // LDR Rd,[Rb,#i]
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = (opcode & 0x07C0) >> 4;

    // Word load, pre-adjust without writeback
    *op0 = core->memory.read<uint32_t>(cpu, op1 += op2);

    // Rotate misaligned reads
    if (op1 & 3)
    {
        int shift = (op1 & 3) * 8;
        *op0 = (*op0 << (32 - shift)) | (*op0 >> shift);
    }

    return ((cpu == 0) ? 1 : 3);
}

FORCE_INLINE int Interpreter::strImm5T(uint16_t opcode) // STR Rd,[Rb,#i]
{
    // Decode the operands
    uint32_t op0 = *registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = (opcode & 0x07C0) >> 4;

    // Word store, pre-adjust without writeback
    core->memory.write<uint32_t>(cpu, op1 + op2, op0);

    return ((cpu == 0) ? 1 : 2);
}

FORCE_INLINE int Interpreter::ldrPcT(uint16_t opcode) // LDR Rd,[PC,#i]
{
    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x0700) >> 8];
    uint32_t op1 = *registers[15] & ~3;
    uint32_t op2 = (opcode & 0x00FF) << 2;

    // Word load, pre-adjust without writeback
    *op0 = core->memory.read<uint32_t>(cpu, op1 += op2);

    // Rotate misaligned reads
    if (op1 & 3)
    {
        int shift = (op1 & 3) * 8;
        *op0 = (*op0 << (32 - shift)) | (*op0 >> shift);
    }

    return ((cpu == 0) ? 1 : 3);
}

FORCE_INLINE int Interpreter::ldrSpT(uint16_t opcode) // LDR Rd,[SP,#i]
{
    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x0700) >> 8];
    uint32_t op1 = *registers[13];
    uint32_t op2 = (opcode & 0x00FF) << 2;

    // Word load, pre-adjust without writeback
    *op0 = core->memory.read<uint32_t>(cpu, op1 += op2);

    // Rotate misaligned reads
    if (op1 & 3)
    {
        int shift = (op1 & 3) * 8;
        *op0 = (*op0 << (32 - shift)) | (*op0 >> shift);
    }

    return ((cpu == 0) ? 1 : 3);
}

FORCE_INLINE int Interpreter::strSpT(uint16_t opcode) // STR Rd,[SP,#i]
{
    // Decode the operands
    uint32_t op0 = *registers[(opcode & 0x0700) >> 8];
    uint32_t op1 = *registers[13];
    uint32_t op2 = (opcode & 0x00FF) << 2;

    // Word store, pre-adjust without writeback
    core->memory.write<uint32_t>(cpu, op1 + op2, op0);

    return ((cpu == 0) ? 1 : 2);
}

FORCE_INLINE int Interpreter::ldmiaT(uint16_t opcode) // LDMIA Rb!,<Rlist>
{
    // Decode the operand
    int m = (opcode & 0x0700) >> 8;
    uint32_t op0 = *registers[m];
    int n = 0;

    // Block load, post-increment
    for (int i = 0; i < 8; i++)
    {
        if (opcode & BIT(i))
        {
            *registers[i] = core->memory.read<uint32_t>(cpu, op0);
            op0 += 4;
            n++;
        }
    }

    // Writeback
    // On ARM9, if Rn is in Rlist, writeback only happens if Rn is the only register, or not the last
    // On ARM7, if Rn is in Rlist, writeback never happens
    if (!(opcode & BIT(m)) || (cpu == 0 && ((opcode & 0x00FF) == BIT(m) || (opcode & 0x00FF & ~(BIT(m + 1) - 1)))))
        *registers[m] = op0;

    return n + ((cpu == 0) ? ((n > 1) ? 0 : 1) : 2);
}

FORCE_INLINE int Interpreter::stmiaT(uint16_t opcode) // STMIA Rb!,<Rlist>
{
    // Decode the operand
    int m = (opcode & 0x0700) >> 8;
    uint32_t op0 = *registers[m];
    int n = 0;

    // On ARM9, if Rn is in Rlist, the writeback value is never stored
    // On ARM7, if Rn is in Rlist, the writeback value is stored if Rn is not the first register
    if (cpu == 1 && (opcode & BIT(m)) && (opcode & (BIT(m) - 1)))
    {
        uint32_t writeback = op0;
        for (int i = 0; i < 8; i++)
        {
            if (opcode & BIT(i))
                writeback += 4;
        }
        *registers[m] = writeback;
    }

    // Block store, post-increment
    for (int i = 0; i < 8; i++)
    {
        if (opcode & BIT(i))
        {
            core->memory.write<uint32_t>(cpu, op0, *registers[i]);
            op0 += 4;
            n++;
        }
    }

    // Writeback
    *registers[m] = op0;

    return n + ((cpu == 0 && n > 1) ? 0 : 1);
}

FORCE_INLINE int Interpreter::popT(uint16_t opcode) // POP <Rlist>
{
    // Decode the operand
    uint32_t op0 = *registers[13];
    int n = 0;

    // Block load, post-increment
    for (int i = 0; i < 8; i++)
    {
        if (opcode & BIT(i))
        {
            *registers[i] = core->memory.read<uint32_t>(cpu, op0);
            op0 += 4;
            n++;
        }
    }

    // Writeback
    *registers[13] = op0;

    return n + ((cpu == 0) ? ((n > 1) ? 0 : 1) : 2);
}

FORCE_INLINE int Interpreter::pushT(uint16_t opcode) // PUSH <Rlist>
{
    // Decode the operand
    uint32_t op0 = *registers[13];
    int n = 0;

    // Decrement the address beforehand because transfers are always done in increasing order
    for (int i = 0; i < 8; i++)
    {
        if (opcode & BIT(i))
        {
            op0 -= 4;
            n++;
        }
    }

    // Writeback
    *registers[13] = op0;

    // Block store, pre-decrement
    for (int i = 0; i < 8; i++)
    {
        if (opcode & BIT(i))
        {
            core->memory.write<uint32_t>(cpu, op0, *registers[i]);
            op0 += 4;
        }
    }

    return n + ((cpu == 0 && n > 1) ? 0 : 1);
}

FORCE_INLINE int Interpreter::popPcT(uint16_t opcode) // POP <Rlist>,PC
{
    // Decode the operand
    uint32_t op0 = *registers[13];
    int n = 1;

    // Block load, post-increment
    for (int i = 0; i < 8; i++)
    {
        if (opcode & BIT(i))
        {
            *registers[i] = core->memory.read<uint32_t>(cpu, op0);
            op0 += 4;
            n++;
        }
    }
    *registers[15] = core->memory.read<uint32_t>(cpu, op0);

    // Writeback
    *registers[13] = op0 + 4;

    // Handle pipelining
    if (cpu == 1 || (*registers[15] & BIT(0)))
    {
        *registers[15] = (*registers[15] & ~1) + 2;
    }
    else
    {
        cpsr &= ~BIT(5);
        *registers[15] = (*registers[15] & ~3) + 4;
    }

    return n + 4;
}

FORCE_INLINE int Interpreter::pushLrT(uint16_t opcode) // PUSH <Rlist>,LR
{
    // Decode the operand
    uint32_t op0 = *registers[13] - 4;
    int n = 1;

    // Decrement the address beforehand because transfers are always done in increasing order
    for (int i = 0; i < 8; i++)
    {
        if (opcode & BIT(i))
        {
            op0 -= 4;
            n++;
        }
    }

    // Writeback
    *registers[13] = op0;

    // Block store, pre-decrement
    for (int i = 0; i < 8; i++)
    {
        if (opcode & BIT(i))
        {
            core->memory.write<uint32_t>(cpu, op0, *registers[i]);
            op0 += 4;
        }
    }
    core->memory.write<uint32_t>(cpu, op0, *registers[14]);

    return n + ((cpu == 0 && n > 1) ? 0 : 1);
}

#endif // INTERPRETER_TRANSFER
