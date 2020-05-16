/*
    Copyright 2019-2020 Hydr8gon

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

#include "interpreter.h"

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

FORCE_INLINE void Interpreter::ldrsbOf(uint32_t opcode, uint32_t op2) // LDRSB Rd,[Rn,op2]
{
    // Decode the other operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16];

    // Signed byte load, pre-adjust without writeback
    *op0 = memory->read<int8_t>(cp15, op1 + op2);

    // Handle pipelining
    if (op0 == registers[15])
        *registers[15] = (*registers[15] & ~3) + 4;
}

FORCE_INLINE void Interpreter::ldrshOf(uint32_t opcode, uint32_t op2) // LDRSH Rd,[Rn,op2]
{
    // Decode the other operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16];

    // Signed half-word load, pre-adjust without writeback
    *op0 = memory->read<int16_t>(cp15, op1 + op2);

    // Handle pipelining
    if (op0 == registers[15])
        *registers[15] = (*registers[15] & ~3) + 4;
}

FORCE_INLINE void Interpreter::ldrbOf(uint32_t opcode, uint32_t op2) // LDRB Rd,[Rn,op2]
{
    // Decode the other operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16];

    // Byte load, pre-adjust without writeback
    *op0 = memory->read<uint8_t>(cp15, op1 + op2);

    // Handle pipelining and THUMB switching
    if (op0 == registers[15])
    {
        if (cp15 && (*op0 & BIT(0)))
        {
            cpsr |= BIT(5);
            *registers[15] &= ~1;
        }
        else
        {
            *registers[15] = (*registers[15] & ~3) + 4;
        }
    }
}

FORCE_INLINE void Interpreter::strbOf(uint32_t opcode, uint32_t op2) // STRB Rd,[Rn,op2]
{
    // Decode the other operands
    // When used as Rd, the program counter is read with +4
    uint32_t op0 = *registers[(opcode & 0x0000F000) >> 12] + (((opcode & 0x0000F000) == 0x0000F000) ? 4 : 0);
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16];

    // Byte store, pre-adjust without writeback
    memory->write<uint8_t>(cp15, op1 + op2, op0);
}

FORCE_INLINE void Interpreter::ldrhOf(uint32_t opcode, uint32_t op2) // LDRH Rd,[Rn,op2]
{
    // Decode the other operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16];

    // Half-word load, pre-adjust without writeback
    *op0 = memory->read<uint16_t>(cp15, op1 + op2);

    // Handle pipelining
    if (op0 == registers[15])
        *registers[15] = (*registers[15] & ~3) + 4;
}

FORCE_INLINE void Interpreter::strhOf(uint32_t opcode, uint32_t op2) // STRH Rd,[Rn,op2]
{
    // Decode the other operands
    // When used as Rd, the program counter is read with +4
    uint32_t op0 = *registers[(opcode & 0x0000F000) >> 12] + (((opcode & 0x0000F000) == 0x0000F000) ? 4 : 0);
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16];

    // Half-word store, pre-adjust without writeback
    memory->write<uint16_t>(cp15, op1 + op2, op0);
}

FORCE_INLINE void Interpreter::ldrOf(uint32_t opcode, uint32_t op2) // LDR Rd,[Rn,op2]
{
    // Decode the other operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16];

    // Word load, pre-adjust without writeback
    *op0 = memory->read<uint32_t>(cp15, op1 += op2);

    // Rotate misaligned reads
    if (op1 & 3)
    {
        int shift = (op1 & 3) * 8;
        *op0 = (*op0 << (32 - shift)) | (*op0 >> shift);
    }

    // Handle pipelining and THUMB switching
    if (op0 == registers[15])
    {
        if (cp15 && (*op0 & BIT(0)))
        {
            cpsr |= BIT(5);
            *registers[15] &= ~1;
        }
        else
        {
            *registers[15] = (*registers[15] & ~3) + 4;
        }
    }
}

FORCE_INLINE void Interpreter::strOf(uint32_t opcode, uint32_t op2) // STR Rd,[Rn,op2]
{
    // Decode the other operands
    // When used as Rd, the program counter is read with +4
    uint32_t op0 = *registers[(opcode & 0x0000F000) >> 12] + (((opcode & 0x0000F000) == 0x0000F000) ? 4 : 0);
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16];

    // Word store, pre-adjust without writeback
    memory->write<uint32_t>(cp15, op1 + op2, op0);
}

FORCE_INLINE void Interpreter::ldrdOf(uint32_t opcode, uint32_t op2) // LDRD Rd,[Rn,op2]
{
    if (!cp15) return; // ARM9 exclusive

    // Decode the other operands
    uint8_t op0 = (opcode & 0x0000F000) >> 12;
    if (op0 == 15) return;
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16];

    // Double word load, pre-adjust without writeback
    *registers[op0]     = memory->read<uint32_t>(cp15, op1 + op2);
    *registers[op0 + 1] = memory->read<uint32_t>(cp15, op1 + op2 + 4);
}

FORCE_INLINE void Interpreter::strdOf(uint32_t opcode, uint32_t op2) // STRD Rd,[Rn,op2]
{
    if (!cp15) return; // ARM9 exclusive

    // Decode the other operands
    uint8_t op0 = (opcode & 0x0000F000) >> 12;
    if (op0 == 15) return;
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16];

    // Double word store, pre-adjust without writeback
    memory->write<uint32_t>(cp15, op1 + op2,     *registers[op0]);
    memory->write<uint32_t>(cp15, op1 + op2 + 4, *registers[op0 + 1]);
}

FORCE_INLINE void Interpreter::ldrsbPr(uint32_t opcode, uint32_t op2) // LDRSB Rd,[Rn,op2]!
{
    // Decode the other operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];

    // Signed byte load, pre-adjust with writeback
    *op1 += op2;
    *op0 = memory->read<int8_t>(cp15, *op1);

    // Handle pipelining
    if (op0 == registers[15])
        *registers[15] = (*registers[15] & ~3) + 4;
}

FORCE_INLINE void Interpreter::ldrshPr(uint32_t opcode, uint32_t op2) // LDRSH Rd,[Rn,op2]!
{
    // Decode the other operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];

    // Signed half-word load, pre-adjust with writeback
    *op1 += op2;
    *op0 = memory->read<int16_t>(cp15, *op1);

    // Handle pipelining
    if (op0 == registers[15])
        *registers[15] = (*registers[15] & ~3) + 4;
}

FORCE_INLINE void Interpreter::ldrbPr(uint32_t opcode, uint32_t op2) // LDRB Rd,[Rn,op2]!
{
    // Decode the other operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];

    // Byte load, pre-adjust with writeback
    *op1 += op2;
    *op0 = memory->read<uint8_t>(cp15, *op1);

    // Handle pipelining and THUMB switching
    if (op0 == registers[15])
    {
        if (cp15 && (*op0 & BIT(0)))
        {
            cpsr |= BIT(5);
            *registers[15] &= ~1;
        }
        else
        {
            *registers[15] = (*registers[15] & ~3) + 4;
        }
    }
}

FORCE_INLINE void Interpreter::strbPr(uint32_t opcode, uint32_t op2) // STRB Rd,[Rn,op2]!
{
    // Decode the other operands
    // When used as Rd, the program counter is read with +4
    uint32_t op0 = *registers[(opcode & 0x0000F000) >> 12] + (((opcode & 0x0000F000) == 0x0000F000) ? 4 : 0);
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];

    // Byte store, pre-adjust with writeback
    *op1 += op2;
    memory->write<uint8_t>(cp15, *op1, op0);
}

FORCE_INLINE void Interpreter::ldrhPr(uint32_t opcode, uint32_t op2) // LDRH Rd,[Rn,op2]!
{
    // Decode the other operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];

    // Half-word load, pre-adjust with writeback
    *op1 += op2;
    *op0 = memory->read<uint16_t>(cp15, *op1);

    // Handle pipelining
    if (op0 == registers[15])
        *registers[15] = (*registers[15] & ~3) + 4;
}

FORCE_INLINE void Interpreter::strhPr(uint32_t opcode, uint32_t op2) // STRH Rd,[Rn,op2]!
{
    // Decode the other operands
    // When used as Rd, the program counter is read with +4
    uint32_t op0 = *registers[(opcode & 0x0000F000) >> 12] + (((opcode & 0x0000F000) == 0x0000F000) ? 4 : 0);
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];

    // Half-word store, pre-adjust with writeback
    *op1 += op2;
    memory->write<uint16_t>(cp15, *op1, op0);
}

FORCE_INLINE void Interpreter::ldrPr(uint32_t opcode, uint32_t op2) // LDR Rd,[Rn,op2]!
{
    // Decode the other operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t address;

    // Word load, pre-adjust with writeback
    *op1 += op2;
    *op0 = memory->read<uint32_t>(cp15, address = *op1);

    // Rotate misaligned reads
    if (address & 3)
    {
        int shift = (address & 3) * 8;
        *op0 = (*op0 << (32 - shift)) | (*op0 >> shift);
    }

    // Handle pipelining and THUMB switching
    if (op0 == registers[15])
    {
        if (cp15 && (*op0 & BIT(0)))
        {
            cpsr |= BIT(5);
            *registers[15] &= ~1;
        }
        else
        {
            *registers[15] = (*registers[15] & ~3) + 4;
        }
    }
}

FORCE_INLINE void Interpreter::strPr(uint32_t opcode, uint32_t op2) // STR Rd,[Rn,op2]!
{
    // Decode the other operands
    // When used as Rd, the program counter is read with +4
    uint32_t op0 = *registers[(opcode & 0x0000F000) >> 12] + (((opcode & 0x0000F000) == 0x0000F000) ? 4 : 0);
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];

    // Word store, pre-adjust with writeback
    *op1 += op2;
    memory->write<uint32_t>(cp15, *op1, op0);
}

FORCE_INLINE void Interpreter::ldrdPr(uint32_t opcode, uint32_t op2) // LDRD Rd,[Rn,op2]!
{
    if (!cp15) return; // ARM9 exclusive

    // Decode the other operands
    uint8_t op0 = (opcode & 0x0000F000) >> 12;
    if (op0 == 15) return;
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];

    // Double word load, pre-adjust with writeback
    *op1 += op2;
    *registers[op0]     = memory->read<uint32_t>(cp15, *op1);
    *registers[op0 + 1] = memory->read<uint32_t>(cp15, *op1 + 4);
}

FORCE_INLINE void Interpreter::strdPr(uint32_t opcode, uint32_t op2) // STRD Rd,[Rn,op2]!
{
    if (!cp15) return; // ARM9 exclusive

    // Decode the other operands
    uint8_t op0 = (opcode & 0x0000F000) >> 12;
    if (op0 == 15) return;
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];

    // Double word store, pre-adjust with writeback
    *op1 += op2;
    memory->write<uint32_t>(cp15, *op1,     *registers[op0]);
    memory->write<uint32_t>(cp15, *op1 + 4, *registers[op0 + 1]);
}

FORCE_INLINE void Interpreter::ldrsbPt(uint32_t opcode, uint32_t op2) // LDRSB Rd,[Rn],op2
{
    // Decode the other operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];

    // Signed byte load, post-adjust
    *op0 = memory->read<int8_t>(cp15, *op1);
    if (op0 != op1) *op1 += op2;

    // Handle pipelining
    if (op0 == registers[15])
        *registers[15] = (*registers[15] & ~3) + 4;
}

FORCE_INLINE void Interpreter::ldrshPt(uint32_t opcode, uint32_t op2) // LDRSH Rd,[Rn],op2
{
    // Decode the other operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];

    // Signed half-word load, post-adjust
    *op0 = memory->read<int16_t>(cp15, *op1);
    if (op0 != op1) *op1 += op2;

    // Handle pipelining
    if (op0 == registers[15])
        *registers[15] = (*registers[15] & ~3) + 4;
}

FORCE_INLINE void Interpreter::ldrbPt(uint32_t opcode, uint32_t op2) // LDRB Rd,[Rn],op2
{
    // Decode the other operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];

    // Byte load, post-adjust
    *op0 = memory->read<uint8_t>(cp15, *op1);
    if (op0 != op1) *op1 += op2;

    // Handle pipelining and THUMB switching
    if (op0 == registers[15])
    {
        if (cp15 && (*op0 & BIT(0)))
        {
            cpsr |= BIT(5);
            *registers[15] &= ~1;
        }
        else
        {
            *registers[15] = (*registers[15] & ~3) + 4;
        }
    }
}

FORCE_INLINE void Interpreter::strbPt(uint32_t opcode, uint32_t op2) // STRB Rd,[Rn],op2
{
    // Decode the other operands
    // When used as Rd, the program counter is read with +4
    uint32_t op0 = *registers[(opcode & 0x0000F000) >> 12] + (((opcode & 0x0000F000) == 0x0000F000) ? 4 : 0);
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];

    // Byte store, post-adjust
    memory->write<uint8_t>(cp15, *op1, op0);
    *op1 += op2;
}

FORCE_INLINE void Interpreter::ldrhPt(uint32_t opcode, uint32_t op2) // LDRH Rd,[Rn],op2
{
    // Decode the other operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];

    // Half-word load, post-adjust
    *op0 = memory->read<uint16_t>(cp15, *op1);
    *op1 += op2;

    // Handle pipelining
    if (op0 == registers[15])
        *registers[15] = (*registers[15] & ~3) + 4;
}

FORCE_INLINE void Interpreter::strhPt(uint32_t opcode, uint32_t op2) // STRH Rd,[Rn],op2
{
    // Decode the other operands
    // When used as Rd, the program counter is read with +4
    uint32_t op0 = *registers[(opcode & 0x0000F000) >> 12] + (((opcode & 0x0000F000) == 0x0000F000) ? 4 : 0);
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];

    // Half-word store, post-adjust
    memory->write<uint16_t>(cp15, *op1, op0);
    *op1 += op2;
}

FORCE_INLINE void Interpreter::ldrPt(uint32_t opcode, uint32_t op2) // LDR Rd,[Rn],op2
{
    // Decode the other operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t address;

    // Word load
    *op0 = memory->read<uint32_t>(cp15, address = *op1);

    // Rotate misaligned reads
    if (address & 3)
    {
        int shift = (address & 3) * 8;
        *op0 = (*op0 << (32 - shift)) | (*op0 >> shift);
    }

    // Post-adjust
    if (op0 != op1) *op1 += op2;

    // Handle pipelining and THUMB switching
    if (op0 == registers[15])
    {
        if (cp15 && (*op0 & BIT(0)))
        {
            cpsr |= BIT(5);
            *registers[15] &= ~1;
        }
        else
        {
            *registers[15] = (*registers[15] & ~3) + 4;
        }
    }
}

FORCE_INLINE void Interpreter::strPt(uint32_t opcode, uint32_t op2) // STR Rd,[Rn],op2
{
    // Decode the other operands
    // When used as Rd, the program counter is read with +4
    uint32_t op0 = *registers[(opcode & 0x0000F000) >> 12] + (((opcode & 0x0000F000) == 0x0000F000) ? 4 : 0);
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];

    // Word store, post-adjust
    memory->write<uint32_t>(cp15, *op1, op0);
    *op1 += op2;
}

FORCE_INLINE void Interpreter::ldrdPt(uint32_t opcode, uint32_t op2) // LDRD Rd,[Rn],op2
{
    if (!cp15) return; // ARM9 exclusive

    // Decode the other operands
    uint8_t op0 = (opcode & 0x0000F000) >> 12;
    if (op0 == 15) return;
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];

    // Double word load, post-adjust
    *registers[op0]     = memory->read<uint32_t>(cp15, *op1);
    *registers[op0 + 1] = memory->read<uint32_t>(cp15, *op1 + 4);
    *op1 += op2;
}

FORCE_INLINE void Interpreter::strdPt(uint32_t opcode, uint32_t op2) // STRD Rd,[Rn],op2
{
    if (!cp15) return; // ARM9 exclusive

    // Decode the other operands
    uint8_t op0 = (opcode & 0x0000F000) >> 12;
    if (op0 == 15) return;
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];

    // Double word store, post-adjust
    memory->write<uint32_t>(cp15, *op1,     *registers[op0]);
    memory->write<uint32_t>(cp15, *op1 + 4, *registers[op0 + 1]);
    *op1 += op2;
}

FORCE_INLINE void Interpreter::swpb(uint32_t opcode) // SWPB Rd,Rm,[Rn]
{
    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[opcode & 0x0000000F];
    uint32_t op2 = *registers[(opcode & 0x000F0000) >> 16];

    // Swap
    *op0 = memory->read<uint8_t>(cp15, op2);
    memory->write<uint8_t>(cp15, op2, op1);
}

FORCE_INLINE void Interpreter::swp(uint32_t opcode) // SWP Rd,Rm,[Rn]
{
    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[opcode & 0x0000000F];
    uint32_t op2 = *registers[(opcode & 0x000F0000) >> 16];

    // Swap
    *op0 = memory->read<uint32_t>(cp15, op2);
    memory->write<uint32_t>(cp15, op2, op1);
}

FORCE_INLINE void Interpreter::ldmda(uint32_t opcode) // LDMDA Rn, <Rlist>
{
    // Decode the operand
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16];

    // Block load, post-decrement
    for (int i = 15; i >= 0; i--)
    {
        if (opcode & BIT(i))
        {
            *registers[i] = memory->read<uint32_t>(cp15, op0);
            op0 -= 4;
        }
    }

    // Handle pipelining and THUMB switching
    if (opcode & BIT(15))
    {
        if (cp15 && (*registers[15] & BIT(0)))
        {
            cpsr |= BIT(5);
            *registers[15] &= ~1;
        }
        else
        {
            *registers[15] = (*registers[15] & ~3) + 4;
        }
    }
}

FORCE_INLINE void Interpreter::stmda(uint32_t opcode) // STMDA Rn, <Rlist>
{
    // Decode the operand
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16];

    // Block store, post-decrement
    for (int i = 15; i >= 0; i--)
    {
        if (opcode & BIT(i))
        {
            memory->write<uint32_t>(cp15, op0, *registers[i]);
            op0 -= 4;
        }
    }
}

FORCE_INLINE void Interpreter::ldmia(uint32_t opcode) // LDMIA Rn, <Rlist>
{
    // Decode the operand
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16];

    // Block load, post-increment
    for (int i = 0; i <= 15; i++)
    {
        if (opcode & BIT(i))
        {
            *registers[i] = memory->read<uint32_t>(cp15, op0);
            op0 += 4;
        }
    }

    // Handle pipelining and THUMB switching
    if (opcode & BIT(15))
    {
        if (cp15 && (*registers[15] & BIT(0)))
        {
            cpsr |= BIT(5);
            *registers[15] &= ~1;
        }
        else
        {
            *registers[15] = (*registers[15] & ~3) + 4;
        }
    }
}

FORCE_INLINE void Interpreter::stmia(uint32_t opcode) // STMIA Rn, <Rlist>
{
    // Decode the operand
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16];

    // Block store, post-increment
    for (int i = 0; i <= 15; i++)
    {
        if (opcode & BIT(i))
        {
            memory->write<uint32_t>(cp15, op0, *registers[i]);
            op0 += 4;
        }
    }
}

FORCE_INLINE void Interpreter::ldmdb(uint32_t opcode) // LDMDB Rn, <Rlist>
{
    // Decode the operand
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16];

    // Block load, pre-decrement
    for (int i = 15; i >= 0; i--)
    {
        if (opcode & BIT(i))
        {
            op0 -= 4;
            *registers[i] = memory->read<uint32_t>(cp15, op0);
        }
    }

    // Handle pipelining and THUMB switching
    if (opcode & BIT(15))
    {
        if (cp15 && (*registers[15] & BIT(0)))
        {
            cpsr |= BIT(5);
            *registers[15] &= ~1;
        }
        else
        {
            *registers[15] = (*registers[15] & ~3) + 4;
        }
    }
}

FORCE_INLINE void Interpreter::stmdb(uint32_t opcode) // STMDB Rn, <Rlist>
{
    // Decode the operand
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16];

    // Block store, pre-decrement
    for (int i = 15; i >= 0; i--)
    {
        if (opcode & BIT(i))
        {
            op0 -= 4;
            memory->write<uint32_t>(cp15, op0, *registers[i]);
        }
    }
}

FORCE_INLINE void Interpreter::ldmib(uint32_t opcode) // LDMIB Rn, <Rlist>
{
    // Decode the operand
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16];

    // Block load, pre-increment
    for (int i = 0; i <= 15; i++)
    {
        if (opcode & BIT(i))
        {
            op0 += 4;
            *registers[i] = memory->read<uint32_t>(cp15, op0);
        }
    }

    // Handle pipelining and THUMB switching
    if (opcode & BIT(15))
    {
        if (cp15 && (*registers[15] & BIT(0)))
        {
            cpsr |= BIT(5);
            *registers[15] &= ~1;
        }
        else
        {
            *registers[15] = (*registers[15] & ~3) + 4;
        }
    }
}

FORCE_INLINE void Interpreter::stmib(uint32_t opcode) // STMIB Rn, <Rlist>
{
    // Decode the operand
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16];

    // Block store, pre-increment
    for (int i = 0; i <= 15; i++)
    {
        if (opcode & BIT(i))
        {
            op0 += 4;
            memory->write<uint32_t>(cp15, op0, *registers[i]);
        }
    }
}

FORCE_INLINE void Interpreter::ldmdaW(uint32_t opcode) // LDMDA Rn!, <Rlist>
{
    // Decode the operand
    uint32_t *op0 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t address = *op0;

    // Block load, post-decrement
    for (int i = 15; i >= 0; i--)
    {
        if (opcode & BIT(i))
        {
            *registers[i] = memory->read<uint32_t>(cp15, address);
            address -= 4;
        }
    }

    // Writeback
    if (!(opcode & BIT((opcode & 0x000F0000) >> 16)))
        *op0 = address;

    // Handle pipelining and THUMB switching
    if (opcode & BIT(15))
    {
        if (cp15 && (*registers[15] & BIT(0)))
        {
            cpsr |= BIT(5);
            *registers[15] &= ~1;
        }
        else
        {
            *registers[15] = (*registers[15] & ~3) + 4;
        }
    }
}

FORCE_INLINE void Interpreter::stmdaW(uint32_t opcode) // STMDA Rn!, <Rlist>
{
    // Decode the operand
    uint32_t *op0 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t address = *op0;

    // Block store, post-decrement
    for (int i = 15; i >= 0; i--)
    {
        if (opcode & BIT(i))
        {
            memory->write<uint32_t>(cp15, address, *registers[i]);
            address -= 4;
        }
    }

    // Writeback
    *op0 = address;
}

FORCE_INLINE void Interpreter::ldmiaW(uint32_t opcode) // LDMIA Rn!, <Rlist>
{
    // Decode the operand
    uint32_t *op0 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t address = *op0;

    // Block load, post-increment
    for (int i = 0; i <= 15; i++)
    {
        if (opcode & BIT(i))
        {
            *registers[i] = memory->read<uint32_t>(cp15, address);
            address += 4;
        }
    }

    // Writeback
    if (!(opcode & BIT((opcode & 0x000F0000) >> 16)))
        *op0 = address;

    // Handle pipelining and THUMB switching
    if (opcode & BIT(15))
    {
        if (cp15 && (*registers[15] & BIT(0)))
        {
            cpsr |= BIT(5);
            *registers[15] &= ~1;
        }
        else
        {
            *registers[15] = (*registers[15] & ~3) + 4;
        }
    }
}

FORCE_INLINE void Interpreter::stmiaW(uint32_t opcode) // STMIA Rn!, <Rlist>
{
    // Decode the operand
    uint32_t *op0 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t address = *op0;

    // Block store, post-increment
    for (int i = 0; i <= 15; i++)
    {
        if (opcode & BIT(i))
        {
            memory->write<uint32_t>(cp15, address, *registers[i]);
            address += 4;
        }
    }

    // Writeback
    *op0 = address;
}

FORCE_INLINE void Interpreter::ldmdbW(uint32_t opcode) // LDMDB Rn!, <Rlist>
{
    // Decode the operand
    uint32_t *op0 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t address = *op0;

    // Block load, pre-decrement
    for (int i = 15; i >= 0; i--)
    {
        if (opcode & BIT(i))
        {
            address -= 4;
            *registers[i] = memory->read<uint32_t>(cp15, address);
        }
    }

    // Writeback
    if (!(opcode & BIT((opcode & 0x000F0000) >> 16)))
        *op0 = address;

    // Handle pipelining and THUMB switching
    if (opcode & BIT(15))
    {
        if (cp15 && (*registers[15] & BIT(0)))
        {
            cpsr |= BIT(5);
            *registers[15] &= ~1;
        }
        else
        {
            *registers[15] = (*registers[15] & ~3) + 4;
        }
    }
}

FORCE_INLINE void Interpreter::stmdbW(uint32_t opcode) // STMDB Rn!, <Rlist>
{
    // Decode the operand
    uint32_t *op0 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t address = *op0;

    // Block store, pre-decrement
    for (int i = 15; i >= 0; i--)
    {
        if (opcode & BIT(i))
        {
            address -= 4;
            memory->write<uint32_t>(cp15, address, *registers[i]);
        }
    }

    // Writeback
    *op0 = address;
}

FORCE_INLINE void Interpreter::ldmibW(uint32_t opcode) // LDMIB Rn!, <Rlist>
{
    // Decode the operand
    uint32_t *op0 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t address = *op0;

    // Block load, pre-increment
    for (int i = 0; i <= 15; i++)
    {
        if (opcode & BIT(i))
        {
            address += 4;
            *registers[i] = memory->read<uint32_t>(cp15, address);
        }
    }

    // Writeback
    if (!(opcode & BIT((opcode & 0x000F0000) >> 16)))
        *op0 = address;

    // Handle pipelining and THUMB switching
    if (opcode & BIT(15))
    {
        if (cp15 && (*registers[15] & BIT(0)))
        {
            cpsr |= BIT(5);
            *registers[15] &= ~1;
        }
        else
        {
            *registers[15] = (*registers[15] & ~3) + 4;
        }
    }
}

FORCE_INLINE void Interpreter::stmibW(uint32_t opcode) // STMIB Rn!, <Rlist>
{
    // Decode the operand
    uint32_t *op0 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t address = *op0;

    // Block store, pre-increment
    for (int i = 0; i <= 15; i++)
    {
        if (opcode & BIT(i))
        {
            address += 4;
            memory->write<uint32_t>(cp15, address, *registers[i]);
        }
    }

    // Writeback
    *op0 = address;
}

FORCE_INLINE void Interpreter::ldmdaU(uint32_t opcode) // LDMDA Rn, <Rlist>^
{
    // Decode the operand
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16];

    // Block load, post-increment
    for (int i = 0; i <= 15; i++)
    {
        if (opcode & BIT(i))
        {
            registersUsr[i] = memory->read<uint32_t>(cp15, op0);
            op0 += 4;
        }
    }

    // Handle pipelining, THUMB switching, and CPU mode switching
    if (opcode & BIT(15))
    {
        if (spsr)
        {
            cpsr = *spsr;
            setMode(cpsr);
        }

        if (cp15 && (*registers[15] & BIT(0)))
        {
            cpsr |= BIT(5);
            *registers[15] &= ~1;
        }
        else
        {
            *registers[15] = (*registers[15] & ~3) + 4;
        }
    }
}

FORCE_INLINE void Interpreter::stmdaU(uint32_t opcode) // STMDA Rn, <Rlist>^
{
    // Decode the operand
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16];

    // Block store, post-decrement
    for (int i = 15; i >= 0; i--)
    {
        if (opcode & BIT(i))
        {
            memory->write<uint32_t>(cp15, op0, registersUsr[i]);
            op0 -= 4;
        }
    }
}

FORCE_INLINE void Interpreter::ldmiaU(uint32_t opcode) // LDMIA Rn, <Rlist>^
{
    // Decode the operand
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16];

    // Block load, post-increment
    for (int i = 0; i <= 15; i++)
    {
        if (opcode & BIT(i))
        {
            registersUsr[i] = memory->read<uint32_t>(cp15, op0);
            op0 += 4;
        }
    }

    // Handle pipelining, THUMB switching, and CPU mode switching
    if (opcode & BIT(15))
    {
        if (spsr)
        {
            cpsr = *spsr;
            setMode(cpsr);
        }

        if (cp15 && (*registers[15] & BIT(0)))
        {
            cpsr |= BIT(5);
            *registers[15] &= ~1;
        }
        else
        {
            *registers[15] = (*registers[15] & ~3) + 4;
        }
    }
}

FORCE_INLINE void Interpreter::stmiaU(uint32_t opcode) // STMIA Rn, <Rlist>^
{
    // Decode the operand
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16];

    // Block store, post-increment
    for (int i = 0; i <= 15; i++)
    {
        if (opcode & BIT(i))
        {
            memory->write<uint32_t>(cp15, op0, registersUsr[i]);
            op0 += 4;
        }
    }
}

FORCE_INLINE void Interpreter::ldmdbU(uint32_t opcode) // LDMDB Rn, <Rlist>^
{
    // Decode the operand
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16];

    // Block load, pre-decrement
    for (int i = 15; i >= 0; i--)
    {
        if (opcode & BIT(i))
        {
            op0 -= 4;
            registersUsr[i] = memory->read<uint32_t>(cp15, op0);
        }
    }

    // Handle pipelining, THUMB switching, and CPU mode switching
    if (opcode & BIT(15))
    {
        if (spsr)
        {
            cpsr = *spsr;
            setMode(cpsr);
        }

        if (cp15 && (*registers[15] & BIT(0)))
        {
            cpsr |= BIT(5);
            *registers[15] &= ~1;
        }
        else
        {
            *registers[15] = (*registers[15] & ~3) + 4;
        }
    }
}

FORCE_INLINE void Interpreter::stmdbU(uint32_t opcode) // STMDB Rn, <Rlist>^
{
    // Decode the operand
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16];

    // Block store, pre-decrement
    for (int i = 15; i >= 0; i--)
    {
        if (opcode & BIT(i))
        {
            op0 -= 4;
            memory->write<uint32_t>(cp15, op0, registersUsr[i]);
        }
    }
}

FORCE_INLINE void Interpreter::ldmibU(uint32_t opcode) // LDMIB Rn, <Rlist>^
{
    // Decode the operand
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16];

    // Block load, pre-increment
    for (int i = 0; i <= 15; i++)
    {
        if (opcode & BIT(i))
        {
            op0 += 4;
            registersUsr[i] = memory->read<uint32_t>(cp15, op0);
        }
    }

    // Handle pipelining, THUMB switching, and CPU mode switching
    if (opcode & BIT(15))
    {
        if (spsr)
        {
            cpsr = *spsr;
            setMode(cpsr);
        }

        if (cp15 && (*registers[15] & BIT(0)))
        {
            cpsr |= BIT(5);
            *registers[15] &= ~1;
        }
        else
        {
            *registers[15] = (*registers[15] & ~3) + 4;
        }
    }
}

FORCE_INLINE void Interpreter::stmibU(uint32_t opcode) // STMIB Rn, <Rlist>^
{
    // Decode the operand
    uint32_t op0 = *registers[(opcode & 0x000F0000) >> 16];

    // Block store, pre-increment
    for (int i = 0; i <= 15; i++)
    {
        if (opcode & BIT(i))
        {
            op0 += 4;
            memory->write<uint32_t>(cp15, op0, registersUsr[i]);
        }
    }
}

FORCE_INLINE void Interpreter::ldmdaUW(uint32_t opcode) // LDMDA Rn!, <Rlist>^
{
    // Decode the operand
    uint32_t *op0 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t address = *op0;

    // Block load, post-decrement
    for (int i = 15; i >= 0; i--)
    {
        if (opcode & BIT(i))
        {
            registersUsr[i] = memory->read<uint32_t>(cp15, address);
            address -= 4;
        }
    }

    // Writeback
    if (!(opcode & BIT((opcode & 0x000F0000) >> 16)))
        *op0 = address;

    // Handle pipelining, THUMB switching, and CPU mode switching
    if (opcode & BIT(15))
    {
        if (spsr)
        {
            cpsr = *spsr;
            setMode(cpsr);
        }

        if (cp15 && (*registers[15] & BIT(0)))
        {
            cpsr |= BIT(5);
            *registers[15] &= ~1;
        }
        else
        {
            *registers[15] = (*registers[15] & ~3) + 4;
        }
    }
}

FORCE_INLINE void Interpreter::stmdaUW(uint32_t opcode) // STMDA Rn!, <Rlist>^
{
    // Decode the operand
    uint32_t *op0 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t address = *op0;

    // Block store, post-decrement
    for (int i = 15; i >= 0; i--)
    {
        if (opcode & BIT(i))
        {
            memory->write<uint32_t>(cp15, address, registersUsr[i]);
            address -= 4;
        }
    }

    // Writeback
    *op0 = address;
}

FORCE_INLINE void Interpreter::ldmiaUW(uint32_t opcode) // LDMIA Rn!, <Rlist>^
{
    // Decode the operand
    uint32_t *op0 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t address = *op0;

    // Block load, post-increment
    for (int i = 0; i <= 15; i++)
    {
        if (opcode & BIT(i))
        {
            registersUsr[i] = memory->read<uint32_t>(cp15, address);
            address += 4;
        }
    }

    // Writeback
    if (!(opcode & BIT((opcode & 0x000F0000) >> 16)))
        *op0 = address;

    // Handle pipelining, THUMB switching, and CPU mode switching
    if (opcode & BIT(15))
    {
        if (spsr)
        {
            cpsr = *spsr;
            setMode(cpsr);
        }

        if (cp15 && (*registers[15] & BIT(0)))
        {
            cpsr |= BIT(5);
            *registers[15] &= ~1;
        }
        else
        {
            *registers[15] = (*registers[15] & ~3) + 4;
        }
    }
}

FORCE_INLINE void Interpreter::stmiaUW(uint32_t opcode) // STMIA Rn!, <Rlist>^
{
    // Decode the operand
    uint32_t *op0 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t address = *op0;

    // Block store, post-increment
    for (int i = 0; i <= 15; i++)
    {
        if (opcode & BIT(i))
        {
            memory->write<uint32_t>(cp15, address, registersUsr[i]);
            address += 4;
        }
    }

    // Writeback
    *op0 = address;
}

FORCE_INLINE void Interpreter::ldmdbUW(uint32_t opcode) // LDMDB Rn!, <Rlist>^
{
    // Decode the operand
    uint32_t *op0 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t address = *op0;

    // Block load, pre-decrement
    for (int i = 15; i >= 0; i--)
    {
        if (opcode & BIT(i))
        {
            address -= 4;
            registersUsr[i] = memory->read<uint32_t>(cp15, address);
        }
    }

    // Writeback
    if (!(opcode & BIT((opcode & 0x000F0000) >> 16)))
        *op0 = address;

    // Handle pipelining, THUMB switching, and CPU mode switching
    if (opcode & BIT(15))
    {
        if (spsr)
        {
            cpsr = *spsr;
            setMode(cpsr);
        }

        if (cp15 && (*registers[15] & BIT(0)))
        {
            cpsr |= BIT(5);
            *registers[15] &= ~1;
        }
        else
        {
            *registers[15] = (*registers[15] & ~3) + 4;
        }
    }
}

FORCE_INLINE void Interpreter::stmdbUW(uint32_t opcode) // STMDB Rn!, <Rlist>^
{
    // Decode the operand
    uint32_t *op0 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t address = *op0;

    // Block store, pre-decrement
    for (int i = 15; i >= 0; i--)
    {
        if (opcode & BIT(i))
        {
            address -= 4;
            memory->write<uint32_t>(cp15, address, registersUsr[i]);
        }
    }

    // Writeback
    *op0 = address;
}

FORCE_INLINE void Interpreter::ldmibUW(uint32_t opcode) // LDMIB Rn!, <Rlist>^
{
    // Decode the operand
    uint32_t *op0 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t address = *op0;

    // Block load, pre-increment with writeback
    for (int i = 0; i <= 15; i++)
    {
        if (opcode & BIT(i))
        {
            address += 4;
            registersUsr[i] = memory->read<uint32_t>(cp15, address);
        }
    }

    // Writeback
    if (!(opcode & BIT((opcode & 0x000F0000) >> 16)))
        *op0 = address;

    // Handle pipelining, THUMB switching, and CPU mode switching
    if (opcode & BIT(15))
    {
        if (spsr)
        {
            cpsr = *spsr;
            setMode(cpsr);
        }

        if (cp15 && (*registers[15] & BIT(0)))
        {
            cpsr |= BIT(5);
            *registers[15] &= ~1;
        }
        else
        {
            *registers[15] = (*registers[15] & ~3) + 4;
        }
    }
}

FORCE_INLINE void Interpreter::stmibUW(uint32_t opcode) // STMIB Rn!, <Rlist>^
{
    // Decode the operand
    uint32_t *op0 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t address = *op0;

    // Block store, pre-increment
    for (int i = 0; i <= 15; i++)
    {
        if (opcode & BIT(i))
        {
            address += 4;
            memory->write<uint32_t>(cp15, address, registersUsr[i]);
        }
    }

    // Writeback
    *op0 = address;
}

FORCE_INLINE void Interpreter::msrRc(uint32_t opcode) // MSR CPSR,Rm
{
    // Decode the operand
    uint32_t op1 = *registers[opcode & 0x0000000F];

    // Write the first 8 bits of the status flags, but only allow changing the CPU mode when not in user mode
    if (opcode & BIT(16))
    {
        cpsr = (cpsr & ~0x000000E0) | (op1 & 0x000000E0);
        if ((cpsr & 0x0000001F) != 0x10)
            setMode(op1);
    }

    // Write the remaining 8-bit blocks of the status flags
    for (int i = 0; i < 3; i++)
    {
        if (opcode & BIT(17 + i))
            cpsr = (cpsr & ~(0x0000FF00 << (i * 8))) | (op1 & (0x0000FF00 << (i * 8)));
    }
}

FORCE_INLINE void Interpreter::msrRs(uint32_t opcode) // MSR SPSR,Rm
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
}

FORCE_INLINE void Interpreter::msrIc(uint32_t opcode) // MSR CPSR,#i
{
    // Decode the operand
    // Can be any 8 bits rotated right by a multiple of 2
    uint32_t value = opcode & 0x000000FF;
    uint8_t shift = (opcode & 0x00000F00) >> 7;
    uint32_t op1 = (value << (32 - shift)) | (value >> shift);

    // Write the first 8 bits of the status flags, but only allow changing the CPU mode when not in user mode
    if (opcode & BIT(16))
    {
        cpsr = (cpsr & ~0x000000E0) | (op1 & 0x000000E0);
        if ((cpsr & 0x0000001F) != 0x10)
            setMode(op1);
    }

    // Write the remaining 8-bit blocks of the status flags
    for (int i = 0; i < 3; i++)
    {
        if (opcode & BIT(17 + i))
            cpsr = (cpsr & ~(0x0000FF00 << (i * 8))) | (op1 & (0x0000FF00 << (i * 8)));
    }
}

FORCE_INLINE void Interpreter::msrIs(uint32_t opcode) // MSR SPSR,#i
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
}

FORCE_INLINE void Interpreter::mrsRc(uint32_t opcode) // MRS Rd,CPSR
{
    // Decode the operand
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];

    // Copy the status flags to a register
    *op0 = cpsr;
}

FORCE_INLINE void Interpreter::mrsRs(uint32_t opcode) // MRS Rd,SPSR
{
    // Decode the operand
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];

    // Copy the saved status flags to a register
    if (spsr) *op0 = *spsr;
}

FORCE_INLINE void Interpreter::mrc(uint32_t opcode) // MRC Pn,<cpopc>,Rd,Cn,Cm,<cp>
{
    if (!cp15) return; // ARM9 exclusive

    // Decode the operands
    uint32_t *op2 = registers[(opcode & 0x0000F000) >> 12];
    int op3 = (opcode & 0x000F0000) >> 16;
    int op4 = opcode & 0x0000000F;
    int op5 = (opcode & 0x000000E0) >> 5;

    // Read from a CP15 register
    *op2 = cp15->read(op3, op4, op5);
}

FORCE_INLINE void Interpreter::mcr(uint32_t opcode) // MCR Pn,<cpopc>,Rd,Cn,Cm,<cp>
{
    if (!cp15) return; // ARM9 exclusive

    // Decode the operands
    uint32_t op2 = *registers[(opcode & 0x0000F000) >> 12];
    int op3 = (opcode & 0x000F0000) >> 16;
    int op4 = opcode & 0x0000000F;
    int op5 = (opcode & 0x000000E0) >> 5;

    // Write to a CP15 register
    cp15->write(op3, op4, op5, op2);
}

FORCE_INLINE void Interpreter::ldrsbRegT(uint16_t opcode) // LDRSB Rd,[Rb,Ro]
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = *registers[(opcode & 0x01C0) >> 6];

    // Signed byte load, pre-adjust without writeback
    *op0 = memory->read<int8_t>(cp15, op1 + op2);
}

FORCE_INLINE void Interpreter::ldrshRegT(uint16_t opcode) // LDRSH Rd,[Rb,Ro]
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = *registers[(opcode & 0x01C0) >> 6];

    // Signed half-word load, pre-adjust without writeback
    *op0 = memory->read<int16_t>(cp15, op1 + op2);
}

FORCE_INLINE void Interpreter::ldrbRegT(uint16_t opcode) // LDRB Rd,[Rb,Ro]
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = *registers[(opcode & 0x01C0) >> 6];

    // Byte load, pre-adjust without writeback
    *op0 = memory->read<uint8_t>(cp15, op1 + op2);
}

FORCE_INLINE void Interpreter::strbRegT(uint16_t opcode) // STRB Rd,[Rb,Ro]
{
    // Decode the operands
    uint32_t op0 = *registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = *registers[(opcode & 0x01C0) >> 6];

    // Byte write, pre-adjust without writeback
    memory->write<uint8_t>(cp15, op1 + op2, op0);
}

FORCE_INLINE void Interpreter::ldrhRegT(uint16_t opcode) // LDRH Rd,[Rb,Ro]
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = *registers[(opcode & 0x01C0) >> 6];

    // Half-word load, pre-adjust without writeback
    *op0 = memory->read<uint16_t>(cp15, op1 + op2);
}

FORCE_INLINE void Interpreter::strhRegT(uint16_t opcode) // STRH Rd,[Rb,Ro]
{
    // Decode the operands
    uint32_t op0 = *registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = *registers[(opcode & 0x01C0) >> 6];

    // Half-word write, pre-adjust without writeback
    memory->write<uint16_t>(cp15, op1 + op2, op0);
}

FORCE_INLINE void Interpreter::ldrRegT(uint16_t opcode) // LDR Rd,[Rb,Ro]
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = *registers[(opcode & 0x01C0) >> 6];

    // Word load, pre-adjust without writeback
    *op0 = memory->read<uint32_t>(cp15, op1 += op2);

    // Rotate misaligned reads
    if (op1 & 3)
    {
        int shift = (op1 & 3) * 8;
        *op0 = (*op0 << (32 - shift)) | (*op0 >> shift);
    }
}

FORCE_INLINE void Interpreter::strRegT(uint16_t opcode) // STR Rd,[Rb,Ro]
{
    // Decode the operands
    uint32_t op0 = *registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = *registers[(opcode & 0x01C0) >> 6];

    // Word write, pre-adjust without writeback
    memory->write<uint32_t>(cp15, op1 + op2, op0);
}

FORCE_INLINE void Interpreter::ldrbImm5T(uint16_t opcode) // LDRB Rd,[Rb,#i]
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = (opcode & 0x07C0) >> 6;

    // Byte load, pre-adjust without writeback
    *op0 = memory->read<uint8_t>(cp15, op1 + op2);
}

FORCE_INLINE void Interpreter::strbImm5T(uint16_t opcode) // STRB Rd,[Rb,#i]
{
    // Decode the operands
    uint32_t op0 = *registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = (opcode & 0x07C0) >> 6;

    // Byte store, pre-adjust without writeback
    memory->write<uint8_t>(cp15, op1 + op2, op0);
}

FORCE_INLINE void Interpreter::ldrhImm5T(uint16_t opcode) // LDRH Rd,[Rb,#i]
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = (opcode & 0x07C0) >> 5;

    // Half-word load, pre-adjust without writeback
    *op0 = memory->read<uint16_t>(cp15, op1 + op2);
}

FORCE_INLINE void Interpreter::strhImm5T(uint16_t opcode) // STRH Rd,[Rb,#i]
{
    // Decode the operands
    uint32_t op0 = *registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = (opcode & 0x07C0) >> 5;

    // Half-word store, pre-adjust without writeback
    memory->write<uint16_t>(cp15, op1 + op2, op0);
}

FORCE_INLINE void Interpreter::ldrImm5T(uint16_t opcode) // LDR Rd,[Rb,#i]
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = (opcode & 0x07C0) >> 4;

    // Word load, pre-adjust without writeback
    *op0 = memory->read<uint32_t>(cp15, op1 += op2);

    // Rotate misaligned reads
    if (op1 & 3)
    {
        int shift = (op1 & 3) * 8;
        *op0 = (*op0 << (32 - shift)) | (*op0 >> shift);
    }
}

FORCE_INLINE void Interpreter::strImm5T(uint16_t opcode) // STR Rd,[Rb,#i]
{
    // Decode the operands
    uint32_t op0 = *registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = (opcode & 0x07C0) >> 4;

    // Word store, pre-adjust without writeback
    memory->write<uint32_t>(cp15, op1 + op2, op0);
}

FORCE_INLINE void Interpreter::ldrPcT(uint16_t opcode) // LDR Rd,[PC,#i]
{
    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x0700) >> 8];
    uint32_t op1 = *registers[15] & ~3;
    uint32_t op2 = (opcode & 0x00FF) << 2;

    // Word load, pre-adjust without writeback
    *op0 = memory->read<uint32_t>(cp15, op1 += op2);

    // Rotate misaligned reads
    if (op1 & 3)
    {
        int shift = (op1 & 3) * 8;
        *op0 = (*op0 << (32 - shift)) | (*op0 >> shift);
    }
}

FORCE_INLINE void Interpreter::ldrSpT(uint16_t opcode) // LDR Rd,[SP,#i]
{
    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x0700) >> 8];
    uint32_t op1 = *registers[13];
    uint32_t op2 = (opcode & 0x00FF) << 2;

    // Word load, pre-adjust without writeback
    *op0 = memory->read<uint32_t>(cp15, op1 += op2);

    // Rotate misaligned reads
    if (op1 & 3)
    {
        int shift = (op1 & 3) * 8;
        *op0 = (*op0 << (32 - shift)) | (*op0 >> shift);
    }
}

FORCE_INLINE void Interpreter::strSpT(uint16_t opcode) // STR Rd,[SP,#i]
{
    // Decode the operands
    uint32_t op0 = *registers[(opcode & 0x0700) >> 8];
    uint32_t op1 = *registers[13];
    uint32_t op2 = (opcode & 0x00FF) << 2;

    // Word store, pre-adjust without writeback
    memory->write<uint32_t>(cp15, op1 + op2, op0);
}

FORCE_INLINE void Interpreter::ldmiaT(uint16_t opcode) // LDMIA Rb!,<Rlist>
{
    // Decode the operand
    uint32_t *op0 = registers[(opcode & 0x0700) >> 8];
    uint32_t address = *op0;

    // Block load, post-increment
    for (int i = 0; i <= 7; i++)
    {
        if (opcode & BIT(i))
        {
            *registers[i] = memory->read<uint32_t>(cp15, address);
            address += 4;
        }
    }

    // Writeback
    if (!(opcode & BIT((opcode & 0x0700) >> 8)))
        *op0 = address;
}

FORCE_INLINE void Interpreter::stmiaT(uint16_t opcode) // STMIA Rb!,<Rlist>
{
    // Decode the operand
    uint32_t *op0 = registers[(opcode & 0x0700) >> 8];
    uint32_t address = *op0;

    // Block store, post-increment
    for (int i = 0; i <= 7; i++)
    {
        if (opcode & BIT(i))
        {
            memory->write<uint32_t>(cp15, address, *registers[i]);
            address += 4;
        }
    }

    // Writeback
    *op0 = address;
}

FORCE_INLINE void Interpreter::popT(uint16_t opcode) // POP <Rlist>
{
    // Decode the operand
    uint32_t *op0 = registers[13];
    uint32_t address = *op0;

    // Block load, post-increment
    for (int i = 0; i <= 7; i++)
    {
        if (opcode & BIT(i))
        {
            *registers[i] = memory->read<uint32_t>(cp15, address);
            address += 4;
        }
    }

    // Writeback
    *op0 = address;
}

FORCE_INLINE void Interpreter::pushT(uint16_t opcode) // PUSH <Rlist>
{
    // Decode the operand
    uint32_t *op0 = registers[13];
    uint32_t address = *op0;

    // Block store, pre-decrement
    for (int i = 7; i >= 0; i--)
    {
        if (opcode & BIT(i))
        {
            address -= 4;
            memory->write<uint32_t>(cp15, address, *registers[i]);
        }
    }

    // Writeback
    *op0 = address;
}

FORCE_INLINE void Interpreter::popPcT(uint16_t opcode) // POP <Rlist>,PC
{
    // Decode the operand
    uint32_t *op0 = registers[13];
    uint32_t address = *op0;

    // Block load, post-increment
    for (int i = 0; i <= 7; i++)
    {
        if (opcode & BIT(i))
        {
            *registers[i] = memory->read<uint32_t>(cp15, address);
            address += 4;
        }
    }
    *registers[15] = memory->read<uint32_t>(cp15, address);
    address += 4;

    // Writeback
    *op0 = address;

    // Handle pipelining
    if (cp15 && (*registers[15] & BIT(0)))
    {
        *registers[15] = (*registers[15] & ~1) + 2;
    }
    else
    {
        cpsr &= ~BIT(5);
        *registers[15] = (*registers[15] & ~3) + 6;
    }
}

FORCE_INLINE void Interpreter::pushLrT(uint16_t opcode) // PUSH <Rlist>,LR
{
    // Decode the operand
    uint32_t *op0 = registers[13];
    uint32_t address = *op0;

    // Block store, pre-decrement
    address -= 4;
    memory->write<uint32_t>(cp15, address, *registers[14]);
    for (int i = 7; i >= 0; i--)
    {
        if (opcode & BIT(i))
        {
            address -= 4;
            memory->write<uint32_t>(cp15, address, *registers[i]);
        }
    }

    // Writeback
    *op0 = address;
}

#endif // INTERPRETER_TRANSFER
