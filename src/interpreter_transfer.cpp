/*
    Copyright 2019-2023 Hydr8gon

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

// Precomputed bit counts corresponding to index
const uint8_t Interpreter::bitCount[] =
{
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8
};

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
    *op0 = (int8_t)core->memory.read<uint8_t>(cpu, op1 + op2);

    // Handle pipelining
    if (op0 != registers[15]) return ((cpu == 0) ? 1 : 3);
    flushPipeline();
    return 5;
}

FORCE_INLINE int Interpreter::ldrshOf(uint32_t opcode, uint32_t op2) // LDRSH Rd,[Rn,op2]
{
    // Decode the other operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16];

    // Signed half-word load, pre-adjust without writeback
    *op0 = (int16_t)core->memory.read<uint16_t>(cpu, op1 += op2);

    // Shift misaligned reads on ARM7
    if (cpu == 1 && (op1 & 1))
        *op0 = (int16_t)*op0 >> 8;

    // Handle pipelining
    if (op0 != registers[15]) return ((cpu == 0) ? 1 : 3);
    flushPipeline();
    return 5;
}

FORCE_INLINE int Interpreter::ldrbOf(uint32_t opcode, uint32_t op2) // LDRB Rd,[Rn,op2]
{
    // Decode the other operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16];

    // Byte load, pre-adjust without writeback
    *op0 = core->memory.read<uint8_t>(cpu, op1 + op2);

    // Handle pipelining and THUMB switching
    if (op0 != registers[15]) return ((cpu == 0) ? 1 : 3);
    if (cpu == 0 && (*op0 & BIT(0))) cpsr |= BIT(5);
    flushPipeline();
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
    flushPipeline();
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

    // Handle pipelining and THUMB switching
    if (op0 != registers[15]) return ((cpu == 0) ? 1 : 3);
    if (cpu == 0 && (*op0 & BIT(0))) cpsr |= BIT(5);
    flushPipeline();
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
    *op0 = (int8_t)core->memory.read<uint8_t>(cpu, *op1);

    // Handle pipelining
    if (op0 != registers[15]) return ((cpu == 0) ? 1 : 3);
    flushPipeline();
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
    *op0 = (int16_t)core->memory.read<uint16_t>(cpu, address = *op1);

    // Shift misaligned reads on ARM7
    if (cpu == 1 && (address & 1))
        *op0 = (int16_t)*op0 >> 8;

    // Handle pipelining
    if (op0 != registers[15]) return ((cpu == 0) ? 1 : 3);
    flushPipeline();
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

    // Handle pipelining and THUMB switching
    if (op0 != registers[15]) return ((cpu == 0) ? 1 : 3);
    if (cpu == 0 && (*op0 & BIT(0))) cpsr |= BIT(5);
    flushPipeline();
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
    flushPipeline();
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

    // Handle pipelining and THUMB switching
    if (op0 != registers[15]) return ((cpu == 0) ? 1 : 3);
    if (cpu == 0 && (*op0 & BIT(0))) cpsr |= BIT(5);
    flushPipeline();
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
    *op0 = (int8_t)core->memory.read<uint8_t>(cpu, *op1);
    if (op0 != op1) *op1 += op2;

    // Handle pipelining
    if (op0 != registers[15]) return ((cpu == 0) ? 1 : 3);
    flushPipeline();
    return 5;
}

FORCE_INLINE int Interpreter::ldrshPt(uint32_t opcode, uint32_t op2) // LDRSH Rd,[Rn],op2
{
    // Decode the other operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t address;

    // Signed half-word load, post-adjust
    *op0 = (int16_t)core->memory.read<uint16_t>(cpu, address = *op1);
    if (op0 != op1) *op1 += op2;

    // Shift misaligned reads on ARM7
    if (cpu == 1 && (address & 1))
        *op0 = (int16_t)*op0 >> 8;

    // Handle pipelining
    if (op0 != registers[15]) return ((cpu == 0) ? 1 : 3);
    flushPipeline();
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

    // Handle pipelining and THUMB switching
    if (op0 != registers[15]) return ((cpu == 0) ? 1 : 3);
    if (cpu == 0 && (*op0 & BIT(0))) cpsr |= BIT(5);
    flushPipeline();
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
    flushPipeline();
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

    // Handle pipelining and THUMB switching
    if (op0 != registers[15]) return ((cpu == 0) ? 1 : 3);
    if (cpu == 0 && (*op0 & BIT(0))) cpsr |= BIT(5);
    flushPipeline();
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

int Interpreter::ldrsbOfrm(uint32_t opcode) { return ldrsbOf(opcode, -rp(opcode));  } // LDRSB Rd,[Rn,-Rm]
int Interpreter::ldrsbOfim(uint32_t opcode) { return ldrsbOf(opcode, -ipH(opcode)); } // LDRSB Rd,[Rn,-#i]
int Interpreter::ldrsbOfrp(uint32_t opcode) { return ldrsbOf(opcode,  rp(opcode));  } // LDRSB Rd,[Rn,Rm]
int Interpreter::ldrsbOfip(uint32_t opcode) { return ldrsbOf(opcode,  ipH(opcode)); } // LDRSB Rd,[Rn,#i]
int Interpreter::ldrsbPrrm(uint32_t opcode) { return ldrsbPr(opcode, -rp(opcode));  } // LDRSB Rd,[Rn,-Rm]!
int Interpreter::ldrsbPrim(uint32_t opcode) { return ldrsbPr(opcode, -ipH(opcode)); } // LDRSB Rd,[Rn,-#i]!
int Interpreter::ldrsbPrrp(uint32_t opcode) { return ldrsbPr(opcode,  rp(opcode));  } // LDRSB Rd,[Rn,Rm]!
int Interpreter::ldrsbPrip(uint32_t opcode) { return ldrsbPr(opcode,  ipH(opcode)); } // LDRSB Rd,[Rn,#i]!
int Interpreter::ldrsbPtrm(uint32_t opcode) { return ldrsbPt(opcode, -rp(opcode));  } // LDRSB Rd,[Rn],-Rm
int Interpreter::ldrsbPtim(uint32_t opcode) { return ldrsbPt(opcode, -ipH(opcode)); } // LDRSB Rd,[Rn],-#i
int Interpreter::ldrsbPtrp(uint32_t opcode) { return ldrsbPt(opcode,  rp(opcode));  } // LDRSB Rd,[Rn],Rm
int Interpreter::ldrsbPtip(uint32_t opcode) { return ldrsbPt(opcode,  ipH(opcode)); } // LDRSB Rd,[Rn],#i
int Interpreter::ldrshOfrm(uint32_t opcode) { return ldrshOf(opcode, -rp(opcode));  } // LDRSH Rd,[Rn,-Rm]
int Interpreter::ldrshOfim(uint32_t opcode) { return ldrshOf(opcode, -ipH(opcode)); } // LDRSH Rd,[Rn,-#i]
int Interpreter::ldrshOfrp(uint32_t opcode) { return ldrshOf(opcode,  rp(opcode));  } // LDRSH Rd,[Rn,Rm]
int Interpreter::ldrshOfip(uint32_t opcode) { return ldrshOf(opcode,  ipH(opcode)); } // LDRSH Rd,[Rn,#i]
int Interpreter::ldrshPrrm(uint32_t opcode) { return ldrshPr(opcode, -rp(opcode));  } // LDRSH Rd,[Rn,-Rm]!
int Interpreter::ldrshPrim(uint32_t opcode) { return ldrshPr(opcode, -ipH(opcode)); } // LDRSH Rd,[Rn,-#i]!
int Interpreter::ldrshPrrp(uint32_t opcode) { return ldrshPr(opcode,  rp(opcode));  } // LDRSH Rd,[Rn,Rm]!
int Interpreter::ldrshPrip(uint32_t opcode) { return ldrshPr(opcode,  ipH(opcode)); } // LDRSH Rd,[Rn,#i]!
int Interpreter::ldrshPtrm(uint32_t opcode) { return ldrshPt(opcode, -rp(opcode));  } // LDRSH Rd,[Rn],-Rm
int Interpreter::ldrshPtim(uint32_t opcode) { return ldrshPt(opcode, -ipH(opcode)); } // LDRSH Rd,[Rn],-#i
int Interpreter::ldrshPtrp(uint32_t opcode) { return ldrshPt(opcode,  rp(opcode));  } // LDRSH Rd,[Rn],Rm
int Interpreter::ldrshPtip(uint32_t opcode) { return ldrshPt(opcode,  ipH(opcode)); } // LDRSH Rd,[Rn],#i

int Interpreter::ldrbOfim(uint32_t opcode)   { return ldrbOf(opcode, -ip(opcode));   } // LDRB Rd,[Rn,-#i]
int Interpreter::ldrbOfip(uint32_t opcode)   { return ldrbOf(opcode,  ip(opcode));   } // LDRB Rd,[Rn,#i]
int Interpreter::ldrbOfrmll(uint32_t opcode) { return ldrbOf(opcode, -rpll(opcode)); } // LDRB Rd,[Rn,-Rm,LSL #i]
int Interpreter::ldrbOfrmlr(uint32_t opcode) { return ldrbOf(opcode, -rplr(opcode)); } // LDRB Rd,[Rn,-Rm,LSR #i]
int Interpreter::ldrbOfrmar(uint32_t opcode) { return ldrbOf(opcode, -rpar(opcode)); } // LDRB Rd,[Rn,-Rm,ASR #i]
int Interpreter::ldrbOfrmrr(uint32_t opcode) { return ldrbOf(opcode, -rprr(opcode)); } // LDRB Rd,[Rn,-Rm,ROR #i]
int Interpreter::ldrbOfrpll(uint32_t opcode) { return ldrbOf(opcode,  rpll(opcode)); } // LDRB Rd,[Rn,Rm,LSL #i]
int Interpreter::ldrbOfrplr(uint32_t opcode) { return ldrbOf(opcode,  rplr(opcode)); } // LDRB Rd,[Rn,Rm,LSR #i]
int Interpreter::ldrbOfrpar(uint32_t opcode) { return ldrbOf(opcode,  rpar(opcode)); } // LDRB Rd,[Rn,Rm,ASR #i]
int Interpreter::ldrbOfrprr(uint32_t opcode) { return ldrbOf(opcode,  rprr(opcode)); } // LDRB Rd,[Rn,Rm,ROR #i]
int Interpreter::ldrbPrim(uint32_t opcode)   { return ldrbPr(opcode, -ip(opcode));   } // LDRB Rd,[Rn,-#i]!
int Interpreter::ldrbPrip(uint32_t opcode)   { return ldrbPr(opcode,  ip(opcode));   } // LDRB Rd,[Rn,#i]!
int Interpreter::ldrbPrrmll(uint32_t opcode) { return ldrbPr(opcode, -rpll(opcode)); } // LDRB Rd,[Rn,-Rm,LSL #i]!
int Interpreter::ldrbPrrmlr(uint32_t opcode) { return ldrbPr(opcode, -rplr(opcode)); } // LDRB Rd,[Rn,-Rm,LSR #i]!
int Interpreter::ldrbPrrmar(uint32_t opcode) { return ldrbPr(opcode, -rpar(opcode)); } // LDRB Rd,[Rn,-Rm,ASR #i]!
int Interpreter::ldrbPrrmrr(uint32_t opcode) { return ldrbPr(opcode, -rprr(opcode)); } // LDRB Rd,[Rn,-Rm,ROR #i]!
int Interpreter::ldrbPrrpll(uint32_t opcode) { return ldrbPr(opcode,  rpll(opcode)); } // LDRB Rd,[Rn,Rm,LSL #i]!
int Interpreter::ldrbPrrplr(uint32_t opcode) { return ldrbPr(opcode,  rplr(opcode)); } // LDRB Rd,[Rn,Rm,LSR #i]!
int Interpreter::ldrbPrrpar(uint32_t opcode) { return ldrbPr(opcode,  rpar(opcode)); } // LDRB Rd,[Rn,Rm,ASR #i]!
int Interpreter::ldrbPrrprr(uint32_t opcode) { return ldrbPr(opcode,  rprr(opcode)); } // LDRB Rd,[Rn,Rm,ROR #i]!
int Interpreter::ldrbPtim(uint32_t opcode)   { return ldrbPt(opcode, -ip(opcode));   } // LDRB Rd,[Rn],-#i
int Interpreter::ldrbPtip(uint32_t opcode)   { return ldrbPt(opcode,  ip(opcode));   } // LDRB Rd,[Rn],#i
int Interpreter::ldrbPtrmll(uint32_t opcode) { return ldrbPt(opcode, -rpll(opcode)); } // LDRB Rd,[Rn],-Rm,LSL #i
int Interpreter::ldrbPtrmlr(uint32_t opcode) { return ldrbPt(opcode, -rplr(opcode)); } // LDRB Rd,[Rn],-Rm,LSR #i
int Interpreter::ldrbPtrmar(uint32_t opcode) { return ldrbPt(opcode, -rpar(opcode)); } // LDRB Rd,[Rn],-Rm,ASR #i
int Interpreter::ldrbPtrmrr(uint32_t opcode) { return ldrbPt(opcode, -rprr(opcode)); } // LDRB Rd,[Rn],-Rm,ROR #i
int Interpreter::ldrbPtrpll(uint32_t opcode) { return ldrbPt(opcode,  rpll(opcode)); } // LDRB Rd,[Rn],Rm,LSL #i
int Interpreter::ldrbPtrplr(uint32_t opcode) { return ldrbPt(opcode,  rplr(opcode)); } // LDRB Rd,[Rn],Rm,LSR #i
int Interpreter::ldrbPtrpar(uint32_t opcode) { return ldrbPt(opcode,  rpar(opcode)); } // LDRB Rd,[Rn],Rm,ASR #i
int Interpreter::ldrbPtrprr(uint32_t opcode) { return ldrbPt(opcode,  rprr(opcode)); } // LDRB Rd,[Rn],Rm,ROR #i

int Interpreter::strbOfim(uint32_t opcode)   { return strbOf(opcode, -ip(opcode));   } // STRB Rd,[Rn,-#i]
int Interpreter::strbOfip(uint32_t opcode)   { return strbOf(opcode,  ip(opcode));   } // STRB Rd,[Rn,#i]
int Interpreter::strbOfrmll(uint32_t opcode) { return strbOf(opcode, -rpll(opcode)); } // STRB Rd,[Rn,-Rm,LSL #i]
int Interpreter::strbOfrmlr(uint32_t opcode) { return strbOf(opcode, -rplr(opcode)); } // STRB Rd,[Rn,-Rm,LSR #i]
int Interpreter::strbOfrmar(uint32_t opcode) { return strbOf(opcode, -rpar(opcode)); } // STRB Rd,[Rn,-Rm,ASR #i]
int Interpreter::strbOfrmrr(uint32_t opcode) { return strbOf(opcode, -rprr(opcode)); } // STRB Rd,[Rn,-Rm,ROR #i]
int Interpreter::strbOfrpll(uint32_t opcode) { return strbOf(opcode,  rpll(opcode)); } // STRB Rd,[Rn,Rm,LSL #i]
int Interpreter::strbOfrplr(uint32_t opcode) { return strbOf(opcode,  rplr(opcode)); } // STRB Rd,[Rn,Rm,LSR #i]
int Interpreter::strbOfrpar(uint32_t opcode) { return strbOf(opcode,  rpar(opcode)); } // STRB Rd,[Rn,Rm,ASR #i]
int Interpreter::strbOfrprr(uint32_t opcode) { return strbOf(opcode,  rprr(opcode)); } // STRB Rd,[Rn,Rm,ROR #i]
int Interpreter::strbPrim(uint32_t opcode)   { return strbPr(opcode, -ip(opcode));   } // STRB Rd,[Rn,-#i]!
int Interpreter::strbPrip(uint32_t opcode)   { return strbPr(opcode,  ip(opcode));   } // STRB Rd,[Rn,#i]!
int Interpreter::strbPrrmll(uint32_t opcode) { return strbPr(opcode, -rpll(opcode)); } // STRB Rd,[Rn,-Rm,LSL #i]!
int Interpreter::strbPrrmlr(uint32_t opcode) { return strbPr(opcode, -rplr(opcode)); } // STRB Rd,[Rn,-Rm,LSR #i]!
int Interpreter::strbPrrmar(uint32_t opcode) { return strbPr(opcode, -rpar(opcode)); } // STRB Rd,[Rn,-Rm,ASR #i]!
int Interpreter::strbPrrmrr(uint32_t opcode) { return strbPr(opcode, -rprr(opcode)); } // STRB Rd,[Rn,-Rm,ROR #i]!
int Interpreter::strbPrrpll(uint32_t opcode) { return strbPr(opcode,  rpll(opcode)); } // STRB Rd,[Rn,Rm,LSL #i]!
int Interpreter::strbPrrplr(uint32_t opcode) { return strbPr(opcode,  rplr(opcode)); } // STRB Rd,[Rn,Rm,LSR #i]!
int Interpreter::strbPrrpar(uint32_t opcode) { return strbPr(opcode,  rpar(opcode)); } // STRB Rd,[Rn,Rm,ASR #i]!
int Interpreter::strbPrrprr(uint32_t opcode) { return strbPr(opcode,  rprr(opcode)); } // STRB Rd,[Rn,Rm,ROR #i]!
int Interpreter::strbPtim(uint32_t opcode)   { return strbPt(opcode, -ip(opcode));   } // STRB Rd,[Rn],-#i
int Interpreter::strbPtip(uint32_t opcode)   { return strbPt(opcode,  ip(opcode));   } // STRB Rd,[Rn],#i
int Interpreter::strbPtrmll(uint32_t opcode) { return strbPt(opcode, -rpll(opcode)); } // STRB Rd,[Rn],-Rm,LSL #i
int Interpreter::strbPtrmlr(uint32_t opcode) { return strbPt(opcode, -rplr(opcode)); } // STRB Rd,[Rn],-Rm,LSR #i
int Interpreter::strbPtrmar(uint32_t opcode) { return strbPt(opcode, -rpar(opcode)); } // STRB Rd,[Rn],-Rm,ASR #i
int Interpreter::strbPtrmrr(uint32_t opcode) { return strbPt(opcode, -rprr(opcode)); } // STRB Rd,[Rn],-Rm,ROR #i
int Interpreter::strbPtrpll(uint32_t opcode) { return strbPt(opcode,  rpll(opcode)); } // STRB Rd,[Rn],Rm,LSL #i
int Interpreter::strbPtrplr(uint32_t opcode) { return strbPt(opcode,  rplr(opcode)); } // STRB Rd,[Rn],Rm,LSR #i
int Interpreter::strbPtrpar(uint32_t opcode) { return strbPt(opcode,  rpar(opcode)); } // STRB Rd,[Rn],Rm,ASR #i
int Interpreter::strbPtrprr(uint32_t opcode) { return strbPt(opcode,  rprr(opcode)); } // STRB Rd,[Rn],Rm,ROR #i

int Interpreter::ldrhOfrm(uint32_t opcode) { return ldrhOf(opcode, -rp(opcode));  } // LDRH Rd,[Rn,-Rm]
int Interpreter::ldrhOfim(uint32_t opcode) { return ldrhOf(opcode, -ipH(opcode)); } // LDRH Rd,[Rn,-#i]
int Interpreter::ldrhOfrp(uint32_t opcode) { return ldrhOf(opcode,  rp(opcode));  } // LDRH Rd,[Rn,Rm]
int Interpreter::ldrhOfip(uint32_t opcode) { return ldrhOf(opcode,  ipH(opcode)); } // LDRH Rd,[Rn,#i]
int Interpreter::ldrhPrrm(uint32_t opcode) { return ldrhPr(opcode, -rp(opcode));  } // LDRH Rd,[Rn,-Rm]!
int Interpreter::ldrhPrim(uint32_t opcode) { return ldrhPr(opcode, -ipH(opcode)); } // LDRH Rd,[Rn,-#i]!
int Interpreter::ldrhPrrp(uint32_t opcode) { return ldrhPr(opcode,  rp(opcode));  } // LDRH Rd,[Rn,Rm]!
int Interpreter::ldrhPrip(uint32_t opcode) { return ldrhPr(opcode,  ipH(opcode)); } // LDRH Rd,[Rn,#i]!
int Interpreter::ldrhPtrm(uint32_t opcode) { return ldrhPt(opcode, -rp(opcode));  } // LDRH Rd,[Rn],-Rm
int Interpreter::ldrhPtim(uint32_t opcode) { return ldrhPt(opcode, -ipH(opcode)); } // LDRH Rd,[Rn],-#i
int Interpreter::ldrhPtrp(uint32_t opcode) { return ldrhPt(opcode,  rp(opcode));  } // LDRH Rd,[Rn],Rm
int Interpreter::ldrhPtip(uint32_t opcode) { return ldrhPt(opcode,  ipH(opcode)); } // LDRH Rd,[Rn],#i
int Interpreter::strhOfrm(uint32_t opcode) { return strhOf(opcode, -rp(opcode));  } // STRH Rd,[Rn,-Rm]
int Interpreter::strhOfim(uint32_t opcode) { return strhOf(opcode, -ipH(opcode)); } // STRH Rd,[Rn,-#i]
int Interpreter::strhOfrp(uint32_t opcode) { return strhOf(opcode,  rp(opcode));  } // STRH Rd,[Rn,Rm]
int Interpreter::strhOfip(uint32_t opcode) { return strhOf(opcode,  ipH(opcode)); } // STRH Rd,[Rn,#i]
int Interpreter::strhPrrm(uint32_t opcode) { return strhPr(opcode, -rp(opcode));  } // STRH Rd,[Rn,-Rm]!
int Interpreter::strhPrim(uint32_t opcode) { return strhPr(opcode, -ipH(opcode)); } // STRH Rd,[Rn,-#i]!
int Interpreter::strhPrrp(uint32_t opcode) { return strhPr(opcode,  rp(opcode));  } // STRH Rd,[Rn,Rm]!
int Interpreter::strhPrip(uint32_t opcode) { return strhPr(opcode,  ipH(opcode)); } // STRH Rd,[Rn,#i]!
int Interpreter::strhPtrm(uint32_t opcode) { return strhPt(opcode, -rp(opcode));  } // STRH Rd,[Rn],-Rm
int Interpreter::strhPtim(uint32_t opcode) { return strhPt(opcode, -ipH(opcode)); } // STRH Rd,[Rn],-#i
int Interpreter::strhPtrp(uint32_t opcode) { return strhPt(opcode,  rp(opcode));  } // STRH Rd,[Rn],Rm
int Interpreter::strhPtip(uint32_t opcode) { return strhPt(opcode,  ipH(opcode)); } // STRH Rd,[Rn],#i

int Interpreter::ldrOfim(uint32_t opcode)   { return ldrOf(opcode, -ip(opcode));   } // LDR Rd,[Rn,-#i]
int Interpreter::ldrOfip(uint32_t opcode)   { return ldrOf(opcode,  ip(opcode));   } // LDR Rd,[Rn,#i]
int Interpreter::ldrOfrmll(uint32_t opcode) { return ldrOf(opcode, -rpll(opcode)); } // LDR Rd,[Rn,-Rm,LSL #i]
int Interpreter::ldrOfrmlr(uint32_t opcode) { return ldrOf(opcode, -rplr(opcode)); } // LDR Rd,[Rn,-Rm,LSR #i]
int Interpreter::ldrOfrmar(uint32_t opcode) { return ldrOf(opcode, -rpar(opcode)); } // LDR Rd,[Rn,-Rm,ASR #i]
int Interpreter::ldrOfrmrr(uint32_t opcode) { return ldrOf(opcode, -rprr(opcode)); } // LDR Rd,[Rn,-Rm,ROR #i]
int Interpreter::ldrOfrpll(uint32_t opcode) { return ldrOf(opcode,  rpll(opcode)); } // LDR Rd,[Rn,Rm,LSL #i]
int Interpreter::ldrOfrplr(uint32_t opcode) { return ldrOf(opcode,  rplr(opcode)); } // LDR Rd,[Rn,Rm,LSR #i]
int Interpreter::ldrOfrpar(uint32_t opcode) { return ldrOf(opcode,  rpar(opcode)); } // LDR Rd,[Rn,Rm,ASR #i]
int Interpreter::ldrOfrprr(uint32_t opcode) { return ldrOf(opcode,  rprr(opcode)); } // LDR Rd,[Rn,Rm,ROR #i]
int Interpreter::ldrPrim(uint32_t opcode)   { return ldrPr(opcode, -ip(opcode));   } // LDR Rd,[Rn,-#i]!
int Interpreter::ldrPrip(uint32_t opcode)   { return ldrPr(opcode,  ip(opcode));   } // LDR Rd,[Rn,#i]!
int Interpreter::ldrPrrmll(uint32_t opcode) { return ldrPr(opcode, -rpll(opcode)); } // LDR Rd,[Rn,-Rm,LSL #i]!
int Interpreter::ldrPrrmlr(uint32_t opcode) { return ldrPr(opcode, -rplr(opcode)); } // LDR Rd,[Rn,-Rm,LSR #i]!
int Interpreter::ldrPrrmar(uint32_t opcode) { return ldrPr(opcode, -rpar(opcode)); } // LDR Rd,[Rn,-Rm,ASR #i]!
int Interpreter::ldrPrrmrr(uint32_t opcode) { return ldrPr(opcode, -rprr(opcode)); } // LDR Rd,[Rn,-Rm,ROR #i]!
int Interpreter::ldrPrrpll(uint32_t opcode) { return ldrPr(opcode,  rpll(opcode)); } // LDR Rd,[Rn,Rm,LSL #i]!
int Interpreter::ldrPrrplr(uint32_t opcode) { return ldrPr(opcode,  rplr(opcode)); } // LDR Rd,[Rn,Rm,LSR #i]!
int Interpreter::ldrPrrpar(uint32_t opcode) { return ldrPr(opcode,  rpar(opcode)); } // LDR Rd,[Rn,Rm,ASR #i]!
int Interpreter::ldrPrrprr(uint32_t opcode) { return ldrPr(opcode,  rprr(opcode)); } // LDR Rd,[Rn,Rm,ROR #i]!
int Interpreter::ldrPtim(uint32_t opcode)   { return ldrPt(opcode, -ip(opcode));   } // LDR Rd,[Rn],-#i
int Interpreter::ldrPtip(uint32_t opcode)   { return ldrPt(opcode,  ip(opcode));   } // LDR Rd,[Rn],#i
int Interpreter::ldrPtrmll(uint32_t opcode) { return ldrPt(opcode, -rpll(opcode)); } // LDR Rd,[Rn],-Rm,LSL #i
int Interpreter::ldrPtrmlr(uint32_t opcode) { return ldrPt(opcode, -rplr(opcode)); } // LDR Rd,[Rn],-Rm,LSR #i
int Interpreter::ldrPtrmar(uint32_t opcode) { return ldrPt(opcode, -rpar(opcode)); } // LDR Rd,[Rn],-Rm,ASR #i
int Interpreter::ldrPtrmrr(uint32_t opcode) { return ldrPt(opcode, -rprr(opcode)); } // LDR Rd,[Rn],-Rm,ROR #i
int Interpreter::ldrPtrpll(uint32_t opcode) { return ldrPt(opcode,  rpll(opcode)); } // LDR Rd,[Rn],Rm,LSL #i
int Interpreter::ldrPtrplr(uint32_t opcode) { return ldrPt(opcode,  rplr(opcode)); } // LDR Rd,[Rn],Rm,LSR #i
int Interpreter::ldrPtrpar(uint32_t opcode) { return ldrPt(opcode,  rpar(opcode)); } // LDR Rd,[Rn],Rm,ASR #i
int Interpreter::ldrPtrprr(uint32_t opcode) { return ldrPt(opcode,  rprr(opcode)); } // LDR Rd,[Rn],Rm,ROR #i

int Interpreter::strOfim(uint32_t opcode)   { return strOf(opcode, -ip(opcode));   } // STR Rd,[Rn,-#i]
int Interpreter::strOfip(uint32_t opcode)   { return strOf(opcode,  ip(opcode));   } // STR Rd,[Rn,#i]
int Interpreter::strOfrmll(uint32_t opcode) { return strOf(opcode, -rpll(opcode)); } // STR Rd,[Rn,-Rm,LSL #i]
int Interpreter::strOfrmlr(uint32_t opcode) { return strOf(opcode, -rplr(opcode)); } // STR Rd,[Rn,-Rm,LSR #i]
int Interpreter::strOfrmar(uint32_t opcode) { return strOf(opcode, -rpar(opcode)); } // STR Rd,[Rn,-Rm,ASR #i]
int Interpreter::strOfrmrr(uint32_t opcode) { return strOf(opcode, -rprr(opcode)); } // STR Rd,[Rn,-Rm,ROR #i]
int Interpreter::strOfrpll(uint32_t opcode) { return strOf(opcode,  rpll(opcode)); } // STR Rd,[Rn,Rm,LSL #i]
int Interpreter::strOfrplr(uint32_t opcode) { return strOf(opcode,  rplr(opcode)); } // STR Rd,[Rn,Rm,LSR #i]
int Interpreter::strOfrpar(uint32_t opcode) { return strOf(opcode,  rpar(opcode)); } // STR Rd,[Rn,Rm,ASR #i]
int Interpreter::strOfrprr(uint32_t opcode) { return strOf(opcode,  rprr(opcode)); } // STR Rd,[Rn,Rm,ROR #i]
int Interpreter::strPrim(uint32_t opcode)   { return strPr(opcode, -ip(opcode));   } // STR Rd,[Rn,-#i]!
int Interpreter::strPrip(uint32_t opcode)   { return strPr(opcode,  ip(opcode));   } // STR Rd,[Rn,#i]!
int Interpreter::strPrrmll(uint32_t opcode) { return strPr(opcode, -rpll(opcode)); } // STR Rd,[Rn,-Rm,LSL #i]!
int Interpreter::strPrrmlr(uint32_t opcode) { return strPr(opcode, -rplr(opcode)); } // STR Rd,[Rn,-Rm,LSR #i]!
int Interpreter::strPrrmar(uint32_t opcode) { return strPr(opcode, -rpar(opcode)); } // STR Rd,[Rn,-Rm,ASR #i]!
int Interpreter::strPrrmrr(uint32_t opcode) { return strPr(opcode, -rprr(opcode)); } // STR Rd,[Rn,-Rm,ROR #i]!
int Interpreter::strPrrpll(uint32_t opcode) { return strPr(opcode,  rpll(opcode)); } // STR Rd,[Rn,Rm,LSL #i]!
int Interpreter::strPrrplr(uint32_t opcode) { return strPr(opcode,  rplr(opcode)); } // STR Rd,[Rn,Rm,LSR #i]!
int Interpreter::strPrrpar(uint32_t opcode) { return strPr(opcode,  rpar(opcode)); } // STR Rd,[Rn,Rm,ASR #i]!
int Interpreter::strPrrprr(uint32_t opcode) { return strPr(opcode,  rprr(opcode)); } // STR Rd,[Rn,Rm,ROR #i]!
int Interpreter::strPtim(uint32_t opcode)   { return strPt(opcode, -ip(opcode));   } // STR Rd,[Rn],-#i
int Interpreter::strPtip(uint32_t opcode)   { return strPt(opcode,  ip(opcode));   } // STR Rd,[Rn],#i
int Interpreter::strPtrmll(uint32_t opcode) { return strPt(opcode, -rpll(opcode)); } // STR Rd,[Rn],-Rm,LSL #i
int Interpreter::strPtrmlr(uint32_t opcode) { return strPt(opcode, -rplr(opcode)); } // STR Rd,[Rn],-Rm,LSR #i
int Interpreter::strPtrmar(uint32_t opcode) { return strPt(opcode, -rpar(opcode)); } // STR Rd,[Rn],-Rm,ASR #i
int Interpreter::strPtrmrr(uint32_t opcode) { return strPt(opcode, -rprr(opcode)); } // STR Rd,[Rn],-Rm,ROR #i
int Interpreter::strPtrpll(uint32_t opcode) { return strPt(opcode,  rpll(opcode)); } // STR Rd,[Rn],Rm,LSL #i
int Interpreter::strPtrplr(uint32_t opcode) { return strPt(opcode,  rplr(opcode)); } // STR Rd,[Rn],Rm,LSR #i
int Interpreter::strPtrpar(uint32_t opcode) { return strPt(opcode,  rpar(opcode)); } // STR Rd,[Rn],Rm,ASR #i
int Interpreter::strPtrprr(uint32_t opcode) { return strPt(opcode,  rprr(opcode)); } // STR Rd,[Rn],Rm,ROR #i

int Interpreter::ldrdOfrm(uint32_t opcode) { return ldrdOf(opcode, -rp(opcode));  } // LDRD Rd,[Rn,-Rm]
int Interpreter::ldrdOfim(uint32_t opcode) { return ldrdOf(opcode, -ipH(opcode)); } // LDRD Rd,[Rn,-#i]
int Interpreter::ldrdOfrp(uint32_t opcode) { return ldrdOf(opcode,  rp(opcode));  } // LDRD Rd,[Rn,Rm]
int Interpreter::ldrdOfip(uint32_t opcode) { return ldrdOf(opcode,  ipH(opcode)); } // LDRD Rd,[Rn,#i]
int Interpreter::ldrdPrrm(uint32_t opcode) { return ldrdPr(opcode, -rp(opcode));  } // LDRD Rd,[Rn,-Rm]!
int Interpreter::ldrdPrim(uint32_t opcode) { return ldrdPr(opcode, -ipH(opcode)); } // LDRD Rd,[Rn,-#i]!
int Interpreter::ldrdPrrp(uint32_t opcode) { return ldrdPr(opcode,  rp(opcode));  } // LDRD Rd,[Rn,Rm]!
int Interpreter::ldrdPrip(uint32_t opcode) { return ldrdPr(opcode,  ipH(opcode)); } // LDRD Rd,[Rn,#i]!
int Interpreter::ldrdPtrm(uint32_t opcode) { return ldrdPt(opcode, -rp(opcode));  } // LDRD Rd,[Rn],-Rm
int Interpreter::ldrdPtim(uint32_t opcode) { return ldrdPt(opcode, -ipH(opcode)); } // LDRD Rd,[Rn],-#i
int Interpreter::ldrdPtrp(uint32_t opcode) { return ldrdPt(opcode,  rp(opcode));  } // LDRD Rd,[Rn],Rm
int Interpreter::ldrdPtip(uint32_t opcode) { return ldrdPt(opcode,  ipH(opcode)); } // LDRD Rd,[Rn],#i
int Interpreter::strdOfrm(uint32_t opcode) { return strdOf(opcode, -rp(opcode));  } // STRD Rd,[Rn,-Rm]
int Interpreter::strdOfim(uint32_t opcode) { return strdOf(opcode, -ipH(opcode)); } // STRD Rd,[Rn,-#i]
int Interpreter::strdOfrp(uint32_t opcode) { return strdOf(opcode,  rp(opcode));  } // STRD Rd,[Rn,Rm]
int Interpreter::strdOfip(uint32_t opcode) { return strdOf(opcode,  ipH(opcode)); } // STRD Rd,[Rn,#i]
int Interpreter::strdPrrm(uint32_t opcode) { return strdPr(opcode, -rp(opcode));  } // STRD Rd,[Rn,-Rm]!
int Interpreter::strdPrim(uint32_t opcode) { return strdPr(opcode, -ipH(opcode)); } // STRD Rd,[Rn,-#i]!
int Interpreter::strdPrrp(uint32_t opcode) { return strdPr(opcode,  rp(opcode));  } // STRD Rd,[Rn,Rm]!
int Interpreter::strdPrip(uint32_t opcode) { return strdPr(opcode,  ipH(opcode)); } // STRD Rd,[Rn,#i]!
int Interpreter::strdPtrm(uint32_t opcode) { return strdPt(opcode, -rp(opcode));  } // STRD Rd,[Rn],-Rm
int Interpreter::strdPtim(uint32_t opcode) { return strdPt(opcode, -ipH(opcode)); } // STRD Rd,[Rn],-#i
int Interpreter::strdPtrp(uint32_t opcode) { return strdPt(opcode,  rp(opcode));  } // STRD Rd,[Rn],Rm
int Interpreter::strdPtip(uint32_t opcode) { return strdPt(opcode,  ipH(opcode)); } // STRD Rd,[Rn],#i

int Interpreter::swpb(uint32_t opcode) // SWPB Rd,Rm,[Rn]
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

int Interpreter::swp(uint32_t opcode) // SWP Rd,Rm,[Rn]
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

int Interpreter::ldmda(uint32_t opcode) // LDMDA Rn, <Rlist>
{
    // Decode the operand, decrementing the address beforehand so the transfer can be incrementing
    int n = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16] - n * 4;

    // Block load, post-decrement
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            op0 += 4;
            *registers[i] = core->memory.read<uint32_t>(cpu, op0);
        }
    }

    // Handle pipelining and THUMB switching
    if (!(opcode & BIT(15))) return n + ((cpu == 0) ? ((n > 1) ? 0 : 1) : 2);
    if (cpu == 0 && (*registers[15] & BIT(0))) cpsr |= BIT(5);
    flushPipeline();
    return n + 4;
}

int Interpreter::stmda(uint32_t opcode) // STMDA Rn, <Rlist>
{
    // Decode the operand, decrementing the address beforehand so the transfer can be incrementing
    int n = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16] - n * 4;

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

int Interpreter::ldmia(uint32_t opcode) // LDMIA Rn, <Rlist>
{
    // Decode the operand
    int n = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16];

    // Block load, post-increment
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            *registers[i] = core->memory.read<uint32_t>(cpu, op0);
            op0 += 4;
        }
    }

    // Handle pipelining and THUMB switching
    if (!(opcode & BIT(15))) return n + ((cpu == 0) ? ((n > 1) ? 0 : 1) : 2);
    if (cpu == 0 && (*registers[15] & BIT(0))) cpsr |= BIT(5);
    flushPipeline();
    return n + 4;
}

int Interpreter::stmia(uint32_t opcode) // STMIA Rn, <Rlist>
{
    // Decode the operand
    int n = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16];

    // Block store, post-increment
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

int Interpreter::ldmdb(uint32_t opcode) // LDMDB Rn, <Rlist>
{
    // Decode the operand, decrementing the address beforehand so the transfer can be incrementing
    int n = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16] - n * 4;

    // Block load, pre-decrement
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            *registers[i] = core->memory.read<uint32_t>(cpu, op0);
            op0 += 4;
        }
    }

    // Handle pipelining and THUMB switching
    if (!(opcode & BIT(15))) return n + ((cpu == 0) ? ((n > 1) ? 0 : 1) : 2);
    if (cpu == 0 && (*registers[15] & BIT(0))) cpsr |= BIT(5);
    flushPipeline();
    return n + 4;
}

int Interpreter::stmdb(uint32_t opcode) // STMDB Rn, <Rlist>
{
    // Decode the operand, decrementing the address beforehand so the transfer can be incrementing
    int n = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16] - n * 4;

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

int Interpreter::ldmib(uint32_t opcode) // LDMIB Rn, <Rlist>
{
    // Decode the operand
    int n = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16];

    // Block load, pre-increment
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            op0 += 4;
            *registers[i] = core->memory.read<uint32_t>(cpu, op0);
        }
    }

    // Handle pipelining and THUMB switching
    if (!(opcode & BIT(15))) return n + ((cpu == 0) ? ((n > 1) ? 0 : 1) : 2);
    if (cpu == 0 && (*registers[15] & BIT(0))) cpsr |= BIT(5);
    flushPipeline();
    return n + 4;
}

int Interpreter::stmib(uint32_t opcode) // STMIB Rn, <Rlist>
{
    // Decode the operand
    int n = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16];

    // Block store, pre-increment
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

int Interpreter::ldmdaW(uint32_t opcode) // LDMDA Rn!, <Rlist>
{
    // Decode the operand, decrementing the address beforehand so the transfer can be incrementing
    int m = (opcode & 0x000F0000) >> 16;
    int n = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[m] - n * 4;

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

    // Handle pipelining and THUMB switching
    if (!(opcode & BIT(15))) return n + ((cpu == 0) ? ((n > 1) ? 0 : 1) : 2);
    if (cpu == 0 && (*registers[15] & BIT(0))) cpsr |= BIT(5);
    flushPipeline();
    return n + 4;
}

int Interpreter::stmdaW(uint32_t opcode) // STMDA Rn!, <Rlist>
{
    // Decode the operand, decrementing the address beforehand so the transfer can be incrementing
    int m = (opcode & 0x000F0000) >> 16;
    int n = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[m] - n * 4;

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

int Interpreter::ldmiaW(uint32_t opcode) // LDMIA Rn!, <Rlist>
{
    // Decode the operand
    int m = (opcode & 0x000F0000) >> 16;
    int n = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[m];

    // Block load, post-increment
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
        *registers[m] = op0;

    // Handle pipelining and THUMB switching
    if (!(opcode & BIT(15))) return n + ((cpu == 0) ? ((n > 1) ? 0 : 1) : 2);
    if (cpu == 0 && (*registers[15] & BIT(0))) cpsr |= BIT(5);
    flushPipeline();
    return n + 4;
}

int Interpreter::stmiaW(uint32_t opcode) // STMIA Rn!, <Rlist>
{
    // Decode the operand
    int m = (opcode & 0x000F0000) >> 16;
    int n = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[m];

    // On ARM9, if Rn is in Rlist, the writeback value is never stored
    // On ARM7, if Rn is in Rlist, the writeback value is stored if Rn is not the first register
    if (cpu == 1 && (opcode & BIT(m)) && (opcode & (BIT(m) - 1)))
        *registers[m] = op0 + n * 4;

    // Block store, post-increment
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            core->memory.write<uint32_t>(cpu, op0, *registers[i]);
            op0 += 4;
        }
    }

    // Writeback
    *registers[m] = op0;

    return n + ((cpu == 0 && n > 1) ? 0 : 1);
}

int Interpreter::ldmdbW(uint32_t opcode) // LDMDB Rn!, <Rlist>
{
    // Decode the operand, decrementing the address beforehand so the transfer can be incrementing
    int m = (opcode & 0x000F0000) >> 16;
    int n = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[m] - n * 4;

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

    // Handle pipelining and THUMB switching
    if (!(opcode & BIT(15))) return n + ((cpu == 0) ? ((n > 1) ? 0 : 1) : 2);
    if (cpu == 0 && (*registers[15] & BIT(0))) cpsr |= BIT(5);
    flushPipeline();
    return n + 4;
}

int Interpreter::stmdbW(uint32_t opcode) // STMDB Rn!, <Rlist>
{
    // Decode the operand, decrementing the address beforehand so the transfer can be incrementing
    int m = (opcode & 0x000F0000) >> 16;
    int n = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[m] - n * 4;

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

int Interpreter::ldmibW(uint32_t opcode) // LDMIB Rn!, <Rlist>
{
    // Decode the operand
    int m = (opcode & 0x000F0000) >> 16;
    int n = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[m];

    // Block load, pre-increment
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
        *registers[m] = op0;

    // Handle pipelining and THUMB switching
    if (!(opcode & BIT(15))) return n + ((cpu == 0) ? ((n > 1) ? 0 : 1) : 2);
    if (cpu == 0 && (*registers[15] & BIT(0))) cpsr |= BIT(5);
    flushPipeline();
    return n + 4;
}

int Interpreter::stmibW(uint32_t opcode) // STMIB Rn!, <Rlist>
{
    // Decode the operand
    int m = (opcode & 0x000F0000) >> 16;
    int n = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[m];

    // On ARM9, if Rn is in Rlist, the writeback value is never stored
    // On ARM7, if Rn is in Rlist, the writeback value is stored if Rn is not the first register
    if (cpu == 1 && (opcode & BIT(m)) && (opcode & (BIT(m) - 1)))
        *registers[m] = op0 + n * 4;

    // Block store, pre-increment
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            op0 += 4;
            core->memory.write<uint32_t>(cpu, op0, *registers[i]);
        }
    }

    // Writeback
    *registers[m] = op0;

    return n + ((cpu == 0 && n > 1) ? 0 : 1);
}

int Interpreter::ldmdaU(uint32_t opcode) // LDMDA Rn, <Rlist>^
{
    // Decode the operand, decrementing the address beforehand so the transfer can be incrementing
    int n = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16] - n * 4;

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
    if (cpu == 0 && (*registers[15] & BIT(0))) cpsr |= BIT(5);
    flushPipeline();
    return n + 4;
}

int Interpreter::stmdaU(uint32_t opcode) // STMDA Rn, <Rlist>^
{
    // Decode the operand, decrementing the address beforehand so the transfer can be incrementing
    int n = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16] - n * 4;

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

int Interpreter::ldmiaU(uint32_t opcode) // LDMIA Rn, <Rlist>^
{
    // Decode the operand
    int n = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16];

    if (!(opcode & BIT(15))) // PC not in Rlist
    {
        // Block load, post-increment (user registers)
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

    // Block load, post-increment
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
    if (cpu == 0 && (*registers[15] & BIT(0))) cpsr |= BIT(5);
    flushPipeline();
    return n + 4;
}

int Interpreter::stmiaU(uint32_t opcode) // STMIA Rn, <Rlist>^
{
    // Decode the operand
    int n = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16];

    // Block store, post-increment (user registers)
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

int Interpreter::ldmdbU(uint32_t opcode) // LDMDB Rn, <Rlist>^
{
    // Decode the operand, decrementing the address beforehand so the transfer can be incrementing
    int n = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16] - n * 4;

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
    if (cpu == 0 && (*registers[15] & BIT(0))) cpsr |= BIT(5);
    flushPipeline();
    return n + 4;
}

int Interpreter::stmdbU(uint32_t opcode) // STMDB Rn, <Rlist>^
{
    // Decode the operand, decrementing the address beforehand so the transfer can be incrementing
    int n = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16] - n * 4;

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

int Interpreter::ldmibU(uint32_t opcode) // LDMIB Rn, <Rlist>^
{
    // Decode the operand
    int n = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16];

    if (!(opcode & BIT(15))) // PC not in Rlist
    {
        // Block load, pre-increment (user registers)
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

    // Block load, pre-increment
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
    if (cpu == 0 && (*registers[15] & BIT(0))) cpsr |= BIT(5);
    flushPipeline();
    return n + 4;
}

int Interpreter::stmibU(uint32_t opcode) // STMIB Rn, <Rlist>^
{
    // Decode the operand
    int n = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16];

    // Block store, pre-increment (user registers)
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

int Interpreter::ldmdaUW(uint32_t opcode) // LDMDA Rn!, <Rlist>^
{
    // Decode the operand, decrementing the address beforehand so the transfer can be incrementing
    int m = (opcode & 0x000F0000) >> 16;
    int n = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[m] - n * 4;

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
    if (cpu == 0 && (*registers[15] & BIT(0))) cpsr |= BIT(5);
    flushPipeline();
    return n + 4;
}

int Interpreter::stmdaUW(uint32_t opcode) // STMDA Rn!, <Rlist>^
{
    // Decode the operand, decrementing the address beforehand so the transfer can be incrementing
    int m = (opcode & 0x000F0000) >> 16;
    int n = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[m] - n * 4;

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

int Interpreter::ldmiaUW(uint32_t opcode) // LDMIA Rn!, <Rlist>^
{
    // Decode the operand
    int m = (opcode & 0x000F0000) >> 16;
    int n = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[m];

    if (!(opcode & BIT(15))) // PC not in Rlist
    {
        // Block load, post-increment (user registers)
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
    if (cpu == 0 && (*registers[15] & BIT(0))) cpsr |= BIT(5);
    flushPipeline();
    return n + 4;
}

int Interpreter::stmiaUW(uint32_t opcode) // STMIA Rn!, <Rlist>^
{
    // Decode the operand
    int m = (opcode & 0x000F0000) >> 16;
    int n = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[m];

    // On ARM9, if Rn is in Rlist, the writeback value is never stored
    // On ARM7, if Rn is in Rlist, the writeback value is stored if Rn is not the first register
    if (cpu == 1 && (opcode & BIT(m)) && (opcode & (BIT(m) - 1)))
        *registers[m] = op0 + n * 4;

    // Block store, post-increment (user registers)
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            core->memory.write<uint32_t>(cpu, op0, registersUsr[i]);
            op0 += 4;
        }
    }

    // Writeback
    *registers[m] = op0;

    return n + ((cpu == 0 && n > 1) ? 0 : 1);
}

int Interpreter::ldmdbUW(uint32_t opcode) // LDMDB Rn!, <Rlist>^
{
    // Decode the operand, decrementing the address beforehand so the transfer can be incrementing
    int m = (opcode & 0x000F0000) >> 16;
    int n = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[m] - n * 4;

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
    if (cpu == 0 && (*registers[15] & BIT(0))) cpsr |= BIT(5);
    flushPipeline();
    return n + 4;
}

int Interpreter::stmdbUW(uint32_t opcode) // STMDB Rn!, <Rlist>^
{
    // Decode the operand, decrementing the address beforehand so the transfer can be incrementing
    int m = (opcode & 0x000F0000) >> 16;
    int n = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[m] - n * 4;

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

int Interpreter::ldmibUW(uint32_t opcode) // LDMIB Rn!, <Rlist>^
{
    // Decode the operand
    int m = (opcode & 0x000F0000) >> 16;
    int n = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[m];

    if (!(opcode & BIT(15))) // PC not in Rlist
    {
        // Block load, pre-increment (user registers)
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
    if (cpu == 0 && (*registers[15] & BIT(0))) cpsr |= BIT(5);
    flushPipeline();
    return n + 4;
}

int Interpreter::stmibUW(uint32_t opcode) // STMIB Rn!, <Rlist>^
{
    // Decode the operand
    int m = (opcode & 0x000F0000) >> 16;
    int n = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[m];

    // On ARM9, if Rn is in Rlist, the writeback value is never stored
    // On ARM7, if Rn is in Rlist, the writeback value is stored if Rn is not the first register
    if (cpu == 1 && (opcode & BIT(m)) && (opcode & (BIT(m) - 1)))
        *registers[m] = op0 + n * 4;

    // Block store, pre-increment (user registers)
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            op0 += 4;
            core->memory.write<uint32_t>(cpu, op0, registersUsr[i]);
        }
    }

    // Writeback
    *registers[m] = op0;

    return n + ((cpu == 0 && n > 1) ? 0 : 1);
}

int Interpreter::msrRc(uint32_t opcode) // MSR CPSR,Rm
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

int Interpreter::msrRs(uint32_t opcode) // MSR SPSR,Rm
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

int Interpreter::msrIc(uint32_t opcode) // MSR CPSR,#i
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

int Interpreter::msrIs(uint32_t opcode) // MSR SPSR,#i
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

int Interpreter::mrsRc(uint32_t opcode) // MRS Rd,CPSR
{
    // Decode the operand
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];

    // Copy the status flags to a register
    *op0 = cpsr;

    return (cpu == 0 ? 2 : 1);
}

int Interpreter::mrsRs(uint32_t opcode) // MRS Rd,SPSR
{
    // Decode the operand
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];

    // Copy the saved status flags to a register
    if (spsr) *op0 = *spsr;

    return (cpu == 0 ? 2 : 1);
}

int Interpreter::mrc(uint32_t opcode) // MRC Pn,<cpopc>,Rd,Cn,Cm,<cp>
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

int Interpreter::mcr(uint32_t opcode) // MCR Pn,<cpopc>,Rd,Cn,Cm,<cp>
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

int Interpreter::ldrsbRegT(uint16_t opcode) // LDRSB Rd,[Rb,Ro]
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = *registers[(opcode & 0x01C0) >> 6];

    // Signed byte load, pre-adjust without writeback
    *op0 = (int8_t)core->memory.read<uint8_t>(cpu, op1 + op2);

    return ((cpu == 0) ? 1 : 3);
}

int Interpreter::ldrshRegT(uint16_t opcode) // LDRSH Rd,[Rb,Ro]
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = *registers[(opcode & 0x01C0) >> 6];

    // Signed half-word load, pre-adjust without writeback
    *op0 = (int16_t)core->memory.read<uint16_t>(cpu, op1 += op2);

    // Shift misaligned reads on ARM7
    if (cpu == 1 && (op1 & 1))
        *op0 = (int16_t)*op0 >> 8;

    return ((cpu == 0) ? 1 : 3);
}

int Interpreter::ldrbRegT(uint16_t opcode) // LDRB Rd,[Rb,Ro]
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = *registers[(opcode & 0x01C0) >> 6];

    // Byte load, pre-adjust without writeback
    *op0 = core->memory.read<uint8_t>(cpu, op1 + op2);

    return ((cpu == 0) ? 1 : 3);
}

int Interpreter::strbRegT(uint16_t opcode) // STRB Rd,[Rb,Ro]
{
    // Decode the operands
    uint32_t op0 = *registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = *registers[(opcode & 0x01C0) >> 6];

    // Byte write, pre-adjust without writeback
    core->memory.write<uint8_t>(cpu, op1 + op2, op0);

    return ((cpu == 0) ? 1 : 2);
}

int Interpreter::ldrhRegT(uint16_t opcode) // LDRH Rd,[Rb,Ro]
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

int Interpreter::strhRegT(uint16_t opcode) // STRH Rd,[Rb,Ro]
{
    // Decode the operands
    uint32_t op0 = *registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = *registers[(opcode & 0x01C0) >> 6];

    // Half-word write, pre-adjust without writeback
    core->memory.write<uint16_t>(cpu, op1 + op2, op0);

    return ((cpu == 0) ? 1 : 2);
}

int Interpreter::ldrRegT(uint16_t opcode) // LDR Rd,[Rb,Ro]
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

int Interpreter::strRegT(uint16_t opcode) // STR Rd,[Rb,Ro]
{
    // Decode the operands
    uint32_t op0 = *registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = *registers[(opcode & 0x01C0) >> 6];

    // Word write, pre-adjust without writeback
    core->memory.write<uint32_t>(cpu, op1 + op2, op0);

    return ((cpu == 0) ? 1 : 2);
}

int Interpreter::ldrbImm5T(uint16_t opcode) // LDRB Rd,[Rb,#i]
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = (opcode & 0x07C0) >> 6;

    // Byte load, pre-adjust without writeback
    *op0 = core->memory.read<uint8_t>(cpu, op1 + op2);

    return ((cpu == 0) ? 1 : 3);
}

int Interpreter::strbImm5T(uint16_t opcode) // STRB Rd,[Rb,#i]
{
    // Decode the operands
    uint32_t op0 = *registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = (opcode & 0x07C0) >> 6;

    // Byte store, pre-adjust without writeback
    core->memory.write<uint8_t>(cpu, op1 + op2, op0);

    return ((cpu == 0) ? 1 : 2);
}

int Interpreter::ldrhImm5T(uint16_t opcode) // LDRH Rd,[Rb,#i]
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

int Interpreter::strhImm5T(uint16_t opcode) // STRH Rd,[Rb,#i]
{
    // Decode the operands
    uint32_t op0 = *registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = (opcode & 0x07C0) >> 5;

    // Half-word store, pre-adjust without writeback
    core->memory.write<uint16_t>(cpu, op1 + op2, op0);

    return ((cpu == 0) ? 1 : 2);
}

int Interpreter::ldrImm5T(uint16_t opcode) // LDR Rd,[Rb,#i]
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

int Interpreter::strImm5T(uint16_t opcode) // STR Rd,[Rb,#i]
{
    // Decode the operands
    uint32_t op0 = *registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = (opcode & 0x07C0) >> 4;

    // Word store, pre-adjust without writeback
    core->memory.write<uint32_t>(cpu, op1 + op2, op0);

    return ((cpu == 0) ? 1 : 2);
}

int Interpreter::ldrPcT(uint16_t opcode) // LDR Rd,[PC,#i]
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

int Interpreter::ldrSpT(uint16_t opcode) // LDR Rd,[SP,#i]
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

int Interpreter::strSpT(uint16_t opcode) // STR Rd,[SP,#i]
{
    // Decode the operands
    uint32_t op0 = *registers[(opcode & 0x0700) >> 8];
    uint32_t op1 = *registers[13];
    uint32_t op2 = (opcode & 0x00FF) << 2;

    // Word store, pre-adjust without writeback
    core->memory.write<uint32_t>(cpu, op1 + op2, op0);

    return ((cpu == 0) ? 1 : 2);
}

int Interpreter::ldmiaT(uint16_t opcode) // LDMIA Rb!,<Rlist>
{
    // Decode the operand
    int m = (opcode & 0x0700) >> 8;
    int n = bitCount[opcode & 0xFF];
    uint32_t op0 = *registers[m];

    // Block load, post-increment
    for (int i = 0; i < 8; i++)
    {
        if (opcode & BIT(i))
        {
            *registers[i] = core->memory.read<uint32_t>(cpu, op0);
            op0 += 4;
        }
    }

    // Writeback
    // On ARM7 and ARM9 in THUMB mode, if Rn is in Rlist, writeback never happens
    if (!(opcode & BIT(m)))
        *registers[m] = op0;

    return n + ((cpu == 0) ? ((n > 1) ? 0 : 1) : 2);
}

int Interpreter::stmiaT(uint16_t opcode) // STMIA Rb!,<Rlist>
{
    // Decode the operand
    int m = (opcode & 0x0700) >> 8;
    int n = bitCount[opcode & 0xFF];
    uint32_t op0 = *registers[m];

    // On ARM9, if Rn is in Rlist, the writeback value is never stored
    // On ARM7, if Rn is in Rlist, the writeback value is stored if Rn is not the first register
    if (cpu == 1 && (opcode & BIT(m)) && (opcode & (BIT(m) - 1)))
        *registers[m] = op0 + n * 4;

    // Block store, post-increment
    for (int i = 0; i < 8; i++)
    {
        if (opcode & BIT(i))
        {
            core->memory.write<uint32_t>(cpu, op0, *registers[i]);
            op0 += 4;
        }
    }

    // Writeback
    *registers[m] = op0;

    return n + ((cpu == 0 && n > 1) ? 0 : 1);
}

int Interpreter::popT(uint16_t opcode) // POP <Rlist>
{
    // Decode the operand
    int n = bitCount[opcode & 0xFF];
    uint32_t op0 = *registers[13];

    // Block load, post-increment
    for (int i = 0; i < 8; i++)
    {
        if (opcode & BIT(i))
        {
            *registers[i] = core->memory.read<uint32_t>(cpu, op0);
            op0 += 4;
        }
    }

    // Writeback
    *registers[13] = op0;

    return n + ((cpu == 0) ? ((n > 1) ? 0 : 1) : 2);
}

int Interpreter::pushT(uint16_t opcode) // PUSH <Rlist>
{
    // Decode the operand, decrementing the address beforehand so the transfer can be incrementing
    int n = bitCount[opcode & 0xFF];
    uint32_t op0 = *registers[13] - n * 4;

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

int Interpreter::popPcT(uint16_t opcode) // POP <Rlist>,PC
{
    // Decode the operand
    int n = bitCount[opcode & 0xFF] + 1;
    uint32_t op0 = *registers[13];

    // Block load, post-increment
    for (int i = 0; i < 8; i++)
    {
        if (opcode & BIT(i))
        {
            *registers[i] = core->memory.read<uint32_t>(cpu, op0);
            op0 += 4;
        }
    }
    *registers[15] = core->memory.read<uint32_t>(cpu, op0);

    // Writeback
    *registers[13] = op0 + 4;

    // Handle pipelining
    if (cpu == 0 && !(*registers[15] & BIT(0))) cpsr &= ~BIT(5);
    flushPipeline();
    return n + 4;
}

int Interpreter::pushLrT(uint16_t opcode) // PUSH <Rlist>,LR
{
    // Decode the operand, decrementing the address beforehand so the transfer can be incrementing
    int n = bitCount[opcode & 0xFF] + 1;
    uint32_t op0 = *registers[13] - n * 4;

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
