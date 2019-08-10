/*
    Copyright 2019 Hydr8gon

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

#include "interpreter_transfer.h"
#include "core.h"
#include "memory.h"

// Register values for ARM opcodes
#define RN (*cpu->registers[(opcode & 0x000F0000) >> 16])
#define RD (*cpu->registers[(opcode & 0x0000F000) >> 12])
#define RM (*cpu->registers[(opcode & 0x0000000F)])

// Immediate values for ARM opcodes
#define SING_IMM (opcode & 0x00000FFF)
#define SPEC_IMM (((opcode & 0x00000F00) >> 4) | (opcode & 0x0000000F))
#define SHIFT    ((opcode & 0x00000F80) >> 7)

// Register values for THUMB opcodes
#define RO_THUMB  (*cpu->registers[(opcode & 0x01C0) >> 6])
#define RB_THUMB  (*cpu->registers[(opcode & 0x0038) >> 3])
#define RD_THUMB  (*cpu->registers[(opcode & 0x0007)])
#define RD8_THUMB (*cpu->registers[(opcode & 0x0700) >> 8])

// Immediate values for THUMB opcodes
#define IMM5_THUMB ((opcode & 0x07C0) >> 6)
#define IMM8_THUMB (opcode & 0x00FF)

// Shifted values
#define LSL (RM << SHIFT)
#define LSR (SHIFT ? (RM >> SHIFT) : 0)
#define ASR (SHIFT ? ((int32_t)RM >> SHIFT) : ((RM & BIT(31)) ? 0xFFFFFFFF : 0))
#define ROR (SHIFT ? ((RM << (32 - SHIFT)) | (RM >> SHIFT)) : (((cpu->cpsr & BIT(29)) << 2) | (RM >> 1)))

// Store, post-adjust
#define STR_PT(type, sign, op0, op1, op2) \
    uint32_t offset = op2;                \
    memory::write<type>(cpu, op1, op0);   \
    op1 = op1 sign offset;

// Load, post-adjust
#define LDR_PT(type, sign, op0, op1, op2) \
    uint32_t offset = op2;                \
    op0 = memory::read<type>(cpu, op1);   \
    op1 = op1 sign offset;

// Store, pre-adjust without writeback
#define STR_OF(type, sign, op0, op1, op2)        \
    memory::write<type>(cpu, op1 sign op2, op0);

// Load, pre-adjust without writeback
#define LDR_OF(type, sign, op0, op1, op2)        \
    op0 = memory::read<type>(cpu, op1 sign op2);

// Store, pre-adjust with writeback
#define STR_PR(type, sign, op0, op1, op2) \
    op1 = op1 sign op2;                   \
    memory::write<type>(cpu, op1, op0);

// Load, pre-adjust with writeback
#define LDR_PR(type, sign, op0, op1, op2) \
    op1 = op1 sign op2;                   \
    op0 = memory::read<type>(cpu, op1);

// Swap between registers and memory
#define SWP(type, op0, op1, op2)          \
    type value = op1;                     \
    op0 = memory::read<type>(cpu, op2);   \
    memory::write<type>(cpu, op2, value);

// Block store, post-decrement
#define STMDA(op0, regs, list)                                       \
    uint32_t address = op0;                                          \
    int8_t offset = 0;                                               \
    for (int i = 15; i >= 0; i--)                                    \
    {                                                                \
        if ((list) & BIT(i))                                         \
        {                                                            \
            memory::write<uint32_t>(cpu, address + offset, regs[i]); \
            offset -= 4;                                             \
        }                                                            \
    }

// Block load, post-decrement
#define LDMDA(op0, regs, list)                                       \
    uint32_t address = op0;                                          \
    int8_t offset = 0;                                               \
    for (int i = 15; i >= 0; i--)                                    \
    {                                                                \
        if ((list) & BIT(i))                                         \
        {                                                            \
            regs[i] = memory::read<uint32_t>(cpu, address + offset); \
            offset -= 4;                                             \
        }                                                            \
    }

// Block store, post-increment
#define STMIA(op0, regs, list)                                       \
    uint32_t address = op0;                                          \
    int8_t offset = 0;                                               \
    for (int i = 0; i <= 15; i++)                                    \
    {                                                                \
        if ((list) & BIT(i))                                         \
        {                                                            \
            memory::write<uint32_t>(cpu, address + offset, regs[i]); \
            offset += 4;                                             \
        }                                                            \
    }

// Block load, post-increment
#define LDMIA(op0, regs, list)                                       \
    uint32_t address = op0;                                          \
    int8_t offset = 0;                                               \
    for (int i = 0; i <= 15; i++)                                    \
    {                                                                \
        if ((list) & BIT(i))                                         \
        {                                                            \
            regs[i] = memory::read<uint32_t>(cpu, address + offset); \
            offset += 4;                                             \
        }                                                            \
    }

// Block store, pre-decrement
#define STMDB(op0, regs, list)                                       \
    uint32_t address = op0;                                          \
    int8_t offset = 0;                                               \
    for (int i = 15; i >= 0; i--)                                    \
    {                                                                \
        if ((list) & BIT(i))                                         \
        {                                                            \
            offset -= 4;                                             \
            memory::write<uint32_t>(cpu, address + offset, regs[i]); \
        }                                                            \
    }

// Block load, pre-decrement
#define LDMDB(op0, regs, list)                                       \
    uint32_t address = op0;                                          \
    int8_t offset = 0;                                               \
    for (int i = 15; i >= 0; i--)                                    \
    {                                                                \
        if ((list) & BIT(i))                                         \
        {                                                            \
            offset -= 4;                                             \
            regs[i] = memory::read<uint32_t>(cpu, address + offset); \
        }                                                            \
    }

// Block store, pre-increment
#define STMIB(op0, regs, list)                                       \
    uint32_t address = op0;                                          \
    int8_t offset = 0;                                               \
    for (int i = 0; i <= 15; i++)                                    \
    {                                                                \
        if ((list) & BIT(i))                                         \
        {                                                            \
            offset += 4;                                             \
            memory::write<uint32_t>(cpu, address + offset, regs[i]); \
        }                                                            \
    }

// Block load, pre-increment
#define LDMIB(op0, regs, list)                                       \
    uint32_t address = op0;                                          \
    int8_t offset = 0;                                               \
    for (int i = 0; i <= 15; i++)                                    \
    {                                                                \
        if ((list) & BIT(i))                                         \
        {                                                            \
            offset += 4;                                             \
            regs[i] = memory::read<uint32_t>(cpu, address + offset); \
        }                                                            \
    }

// Writeback for block transfers
#define WRITEBACK(op0)  \
    if (op0 == address) \
        op0 += offset;

// Switch to THUMB mode for ARM loads to the program counter
#define THUMB_SWITCH                                      \
    if (cpu->type == 9 && (*cpu->registers[15] & BIT(0))) \
    {                                                     \
        cpu->cpsr |= BIT(5);                              \
        *cpu->registers[15] &= ~BIT(0);                   \
    }

// Switch to ARM mode for THUMB loads to the program counter
#define ARM_SWITCH                          \
    if (cpu->type == 9)                     \
    {                                       \
        if (*cpu->registers[15] & BIT(0))   \
            *cpu->registers[15] &= ~BIT(0); \
        else                                \
            cpu->cpsr &= ~BIT(5);           \
    }

// Return to the previous CPU mode if the program counter was loaded in a user-mode block transfer
#define MODE_SWITCH                                          \
    if ((opcode & BIT(15)) && cpu->spsr)                     \
    {                                                        \
        cpu->cpsr = *cpu->spsr;                              \
        interpreter::setMode(cpu, (cpu->cpsr & 0x0000001F)); \
    }                                                        \
    THUMB_SWITCH

// Below, the above macros are combined to form all the variations of the transfer instructions

namespace interpreter_transfer
{

void strhPtrm (interpreter::Cpu *cpu, uint32_t opcode) { STR_PT(uint16_t, -, RD, RN, RM) } // STRH  Rd,[Rn],-Rm
void ldrhPtrm (interpreter::Cpu *cpu, uint32_t opcode) { LDR_PT(uint16_t, -, RD, RN, RM) } // LDRH  Rd,[Rn],-Rm
void ldrsbPtrm(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PT(int8_t,   -, RD, RN, RM) } // LDRSB Rd,[Rn],-Rm
void ldrshPtrm(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PT(int16_t,  -, RD, RN, RM) } // LDRSH Rd,[Rn],-Rm

void strhPtim (interpreter::Cpu *cpu, uint32_t opcode) { STR_PT(uint16_t, -, RD, RN, SPEC_IMM) } // STRH  Rd,[Rn],-#i
void ldrhPtim (interpreter::Cpu *cpu, uint32_t opcode) { LDR_PT(uint16_t, -, RD, RN, SPEC_IMM) } // LDRH  Rd,[Rn],-#i
void ldrsbPtim(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PT(int8_t,   -, RD, RN, SPEC_IMM) } // LDRSB Rd,[Rn],-#i
void ldrshPtim(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PT(int16_t,  -, RD, RN, SPEC_IMM) } // LDRSH Rd,[Rn],-#i

void strhPtrp (interpreter::Cpu *cpu, uint32_t opcode) { STR_PT(uint16_t, +, RD, RN, RM) } // STRH  Rd,[Rn],Rm
void ldrhPtrp (interpreter::Cpu *cpu, uint32_t opcode) { LDR_PT(uint16_t, +, RD, RN, RM) } // LDRH  Rd,[Rn],Rm
void ldrsbPtrp(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PT(int8_t,   +, RD, RN, RM) } // LDRSB Rd,[Rn],Rm
void ldrshPtrp(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PT(int16_t,  +, RD, RN, RM) } // LDRSH Rd,[Rn],Rm

void strhPtip (interpreter::Cpu *cpu, uint32_t opcode) { STR_PT(uint16_t, +, RD, RN, SPEC_IMM) } // STRH  Rd,[Rn],#i
void ldrhPtip (interpreter::Cpu *cpu, uint32_t opcode) { LDR_PT(uint16_t, +, RD, RN, SPEC_IMM) } // LDRH  Rd,[Rn],#i
void ldrsbPtip(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PT(int8_t,   +, RD, RN, SPEC_IMM) } // LDRSB Rd,[Rn],#i
void ldrshPtip(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PT(int16_t,  +, RD, RN, SPEC_IMM) } // LDRSH Rd,[Rn],#i

void swp(interpreter::Cpu *cpu, uint32_t opcode) { SWP(uint32_t, RD, RM, RN) } // SWP Rd,Rm,[Rn]

void strhOfrm (interpreter::Cpu *cpu, uint32_t opcode) { STR_OF(uint16_t, -, RD, RN, RM) } // STRH  Rd,[Rn,-Rm]
void ldrhOfrm (interpreter::Cpu *cpu, uint32_t opcode) { LDR_OF(uint16_t, -, RD, RN, RM) } // LDRH  Rd,[Rn,-Rm]
void ldrsbOfrm(interpreter::Cpu *cpu, uint32_t opcode) { LDR_OF(int8_t,   -, RD, RN, RM) } // LDRSB Rd,[Rn,-Rm]
void ldrshOfrm(interpreter::Cpu *cpu, uint32_t opcode) { LDR_OF(int16_t,  -, RD, RN, RM) } // LDRSH Rd,[Rn,-Rm]

void strhPrrm (interpreter::Cpu *cpu, uint32_t opcode) { STR_PR(uint16_t, -, RD, RN, RM) } // STRH  Rd,[Rn,-Rm]!
void ldrhPrrm (interpreter::Cpu *cpu, uint32_t opcode) { LDR_PR(uint16_t, -, RD, RN, RM) } // LDRH  Rd,[Rn,-Rm]!
void ldrsbPrrm(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PR(int8_t,   -, RD, RN, RM) } // LDRSB Rd,[Rn,-Rm]!
void ldrshPrrm(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PR(int16_t,  -, RD, RN, RM) } // LDRSH Rd,[Rn,-Rm]!

void swpb(interpreter::Cpu *cpu, uint32_t opcode) { SWP(uint8_t, RD, RM, RN) } // SWPB Rd,Rm,[Rn]

void strhOfim (interpreter::Cpu *cpu, uint32_t opcode) { STR_OF(uint16_t, -, RD, RN, SPEC_IMM) } // STRH  Rd,[Rn,-#i]
void ldrhOfim (interpreter::Cpu *cpu, uint32_t opcode) { LDR_OF(uint16_t, -, RD, RN, SPEC_IMM) } // LDRH  Rd,[Rn,-#i]
void ldrsbOfim(interpreter::Cpu *cpu, uint32_t opcode) { LDR_OF(int8_t,   -, RD, RN, SPEC_IMM) } // LDRSB Rd,[Rn,-#i]
void ldrshOfim(interpreter::Cpu *cpu, uint32_t opcode) { LDR_OF(int16_t,  -, RD, RN, SPEC_IMM) } // LDRSH Rd,[Rn,-#i]

void strhPrim (interpreter::Cpu *cpu, uint32_t opcode) { STR_PR(uint16_t, -, RD, RN, SPEC_IMM) } // STRH  Rd,[Rn,-#i]!
void ldrhPrim (interpreter::Cpu *cpu, uint32_t opcode) { LDR_PR(uint16_t, -, RD, RN, SPEC_IMM) } // LDRH  Rd,[Rn,-#i]!
void ldrsbPrim(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PR(int8_t,   -, RD, RN, SPEC_IMM) } // LDRSB Rd,[Rn,-#i]!
void ldrshPrim(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PR(int16_t,  -, RD, RN, SPEC_IMM) } // LDRSH Rd,[Rn,-#i]!

void strhOfrp (interpreter::Cpu *cpu, uint32_t opcode) { STR_OF(uint16_t, +, RD, RN, RM) } // STRH  Rd,[Rn,Rm]
void ldrhOfrp (interpreter::Cpu *cpu, uint32_t opcode) { LDR_OF(uint16_t, +, RD, RN, RM) } // LDRH  Rd,[Rn,Rm]
void ldrsbOfrp(interpreter::Cpu *cpu, uint32_t opcode) { LDR_OF(int8_t,   +, RD, RN, RM) } // LDRSB Rd,[Rn,Rm]
void ldrshOfrp(interpreter::Cpu *cpu, uint32_t opcode) { LDR_OF(int16_t,  +, RD, RN, RM) } // LDRSH Rd,[Rn,Rm]

void strhPrrp (interpreter::Cpu *cpu, uint32_t opcode) { STR_PR(uint16_t, +, RD, RN, RM) } // STRH  Rd,[Rn,Rm]!
void ldrhPrrp (interpreter::Cpu *cpu, uint32_t opcode) { LDR_PR(uint16_t, +, RD, RN, RM) } // LDRH  Rd,[Rn,Rm]!
void ldrsbPrrp(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PR(int8_t,   +, RD, RN, RM) } // LDRSB Rd,[Rn,-#i]!
void ldrshPrrp(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PR(int16_t,  +, RD, RN, RM) } // LDRSH Rd,[Rn,-#i]!

void strhOfip (interpreter::Cpu *cpu, uint32_t opcode) { STR_OF(uint16_t, +, RD, RN, SPEC_IMM) } // STRH  Rd,[Rn,#i]
void ldrhOfip (interpreter::Cpu *cpu, uint32_t opcode) { LDR_OF(uint16_t, +, RD, RN, SPEC_IMM) } // LDRH  Rd,[Rn,#i]
void ldrsbOfip(interpreter::Cpu *cpu, uint32_t opcode) { LDR_OF(int8_t,   +, RD, RN, SPEC_IMM) } // LDRSB Rd,[Rn,#i]
void ldrshOfip(interpreter::Cpu *cpu, uint32_t opcode) { LDR_OF(int16_t,  +, RD, RN, SPEC_IMM) } // LDRSH Rd,[Rn,#i]

void strhPrip (interpreter::Cpu *cpu, uint32_t opcode) { STR_PR(uint16_t, +, RD, RN, SPEC_IMM) } // STRH  Rd,[Rn,#i]!
void ldrhPrip (interpreter::Cpu *cpu, uint32_t opcode) { LDR_PR(uint16_t, +, RD, RN, SPEC_IMM) } // LDRH  Rd,[Rn,#i]!
void ldrsbPrip(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PR(int8_t,   +, RD, RN, SPEC_IMM) } // LDRSB Rd,[Rn,#i]!
void ldrshPrip(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PR(int16_t,  +, RD, RN, SPEC_IMM) } // LDRSH Rd,[Rn,#i]!

void strPtim (interpreter::Cpu *cpu, uint32_t opcode) { STR_PT(uint32_t, -, RD, RN, SING_IMM)              } // STR  Rd,[Rn],-#i
void ldrPtim (interpreter::Cpu *cpu, uint32_t opcode) { LDR_PT(uint32_t, -, RD, RN, SING_IMM) THUMB_SWITCH } // LDR  Rd,[Rn],-#i
void strbPtim(interpreter::Cpu *cpu, uint32_t opcode) { STR_PT(uint8_t,  -, RD, RN, SING_IMM)              } // STRB Rd,[Rn],-#i
void ldrbPtim(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PT(uint8_t,  -, RD, RN, SING_IMM) THUMB_SWITCH } // LDRB Rd,[Rn],-#i

void strPtip (interpreter::Cpu *cpu, uint32_t opcode) { STR_PT(uint32_t, +, RD, RN, SING_IMM)              } // STR  Rd,[Rn],#i
void ldrPtip (interpreter::Cpu *cpu, uint32_t opcode) { LDR_PT(uint32_t, +, RD, RN, SING_IMM) THUMB_SWITCH } // LDR  Rd,[Rn],#i
void strbPtip(interpreter::Cpu *cpu, uint32_t opcode) { STR_PT(uint8_t,  +, RD, RN, SING_IMM)              } // STRB Rd,[Rn],#i
void ldrbPtip(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PT(uint8_t,  +, RD, RN, SING_IMM) THUMB_SWITCH } // LDRB Rd,[Rn],#i

void strOfim (interpreter::Cpu *cpu, uint32_t opcode) { STR_OF(uint32_t, -, RD, RN, SING_IMM)              } // STR  Rd,[Rn,-#i]
void ldrOfim (interpreter::Cpu *cpu, uint32_t opcode) { LDR_OF(uint32_t, -, RD, RN, SING_IMM) THUMB_SWITCH } // LDR  Rd,[Rn,-#i]
void strbOfim(interpreter::Cpu *cpu, uint32_t opcode) { STR_OF(uint8_t,  -, RD, RN, SING_IMM)              } // STRB Rd,[Rn,-#i]
void ldrbOfim(interpreter::Cpu *cpu, uint32_t opcode) { LDR_OF(uint8_t,  -, RD, RN, SING_IMM) THUMB_SWITCH } // LDRB Rd,[Rn,-#i]

void strPrim (interpreter::Cpu *cpu, uint32_t opcode) { STR_PR(uint32_t, -, RD, RN, SING_IMM)              } // STR  Rd,[Rn,-#i]!
void ldrPrim (interpreter::Cpu *cpu, uint32_t opcode) { LDR_PR(uint32_t, -, RD, RN, SING_IMM) THUMB_SWITCH } // LDR  Rd,[Rn,-#i]!
void strbPrim(interpreter::Cpu *cpu, uint32_t opcode) { STR_PR(uint8_t,  -, RD, RN, SING_IMM)              } // STRB Rd,[Rn,-#i]!
void ldrbPrim(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PR(uint8_t,  -, RD, RN, SING_IMM) THUMB_SWITCH } // LDRB Rd,[Rn,-#i]!

void strOfip (interpreter::Cpu *cpu, uint32_t opcode) { STR_OF(uint32_t, +, RD, RN, SING_IMM)              } // STR  Rd,[Rn,#i]
void ldrOfip (interpreter::Cpu *cpu, uint32_t opcode) { LDR_OF(uint32_t, +, RD, RN, SING_IMM) THUMB_SWITCH } // LDR  Rd,[Rn,#i]
void strbOfip(interpreter::Cpu *cpu, uint32_t opcode) { STR_OF(uint8_t,  +, RD, RN, SING_IMM)              } // STRB Rd,[Rn,#i]
void ldrbOfip(interpreter::Cpu *cpu, uint32_t opcode) { LDR_OF(uint8_t,  +, RD, RN, SING_IMM) THUMB_SWITCH } // LDRB Rd,[Rn,#i]

void strPrip (interpreter::Cpu *cpu, uint32_t opcode) { STR_PR(uint32_t, +, RD, RN, SING_IMM)              } // STR  Rd,[Rn,#i]!
void ldrPrip (interpreter::Cpu *cpu, uint32_t opcode) { LDR_PR(uint32_t, +, RD, RN, SING_IMM) THUMB_SWITCH } // LDR  Rd,[Rn,#i]!
void strbPrip(interpreter::Cpu *cpu, uint32_t opcode) { STR_PR(uint8_t,  +, RD, RN, SING_IMM)              } // STRB Rd,[Rn,#i]!
void ldrbPrip(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PR(uint8_t,  +, RD, RN, SING_IMM) THUMB_SWITCH } // LDRB Rd,[Rn,#i]!

void strPtrmll(interpreter::Cpu *cpu, uint32_t opcode) { STR_PT(uint32_t, -, RD, RN, LSL)              } // STR Rd,[Rn],-Rm,LSL #i
void strPtrmlr(interpreter::Cpu *cpu, uint32_t opcode) { STR_PT(uint32_t, -, RD, RN, LSR)              } // STR Rd,[Rn],-Rm,LSR #i
void strPtrmar(interpreter::Cpu *cpu, uint32_t opcode) { STR_PT(uint32_t, -, RD, RN, ASR)              } // STR Rd,[Rn],-Rm,ASR #i
void strPtrmrr(interpreter::Cpu *cpu, uint32_t opcode) { STR_PT(uint32_t, -, RD, RN, ROR)              } // STR Rd,[Rn],-Rm,ROR #i
void ldrPtrmll(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PT(uint32_t, -, RD, RN, LSL) THUMB_SWITCH } // LDR Rd,[Rn],-Rm,LSL #i
void ldrPtrmlr(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PT(uint32_t, -, RD, RN, LSR) THUMB_SWITCH } // LDR Rd,[Rn],-Rm,LSR #i
void ldrPtrmar(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PT(uint32_t, -, RD, RN, ASR) THUMB_SWITCH } // LDR Rd,[Rn],-Rm,ASR #i
void ldrPtrmrr(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PT(uint32_t, -, RD, RN, ROR) THUMB_SWITCH } // LDR Rd,[Rn],-Rm,ROR #i

void strbPtrmll(interpreter::Cpu *cpu, uint32_t opcode) { STR_PT(uint8_t, -, RD, RN, LSL)              } // STRB Rd,[Rn],-Rm,LSL #i
void strbPtrmlr(interpreter::Cpu *cpu, uint32_t opcode) { STR_PT(uint8_t, -, RD, RN, LSR)              } // STRB Rd,[Rn],-Rm,LSR #i
void strbPtrmar(interpreter::Cpu *cpu, uint32_t opcode) { STR_PT(uint8_t, -, RD, RN, ASR)              } // STRB Rd,[Rn],-Rm,ASR #i
void strbPtrmrr(interpreter::Cpu *cpu, uint32_t opcode) { STR_PT(uint8_t, -, RD, RN, ROR)              } // STRB Rd,[Rn],-Rm,ROR #i
void ldrbPtrmll(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PT(uint8_t, -, RD, RN, LSL) THUMB_SWITCH } // LDRB Rd,[Rn],-Rm,LSL #i
void ldrbPtrmlr(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PT(uint8_t, -, RD, RN, LSR) THUMB_SWITCH } // LDRB Rd,[Rn],-Rm,LSR #i
void ldrbPtrmar(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PT(uint8_t, -, RD, RN, ASR) THUMB_SWITCH } // LDRB Rd,[Rn],-Rm,ASR #i
void ldrbPtrmrr(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PT(uint8_t, -, RD, RN, ROR) THUMB_SWITCH } // LDRB Rd,[Rn],-Rm,ROR #i

void strPtrpll(interpreter::Cpu *cpu, uint32_t opcode) { STR_PT(uint32_t, +, RD, RN, LSL)              } // STR Rd,[Rn],Rm,LSL #i
void strPtrplr(interpreter::Cpu *cpu, uint32_t opcode) { STR_PT(uint32_t, +, RD, RN, LSR)              } // STR Rd,[Rn],Rm,LSR #i
void strPtrpar(interpreter::Cpu *cpu, uint32_t opcode) { STR_PT(uint32_t, +, RD, RN, ASR)              } // STR Rd,[Rn],Rm,ASR #i
void strPtrprr(interpreter::Cpu *cpu, uint32_t opcode) { STR_PT(uint32_t, +, RD, RN, ROR)              } // STR Rd,[Rn],Rm,ROR #i
void ldrPtrpll(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PT(uint32_t, +, RD, RN, LSL) THUMB_SWITCH } // LDR Rd,[Rn],Rm,LSL #i
void ldrPtrplr(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PT(uint32_t, +, RD, RN, LSR) THUMB_SWITCH } // LDR Rd,[Rn],Rm,LSR #i
void ldrPtrpar(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PT(uint32_t, +, RD, RN, ASR) THUMB_SWITCH } // LDR Rd,[Rn],Rm,ASR #i
void ldrPtrprr(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PT(uint32_t, +, RD, RN, ROR) THUMB_SWITCH } // LDR Rd,[Rn],Rm,ROR #i

void strbPtrpll(interpreter::Cpu *cpu, uint32_t opcode) { STR_PT(uint8_t, +, RD, RN, LSL)              } // STRB Rd,[Rn],Rm,LSL #i
void strbPtrplr(interpreter::Cpu *cpu, uint32_t opcode) { STR_PT(uint8_t, +, RD, RN, LSR)              } // STRB Rd,[Rn],Rm,LSR #i
void strbPtrpar(interpreter::Cpu *cpu, uint32_t opcode) { STR_PT(uint8_t, +, RD, RN, ASR)              } // STRB Rd,[Rn],Rm,ASR #i
void strbPtrprr(interpreter::Cpu *cpu, uint32_t opcode) { STR_PT(uint8_t, +, RD, RN, ROR)              } // STRB Rd,[Rn],Rm,ROR #i
void ldrbPtrpll(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PT(uint8_t, +, RD, RN, LSL) THUMB_SWITCH } // LDRB Rd,[Rn],Rm,LSL #i
void ldrbPtrplr(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PT(uint8_t, +, RD, RN, LSR) THUMB_SWITCH } // LDRB Rd,[Rn],Rm,LSR #i
void ldrbPtrpar(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PT(uint8_t, +, RD, RN, ASR) THUMB_SWITCH } // LDRB Rd,[Rn],Rm,ASR #i
void ldrbPtrprr(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PT(uint8_t, +, RD, RN, ROR) THUMB_SWITCH } // LDRB Rd,[Rn],Rm,ROR #i

void strOfrmll(interpreter::Cpu *cpu, uint32_t opcode) { STR_OF(uint32_t, -, RD, RN, LSL)              } // STR Rd,[Rn,-Rm,LSL #i]
void strOfrmlr(interpreter::Cpu *cpu, uint32_t opcode) { STR_OF(uint32_t, -, RD, RN, LSR)              } // STR Rd,[Rn,-Rm,LSR #i]
void strOfrmar(interpreter::Cpu *cpu, uint32_t opcode) { STR_OF(uint32_t, -, RD, RN, ASR)              } // STR Rd,[Rn,-Rm,ASR #i]
void strOfrmrr(interpreter::Cpu *cpu, uint32_t opcode) { STR_OF(uint32_t, -, RD, RN, ROR)              } // STR Rd,[Rn,-Rm,ROR #i]
void ldrOfrmll(interpreter::Cpu *cpu, uint32_t opcode) { LDR_OF(uint32_t, -, RD, RN, LSL) THUMB_SWITCH } // LDR Rd,[Rn,-Rm,LSL #i]
void ldrOfrmlr(interpreter::Cpu *cpu, uint32_t opcode) { LDR_OF(uint32_t, -, RD, RN, LSR) THUMB_SWITCH } // LDR Rd,[Rn,-Rm,LSR #i]
void ldrOfrmar(interpreter::Cpu *cpu, uint32_t opcode) { LDR_OF(uint32_t, -, RD, RN, ASR) THUMB_SWITCH } // LDR Rd,[Rn,-Rm,ASR #i]
void ldrOfrmrr(interpreter::Cpu *cpu, uint32_t opcode) { LDR_OF(uint32_t, -, RD, RN, ROR) THUMB_SWITCH } // LDR Rd,[Rn,-Rm,ROR #i]

void strPrrmll(interpreter::Cpu *cpu, uint32_t opcode) { STR_PR(uint32_t, -, RD, RN, LSL)              } // STR Rd,[Rn,-Rm,LSL #i]!
void strPrrmlr(interpreter::Cpu *cpu, uint32_t opcode) { STR_PR(uint32_t, -, RD, RN, LSR)              } // STR Rd,[Rn,-Rm,LSR #i]!
void strPrrmar(interpreter::Cpu *cpu, uint32_t opcode) { STR_PR(uint32_t, -, RD, RN, ASR)              } // STR Rd,[Rn,-Rm,ASR #i]!
void strPrrmrr(interpreter::Cpu *cpu, uint32_t opcode) { STR_PR(uint32_t, -, RD, RN, ROR)              } // STR Rd,[Rn,-Rm,ROR #i]!
void ldrPrrmll(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PR(uint32_t, -, RD, RN, LSL) THUMB_SWITCH } // LDR Rd,[Rn,-Rm,LSL #i]!
void ldrPrrmlr(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PR(uint32_t, -, RD, RN, LSR) THUMB_SWITCH } // LDR Rd,[Rn,-Rm,LSR #i]!
void ldrPrrmar(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PR(uint32_t, -, RD, RN, ASR) THUMB_SWITCH } // LDR Rd,[Rn,-Rm,ASR #i]!
void ldrPrrmrr(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PR(uint32_t, -, RD, RN, ROR) THUMB_SWITCH } // LDR Rd,[Rn,-Rm,ROR #i]!

void strbOfrmll(interpreter::Cpu *cpu, uint32_t opcode) { STR_OF(uint8_t, -, RD, RN, LSL)              } // STRB Rd,[Rn,-Rm,LSL #i]
void strbOfrmlr(interpreter::Cpu *cpu, uint32_t opcode) { STR_OF(uint8_t, -, RD, RN, LSR)              } // STRB Rd,[Rn,-Rm,LSR #i]
void strbOfrmar(interpreter::Cpu *cpu, uint32_t opcode) { STR_OF(uint8_t, -, RD, RN, ASR)              } // STRB Rd,[Rn,-Rm,ASR #i]
void strbOfrmrr(interpreter::Cpu *cpu, uint32_t opcode) { STR_OF(uint8_t, -, RD, RN, ROR)              } // STRB Rd,[Rn,-Rm,ROR #i]
void ldrbOfrmll(interpreter::Cpu *cpu, uint32_t opcode) { LDR_OF(uint8_t, -, RD, RN, LSL) THUMB_SWITCH } // LDRB Rd,[Rn,-Rm,LSL #i]
void ldrbOfrmlr(interpreter::Cpu *cpu, uint32_t opcode) { LDR_OF(uint8_t, -, RD, RN, LSR) THUMB_SWITCH } // LDRB Rd,[Rn,-Rm,LSR #i]
void ldrbOfrmar(interpreter::Cpu *cpu, uint32_t opcode) { LDR_OF(uint8_t, -, RD, RN, ASR) THUMB_SWITCH } // LDRB Rd,[Rn,-Rm,ASR #i]
void ldrbOfrmrr(interpreter::Cpu *cpu, uint32_t opcode) { LDR_OF(uint8_t, -, RD, RN, ROR) THUMB_SWITCH } // LDRB Rd,[Rn,-Rm,ROR #i]

void strbPrrmll(interpreter::Cpu *cpu, uint32_t opcode) { STR_PR(uint8_t, -, RD, RN, LSL)              } // STRB Rd,[Rn,-Rm,LSL #i]!
void strbPrrmlr(interpreter::Cpu *cpu, uint32_t opcode) { STR_PR(uint8_t, -, RD, RN, LSR)              } // STRB Rd,[Rn,-Rm,LSR #i]!
void strbPrrmar(interpreter::Cpu *cpu, uint32_t opcode) { STR_PR(uint8_t, -, RD, RN, ASR)              } // STRB Rd,[Rn,-Rm,ASR #i]!
void strbPrrmrr(interpreter::Cpu *cpu, uint32_t opcode) { STR_PR(uint8_t, -, RD, RN, ROR)              } // STRB Rd,[Rn,-Rm,ROR #i]!
void ldrbPrrmll(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PR(uint8_t, -, RD, RN, LSL) THUMB_SWITCH } // LDRB Rd,[Rn,-Rm,LSL #i]!
void ldrbPrrmlr(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PR(uint8_t, -, RD, RN, LSR) THUMB_SWITCH } // LDRB Rd,[Rn,-Rm,LSR #i]!
void ldrbPrrmar(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PR(uint8_t, -, RD, RN, ASR) THUMB_SWITCH } // LDRB Rd,[Rn,-Rm,ASR #i]!
void ldrbPrrmrr(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PR(uint8_t, -, RD, RN, ROR) THUMB_SWITCH } // LDRB Rd,[Rn,-Rm,ROR #i]!

void strOfrpll(interpreter::Cpu *cpu, uint32_t opcode) { STR_OF(uint32_t, +, RD, RN, LSL)              } // STR Rd,[Rn,Rm,LSL #i]
void strOfrplr(interpreter::Cpu *cpu, uint32_t opcode) { STR_OF(uint32_t, +, RD, RN, LSR)              } // STR Rd,[Rn,Rm,LSR #i]
void strOfrpar(interpreter::Cpu *cpu, uint32_t opcode) { STR_OF(uint32_t, +, RD, RN, ASR)              } // STR Rd,[Rn,Rm,ASR #i]
void strOfrprr(interpreter::Cpu *cpu, uint32_t opcode) { STR_OF(uint32_t, +, RD, RN, ROR)              } // STR Rd,[Rn,Rm,ROR #i]
void ldrOfrpll(interpreter::Cpu *cpu, uint32_t opcode) { LDR_OF(uint32_t, +, RD, RN, LSL) THUMB_SWITCH } // LDR Rd,[Rn,Rm,LSL #i]
void ldrOfrplr(interpreter::Cpu *cpu, uint32_t opcode) { LDR_OF(uint32_t, +, RD, RN, LSR) THUMB_SWITCH } // LDR Rd,[Rn,Rm,LSR #i]
void ldrOfrpar(interpreter::Cpu *cpu, uint32_t opcode) { LDR_OF(uint32_t, +, RD, RN, ASR) THUMB_SWITCH } // LDR Rd,[Rn,Rm,ASR #i]
void ldrOfrprr(interpreter::Cpu *cpu, uint32_t opcode) { LDR_OF(uint32_t, +, RD, RN, ROR) THUMB_SWITCH } // LDR Rd,[Rn,Rm,ROR #i]

void strPrrpll(interpreter::Cpu *cpu, uint32_t opcode) { STR_PR(uint32_t, +, RD, RN, LSL)              } // STR Rd,[Rn,Rm,LSL #i]!
void strPrrplr(interpreter::Cpu *cpu, uint32_t opcode) { STR_PR(uint32_t, +, RD, RN, LSR)              } // STR Rd,[Rn,Rm,LSR #i]!
void strPrrpar(interpreter::Cpu *cpu, uint32_t opcode) { STR_PR(uint32_t, +, RD, RN, ASR)              } // STR Rd,[Rn,Rm,ASR #i]!
void strPrrprr(interpreter::Cpu *cpu, uint32_t opcode) { STR_PR(uint32_t, +, RD, RN, ROR)              } // STR Rd,[Rn,Rm,ROR #i]!
void ldrPrrpll(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PR(uint32_t, +, RD, RN, LSL) THUMB_SWITCH } // LDR Rd,[Rn,Rm,LSL #i]!
void ldrPrrplr(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PR(uint32_t, +, RD, RN, LSR) THUMB_SWITCH } // LDR Rd,[Rn,Rm,LSR #i]!
void ldrPrrpar(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PR(uint32_t, +, RD, RN, ASR) THUMB_SWITCH } // LDR Rd,[Rn,Rm,ASR #i]!
void ldrPrrprr(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PR(uint32_t, +, RD, RN, ROR) THUMB_SWITCH } // LDR Rd,[Rn,Rm,ROR #i]!

void strbOfrpll(interpreter::Cpu *cpu, uint32_t opcode) { STR_OF(uint8_t, +, RD, RN, LSL)              } // STRB Rd,[Rn,Rm,LSL #i]
void strbOfrplr(interpreter::Cpu *cpu, uint32_t opcode) { STR_OF(uint8_t, +, RD, RN, LSR)              } // STRB Rd,[Rn,Rm,LSR #i]
void strbOfrpar(interpreter::Cpu *cpu, uint32_t opcode) { STR_OF(uint8_t, +, RD, RN, ASR)              } // STRB Rd,[Rn,Rm,ASR #i]
void strbOfrprr(interpreter::Cpu *cpu, uint32_t opcode) { STR_OF(uint8_t, +, RD, RN, ROR)              } // STRB Rd,[Rn,Rm,ROR #i]
void ldrbOfrpll(interpreter::Cpu *cpu, uint32_t opcode) { LDR_OF(uint8_t, +, RD, RN, LSL) THUMB_SWITCH } // LDRB Rd,[Rn,Rm,LSL #i]
void ldrbOfrplr(interpreter::Cpu *cpu, uint32_t opcode) { LDR_OF(uint8_t, +, RD, RN, LSR) THUMB_SWITCH } // LDRB Rd,[Rn,Rm,LSR #i]
void ldrbOfrpar(interpreter::Cpu *cpu, uint32_t opcode) { LDR_OF(uint8_t, +, RD, RN, ASR) THUMB_SWITCH } // LDRB Rd,[Rn,Rm,ASR #i]
void ldrbOfrprr(interpreter::Cpu *cpu, uint32_t opcode) { LDR_OF(uint8_t, +, RD, RN, ROR) THUMB_SWITCH } // LDRB Rd,[Rn,Rm,ROR #i]

void strbPrrpll(interpreter::Cpu *cpu, uint32_t opcode) { STR_PR(uint8_t, +, RD, RN, LSL)              } // STRB Rd,[Rn,Rm,LSL #i]!
void strbPrrplr(interpreter::Cpu *cpu, uint32_t opcode) { STR_PR(uint8_t, +, RD, RN, LSR)              } // STRB Rd,[Rn,Rm,LSR #i]!
void strbPrrpar(interpreter::Cpu *cpu, uint32_t opcode) { STR_PR(uint8_t, +, RD, RN, ASR)              } // STRB Rd,[Rn,Rm,ASR #i]!
void strbPrrprr(interpreter::Cpu *cpu, uint32_t opcode) { STR_PR(uint8_t, +, RD, RN, ROR)              } // STRB Rd,[Rn,Rm,ROR #i]!
void ldrbPrrpll(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PR(uint8_t, +, RD, RN, LSL) THUMB_SWITCH } // LDRB Rd,[Rn,Rm,LSL #i]!
void ldrbPrrplr(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PR(uint8_t, +, RD, RN, LSR) THUMB_SWITCH } // LDRB Rd,[Rn,Rm,LSR #i]!
void ldrbPrrpar(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PR(uint8_t, +, RD, RN, ASR) THUMB_SWITCH } // LDRB Rd,[Rn,Rm,ASR #i]!
void ldrbPrrprr(interpreter::Cpu *cpu, uint32_t opcode) { LDR_PR(uint8_t, +, RD, RN, ROR) THUMB_SWITCH } // LDRB Rd,[Rn,Rm,ROR #i]!

void stmda  (interpreter::Cpu *cpu, uint32_t opcode) { STMDA(RN, *cpu->registers,   (opcode & 0xFFFF))                            } // STMDA Rn, <Rlist>
void ldmda  (interpreter::Cpu *cpu, uint32_t opcode) { LDMDA(RN, *cpu->registers,   (opcode & 0xFFFF))               THUMB_SWITCH } // LDMDA Rn, <Rlist>
void stmdaW (interpreter::Cpu *cpu, uint32_t opcode) { STMDA(RN, *cpu->registers,   (opcode & 0xFFFF)) WRITEBACK(RN)              } // STMDA Rn!,<Rlist>
void ldmdaW (interpreter::Cpu *cpu, uint32_t opcode) { LDMDA(RN, *cpu->registers,   (opcode & 0xFFFF)) WRITEBACK(RN) THUMB_SWITCH } // LDMDA Rn!,<Rlist>
void stmdaU (interpreter::Cpu *cpu, uint32_t opcode) { STMDA(RN, cpu->registersUsr, (opcode & 0xFFFF))                            } // STMDA Rn, <Rlist>^
void ldmdaU (interpreter::Cpu *cpu, uint32_t opcode) { LDMDA(RN, cpu->registersUsr, (opcode & 0xFFFF))                MODE_SWITCH } // LDMDA Rn, <Rlist>^
void stmdaUW(interpreter::Cpu *cpu, uint32_t opcode) { STMDA(RN, cpu->registersUsr, (opcode & 0xFFFF)) WRITEBACK(RN)              } // STMDA Rn!,<Rlist>^
void ldmdaUW(interpreter::Cpu *cpu, uint32_t opcode) { LDMDA(RN, cpu->registersUsr, (opcode & 0xFFFF)) WRITEBACK(RN)  MODE_SWITCH } // LDMDA Rn!,<Rlist>^

void stmia  (interpreter::Cpu *cpu, uint32_t opcode) { STMIA(RN, *cpu->registers,   (opcode & 0xFFFF))                            } // STMIA Rn, <Rlist>
void ldmia  (interpreter::Cpu *cpu, uint32_t opcode) { LDMIA(RN, *cpu->registers,   (opcode & 0xFFFF))               THUMB_SWITCH } // LDMIA Rn, <Rlist>
void stmiaW (interpreter::Cpu *cpu, uint32_t opcode) { STMIA(RN, *cpu->registers,   (opcode & 0xFFFF)) WRITEBACK(RN)              } // STMIA Rn!,<Rlist>
void ldmiaW (interpreter::Cpu *cpu, uint32_t opcode) { LDMIA(RN, *cpu->registers,   (opcode & 0xFFFF)) WRITEBACK(RN) THUMB_SWITCH } // LDMIA Rn!,<Rlist>
void stmiaU (interpreter::Cpu *cpu, uint32_t opcode) { STMIA(RN, cpu->registersUsr, (opcode & 0xFFFF))                            } // STMIA Rn, <Rlist>^
void ldmiaU (interpreter::Cpu *cpu, uint32_t opcode) { LDMIA(RN, cpu->registersUsr, (opcode & 0xFFFF))                MODE_SWITCH } // LDMIA Rn, <Rlist>^
void stmiaUW(interpreter::Cpu *cpu, uint32_t opcode) { STMIA(RN, cpu->registersUsr, (opcode & 0xFFFF)) WRITEBACK(RN)              } // STMIA Rn!,<Rlist>^
void ldmiaUW(interpreter::Cpu *cpu, uint32_t opcode) { LDMIA(RN, cpu->registersUsr, (opcode & 0xFFFF)) WRITEBACK(RN)  MODE_SWITCH } // LDMIA Rn!,<Rlist>^

void stmdb  (interpreter::Cpu *cpu, uint32_t opcode) { STMDB(RN, *cpu->registers,   (opcode & 0xFFFF))                            } // STMDB Rn, <Rlist>
void ldmdb  (interpreter::Cpu *cpu, uint32_t opcode) { LDMDB(RN, *cpu->registers,   (opcode & 0xFFFF))               THUMB_SWITCH } // LDMDB Rn, <Rlist>
void stmdbW (interpreter::Cpu *cpu, uint32_t opcode) { STMDB(RN, *cpu->registers,   (opcode & 0xFFFF)) WRITEBACK(RN)              } // STMDB Rn!,<Rlist>
void ldmdbW (interpreter::Cpu *cpu, uint32_t opcode) { LDMDB(RN, *cpu->registers,   (opcode & 0xFFFF)) WRITEBACK(RN) THUMB_SWITCH } // LDMDB Rn!,<Rlist>
void stmdbU (interpreter::Cpu *cpu, uint32_t opcode) { STMDB(RN, cpu->registersUsr, (opcode & 0xFFFF))                            } // STMDB Rn, <Rlist>^
void ldmdbU (interpreter::Cpu *cpu, uint32_t opcode) { LDMDB(RN, cpu->registersUsr, (opcode & 0xFFFF))                MODE_SWITCH } // LDMDB Rn, <Rlist>^
void stmdbUW(interpreter::Cpu *cpu, uint32_t opcode) { STMDB(RN, cpu->registersUsr, (opcode & 0xFFFF)) WRITEBACK(RN)              } // STMDB Rn!,<Rlist>^
void ldmdbUW(interpreter::Cpu *cpu, uint32_t opcode) { LDMDB(RN, cpu->registersUsr, (opcode & 0xFFFF)) WRITEBACK(RN)  MODE_SWITCH } // LDMDB Rn!,<Rlist>^

void stmib  (interpreter::Cpu *cpu, uint32_t opcode) { STMIB(RN, *cpu->registers,   (opcode & 0xFFFF))                            } // STMIB Rn, <Rlist>
void ldmib  (interpreter::Cpu *cpu, uint32_t opcode) { LDMIB(RN, *cpu->registers,   (opcode & 0xFFFF))               THUMB_SWITCH } // LDMIB Rn, <Rlist>
void stmibW (interpreter::Cpu *cpu, uint32_t opcode) { STMIB(RN, *cpu->registers,   (opcode & 0xFFFF)) WRITEBACK(RN)              } // STMIB Rn!,<Rlist>
void ldmibW (interpreter::Cpu *cpu, uint32_t opcode) { LDMIB(RN, *cpu->registers,   (opcode & 0xFFFF)) WRITEBACK(RN) THUMB_SWITCH } // LDMIB Rn!,<Rlist>
void stmibU (interpreter::Cpu *cpu, uint32_t opcode) { STMIB(RN, cpu->registersUsr, (opcode & 0xFFFF))                            } // STMIB Rn, <Rlist>^
void ldmibU (interpreter::Cpu *cpu, uint32_t opcode) { LDMIB(RN, cpu->registersUsr, (opcode & 0xFFFF))                MODE_SWITCH } // LDMIB Rn, <Rlist>^
void stmibUW(interpreter::Cpu *cpu, uint32_t opcode) { STMIB(RN, cpu->registersUsr, (opcode & 0xFFFF)) WRITEBACK(RN)              } // STMIB Rn!,<Rlist>^
void ldmibUW(interpreter::Cpu *cpu, uint32_t opcode) { LDMIB(RN, cpu->registersUsr, (opcode & 0xFFFF)) WRITEBACK(RN)  MODE_SWITCH } // LDMIB Rn!,<Rlist>^

}

namespace interpreter_transfer_thumb
{

void ldrPc(interpreter::Cpu *cpu, uint32_t opcode) { LDR_OF(uint32_t, +, RD8_THUMB, (*cpu->registers[15] & ~BIT(1)), (IMM8_THUMB << 2)) } // LDR Rd,[PC,#i]

void strReg  (interpreter::Cpu *cpu, uint32_t opcode) { STR_OF(uint32_t, +, RD_THUMB, RB_THUMB, RO_THUMB) } // STR   Rd,[Rb,Ro]
void strhReg (interpreter::Cpu *cpu, uint32_t opcode) { STR_OF(uint16_t, +, RD_THUMB, RB_THUMB, RO_THUMB) } // STRH  Rd,[Rb,Ro]
void strbReg (interpreter::Cpu *cpu, uint32_t opcode) { STR_OF(uint8_t,  +, RD_THUMB, RB_THUMB, RO_THUMB) } // STRB  Rd,[Rb,Ro]
void ldrsbReg(interpreter::Cpu *cpu, uint32_t opcode) { LDR_OF(int8_t,   +, RD_THUMB, RB_THUMB, RO_THUMB) } // LDRSB Rd,[Rb,Ro]

void ldrReg  (interpreter::Cpu *cpu, uint32_t opcode) { LDR_OF(uint32_t, +, RD_THUMB, RB_THUMB, RO_THUMB) } // LDR   Rd,[Rb,Ro]
void ldrhReg (interpreter::Cpu *cpu, uint32_t opcode) { LDR_OF(uint16_t, +, RD_THUMB, RB_THUMB, RO_THUMB) } // LDRH  Rd,[Rb,Ro]
void ldrbReg (interpreter::Cpu *cpu, uint32_t opcode) { LDR_OF(uint8_t,  +, RD_THUMB, RB_THUMB, RO_THUMB) } // LDRB  Rd,[Rb,Ro]
void ldrshReg(interpreter::Cpu *cpu, uint32_t opcode) { LDR_OF(int16_t,  +, RD_THUMB, RB_THUMB, RO_THUMB) } // LDRSH Rd,[Rb,Ro]

void strImm5 (interpreter::Cpu *cpu, uint32_t opcode) { STR_OF(uint32_t, +, RD_THUMB, RB_THUMB, (IMM5_THUMB << 2)) } // STR  Rd,[Rb,#i]
void ldrImm5 (interpreter::Cpu *cpu, uint32_t opcode) { LDR_OF(uint32_t, +, RD_THUMB, RB_THUMB, (IMM5_THUMB << 2)) } // LDR  Rd,[Rb,#i]
void strbImm5(interpreter::Cpu *cpu, uint32_t opcode) { STR_OF(uint8_t,  +, RD_THUMB, RB_THUMB, (IMM5_THUMB << 0)) } // STRB Rd,[Rb,#i]
void ldrbImm5(interpreter::Cpu *cpu, uint32_t opcode) { LDR_OF(uint8_t,  +, RD_THUMB, RB_THUMB, (IMM5_THUMB << 0)) } // LDRB Rd,[Rb,#i]
void strhImm5(interpreter::Cpu *cpu, uint32_t opcode) { STR_OF(uint16_t, +, RD_THUMB, RB_THUMB, (IMM5_THUMB << 1)) } // STRH Rd,[Rb,#i]
void ldrhImm5(interpreter::Cpu *cpu, uint32_t opcode) { LDR_OF(uint16_t, +, RD_THUMB, RB_THUMB, (IMM5_THUMB << 1)) } // LDRH Rd,[Rb,#i]

void strSp(interpreter::Cpu *cpu, uint32_t opcode) { STR_OF(uint32_t, +, RD8_THUMB, *cpu->registers[13], (IMM8_THUMB << 2)) } // STR Rd,[SP,#i]
void ldrSp(interpreter::Cpu *cpu, uint32_t opcode) { LDR_OF(uint32_t, +, RD8_THUMB, *cpu->registers[13], (IMM8_THUMB << 2)) } // LDR Rd,[SP,#i]

void push  (interpreter::Cpu *cpu, uint32_t opcode) { STMDB(*cpu->registers[13], *cpu->registers,            (opcode & 0xFF))  WRITEBACK(*cpu->registers[13]) } // PUSH <Rlist>
void pushLr(interpreter::Cpu *cpu, uint32_t opcode) { STMDB(*cpu->registers[13], *cpu->registers, (BIT(14) | (opcode & 0xFF))) WRITEBACK(*cpu->registers[13]) } // PUSH <Rlist>,LR

void pop  (interpreter::Cpu *cpu, uint32_t opcode) { LDMIA(*cpu->registers[13], *cpu->registers,            (opcode & 0xFF))  WRITEBACK(*cpu->registers[13])            } // POP <Rlist>
void popPc(interpreter::Cpu *cpu, uint32_t opcode) { LDMIA(*cpu->registers[13], *cpu->registers, (BIT(15) | (opcode & 0xFF))) WRITEBACK(*cpu->registers[13]) ARM_SWITCH } // POP <Rlist>,PC

void stmia(interpreter::Cpu *cpu, uint32_t opcode) { STMIA(RD8_THUMB, *cpu->registers, (opcode & 0xFF)) WRITEBACK(RD8_THUMB) } // STMIA Rb!,<Rlist>
void ldmia(interpreter::Cpu *cpu, uint32_t opcode) { LDMIA(RD8_THUMB, *cpu->registers, (opcode & 0xFF)) WRITEBACK(RD8_THUMB) } // LDMIA Rb!,<Rlist>

}
