/*
    Copyright 2019-2022 Hydr8gon

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

FORCE_INLINE uint32_t Interpreter::lli(uint32_t opcode) // Rm,LSL #i
{
    // Decode the operands
    uint32_t value = *registers[opcode & 0x0000000F];
    uint8_t shift = (opcode & 0x00000F80) >> 7;

    // Logical shift left by immediate
    return value << shift;
}

FORCE_INLINE uint32_t Interpreter::llr(uint32_t opcode) // Rm,LSL Rs
{
    // Decode the operands
    // When used as Rm, the program counter is read with +4
    uint32_t value = *registers[opcode & 0x0000000F] + (((opcode & 0x0000000F) == 0x0000000F) ? 4 : 0);
    uint8_t shift = *registers[(opcode & 0x00000F00) >> 8];

    // Logical shift left by register
    // Shifts greater than 32 can be wonky on host systems, so they're handled explicitly
    return (shift < 32) ? (value << shift) : 0;
}

FORCE_INLINE uint32_t Interpreter::lri(uint32_t opcode) // Rm,LSR #i
{
    // Decode the operands
    uint32_t value = *registers[opcode & 0x0000000F];
    uint8_t shift = (opcode & 0x00000F80) >> 7;

    // Logical shift right by immediate
    // A shift of 0 translates to a shift of 32
    return shift ? (value >> shift) : 0;
}

FORCE_INLINE uint32_t Interpreter::lrr(uint32_t opcode) // Rm,LSR Rs
{
    // Decode the operands
    // When used as Rm, the program counter is read with +4
    uint32_t value = *registers[opcode & 0x0000000F] + (((opcode & 0x0000000F) == 0x0000000F) ? 4 : 0);
    uint8_t shift = *registers[(opcode & 0x00000F00) >> 8];

    // Logical shift right by register
    // Shifts greater than 32 can be wonky on host systems, so they're handled explicitly
    return (shift < 32) ? (value >> shift) : 0;
}

FORCE_INLINE uint32_t Interpreter::ari(uint32_t opcode) // Rm,ASR #i
{
    // Decode the operands
    uint32_t value = *registers[opcode & 0x0000000F];
    uint8_t shift = (opcode & 0x00000F80) >> 7;

    // Arithmetic shift right by immediate
    // A shift of 0 translates to a shift of 32
    return shift ? ((int32_t)value >> shift) : ((value & BIT(31)) ? 0xFFFFFFFF : 0);
}

FORCE_INLINE uint32_t Interpreter::arr(uint32_t opcode) // Rm,ASR Rs
{
    // Decode the operands
    // When used as Rm, the program counter is read with +4
    uint32_t value = *registers[opcode & 0x0000000F] + (((opcode & 0x0000000F) == 0x0000000F) ? 4 : 0);
    uint8_t shift = *registers[(opcode & 0x00000F00) >> 8];

    // Arithmetic shift right by register
    // Shifts greater than 32 can be wonky on host systems, so they're handled explicitly
    return (shift < 32) ? ((int32_t)value >> shift) : ((value & BIT(31)) ? 0xFFFFFFFF : 0);
}

FORCE_INLINE uint32_t Interpreter::rri(uint32_t opcode) // Rm,ROR #i
{
    // Decode the operands
    uint32_t value = *registers[opcode & 0x0000000F];
    uint8_t shift = (opcode & 0x00000F80) >> 7;

    // Rotate right by immediate
    // A shift of 0 translates to a rotate with carry of 1
    return shift ? ((value << (32 - shift)) | (value >> shift)) : (((cpsr & BIT(29)) << 2) | (value >> 1));
}

FORCE_INLINE uint32_t Interpreter::rrr(uint32_t opcode) // Rm,ROR Rs
{
    // Decode the operands
    // When used as Rm, the program counter is read with +4
    uint32_t value = *registers[opcode & 0x0000000F] + (((opcode & 0x0000000F) == 0x0000000F) ? 4 : 0);
    uint8_t shift = *registers[(opcode & 0x00000F00) >> 8];

    // Rotate right by register
    return (value << (32 - shift % 32)) | (value >> (shift % 32));
}

FORCE_INLINE uint32_t Interpreter::imm(uint32_t opcode) // #i
{
    // Decode the operands
    uint32_t value = opcode & 0x000000FF;
    uint8_t shift = (opcode & 0x00000F00) >> 7;

    // Immediate
    // Can be any 8 bits rotated right by a multiple of 2
    return (value << (32 - shift)) | (value >> shift);
}

FORCE_INLINE uint32_t Interpreter::lliS(uint32_t opcode) // Rm,LSL #i (S)
{
    // Decode the operands
    uint32_t value = *registers[opcode & 0x0000000F];
    uint8_t shift = (opcode & 0x00000F80) >> 7;

    // Set the carry flag
    if (shift > 0)
        cpsr = (cpsr & ~BIT(29)) | ((bool)(value & BIT(32 - shift)) << 29);

    // Logical shift left by immediate
    return value << shift;
}


FORCE_INLINE uint32_t Interpreter::llrS(uint32_t opcode) // Rm,LSL Rs (S)
{
    // Decode the operands
    // When used as Rm, the program counter is read with +4
    uint32_t value = *registers[opcode & 0x0000000F] + (((opcode & 0x0000000F) == 0x0000000F) ? 4 : 0);
    uint8_t shift = *registers[(opcode & 0x00000F00) >> 8];

    // Set the carry flag
    if (shift > 0)
        cpsr = (cpsr & ~BIT(29)) | ((shift <= 32 && (value & BIT(32 - shift))) << 29);

    // Logical shift left by register
    // Shifts greater than 32 can be wonky on host systems, so they're handled explicitly
    return (shift < 32) ? (value << shift) : 0;
}

FORCE_INLINE uint32_t Interpreter::lriS(uint32_t opcode) // Rm,LSR #i (S)
{
    // Decode the operands
    uint32_t value = *registers[opcode & 0x0000000F];
    uint8_t shift = (opcode & 0x00000F80) >> 7;

    // Set the carry flag
    cpsr = (cpsr & ~BIT(29)) | ((bool)(value & BIT(shift ? (shift - 1) : 31)) << 29);

    // Logical shift right by immediate
    // A shift of 0 translates to a shift of 32
    return shift ? (value >> shift) : 0;
}

FORCE_INLINE uint32_t Interpreter::lrrS(uint32_t opcode) // Rm,LSR Rs (S)
{
    // Decode the operands
    // When used as Rm, the program counter is read with +4
    uint32_t value = *registers[opcode & 0x0000000F] + (((opcode & 0x0000000F) == 0x0000000F) ? 4 : 0);
    uint8_t shift = *registers[(opcode & 0x00000F00) >> 8];

    // Set the carry flag
    if (shift > 0)
        cpsr = (cpsr & ~BIT(29)) | ((shift <= 32 && (value & BIT(shift - 1))) << 29);

    // Logical shift right by register
    // Shifts greater than 32 can be wonky on host systems, so they're handled explicitly
    return (shift < 32) ? (value >> shift) : 0;
}

FORCE_INLINE uint32_t Interpreter::ariS(uint32_t opcode) // Rm,ASR #i (S)
{
    // Decode the operands
    uint32_t value = *registers[opcode & 0x0000000F];
    uint8_t shift = (opcode & 0x00000F80) >> 7;

    // Set the carry flag
    cpsr = (cpsr & ~BIT(29)) | ((bool)(value & BIT(shift ? (shift - 1) : 31)) << 29);

    // Arithmetic shift right by immediate
    // A shift of 0 translates to a shift of 32
    return shift ? ((int32_t)value >> shift) : ((value & BIT(31)) ? 0xFFFFFFFF : 0);
}

FORCE_INLINE uint32_t Interpreter::arrS(uint32_t opcode) // Rm,ASR Rs (S)
{
    // Decode the operands
    // When used as Rm, the program counter is read with +4
    uint32_t value = *registers[opcode & 0x0000000F] + (((opcode & 0x0000000F) == 0x0000000F) ? 4 : 0);
    uint8_t shift = *registers[(opcode & 0x00000F00) >> 8];

    // Set the carry flag
    if (shift > 0)
        cpsr = (cpsr & ~BIT(29)) | ((bool)(value & BIT((shift <= 32) ? (shift - 1) : 31)) << 29);

    // Arithmetic shift right by register
    // Shifts greater than 32 can be wonky on host systems, so they're handled explicitly
    return (shift < 32) ? ((int32_t)value >> shift) : ((value & BIT(31)) ? 0xFFFFFFFF : 0);
}

FORCE_INLINE uint32_t Interpreter::rriS(uint32_t opcode) // Rm,ROR #i (S)
{
    // Decode the operands
    uint32_t value = *registers[opcode & 0x0000000F];
    uint8_t shift = (opcode & 0x00000F80) >> 7;

    // Rotate right by immediate
    // A shift of 0 translates to a rotate with carry of 1
    uint32_t res = shift ? ((value << (32 - shift)) | (value >> shift)) : (((cpsr & BIT(29)) << 2) | (value >> 1));

    // Set the carry flag
    cpsr = (cpsr & ~BIT(29)) | ((bool)(value & BIT(shift ? (shift - 1) : 0)) << 29);

    return res;
}

FORCE_INLINE uint32_t Interpreter::rrrS(uint32_t opcode) // Rm,ROR Rs (S)
{
    // Decode the operands
    // When used as Rm, the program counter is read with +4
    uint32_t value = *registers[opcode & 0x0000000F] + (((opcode & 0x0000000F) == 0x0000000F) ? 4 : 0);
    uint8_t shift = *registers[(opcode & 0x00000F00) >> 8];

    // Set the carry flag
    if (shift > 0)
        cpsr = (cpsr & ~BIT(29)) | ((bool)(value & BIT((shift - 1) % 32)) << 29);

    // Rotate right by register
    return (value << (32 - shift % 32)) | (value >> (shift % 32));
}

FORCE_INLINE uint32_t Interpreter::immS(uint32_t opcode) // #i (S)
{
    // Decode the operands
    uint32_t value = opcode & 0x000000FF;
    uint8_t shift = (opcode & 0x00000F00) >> 7;

    // Set the carry flag
    if (shift > 0)
        cpsr = (cpsr & ~BIT(29)) | ((bool)(value & BIT(shift - 1)) << 29);

    // Immediate
    // Can be any 8 bits rotated right by a multiple of 2
    return (value << (32 - shift)) | (value >> shift);
}

FORCE_INLINE int Interpreter::_and(uint32_t opcode, uint32_t op2) // AND Rd,Rn,op2
{
    // Decode the other operands
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Bitwise and
    *op0 = op1 & op2;

    // Handle pipelining
    if (op0 != registers[15]) return 1;
    flushPipeline();
    return 3;
}

FORCE_INLINE int Interpreter::eor(uint32_t opcode, uint32_t op2) // EOR Rd,Rn,op2
{
    // Decode the other operands
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Bitwise exclusive or
    *op0 = op1 ^ op2;

    // Handle pipelining
    if (op0 != registers[15]) return 1;
    flushPipeline();
    return 3;
}

FORCE_INLINE int Interpreter::sub(uint32_t opcode, uint32_t op2) // SUB Rd,Rn,op2
{
    // Decode the other operands
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Subtraction
    *op0 = op1 - op2;

    // Handle pipelining
    if (op0 != registers[15]) return 1;
    flushPipeline();
    return 3;
}

FORCE_INLINE int Interpreter::rsb(uint32_t opcode, uint32_t op2) // RSB Rd,Rn,op2
{
    // Decode the other operands
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Reverse subtraction
    *op0 = op2 - op1;

    // Handle pipelining
    if (op0 != registers[15]) return 1;
    flushPipeline();
    return 3;
}

FORCE_INLINE int Interpreter::add(uint32_t opcode, uint32_t op2) // ADD Rd,Rn,op2
{
    // Decode the other operands
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Addition
    *op0 = op1 + op2;

    // Handle pipelining
    if (op0 != registers[15]) return 1;
    flushPipeline();
    return 3;
}

FORCE_INLINE int Interpreter::adc(uint32_t opcode, uint32_t op2) // ADC Rd,Rn,op2
{
    // Decode the other operands
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Addition with carry
    *op0 = op1 + op2 + ((cpsr & BIT(29)) >> 29);

    // Handle pipelining
    if (op0 != registers[15]) return 1;
    flushPipeline();
    return 3;
}

FORCE_INLINE int Interpreter::sbc(uint32_t opcode, uint32_t op2) // SBC Rd,Rn,op2
{
    // Decode the other operands
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Subtraction with carry
    *op0 = op1 - op2 - 1 + ((cpsr & BIT(29)) >> 29);

    // Handle pipelining
    if (op0 != registers[15]) return 1;
    flushPipeline();
    return 3;
}

FORCE_INLINE int Interpreter::rsc(uint32_t opcode, uint32_t op2) // RSC Rd,Rn,op2
{
    // Decode the other operands
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Reverse subtraction with carry
    *op0 = op2 - op1 - 1 + ((cpsr & BIT(29)) >> 29);

    // Handle pipelining
    if (op0 != registers[15]) return 1;
    flushPipeline();
    return 3;
}

FORCE_INLINE int Interpreter::tst(uint32_t opcode, uint32_t op2) // TST Rn,op2
{
    // Decode the other operand
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Test bits
    uint32_t res = op1 & op2;

    // Set the flags
    cpsr = (cpsr & ~0xC0000000) | (res & BIT(31)) | ((res == 0) << 30);

    return 1;
}

FORCE_INLINE int Interpreter::teq(uint32_t opcode, uint32_t op2) // TEQ Rn,op2
{
    // Decode the other operand
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Test bits
    uint32_t res = op1 ^ op2;

    // Set the flags
    cpsr = (cpsr & ~0xC0000000) | (res & BIT(31)) | ((res == 0) << 30);

    return 1;
}

FORCE_INLINE int Interpreter::cmp(uint32_t opcode, uint32_t op2) // CMP Rn,op2
{
    // Decode the other operand
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Compare
    uint32_t res = op1 - op2;

    // Set the flags
    cpsr = (cpsr & ~0xF0000000) | (res & BIT(31)) | ((res == 0) << 30) | ((op1 >= res) << 29) |
        (((op2 & BIT(31)) != (op1 & BIT(31)) && (res & BIT(31)) == (op2 & BIT(31))) << 28);

    return 1;
}

FORCE_INLINE int Interpreter::cmn(uint32_t opcode, uint32_t op2) // CMN Rn,op2
{
    // Decode the other operand
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Compare negative
    uint32_t res = op1 + op2;

    // Set the flags
    cpsr = (cpsr & ~0xF0000000) | (res & BIT(31)) | ((res == 0) << 30) | ((op1 > res) << 29) |
        (((op2 & BIT(31)) == (op1 & BIT(31)) && (res & BIT(31)) != (op2 & BIT(31))) << 28);

    return 1;
}

FORCE_INLINE int Interpreter::orr(uint32_t opcode, uint32_t op2) // ORR Rd,Rn,op2
{
    // Decode the other operands
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Bitwise or
    *op0 = op1 | op2;

    // Handle pipelining
    if (op0 != registers[15]) return 1;
    flushPipeline();
    return 3;
}

FORCE_INLINE int Interpreter::mov(uint32_t opcode, uint32_t op2) // MOV Rd,op2
{
    // Decode the other operand
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];

    // Move
    *op0 = op2;

    // Handle pipelining
    if (op0 != registers[15]) return 1;
    flushPipeline();
    return 3;
}

FORCE_INLINE int Interpreter::bic(uint32_t opcode, uint32_t op2) // BIC Rd,Rn,op2
{
    // Decode the other operands
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Bit clear
    *op0 = op1 & ~op2;

    // Handle pipelining
    if (op0 != registers[15]) return 1;
    flushPipeline();
    return 3;
}

FORCE_INLINE int Interpreter::mvn(uint32_t opcode, uint32_t op2) // MVN Rd,op2
{
    // Decode the other operand
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];

    // Move negative
    *op0 = ~op2;

    // Handle pipelining
    if (op0 != registers[15]) return 1;
    flushPipeline();
    return 3;
}

FORCE_INLINE int Interpreter::ands(uint32_t opcode, uint32_t op2) // ANDS Rd,Rn,op2
{
    // Decode the other operands
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Bitwise and
    *op0 = op1 & op2;

    // Handle mode switching
    if (op0 == registers[15] && spsr)
    {
        setCpsr(*spsr);
        flushPipeline();
        return 3;
    }

    // Set the flags
    cpsr = (cpsr & ~0xC0000000) | (*op0 & BIT(31)) | ((*op0 == 0) << 30);

    // Handle pipelining
    if (op0 != registers[15]) return 1;
    flushPipeline();
    return 3;
}

FORCE_INLINE int Interpreter::eors(uint32_t opcode, uint32_t op2) // EORS Rd,Rn,op2
{
    // Decode the other operands
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Bitwise exclusive or
    *op0 = op1 ^ op2;

    // Handle mode switching
    if (op0 == registers[15] && spsr)
    {
        setCpsr(*spsr);
        flushPipeline();
        return 3;
    }

    // Set the flags
    cpsr = (cpsr & ~0xC0000000) | (*op0 & BIT(31)) | ((*op0 == 0) << 30);

    // Handle pipelining
    if (op0 != registers[15]) return 1;
    flushPipeline();
    return 3;
}

FORCE_INLINE int Interpreter::subs(uint32_t opcode, uint32_t op2) // SUBS Rd,Rn,op2
{
    // Decode the other operands
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Subtraction
    *op0 = op1 - op2;

    // Handle mode switching
    if (op0 == registers[15] && spsr)
    {
        setCpsr(*spsr);
        flushPipeline();
        return 3;
    }

    // Set the flags
    cpsr = (cpsr & ~0xF0000000) | (*op0 & BIT(31)) | ((*op0 == 0) << 30) | ((op1 >= *op0) << 29) |
        (((op2 & BIT(31)) != (op1 & BIT(31)) && (*op0 & BIT(31)) == (op2 & BIT(31))) << 28);

    // Handle pipelining
    if (op0 != registers[15]) return 1;
    flushPipeline();
    return 3;
}

FORCE_INLINE int Interpreter::rsbs(uint32_t opcode, uint32_t op2) // RSBS Rd,Rn,op2
{
    // Decode the other operands
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Reverse subtraction
    *op0 = op2 - op1;

    // Handle mode switching
    if (op0 == registers[15] && spsr)
    {
        setCpsr(*spsr);
        flushPipeline();
        return 3;
    }

    // Set the flags
    cpsr = (cpsr & ~0xF0000000) | (*op0 & BIT(31)) | ((*op0 == 0) << 30) | ((op2 >= *op0) << 29) |
        (((op1 & BIT(31)) != (op2 & BIT(31)) && (*op0 & BIT(31)) == (op1 & BIT(31))) << 28);

    // Handle pipelining
    if (op0 != registers[15]) return 1;
    flushPipeline();
    return 3;
}

FORCE_INLINE int Interpreter::adds(uint32_t opcode, uint32_t op2) // ADDS Rd,Rn,op2
{
    // Decode the other operands
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Addition
    *op0 = op1 + op2;

    // Handle mode switching
    if (op0 == registers[15] && spsr)
    {
        setCpsr(*spsr);
        flushPipeline();
        return 3;
    }

    // Set the flags
    cpsr = (cpsr & ~0xF0000000) | (*op0 & BIT(31)) | ((*op0 == 0) << 30) | ((op1 > *op0) << 29) |
        (((op2 & BIT(31)) == (op1 & BIT(31)) && (*op0 & BIT(31)) != (op2 & BIT(31))) << 28);

    // Handle pipelining
    if (op0 != registers[15]) return 1;
    flushPipeline();
    return 3;
}

FORCE_INLINE int Interpreter::adcs(uint32_t opcode, uint32_t op2) // ADCS Rd,Rn,op2
{
    // Decode the other operands
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Addition with carry
    *op0 = op1 + op2 + ((cpsr & BIT(29)) >> 29);

    // Handle mode switching
    if (op0 == registers[15] && spsr)
    {
        setCpsr(*spsr);
        flushPipeline();
        return 3;
    }

    // Set the flags
    cpsr = (cpsr & ~0xF0000000) | (*op0 & BIT(31)) | ((*op0 == 0) << 30) |
        ((op1 > *op0 || (op2 == 0xFFFFFFFF && (cpsr & BIT(29)))) << 29) |
        (((op2 & BIT(31)) == (op1 & BIT(31)) && (*op0 & BIT(31)) != (op2 & BIT(31))) << 28);

    // Handle pipelining
    if (op0 != registers[15]) return 1;
    flushPipeline();
    return 3;
}

FORCE_INLINE int Interpreter::sbcs(uint32_t opcode, uint32_t op2) // SBCS Rd,Rn,op2
{
    // Decode the other operands
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Subtraction with carry
    *op0 = op1 - op2 - 1 + ((cpsr & BIT(29)) >> 29);

    // Handle mode switching
    if (op0 == registers[15] && spsr)
    {
        setCpsr(*spsr);
        flushPipeline();
        return 3;
    }

    // Set the flags
    cpsr = (cpsr & ~0xF0000000) | (*op0 & BIT(31)) | ((*op0 == 0) << 30) |
        ((op1 >= *op0 && (op2 != 0xFFFFFFFF || (cpsr & BIT(29)))) << 29) |
        (((op2 & BIT(31)) != (op1 & BIT(31)) && (*op0 & BIT(31)) == (op2 & BIT(31))) << 28);

    // Handle pipelining
    if (op0 != registers[15]) return 1;
    flushPipeline();
    return 3;
}

FORCE_INLINE int Interpreter::rscs(uint32_t opcode, uint32_t op2) // RSCS Rd,Rn,op2
{
    // Decode the other operands
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Reverse subtraction with carry
    *op0 = op2 - op1 - 1 + ((cpsr & BIT(29)) >> 29);

    // Handle mode switching
    if (op0 == registers[15] && spsr)
    {
        setCpsr(*spsr);
        flushPipeline();
        return 3;
    }

    // Set the flags
    cpsr = (cpsr & ~0xC0000000) | (*op0 & BIT(31)) | ((*op0 == 0) << 30) |
        ((op2 >= *op0 && (op1 != 0xFFFFFFFF || (cpsr & BIT(29)))) << 29) |
        (((op1 & BIT(31)) != (op2 & BIT(31)) && (*op0 & BIT(31)) == (op1 & BIT(31))) << 28);

    // Handle pipelining
    if (op0 != registers[15]) return 1;
    flushPipeline();
    return 3;
}

FORCE_INLINE int Interpreter::orrs(uint32_t opcode, uint32_t op2) // ORRS Rd,Rn,op2
{
    // Decode the other operands
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Bitwise or
    *op0 = op1 | op2;

    // Handle mode switching
    if (op0 == registers[15] && spsr)
    {
        setCpsr(*spsr);
        flushPipeline();
        return 3;
    }

    // Set the flags
    cpsr = (cpsr & ~0xC0000000) | (*op0 & BIT(31)) | ((*op0 == 0) << 30);

    // Handle pipelining
    if (op0 != registers[15]) return 1;
    flushPipeline();
    return 3;
}

FORCE_INLINE int Interpreter::movs(uint32_t opcode, uint32_t op2) // MOVS Rd,op2
{
    // Decode the other operand
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];

    // Move
    *op0 = op2;

    // Handle mode switching
    if (op0 == registers[15] && spsr)
    {
        setCpsr(*spsr);
        flushPipeline();
        return 3;
    }

    // Set the flags
    cpsr = (cpsr & ~0xC0000000) | (*op0 & BIT(31)) | ((*op0 == 0) << 30);

    // Handle pipelining
    if (op0 != registers[15]) return 1;
    flushPipeline();
    return 3;
}

FORCE_INLINE int Interpreter::bics(uint32_t opcode, uint32_t op2) // BICS Rd,Rn,op2
{
    // Decode the other operands
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Bit clear
    *op0 = op1 & ~op2;

    // Handle mode switching
    if (op0 == registers[15] && spsr)
    {
        setCpsr(*spsr);
        flushPipeline();
        return 3;
    }

    // Set the flags
    cpsr = (cpsr & ~0xC0000000) | (*op0 & BIT(31)) | ((*op0 == 0) << 30);

    // Handle pipelining
    if (op0 != registers[15]) return 1;
    flushPipeline();
    return 3;
}

FORCE_INLINE int Interpreter::mvns(uint32_t opcode, uint32_t op2) // MVNS Rd,op2
{
    // Decode the other operand
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];

    // Move negative
    *op0 = ~op2;

    // Handle mode switching
    if (op0 == registers[15] && spsr)
    {
        setCpsr(*spsr);
        flushPipeline();
        return 3;
    }

    // Set the flags
    cpsr = (cpsr & ~0xC0000000) | (*op0 & BIT(31)) | ((*op0 == 0) << 30);

    // Handle pipelining
    if (op0 != registers[15]) return 1;
    flushPipeline();
    return 3;
}

int Interpreter::andLli(uint32_t opcode)  { return _and(opcode, lli(opcode));      } // AND  Rd,Rn,Rm,LSL #i
int Interpreter::andLlr(uint32_t opcode)  { return _and(opcode, llr(opcode)) + 1;  } // AND  Rd,Rn,Rm,LSL Rs
int Interpreter::andLri(uint32_t opcode)  { return _and(opcode, lri(opcode));      } // AND  Rd,Rn,Rm,LSR #i
int Interpreter::andLrr(uint32_t opcode)  { return _and(opcode, lrr(opcode)) + 1;  } // AND  Rd,Rn,Rm,LSR Rs
int Interpreter::andAri(uint32_t opcode)  { return _and(opcode, ari(opcode));      } // AND  Rd,Rn,Rm,ASR #i
int Interpreter::andArr(uint32_t opcode)  { return _and(opcode, arr(opcode)) + 1;  } // AND  Rd,Rn,Rm,ASR Rs
int Interpreter::andRri(uint32_t opcode)  { return _and(opcode, rri(opcode));      } // AND  Rd,Rn,Rm,ROR #i
int Interpreter::andRrr(uint32_t opcode)  { return _and(opcode, rrr(opcode)) + 1;  } // AND  Rd,Rn,Rm,ROR Rs
int Interpreter::andImm(uint32_t opcode)  { return _and(opcode, imm(opcode));      } // AND  Rd,Rn,#i
int Interpreter::andsLli(uint32_t opcode) { return ands(opcode, lliS(opcode));     } // ANDS Rd,Rn,Rm,LSL #i
int Interpreter::andsLlr(uint32_t opcode) { return ands(opcode, llrS(opcode)) + 1; } // ANDS Rd,Rn,Rm,LSL Rs
int Interpreter::andsLri(uint32_t opcode) { return ands(opcode, lriS(opcode));     } // ANDS Rd,Rn,Rm,LSR #i
int Interpreter::andsLrr(uint32_t opcode) { return ands(opcode, lrrS(opcode)) + 1; } // ANDS Rd,Rn,Rm,LSR Rs
int Interpreter::andsAri(uint32_t opcode) { return ands(opcode, ariS(opcode));     } // ANDS Rd,Rn,Rm,ASR #i
int Interpreter::andsArr(uint32_t opcode) { return ands(opcode, arrS(opcode)) + 1; } // ANDS Rd,Rn,Rm,ASR Rs
int Interpreter::andsRri(uint32_t opcode) { return ands(opcode, rriS(opcode));     } // ANDS Rd,Rn,Rm,ROR #i
int Interpreter::andsRrr(uint32_t opcode) { return ands(opcode, rrrS(opcode)) + 1; } // ANDS Rd,Rn,Rm,ROR Rs
int Interpreter::andsImm(uint32_t opcode) { return ands(opcode, immS(opcode));     } // ANDS Rd,Rn,#i

int Interpreter::eorLli(uint32_t opcode)  { return eor(opcode, lli(opcode));       } // EOR  Rd,Rn,Rm,LSL #i
int Interpreter::eorLlr(uint32_t opcode)  { return eor(opcode, llr(opcode)) + 1;   } // EOR  Rd,Rn,Rm,LSL Rs
int Interpreter::eorLri(uint32_t opcode)  { return eor(opcode, lri(opcode));       } // EOR  Rd,Rn,Rm,LSR #i
int Interpreter::eorLrr(uint32_t opcode)  { return eor(opcode, lrr(opcode)) + 1;   } // EOR  Rd,Rn,Rm,LSR Rs
int Interpreter::eorAri(uint32_t opcode)  { return eor(opcode, ari(opcode));       } // EOR  Rd,Rn,Rm,ASR #i
int Interpreter::eorArr(uint32_t opcode)  { return eor(opcode, arr(opcode)) + 1;   } // EOR  Rd,Rn,Rm,ASR Rs
int Interpreter::eorRri(uint32_t opcode)  { return eor(opcode, rri(opcode));       } // EOR  Rd,Rn,Rm,ROR #i
int Interpreter::eorRrr(uint32_t opcode)  { return eor(opcode, rrr(opcode)) + 1;   } // EOR  Rd,Rn,Rm,ROR Rs
int Interpreter::eorImm(uint32_t opcode)  { return eor(opcode, imm(opcode));       } // EOR  Rd,Rn,#i
int Interpreter::eorsLli(uint32_t opcode) { return eors(opcode, lliS(opcode));     } // EORS Rd,Rn,Rm,LSL #i
int Interpreter::eorsLlr(uint32_t opcode) { return eors(opcode, llrS(opcode)) + 1; } // EORS Rd,Rn,Rm,LSL Rs
int Interpreter::eorsLri(uint32_t opcode) { return eors(opcode, lriS(opcode));     } // EORS Rd,Rn,Rm,LSR #i
int Interpreter::eorsLrr(uint32_t opcode) { return eors(opcode, lrrS(opcode)) + 1; } // EORS Rd,Rn,Rm,LSR Rs
int Interpreter::eorsAri(uint32_t opcode) { return eors(opcode, ariS(opcode));     } // EORS Rd,Rn,Rm,ASR #i
int Interpreter::eorsArr(uint32_t opcode) { return eors(opcode, arrS(opcode)) + 1; } // EORS Rd,Rn,Rm,ASR Rs
int Interpreter::eorsRri(uint32_t opcode) { return eors(opcode, rriS(opcode));     } // EORS Rd,Rn,Rm,ROR #i
int Interpreter::eorsRrr(uint32_t opcode) { return eors(opcode, rrrS(opcode)) + 1; } // EORS Rd,Rn,Rm,ROR Rs
int Interpreter::eorsImm(uint32_t opcode) { return eors(opcode, immS(opcode));     } // EORS Rd,Rn,#i

int Interpreter::subLli(uint32_t opcode)  { return sub(opcode, lli(opcode));      } // SUB  Rd,Rn,Rm,LSL #i
int Interpreter::subLlr(uint32_t opcode)  { return sub(opcode, llr(opcode)) + 1;  } // SUB  Rd,Rn,Rm,LSL Rs
int Interpreter::subLri(uint32_t opcode)  { return sub(opcode, lri(opcode));      } // SUB  Rd,Rn,Rm,LSR #i
int Interpreter::subLrr(uint32_t opcode)  { return sub(opcode, lrr(opcode)) + 1;  } // SUB  Rd,Rn,Rm,LSR Rs
int Interpreter::subAri(uint32_t opcode)  { return sub(opcode, ari(opcode));      } // SUB  Rd,Rn,Rm,ASR #i
int Interpreter::subArr(uint32_t opcode)  { return sub(opcode, arr(opcode)) + 1;  } // SUB  Rd,Rn,Rm,ASR Rs
int Interpreter::subRri(uint32_t opcode)  { return sub(opcode, rri(opcode));      } // SUB  Rd,Rn,Rm,ROR #i
int Interpreter::subRrr(uint32_t opcode)  { return sub(opcode, rrr(opcode)) + 1;  } // SUB  Rd,Rn,Rm,ROR Rs
int Interpreter::subImm(uint32_t opcode)  { return sub(opcode, imm(opcode));      } // SUB  Rd,Rn,#i
int Interpreter::subsLli(uint32_t opcode) { return subs(opcode, lli(opcode));     } // SUBS Rd,Rn,Rm,LSL #i
int Interpreter::subsLlr(uint32_t opcode) { return subs(opcode, llr(opcode)) + 1; } // SUBS Rd,Rn,Rm,LSL Rs
int Interpreter::subsLri(uint32_t opcode) { return subs(opcode, lri(opcode));     } // SUBS Rd,Rn,Rm,LSR #i
int Interpreter::subsLrr(uint32_t opcode) { return subs(opcode, lrr(opcode)) + 1; } // SUBS Rd,Rn,Rm,LSR Rs
int Interpreter::subsAri(uint32_t opcode) { return subs(opcode, ari(opcode));     } // SUBS Rd,Rn,Rm,ASR #i
int Interpreter::subsArr(uint32_t opcode) { return subs(opcode, arr(opcode)) + 1; } // SUBS Rd,Rn,Rm,ASR Rs
int Interpreter::subsRri(uint32_t opcode) { return subs(opcode, rri(opcode));     } // SUBS Rd,Rn,Rm,ROR #i
int Interpreter::subsRrr(uint32_t opcode) { return subs(opcode, rrr(opcode)) + 1; } // SUBS Rd,Rn,Rm,ROR Rs
int Interpreter::subsImm(uint32_t opcode) { return subs(opcode, imm(opcode));     } // SUBS Rd,Rn,#i

int Interpreter::rsbLli(uint32_t opcode)  { return rsb(opcode, lli(opcode));      } // RSB  Rd,Rn,Rm,LSL #i
int Interpreter::rsbLlr(uint32_t opcode)  { return rsb(opcode, llr(opcode)) + 1;  } // RSB  Rd,Rn,Rm,LSL Rs
int Interpreter::rsbLri(uint32_t opcode)  { return rsb(opcode, lri(opcode));      } // RSB  Rd,Rn,Rm,LSR #i
int Interpreter::rsbLrr(uint32_t opcode)  { return rsb(opcode, lrr(opcode)) + 1;  } // RSB  Rd,Rn,Rm,LSR Rs
int Interpreter::rsbAri(uint32_t opcode)  { return rsb(opcode, ari(opcode));      } // RSB  Rd,Rn,Rm,ASR #i
int Interpreter::rsbArr(uint32_t opcode)  { return rsb(opcode, arr(opcode)) + 1;  } // RSB  Rd,Rn,Rm,ASR Rs
int Interpreter::rsbRri(uint32_t opcode)  { return rsb(opcode, rri(opcode));      } // RSB  Rd,Rn,Rm,ROR #i
int Interpreter::rsbRrr(uint32_t opcode)  { return rsb(opcode, rrr(opcode)) + 1;  } // RSB  Rd,Rn,Rm,ROR Rs
int Interpreter::rsbImm(uint32_t opcode)  { return rsb(opcode, imm(opcode));      } // RSB  Rd,Rn,#i
int Interpreter::rsbsLli(uint32_t opcode) { return rsbs(opcode, lli(opcode));     } // RSBS Rd,Rn,Rm,LSL #i
int Interpreter::rsbsLlr(uint32_t opcode) { return rsbs(opcode, llr(opcode)) + 1; } // RSBS Rd,Rn,Rm,LSL Rs
int Interpreter::rsbsLri(uint32_t opcode) { return rsbs(opcode, lri(opcode));     } // RSBS Rd,Rn,Rm,LSR #i
int Interpreter::rsbsLrr(uint32_t opcode) { return rsbs(opcode, lrr(opcode)) + 1; } // RSBS Rd,Rn,Rm,LSR Rs
int Interpreter::rsbsAri(uint32_t opcode) { return rsbs(opcode, ari(opcode));     } // RSBS Rd,Rn,Rm,ASR #i
int Interpreter::rsbsArr(uint32_t opcode) { return rsbs(opcode, arr(opcode)) + 1; } // RSBS Rd,Rn,Rm,ASR Rs
int Interpreter::rsbsRri(uint32_t opcode) { return rsbs(opcode, rri(opcode));     } // RSBS Rd,Rn,Rm,ROR #i
int Interpreter::rsbsRrr(uint32_t opcode) { return rsbs(opcode, rrr(opcode)) + 1; } // RSBS Rd,Rn,Rm,ROR Rs
int Interpreter::rsbsImm(uint32_t opcode) { return rsbs(opcode, imm(opcode));     } // RSBS Rd,Rn,#i

int Interpreter::addLli(uint32_t opcode)  { return add(opcode, lli(opcode));      } // ADD  Rd,Rn,Rm,LSL #i
int Interpreter::addLlr(uint32_t opcode)  { return add(opcode, llr(opcode)) + 1;  } // ADD  Rd,Rn,Rm,LSL Rs
int Interpreter::addLri(uint32_t opcode)  { return add(opcode, lri(opcode));      } // ADD  Rd,Rn,Rm,LSR #i
int Interpreter::addLrr(uint32_t opcode)  { return add(opcode, lrr(opcode)) + 1;  } // ADD  Rd,Rn,Rm,LSR Rs
int Interpreter::addAri(uint32_t opcode)  { return add(opcode, ari(opcode));      } // ADD  Rd,Rn,Rm,ASR #i
int Interpreter::addArr(uint32_t opcode)  { return add(opcode, arr(opcode)) + 1;  } // ADD  Rd,Rn,Rm,ASR Rs
int Interpreter::addRri(uint32_t opcode)  { return add(opcode, rri(opcode));      } // ADD  Rd,Rn,Rm,ROR #i
int Interpreter::addRrr(uint32_t opcode)  { return add(opcode, rrr(opcode)) + 1;  } // ADD  Rd,Rn,Rm,ROR Rs
int Interpreter::addImm(uint32_t opcode)  { return add(opcode, imm(opcode));      } // ADD  Rd,Rn,#i
int Interpreter::addsLli(uint32_t opcode) { return adds(opcode, lli(opcode));     } // ADDS Rd,Rn,Rm,LSL #i
int Interpreter::addsLlr(uint32_t opcode) { return adds(opcode, llr(opcode)) + 1; } // ADDS Rd,Rn,Rm,LSL Rs
int Interpreter::addsLri(uint32_t opcode) { return adds(opcode, lri(opcode));     } // ADDS Rd,Rn,Rm,LSR #i
int Interpreter::addsLrr(uint32_t opcode) { return adds(opcode, lrr(opcode)) + 1; } // ADDS Rd,Rn,Rm,LSR Rs
int Interpreter::addsAri(uint32_t opcode) { return adds(opcode, ari(opcode));     } // ADDS Rd,Rn,Rm,ASR #i
int Interpreter::addsArr(uint32_t opcode) { return adds(opcode, arr(opcode)) + 1; } // ADDS Rd,Rn,Rm,ASR Rs
int Interpreter::addsRri(uint32_t opcode) { return adds(opcode, rri(opcode));     } // ADDS Rd,Rn,Rm,ROR #i
int Interpreter::addsRrr(uint32_t opcode) { return adds(opcode, rrr(opcode)) + 1; } // ADDS Rd,Rn,Rm,ROR Rs
int Interpreter::addsImm(uint32_t opcode) { return adds(opcode, imm(opcode));     } // ADDS Rd,Rn,#i

int Interpreter::adcLli(uint32_t opcode)  { return adc(opcode, lli(opcode));      } // ADC  Rd,Rn,Rm,LSL #i
int Interpreter::adcLlr(uint32_t opcode)  { return adc(opcode, llr(opcode)) + 1;  } // ADC  Rd,Rn,Rm,LSL Rs
int Interpreter::adcLri(uint32_t opcode)  { return adc(opcode, lri(opcode));      } // ADC  Rd,Rn,Rm,LSR #i
int Interpreter::adcLrr(uint32_t opcode)  { return adc(opcode, lrr(opcode)) + 1;  } // ADC  Rd,Rn,Rm,LSR Rs
int Interpreter::adcAri(uint32_t opcode)  { return adc(opcode, ari(opcode));      } // ADC  Rd,Rn,Rm,ASR #i
int Interpreter::adcArr(uint32_t opcode)  { return adc(opcode, arr(opcode)) + 1;  } // ADC  Rd,Rn,Rm,ASR Rs
int Interpreter::adcRri(uint32_t opcode)  { return adc(opcode, rri(opcode));      } // ADC  Rd,Rn,Rm,ROR #i
int Interpreter::adcRrr(uint32_t opcode)  { return adc(opcode, rrr(opcode)) + 1;  } // ADC  Rd,Rn,Rm,ROR Rs
int Interpreter::adcImm(uint32_t opcode)  { return adc(opcode, imm(opcode));      } // ADC  Rd,Rn,#i
int Interpreter::adcsLli(uint32_t opcode) { return adcs(opcode, lli(opcode));     } // ADCS Rd,Rn,Rm,LSL #i
int Interpreter::adcsLlr(uint32_t opcode) { return adcs(opcode, llr(opcode)) + 1; } // ADCS Rd,Rn,Rm,LSL Rs
int Interpreter::adcsLri(uint32_t opcode) { return adcs(opcode, lri(opcode));     } // ADCS Rd,Rn,Rm,LSR #i
int Interpreter::adcsLrr(uint32_t opcode) { return adcs(opcode, lrr(opcode)) + 1; } // ADCS Rd,Rn,Rm,LSR Rs
int Interpreter::adcsAri(uint32_t opcode) { return adcs(opcode, ari(opcode));     } // ADCS Rd,Rn,Rm,ASR #i
int Interpreter::adcsArr(uint32_t opcode) { return adcs(opcode, arr(opcode)) + 1; } // ADCS Rd,Rn,Rm,ASR Rs
int Interpreter::adcsRri(uint32_t opcode) { return adcs(opcode, rri(opcode));     } // ADCS Rd,Rn,Rm,ROR #i
int Interpreter::adcsRrr(uint32_t opcode) { return adcs(opcode, rrr(opcode)) + 1; } // ADCS Rd,Rn,Rm,ROR Rs
int Interpreter::adcsImm(uint32_t opcode) { return adcs(opcode, imm(opcode));     } // ADCS Rd,Rn,#i

int Interpreter::sbcLli(uint32_t opcode)  { return sbc(opcode, lli(opcode));      } // SBC  Rd,Rn,Rm,LSL #i
int Interpreter::sbcLlr(uint32_t opcode)  { return sbc(opcode, llr(opcode)) + 1;  } // SBC  Rd,Rn,Rm,LSL Rs
int Interpreter::sbcLri(uint32_t opcode)  { return sbc(opcode, lri(opcode));      } // SBC  Rd,Rn,Rm,LSR #i
int Interpreter::sbcLrr(uint32_t opcode)  { return sbc(opcode, lrr(opcode)) + 1;  } // SBC  Rd,Rn,Rm,LSR Rs
int Interpreter::sbcAri(uint32_t opcode)  { return sbc(opcode, ari(opcode));      } // SBC  Rd,Rn,Rm,ASR #i
int Interpreter::sbcArr(uint32_t opcode)  { return sbc(opcode, arr(opcode)) + 1;  } // SBC  Rd,Rn,Rm,ASR Rs
int Interpreter::sbcRri(uint32_t opcode)  { return sbc(opcode, rri(opcode));      } // SBC  Rd,Rn,Rm,ROR #i
int Interpreter::sbcRrr(uint32_t opcode)  { return sbc(opcode, rrr(opcode)) + 1;  } // SBC  Rd,Rn,Rm,ROR Rs
int Interpreter::sbcImm(uint32_t opcode)  { return sbc(opcode, imm(opcode));      } // SBC  Rd,Rn,#i
int Interpreter::sbcsLli(uint32_t opcode) { return sbcs(opcode, lli(opcode));     } // SBCS Rd,Rn,Rm,LSL #i
int Interpreter::sbcsLlr(uint32_t opcode) { return sbcs(opcode, llr(opcode)) + 1; } // SBCS Rd,Rn,Rm,LSL Rs
int Interpreter::sbcsLri(uint32_t opcode) { return sbcs(opcode, lri(opcode));     } // SBCS Rd,Rn,Rm,LSR #i
int Interpreter::sbcsLrr(uint32_t opcode) { return sbcs(opcode, lrr(opcode)) + 1; } // SBCS Rd,Rn,Rm,LSR Rs
int Interpreter::sbcsAri(uint32_t opcode) { return sbcs(opcode, ari(opcode));     } // SBCS Rd,Rn,Rm,ASR #i
int Interpreter::sbcsArr(uint32_t opcode) { return sbcs(opcode, arr(opcode)) + 1; } // SBCS Rd,Rn,Rm,ASR Rs
int Interpreter::sbcsRri(uint32_t opcode) { return sbcs(opcode, rri(opcode));     } // SBCS Rd,Rn,Rm,ROR #i
int Interpreter::sbcsRrr(uint32_t opcode) { return sbcs(opcode, rrr(opcode)) + 1; } // SBCS Rd,Rn,Rm,ROR Rs
int Interpreter::sbcsImm(uint32_t opcode) { return sbcs(opcode, imm(opcode));     } // SBCS Rd,Rn,#i

int Interpreter::rscLli(uint32_t opcode)  { return rsc(opcode, lli(opcode));      } // RSC  Rd,Rn,Rm,LSL #i
int Interpreter::rscLlr(uint32_t opcode)  { return rsc(opcode, llr(opcode)) + 1;  } // RSC  Rd,Rn,Rm,LSL Rs
int Interpreter::rscLri(uint32_t opcode)  { return rsc(opcode, lri(opcode));      } // RSC  Rd,Rn,Rm,LSR #i
int Interpreter::rscLrr(uint32_t opcode)  { return rsc(opcode, lrr(opcode)) + 1;  } // RSC  Rd,Rn,Rm,LSR Rs
int Interpreter::rscAri(uint32_t opcode)  { return rsc(opcode, ari(opcode));      } // RSC  Rd,Rn,Rm,ASR #i
int Interpreter::rscArr(uint32_t opcode)  { return rsc(opcode, arr(opcode)) + 1;  } // RSC  Rd,Rn,Rm,ASR Rs
int Interpreter::rscRri(uint32_t opcode)  { return rsc(opcode, rri(opcode));      } // RSC  Rd,Rn,Rm,ROR #i
int Interpreter::rscRrr(uint32_t opcode)  { return rsc(opcode, rrr(opcode)) + 1;  } // RSC  Rd,Rn,Rm,ROR Rs
int Interpreter::rscImm(uint32_t opcode)  { return rsc(opcode, imm(opcode));      } // RSC  Rd,Rn,#i
int Interpreter::rscsLli(uint32_t opcode) { return rscs(opcode, lli(opcode));     } // RSCS Rd,Rn,Rm,LSL #i
int Interpreter::rscsLlr(uint32_t opcode) { return rscs(opcode, llr(opcode)) + 1; } // RSCS Rd,Rn,Rm,LSL Rs
int Interpreter::rscsLri(uint32_t opcode) { return rscs(opcode, lri(opcode));     } // RSCS Rd,Rn,Rm,LSR #i
int Interpreter::rscsLrr(uint32_t opcode) { return rscs(opcode, lrr(opcode)) + 1; } // RSCS Rd,Rn,Rm,LSR Rs
int Interpreter::rscsAri(uint32_t opcode) { return rscs(opcode, ari(opcode));     } // RSCS Rd,Rn,Rm,ASR #i
int Interpreter::rscsArr(uint32_t opcode) { return rscs(opcode, arr(opcode)) + 1; } // RSCS Rd,Rn,Rm,ASR Rs
int Interpreter::rscsRri(uint32_t opcode) { return rscs(opcode, rri(opcode));     } // RSCS Rd,Rn,Rm,ROR #i
int Interpreter::rscsRrr(uint32_t opcode) { return rscs(opcode, rrr(opcode)) + 1; } // RSCS Rd,Rn,Rm,ROR Rs
int Interpreter::rscsImm(uint32_t opcode) { return rscs(opcode, imm(opcode));     } // RSCS Rd,Rn,#i

int Interpreter::tstLli(uint32_t opcode) { return tst(opcode, lliS(opcode));     } // TST Rn,Rm,LSL #i
int Interpreter::tstLlr(uint32_t opcode) { return tst(opcode, llrS(opcode)) + 1; } // TST Rn,Rm,LSL Rs
int Interpreter::tstLri(uint32_t opcode) { return tst(opcode, lriS(opcode));     } // TST Rn,Rm,LSR #i
int Interpreter::tstLrr(uint32_t opcode) { return tst(opcode, lrrS(opcode)) + 1; } // TST Rn,Rm,LSR Rs
int Interpreter::tstAri(uint32_t opcode) { return tst(opcode, ariS(opcode));     } // TST Rn,Rm,ASR #i
int Interpreter::tstArr(uint32_t opcode) { return tst(opcode, arrS(opcode)) + 1; } // TST Rn,Rm,ASR Rs
int Interpreter::tstRri(uint32_t opcode) { return tst(opcode, rriS(opcode));     } // TST Rn,Rm,ROR #i
int Interpreter::tstRrr(uint32_t opcode) { return tst(opcode, rrrS(opcode)) + 1; } // TST Rn,Rm,ROR Rs
int Interpreter::tstImm(uint32_t opcode) { return tst(opcode, immS(opcode));     } // TST Rn,#i
int Interpreter::teqLli(uint32_t opcode) { return teq(opcode, lliS(opcode));     } // TEQ Rn,Rm,LSL #i
int Interpreter::teqLlr(uint32_t opcode) { return teq(opcode, llrS(opcode)) + 1; } // TEQ Rn,Rm,LSL Rs
int Interpreter::teqLri(uint32_t opcode) { return teq(opcode, lriS(opcode));     } // TEQ Rn,Rm,LSR #i
int Interpreter::teqLrr(uint32_t opcode) { return teq(opcode, lrrS(opcode)) + 1; } // TEQ Rn,Rm,LSR Rs
int Interpreter::teqAri(uint32_t opcode) { return teq(opcode, ariS(opcode));     } // TEQ Rn,Rm,ASR #i
int Interpreter::teqArr(uint32_t opcode) { return teq(opcode, arrS(opcode)) + 1; } // TEQ Rn,Rm,ASR Rs
int Interpreter::teqRri(uint32_t opcode) { return teq(opcode, rriS(opcode));     } // TEQ Rn,Rm,ROR #i
int Interpreter::teqRrr(uint32_t opcode) { return teq(opcode, rrrS(opcode)) + 1; } // TEQ Rn,Rm,ROR Rs
int Interpreter::teqImm(uint32_t opcode) { return teq(opcode, immS(opcode));     } // TEQ Rn,#i

int Interpreter::cmpLli(uint32_t opcode) { return cmp(opcode, lliS(opcode));     } // CMP Rn,Rm,LSL #i
int Interpreter::cmpLlr(uint32_t opcode) { return cmp(opcode, llrS(opcode)) + 1; } // CMP Rn,Rm,LSL Rs
int Interpreter::cmpLri(uint32_t opcode) { return cmp(opcode, lriS(opcode));     } // CMP Rn,Rm,LSR #i
int Interpreter::cmpLrr(uint32_t opcode) { return cmp(opcode, lrrS(opcode)) + 1; } // CMP Rn,Rm,LSR Rs
int Interpreter::cmpAri(uint32_t opcode) { return cmp(opcode, ariS(opcode));     } // CMP Rn,Rm,ASR #i
int Interpreter::cmpArr(uint32_t opcode) { return cmp(opcode, arrS(opcode)) + 1; } // CMP Rn,Rm,ASR Rs
int Interpreter::cmpRri(uint32_t opcode) { return cmp(opcode, rriS(opcode));     } // CMP Rn,Rm,ROR #i
int Interpreter::cmpRrr(uint32_t opcode) { return cmp(opcode, rrrS(opcode)) + 1; } // CMP Rn,Rm,ROR Rs
int Interpreter::cmpImm(uint32_t opcode) { return cmp(opcode, immS(opcode));     } // CMP Rn,#i
int Interpreter::cmnLli(uint32_t opcode) { return cmn(opcode, lliS(opcode));     } // CMN Rn,Rm,LSL #i
int Interpreter::cmnLlr(uint32_t opcode) { return cmn(opcode, llrS(opcode)) + 1; } // CMN Rn,Rm,LSL Rs
int Interpreter::cmnLri(uint32_t opcode) { return cmn(opcode, lriS(opcode));     } // CMN Rn,Rm,LSR #i
int Interpreter::cmnLrr(uint32_t opcode) { return cmn(opcode, lrrS(opcode)) + 1; } // CMN Rn,Rm,LSR Rs
int Interpreter::cmnAri(uint32_t opcode) { return cmn(opcode, ariS(opcode));     } // CMN Rn,Rm,ASR #i
int Interpreter::cmnArr(uint32_t opcode) { return cmn(opcode, arrS(opcode)) + 1; } // CMN Rn,Rm,ASR Rs
int Interpreter::cmnRri(uint32_t opcode) { return cmn(opcode, rriS(opcode));     } // CMN Rn,Rm,ROR #i
int Interpreter::cmnRrr(uint32_t opcode) { return cmn(opcode, rrrS(opcode)) + 1; } // CMN Rn,Rm,ROR Rs
int Interpreter::cmnImm(uint32_t opcode) { return cmn(opcode, immS(opcode));     } // CMN Rn,#i

int Interpreter::orrLli(uint32_t opcode)  { return orr(opcode, lli(opcode));       } // ORR  Rd,Rn,Rm,LSL #i
int Interpreter::orrLlr(uint32_t opcode)  { return orr(opcode, llr(opcode)) + 1;   } // ORR  Rd,Rn,Rm,LSL Rs
int Interpreter::orrLri(uint32_t opcode)  { return orr(opcode, lri(opcode));       } // ORR  Rd,Rn,Rm,LSR #i
int Interpreter::orrLrr(uint32_t opcode)  { return orr(opcode, lrr(opcode)) + 1;   } // ORR  Rd,Rn,Rm,LSR Rs
int Interpreter::orrAri(uint32_t opcode)  { return orr(opcode, ari(opcode));       } // ORR  Rd,Rn,Rm,ASR #i
int Interpreter::orrArr(uint32_t opcode)  { return orr(opcode, arr(opcode)) + 1;   } // ORR  Rd,Rn,Rm,ASR Rs
int Interpreter::orrRri(uint32_t opcode)  { return orr(opcode, rri(opcode));       } // ORR  Rd,Rn,Rm,ROR #i
int Interpreter::orrRrr(uint32_t opcode)  { return orr(opcode, rrr(opcode)) + 1;   } // ORR  Rd,Rn,Rm,ROR Rs
int Interpreter::orrImm(uint32_t opcode)  { return orr(opcode, imm(opcode));       } // ORR  Rd,Rn,#i
int Interpreter::orrsLli(uint32_t opcode) { return orrs(opcode, lliS(opcode));     } // ORRS Rd,Rn,Rm,LSL #i
int Interpreter::orrsLlr(uint32_t opcode) { return orrs(opcode, llrS(opcode)) + 1; } // ORRS Rd,Rn,Rm,LSL Rs
int Interpreter::orrsLri(uint32_t opcode) { return orrs(opcode, lriS(opcode));     } // ORRS Rd,Rn,Rm,LSR #i
int Interpreter::orrsLrr(uint32_t opcode) { return orrs(opcode, lrrS(opcode)) + 1; } // ORRS Rd,Rn,Rm,LSR Rs
int Interpreter::orrsAri(uint32_t opcode) { return orrs(opcode, ariS(opcode));     } // ORRS Rd,Rn,Rm,ASR #i
int Interpreter::orrsArr(uint32_t opcode) { return orrs(opcode, arrS(opcode)) + 1; } // ORRS Rd,Rn,Rm,ASR Rs
int Interpreter::orrsRri(uint32_t opcode) { return orrs(opcode, rriS(opcode));     } // ORRS Rd,Rn,Rm,ROR #i
int Interpreter::orrsRrr(uint32_t opcode) { return orrs(opcode, rrrS(opcode)) + 1; } // ORRS Rd,Rn,Rm,ROR Rs
int Interpreter::orrsImm(uint32_t opcode) { return orrs(opcode, immS(opcode));     } // ORRS Rd,Rn,#i

int Interpreter::movLli(uint32_t opcode)  { return mov(opcode, lli(opcode));       } // MOV  Rd,Rm,LSL #i
int Interpreter::movLlr(uint32_t opcode)  { return mov(opcode, llr(opcode)) + 1;   } // MOV  Rd,Rm,LSL Rs
int Interpreter::movLri(uint32_t opcode)  { return mov(opcode, lri(opcode));       } // MOV  Rd,Rm,LSR #i
int Interpreter::movLrr(uint32_t opcode)  { return mov(opcode, lrr(opcode)) + 1;   } // MOV  Rd,Rm,LSR Rs
int Interpreter::movAri(uint32_t opcode)  { return mov(opcode, ari(opcode));       } // MOV  Rd,Rm,ASR #i
int Interpreter::movArr(uint32_t opcode)  { return mov(opcode, arr(opcode)) + 1;   } // MOV  Rd,Rm,ASR Rs
int Interpreter::movRri(uint32_t opcode)  { return mov(opcode, rri(opcode));       } // MOV  Rd,Rm,ROR #i
int Interpreter::movRrr(uint32_t opcode)  { return mov(opcode, rrr(opcode)) + 1;   } // MOV  Rd,Rm,ROR Rs
int Interpreter::movImm(uint32_t opcode)  { return mov(opcode, imm(opcode));       } // MOV  Rd,#i
int Interpreter::movsLli(uint32_t opcode) { return movs(opcode, lliS(opcode));     } // MOVS Rd,Rm,LSL #i
int Interpreter::movsLlr(uint32_t opcode) { return movs(opcode, llrS(opcode)) + 1; } // MOVS Rd,Rm,LSL Rs
int Interpreter::movsLri(uint32_t opcode) { return movs(opcode, lriS(opcode));     } // MOVS Rd,Rm,LSR #i
int Interpreter::movsLrr(uint32_t opcode) { return movs(opcode, lrrS(opcode)) + 1; } // MOVS Rd,Rm,LSR Rs
int Interpreter::movsAri(uint32_t opcode) { return movs(opcode, ariS(opcode));     } // MOVS Rd,Rm,ASR #i
int Interpreter::movsArr(uint32_t opcode) { return movs(opcode, arrS(opcode)) + 1; } // MOVS Rd,Rm,ASR Rs
int Interpreter::movsRri(uint32_t opcode) { return movs(opcode, rriS(opcode));     } // MOVS Rd,Rm,ROR #i
int Interpreter::movsRrr(uint32_t opcode) { return movs(opcode, rrrS(opcode)) + 1; } // MOVS Rd,Rm,ROR Rs
int Interpreter::movsImm(uint32_t opcode) { return movs(opcode, immS(opcode));     } // MOVS Rd,#i

int Interpreter::bicLli(uint32_t opcode)  { return bic(opcode, lli(opcode));       } // BIC  Rd,Rn,Rm,LSL #i
int Interpreter::bicLlr(uint32_t opcode)  { return bic(opcode, llr(opcode)) + 1;   } // BIC  Rd,Rn,Rm,LSL Rs
int Interpreter::bicLri(uint32_t opcode)  { return bic(opcode, lri(opcode));       } // BIC  Rd,Rn,Rm,LSR #i
int Interpreter::bicLrr(uint32_t opcode)  { return bic(opcode, lrr(opcode)) + 1;   } // BIC  Rd,Rn,Rm,LSR Rs
int Interpreter::bicAri(uint32_t opcode)  { return bic(opcode, ari(opcode));       } // BIC  Rd,Rn,Rm,ASR #i
int Interpreter::bicArr(uint32_t opcode)  { return bic(opcode, arr(opcode)) + 1;   } // BIC  Rd,Rn,Rm,ASR Rs
int Interpreter::bicRri(uint32_t opcode)  { return bic(opcode, rri(opcode));       } // BIC  Rd,Rn,Rm,ROR #i
int Interpreter::bicRrr(uint32_t opcode)  { return bic(opcode, rrr(opcode)) + 1;   } // BIC  Rd,Rn,Rm,ROR Rs
int Interpreter::bicImm(uint32_t opcode)  { return bic(opcode, imm(opcode));       } // BIC  Rd,Rn,#i
int Interpreter::bicsLli(uint32_t opcode) { return bics(opcode, lliS(opcode));     } // BICS Rd,Rn,Rm,LSL #i
int Interpreter::bicsLlr(uint32_t opcode) { return bics(opcode, llrS(opcode)) + 1; } // BICS Rd,Rn,Rm,LSL Rs
int Interpreter::bicsLri(uint32_t opcode) { return bics(opcode, lriS(opcode));     } // BICS Rd,Rn,Rm,LSR #i
int Interpreter::bicsLrr(uint32_t opcode) { return bics(opcode, lrrS(opcode)) + 1; } // BICS Rd,Rn,Rm,LSR Rs
int Interpreter::bicsAri(uint32_t opcode) { return bics(opcode, ariS(opcode));     } // BICS Rd,Rn,Rm,ASR #i
int Interpreter::bicsArr(uint32_t opcode) { return bics(opcode, arrS(opcode)) + 1; } // BICS Rd,Rn,Rm,ASR Rs
int Interpreter::bicsRri(uint32_t opcode) { return bics(opcode, rriS(opcode));     } // BICS Rd,Rn,Rm,ROR #i
int Interpreter::bicsRrr(uint32_t opcode) { return bics(opcode, rrrS(opcode)) + 1; } // BICS Rd,Rn,Rm,ROR Rs
int Interpreter::bicsImm(uint32_t opcode) { return bics(opcode, immS(opcode));     } // BICS Rd,Rn,#i

int Interpreter::mvnLli(uint32_t opcode)  { return mvn(opcode, lli(opcode));       } // MVN  Rd,Rm,LSL #i
int Interpreter::mvnLlr(uint32_t opcode)  { return mvn(opcode, llr(opcode)) + 1;   } // MVN  Rd,Rm,LSL Rs
int Interpreter::mvnLri(uint32_t opcode)  { return mvn(opcode, lri(opcode));       } // MVN  Rd,Rm,LSR #i
int Interpreter::mvnLrr(uint32_t opcode)  { return mvn(opcode, lrr(opcode)) + 1;   } // MVN  Rd,Rm,LSR Rs
int Interpreter::mvnAri(uint32_t opcode)  { return mvn(opcode, ari(opcode));       } // MVN  Rd,Rm,ASR #i
int Interpreter::mvnArr(uint32_t opcode)  { return mvn(opcode, arr(opcode)) + 1;   } // MVN  Rd,Rm,ASR Rs
int Interpreter::mvnRri(uint32_t opcode)  { return mvn(opcode, rri(opcode));       } // MVN  Rd,Rm,ROR #i
int Interpreter::mvnRrr(uint32_t opcode)  { return mvn(opcode, rrr(opcode)) + 1;   } // MVN  Rd,Rm,ROR Rs
int Interpreter::mvnImm(uint32_t opcode)  { return mvn(opcode, imm(opcode));       } // MVN  Rd,#i
int Interpreter::mvnsLli(uint32_t opcode) { return mvns(opcode, lliS(opcode));     } // MVNS Rd,Rm,LSL #i
int Interpreter::mvnsLlr(uint32_t opcode) { return mvns(opcode, llrS(opcode)) + 1; } // MVNS Rd,Rm,LSL Rs
int Interpreter::mvnsLri(uint32_t opcode) { return mvns(opcode, lriS(opcode));     } // MVNS Rd,Rm,LSR #i
int Interpreter::mvnsLrr(uint32_t opcode) { return mvns(opcode, lrrS(opcode)) + 1; } // MVNS Rd,Rm,LSR Rs
int Interpreter::mvnsAri(uint32_t opcode) { return mvns(opcode, ariS(opcode));     } // MVNS Rd,Rm,ASR #i
int Interpreter::mvnsArr(uint32_t opcode) { return mvns(opcode, arrS(opcode)) + 1; } // MVNS Rd,Rm,ASR Rs
int Interpreter::mvnsRri(uint32_t opcode) { return mvns(opcode, rriS(opcode));     } // MVNS Rd,Rm,ROR #i
int Interpreter::mvnsRrr(uint32_t opcode) { return mvns(opcode, rrrS(opcode)) + 1; } // MVNS Rd,Rm,ROR Rs
int Interpreter::mvnsImm(uint32_t opcode) { return mvns(opcode, immS(opcode));     } // MVNS Rd,#i

int Interpreter::mul(uint32_t opcode) // MUL Rd,Rm,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t op1 = *registers[opcode & 0x0000000F];
    int32_t op2 = *registers[(opcode & 0x00000F00) >> 8];

    // Multiplication
    *op0 = op1 * op2;

    // Calculate timing
    if (cpu == 0) return 2;
    int m; for (m = 1; (op2 < (-1 << (m * 8)) || op2 >= (1 << (m * 8))) && m < 4; m++);
    return m + 1;
}

int Interpreter::mla(uint32_t opcode) // MLA Rd,Rm,Rs,Rn
{
    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t op1 = *registers[opcode & 0x0000000F];
    int32_t op2 = *registers[(opcode & 0x00000F00) >> 8];
    uint32_t op3 = *registers[(opcode & 0x0000F000) >> 12];

    // Multiplication and accumulate
    *op0 = op1 * op2 + op3;

    // Calculate timing
    if (cpu == 0) return 2;
    int m; for (m = 1; (op2 < (-1 << (m * 8)) || op2 >= (1 << (m * 8))) && m < 4; m++);
    return m + 2;
}

int Interpreter::umull(uint32_t opcode) // UMULL RdLo,RdHi,Rm,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t op2 = *registers[opcode & 0x0000000F];
    int32_t op3 = *registers[(opcode & 0x00000F00) >> 8];

    // Unsigned long multiplication
    uint64_t res = (uint64_t)op2 * (uint32_t)op3;
    *op1 = res >> 32;
    *op0 = res;

    // Calculate timing
    if (cpu == 0) return 3;
    int m; for (m = 1; (op3 < (-1 << (m * 8)) || op3 >= (1 << (m * 8))) && m < 4; m++);
    return m + 2;
}

int Interpreter::umlal(uint32_t opcode) // UMLAL RdLo,RdHi,Rm,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t op2 = *registers[opcode & 0x0000000F];
    int32_t op3 = *registers[(opcode & 0x00000F00) >> 8];

    // Unsigned long multiplication and accumulate
    uint64_t res = (uint64_t)op2 * (uint32_t)op3;
    res += ((uint64_t)*op1 << 32) | *op0;
    *op1 = res >> 32;
    *op0 = res;

    // Calculate timing
    if (cpu == 0) return 3;
    int m; for (m = 1; (op3 < (-1 << (m * 8)) || op3 >= (1 << (m * 8))) && m < 4; m++);
    return m + 3;
}

int Interpreter::smull(uint32_t opcode) // SMULL RdLo,RdHi,Rm,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];
    int32_t op2 = *registers[opcode & 0x0000000F];
    int32_t op3 = *registers[(opcode & 0x00000F00) >> 8];

    // Signed long multiplication
    int64_t res = (int64_t)op2 * op3;
    *op1 = res >> 32;
    *op0 = res;

    // Calculate timing
    if (cpu == 0) return 3;
    int m; for (m = 1; (op3 < (-1 << (m * 8)) || op3 >= (1 << (m * 8))) && m < 4; m++);
    return m + 2;
}

int Interpreter::smlal(uint32_t opcode) // SMLAL RdLo,RdHi,Rm,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];
    int32_t op2 = *registers[opcode & 0x0000000F];
    int32_t op3 = *registers[(opcode & 0x00000F00) >> 8];

    // Signed long multiplication and accumulate
    int64_t res = (int64_t)op2 * op3;
    res += ((int64_t)*op1 << 32) | *op0;
    *op1 = res >> 32;
    *op0 = res;

    // Calculate timing
    if (cpu == 0) return 3;
    int m; for (m = 1; (op3 < (-1 << (m * 8)) || op3 >= (1 << (m * 8))) && m < 4; m++);
    return m + 3;
}

int Interpreter::muls(uint32_t opcode) // MULS Rd,Rm,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t op1 = *registers[opcode & 0x0000000F];
    int32_t op2 = *registers[(opcode & 0x00000F00) >> 8];

    // Multiplication
    *op0 = op1 * op2;

    // Set the flags
    cpsr = (cpsr & ~0xC0000000) | (*op0 & BIT(31)) | ((*op0 == 0) << 30);

    // Calculate timing
    if (cpu == 0) return 4;
    int m; for (m = 1; (op2 < (-1 << (m * 8)) || op2 >= (1 << (m * 8))) && m < 4; m++);
    return m + 1;
}

int Interpreter::mlas(uint32_t opcode) // MLAS Rd,Rm,Rs,Rn
{
    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t op1 = *registers[opcode & 0x0000000F];
    int32_t op2 = *registers[(opcode & 0x00000F00) >> 8];
    uint32_t op3 = *registers[(opcode & 0x0000F000) >> 12];

    // Multiplication and accumulate
    *op0 = op1 * op2 + op3;

    // Set the flags
    cpsr = (cpsr & ~0xC0000000) | (*op0 & BIT(31)) | ((*op0 == 0) << 30);

    // Calculate timing
    if (cpu == 0) return 4;
    int m; for (m = 1; (op2 < (-1 << (m * 8)) || op2 >= (1 << (m * 8))) && m < 4; m++);
    return m + 2;
}

int Interpreter::umulls(uint32_t opcode) // UMULLS RdLo,RdHi,Rm,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t op2 = *registers[opcode & 0x0000000F];
    int32_t op3 = *registers[(opcode & 0x00000F00) >> 8];

    // Unsigned long multiplication
    uint64_t res = (uint64_t)op2 * (uint32_t)op3;
    *op1 = res >> 32;
    *op0 = res;

    // Set the flags
    cpsr = (cpsr & ~0xC0000000) | (*op1 & BIT(31)) | ((*op1 == 0) << 30);

    // Calculate timing
    if (cpu == 0) return 5;
    int m; for (m = 1; (op3 < (-1 << (m * 8)) || op3 >= (1 << (m * 8))) && m < 4; m++);
    return m + 2;
}

int Interpreter::umlals(uint32_t opcode) // UMLALS RdLo,RdHi,Rm,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t op2 = *registers[opcode & 0x0000000F];
    int32_t op3 = *registers[(opcode & 0x00000F00) >> 8];

    // Unsigned long multiplication and accumulate
    uint64_t res = (uint64_t)op2 * (uint32_t)op3;
    res += ((uint64_t)*op1 << 32) | *op0;
    *op1 = res >> 32;
    *op0 = res;

    // Set the flags
    cpsr = (cpsr & ~0xC0000000) | (*op1 & BIT(31)) | ((*op1 == 0) << 30);

    // Calculate timing
    if (cpu == 0) return 5;
    int m; for (m = 1; (op3 < (-1 << (m * 8)) || op3 >= (1 << (m * 8))) && m < 4; m++);
    return m + 3;
}

int Interpreter::smulls(uint32_t opcode) // SMULLS RdLo,RdHi,Rm,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];
    int32_t op2 = *registers[opcode & 0x0000000F];
    int32_t op3 = *registers[(opcode & 0x00000F00) >> 8];

    // Signed long multiplication
    int64_t res = (int64_t)op2 * op3;
    *op1 = res >> 32;
    *op0 = res;

    // Set the flags
    cpsr = (cpsr & ~0xC0000000) | (*op1 & BIT(31)) | ((*op1 == 0) << 30);

    // Calculate timing
    if (cpu == 0) return 5;
    int m; for (m = 1; (op3 < (-1 << (m * 8)) || op3 >= (1 << (m * 8))) && m < 4; m++);
    return m + 2;
}

int Interpreter::smlals(uint32_t opcode) // SMLALS RdLo,RdHi,Rm,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];
    int32_t op2 = *registers[opcode & 0x0000000F];
    int32_t op3 = *registers[(opcode & 0x00000F00) >> 8];

    // Signed long multiplication and accumulate
    int64_t res = (int64_t)op2 * op3;
    res += ((int64_t)*op1 << 32) | *op0;
    *op1 = res >> 32;
    *op0 = res;

    // Set the flags
    cpsr = (cpsr & ~0xC0000000) | (*op1 & BIT(31)) | ((*op1 == 0) << 30);

    // Calculate timing
    if (cpu == 0) return 5;
    int m; for (m = 1; (op3 < (-1 << (m * 8)) || op3 >= (1 << (m * 8))) && m < 4; m++);
    return m + 3;
}

int Interpreter::smulbb(uint32_t opcode) // SMULBB Rd,Rm,Rs
{
    if (cpu == 1) return 1; // ARM9 exclusive

    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x000F0000) >> 16];
    int16_t op1 = *registers[opcode & 0x0000000F];
    int16_t op2 = *registers[(opcode & 0x00000F00) >> 8];

    // Signed half-word multiplication
    *op0 = op1 * op2;

    return 1;
}

int Interpreter::smulbt(uint32_t opcode) // SMULBT Rd,Rm,Rs
{
    if (cpu == 1) return 1; // ARM9 exclusive

    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x000F0000) >> 16];
    int16_t op1 = *registers[opcode & 0x0000000F];
    int16_t op2 = *registers[(opcode & 0x00000F00) >> 8] >> 16;

    // Signed half-word multiplication
    *op0 = op1 * op2;

    return 1;
}

int Interpreter::smultb(uint32_t opcode) // SMULTB Rd,Rm,Rs
{
    if (cpu == 1) return 1; // ARM9 exclusive

    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x000F0000) >> 16];
    int16_t op1 = *registers[opcode & 0x0000000F] >> 16;
    int16_t op2 = *registers[(opcode & 0x00000F00) >> 8];

    // Signed half-word multiplication
    *op0 = op1 * op2;

    return 1;
}

int Interpreter::smultt(uint32_t opcode) // SMULTT Rd,Rm,Rs
{
    if (cpu == 1) return 1; // ARM9 exclusive

    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x000F0000) >> 16];
    int16_t op1 = *registers[opcode & 0x0000000F] >> 16;
    int16_t op2 = *registers[(opcode & 0x00000F00) >> 8] >> 16;

    // Signed half-word multiplication
    *op0 = op1 * op2;

    return 1;
}

int Interpreter::smulwb(uint32_t opcode) // SMULWB Rd,Rm,Rs
{
    if (cpu == 1) return 1; // ARM9 exclusive

    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x000F0000) >> 16];
    int32_t op1 = *registers[opcode & 0x0000000F];
    int16_t op2 = *registers[(opcode & 0x00000F00) >> 8];

    // Signed word by half-word multiplication
    *op0 = ((int64_t)op1 * op2) >> 16;

    return 1;
}

int Interpreter::smulwt(uint32_t opcode) // SMULWT Rd,Rm,Rs
{
    if (cpu == 1) return 1; // ARM9 exclusive

    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x000F0000) >> 16];
    int32_t op1 = *registers[opcode & 0x0000000F];
    int16_t op2 = *registers[(opcode & 0x00000F00) >> 8] >> 16;

    // Signed word by half-word multiplication
    *op0 = ((int64_t)op1 * op2) >> 16;

    return 1;
}

int Interpreter::smlabb(uint32_t opcode) // SMLABB Rd,Rm,Rs,Rn
{
    if (cpu == 1) return 1; // ARM9 exclusive

    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x000F0000) >> 16];
    int16_t op1 = *registers[opcode & 0x0000000F];
    int16_t op2 = *registers[(opcode & 0x00000F00) >> 8];
    uint32_t op3 = *registers[(opcode & 0x0000F000) >> 12];

    // Signed half-word multiplication and accumulate
    uint32_t res = op1 * op2;
    *op0 = res + op3;

    // Set the Q flag
    if ((*op0 & BIT(31)) != (res & BIT(31))) cpsr |= BIT(27);

    return 1;
}

int Interpreter::smlabt(uint32_t opcode) // SMLABT Rd,Rm,Rs,Rn
{
    if (cpu == 1) return 1; // ARM9 exclusive

    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x000F0000) >> 16];
    int16_t op1 = *registers[opcode & 0x0000000F];
    int16_t op2 = *registers[(opcode & 0x00000F00) >> 8] >> 16;
    uint32_t op3 = *registers[(opcode & 0x0000F000) >> 12];

    // Signed half-word multiplication and accumulate
    uint32_t res = op1 * op2;
    *op0 = res + op3;

    // Set the Q flag
    if ((*op0 & BIT(31)) != (res & BIT(31))) cpsr |= BIT(27);

    return 1;
}

int Interpreter::smlatb(uint32_t opcode) // SMLATB Rd,Rm,Rs,Rn
{
    if (cpu == 1) return 1; // ARM9 exclusive

    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x000F0000) >> 16];
    int16_t op1 = *registers[opcode & 0x0000000F] >> 16;
    int16_t op2 = *registers[(opcode & 0x00000F00) >> 8];
    uint32_t op3 = *registers[(opcode & 0x0000F000) >> 12];

    // Signed half-word multiplication and accumulate
    uint32_t res = op1 * op2;
    *op0 = res + op3;

    // Set the Q flag
    if ((*op0 & BIT(31)) != (res & BIT(31))) cpsr |= BIT(27);

    return 1;
}

int Interpreter::smlatt(uint32_t opcode) // SMLATT Rd,Rm,Rs,Rn
{
    if (cpu == 1) return 1; // ARM9 exclusive

    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x000F0000) >> 16];
    int16_t op1 = *registers[opcode & 0x0000000F] >> 16;
    int16_t op2 = *registers[(opcode & 0x00000F00) >> 8] >> 16;
    uint32_t op3 = *registers[(opcode & 0x0000F000) >> 12];

    // Signed half-word multiplication and accumulate
    uint32_t res = op1 * op2;
    *op0 = res + op3;

    // Set the Q flag
    if ((*op0 & BIT(31)) != (res & BIT(31))) cpsr |= BIT(27);

    return 1;
}

int Interpreter::smlawb(uint32_t opcode) // SMLAWB Rd,Rm,Rs,Rn
{
    if (cpu == 1) return 1; // ARM9 exclusive

    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x000F0000) >> 16];
    int32_t op1 = *registers[opcode & 0x0000000F];
    int16_t op2 = *registers[(opcode & 0x00000F00) >> 8];
    uint32_t op3 = *registers[(opcode & 0x0000F000) >> 12];

    // Signed word by half-word multiplication and accumulate
    uint32_t res = ((int64_t)op1 * op2) >> 16;
    *op0 = res + op3;

    // Set the Q flag
    if ((*op0 & BIT(31)) != (res & BIT(31))) cpsr |= BIT(27);

    return 1;
}

int Interpreter::smlawt(uint32_t opcode) // SMLAWT Rd,Rm,Rs,Rn
{
    if (cpu == 1) return 1; // ARM9 exclusive

    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x000F0000) >> 16];
    int32_t op1 = *registers[opcode & 0x0000000F];
    int16_t op2 = *registers[(opcode & 0x00000F00) >> 8] >> 16;
    uint32_t op3 = *registers[(opcode & 0x0000F000) >> 12];

    // Signed word by half-word multiplication and accumulate
    uint32_t res = ((int64_t)op1 * op2) >> 16;
    *op0 = res + op3;

    // Set the Q flag
    if ((*op0 & BIT(31)) != (res & BIT(31))) cpsr |= BIT(27);

    return 1;
}

int Interpreter::smlalbb(uint32_t opcode) // SMLALBB RdLo,RdHi,Rm,Rs
{
    if (cpu == 1) return 1; // ARM9 exclusive

    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];
    int16_t op2 = *registers[opcode & 0x0000000F];
    int16_t op3 = *registers[(opcode & 0x00000F00) >> 8];

    // Signed long half-word multiplication and accumulate
    int64_t res = ((int64_t)*op1 << 32) | *op0;
    res += op2 * op3;
    *op1 = res >> 32;
    *op0 = res;

    return 2;
}

int Interpreter::smlalbt(uint32_t opcode) // SMLALBT RdLo,RdHi,Rm,Rs
{
    if (cpu == 1) return 1; // ARM9 exclusive

    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];
    int16_t op2 = *registers[opcode & 0x0000000F];
    int16_t op3 = *registers[(opcode & 0x00000F00) >> 8] >> 16;

    // Signed long half-word multiplication and accumulate
    int64_t res = ((int64_t)*op1 << 32) | *op0;
    res += op2 * op3;
    *op1 = res >> 32;
    *op0 = res;

    return 2;
}

int Interpreter::smlaltb(uint32_t opcode) // SMLALTB RdLo,RdHi,Rm,Rs
{
    if (cpu == 1) return 1; // ARM9 exclusive

    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];
    int16_t op2 = *registers[opcode & 0x0000000F] >> 16;
    int16_t op3 = *registers[(opcode & 0x00000F00) >> 8];

    // Signed long half-word multiplication and accumulate
    int64_t res = ((int64_t)*op1 << 32) | *op0;
    res += op2 * op3;
    *op1 = res >> 32;
    *op0 = res;

    return 2;
}

int Interpreter::smlaltt(uint32_t opcode) // SMLALTT RdLo,RdHi,Rm,Rs
{
    if (cpu == 1) return 1; // ARM9 exclusive

    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];
    int16_t op2 = *registers[opcode & 0x0000000F] >> 16;
    int16_t op3 = *registers[(opcode & 0x00000F00) >> 8] >> 16;

    // Signed long half-word multiplication and accumulate
    int64_t res = ((int64_t)*op1 << 32) | *op0;
    res += op2 * op3;
    *op1 = res >> 32;
    *op0 = res;

    return 2;
}

int Interpreter::qadd(uint32_t opcode) // QADD Rd,Rm,Rn
{
    if (cpu == 1) return 1; // ARM9 exclusive

    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    int32_t op1 = *registers[opcode & 0x0000000F];
    int32_t op2 = *registers[(opcode & 0x000F0000) >> 16];

    // Signed saturated addition
    int64_t res = (int64_t)op1 + op2;
    if (res > 0x7FFFFFFF)
    {
        res = 0x7FFFFFFF;
        cpsr |= BIT(27); // Q
    }
    else if (res < -0x80000000)
    {
        res = -0x80000000;
        cpsr |= BIT(27); // Q
    }

    *op0 = res;

    return 1;
}

int Interpreter::qsub(uint32_t opcode) // QSUB Rd,Rm,Rn
{
    if (cpu == 1) return 1; // ARM9 exclusive

    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    int32_t op1 = *registers[opcode & 0x0000000F];
    int32_t op2 = *registers[(opcode & 0x000F0000) >> 16];

    // Signed saturated subtraction
    int64_t res = (int64_t)op1 - op2;
    if (res > 0x7FFFFFFF)
    {
        res = 0x7FFFFFFF;
        cpsr |= BIT(27); // Q
    }
    else if (res < -0x80000000)
    {
        res = -0x80000000;
        cpsr |= BIT(27); // Q
    }

    *op0 = res;

    return 1;
}

int Interpreter::qdadd(uint32_t opcode) // QDADD Rd,Rm,Rn
{
    if (cpu == 1) return 1; // ARM9 exclusive

    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    int32_t op1 = *registers[opcode & 0x0000000F];
    int32_t op2 = *registers[(opcode & 0x000F0000) >> 16];

    // Signed saturated double and addition
    int64_t res = (int64_t)op2 * 2;
    if (res > 0x7FFFFFFF)
    {
        res = 0x7FFFFFFF;
        cpsr |= BIT(27); // Q
    }
    else if (res < -0x80000000)
    {
        res = -0x80000000;
        cpsr |= BIT(27); // Q
    }

    res += op1;
    if (res > 0x7FFFFFFF)
    {
        res = 0x7FFFFFFF;
        cpsr |= BIT(27); // Q
    }
    else if (res < -0x80000000)
    {
        res = -0x80000000;
        cpsr |= BIT(27); // Q
    }

    *op0 = res;

    return 1;
}

int Interpreter::qdsub(uint32_t opcode) // QDSUB Rd,Rm,Rn
{
    if (cpu == 1) return 1; // ARM9 exclusive

    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    int32_t op1 = *registers[opcode & 0x0000000F];
    int32_t op2 = *registers[(opcode & 0x000F0000) >> 16];

    // Signed saturated double and subtraction
    int64_t res = (int64_t)op2 * 2;
    if (res > 0x7FFFFFFF)
    {
        res = 0x7FFFFFFF;
        cpsr |= BIT(27); // Q
    }
    else if (res < -0x80000000)
    {
        res = -0x80000000;
        cpsr |= BIT(27); // Q
    }

    res -= op1;
    if (res > 0x7FFFFFFF)
    {
        res = 0x7FFFFFFF;
        cpsr |= BIT(27); // Q
    }
    else if (res < -0x80000000)
    {
        res = -0x80000000;
        cpsr |= BIT(27); // Q
    }

    *op0 = res;

    return 1;
}

int Interpreter::clz(uint32_t opcode) // CLZ Rd,Rm
{
    if (cpu == 1) return 1; // ARM9 exclusive

    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[opcode & 0x0000000F];

    // Count leading zeros
    int count = 0;
    while (op1 != 0)
    {
        op1 >>= 1;
        count++;
    }
    *op0 = 32 - count;

    return 1;
}

int Interpreter::addRegT(uint16_t opcode) // ADD Rd,Rs,Rn
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = *registers[(opcode & 0x01C0) >> 6];

    // Addition
    *op0 = op1 + op2;

    // Set the flags
    cpsr = (cpsr & ~0xF0000000) | (*op0 & BIT(31)) | ((*op0 == 0) << 30) | ((op1 > *op0) << 29) |
        (((op2 & BIT(31)) == (op1 & BIT(31)) && (*op0 & BIT(31)) != (op2 & BIT(31))) << 28);

    return 1;
}

int Interpreter::subRegT(uint16_t opcode) // SUB Rd,Rs,Rn
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = *registers[(opcode & 0x01C0) >> 6];

    // Subtraction
    *op0 = op1 - op2;

    // Set the flags
    cpsr = (cpsr & ~0xF0000000) | (*op0 & BIT(31)) | ((*op0 == 0) << 30) | ((op1 >= *op0) << 29) |
        (((op2 & BIT(31)) != (op1 & BIT(31)) && (*op0 & BIT(31)) == (op2 & BIT(31))) << 28);

    return 1;
}

int Interpreter::addHT(uint16_t opcode) // ADD Rd,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[((opcode & 0x0080) >> 4) | (opcode & 0x0007)];
    uint32_t op2 = *registers[(opcode & 0x0078) >> 3];

    // Addition
    *op0 += op2;

    // Handle pipelining
    if (op0 != registers[15]) return 1;
    flushPipeline();
    return 3;
}

int Interpreter::cmpHT(uint16_t opcode) // CMP Rd,Rs
{
    // Decode the operands
    uint32_t op1 = *registers[((opcode & 0x0080) >> 4) | (opcode & 0x0007)];
    uint32_t op2 = *registers[(opcode & 0x0078) >> 3];

    // Compare
    uint32_t res = op1 - op2;

    // Set the flags
    cpsr = (cpsr & ~0xF0000000) | (res & BIT(31)) | ((res == 0) << 30) | ((op1 >= res) << 29) |
        (((op2 & BIT(31)) != (op1 & BIT(31)) && (res & BIT(31)) == (op2 & BIT(31))) << 28);

    return 1;
}

int Interpreter::movHT(uint16_t opcode) // MOV Rd,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[((opcode & 0x0080) >> 4) | (opcode & 0x0007)];
    uint32_t op2 = *registers[(opcode & 0x0078) >> 3];

    // Move
    *op0 = op2;

    // Handle pipelining
    if (op0 != registers[15]) return 1;
    flushPipeline();
    return 3;
}

int Interpreter::addPcT(uint16_t opcode) // ADD Rd,PC,#i
{
    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x0700) >> 8];
    uint32_t op1 = *registers[15] & ~3;
    uint32_t op2 = (opcode & 0x00FF) << 2;

    // Addition
    *op0 = op1 + op2;

    return 1;
}

int Interpreter::addSpT(uint16_t opcode) // ADD Rd,SP,#i
{
    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x0700) >> 8];
    uint32_t op1 = *registers[13];
    uint32_t op2 = (opcode & 0x00FF) << 2;

    // Addition
    *op0 = op1 + op2;

    return 1;
}

int Interpreter::addSpImmT(uint16_t opcode) // ADD SP,#i
{
    // Decode the operands
    uint32_t *op0 = registers[13];
    uint32_t op2 = ((opcode & BIT(7)) ? (0 - (opcode & 0x007F)) : (opcode & 0x007F)) << 2;

    // Addition
    *op0 += op2;

    return 1;
}

int Interpreter::lslImmT(uint16_t opcode) // LSL Rd,Rs,#i
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint8_t op2 = (opcode & 0x07C0) >> 6;

    // Logical shift left
    *op0 = op1 << op2;

    // Set the flags
    cpsr = (cpsr & ~0xC0000000) | (*op0 & BIT(31)) | ((*op0 == 0) << 30);
    if (op2 > 0)
        cpsr = (cpsr & ~BIT(29)) | ((bool)(op1 & BIT(32 - op2)) << 29);

    return 1;
}

int Interpreter::lsrImmT(uint16_t opcode) // LSR Rd,Rs,#i
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint8_t op2 = (opcode & 0x07C0) >> 6;

    // Logical shift right
    // A shift of 0 translates to a shift of 32
    *op0 = op2 ? (op1 >> op2) : 0;

    // Set the flags
    cpsr = (cpsr & ~0xE0000000) | (*op0 & BIT(31)) | ((*op0 == 0) << 30) |
        ((bool)(op1 & BIT(op2 ? (op2 - 1) : 31)) << 29);

    return 1;
}

int Interpreter::asrImmT(uint16_t opcode) // ASR Rd,Rs,#i
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint8_t op2 = (opcode & 0x07C0) >> 6;

    // Arithmetic shift right
    // A shift of 0 translates to a shift of 32
    *op0 = op2 ? ((int32_t)op1 >> op2) : ((op1 & BIT(31)) ? 0xFFFFFFFF : 0);

    // Set the flags
    cpsr = (cpsr & ~0xE0000000) | (*op0 & BIT(31)) | ((*op0 == 0) << 30) |
        ((bool)(op1 & BIT(op2 ? (op2 - 1) : 31)) << 29);

    return 1;
}

int Interpreter::addImm3T(uint16_t opcode) // ADD Rd,Rs,#i
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = (opcode & 0x01C0) >> 6;

    // Addition
    *op0 = op1 + op2;

    // Set the flags
    cpsr = (cpsr & ~0xF0000000) | (*op0 & BIT(31)) | ((*op0 == 0) << 30) | ((op1 > *op0) << 29) |
        (((op2 & BIT(31)) == (op1 & BIT(31)) && (*op0 & BIT(31)) != (op2 & BIT(31))) << 28);

    return 1;
}

int Interpreter::subImm3T(uint16_t opcode) // SUB Rd,Rs,#i
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = (opcode & 0x01C0) >> 6;

    // Subtraction
    *op0 = op1 - op2;

    // Set the flags
    cpsr = (cpsr & ~0xF0000000) | (*op0 & BIT(31)) | ((*op0 == 0) << 30) | ((op1 >= *op0) << 29) |
        (((op2 & BIT(31)) != (op1 & BIT(31)) && (*op0 & BIT(31)) == (op2 & BIT(31))) << 28);

    return 1;
}

int Interpreter::addImm8T(uint16_t opcode) // ADD Rd,#i
{
    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x0700) >> 8];
    uint32_t op1 = *registers[(opcode & 0x0700) >> 8];
    uint32_t op2 = opcode & 0x00FF;

    // Addition
    *op0 += op2;

    // Set the flags
    cpsr = (cpsr & ~0xF0000000) | (*op0 & BIT(31)) | ((*op0 == 0) << 30) | ((op1 > *op0) << 29) |
        (((op2 & BIT(31)) == (op1 & BIT(31)) && (*op0 & BIT(31)) != (op2 & BIT(31))) << 28);

    return 1;
}

int Interpreter::subImm8T(uint16_t opcode) // SUB Rd,#i
{
    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x0700) >> 8];
    uint32_t op1 = *registers[(opcode & 0x0700) >> 8];
    uint32_t op2 = opcode & 0x00FF;

    // Subtraction
    *op0 -= op2;

    // Set the flags
    cpsr = (cpsr & ~0xF0000000) | (*op0 & BIT(31)) | ((*op0 == 0) << 30) | ((op1 >= *op0) << 29) |
        (((op2 & BIT(31)) != (op1 & BIT(31)) && (*op0 & BIT(31)) == (op2 & BIT(31))) << 28);

    return 1;
}


int Interpreter::cmpImm8T(uint16_t opcode) // CMP Rd,#i
{
    // Decode the operands
    uint32_t op1 = *registers[(opcode & 0x0700) >> 8];
    uint32_t op2 = opcode & 0x00FF;

    // Compare
    uint32_t res = op1 - op2;

    // Set the flags
    cpsr = (cpsr & ~0xF0000000) | (res & BIT(31)) | ((res == 0) << 30) | ((op1 >= res) << 29) |
        (((op2 & BIT(31)) != (op1 & BIT(31)) && (res & BIT(31)) == (op2 & BIT(31))) << 28);

    return 1;
}

int Interpreter::movImm8T(uint16_t opcode) // MOV Rd,#i
{
    // Decode the other operand
    uint32_t *op0 = registers[(opcode & 0x0700) >> 8];
    uint32_t op2 = opcode & 0x00FF;

    // Move
    *op0 = op2;

    // Set the flags
    cpsr = (cpsr & ~0xC0000000) | (*op0 & BIT(31)) | ((*op0 == 0) << 30);

    return 1;
}

int Interpreter::lslDpT(uint16_t opcode) // LSL Rd,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[opcode & 0x0007];
    uint8_t op2 = *registers[(opcode & 0x0038) >> 3];

    // Logical shift left
    // Shifts greater than 32 can be wonky on host systems, so they're handled explicitly
    *op0 = (op2 < 32) ? (*op0 << op2) : 0;

    // Set the flags
    cpsr = (cpsr & ~0xC0000000) | (*op0 & BIT(31)) | ((*op0 == 0) << 30);
    if (op2 > 0)
        cpsr = (cpsr & ~BIT(29)) | ((op2 <= 32 && (op1 & BIT(32 - op2))) << 29);

    return 1;
}

int Interpreter::lsrDpT(uint16_t opcode) // LSR Rd,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[opcode & 0x0007];
    uint8_t op2 = *registers[(opcode & 0x0038) >> 3];

    // Logical shift right
    // Shifts greater than 32 can be wonky on host systems, so they're handled explicitly
    *op0 = (op2 < 32) ? (*op0 >> op2) : 0;

    // Set the flags
    cpsr = (cpsr & ~0xC0000000) | (*op0 & BIT(31)) | ((*op0 == 0) << 30);
    if (op2 > 0)
        cpsr = (cpsr & ~BIT(29)) | ((op2 <= 32 && (op1 & BIT(op2 - 1))) << 29);

    return 1;
}

int Interpreter::asrDpT(uint16_t opcode) // ASR Rd,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[opcode & 0x0007];
    uint8_t op2 = *registers[(opcode & 0x0038) >> 3];

    // Arithmetic shift right
    // Shifts greater than 32 can be wonky on host systems, so they're handled explicitly
    *op0 = (op2 < 32) ? ((int32_t)(*op0) >> op2) : ((*op0 & BIT(31)) ? 0xFFFFFFFF : 0);

    // Set the flags
    cpsr = (cpsr & ~0xC0000000) | (*op0 & BIT(31)) | ((*op0 == 0) << 30);
    if (op2 > 0)
        cpsr = (cpsr & ~BIT(29)) | ((bool)(op1 & BIT((op2 <= 32) ? (op2 - 1) : 31)) << 29);

    return 1;
}

int Interpreter::rorDpT(uint16_t opcode) // ROR Rd,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[opcode & 0x0007];
    uint8_t op2 = *registers[(opcode & 0x0038) >> 3];

    // Rotate right
    *op0 = (*op0 << (32 - op2 % 32)) | (*op0 >> (op2 % 32));

    // Set the flags
    cpsr = (cpsr & ~0xC0000000) | (*op0 & BIT(31)) | ((*op0 == 0) << 30);
    if (op2 > 0)
        cpsr = (cpsr & ~BIT(29)) | ((bool)(op1 & BIT((op2 - 1) % 32)) << 29);

    return 1;
}

int Interpreter::andDpT(uint16_t opcode) // AND Rd,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op2 = *registers[(opcode & 0x0038) >> 3];

    // Bitwise and
    *op0 &= op2;

    // Set the flags
    cpsr = (cpsr & ~0xC0000000) | (*op0 & BIT(31)) | ((*op0 == 0) << 30);

    return 1;
}

int Interpreter::eorDpT(uint16_t opcode) // EOR Rd,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op2 = *registers[(opcode & 0x0038) >> 3];

    // Bitwise exclusive or
    *op0 ^= op2;

    // Set the flags
    cpsr = (cpsr & ~0xC0000000) | (*op0 & BIT(31)) | ((*op0 == 0) << 30);

    return 1;
}

int Interpreter::adcDpT(uint16_t opcode) // ADC Rd,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[opcode & 0x0007];
    uint32_t op2 = *registers[(opcode & 0x0038) >> 3];

    // Addition with carry
    *op0 += op2 + ((cpsr & BIT(29)) >> 29);

    // Set the flags
    cpsr = (cpsr & ~0xF0000000) | (*op0 & BIT(31)) | ((*op0 == 0) << 30) |
        ((op1 > *op0 || (op2 == 0xFFFFFFFF && (cpsr & BIT(29)))) << 29) |
        (((op2 & BIT(31)) == (op1 & BIT(31)) && (*op0 & BIT(31)) != (op2 & BIT(31))) << 28);

    return 1;
}

int Interpreter::sbcDpT(uint16_t opcode) // SBC Rd,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[opcode & 0x0007];
    uint32_t op2 = *registers[(opcode & 0x0038) >> 3];

    // Subtraction with carry
    *op0 = op1 - op2 - 1 + ((cpsr & BIT(29)) >> 29);

    // Set the flags
    cpsr = (cpsr & ~0xF0000000) | (*op0 & BIT(31)) | ((*op0 == 0) << 30) |
        ((op1 >= *op0 && (op2 != 0xFFFFFFFF || (cpsr & BIT(29)))) << 29) |
        (((op2 & BIT(31)) != (op1 & BIT(31)) && (*op0 & BIT(31)) == (op2 & BIT(31))) << 28);

    return 1;
}

int Interpreter::tstDpT(uint16_t opcode) // TST Rd,Rs
{
    // Decode the operands
    uint32_t op1 = *registers[opcode & 0x0007];
    uint32_t op2 = *registers[(opcode & 0x0038) >> 3];

    // Test bits
    uint32_t res = op1 & op2;

    // Set the flags
    cpsr = (cpsr & ~0xC0000000) | (res & BIT(31)) | ((res == 0) << 30);

    return 1;
}

int Interpreter::cmpDpT(uint16_t opcode) // CMP Rd,Rs
{
    // Decode the operands
    uint32_t op1 = *registers[opcode & 0x0007];
    uint32_t op2 = *registers[(opcode & 0x0038) >> 3];

    // Compare
    uint32_t res = op1 - op2;

    // Set the flags
    cpsr = (cpsr & ~0xF0000000) | (res & BIT(31)) | ((res == 0) << 30) | ((op1 >= res) << 29) |
        (((op2 & BIT(31)) != (op1 & BIT(31)) && (res & BIT(31)) == (op2 & BIT(31))) << 28);

    return 1;
}

int Interpreter::cmnDpT(uint16_t opcode) // CMN Rd,Rs
{
    // Decode the operands
    uint32_t op1 = *registers[opcode & 0x0007];
    uint32_t op2 = *registers[(opcode & 0x0038) >> 3];

    // Compare negative
    uint32_t res = op1 + op2;

    // Set the flags
    cpsr = (cpsr & ~0xF0000000) | (res & BIT(31)) | ((res == 0) << 30) | ((op1 > res) << 29) |
        (((op2 & BIT(31)) == (op1 & BIT(31)) && (res & BIT(31)) != (op2 & BIT(31))) << 28);

    return 1;
}

int Interpreter::orrDpT(uint16_t opcode) // ORR Rd,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op2 = *registers[(opcode & 0x0038) >> 3];

    // Bitwise or
    *op0 |= op2;

    // Set the flags
    cpsr = (cpsr & ~0xC0000000) | (*op0 & BIT(31)) | ((*op0 == 0) << 30);

    return 1;
}

int Interpreter::bicDpT(uint16_t opcode) // BIC Rd,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op2 = *registers[(opcode & 0x0038) >> 3];

    // Bit clear
    *op0 &= ~op2;

    // Set the flags
    cpsr = (cpsr & ~0xC0000000) | (*op0 & BIT(31)) | ((*op0 == 0) << 30);

    return 1;
}

int Interpreter::mvnDpT(uint16_t opcode) // MVN Rd,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op2 = *registers[(opcode & 0x0038) >> 3];

    // Move negative
    *op0 = ~op2;

    // Set the flags
    cpsr = (cpsr & ~0xC0000000) | (*op0 & BIT(31)) | ((*op0 == 0) << 30);

    return 1;
}

int Interpreter::negDpT(uint16_t opcode) // NEG Rd,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = 0;
    uint32_t op2 = *registers[(opcode & 0x0038) >> 3];

    // Negation
    *op0 = op1 - op2;

    // Set the flags
    cpsr = (cpsr & ~0xF0000000) | (*op0 & BIT(31)) | ((*op0 == 0) << 30) | ((op1 >= *op0) << 29) |
        (((op2 & BIT(31)) != (op1 & BIT(31)) && (*op0 & BIT(31)) == (op2 & BIT(31))) << 28);

    return 1;
}

int Interpreter::mulDpT(uint16_t opcode) // MUL Rd,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    int32_t op2 = *registers[opcode & 0x0007];

    // Multiplication
    *op0 = op1 * op2;

    // Set the flags
    cpsr = (cpsr & ~0xC0000000) | (*op0 & BIT(31)) | ((*op0 == 0) << 30);

    // Calculate timing
    if (cpu == 0) return 4;
    int m; for (m = 1; (op2 < (-1 << (m * 8)) || op2 >= (1 << (m * 8))) && m < 4; m++);
    return m + 1;
}
