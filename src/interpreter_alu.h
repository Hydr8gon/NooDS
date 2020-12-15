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

#ifndef INTERPRETER_ALU
#define INTERPRETER_ALU

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
    {
        if (value & BIT(32 - shift)) cpsr |= BIT(29); else cpsr &= ~BIT(29);
    }

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
    {
        if (shift <= 32 && (value & BIT(32 - shift))) cpsr |= BIT(29); else cpsr &= ~BIT(29);
    }

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
    if (value & BIT(shift ? (shift - 1) : 31)) cpsr |= BIT(29); else cpsr &= ~BIT(29);

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
    {
        if (shift <= 32 && (value & BIT(shift - 1))) cpsr |= BIT(29); else cpsr &= ~BIT(29);
    }

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
    if ((shift == 0 && (value & BIT(31))) || (shift > 0 && (value & BIT(shift - 1))))
        cpsr |= BIT(29); else cpsr &= ~BIT(29);

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
    {
        if ((shift > 32 && (value & BIT(31))) || (shift <= 32 && (value & BIT(shift - 1))))
            cpsr |= BIT(29); else cpsr &= ~BIT(29);
    }

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
    if (value & BIT(shift ? (shift - 1) : 0)) cpsr |= BIT(29); else cpsr &= ~BIT(29);

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
    {
        if (value & BIT((shift - 1) % 32)) cpsr |= BIT(29); else cpsr &= ~BIT(29);
    }

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
    {
        if (value & BIT(shift - 1)) cpsr |= BIT(29); else cpsr &= ~BIT(29);
    }

    // Immediate
    // Can be any 8 bits rotated right by a multiple of 2
    return (value << (32 - shift)) | (value >> shift);
}

FORCE_INLINE void Interpreter::_and(uint32_t opcode, uint32_t op2) // AND Rd,Rn,op2
{
    // Decode the other operands
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Bitwise and
    *op0 = op1 & op2;

    // Handle pipelining
    if (op0 == registers[15])
        *registers[15] = (*registers[15] & ~3) + 4;
}

FORCE_INLINE void Interpreter::eor(uint32_t opcode, uint32_t op2) // EOR Rd,Rn,op2
{
    // Decode the other operands
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Bitwise exclusive or
    *op0 = op1 ^ op2;

    // Handle pipelining
    if (op0 == registers[15])
        *registers[15] = (*registers[15] & ~3) + 4;
}

FORCE_INLINE void Interpreter::sub(uint32_t opcode, uint32_t op2) // SUB Rd,Rn,op2
{
    // Decode the other operands
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Subtraction
    *op0 = op1 - op2;

    // Handle pipelining
    if (op0 == registers[15])
        *registers[15] = (*registers[15] & ~3) + 4;
}

FORCE_INLINE void Interpreter::rsb(uint32_t opcode, uint32_t op2) // RSB Rd,Rn,op2
{
    // Decode the other operands
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Reverse subtraction
    *op0 = op2 - op1;

    // Handle pipelining
    if (op0 == registers[15])
        *registers[15] = (*registers[15] & ~3) + 4;
}

FORCE_INLINE void Interpreter::add(uint32_t opcode, uint32_t op2) // ADD Rd,Rn,op2
{
    // Decode the other operands
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Addition
    *op0 = op1 + op2;

    // Handle pipelining
    if (op0 == registers[15])
        *registers[15] = (*registers[15] & ~3) + 4;
}

FORCE_INLINE void Interpreter::adc(uint32_t opcode, uint32_t op2) // ADC Rd,Rn,op2
{
    // Decode the other operands
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Addition with carry
    *op0 = op1 + op2 + ((cpsr & BIT(29)) >> 29);

    // Handle pipelining
    if (op0 == registers[15])
        *registers[15] = (*registers[15] & ~3) + 4;
}

FORCE_INLINE void Interpreter::sbc(uint32_t opcode, uint32_t op2) // SBC Rd,Rn,op2
{
    // Decode the other operands
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Subtraction with carry
    *op0 = op1 - op2 - 1 + ((cpsr & BIT(29)) >> 29);

    // Handle pipelining
    if (op0 == registers[15])
        *registers[15] = (*registers[15] & ~3) + 4;
}

FORCE_INLINE void Interpreter::rsc(uint32_t opcode, uint32_t op2) // RSC Rd,Rn,op2
{
    // Decode the other operands
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Reverse subtraction with carry
    *op0 = op2 - op1 - 1 + ((cpsr & BIT(29)) >> 29);

    // Handle pipelining
    if (op0 == registers[15])
        *registers[15] = (*registers[15] & ~3) + 4;
}

FORCE_INLINE void Interpreter::tst(uint32_t opcode, uint32_t op2) // TST Rn,op2
{
    // Decode the other operand
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Test bits
    uint32_t res = op1 & op2;

    // Set the flags
    if (res & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (res == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
}

FORCE_INLINE void Interpreter::teq(uint32_t opcode, uint32_t op2) // TEQ Rn,op2
{
    // Decode the other operand
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Test bits
    uint32_t res = op1 ^ op2;

    // Set the flags
    if (res & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (res == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
}

FORCE_INLINE void Interpreter::cmp(uint32_t opcode, uint32_t op2) // CMP Rn,op2
{
    // Decode the other operand
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Compare
    uint32_t res = op1 - op2;

    // Set the flags
    if (res & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (res == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
    if (op1 >= res)    cpsr |= BIT(29); else cpsr &= ~BIT(29);
    if ((op2 & BIT(31)) != (op1 & BIT(31)) && (res & BIT(31)) == (op2 & BIT(31)))
        cpsr |= BIT(28); else cpsr &= ~BIT(28);
}

FORCE_INLINE void Interpreter::cmn(uint32_t opcode, uint32_t op2) // CMN Rn,op2
{
    // Decode the other operand
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Compare negative
    uint32_t res = op1 + op2;

    // Set the flags
    if (res & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (res == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
    if (op1 > res)     cpsr |= BIT(29); else cpsr &= ~BIT(29);
    if ((op2 & BIT(31)) == (op1 & BIT(31)) && (res & BIT(31)) != (op2 & BIT(31)))
        cpsr |= BIT(28); else cpsr &= ~BIT(28);
}

FORCE_INLINE void Interpreter::orr(uint32_t opcode, uint32_t op2) // ORR Rd,Rn,op2
{
    // Decode the other operands
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Bitwise or
    *op0 = op1 | op2;

    // Handle pipelining
    if (op0 == registers[15])
        *registers[15] = (*registers[15] & ~3) + 4;
}

FORCE_INLINE void Interpreter::mov(uint32_t opcode, uint32_t op2) // MOV Rd,op2
{
    // Decode the other operand
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];

    // Move
    *op0 = op2;

    // Handle pipelining
    if (op0 == registers[15])
        *registers[15] = (*registers[15] & ~3) + 4;
}

FORCE_INLINE void Interpreter::bic(uint32_t opcode, uint32_t op2) // BIC Rd,Rn,op2
{
    // Decode the other operands
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Bit clear
    *op0 = op1 & ~op2;

    // Handle pipelining
    if (op0 == registers[15])
        *registers[15] = (*registers[15] & ~3) + 4;
}

FORCE_INLINE void Interpreter::mvn(uint32_t opcode, uint32_t op2) // MVN Rd,op2
{
    // Decode the other operand
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];

    // Move negative
    *op0 = ~op2;

    // Handle pipelining
    if (op0 == registers[15])
        *registers[15] = (*registers[15] & ~3) + 4;
}

FORCE_INLINE void Interpreter::ands(uint32_t opcode, uint32_t op2) // ANDS Rd,Rn,op2
{
    // Decode the other operands
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Bitwise and
    *op0 = op1 & op2;

    // Handle pipelining and mode switching
    if (op0 == registers[15])
    {
        if (spsr)
        {
            cpsr = *spsr;
            setMode(cpsr);
            *registers[15] = (cpsr & BIT(5)) ? ((*registers[15] & ~1) + 2) : ((*registers[15] & ~3) + 4);
            return;
        }

        *registers[15] = (*registers[15] & ~3) + 4;
    }

    // Set the flags
    if (*op0 & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (*op0 == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
}

FORCE_INLINE void Interpreter::eors(uint32_t opcode, uint32_t op2) // EORS Rd,Rn,op2
{
    // Decode the other operands
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Bitwise exclusive or
    *op0 = op1 ^ op2;

    // Handle pipelining and mode switching
    if (op0 == registers[15])
    {
        if (spsr)
        {
            cpsr = *spsr;
            setMode(cpsr);
            *registers[15] = (cpsr & BIT(5)) ? ((*registers[15] & ~1) + 2) : ((*registers[15] & ~3) + 4);
            return;
        }

        *registers[15] = (*registers[15] & ~3) + 4;
    }

    // Set the flags
    if (*op0 & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (*op0 == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
}

FORCE_INLINE void Interpreter::subs(uint32_t opcode, uint32_t op2) // SUBS Rd,Rn,op2
{
    // Decode the other operands
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Subtraction
    *op0 = op1 - op2;

    // Handle pipelining and mode switching
    if (op0 == registers[15])
    {
        if (spsr)
        {
            cpsr = *spsr;
            setMode(cpsr);
            *registers[15] = (cpsr & BIT(5)) ? ((*registers[15] & ~1) + 2) : ((*registers[15] & ~3) + 4);
            return;
        }

        *registers[15] = (*registers[15] & ~3) + 4;
    }

    // Set the flags
    if (*op0 & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (*op0 == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
    if (op1 >= *op0)    cpsr |= BIT(29); else cpsr &= ~BIT(29);
    if ((op2 & BIT(31)) != (op1 & BIT(31)) && (*op0 & BIT(31)) == (op2 & BIT(31)))
        cpsr |= BIT(28); else cpsr &= ~BIT(28);
}

FORCE_INLINE void Interpreter::rsbs(uint32_t opcode, uint32_t op2) // RSBS Rd,Rn,op2
{
    // Decode the other operands
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Reverse subtraction
    *op0 = op2 - op1;

    // Handle pipelining and mode switching
    if (op0 == registers[15])
    {
        if (spsr)
        {
            cpsr = *spsr;
            setMode(cpsr);
            *registers[15] = (cpsr & BIT(5)) ? ((*registers[15] & ~1) + 2) : ((*registers[15] & ~3) + 4);
            return;
        }

        *registers[15] = (*registers[15] & ~3) + 4;
    }

    // Set the flags
    if (*op0 & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (*op0 == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
    if (op2 >= *op0)    cpsr |= BIT(29); else cpsr &= ~BIT(29);
    if ((op1 & BIT(31)) != (op2 & BIT(31)) && (*op0 & BIT(31)) == (op1 & BIT(31)))
        cpsr |= BIT(28); else cpsr &= ~BIT(28);
}

FORCE_INLINE void Interpreter::adds(uint32_t opcode, uint32_t op2) // ADDS Rd,Rn,op2
{
    // Decode the other operands
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Addition
    *op0 = op1 + op2;

    // Handle pipelining and mode switching
    if (op0 == registers[15])
    {
        if (spsr)
        {
            cpsr = *spsr;
            setMode(cpsr);
            *registers[15] = (cpsr & BIT(5)) ? ((*registers[15] & ~1) + 2) : ((*registers[15] & ~3) + 4);
            return;
        }

        *registers[15] = (*registers[15] & ~3) + 4;
    }

    // Set the flags
    if (*op0 & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (*op0 == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
    if (op1 > *op0)     cpsr |= BIT(29); else cpsr &= ~BIT(29);
    if ((op2 & BIT(31)) == (op1 & BIT(31)) && (*op0 & BIT(31)) != (op2 & BIT(31)))
        cpsr |= BIT(28); else cpsr &= ~BIT(28);
}

FORCE_INLINE void Interpreter::adcs(uint32_t opcode, uint32_t op2) // ADCS Rd,Rn,op2
{
    // Decode the other operands
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Addition with carry
    *op0 = op1 + op2 + ((cpsr & BIT(29)) >> 29);

    // Handle pipelining and mode switching
    if (op0 == registers[15])
    {
        if (spsr)
        {
            cpsr = *spsr;
            setMode(cpsr);
            *registers[15] = (cpsr & BIT(5)) ? ((*registers[15] & ~1) + 2) : ((*registers[15] & ~3) + 4);
            return;
        }

        *registers[15] = (*registers[15] & ~3) + 4;
    }

    // Set the flags
    if (*op0 & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (*op0 == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
    if (op1 > *op0 || (op2 == 0xFFFFFFFF && (cpsr & BIT(29)))) cpsr |= BIT(29); else cpsr &= ~BIT(29);
    if ((op2 & BIT(31)) == (op1 & BIT(31)) && (*op0 & BIT(31)) != (op2 & BIT(31)))
        cpsr |= BIT(28); else cpsr &= ~BIT(28);
}

FORCE_INLINE void Interpreter::sbcs(uint32_t opcode, uint32_t op2) // SBCS Rd,Rn,op2
{
    // Decode the other operands
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Subtraction with carry
    *op0 = op1 - op2 - 1 + ((cpsr & BIT(29)) >> 29);

    // Handle pipelining and mode switching
    if (op0 == registers[15])
    {
        if (spsr)
        {
            cpsr = *spsr;
            setMode(cpsr);
            *registers[15] = (cpsr & BIT(5)) ? ((*registers[15] & ~1) + 2) : ((*registers[15] & ~3) + 4);
            return;
        }

        *registers[15] = (*registers[15] & ~3) + 4;
    }

    // Set the flags
    if (*op0 & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (*op0 == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
    if (op1 >= *op0 && (op2 != 0xFFFFFFFF || (cpsr & BIT(29)))) cpsr |= BIT(29); else cpsr &= ~BIT(29);
    if ((op2 & BIT(31)) != (op1 & BIT(31)) && (*op0 & BIT(31)) == (op2 & BIT(31)))
        cpsr |= BIT(28); else cpsr &= ~BIT(28);
}

FORCE_INLINE void Interpreter::rscs(uint32_t opcode, uint32_t op2) // RSCS Rd,Rn,op2
{
    // Decode the other operands
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Reverse subtraction with carry
    *op0 = op2 - op1 - 1 + ((cpsr & BIT(29)) >> 29);

    // Handle pipelining and mode switching
    if (op0 == registers[15])
    {
        if (spsr)
        {
            cpsr = *spsr;
            setMode(cpsr);
            *registers[15] = (cpsr & BIT(5)) ? ((*registers[15] & ~1) + 2) : ((*registers[15] & ~3) + 4);
            return;
        }

        *registers[15] = (*registers[15] & ~3) + 4;
    }

    // Set the flags
    if (*op0 & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (*op0 == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
    if (op2 >= *op0 && (op1 != 0xFFFFFFFF || (cpsr & BIT(29)))) cpsr |= BIT(29); else cpsr &= ~BIT(29);
    if ((op1 & BIT(31)) != (op2 & BIT(31)) && (*op0 & BIT(31)) == (op1 & BIT(31)))
        cpsr |= BIT(28); else cpsr &= ~BIT(28);
}

FORCE_INLINE void Interpreter::orrs(uint32_t opcode, uint32_t op2) // ORRS Rd,Rn,op2
{
    // Decode the other operands
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Bitwise or
    *op0 = op1 | op2;

    // Handle pipelining and mode switching
    if (op0 == registers[15])
    {
        if (spsr)
        {
            cpsr = *spsr;
            setMode(cpsr);
            *registers[15] = (cpsr & BIT(5)) ? ((*registers[15] & ~1) + 2) : ((*registers[15] & ~3) + 4);
            return;
        }

        *registers[15] = (*registers[15] & ~3) + 4;
    }

    // Set the flags
    if (*op0 & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (*op0 == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
}

FORCE_INLINE void Interpreter::movs(uint32_t opcode, uint32_t op2) // MOVS Rd,op2
{
    // Decode the other operand
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];

    // Move
    *op0 = op2;

    // Handle pipelining and mode switching
    if (op0 == registers[15])
    {
        if (spsr)
        {
            cpsr = *spsr;
            setMode(cpsr);
            *registers[15] = (cpsr & BIT(5)) ? ((*registers[15] & ~1) + 2) : ((*registers[15] & ~3) + 4);
            return;
        }

        *registers[15] = (*registers[15] & ~3) + 4;
    }

    // Set the flags
    if (*op0 & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (*op0 == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
}

FORCE_INLINE void Interpreter::bics(uint32_t opcode, uint32_t op2) // BICS Rd,Rn,op2
{
    // Decode the other operands
    // When used as Rn when shifting by register, the program counter is read with +4
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t op1 = *registers[(opcode & 0x000F0000) >> 16] + (((opcode & 0x020F0010) == 0x000F0010) ? 4 : 0);

    // Bit clear
    *op0 = op1 & ~op2;

    // Handle pipelining and mode switching
    if (op0 == registers[15])
    {
        if (spsr)
        {
            cpsr = *spsr;
            setMode(cpsr);
            *registers[15] = (cpsr & BIT(5)) ? ((*registers[15] & ~1) + 2) : ((*registers[15] & ~3) + 4);
            return;
        }

        *registers[15] = (*registers[15] & ~3) + 4;
    }

    // Set the flags
    if (*op0 & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (*op0 == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
}

FORCE_INLINE void Interpreter::mvns(uint32_t opcode, uint32_t op2) // MVNS Rd,op2
{
    // Decode the other operand
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];

    // Move negative
    *op0 = ~op2;

    // Handle pipelining and mode switching
    if (op0 == registers[15])
    {
        if (spsr)
        {
            cpsr = *spsr;
            setMode(cpsr);
            *registers[15] = (cpsr & BIT(5)) ? ((*registers[15] & ~1) + 2) : ((*registers[15] & ~3) + 4);
            return;
        }

        *registers[15] = (*registers[15] & ~3) + 4;
    }

    // Set the flags
    if (*op0 & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (*op0 == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
}

FORCE_INLINE void Interpreter::mul(uint32_t opcode) // MUL Rd,Rm,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t op1 = *registers[opcode & 0x0000000F];
    uint32_t op2 = *registers[(opcode & 0x00000F00) >> 8];

    // Multiplication
    *op0 = op1 * op2;
}

FORCE_INLINE void Interpreter::mla(uint32_t opcode) // MLA Rd,Rm,Rs,Rn
{
    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t op1 = *registers[opcode & 0x0000000F];
    uint32_t op2 = *registers[(opcode & 0x00000F00) >> 8];
    uint32_t op3 = *registers[(opcode & 0x0000F000) >> 12];

    // Multiplication and accumulate
    *op0 = op1 * op2 + op3;
}

FORCE_INLINE void Interpreter::umull(uint32_t opcode) // UMULL RdLo,RdHi,Rm,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t op2 = *registers[opcode & 0x0000000F];
    uint32_t op3 = *registers[(opcode & 0x00000F00) >> 8];

    // Unsigned long multiplication
    uint64_t res = (uint64_t)op2 * op3;
    *op1 = res >> 32;
    *op0 = res;
}

FORCE_INLINE void Interpreter::umlal(uint32_t opcode) // UMLAL RdLo,RdHi,Rm,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t op2 = *registers[opcode & 0x0000000F];
    uint32_t op3 = *registers[(opcode & 0x00000F00) >> 8];

    // Unsigned long multiplication and accumulate
    uint64_t res = (uint64_t)op2 * op3;
    res += ((uint64_t)*op1 << 32) | *op0;
    *op1 = res >> 32;
    *op0 = res;
}

FORCE_INLINE void Interpreter::smull(uint32_t opcode) // SMULL RdLo,RdHi,Rm,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t op2 = *registers[opcode & 0x0000000F];
    uint32_t op3 = *registers[(opcode & 0x00000F00) >> 8];

    // Signed long multiplication
    int64_t res = (int32_t)op2;
    res *= (int32_t)op3;
    *op1 = res >> 32;
    *op0 = res;
}

FORCE_INLINE void Interpreter::smlal(uint32_t opcode) // SMLAL RdLo,RdHi,Rm,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t op2 = *registers[opcode & 0x0000000F];
    uint32_t op3 = *registers[(opcode & 0x00000F00) >> 8];

    // Signed long multiplication and accumulate
    int64_t res = (int32_t)op2;
    res *= (int32_t)op3;
    res += ((int64_t)*op1 << 32) | *op0;
    *op1 = res >> 32;
    *op0 = res;
}

FORCE_INLINE void Interpreter::muls(uint32_t opcode) // MULS Rd,Rm,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t op1 = *registers[opcode & 0x0000000F];
    uint32_t op2 = *registers[(opcode & 0x00000F00) >> 8];

    // Multiplication
    *op0 = op1 * op2;

    // Set the flags
    if (*op0 & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (*op0 == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
    if (cpu == 1) cpsr &= ~BIT(29); // The carry flag is destroyed on ARM7
}

FORCE_INLINE void Interpreter::mlas(uint32_t opcode) // MLAS Rd,Rm,Rs,Rn
{
    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t op1 = *registers[opcode & 0x0000000F];
    uint32_t op2 = *registers[(opcode & 0x00000F00) >> 8];
    uint32_t op3 = *registers[(opcode & 0x0000F000) >> 12];

    // Multiplication and accumulate
    *op0 = op1 * op2 + op3;

    // Set the flags
    if (*op0 & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (*op0 == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
    if (cpu == 1) cpsr &= ~BIT(29); // The carry flag is destroyed on ARM7
}

FORCE_INLINE void Interpreter::umulls(uint32_t opcode) // UMULLS RdLo,RdHi,Rm,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t op2 = *registers[opcode & 0x0000000F];
    uint32_t op3 = *registers[(opcode & 0x00000F00) >> 8];

    // Unsigned long multiplication
    uint64_t res = (uint64_t)op2 * op3;
    *op1 = res >> 32;
    *op0 = res;

    // Set the flags
    if (*op1 & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (*op1 == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
    if (cpu == 1) cpsr &= ~BIT(29); // The carry flag is destroyed on ARM7
}

FORCE_INLINE void Interpreter::umlals(uint32_t opcode) // UMLALS RdLo,RdHi,Rm,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t op2 = *registers[opcode & 0x0000000F];
    uint32_t op3 = *registers[(opcode & 0x00000F00) >> 8];

    // Unsigned long multiplication and accumulate
    uint64_t res = (uint64_t)op2 * op3;
    res += ((uint64_t)*op1 << 32) | *op0;
    *op1 = res >> 32;
    *op0 = res;

    // Set the flags
    if (*op1 & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (*op1 == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
    if (cpu == 1) cpsr &= ~BIT(29); // The carry flag is destroyed on ARM7
}

FORCE_INLINE void Interpreter::smulls(uint32_t opcode) // SMULLS RdLo,RdHi,Rm,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t op2 = *registers[opcode & 0x0000000F];
    uint32_t op3 = *registers[(opcode & 0x00000F00) >> 8];

    // Signed long multiplication
    int64_t res = (int32_t)op2;
    res *= (int32_t)op3;
    *op1 = res >> 32;
    *op0 = res;

    // Set the flags
    if (*op1 & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (*op1 == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
    if (cpu == 1) cpsr &= ~BIT(29); // The carry flag is destroyed on ARM7
}

FORCE_INLINE void Interpreter::smlals(uint32_t opcode) // SMLALS RdLo,RdHi,Rm,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x0000F000) >> 12];
    uint32_t *op1 = registers[(opcode & 0x000F0000) >> 16];
    uint32_t op2 = *registers[opcode & 0x0000000F];
    uint32_t op3 = *registers[(opcode & 0x00000F00) >> 8];

    // Signed long multiplication and accumulate
    int64_t res = (int32_t)op2;
    res *= (int32_t)op3;
    res += ((int64_t)*op1 << 32) | *op0;
    *op1 = res >> 32;
    *op0 = res;

    // Set the flags
    if (*op1 & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (*op1 == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
    if (cpu == 1) cpsr &= ~BIT(29); // The carry flag is destroyed on ARM7
}

FORCE_INLINE void Interpreter::smulbb(uint32_t opcode) // SMULBB Rd,Rm,Rs
{
    if (cpu == 1) return; // ARM9 exclusive

    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x000F0000) >> 16];
    int16_t op1 = *registers[opcode & 0x0000000F];
    int16_t op2 = *registers[(opcode & 0x00000F00) >> 8];

    // Signed half-word multiplication
    *op0 = op1 * op2;
}

FORCE_INLINE void Interpreter::smulbt(uint32_t opcode) // SMULBT Rd,Rm,Rs
{
    if (cpu == 1) return; // ARM9 exclusive

    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x000F0000) >> 16];
    int16_t op1 = *registers[opcode & 0x0000000F];
    int16_t op2 = *registers[(opcode & 0x00000F00) >> 8] >> 16;

    // Signed half-word multiplication
    *op0 = op1 * op2;
}

FORCE_INLINE void Interpreter::smultb(uint32_t opcode) // SMULTB Rd,Rm,Rs
{
    if (cpu == 1) return; // ARM9 exclusive

    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x000F0000) >> 16];
    int16_t op1 = *registers[opcode & 0x0000000F] >> 16;
    int16_t op2 = *registers[(opcode & 0x00000F00) >> 8];

    // Signed half-word multiplication
    *op0 = op1 * op2;
}

FORCE_INLINE void Interpreter::smultt(uint32_t opcode) // SMULTT Rd,Rm,Rs
{
    if (cpu == 1) return; // ARM9 exclusive

    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x000F0000) >> 16];
    int16_t op1 = *registers[opcode & 0x0000000F] >> 16;
    int16_t op2 = *registers[(opcode & 0x00000F00) >> 8] >> 16;

    // Signed half-word multiplication
    *op0 = op1 * op2;
}

FORCE_INLINE void Interpreter::smulwb(uint32_t opcode) // SMULWB Rd,Rm,Rs
{
    if (cpu == 1) return; // ARM9 exclusive

    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x000F0000) >> 16];
    int32_t op1 = *registers[opcode & 0x0000000F];
    int16_t op2 = *registers[(opcode & 0x00000F00) >> 8];

    // Signed word by half-word multiplication
    *op0 = ((int64_t)op1 * op2) >> 16;
}

FORCE_INLINE void Interpreter::smulwt(uint32_t opcode) // SMULWT Rd,Rm,Rs
{
    if (cpu == 1) return; // ARM9 exclusive

    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x000F0000) >> 16];
    int32_t op1 = *registers[opcode & 0x0000000F];
    int16_t op2 = *registers[(opcode & 0x00000F00) >> 8] >> 16;

    // Signed word by half-word multiplication
    *op0 = ((int64_t)op1 * op2) >> 16;
}

FORCE_INLINE void Interpreter::smlabb(uint32_t opcode) // SMLABB Rd,Rm,Rs,Rn
{
    if (cpu == 1) return; // ARM9 exclusive

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
}

FORCE_INLINE void Interpreter::smlabt(uint32_t opcode) // SMLABT Rd,Rm,Rs,Rn
{
    if (cpu == 1) return; // ARM9 exclusive

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
}

FORCE_INLINE void Interpreter::smlatb(uint32_t opcode) // SMLATB Rd,Rm,Rs,Rn
{
    if (cpu == 1) return; // ARM9 exclusive

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
}

FORCE_INLINE void Interpreter::smlatt(uint32_t opcode) // SMLATT Rd,Rm,Rs,Rn
{
    if (cpu == 1) return; // ARM9 exclusive

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
}

FORCE_INLINE void Interpreter::smlawb(uint32_t opcode) // SMLAWB Rd,Rm,Rs,Rn
{
    if (cpu == 1) return; // ARM9 exclusive

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
}

FORCE_INLINE void Interpreter::smlawt(uint32_t opcode) // SMLAWT Rd,Rm,Rs,Rn
{
    if (cpu == 1) return; // ARM9 exclusive

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
}

FORCE_INLINE void Interpreter::smlalbb(uint32_t opcode) // SMLALBB RdLo,RdHi,Rm,Rs
{
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
}

FORCE_INLINE void Interpreter::smlalbt(uint32_t opcode) // SMLALBT RdLo,RdHi,Rm,Rs
{
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
}

FORCE_INLINE void Interpreter::smlaltb(uint32_t opcode) // SMLALTB RdLo,RdHi,Rm,Rs
{
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
}

FORCE_INLINE void Interpreter::smlaltt(uint32_t opcode) // SMLALTT RdLo,RdHi,Rm,Rs
{
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
}

FORCE_INLINE void Interpreter::qadd(uint32_t opcode) // QADD Rd,Rm,Rn
{
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
}

FORCE_INLINE void Interpreter::qsub(uint32_t opcode) // QSUB Rd,Rm,Rn
{
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
}

FORCE_INLINE void Interpreter::qdadd(uint32_t opcode) // QDADD Rd,Rm,Rn
{
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
}

FORCE_INLINE void Interpreter::qdsub(uint32_t opcode) // QDSUB Rd,Rm,Rn
{
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
}

FORCE_INLINE void Interpreter::clz(uint32_t opcode) // CLZ Rd,Rm
{
    if (cpu == 1) return; // ARM9 exclusive

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
}

FORCE_INLINE void Interpreter::addRegT(uint16_t opcode) // ADD Rd,Rs,Rn
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = *registers[(opcode & 0x01C0) >> 6];

    // Addition
    *op0 = op1 + op2;

    // Set the flags
    if (*op0 & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (*op0 == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
    if (op1 > *op0)     cpsr |= BIT(29); else cpsr &= ~BIT(29);
    if ((op2 & BIT(31)) == (op1 & BIT(31)) && (*op0 & BIT(31)) != (op2 & BIT(31)))
        cpsr |= BIT(28); else cpsr &= ~BIT(28);
}

FORCE_INLINE void Interpreter::subRegT(uint16_t opcode) // SUB Rd,Rs,Rn
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = *registers[(opcode & 0x01C0) >> 6];

    // Subtraction
    *op0 = op1 - op2;

    // Set the flags
    if (*op0 & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (*op0 == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
    if (op1 >= *op0)    cpsr |= BIT(29); else cpsr &= ~BIT(29);
    if ((op2 & BIT(31)) != (op1 & BIT(31)) && (*op0 & BIT(31)) == (op2 & BIT(31)))
        cpsr |= BIT(28); else cpsr &= ~BIT(28);
}

FORCE_INLINE void Interpreter::addHT(uint16_t opcode) // ADD Rd,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[((opcode & 0x0080) >> 4) | (opcode & 0x0007)];
    uint32_t op2 = *registers[(opcode & 0x0078) >> 3];

    // Addition
    *op0 += op2;

    // Handle pipelining
    if (op0 == registers[15])
        *registers[15] = (*registers[15] & ~1) + 2;
}

FORCE_INLINE void Interpreter::cmpHT(uint16_t opcode) // CMP Rd,Rs
{
    // Decode the operands
    uint32_t op1 = *registers[((opcode & 0x0080) >> 4) | (opcode & 0x0007)];
    uint32_t op2 = *registers[(opcode & 0x0078) >> 3];

    // Compare
    uint32_t res = op1 - op2;

    // Set the flags
    if (res & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (res == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
    if (op1 >= res)    cpsr |= BIT(29); else cpsr &= ~BIT(29);
    if ((op2 & BIT(31)) != (op1 & BIT(31)) && (res & BIT(31)) == (op2 & BIT(31)))
        cpsr |= BIT(28); else cpsr &= ~BIT(28);
}

FORCE_INLINE void Interpreter::movHT(uint16_t opcode) // MOV Rd,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[((opcode & 0x0080) >> 4) | (opcode & 0x0007)];
    uint32_t op2 = *registers[(opcode & 0x0078) >> 3];

    // Move
    *op0 = op2;

    // Handle pipelining
    if (op0 == registers[15])
        *registers[15] = (*registers[15] & ~1) + 2;
}

FORCE_INLINE void Interpreter::addPcT(uint16_t opcode) // ADD Rd,PC,#i
{
    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x0700) >> 8];
    uint32_t op1 = *registers[15] & ~3;
    uint32_t op2 = (opcode & 0x00FF) << 2;

    // Addition
    *op0 = op1 + op2;
}

FORCE_INLINE void Interpreter::addSpT(uint16_t opcode) // ADD Rd,SP,#i
{
    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x0700) >> 8];
    uint32_t op1 = *registers[13];
    uint32_t op2 = (opcode & 0x00FF) << 2;

    // Addition
    *op0 = op1 + op2;
}

FORCE_INLINE void Interpreter::addSpImmT(uint16_t opcode) // ADD SP,#i
{
    // Decode the operands
    uint32_t *op0 = registers[13];
    uint32_t op2 = ((opcode & BIT(7)) ? (0 - (opcode & 0x007F)) : (opcode & 0x007F)) << 2;

    // Addition
    *op0 += op2;
}

FORCE_INLINE void Interpreter::lslImmT(uint16_t opcode) // LSL Rd,Rs,#i
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint8_t op2 = (opcode & 0x07C0) >> 6;

    // Logical shift left
    *op0 = op1 << op2;

    // Set the flags
    if (*op0 & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (*op0 == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
    if (op2 > 0)
    {
        if (op1 & BIT(32 - op2)) cpsr |= BIT(29); else cpsr &= ~BIT(29);
    }
}

FORCE_INLINE void Interpreter::lsrImmT(uint16_t opcode) // LSR Rd,Rs,#i
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint8_t op2 = (opcode & 0x07C0) >> 6;

    // Logical shift right
    // A shift of 0 translates to a shift of 32
    *op0 = op2 ? (op1 >> op2) : 0;

    // Set the flags
    if (*op0 & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (*op0 == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
    if (op1 & BIT(op2 ? (op2 - 1) : 31)) cpsr |= BIT(29); else cpsr &= ~BIT(29);
}

FORCE_INLINE void Interpreter::asrImmT(uint16_t opcode) // ASR Rd,Rs,#i
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint8_t op2 = (opcode & 0x07C0) >> 6;

    // Arithmetic shift right
    // A shift of 0 translates to a shift of 32
    *op0 = op2 ? ((int32_t)op1 >> op2) : ((op1 & BIT(31)) ? 0xFFFFFFFF : 0);

    // Set the flags
    if (*op0 & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (*op0 == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
    if ((op2 == 0 && (op1 & BIT(31))) || (op2 > 0 && (op1 & BIT(op2 - 1))))
        cpsr |= BIT(29); else cpsr &= ~BIT(29);
}

FORCE_INLINE void Interpreter::addImm3T(uint16_t opcode) // ADD Rd,Rs,#i
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = (opcode & 0x01C0) >> 6;

    // Addition
    *op0 = op1 + op2;

    // Set the flags
    if (*op0 & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (*op0 == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
    if (op1 > *op0)     cpsr |= BIT(29); else cpsr &= ~BIT(29);
    if ((op2 & BIT(31)) == (op1 & BIT(31)) && (*op0 & BIT(31)) != (op2 & BIT(31)))
        cpsr |= BIT(28); else cpsr &= ~BIT(28);
}

FORCE_INLINE void Interpreter::subImm3T(uint16_t opcode) // SUB Rd,Rs,#i
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[(opcode & 0x0038) >> 3];
    uint32_t op2 = (opcode & 0x01C0) >> 6;

    // Subtraction
    *op0 = op1 - op2;

    // Set the flags
    if (*op0 & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (*op0 == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
    if (op1 >= *op0)    cpsr |= BIT(29); else cpsr &= ~BIT(29);
    if ((op2 & BIT(31)) != (op1 & BIT(31)) && (*op0 & BIT(31)) == (op2 & BIT(31)))
        cpsr |= BIT(28); else cpsr &= ~BIT(28);
}

FORCE_INLINE void Interpreter::addImm8T(uint16_t opcode) // ADD Rd,#i
{
    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x0700) >> 8];
    uint32_t op1 = *registers[(opcode & 0x0700) >> 8];
    uint32_t op2 = opcode & 0x00FF;

    // Addition
    *op0 += op2;

    // Set the flags
    if (*op0 & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (*op0 == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
    if (op1 > *op0)     cpsr |= BIT(29); else cpsr &= ~BIT(29);
    if ((op2 & BIT(31)) == (op1 & BIT(31)) && (*op0 & BIT(31)) != (op2 & BIT(31)))
        cpsr |= BIT(28); else cpsr &= ~BIT(28);
}

FORCE_INLINE void Interpreter::subImm8T(uint16_t opcode) // SUB Rd,#i
{
    // Decode the operands
    uint32_t *op0 = registers[(opcode & 0x0700) >> 8];
    uint32_t op1 = *registers[(opcode & 0x0700) >> 8];
    uint32_t op2 = opcode & 0x00FF;

    // Subtraction
    *op0 -= op2;

    // Set the flags
    if (*op0 & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (*op0 == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
    if (op1 >= *op0)    cpsr |= BIT(29); else cpsr &= ~BIT(29);
    if ((op2 & BIT(31)) != (op1 & BIT(31)) && (*op0 & BIT(31)) == (op2 & BIT(31)))
        cpsr |= BIT(28); else cpsr &= ~BIT(28);
}


FORCE_INLINE void Interpreter::cmpImm8T(uint16_t opcode) // CMP Rd,#i
{
    // Decode the operands
    uint32_t op1 = *registers[(opcode & 0x0700) >> 8];
    uint32_t op2 = opcode & 0x00FF;

    // Compare
    uint32_t res = op1 - op2;

    // Set the flags
    if (res & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (res == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
    if (op1 >= res)    cpsr |= BIT(29); else cpsr &= ~BIT(29);
    if ((op2 & BIT(31)) != (op1 & BIT(31)) && (res & BIT(31)) == (op2 & BIT(31)))
        cpsr |= BIT(28); else cpsr &= ~BIT(28);
}

FORCE_INLINE void Interpreter::movImm8T(uint16_t opcode) // MOV Rd,#i
{
    // Decode the other operand
    uint32_t *op0 = registers[(opcode & 0x0700) >> 8];
    uint32_t op2 = opcode & 0x00FF;

    // Move
    *op0 = op2;

    // Set the flags
    if (*op0 & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (*op0 == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
}

FORCE_INLINE void Interpreter::lslDpT(uint16_t opcode) // LSL Rd,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[opcode & 0x0007];
    uint8_t op2 = *registers[(opcode & 0x0038) >> 3];

    // Logical shift left
    // Shifts greater than 32 can be wonky on host systems, so they're handled explicitly
    *op0 = (op2 < 32) ? (*op0 << op2) : 0;

    // Set the flags
    if (*op0 & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (*op0 == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
    if (op2 > 0)
    {
        if (op2 <= 32 && (op1 & BIT(32 - op2))) cpsr |= BIT(29); else cpsr &= ~BIT(29);
    }
}

FORCE_INLINE void Interpreter::lsrDpT(uint16_t opcode) // LSR Rd,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[opcode & 0x0007];
    uint8_t op2 = *registers[(opcode & 0x0038) >> 3];

    // Logical shift right
    // Shifts greater than 32 can be wonky on host systems, so they're handled explicitly
    *op0 = (op2 < 32) ? (*op0 >> op2) : 0;

    // Set the flags
    if (*op0 & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (*op0 == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
    if (op2 > 0)
    {
        if (op2 <= 32 && (op1 & BIT(op2 - 1))) cpsr |= BIT(29); else cpsr &= ~BIT(29);
    }
}

FORCE_INLINE void Interpreter::asrDpT(uint16_t opcode) // ASR Rd,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[opcode & 0x0007];
    uint8_t op2 = *registers[(opcode & 0x0038) >> 3];

    // Arithmetic shift right
    // Shifts greater than 32 can be wonky on host systems, so they're handled explicitly
    *op0 = (op2 < 32) ? ((int32_t)(*op0) >> op2) : ((*op0 & BIT(31)) ? 0xFFFFFFFF : 0);

    // Set the flags
    if (*op0 & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (*op0 == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
    if (op2 > 0)
    {
        if ((op2 > 32 && (op1 & BIT(31))) || (op2 <= 32 && (op1 & BIT(op2 - 1))))
            cpsr |= BIT(29); else cpsr &= ~BIT(29);
    }
}

FORCE_INLINE void Interpreter::rorDpT(uint16_t opcode) // ROR Rd,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[opcode & 0x0007];
    uint8_t op2 = *registers[(opcode & 0x0038) >> 3];

    // Rotate right
    *op0 = (*op0 << (32 - op2 % 32)) | (*op0 >> (op2 % 32));

    // Set the flags
    if (*op0 & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (*op0 == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
    if (op2 > 0)
    {
        if (op1 & BIT((op2 - 1) % 32)) cpsr |= BIT(29); else cpsr &= ~BIT(29);
    }
}

FORCE_INLINE void Interpreter::andDpT(uint16_t opcode) // AND Rd,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op2 = *registers[(opcode & 0x0038) >> 3];

    // Bitwise and
    *op0 &= op2;

    // Set the flags
    if (*op0 & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (*op0 == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
}

FORCE_INLINE void Interpreter::eorDpT(uint16_t opcode) // EOR Rd,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op2 = *registers[(opcode & 0x0038) >> 3];

    // Bitwise exclusive or
    *op0 ^= op2;

    // Set the flags
    if (*op0 & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (*op0 == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
}

FORCE_INLINE void Interpreter::adcDpT(uint16_t opcode) // ADC Rd,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[opcode & 0x0007];
    uint32_t op2 = *registers[(opcode & 0x0038) >> 3];

    // Addition with carry
    *op0 += op2 + ((cpsr & BIT(29)) >> 29);

    // Set the flags
    if (*op0 & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (*op0 == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
    if (op1 > *op0 || (op2 == 0xFFFFFFFF && (cpsr & BIT(29)))) cpsr |= BIT(29); else cpsr &= ~BIT(29);
    if ((op2 & BIT(31)) == (op1 & BIT(31)) && (*op0 & BIT(31)) != (op2 & BIT(31)))
        cpsr |= BIT(28); else cpsr &= ~BIT(28);
}

FORCE_INLINE void Interpreter::sbcDpT(uint16_t opcode) // SBC Rd,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = *registers[opcode & 0x0007];
    uint32_t op2 = *registers[(opcode & 0x0038) >> 3];

    // Subtraction with carry
    *op0 = op1 - op2 - 1 + ((cpsr & BIT(29)) >> 29);

    // Set the flags
    if (*op0 & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (*op0 == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
    if (op1 >= *op0 && (op2 != 0xFFFFFFFF || (cpsr & BIT(29)))) cpsr |= BIT(29); else cpsr &= ~BIT(29);
    if ((op2 & BIT(31)) != (op1 & BIT(31)) && (*op0 & BIT(31)) == (op2 & BIT(31)))
        cpsr |= BIT(28); else cpsr &= ~BIT(28);
}

FORCE_INLINE void Interpreter::tstDpT(uint16_t opcode) // TST Rd,Rs
{
    // Decode the operands
    uint32_t op1 = *registers[opcode & 0x0007];
    uint32_t op2 = *registers[(opcode & 0x0038) >> 3];

    // Test bits
    uint32_t res = op1 & op2;

    // Set the flags
    if (res & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (res == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
}

FORCE_INLINE void Interpreter::cmpDpT(uint16_t opcode) // CMP Rd,Rs
{
    // Decode the operands
    uint32_t op1 = *registers[opcode & 0x0007];
    uint32_t op2 = *registers[(opcode & 0x0038) >> 3];

    // Compare
    uint32_t res = op1 - op2;

    // Set the flags
    if (res & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (res == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
    if (op1 >= res)    cpsr |= BIT(29); else cpsr &= ~BIT(29);
    if ((op2 & BIT(31)) != (op1 & BIT(31)) && (res & BIT(31)) == (op2 & BIT(31)))
        cpsr |= BIT(28); else cpsr &= ~BIT(28);
}

FORCE_INLINE void Interpreter::cmnDpT(uint16_t opcode) // CMN Rd,Rs
{
    // Decode the operands
    uint32_t op1 = *registers[opcode & 0x0007];
    uint32_t op2 = *registers[(opcode & 0x0038) >> 3];

    // Compare negative
    uint32_t res = op1 + op2;

    // Set the flags
    if (res & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (res == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
    if (op1 > res)     cpsr |= BIT(29); else cpsr &= ~BIT(29);
    if ((op2 & BIT(31)) == (op1 & BIT(31)) && (res & BIT(31)) != (op2 & BIT(31)))
        cpsr |= BIT(28); else cpsr &= ~BIT(28);
}

FORCE_INLINE void Interpreter::orrDpT(uint16_t opcode) // ORR Rd,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op2 = *registers[(opcode & 0x0038) >> 3];

    // Bitwise or
    *op0 |= op2;

    // Set the flags
    if (*op0 & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (*op0 == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
}

FORCE_INLINE void Interpreter::bicDpT(uint16_t opcode) // BIC Rd,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op2 = *registers[(opcode & 0x0038) >> 3];

    // Bit clear
    *op0 &= ~op2;

    // Set the flags
    if (*op0 & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (*op0 == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
}

FORCE_INLINE void Interpreter::mvnDpT(uint16_t opcode) // MVN Rd,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op2 = *registers[(opcode & 0x0038) >> 3];

    // Move negative
    *op0 = ~op2;

    // Set the flags
    if (*op0 & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (*op0 == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
}

FORCE_INLINE void Interpreter::negDpT(uint16_t opcode) // NEG Rd,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op1 = 0;
    uint32_t op2 = *registers[(opcode & 0x0038) >> 3];

    // Negation
    *op0 = op1 - op2;

    // Set the flags
    if (*op0 & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (*op0 == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
    if (op1 >= *op0)    cpsr |= BIT(29); else cpsr &= ~BIT(29);
    if ((op2 & BIT(31)) != (op1 & BIT(31)) && (*op0 & BIT(31)) == (op2 & BIT(31)))
        cpsr |= BIT(28); else cpsr &= ~BIT(28);
}

FORCE_INLINE void Interpreter::mulDpT(uint16_t opcode) // MUL Rd,Rs
{
    // Decode the operands
    uint32_t *op0 = registers[opcode & 0x0007];
    uint32_t op2 = *registers[(opcode & 0x0038) >> 3];

    // Multiplication
    *op0 *= op2;

    // Set the flags
    if (*op0 & BIT(31)) cpsr |= BIT(31); else cpsr &= ~BIT(31);
    if (*op0 == 0)      cpsr |= BIT(30); else cpsr &= ~BIT(30);
    if (cpu == 1) cpsr &= ~BIT(29); // The carry flag is destroyed on ARM7
}

#endif // INTERPRETER_ALU
