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

#include "interpreter_alu.h"
#include "core.h"

// Register values for ARM opcodes
// When used as Rm, the program counter is read as PC+12 if shifted by a register, or the usual PC+8 if not
#define RN  (*cpu->registers[(opcode & 0x000F0000) >> 16])
#define RD  (*cpu->registers[(opcode & 0x0000F000) >> 12])
#define RS  (*cpu->registers[(opcode & 0x00000F00) >> 8])
#define RMI (*cpu->registers[(opcode & 0x0000000F)])
#define RMR (*cpu->registers[(opcode & 0x0000000F)] + (((opcode & 0x0000000F) == 0xF) ? 4 : 0))

// Immediate values for ARM opcodes
#define IMM       (opcode & 0x000000FF)
#define IMM_SHIFT ((opcode & 0x00000F00) >> 7)
#define REG_SHIFT ((opcode & 0x00000F80) >> 7)

// Register values for THUMB opcodes
#define RN_THUMB  (*cpu->registers[(opcode & 0x01C0) >> 6])
#define RS_THUMB  (*cpu->registers[(opcode & 0x0038) >> 3])
#define RD_THUMB  (*cpu->registers[(opcode & 0x0007)])
#define RD8_THUMB (*cpu->registers[(opcode & 0x0700) >> 8])
#define RSH_THUMB (*cpu->registers[(opcode & 0x0078) >> 3])
#define RDH_THUMB (*cpu->registers[(opcode & 0x0007) | ((opcode & 0x0080) >> 4)])

// Immediate values for THUMB opcodes
#define IMM3_THUMB ((opcode & 0x01C0) >> 6)
#define IMM5_THUMB ((opcode & 0x07C0) >> 6)
#define IMM7_THUMB ((opcode & BIT(7)) ? (0 - (opcode & 0x007F)) : (opcode & 0x007F))
#define IMM8_THUMB (opcode & 0x00FF)
#define DP_SWITCH  ((opcode & 0x00C0) >> 6)

// Shifts for immediate values
// A shift amount of 0 for LSR and ASR translates to a shift of 32
// A shift amount of 0 for ROR translates to RCR (rotate with carry) 1
#define LLI(value, amount) (value << amount)
#define LRI(value, amount) (amount ? (value >> amount) : 0)
#define ARI(value, amount) (amount ? ((int32_t)value >> amount) : ((value & BIT(31)) ? 0xFFFFFFFF : 0))
#define RRI(value, amount) (amount ? ((value << (32 - amount)) | (value >> amount)) : (((cpu->cpsr & BIT(29)) << 2) | (value >> 1)))

// Shifts for registers
// Only the lower 8 bits are used
// Shifts greater than 32 can be wonky on target systems, so they're handled explicitly
#define LLR(value, amount) (((amount & 0xFF) < 32) ? (value << (amount & 0xFF)) : 0)
#define LRR(value, amount) (((amount & 0xFF) < 32) ? (value >> (amount & 0xFF)) : 0)
#define ARR(value, amount) (((amount & 0xFF) < 32) ? ((int32_t)value >> (amount & 0xFF)) : ((value & BIT(31)) ? 0xFFFFFFFF : 0))
#define RRR(value, amount) ((value << (32 - (amount & 0xFF) % 32)) | (value >> ((amount & 0xFF) % 32)))

// If the program counter is the destination, set bit 0 to indicate a jump
#define PC_PIPELINE(op0)            \
    if (&op0 == cpu->registers[15]) \
        op0 |= BIT(0);

// Bitwise and
#define AND(op0, op1, op2) \
    op0 = op1 & op2;       \
    PC_PIPELINE(op0)

// Bitwise exclusive or
#define EOR(op0, op1, op2) \
    op0 = op1 ^ op2;       \
    PC_PIPELINE(op0)

// Subtraction
#define SUB(op0, op1, op2) \
    uint32_t pre = op1;    \
    uint32_t sub = op2;    \
    op0 = pre - sub;       \
    PC_PIPELINE(op0)

// Reverse subtraction
#define RSB(op0, op1, op2) \
    uint32_t pre = op2;    \
    uint32_t sub = op1;    \
    op0 = pre - sub;       \
    PC_PIPELINE(op0)

// Addition
#define ADD(op0, op1, op2) \
    uint32_t pre = op1;    \
    uint32_t add = op2;    \
    op0 = pre + add;       \
    PC_PIPELINE(op0)

// Addition with carry
#define ADC(op0, op1, op2)                           \
    uint32_t pre = op1;                              \
    uint32_t add = op2;                              \
    op0 = pre + add + ((cpu->cpsr & BIT(29)) >> 29); \
    PC_PIPELINE(op0)

// Subtraction with carry
#define SBC(op0, op1, op2)                               \
    uint32_t pre = op1;                                  \
    uint32_t sub = op2;                                  \
    op0 = pre - sub - 1 + ((cpu->cpsr & BIT(29)) >> 29); \
    PC_PIPELINE(op0)

// Reverse subtraction with carry
#define RSC(op0, op1, op2)                               \
    uint32_t pre = op2;                                  \
    uint32_t sub = op1;                                  \
    op0 = pre - sub - 1 + ((cpu->cpsr & BIT(29)) >> 29); \
    PC_PIPELINE(op0)

// Test bits
#define TST(op1, op2)         \
    uint32_t res = op1 & op2; \
    COMMON_FLAGS(res)

// Test equivalence
#define TEQ(op1, op2)         \
    uint32_t res = op1 ^ op2; \
    COMMON_FLAGS(res)

// Compare
#define CMP(op1, op2)         \
    uint32_t pre = op1;       \
    uint32_t sub = op2;       \
    uint32_t res = pre - sub; \
    SUB_FLAGS(res)

// Compare negative
#define CMN(op1, op2)         \
    uint32_t pre = op1;       \
    uint32_t add = op2;       \
    uint32_t res = pre + add; \
    ADD_FLAGS(res)

// Bitwise or
#define ORR(op0, op1, op2) \
    op0 = op1 | op2;       \
    PC_PIPELINE(op0)

// Move
#define MOV(op0, op2) \
    op0 = op2;        \
    PC_PIPELINE(op0)

// Bit clear
#define BIC(op0, op1, op2) \
    op0 = op1 & ~op2;      \
    PC_PIPELINE(op0)

// Move not
#define MVN(op0, op2) \
    op0 = ~op2;       \
    PC_PIPELINE(op0)

// Multiply
#define MUL(op0, op1, op2) \
    op0 = op1 * op2;

// Multiply and accumulate
#define MLA(op0, op1, op2, op3) \
    op0 = op1 * op2 + op3;

// Unsigned long multiply
#define UMULL(op0, op1, op2, op3)       \
    uint64_t res = (uint64_t)op2 * op3; \
    op1 = res >> 32;                    \
    op0 = res;

// Unsigned long multiply and accumulate
#define UMLAL(op0, op1, op2, op3)       \
    uint64_t res = (uint64_t)op2 * op3; \
    res += ((uint64_t)op1 << 32) | op0; \
    op1 = res >> 32;                    \
    op0 = res;

// Signed long multiply
#define SMULL(op0, op1, op2, op3) \
    int64_t res = (int32_t)op2;   \
    res *= (int32_t)op3;          \
    op1 = res >> 32;              \
    op0 = res;

// Signed long multiply and accumulate
#define SMLAL(op0, op1, op2, op3)      \
    int64_t res = (int32_t)op2;        \
    res *= (int32_t)op3;               \
    res += ((int64_t)op1 << 32) | op0; \
    op1 = res >> 32;                   \
    op0 = res;

// Set or clear a flag bit
#define SET_FLAG(bit, cond) if (cond) cpu->cpsr |= BIT(bit); else cpu->cpsr &= ~BIT(bit);

// Get an LLI shifted value and set the carry flag
#define LLI_CARRY(value, amount)                                 \
    uint32_t lli = LLI(value, amount);                           \
    if (amount > 0)                                              \
    {                                                            \
        SET_FLAG(29, amount <= 32 && (value & BIT(32 - amount))) \
    }

// Get an LRI shifted value and set the carry flag
#define LRI_CARRY(value, amount)                            \
    uint32_t lri = LRI(value, amount);                      \
    SET_FLAG(29, (value & BIT(amount ? (amount - 1) : 31)))

// Get an ARI shifted value and set the carry flag
#define ARI_CARRY(value, amount)                                                                  \
    uint32_t ari = ARI(value, amount);                                                            \
    SET_FLAG(29, (amount == 0 && (value & BIT(31))) || (amount > 0 && (value & BIT(amount - 1))))

// Get an RRI shifted value and set the carry flag
#define RRI_CARRY(value, amount)                           \
    uint32_t rri = RRI(value, amount);                     \
    SET_FLAG(29, (value & BIT(amount ? (amount - 1) : 0)))

// Get an LLR shifted value and set the carry flag
#define LLR_CARRY(value, amount)                                                   \
    uint32_t llr = LLR(value, amount);                                             \
    if ((amount & 0xFF) > 0)                                                       \
    {                                                                              \
        SET_FLAG(29, (amount & 0xFF) <= 32 && (value & BIT(32 - (amount & 0xFF)))) \
    }

// Get an LRR shifted value and set the carry flag
#define LRR_CARRY(value, amount)                                                  \
    uint32_t lrr = LRR(value, amount);                                            \
    if ((amount & 0xFF) > 0)                                                      \
    {                                                                             \
        SET_FLAG(29, (amount & 0xFF) <= 32 && (value & BIT((amount & 0xFF) - 1))) \
    }

// Get an ARR shifted value and set the carry flag
#define ARR_CARRY(value, amount)                                                                                                   \
    uint32_t arr = ARR(value, amount);                                                                                             \
    if ((amount & 0xFF) > 0)                                                                                                       \
    {                                                                                                                              \
        SET_FLAG(29, ((amount & 0xFF) > 32 && (value & BIT(31))) || ((amount & 0xFF) <= 32 && (value & BIT((amount & 0xFF) - 1)))) \
    }

// Get an RRR shifted value and set the carry flag
#define RRR_CARRY(value, amount)                                \
    uint32_t rrr = RRR(value, amount);                          \
    if ((amount & 0xFF) > 0)                                    \
    {                                                           \
        SET_FLAG(29, (value & BIT(((amount & 0xFF) - 1) % 32))) \
    }

// Set the N and Z flags after an operation was performed
// Also return to the previous CPU mode if the result was stored in the program counter
#define COMMON_FLAGS(dst)                                   \
    if(&dst == cpu->registers[15] && cpu->spsr)             \
    {                                                       \
        cpu->cpsr = *cpu->spsr & ~0x0000001F;               \
        interpreter::setMode(cpu, *cpu->spsr & 0x0000001F); \
        return;                                             \
    }                                                       \
    SET_FLAG(31, dst & BIT(31))                             \
    SET_FLAG(30, dst == 0)

// Set the C and V flags for a subtraction operation
#define SUB_FLAGS(dst)                                                                     \
    COMMON_FLAGS(dst)                                                                      \
    SET_FLAG(29, pre >= dst)                                                               \
    SET_FLAG(28, (sub & BIT(31)) != (pre & BIT(31)) && (dst & BIT(31)) == (sub & BIT(31)))

// Set the C and V flags for an addition operation
#define ADD_FLAGS(dst)                                                                     \
    COMMON_FLAGS(dst)                                                                      \
    SET_FLAG(29, pre > dst)                                                                \
    SET_FLAG(28, (add & BIT(31)) == (pre & BIT(31)) && (dst & BIT(31)) != (add & BIT(31)))

// Set the C and V flags for an addition with carry operation
#define ADC_FLAGS(dst)                                                                     \
    COMMON_FLAGS(dst)                                                                      \
    SET_FLAG(29, pre > dst || (add == 0xFFFFFFFF && (cpu->cpsr & BIT(29))))                \
    SET_FLAG(28, (add & BIT(31)) == (pre & BIT(31)) && (dst & BIT(31)) != (add & BIT(31)))

// Set the C and V flags for a subtraction with carry operation
#define SBC_FLAGS(dst)                                                                     \
    COMMON_FLAGS(dst)                                                                      \
    SET_FLAG(29, pre >= dst && (sub != 0xFFFFFFFF || (cpu->cpsr & BIT(29))))               \
    SET_FLAG(28, (sub & BIT(31)) != (pre & BIT(31)) && (dst & BIT(31)) == (sub & BIT(31)))

// Set the flags for a multiplication operation
// Just sets the common flags, but on ARM7 the C flag is destroyed
#define MUL_FLAGS(dst)         \
    COMMON_FLAGS(dst)          \
    if (cpu->type == 7)        \
        cpu->cpsr &= ~BIT(29);

// Below, the above macros are combined to form all the variations of the ALU instructions

namespace interpreter_alu
{

void andLli(interpreter::Cpu *cpu, uint32_t opcode) { AND(RD, RN, LLI(RMI, REG_SHIFT)) } // AND Rd,Rn,Rm,LSL #i
void andLlr(interpreter::Cpu *cpu, uint32_t opcode) { AND(RD, RN, LLR(RMR, RS))        } // AND Rd,Rn,Rm,LSL Rs
void andLri(interpreter::Cpu *cpu, uint32_t opcode) { AND(RD, RN, LRI(RMI, REG_SHIFT)) } // AND Rd,Rn,Rm,LSR #i
void andLrr(interpreter::Cpu *cpu, uint32_t opcode) { AND(RD, RN, LRR(RMR, RS))        } // AND Rd,Rn,Rm,LSR Rs
void andAri(interpreter::Cpu *cpu, uint32_t opcode) { AND(RD, RN, ARI(RMI, REG_SHIFT)) } // AND Rd,Rn,Rm,ASR #i
void andArr(interpreter::Cpu *cpu, uint32_t opcode) { AND(RD, RN, ARR(RMR, RS))        } // AND Rd,Rn,Rm,ASR Rs
void andRri(interpreter::Cpu *cpu, uint32_t opcode) { AND(RD, RN, RRI(RMI, REG_SHIFT)) } // AND Rd,Rn,Rm,ROR #i
void andRrr(interpreter::Cpu *cpu, uint32_t opcode) { AND(RD, RN, RRR(RMR, RS))        } // AND Rd,Rn,Rm,ROR Rs

void mul(interpreter::Cpu *cpu, uint32_t opcode) { MUL(RN, RMR, RS) } // MUL Rd,Rm,Rs

void andsLli(interpreter::Cpu *cpu, uint32_t opcode) { LLI_CARRY(RMI, REG_SHIFT) AND(RD, RN, lli) COMMON_FLAGS(RD) } // ANDS Rd,Rn,Rm,LSL #i
void andsLlr(interpreter::Cpu *cpu, uint32_t opcode) { LLR_CARRY(RMR, RS)        AND(RD, RN, llr) COMMON_FLAGS(RD) } // ANDS Rd,Rn,Rm,LSL Rs
void andsLri(interpreter::Cpu *cpu, uint32_t opcode) { LRI_CARRY(RMI, REG_SHIFT) AND(RD, RN, lri) COMMON_FLAGS(RD) } // ANDS Rd,Rn,Rm,LSR #i
void andsLrr(interpreter::Cpu *cpu, uint32_t opcode) { LRR_CARRY(RMR, RS)        AND(RD, RN, lrr) COMMON_FLAGS(RD) } // ANDS Rd,Rn,Rm,LSR Rs
void andsAri(interpreter::Cpu *cpu, uint32_t opcode) { ARI_CARRY(RMI, REG_SHIFT) AND(RD, RN, ari) COMMON_FLAGS(RD) } // ANDS Rd,Rn,Rm,ASR #i
void andsArr(interpreter::Cpu *cpu, uint32_t opcode) { ARR_CARRY(RMR, RS)        AND(RD, RN, arr) COMMON_FLAGS(RD) } // ANDS Rd,Rn,Rm,ASR Rs
void andsRri(interpreter::Cpu *cpu, uint32_t opcode) { RRI_CARRY(RMI, REG_SHIFT) AND(RD, RN, rri) COMMON_FLAGS(RD) } // ANDS Rd,Rn,Rm,ROR #i
void andsRrr(interpreter::Cpu *cpu, uint32_t opcode) { RRR_CARRY(RMR, RS)        AND(RD, RN, rrr) COMMON_FLAGS(RD) } // ANDS Rd,Rn,Rm,ROR Rs

void muls(interpreter::Cpu *cpu, uint32_t opcode) { MUL(RN, RMR, RS) MUL_FLAGS(RN) } // MULS Rd,Rm,Rs

void eorLli(interpreter::Cpu *cpu, uint32_t opcode) { EOR(RD, RN, LLI(RMI, REG_SHIFT)) } // EOR Rd,Rn,Rm,LSL #i
void eorLlr(interpreter::Cpu *cpu, uint32_t opcode) { EOR(RD, RN, LLR(RMR, RS))        } // EOR Rd,Rn,Rm,LSL Rs
void eorLri(interpreter::Cpu *cpu, uint32_t opcode) { EOR(RD, RN, LRI(RMI, REG_SHIFT)) } // EOR Rd,Rn,Rm,LSR #i
void eorLrr(interpreter::Cpu *cpu, uint32_t opcode) { EOR(RD, RN, LRR(RMR, RS))        } // EOR Rd,Rn,Rm,LSR Rs
void eorAri(interpreter::Cpu *cpu, uint32_t opcode) { EOR(RD, RN, ARI(RMI, REG_SHIFT)) } // EOR Rd,Rn,Rm,ASR #i
void eorArr(interpreter::Cpu *cpu, uint32_t opcode) { EOR(RD, RN, ARR(RMR, RS))        } // EOR Rd,Rn,Rm,ASR Rs
void eorRri(interpreter::Cpu *cpu, uint32_t opcode) { EOR(RD, RN, RRI(RMI, REG_SHIFT)) } // EOR Rd,Rn,Rm,ROR #i
void eorRrr(interpreter::Cpu *cpu, uint32_t opcode) { EOR(RD, RN, RRR(RMR, RS))        } // EOR Rd,Rn,Rm,ROR Rs

void mla(interpreter::Cpu *cpu, uint32_t opcode) { MLA(RN, RMR, RS, RD) } // MLA Rd,Rm,Rs,Rn

void eorsLli(interpreter::Cpu *cpu, uint32_t opcode) { LLI_CARRY(RMI, REG_SHIFT) EOR(RD, RN, lli) COMMON_FLAGS(RD) } // EORS Rd,Rn,Rm,LSL #i
void eorsLlr(interpreter::Cpu *cpu, uint32_t opcode) { LLR_CARRY(RMR, RS)        EOR(RD, RN, llr) COMMON_FLAGS(RD) } // EORS Rd,Rn,Rm,LSL Rs
void eorsLri(interpreter::Cpu *cpu, uint32_t opcode) { LRI_CARRY(RMI, REG_SHIFT) EOR(RD, RN, lri) COMMON_FLAGS(RD) } // EORS Rd,Rn,Rm,LSR #i
void eorsLrr(interpreter::Cpu *cpu, uint32_t opcode) { LRR_CARRY(RMR, RS)        EOR(RD, RN, lrr) COMMON_FLAGS(RD) } // EORS Rd,Rn,Rm,LSR Rs
void eorsAri(interpreter::Cpu *cpu, uint32_t opcode) { ARI_CARRY(RMI, REG_SHIFT) EOR(RD, RN, ari) COMMON_FLAGS(RD) } // EORS Rd,Rn,Rm,ASR #i
void eorsArr(interpreter::Cpu *cpu, uint32_t opcode) { ARR_CARRY(RMR, RS)        EOR(RD, RN, arr) COMMON_FLAGS(RD) } // EORS Rd,Rn,Rm,ASR Rs
void eorsRri(interpreter::Cpu *cpu, uint32_t opcode) { RRI_CARRY(RMI, REG_SHIFT) EOR(RD, RN, rri) COMMON_FLAGS(RD) } // EORS Rd,Rn,Rm,ROR #i
void eorsRrr(interpreter::Cpu *cpu, uint32_t opcode) { RRR_CARRY(RMR, RS)        EOR(RD, RN, rrr) COMMON_FLAGS(RD) } // EORS Rd,Rn,Rm,ROR Rs

void mlas(interpreter::Cpu *cpu, uint32_t opcode) { MLA(RN, RMR, RS, RD) MUL_FLAGS(RN) } // MLAS Rd,Rm,Rs,Rn

void subLli(interpreter::Cpu *cpu, uint32_t opcode) { SUB(RD, RN, LLI(RMI, REG_SHIFT)) } // SUB Rd,Rn,Rm,LSL #i
void subLlr(interpreter::Cpu *cpu, uint32_t opcode) { SUB(RD, RN, LLR(RMR, RS))        } // SUB Rd,Rn,Rm,LSL Rs
void subLri(interpreter::Cpu *cpu, uint32_t opcode) { SUB(RD, RN, LRI(RMI, REG_SHIFT)) } // SUB Rd,Rn,Rm,LSR #i
void subLrr(interpreter::Cpu *cpu, uint32_t opcode) { SUB(RD, RN, LRR(RMR, RS))        } // SUB Rd,Rn,Rm,LSR Rs
void subAri(interpreter::Cpu *cpu, uint32_t opcode) { SUB(RD, RN, ARI(RMI, REG_SHIFT)) } // SUB Rd,Rn,Rm,ASR #i
void subArr(interpreter::Cpu *cpu, uint32_t opcode) { SUB(RD, RN, ARR(RMR, RS))        } // SUB Rd,Rn,Rm,ASR Rs
void subRri(interpreter::Cpu *cpu, uint32_t opcode) { SUB(RD, RN, RRI(RMI, REG_SHIFT)) } // SUB Rd,Rn,Rm,ROR #i
void subRrr(interpreter::Cpu *cpu, uint32_t opcode) { SUB(RD, RN, RRR(RMR, RS))        } // SUB Rd,Rn,Rm,ROR Rs

void subsLli(interpreter::Cpu *cpu, uint32_t opcode) { SUB(RD, RN, LLI(RMI, REG_SHIFT)) SUB_FLAGS(RD) } // SUBS Rd,Rn,Rm,LSL #i
void subsLlr(interpreter::Cpu *cpu, uint32_t opcode) { SUB(RD, RN, LLR(RMR, RS))        SUB_FLAGS(RD) } // SUBS Rd,Rn,Rm,LSL Rs
void subsLri(interpreter::Cpu *cpu, uint32_t opcode) { SUB(RD, RN, LRI(RMI, REG_SHIFT)) SUB_FLAGS(RD) } // SUBS Rd,Rn,Rm,LSR #i
void subsLrr(interpreter::Cpu *cpu, uint32_t opcode) { SUB(RD, RN, LRR(RMR, RS))        SUB_FLAGS(RD) } // SUBS Rd,Rn,Rm,LSR Rs
void subsAri(interpreter::Cpu *cpu, uint32_t opcode) { SUB(RD, RN, ARI(RMI, REG_SHIFT)) SUB_FLAGS(RD) } // SUBS Rd,Rn,Rm,ASR #i
void subsArr(interpreter::Cpu *cpu, uint32_t opcode) { SUB(RD, RN, ARR(RMR, RS))        SUB_FLAGS(RD) } // SUBS Rd,Rn,Rm,ASR Rs
void subsRri(interpreter::Cpu *cpu, uint32_t opcode) { SUB(RD, RN, RRI(RMI, REG_SHIFT)) SUB_FLAGS(RD) } // SUBS Rd,Rn,Rm,ROR #i
void subsRrr(interpreter::Cpu *cpu, uint32_t opcode) { SUB(RD, RN, RRR(RMR, RS))        SUB_FLAGS(RD) } // SUBS Rd,Rn,Rm,ROR Rs

void rsbLli(interpreter::Cpu *cpu, uint32_t opcode) { RSB(RD, RN, LLI(RMI, REG_SHIFT)) } // RSB Rd,Rn,Rm,LSL #i
void rsbLlr(interpreter::Cpu *cpu, uint32_t opcode) { RSB(RD, RN, LLR(RMR, RS))        } // RSB Rd,Rn,Rm,LSL Rs
void rsbLri(interpreter::Cpu *cpu, uint32_t opcode) { RSB(RD, RN, LRI(RMI, REG_SHIFT)) } // RSB Rd,Rn,Rm,LSR #i
void rsbLrr(interpreter::Cpu *cpu, uint32_t opcode) { RSB(RD, RN, LRR(RMR, RS))        } // RSB Rd,Rn,Rm,LSR Rs
void rsbAri(interpreter::Cpu *cpu, uint32_t opcode) { RSB(RD, RN, ARI(RMI, REG_SHIFT)) } // RSB Rd,Rn,Rm,ASR #i
void rsbArr(interpreter::Cpu *cpu, uint32_t opcode) { RSB(RD, RN, ARR(RMR, RS))        } // RSB Rd,Rn,Rm,ASR Rs
void rsbRri(interpreter::Cpu *cpu, uint32_t opcode) { RSB(RD, RN, RRI(RMI, REG_SHIFT)) } // RSB Rd,Rn,Rm,ROR #i
void rsbRrr(interpreter::Cpu *cpu, uint32_t opcode) { RSB(RD, RN, RRR(RMR, RS))        } // RSB Rd,Rn,Rm,ROR Rs

void rsbsLli(interpreter::Cpu *cpu, uint32_t opcode) { RSB(RD, RN, LLI(RMI, REG_SHIFT)) SUB_FLAGS(RD) } // RSBS Rd,Rn,Rm,LSL #i
void rsbsLlr(interpreter::Cpu *cpu, uint32_t opcode) { RSB(RD, RN, LLR(RMR, RS))        SUB_FLAGS(RD) } // RSBS Rd,Rn,Rm,LSL Rs
void rsbsLri(interpreter::Cpu *cpu, uint32_t opcode) { RSB(RD, RN, LRI(RMI, REG_SHIFT)) SUB_FLAGS(RD) } // RSBS Rd,Rn,Rm,LSR #i
void rsbsLrr(interpreter::Cpu *cpu, uint32_t opcode) { RSB(RD, RN, LRR(RMR, RS))        SUB_FLAGS(RD) } // RSBS Rd,Rn,Rm,LSR Rs
void rsbsAri(interpreter::Cpu *cpu, uint32_t opcode) { RSB(RD, RN, ARI(RMI, REG_SHIFT)) SUB_FLAGS(RD) } // RSBS Rd,Rn,Rm,ASR #i
void rsbsArr(interpreter::Cpu *cpu, uint32_t opcode) { RSB(RD, RN, ARR(RMR, RS))        SUB_FLAGS(RD) } // RSBS Rd,Rn,Rm,ASR Rs
void rsbsRri(interpreter::Cpu *cpu, uint32_t opcode) { RSB(RD, RN, RRI(RMI, REG_SHIFT)) SUB_FLAGS(RD) } // RSBS Rd,Rn,Rm,ROR #i
void rsbsRrr(interpreter::Cpu *cpu, uint32_t opcode) { RSB(RD, RN, RRR(RMR, RS))        SUB_FLAGS(RD) } // RSBS Rd,Rn,Rm,ROR Rs

void addLli(interpreter::Cpu *cpu, uint32_t opcode) { ADD(RD, RN, LLI(RMI, REG_SHIFT)) } // ADD Rd,Rn,Rm,LSL #i
void addLlr(interpreter::Cpu *cpu, uint32_t opcode) { ADD(RD, RN, LLR(RMR, RS))        } // ADD Rd,Rn,Rm,LSL Rs
void addLri(interpreter::Cpu *cpu, uint32_t opcode) { ADD(RD, RN, LRI(RMI, REG_SHIFT)) } // ADD Rd,Rn,Rm,LSR #i
void addLrr(interpreter::Cpu *cpu, uint32_t opcode) { ADD(RD, RN, LRR(RMR, RS))        } // ADD Rd,Rn,Rm,LSR Rs
void addAri(interpreter::Cpu *cpu, uint32_t opcode) { ADD(RD, RN, ARI(RMI, REG_SHIFT)) } // ADD Rd,Rn,Rm,ASR #i
void addArr(interpreter::Cpu *cpu, uint32_t opcode) { ADD(RD, RN, ARR(RMR, RS))        } // ADD Rd,Rn,Rm,ASR Rs
void addRri(interpreter::Cpu *cpu, uint32_t opcode) { ADD(RD, RN, RRI(RMI, REG_SHIFT)) } // ADD Rd,Rn,Rm,ROR #i
void addRrr(interpreter::Cpu *cpu, uint32_t opcode) { ADD(RD, RN, RRR(RMR, RS))        } // ADD Rd,Rn,Rm,ROR Rs

void umull(interpreter::Cpu *cpu, uint32_t opcode) { UMULL(RD, RN, RMR, RS) } // UMULL RdLo,RdHi,Rm,Rs

void addsLli(interpreter::Cpu *cpu, uint32_t opcode) { ADD(RD, RN, LLI(RMI, REG_SHIFT)) ADD_FLAGS(RD) } // ADDS Rd,Rn,Rm,LSL #i
void addsLlr(interpreter::Cpu *cpu, uint32_t opcode) { ADD(RD, RN, LLR(RMR, RS))        ADD_FLAGS(RD) } // ADDS Rd,Rn,Rm,LSL Rs
void addsLri(interpreter::Cpu *cpu, uint32_t opcode) { ADD(RD, RN, LRI(RMI, REG_SHIFT)) ADD_FLAGS(RD) } // ADDS Rd,Rn,Rm,LSR #i
void addsLrr(interpreter::Cpu *cpu, uint32_t opcode) { ADD(RD, RN, LRR(RMR, RS))        ADD_FLAGS(RD) } // ADDS Rd,Rn,Rm,LSR Rs
void addsAri(interpreter::Cpu *cpu, uint32_t opcode) { ADD(RD, RN, ARI(RMI, REG_SHIFT)) ADD_FLAGS(RD) } // ADDS Rd,Rn,Rm,ASR #i
void addsArr(interpreter::Cpu *cpu, uint32_t opcode) { ADD(RD, RN, ARR(RMR, RS))        ADD_FLAGS(RD) } // ADDS Rd,Rn,Rm,ASR Rs
void addsRri(interpreter::Cpu *cpu, uint32_t opcode) { ADD(RD, RN, RRI(RMI, REG_SHIFT)) ADD_FLAGS(RD) } // ADDS Rd,Rn,Rm,ROR #i
void addsRrr(interpreter::Cpu *cpu, uint32_t opcode) { ADD(RD, RN, RRR(RMR, RS))        ADD_FLAGS(RD) } // ADDS Rd,Rn,Rm,ROR Rs

void umulls(interpreter::Cpu *cpu, uint32_t opcode) { UMULL(RD, RN, RMR, RS) MUL_FLAGS(RN) } // UMULLS RdLo,RdHi,Rm,Rs

void adcLli(interpreter::Cpu *cpu, uint32_t opcode) { ADC(RD, RN, LLI(RMI, REG_SHIFT)) } // ADC Rd,Rn,Rm,LSL #i
void adcLlr(interpreter::Cpu *cpu, uint32_t opcode) { ADC(RD, RN, LLR(RMR, RS))        } // ADC Rd,Rn,Rm,LSL Rs
void adcLri(interpreter::Cpu *cpu, uint32_t opcode) { ADC(RD, RN, LRI(RMI, REG_SHIFT)) } // ADC Rd,Rn,Rm,LSR #i
void adcLrr(interpreter::Cpu *cpu, uint32_t opcode) { ADC(RD, RN, LRR(RMR, RS))        } // ADC Rd,Rn,Rm,LSR Rs
void adcAri(interpreter::Cpu *cpu, uint32_t opcode) { ADC(RD, RN, ARI(RMI, REG_SHIFT)) } // ADC Rd,Rn,Rm,ASR #i
void adcArr(interpreter::Cpu *cpu, uint32_t opcode) { ADC(RD, RN, ARR(RMR, RS))        } // ADC Rd,Rn,Rm,ASR Rs
void adcRri(interpreter::Cpu *cpu, uint32_t opcode) { ADC(RD, RN, RRI(RMI, REG_SHIFT)) } // ADC Rd,Rn,Rm,ROR #i
void adcRrr(interpreter::Cpu *cpu, uint32_t opcode) { ADC(RD, RN, RRR(RMR, RS))        } // ADC Rd,Rn,Rm,ROR Rs

void umlal(interpreter::Cpu *cpu, uint32_t opcode) { UMLAL(RD, RN, RMR, RS) } // UMLAL RdLo,RdHi,Rm,Rs

void adcsLli(interpreter::Cpu *cpu, uint32_t opcode) { ADC(RD, RN, LLI(RMI, REG_SHIFT)) ADC_FLAGS(RD) } // ADCS Rd,Rn,Rm,LSL #i
void adcsLlr(interpreter::Cpu *cpu, uint32_t opcode) { ADC(RD, RN, LLR(RMR, RS))        ADC_FLAGS(RD) } // ADCS Rd,Rn,Rm,LSL Rs
void adcsLri(interpreter::Cpu *cpu, uint32_t opcode) { ADC(RD, RN, LRI(RMI, REG_SHIFT)) ADC_FLAGS(RD) } // ADCS Rd,Rn,Rm,LSR #i
void adcsLrr(interpreter::Cpu *cpu, uint32_t opcode) { ADC(RD, RN, LRR(RMR, RS))        ADC_FLAGS(RD) } // ADCS Rd,Rn,Rm,LSR Rs
void adcsAri(interpreter::Cpu *cpu, uint32_t opcode) { ADC(RD, RN, ARI(RMI, REG_SHIFT)) ADC_FLAGS(RD) } // ADCS Rd,Rn,Rm,ASR #i
void adcsArr(interpreter::Cpu *cpu, uint32_t opcode) { ADC(RD, RN, ARR(RMR, RS))        ADC_FLAGS(RD) } // ADCS Rd,Rn,Rm,ASR Rs
void adcsRri(interpreter::Cpu *cpu, uint32_t opcode) { ADC(RD, RN, RRI(RMI, REG_SHIFT)) ADC_FLAGS(RD) } // ADCS Rd,Rn,Rm,ROR #i
void adcsRrr(interpreter::Cpu *cpu, uint32_t opcode) { ADC(RD, RN, RRR(RMR, RS))        ADC_FLAGS(RD) } // ADCS Rd,Rn,Rm,ROR Rs

void umlals(interpreter::Cpu *cpu, uint32_t opcode) { UMLAL(RD, RN, RMR, RS) MUL_FLAGS(RN) } // UMLALS RdLo,RdHi,Rm,Rs

void sbcLli(interpreter::Cpu *cpu, uint32_t opcode) { SBC(RD, RN, LLI(RMI, REG_SHIFT)) } // SBC Rd,Rn,Rm,LSL #i
void sbcLlr(interpreter::Cpu *cpu, uint32_t opcode) { SBC(RD, RN, LLR(RMR, RS))        } // SBC Rd,Rn,Rm,LSL Rs
void sbcLri(interpreter::Cpu *cpu, uint32_t opcode) { SBC(RD, RN, LRI(RMI, REG_SHIFT)) } // SBC Rd,Rn,Rm,LSR #i
void sbcLrr(interpreter::Cpu *cpu, uint32_t opcode) { SBC(RD, RN, LRR(RMR, RS))        } // SBC Rd,Rn,Rm,LSR Rs
void sbcAri(interpreter::Cpu *cpu, uint32_t opcode) { SBC(RD, RN, ARI(RMI, REG_SHIFT)) } // SBC Rd,Rn,Rm,ASR #i
void sbcArr(interpreter::Cpu *cpu, uint32_t opcode) { SBC(RD, RN, ARR(RMR, RS))        } // SBC Rd,Rn,Rm,ASR Rs
void sbcRri(interpreter::Cpu *cpu, uint32_t opcode) { SBC(RD, RN, RRI(RMI, REG_SHIFT)) } // SBC Rd,Rn,Rm,ROR #i
void sbcRrr(interpreter::Cpu *cpu, uint32_t opcode) { SBC(RD, RN, RRR(RMR, RS))        } // SBC Rd,Rn,Rm,ROR Rs

void smull(interpreter::Cpu *cpu, uint32_t opcode) { SMULL(RD, RN, RMR, RS) } // SMULL RdLo,RdHi,Rm,Rs

void sbcsLli(interpreter::Cpu *cpu, uint32_t opcode) { SBC(RD, RN, LLI(RMI, REG_SHIFT)) SBC_FLAGS(RD) } // SBCS Rd,Rn,Rm,LSL #i
void sbcsLlr(interpreter::Cpu *cpu, uint32_t opcode) { SBC(RD, RN, LLR(RMR, RS))        SBC_FLAGS(RD) } // SBCS Rd,Rn,Rm,LSL Rs
void sbcsLri(interpreter::Cpu *cpu, uint32_t opcode) { SBC(RD, RN, LRI(RMI, REG_SHIFT)) SBC_FLAGS(RD) } // SBCS Rd,Rn,Rm,LSR #i
void sbcsLrr(interpreter::Cpu *cpu, uint32_t opcode) { SBC(RD, RN, LRR(RMR, RS))        SBC_FLAGS(RD) } // SBCS Rd,Rn,Rm,LSR Rs
void sbcsAri(interpreter::Cpu *cpu, uint32_t opcode) { SBC(RD, RN, ARI(RMI, REG_SHIFT)) SBC_FLAGS(RD) } // SBCS Rd,Rn,Rm,ASR #i
void sbcsArr(interpreter::Cpu *cpu, uint32_t opcode) { SBC(RD, RN, ARR(RMR, RS))        SBC_FLAGS(RD) } // SBCS Rd,Rn,Rm,ASR Rs
void sbcsRri(interpreter::Cpu *cpu, uint32_t opcode) { SBC(RD, RN, RRI(RMI, REG_SHIFT)) SBC_FLAGS(RD) } // SBCS Rd,Rn,Rm,ROR #i
void sbcsRrr(interpreter::Cpu *cpu, uint32_t opcode) { SBC(RD, RN, RRR(RMR, RS))        SBC_FLAGS(RD) } // SBCS Rd,Rn,Rm,ROR Rs

void smulls(interpreter::Cpu *cpu, uint32_t opcode) { SMULL(RD, RN, RMR, RS) MUL_FLAGS(RN) } // SMULLS RdLo,RdHi,Rm,Rs

void rscLli(interpreter::Cpu *cpu, uint32_t opcode) { RSC(RD, RN, LLI(RMI, REG_SHIFT)) } // RSC Rd,Rn,Rm,LSL #i
void rscLlr(interpreter::Cpu *cpu, uint32_t opcode) { RSC(RD, RN, LLR(RMR, RS))        } // RSC Rd,Rn,Rm,LSL Rs
void rscLri(interpreter::Cpu *cpu, uint32_t opcode) { RSC(RD, RN, LRI(RMI, REG_SHIFT)) } // RSC Rd,Rn,Rm,LSR #i
void rscLrr(interpreter::Cpu *cpu, uint32_t opcode) { RSC(RD, RN, LRR(RMR, RS))        } // RSC Rd,Rn,Rm,LSR Rs
void rscAri(interpreter::Cpu *cpu, uint32_t opcode) { RSC(RD, RN, ARI(RMI, REG_SHIFT)) } // RSC Rd,Rn,Rm,ASR #i
void rscArr(interpreter::Cpu *cpu, uint32_t opcode) { RSC(RD, RN, ARR(RMR, RS))        } // RSC Rd,Rn,Rm,ASR Rs
void rscRri(interpreter::Cpu *cpu, uint32_t opcode) { RSC(RD, RN, RRI(RMI, REG_SHIFT)) } // RSC Rd,Rn,Rm,ROR #i
void rscRrr(interpreter::Cpu *cpu, uint32_t opcode) { RSC(RD, RN, RRR(RMR, RS))        } // RSC Rd,Rn,Rm,ROR Rs

void smlal(interpreter::Cpu *cpu, uint32_t opcode) { SMLAL(RD, RN, RMR, RS) } // SMLAL RdLo,RdHi,Rm,Rs

void rscsLli(interpreter::Cpu *cpu, uint32_t opcode) { RSC(RD, RN, LLI(RMI, REG_SHIFT)) SBC_FLAGS(RD) } // RSCS Rd,Rn,Rm,LSL #i
void rscsLlr(interpreter::Cpu *cpu, uint32_t opcode) { RSC(RD, RN, LLR(RMR, RS))        SBC_FLAGS(RD) } // RSCS Rd,Rn,Rm,LSL Rs
void rscsLri(interpreter::Cpu *cpu, uint32_t opcode) { RSC(RD, RN, LRI(RMI, REG_SHIFT)) SBC_FLAGS(RD) } // RSCS Rd,Rn,Rm,LSR #i
void rscsLrr(interpreter::Cpu *cpu, uint32_t opcode) { RSC(RD, RN, LRR(RMR, RS))        SBC_FLAGS(RD) } // RSCS Rd,Rn,Rm,LSR Rs
void rscsAri(interpreter::Cpu *cpu, uint32_t opcode) { RSC(RD, RN, ARI(RMI, REG_SHIFT)) SBC_FLAGS(RD) } // RSCS Rd,Rn,Rm,ASR #i
void rscsArr(interpreter::Cpu *cpu, uint32_t opcode) { RSC(RD, RN, ARR(RMR, RS))        SBC_FLAGS(RD) } // RSCS Rd,Rn,Rm,ASR Rs
void rscsRri(interpreter::Cpu *cpu, uint32_t opcode) { RSC(RD, RN, RRI(RMI, REG_SHIFT)) SBC_FLAGS(RD) } // RSCS Rd,Rn,Rm,ROR #i
void rscsRrr(interpreter::Cpu *cpu, uint32_t opcode) { RSC(RD, RN, RRR(RMR, RS))        SBC_FLAGS(RD) } // RSCS Rd,Rn,Rm,ROR Rs

void smlals(interpreter::Cpu *cpu, uint32_t opcode) { SMLAL(RD, RN, RMR, RS) MUL_FLAGS(RN) } // SMLALS RdLo,RdHi,Rm,Rs

void tstLli(interpreter::Cpu *cpu, uint32_t opcode) { LLI_CARRY(RMI, REG_SHIFT) TST(RN, lli) } // TST Rn,Rm,LSL #i
void tstLlr(interpreter::Cpu *cpu, uint32_t opcode) { LLR_CARRY(RMR, RS)        TST(RN, llr) } // TST Rn,Rm,LSL Rs
void tstLri(interpreter::Cpu *cpu, uint32_t opcode) { LRI_CARRY(RMI, REG_SHIFT) TST(RN, lri) } // TST Rn,Rm,LSR #i
void tstLrr(interpreter::Cpu *cpu, uint32_t opcode) { LRR_CARRY(RMR, RS)        TST(RN, lrr) } // TST Rn,Rm,LSR Rs
void tstAri(interpreter::Cpu *cpu, uint32_t opcode) { ARI_CARRY(RMI, REG_SHIFT) TST(RN, ari) } // TST Rn,Rm,ASR #i
void tstArr(interpreter::Cpu *cpu, uint32_t opcode) { ARR_CARRY(RMR, RS)        TST(RN, arr) } // TST Rn,Rm,ASR Rs
void tstRri(interpreter::Cpu *cpu, uint32_t opcode) { RRI_CARRY(RMI, REG_SHIFT) TST(RN, rri) } // TST Rn,Rm,ROR #i
void tstRrr(interpreter::Cpu *cpu, uint32_t opcode) { RRR_CARRY(RMR, RS)        TST(RN, rrr) } // TST Rn,Rm,ROR Rs

void teqLli(interpreter::Cpu *cpu, uint32_t opcode) { LLI_CARRY(RMI, REG_SHIFT) TEQ(RN, lli) } // TEQ Rn,Rm,LSL #i
void teqLlr(interpreter::Cpu *cpu, uint32_t opcode) { LLR_CARRY(RMR, RS)        TEQ(RN, llr) } // TEQ Rn,Rm,LSL Rs
void teqLri(interpreter::Cpu *cpu, uint32_t opcode) { LRI_CARRY(RMI, REG_SHIFT) TEQ(RN, lri) } // TEQ Rn,Rm,LSR #i
void teqLrr(interpreter::Cpu *cpu, uint32_t opcode) { LRR_CARRY(RMR, RS)        TEQ(RN, lrr) } // TEQ Rn,Rm,LSR Rs
void teqAri(interpreter::Cpu *cpu, uint32_t opcode) { ARI_CARRY(RMI, REG_SHIFT) TEQ(RN, ari) } // TEQ Rn,Rm,ASR #i
void teqArr(interpreter::Cpu *cpu, uint32_t opcode) { ARR_CARRY(RMR, RS)        TEQ(RN, arr) } // TEQ Rn,Rm,ASR Rs
void teqRri(interpreter::Cpu *cpu, uint32_t opcode) { RRI_CARRY(RMI, REG_SHIFT) TEQ(RN, rri) } // TEQ Rn,Rm,ROR #i
void teqRrr(interpreter::Cpu *cpu, uint32_t opcode) { RRR_CARRY(RMR, RS)        TEQ(RN, rrr) } // TEQ Rn,Rm,ROR Rs

void cmpLli(interpreter::Cpu *cpu, uint32_t opcode) { CMP(RN, LLI(RMI, REG_SHIFT)) } // CMP Rn,Rm,LSL #i
void cmpLlr(interpreter::Cpu *cpu, uint32_t opcode) { CMP(RN, LLR(RMR, RS))        } // CMP Rn,Rm,LSL Rs
void cmpLri(interpreter::Cpu *cpu, uint32_t opcode) { CMP(RN, LRI(RMI, REG_SHIFT)) } // CMP Rn,Rm,LSR #i
void cmpLrr(interpreter::Cpu *cpu, uint32_t opcode) { CMP(RN, LRR(RMR, RS))        } // CMP Rn,Rm,LSR Rs
void cmpAri(interpreter::Cpu *cpu, uint32_t opcode) { CMP(RN, ARI(RMI, REG_SHIFT)) } // CMP Rn,Rm,ASR #i
void cmpArr(interpreter::Cpu *cpu, uint32_t opcode) { CMP(RN, ARR(RMR, RS))        } // CMP Rn,Rm,ASR Rs
void cmpRri(interpreter::Cpu *cpu, uint32_t opcode) { CMP(RN, RRI(RMI, REG_SHIFT)) } // CMP Rn,Rm,ROR #i
void cmpRrr(interpreter::Cpu *cpu, uint32_t opcode) { CMP(RN, RRR(RMR, RS))        } // CMP Rn,Rm,ROR Rs

void cmnLli(interpreter::Cpu *cpu, uint32_t opcode) { CMN(RN, LLI(RMI, REG_SHIFT)) } // CMN Rn,Rm,LSL #i
void cmnLlr(interpreter::Cpu *cpu, uint32_t opcode) { CMN(RN, LLR(RMR, RS))        } // CMN Rn,Rm,LSL Rs
void cmnLri(interpreter::Cpu *cpu, uint32_t opcode) { CMN(RN, LRI(RMI, REG_SHIFT)) } // CMN Rn,Rm,LSR #i
void cmnLrr(interpreter::Cpu *cpu, uint32_t opcode) { CMN(RN, LRR(RMR, RS))        } // CMN Rn,Rm,LSR Rs
void cmnAri(interpreter::Cpu *cpu, uint32_t opcode) { CMN(RN, ARI(RMI, REG_SHIFT)) } // CMN Rn,Rm,ASR #i
void cmnArr(interpreter::Cpu *cpu, uint32_t opcode) { CMN(RN, ARR(RMR, RS))        } // CMN Rn,Rm,ASR Rs
void cmnRri(interpreter::Cpu *cpu, uint32_t opcode) { CMN(RN, RRI(RMI, REG_SHIFT)) } // CMN Rn,Rm,ROR #i
void cmnRrr(interpreter::Cpu *cpu, uint32_t opcode) { CMN(RN, RRR(RMR, RS))        } // CMN Rn,Rm,ROR Rs

void orrLli(interpreter::Cpu *cpu, uint32_t opcode) { ORR(RD, RN, LLI(RMI, REG_SHIFT)) } // ORR Rd,Rn,Rm,LSL #i
void orrLlr(interpreter::Cpu *cpu, uint32_t opcode) { ORR(RD, RN, LLR(RMR, RS))        } // ORR Rd,Rn,Rm,LSL Rs
void orrLri(interpreter::Cpu *cpu, uint32_t opcode) { ORR(RD, RN, LRI(RMI, REG_SHIFT)) } // ORR Rd,Rn,Rm,LSR #i
void orrLrr(interpreter::Cpu *cpu, uint32_t opcode) { ORR(RD, RN, LRR(RMR, RS))        } // ORR Rd,Rn,Rm,LSR Rs
void orrAri(interpreter::Cpu *cpu, uint32_t opcode) { ORR(RD, RN, ARI(RMI, REG_SHIFT)) } // ORR Rd,Rn,Rm,ASR #i
void orrArr(interpreter::Cpu *cpu, uint32_t opcode) { ORR(RD, RN, ARR(RMR, RS))        } // ORR Rd,Rn,Rm,ASR Rs
void orrRri(interpreter::Cpu *cpu, uint32_t opcode) { ORR(RD, RN, RRI(RMI, REG_SHIFT)) } // ORR Rd,Rn,Rm,ROR #i
void orrRrr(interpreter::Cpu *cpu, uint32_t opcode) { ORR(RD, RN, RRR(RMR, RS))        } // ORR Rd,Rn,Rm,ROR Rs

void orrsLli(interpreter::Cpu *cpu, uint32_t opcode) { LLI_CARRY(RMI, REG_SHIFT) ORR(RD, RN, lli) COMMON_FLAGS(RD) } // ORRS Rd,Rn,Rm,LSL #i
void orrsLlr(interpreter::Cpu *cpu, uint32_t opcode) { LLR_CARRY(RMR, RS)        ORR(RD, RN, llr) COMMON_FLAGS(RD) } // ORRS Rd,Rn,Rm,LSL Rs
void orrsLri(interpreter::Cpu *cpu, uint32_t opcode) { LRI_CARRY(RMI, REG_SHIFT) ORR(RD, RN, lri) COMMON_FLAGS(RD) } // ORRS Rd,Rn,Rm,LSR #i
void orrsLrr(interpreter::Cpu *cpu, uint32_t opcode) { LRR_CARRY(RMR, RS)        ORR(RD, RN, lrr) COMMON_FLAGS(RD) } // ORRS Rd,Rn,Rm,LSR Rs
void orrsAri(interpreter::Cpu *cpu, uint32_t opcode) { ARI_CARRY(RMI, REG_SHIFT) ORR(RD, RN, ari) COMMON_FLAGS(RD) } // ORRS Rd,Rn,Rm,ASR #i
void orrsArr(interpreter::Cpu *cpu, uint32_t opcode) { ARR_CARRY(RMR, RS)        ORR(RD, RN, arr) COMMON_FLAGS(RD) } // ORRS Rd,Rn,Rm,ASR Rs
void orrsRri(interpreter::Cpu *cpu, uint32_t opcode) { RRI_CARRY(RMI, REG_SHIFT) ORR(RD, RN, rri) COMMON_FLAGS(RD) } // ORRS Rd,Rn,Rm,ROR #i
void orrsRrr(interpreter::Cpu *cpu, uint32_t opcode) { RRR_CARRY(RMR, RS)        ORR(RD, RN, rrr) COMMON_FLAGS(RD) } // ORRS Rd,Rn,Rm,ROR Rs

void movLli(interpreter::Cpu *cpu, uint32_t opcode) { MOV(RD, LLI(RMI, REG_SHIFT)) } // MOV Rd,Rm,LSL #i
void movLlr(interpreter::Cpu *cpu, uint32_t opcode) { MOV(RD, LLR(RMR, RS))        } // MOV Rd,Rm,LSL Rs
void movLri(interpreter::Cpu *cpu, uint32_t opcode) { MOV(RD, LRI(RMI, REG_SHIFT)) } // MOV Rd,Rm,LSR #i
void movLrr(interpreter::Cpu *cpu, uint32_t opcode) { MOV(RD, LRR(RMR, RS))        } // MOV Rd,Rm,LSR Rs
void movAri(interpreter::Cpu *cpu, uint32_t opcode) { MOV(RD, ARI(RMI, REG_SHIFT)) } // MOV Rd,Rm,ASR #i
void movArr(interpreter::Cpu *cpu, uint32_t opcode) { MOV(RD, ARR(RMR, RS))        } // MOV Rd,Rm,ASR Rs
void movRri(interpreter::Cpu *cpu, uint32_t opcode) { MOV(RD, RRI(RMI, REG_SHIFT)) } // MOV Rd,Rm,ROR #i
void movRrr(interpreter::Cpu *cpu, uint32_t opcode) { MOV(RD, RRR(RMR, RS))        } // MOV Rd,Rm,ROR Rs

void movsLli(interpreter::Cpu *cpu, uint32_t opcode) { LLI_CARRY(RMI, REG_SHIFT) MOV(RD, lli) COMMON_FLAGS(RD) } // MOVS Rd,Rm,LSL #i
void movsLlr(interpreter::Cpu *cpu, uint32_t opcode) { LLR_CARRY(RMR, RS)        MOV(RD, llr) COMMON_FLAGS(RD) } // MOVS Rd,Rm,LSL Rs
void movsLri(interpreter::Cpu *cpu, uint32_t opcode) { LRI_CARRY(RMI, REG_SHIFT) MOV(RD, lri) COMMON_FLAGS(RD) } // MOVS Rd,Rm,LSR #i
void movsLrr(interpreter::Cpu *cpu, uint32_t opcode) { LRR_CARRY(RMR, RS)        MOV(RD, lrr) COMMON_FLAGS(RD) } // MOVS Rd,Rm,LSR Rs
void movsAri(interpreter::Cpu *cpu, uint32_t opcode) { ARI_CARRY(RMI, REG_SHIFT) MOV(RD, ari) COMMON_FLAGS(RD) } // MOVS Rd,Rm,ASR #i
void movsArr(interpreter::Cpu *cpu, uint32_t opcode) { ARR_CARRY(RMR, RS)        MOV(RD, arr) COMMON_FLAGS(RD) } // MOVS Rd,Rm,ASR Rs
void movsRri(interpreter::Cpu *cpu, uint32_t opcode) { RRI_CARRY(RMI, REG_SHIFT) MOV(RD, rri) COMMON_FLAGS(RD) } // MOVS Rd,Rm,ROR #i
void movsRrr(interpreter::Cpu *cpu, uint32_t opcode) { RRR_CARRY(RMR, RS)        MOV(RD, rrr) COMMON_FLAGS(RD) } // MOVS Rd,Rm,ROR Rs

void bicLli(interpreter::Cpu *cpu, uint32_t opcode) { BIC(RD, RN, LLI(RMI, REG_SHIFT)) } // BIC Rd,Rn,Rm,LSL #i
void bicLlr(interpreter::Cpu *cpu, uint32_t opcode) { BIC(RD, RN, LLR(RMR, RS))        } // BIC Rd,Rn,Rm,LSL Rs
void bicLri(interpreter::Cpu *cpu, uint32_t opcode) { BIC(RD, RN, LRI(RMI, REG_SHIFT)) } // BIC Rd,Rn,Rm,LSR #i
void bicLrr(interpreter::Cpu *cpu, uint32_t opcode) { BIC(RD, RN, LRR(RMR, RS))        } // BIC Rd,Rn,Rm,LSR Rs
void bicAri(interpreter::Cpu *cpu, uint32_t opcode) { BIC(RD, RN, ARI(RMI, REG_SHIFT)) } // BIC Rd,Rn,Rm,ASR #i
void bicArr(interpreter::Cpu *cpu, uint32_t opcode) { BIC(RD, RN, ARR(RMR, RS))        } // BIC Rd,Rn,Rm,ASR Rs
void bicRri(interpreter::Cpu *cpu, uint32_t opcode) { BIC(RD, RN, RRI(RMI, REG_SHIFT)) } // BIC Rd,Rn,Rm,ROR #i
void bicRrr(interpreter::Cpu *cpu, uint32_t opcode) { BIC(RD, RN, RRR(RMR, RS))        } // BIC Rd,Rn,Rm,ROR Rs

void bicsLli(interpreter::Cpu *cpu, uint32_t opcode) { LLI_CARRY(RMI, REG_SHIFT) BIC(RD, RN, lli) COMMON_FLAGS(RD) } // BICS Rd,Rn,Rm,LSL #i
void bicsLlr(interpreter::Cpu *cpu, uint32_t opcode) { LLR_CARRY(RMR, RS)        BIC(RD, RN, llr) COMMON_FLAGS(RD) } // BICS Rd,Rn,Rm,LSL Rs
void bicsLri(interpreter::Cpu *cpu, uint32_t opcode) { LRI_CARRY(RMI, REG_SHIFT) BIC(RD, RN, lri) COMMON_FLAGS(RD) } // BICS Rd,Rn,Rm,LSR #i
void bicsLrr(interpreter::Cpu *cpu, uint32_t opcode) { LRR_CARRY(RMR, RS)        BIC(RD, RN, lrr) COMMON_FLAGS(RD) } // BICS Rd,Rn,Rm,LSR Rs
void bicsAri(interpreter::Cpu *cpu, uint32_t opcode) { ARI_CARRY(RMI, REG_SHIFT) BIC(RD, RN, ari) COMMON_FLAGS(RD) } // BICS Rd,Rn,Rm,ASR #i
void bicsArr(interpreter::Cpu *cpu, uint32_t opcode) { ARR_CARRY(RMR, RS)        BIC(RD, RN, arr) COMMON_FLAGS(RD) } // BICS Rd,Rn,Rm,ASR Rs
void bicsRri(interpreter::Cpu *cpu, uint32_t opcode) { RRI_CARRY(RMI, REG_SHIFT) BIC(RD, RN, rri) COMMON_FLAGS(RD) } // BICS Rd,Rn,Rm,ROR #i
void bicsRrr(interpreter::Cpu *cpu, uint32_t opcode) { RRR_CARRY(RMR, RS)        BIC(RD, RN, rrr) COMMON_FLAGS(RD) } // BICS Rd,Rn,Rm,ROR Rs

void mvnLli(interpreter::Cpu *cpu, uint32_t opcode) { MVN(RD, LLI(RMI, REG_SHIFT)) } // MVN Rd,Rm,LSL #i
void mvnLlr(interpreter::Cpu *cpu, uint32_t opcode) { MVN(RD, LLR(RMR, RS))        } // MVN Rd,Rm,LSL Rs
void mvnLri(interpreter::Cpu *cpu, uint32_t opcode) { MVN(RD, LRI(RMI, REG_SHIFT)) } // MVN Rd,Rm,LSR #i
void mvnLrr(interpreter::Cpu *cpu, uint32_t opcode) { MVN(RD, LRR(RMR, RS))        } // MVN Rd,Rm,LSR Rs
void mvnAri(interpreter::Cpu *cpu, uint32_t opcode) { MVN(RD, ARI(RMI, REG_SHIFT)) } // MVN Rd,Rm,ASR #i
void mvnArr(interpreter::Cpu *cpu, uint32_t opcode) { MVN(RD, ARR(RMR, RS))        } // MVN Rd,Rm,ASR Rs
void mvnRri(interpreter::Cpu *cpu, uint32_t opcode) { MVN(RD, RRI(RMI, REG_SHIFT)) } // MVN Rd,Rm,ROR #i
void mvnRrr(interpreter::Cpu *cpu, uint32_t opcode) { MVN(RD, RRR(RMR, RS))        } // MVN Rd,Rm,ROR Rs

void mvnsLli(interpreter::Cpu *cpu, uint32_t opcode) { LLI_CARRY(RMI, REG_SHIFT) MVN(RD, lli) COMMON_FLAGS(RD) } // MVNS Rd,Rm,LSL #i
void mvnsLlr(interpreter::Cpu *cpu, uint32_t opcode) { LLR_CARRY(RMR, RS)        MVN(RD, llr) COMMON_FLAGS(RD) } // MVNS Rd,Rm,LSL Rs
void mvnsLri(interpreter::Cpu *cpu, uint32_t opcode) { LRI_CARRY(RMI, REG_SHIFT) MVN(RD, lri) COMMON_FLAGS(RD) } // MVNS Rd,Rm,LSR #i
void mvnsLrr(interpreter::Cpu *cpu, uint32_t opcode) { LRR_CARRY(RMR, RS)        MVN(RD, lrr) COMMON_FLAGS(RD) } // MVNS Rd,Rm,LSR Rs
void mvnsAri(interpreter::Cpu *cpu, uint32_t opcode) { ARI_CARRY(RMI, REG_SHIFT) MVN(RD, ari) COMMON_FLAGS(RD) } // MVNS Rd,Rm,ASR #i
void mvnsArr(interpreter::Cpu *cpu, uint32_t opcode) { ARR_CARRY(RMR, RS)        MVN(RD, arr) COMMON_FLAGS(RD) } // MVNS Rd,Rm,ASR Rs
void mvnsRri(interpreter::Cpu *cpu, uint32_t opcode) { RRI_CARRY(RMI, REG_SHIFT) MVN(RD, rri) COMMON_FLAGS(RD) } // MVNS Rd,Rm,ROR #i
void mvnsRrr(interpreter::Cpu *cpu, uint32_t opcode) { RRR_CARRY(RMR, RS)        MVN(RD, rrr) COMMON_FLAGS(RD) } // MVNS Rd,Rm,ROR Rs

void andImm (interpreter::Cpu *cpu, uint32_t opcode) {                           AND(RD, RN, RRR(IMM, IMM_SHIFT))                  } // AND  Rd,Rn,#i
void andsImm(interpreter::Cpu *cpu, uint32_t opcode) { RRR_CARRY(IMM, IMM_SHIFT) AND(RD, RN, rrr)                 COMMON_FLAGS(RD) } // ANDS Rd,Rn,#i
void eorImm (interpreter::Cpu *cpu, uint32_t opcode) {                           EOR(RD, RN, RRR(IMM, IMM_SHIFT))                  } // EOR  Rd,Rn,#i
void eorsImm(interpreter::Cpu *cpu, uint32_t opcode) { RRR_CARRY(IMM, IMM_SHIFT) EOR(RD, RN, rrr)                 COMMON_FLAGS(RD) } // EORS Rd,Rn,#i
void subImm (interpreter::Cpu *cpu, uint32_t opcode) {                           SUB(RD, RN, RRR(IMM, IMM_SHIFT))                  } // SUB  Rd,Rn,#i
void subsImm(interpreter::Cpu *cpu, uint32_t opcode) {                           SUB(RD, RN, RRR(IMM, IMM_SHIFT))    SUB_FLAGS(RD) } // SUBS Rd,Rn,#i
void rsbImm (interpreter::Cpu *cpu, uint32_t opcode) {                           RSB(RD, RN, RRR(IMM, IMM_SHIFT))                  } // RSB  Rd,Rn,#i
void rsbsImm(interpreter::Cpu *cpu, uint32_t opcode) {                           RSB(RD, RN, RRR(IMM, IMM_SHIFT))    SUB_FLAGS(RD) } // RSBS Rd,Rn,#i
void addImm (interpreter::Cpu *cpu, uint32_t opcode) {                           ADD(RD, RN, RRR(IMM, IMM_SHIFT))                  } // ADD  Rd,Rn,#i
void addsImm(interpreter::Cpu *cpu, uint32_t opcode) {                           ADD(RD, RN, RRR(IMM, IMM_SHIFT))    ADD_FLAGS(RD) } // ADDS Rd,Rn,#i
void adcImm (interpreter::Cpu *cpu, uint32_t opcode) {                           ADC(RD, RN, RRR(IMM, IMM_SHIFT))                  } // ADC  Rd,Rn,#i
void adcsImm(interpreter::Cpu *cpu, uint32_t opcode) {                           ADC(RD, RN, RRR(IMM, IMM_SHIFT))    ADC_FLAGS(RD) } // ADCS Rd,Rn,#i
void sbcImm (interpreter::Cpu *cpu, uint32_t opcode) {                           SBC(RD, RN, RRR(IMM, IMM_SHIFT))                  } // SBC  Rd,Rn,#i
void sbcsImm(interpreter::Cpu *cpu, uint32_t opcode) {                           SBC(RD, RN, RRR(IMM, IMM_SHIFT))    SBC_FLAGS(RD) } // SBCS Rd,Rn,#i
void rscImm (interpreter::Cpu *cpu, uint32_t opcode) {                           RSC(RD, RN, RRR(IMM, IMM_SHIFT))                  } // RSC  Rd,Rn,#i
void rscsImm(interpreter::Cpu *cpu, uint32_t opcode) {                           RSC(RD, RN, RRR(IMM, IMM_SHIFT))    SBC_FLAGS(RD) } // RSCS Rd,Rn,#i
void tstImm (interpreter::Cpu *cpu, uint32_t opcode) { RRR_CARRY(IMM, IMM_SHIFT) TST(    RN, rrr)                                  } // TST  Rn,#i
void teqImm (interpreter::Cpu *cpu, uint32_t opcode) { RRR_CARRY(IMM, IMM_SHIFT) TEQ(    RN, rrr)                                  } // TEQ  Rn,#i
void cmpImm (interpreter::Cpu *cpu, uint32_t opcode) {                           CMP(    RN, RRR(IMM, IMM_SHIFT))                  } // CMP  Rn,#i
void cmnImm (interpreter::Cpu *cpu, uint32_t opcode) {                           CMN(    RN, RRR(IMM, IMM_SHIFT))                  } // CMN  Rn,#i
void orrImm (interpreter::Cpu *cpu, uint32_t opcode) {                           ORR(RD, RN, RRR(IMM, IMM_SHIFT))                  } // ORR  Rd,Rn,#i
void orrsImm(interpreter::Cpu *cpu, uint32_t opcode) { RRR_CARRY(IMM, IMM_SHIFT) ORR(RD, RN, rrr)                 COMMON_FLAGS(RD) } // ORRS Rd,Rn,#i
void movImm (interpreter::Cpu *cpu, uint32_t opcode) {                           MOV(RD,     RRR(IMM, IMM_SHIFT))                  } // MOV  Rd,#i
void movsImm(interpreter::Cpu *cpu, uint32_t opcode) { RRR_CARRY(IMM, IMM_SHIFT) MOV(RD,     rrr)                 COMMON_FLAGS(RD) } // MOVS Rd,#i
void bicImm (interpreter::Cpu *cpu, uint32_t opcode) {                           BIC(RD, RN, RRR(IMM, IMM_SHIFT))                  } // BIC  Rd,Rn,#i
void bicsImm(interpreter::Cpu *cpu, uint32_t opcode) { RRR_CARRY(IMM, IMM_SHIFT) BIC(RD, RN, rrr)                 COMMON_FLAGS(RD) } // BICS Rd,Rn,#i
void mvnImm (interpreter::Cpu *cpu, uint32_t opcode) {                           MVN(RD,     RRR(IMM, IMM_SHIFT))                  } // MVN  Rd,#i
void mvnsImm(interpreter::Cpu *cpu, uint32_t opcode) { RRR_CARRY(IMM, IMM_SHIFT) MVN(RD,     rrr)                 COMMON_FLAGS(RD) } // MVNS Rd,#i

}

namespace interpreter_alu_thumb
{

void lslImm5(interpreter::Cpu *cpu, uint32_t opcode) { LLI_CARRY(RS_THUMB, IMM5_THUMB) MOV(RD_THUMB, lli) COMMON_FLAGS(RD_THUMB) } // LSL Rd,Rs,#i
void lsrImm5(interpreter::Cpu *cpu, uint32_t opcode) { LRI_CARRY(RS_THUMB, IMM5_THUMB) MOV(RD_THUMB, lri) COMMON_FLAGS(RD_THUMB) } // LSR Rd,Rs,#i
void asrImm5(interpreter::Cpu *cpu, uint32_t opcode) { ARI_CARRY(RS_THUMB, IMM5_THUMB) MOV(RD_THUMB, ari) COMMON_FLAGS(RD_THUMB) } // ASR Rd,Rs,#i

void addReg (interpreter::Cpu *cpu, uint32_t opcode) { ADD(RD_THUMB, RS_THUMB, RN_THUMB)   ADD_FLAGS(RD_THUMB) } // ADD Rd,Rs,Rn
void subReg (interpreter::Cpu *cpu, uint32_t opcode) { SUB(RD_THUMB, RS_THUMB, RN_THUMB)   SUB_FLAGS(RD_THUMB) } // SUB Rd,Rs,Rn
void addImm3(interpreter::Cpu *cpu, uint32_t opcode) { ADD(RD_THUMB, RS_THUMB, IMM3_THUMB) ADD_FLAGS(RD_THUMB) } // ADD Rd,Rs,#i
void subImm3(interpreter::Cpu *cpu, uint32_t opcode) { SUB(RD_THUMB, RS_THUMB, IMM3_THUMB) SUB_FLAGS(RD_THUMB) } // SUB Rd,Rs,#i

void movImm8(interpreter::Cpu *cpu, uint32_t opcode) { MOV(RD8_THUMB,            IMM8_THUMB) COMMON_FLAGS(RD8_THUMB) } // MOV Rd,#i
void cmpImm8(interpreter::Cpu *cpu, uint32_t opcode) { CMP(RD8_THUMB,            IMM8_THUMB)                         } // CMP Rd,#i
void addImm8(interpreter::Cpu *cpu, uint32_t opcode) { ADD(RD8_THUMB, RD8_THUMB, IMM8_THUMB)    ADD_FLAGS(RD8_THUMB) } // ADD Rd,#i
void subImm8(interpreter::Cpu *cpu, uint32_t opcode) { SUB(RD8_THUMB, RD8_THUMB, IMM8_THUMB)    SUB_FLAGS(RD8_THUMB) } // SUB Rd,#i

void dpG1(interpreter::Cpu *cpu, uint32_t opcode)
{
    switch (DP_SWITCH)
    {
        case 0x0: {                               AND(RD_THUMB, RD_THUMB, RS_THUMB) COMMON_FLAGS(RD_THUMB) } return; // AND Rd,Rs
        case 0x1: {                               EOR(RD_THUMB, RD_THUMB, RS_THUMB) COMMON_FLAGS(RD_THUMB) } return; // EOR Rd,Rs
        case 0x2: { LLR_CARRY(RD_THUMB, RS_THUMB) MOV(RD_THUMB, llr)                COMMON_FLAGS(RD_THUMB) } return; // LSL Rd,Rs
        case 0x3: { LRR_CARRY(RD_THUMB, RS_THUMB) MOV(RD_THUMB, lrr)                COMMON_FLAGS(RD_THUMB) } return; // LSR Rd,Rs
    }
}

void dpG2(interpreter::Cpu *cpu, uint32_t opcode)
{
    switch (DP_SWITCH)
    {
        case 0x0: { ARR_CARRY(RD_THUMB, RS_THUMB) MOV(RD_THUMB, arr)                COMMON_FLAGS(RD_THUMB) } return; // ASR Rd,Rs
        case 0x1: {                               ADC(RD_THUMB, RD_THUMB, RS_THUMB)    ADC_FLAGS(RD_THUMB) } return; // ADC Rd,Rs
        case 0x2: {                               SBC(RD_THUMB, RD_THUMB, RS_THUMB)    SBC_FLAGS(RD_THUMB) } return; // SBC Rd,Rs
        case 0x3: { RRR_CARRY(RD_THUMB, RS_THUMB) MOV(RD_THUMB, rrr)                COMMON_FLAGS(RD_THUMB) } return; // ROR Rd,Rs
    }
}

void dpG3(interpreter::Cpu *cpu, uint32_t opcode)
{
    switch (DP_SWITCH)
    {
        case 0x0: { TST(RD_THUMB,    RS_THUMB)                     } return; // TST Rd,Rs
        case 0x1: { SUB(RD_THUMB, 0, RS_THUMB) SUB_FLAGS(RD_THUMB) } return; // NEG Rd,Rs
        case 0x2: { CMP(RD_THUMB,    RS_THUMB)                     } return; // CMP Rd,Rs
        case 0x3: { CMN(RD_THUMB,    RS_THUMB)                     } return; // CMN Rd,Rs
    }
}

void dpG4(interpreter::Cpu *cpu, uint32_t opcode)
{
    switch (DP_SWITCH)
    {
        case 0x0: { ORR(RD_THUMB, RD_THUMB, RS_THUMB) COMMON_FLAGS(RD_THUMB) } return; // ORR Rd,Rs
        case 0x1: { MUL(RD_THUMB, RD_THUMB, RS_THUMB)    MUL_FLAGS(RD_THUMB) } return; // MUL Rd,Rs
        case 0x2: { BIC(RD_THUMB, RD_THUMB, RS_THUMB) COMMON_FLAGS(RD_THUMB) } return; // BIC Rd,Rs
        case 0x3: { MVN(RD_THUMB,           RS_THUMB) COMMON_FLAGS(RD_THUMB) } return; // MVN Rd,Rs
    }
}

void addH(interpreter::Cpu *cpu, uint32_t opcode) { ADD(RDH_THUMB, RDH_THUMB, RSH_THUMB) } // ADD Rd,Rs
void cmpH(interpreter::Cpu *cpu, uint32_t opcode) { CMP(RDH_THUMB,            RSH_THUMB) } // CMP Rd,Rs
void movH(interpreter::Cpu *cpu, uint32_t opcode) { MOV(RDH_THUMB,            RSH_THUMB) } // MOV Rd,Rs

void addPc   (interpreter::Cpu *cpu, uint32_t opcode) { ADD(RD8_THUMB, (*cpu->registers[15] & ~BIT(1)), (IMM8_THUMB << 2)) } // ADD Rd,PC,#i
void addSp   (interpreter::Cpu *cpu, uint32_t opcode) { ADD(RD8_THUMB,           *cpu->registers[13],   (IMM8_THUMB << 2)) } // ADD Rd,SP,#i
void addSpImm(interpreter::Cpu *cpu, uint32_t opcode) { ADD(*cpu->registers[13], *cpu->registers[13],   (IMM7_THUMB << 2)) } // ADD SP,#i

}
