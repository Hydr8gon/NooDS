/*
    Copyright 2019 Hydr8gon

    This file is part of NoiDS.

    NoiDS is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    NoiDS is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with NoiDS. If not, see <https://www.gnu.org/licenses/>.
*/

#include "interpreter_alu.h"

#define BIT(i) (1 << i)
#define SET_FLAG(bit, cond) if (cond) cpu->cpsr |= BIT(bit); else cpu->cpsr &= ~BIT(bit);

#define RN (*cpu->registers[(opcode & 0x000F0000) >> 16])
#define RD (*cpu->registers[(opcode & 0x0000F000) >> 12])
#define RS (*cpu->registers[(opcode & 0x00000F00) >>  8])
#define RM (*cpu->registers[(opcode & 0x0000000F)])

#define LLI(carry) interpreter::lsl(cpu, *cpu->registers[(opcode & 0x0000000F)], (opcode & 0x00000F80) >> 7,                  carry)
#define LLR(carry) interpreter::lsl(cpu, *cpu->registers[(opcode & 0x0000000F)], *cpu->registers[(opcode & 0x00000F00) >> 8], carry)
#define LRI(carry) interpreter::lsr(cpu, *cpu->registers[(opcode & 0x0000000F)], (opcode & 0x00000F80) >> 7,                  carry)
#define LRR(carry) interpreter::lsr(cpu, *cpu->registers[(opcode & 0x0000000F)], *cpu->registers[(opcode & 0x00000F00) >> 8], carry)
#define ARI(carry) interpreter::asr(cpu, *cpu->registers[(opcode & 0x0000000F)], (opcode & 0x00000F80) >> 7,                  carry)
#define ARR(carry) interpreter::asr(cpu, *cpu->registers[(opcode & 0x0000000F)], *cpu->registers[(opcode & 0x00000F00) >> 8], carry)
#define RRI(carry) interpreter::ror(cpu, *cpu->registers[(opcode & 0x0000000F)], (opcode & 0x00000F80) >> 7,                  carry)
#define RRR(carry) interpreter::ror(cpu, *cpu->registers[(opcode & 0x0000000F)], *cpu->registers[(opcode & 0x00000F00) >> 8], carry)
#define IMM(carry) interpreter::ror(cpu, opcode & 0x000000FF,                    (opcode & 0x00000F00) >> 7,                  carry)

#define COMMON_FLAGS(dst)        \
    SET_FLAG(31, dst & BIT(31)); \
    SET_FLAG(30, dst == 0);

#define SUB_FLAGS(dst)                                                                      \
    SET_FLAG(29, pre >= dst);                                                               \
    SET_FLAG(28, (sub & BIT(31)) != (pre & BIT(31)) && (dst & BIT(31)) == (sub & BIT(31))); \
    COMMON_FLAGS(dst);

#define ADD_FLAGS(dst)                                                                      \
    SET_FLAG(29, pre > dst);                                                                \
    SET_FLAG(28, (add & BIT(31)) == (pre & BIT(31)) && (dst & BIT(31)) != (add & BIT(31))); \
    COMMON_FLAGS(dst);

#define ADC_FLAGS(dst)                                                                      \
    SET_FLAG(29, pre > dst || (add == 0xFFFFFFFF && (cpu->cpsr & BIT(29))));                \
    SET_FLAG(28, (add & BIT(31)) == (pre & BIT(31)) && (dst & BIT(31)) != (add & BIT(31))); \
    COMMON_FLAGS(dst);

#define SBC_FLAGS(dst)                                                                      \
    SET_FLAG(29, pre >= dst && (sub != 0xFFFFFFFF || !(cpu->cpsr & BIT(29))));              \
    SET_FLAG(28, (sub & BIT(31)) != (pre & BIT(31)) && (dst & BIT(31)) == (sub & BIT(31))); \
    COMMON_FLAGS(dst);

#define SPSR_FLAGS(flags)                                   \
    if (((opcode & 0x0000F000) >> 12) == 15 && cpu->spsr)   \
    {                                                       \
        cpu->cpsr = *cpu->spsr & ~0x0000001F;               \
        interpreter::setMode(cpu, *cpu->spsr & 0x0000001F); \
    }                                                       \
    else                                                    \
    {                                                       \
        flags;                                              \
    }

#define AND(op2)   \
    RD = RN & op2;

#define ANDS(op2)                 \
    RD = RN & op2;                \
    SPSR_FLAGS(COMMON_FLAGS(RD));

#define EOR(op2)   \
    RD = RN ^ op2;

#define EORS(op2)                 \
    RD = RN ^ op2;                \
    SPSR_FLAGS(COMMON_FLAGS(RD));

#define SUB(op2)   \
    RD = RN - op2;

#define SUBS(op2)              \
    uint32_t pre = RN;         \
    uint32_t sub = op2;        \
    RD = pre - sub;            \
    SPSR_FLAGS(SUB_FLAGS(RD));

#define RSB(op2)   \
    RD = op2 - RN;

#define RSBS(op2)              \
    uint32_t pre = op2;        \
    uint32_t sub = RN;         \
    RD = pre - sub;            \
    SPSR_FLAGS(SUB_FLAGS(RD));

#define ADD(op2)   \
    RD = RN + op2;

#define ADDS(op2)              \
    uint32_t pre = RN;         \
    uint32_t add = op2;        \
    RD = pre + add;            \
    SPSR_FLAGS(ADD_FLAGS(RD));

#define ADC(op2)                                   \
    RD = RN + op2 + ((cpu->cpsr & BIT(29)) >> 29);

#define ADCS(op2)                                   \
    uint32_t pre = RN;                              \
    uint32_t add = op2;                             \
    RD = pre + add + ((cpu->cpsr & BIT(29)) >> 29); \
    SPSR_FLAGS(ADC_FLAGS(RD));

#define SBC(op2)                                       \
    RD = RN - op2 - 1 + ((cpu->cpsr & BIT(29)) >> 29);

#define SBCS(op2)                                       \
    uint32_t pre = RN;                                  \
    uint32_t sub = op2;                                 \
    RD = pre - sub - 1 + ((cpu->cpsr & BIT(29)) >> 29); \
    SPSR_FLAGS(SBC_FLAGS(RD));

#define RSC(op2)                                       \
    RD = op2 - RN - 1 + ((cpu->cpsr & BIT(29)) >> 29);

#define RSCS(op2)                                       \
    uint32_t pre = op2;                                 \
    uint32_t sub = RN;                                  \
    RD = pre - sub - 1 + ((cpu->cpsr & BIT(29)) >> 29); \
    SPSR_FLAGS(SBC_FLAGS(RD));

#define TST(op2)             \
    uint32_t res = RN & op2; \
    COMMON_FLAGS(res);

#define TEQ(op2)             \
    uint32_t res = RN ^ op2; \
    COMMON_FLAGS(res);

#define CMP(op2)              \
    uint32_t pre = RN;        \
    uint32_t sub = op2;       \
    uint32_t res = pre - sub; \
    SUB_FLAGS(res);

#define CMN(op2)              \
    uint32_t pre = RN;        \
    uint32_t add = op2;       \
    uint32_t res = pre + add; \
    ADD_FLAGS(res);

#define ORR(op2)   \
    RD = RN | op2;

#define ORRS(op2)                 \
    RD = RN | op2;                \
    SPSR_FLAGS(COMMON_FLAGS(RD));

#define MOV(op2) \
    RD = op2;

#define MOVS(op2)                 \
    RD = op2;                     \
    SPSR_FLAGS(COMMON_FLAGS(RD));

#define BIC(op2)    \
    RD = RN & ~op2;

#define BICS(op2)                 \
    RD = RN & ~op2;               \
    SPSR_FLAGS(COMMON_FLAGS(RD));

#define MVN(op2) \
    RD = ~op2;

#define MVNS(op2)                 \
    RD = ~op2;                    \
    SPSR_FLAGS(COMMON_FLAGS(RD));

#define MUL       \
    RN = RM * RS;

#define MULS                   \
    RN = RM * RS;              \
    if (cpu->type == 7)        \
        cpu->cpsr &= ~BIT(29); \
    COMMON_FLAGS(RN);

#define MLA            \
    RN = RM * RS + RD;

#define MLAS                   \
    RN = RM * RS + RD;         \
    if (cpu->type == 7)        \
        cpu->cpsr &= ~BIT(29); \
    COMMON_FLAGS(RN);

namespace interpreter_alu
{

void andLli(interpreter::Cpu *cpu, uint32_t opcode) { AND(LLI(false)); } // AND Rd,Rn,Rm,LSL #i
void andLlr(interpreter::Cpu *cpu, uint32_t opcode) { AND(LLR(false)); } // AND Rd,Rn,Rm,LSL Rs
void andLri(interpreter::Cpu *cpu, uint32_t opcode) { AND(LRI(false)); } // AND Rd,Rn,Rm,LSR #i
void andLrr(interpreter::Cpu *cpu, uint32_t opcode) { AND(LRR(false)); } // AND Rd,Rn,Rm,LSR Rs
void andAri(interpreter::Cpu *cpu, uint32_t opcode) { AND(ARI(false)); } // AND Rd,Rn,Rm,ASR #i
void andArr(interpreter::Cpu *cpu, uint32_t opcode) { AND(ARR(false)); } // AND Rd,Rn,Rm,ASR Rs
void andRri(interpreter::Cpu *cpu, uint32_t opcode) { AND(RRI(false)); } // AND Rd,Rn,Rm,ROR #i
void andRrr(interpreter::Cpu *cpu, uint32_t opcode) { AND(RRR(false)); } // AND Rd,Rn,Rm,ROR Rs

void mul(interpreter::Cpu *cpu, uint32_t opcode) { MUL; } // MUL Rd,Rm,Rs

void andsLli(interpreter::Cpu *cpu, uint32_t opcode) { ANDS(LLI(true)); } // ANDS Rd,Rn,Rm,LSL #i
void andsLlr(interpreter::Cpu *cpu, uint32_t opcode) { ANDS(LLR(true)); } // ANDS Rd,Rn,Rm,LSL Rs
void andsLri(interpreter::Cpu *cpu, uint32_t opcode) { ANDS(LRI(true)); } // ANDS Rd,Rn,Rm,LSR #i
void andsLrr(interpreter::Cpu *cpu, uint32_t opcode) { ANDS(LRR(true)); } // ANDS Rd,Rn,Rm,LSR Rs
void andsAri(interpreter::Cpu *cpu, uint32_t opcode) { ANDS(ARI(true)); } // ANDS Rd,Rn,Rm,ASR #i
void andsArr(interpreter::Cpu *cpu, uint32_t opcode) { ANDS(ARR(true)); } // ANDS Rd,Rn,Rm,ASR Rs
void andsRri(interpreter::Cpu *cpu, uint32_t opcode) { ANDS(RRI(true)); } // ANDS Rd,Rn,Rm,ROR #i
void andsRrr(interpreter::Cpu *cpu, uint32_t opcode) { ANDS(RRR(true)); } // ANDS Rd,Rn,Rm,ROR Rs

void muls(interpreter::Cpu *cpu, uint32_t opcode) { MULS; } // MULS Rd,Rm,Rs

void eorLli(interpreter::Cpu *cpu, uint32_t opcode) { EOR(LLI(false)); } // EOR Rd,Rn,Rm,LSL #i
void eorLlr(interpreter::Cpu *cpu, uint32_t opcode) { EOR(LLR(false)); } // EOR Rd,Rn,Rm,LSL Rs
void eorLri(interpreter::Cpu *cpu, uint32_t opcode) { EOR(LRI(false)); } // EOR Rd,Rn,Rm,LSR #i
void eorLrr(interpreter::Cpu *cpu, uint32_t opcode) { EOR(LRR(false)); } // EOR Rd,Rn,Rm,LSR Rs
void eorAri(interpreter::Cpu *cpu, uint32_t opcode) { EOR(ARI(false)); } // EOR Rd,Rn,Rm,ASR #i
void eorArr(interpreter::Cpu *cpu, uint32_t opcode) { EOR(ARR(false)); } // EOR Rd,Rn,Rm,ASR Rs
void eorRri(interpreter::Cpu *cpu, uint32_t opcode) { EOR(RRI(false)); } // EOR Rd,Rn,Rm,ROR #i
void eorRrr(interpreter::Cpu *cpu, uint32_t opcode) { EOR(RRR(false)); } // EOR Rd,Rn,Rm,ROR Rs

void mla(interpreter::Cpu *cpu, uint32_t opcode) { MLA; } // MLA Rd,Rm,Rs,Rn

void eorsLli(interpreter::Cpu *cpu, uint32_t opcode) { EORS(LLI(true)); } // EORS Rd,Rn,Rm,LSL #i
void eorsLlr(interpreter::Cpu *cpu, uint32_t opcode) { EORS(LLR(true)); } // EORS Rd,Rn,Rm,LSL Rs
void eorsLri(interpreter::Cpu *cpu, uint32_t opcode) { EORS(LRI(true)); } // EORS Rd,Rn,Rm,LSR #i
void eorsLrr(interpreter::Cpu *cpu, uint32_t opcode) { EORS(LRR(true)); } // EORS Rd,Rn,Rm,LSR Rs
void eorsAri(interpreter::Cpu *cpu, uint32_t opcode) { EORS(ARI(true)); } // EORS Rd,Rn,Rm,ASR #i
void eorsArr(interpreter::Cpu *cpu, uint32_t opcode) { EORS(ARR(true)); } // EORS Rd,Rn,Rm,ASR Rs
void eorsRri(interpreter::Cpu *cpu, uint32_t opcode) { EORS(RRI(true)); } // EORS Rd,Rn,Rm,ROR #i
void eorsRrr(interpreter::Cpu *cpu, uint32_t opcode) { EORS(RRR(true)); } // EORS Rd,Rn,Rm,ROR Rs

void mlas(interpreter::Cpu *cpu, uint32_t opcode) { MLAS; } // MLAS Rd,Rm,Rs,Rn

void subLli(interpreter::Cpu *cpu, uint32_t opcode) { SUB(LLI(false)); } // SUB Rd,Rn,Rm,LSL #i
void subLlr(interpreter::Cpu *cpu, uint32_t opcode) { SUB(LLR(false)); } // SUB Rd,Rn,Rm,LSL Rs
void subLri(interpreter::Cpu *cpu, uint32_t opcode) { SUB(LRI(false)); } // SUB Rd,Rn,Rm,LSR #i
void subLrr(interpreter::Cpu *cpu, uint32_t opcode) { SUB(LRR(false)); } // SUB Rd,Rn,Rm,LSR Rs
void subAri(interpreter::Cpu *cpu, uint32_t opcode) { SUB(ARI(false)); } // SUB Rd,Rn,Rm,ASR #i
void subArr(interpreter::Cpu *cpu, uint32_t opcode) { SUB(ARR(false)); } // SUB Rd,Rn,Rm,ASR Rs
void subRri(interpreter::Cpu *cpu, uint32_t opcode) { SUB(RRI(false)); } // SUB Rd,Rn,Rm,ROR #i
void subRrr(interpreter::Cpu *cpu, uint32_t opcode) { SUB(RRR(false)); } // SUB Rd,Rn,Rm,ROR Rs

void subsLli(interpreter::Cpu *cpu, uint32_t opcode) { SUBS(LLI(true)); } // SUBS Rd,Rn,Rm,LSL #i
void subsLlr(interpreter::Cpu *cpu, uint32_t opcode) { SUBS(LLR(true)); } // SUBS Rd,Rn,Rm,LSL Rs
void subsLri(interpreter::Cpu *cpu, uint32_t opcode) { SUBS(LRI(true)); } // SUBS Rd,Rn,Rm,LSR #i
void subsLrr(interpreter::Cpu *cpu, uint32_t opcode) { SUBS(LRR(true)); } // SUBS Rd,Rn,Rm,LSR Rs
void subsAri(interpreter::Cpu *cpu, uint32_t opcode) { SUBS(ARI(true)); } // SUBS Rd,Rn,Rm,ASR #i
void subsArr(interpreter::Cpu *cpu, uint32_t opcode) { SUBS(ARR(true)); } // SUBS Rd,Rn,Rm,ASR Rs
void subsRri(interpreter::Cpu *cpu, uint32_t opcode) { SUBS(RRI(true)); } // SUBS Rd,Rn,Rm,ROR #i
void subsRrr(interpreter::Cpu *cpu, uint32_t opcode) { SUBS(RRR(true)); } // SUBS Rd,Rn,Rm,ROR Rs

void rsbLli(interpreter::Cpu *cpu, uint32_t opcode) { RSB(LLI(false)); } // RSB Rd,Rn,Rm,LSL #i
void rsbLlr(interpreter::Cpu *cpu, uint32_t opcode) { RSB(LLR(false)); } // RSB Rd,Rn,Rm,LSL Rs
void rsbLri(interpreter::Cpu *cpu, uint32_t opcode) { RSB(LRI(false)); } // RSB Rd,Rn,Rm,LSR #i
void rsbLrr(interpreter::Cpu *cpu, uint32_t opcode) { RSB(LRR(false)); } // RSB Rd,Rn,Rm,LSR Rs
void rsbAri(interpreter::Cpu *cpu, uint32_t opcode) { RSB(ARI(false)); } // RSB Rd,Rn,Rm,ASR #i
void rsbArr(interpreter::Cpu *cpu, uint32_t opcode) { RSB(ARR(false)); } // RSB Rd,Rn,Rm,ASR Rs
void rsbRri(interpreter::Cpu *cpu, uint32_t opcode) { RSB(RRI(false)); } // RSB Rd,Rn,Rm,ROR #i
void rsbRrr(interpreter::Cpu *cpu, uint32_t opcode) { RSB(RRR(false)); } // RSB Rd,Rn,Rm,ROR Rs

void rsbsLli(interpreter::Cpu *cpu, uint32_t opcode) { RSBS(LLI(true)); } // RSBS Rd,Rn,Rm,LSL #i
void rsbsLlr(interpreter::Cpu *cpu, uint32_t opcode) { RSBS(LLR(true)); } // RSBS Rd,Rn,Rm,LSL Rs
void rsbsLri(interpreter::Cpu *cpu, uint32_t opcode) { RSBS(LRI(true)); } // RSBS Rd,Rn,Rm,LSR #i
void rsbsLrr(interpreter::Cpu *cpu, uint32_t opcode) { RSBS(LRR(true)); } // RSBS Rd,Rn,Rm,LSR Rs
void rsbsAri(interpreter::Cpu *cpu, uint32_t opcode) { RSBS(ARI(true)); } // RSBS Rd,Rn,Rm,ASR #i
void rsbsArr(interpreter::Cpu *cpu, uint32_t opcode) { RSBS(ARR(true)); } // RSBS Rd,Rn,Rm,ASR Rs
void rsbsRri(interpreter::Cpu *cpu, uint32_t opcode) { RSBS(RRI(true)); } // RSBS Rd,Rn,Rm,ROR #i
void rsbsRrr(interpreter::Cpu *cpu, uint32_t opcode) { RSBS(RRR(true)); } // RSBS Rd,Rn,Rm,ROR Rs

void addLli(interpreter::Cpu *cpu, uint32_t opcode) { ADD(LLI(false)); } // ADD Rd,Rn,Rm,LSL #i
void addLlr(interpreter::Cpu *cpu, uint32_t opcode) { ADD(LLR(false)); } // ADD Rd,Rn,Rm,LSL Rs
void addLri(interpreter::Cpu *cpu, uint32_t opcode) { ADD(LRI(false)); } // ADD Rd,Rn,Rm,LSR #i
void addLrr(interpreter::Cpu *cpu, uint32_t opcode) { ADD(LRR(false)); } // ADD Rd,Rn,Rm,LSR Rs
void addAri(interpreter::Cpu *cpu, uint32_t opcode) { ADD(ARI(false)); } // ADD Rd,Rn,Rm,ASR #i
void addArr(interpreter::Cpu *cpu, uint32_t opcode) { ADD(ARR(false)); } // ADD Rd,Rn,Rm,ASR Rs
void addRri(interpreter::Cpu *cpu, uint32_t opcode) { ADD(RRI(false)); } // ADD Rd,Rn,Rm,ROR #i
void addRrr(interpreter::Cpu *cpu, uint32_t opcode) { ADD(RRR(false)); } // ADD Rd,Rn,Rm,ROR Rs

void addsLli(interpreter::Cpu *cpu, uint32_t opcode) { ADDS(LLI(true)); } // ADDS Rd,Rn,Rm,LSL #i
void addsLlr(interpreter::Cpu *cpu, uint32_t opcode) { ADDS(LLR(true)); } // ADDS Rd,Rn,Rm,LSL Rs
void addsLri(interpreter::Cpu *cpu, uint32_t opcode) { ADDS(LRI(true)); } // ADDS Rd,Rn,Rm,LSR #i
void addsLrr(interpreter::Cpu *cpu, uint32_t opcode) { ADDS(LRR(true)); } // ADDS Rd,Rn,Rm,LSR Rs
void addsAri(interpreter::Cpu *cpu, uint32_t opcode) { ADDS(ARI(true)); } // ADDS Rd,Rn,Rm,ASR #i
void addsArr(interpreter::Cpu *cpu, uint32_t opcode) { ADDS(ARR(true)); } // ADDS Rd,Rn,Rm,ASR Rs
void addsRri(interpreter::Cpu *cpu, uint32_t opcode) { ADDS(RRI(true)); } // ADDS Rd,Rn,Rm,ROR #i
void addsRrr(interpreter::Cpu *cpu, uint32_t opcode) { ADDS(RRR(true)); } // ADDS Rd,Rn,Rm,ROR Rs

void adcLli(interpreter::Cpu *cpu, uint32_t opcode) { ADC(LLI(false)); } // ADC Rd,Rn,Rm,LSL #i
void adcLlr(interpreter::Cpu *cpu, uint32_t opcode) { ADC(LLR(false)); } // ADC Rd,Rn,Rm,LSL Rs
void adcLri(interpreter::Cpu *cpu, uint32_t opcode) { ADC(LRI(false)); } // ADC Rd,Rn,Rm,LSR #i
void adcLrr(interpreter::Cpu *cpu, uint32_t opcode) { ADC(LRR(false)); } // ADC Rd,Rn,Rm,LSR Rs
void adcAri(interpreter::Cpu *cpu, uint32_t opcode) { ADC(ARI(false)); } // ADC Rd,Rn,Rm,ASR #i
void adcArr(interpreter::Cpu *cpu, uint32_t opcode) { ADC(ARR(false)); } // ADC Rd,Rn,Rm,ASR Rs
void adcRri(interpreter::Cpu *cpu, uint32_t opcode) { ADC(RRI(false)); } // ADC Rd,Rn,Rm,ROR #i
void adcRrr(interpreter::Cpu *cpu, uint32_t opcode) { ADC(RRR(false)); } // ADC Rd,Rn,Rm,ROR Rs

void adcsLli(interpreter::Cpu *cpu, uint32_t opcode) { ADCS(LLI(true)); } // ADCS Rd,Rn,Rm,LSL #i
void adcsLlr(interpreter::Cpu *cpu, uint32_t opcode) { ADCS(LLR(true)); } // ADCS Rd,Rn,Rm,LSL Rs
void adcsLri(interpreter::Cpu *cpu, uint32_t opcode) { ADCS(LRI(true)); } // ADCS Rd,Rn,Rm,LSR #i
void adcsLrr(interpreter::Cpu *cpu, uint32_t opcode) { ADCS(LRR(true)); } // ADCS Rd,Rn,Rm,LSR Rs
void adcsAri(interpreter::Cpu *cpu, uint32_t opcode) { ADCS(ARI(true)); } // ADCS Rd,Rn,Rm,ASR #i
void adcsArr(interpreter::Cpu *cpu, uint32_t opcode) { ADCS(ARR(true)); } // ADCS Rd,Rn,Rm,ASR Rs
void adcsRri(interpreter::Cpu *cpu, uint32_t opcode) { ADCS(RRI(true)); } // ADCS Rd,Rn,Rm,ROR #i
void adcsRrr(interpreter::Cpu *cpu, uint32_t opcode) { ADCS(RRR(true)); } // ADCS Rd,Rn,Rm,ROR Rs

void sbcLli(interpreter::Cpu *cpu, uint32_t opcode) { SBC(LLI(false)); } // SBC Rd,Rn,Rm,LSL #i
void sbcLlr(interpreter::Cpu *cpu, uint32_t opcode) { SBC(LLR(false)); } // SBC Rd,Rn,Rm,LSL Rs
void sbcLri(interpreter::Cpu *cpu, uint32_t opcode) { SBC(LRI(false)); } // SBC Rd,Rn,Rm,LSR #i
void sbcLrr(interpreter::Cpu *cpu, uint32_t opcode) { SBC(LRR(false)); } // SBC Rd,Rn,Rm,LSR Rs
void sbcAri(interpreter::Cpu *cpu, uint32_t opcode) { SBC(ARI(false)); } // SBC Rd,Rn,Rm,ASR #i
void sbcArr(interpreter::Cpu *cpu, uint32_t opcode) { SBC(ARR(false)); } // SBC Rd,Rn,Rm,ASR Rs
void sbcRri(interpreter::Cpu *cpu, uint32_t opcode) { SBC(RRI(false)); } // SBC Rd,Rn,Rm,ROR #i
void sbcRrr(interpreter::Cpu *cpu, uint32_t opcode) { SBC(RRR(false)); } // SBC Rd,Rn,Rm,ROR Rs

void sbcsLli(interpreter::Cpu *cpu, uint32_t opcode) { SBCS(LLI(true)); } // SBCS Rd,Rn,Rm,LSL #i
void sbcsLlr(interpreter::Cpu *cpu, uint32_t opcode) { SBCS(LLR(true)); } // SBCS Rd,Rn,Rm,LSL Rs
void sbcsLri(interpreter::Cpu *cpu, uint32_t opcode) { SBCS(LRI(true)); } // SBCS Rd,Rn,Rm,LSR #i
void sbcsLrr(interpreter::Cpu *cpu, uint32_t opcode) { SBCS(LRR(true)); } // SBCS Rd,Rn,Rm,LSR Rs
void sbcsAri(interpreter::Cpu *cpu, uint32_t opcode) { SBCS(ARI(true)); } // SBCS Rd,Rn,Rm,ASR #i
void sbcsArr(interpreter::Cpu *cpu, uint32_t opcode) { SBCS(ARR(true)); } // SBCS Rd,Rn,Rm,ASR Rs
void sbcsRri(interpreter::Cpu *cpu, uint32_t opcode) { SBCS(RRI(true)); } // SBCS Rd,Rn,Rm,ROR #i
void sbcsRrr(interpreter::Cpu *cpu, uint32_t opcode) { SBCS(RRR(true)); } // SBCS Rd,Rn,Rm,ROR Rs

void rscLli(interpreter::Cpu *cpu, uint32_t opcode) { RSC(LLI(false)); } // RSC Rd,Rn,Rm,LSL #i
void rscLlr(interpreter::Cpu *cpu, uint32_t opcode) { RSC(LLR(false)); } // RSC Rd,Rn,Rm,LSL Rs
void rscLri(interpreter::Cpu *cpu, uint32_t opcode) { RSC(LRI(false)); } // RSC Rd,Rn,Rm,LSR #i
void rscLrr(interpreter::Cpu *cpu, uint32_t opcode) { RSC(LRR(false)); } // RSC Rd,Rn,Rm,LSR Rs
void rscAri(interpreter::Cpu *cpu, uint32_t opcode) { RSC(ARI(false)); } // RSC Rd,Rn,Rm,ASR #i
void rscArr(interpreter::Cpu *cpu, uint32_t opcode) { RSC(ARR(false)); } // RSC Rd,Rn,Rm,ASR Rs
void rscRri(interpreter::Cpu *cpu, uint32_t opcode) { RSC(RRI(false)); } // RSC Rd,Rn,Rm,ROR #i
void rscRrr(interpreter::Cpu *cpu, uint32_t opcode) { RSC(RRR(false)); } // RSC Rd,Rn,Rm,ROR Rs

void rscsLli(interpreter::Cpu *cpu, uint32_t opcode) { RSCS(LLI(true)); } // RSCS Rd,Rn,Rm,LSL #i
void rscsLlr(interpreter::Cpu *cpu, uint32_t opcode) { RSCS(LLR(true)); } // RSCS Rd,Rn,Rm,LSL Rs
void rscsLri(interpreter::Cpu *cpu, uint32_t opcode) { RSCS(LRI(true)); } // RSCS Rd,Rn,Rm,LSR #i
void rscsLrr(interpreter::Cpu *cpu, uint32_t opcode) { RSCS(LRR(true)); } // RSCS Rd,Rn,Rm,LSR Rs
void rscsAri(interpreter::Cpu *cpu, uint32_t opcode) { RSCS(ARI(true)); } // RSCS Rd,Rn,Rm,ASR #i
void rscsArr(interpreter::Cpu *cpu, uint32_t opcode) { RSCS(ARR(true)); } // RSCS Rd,Rn,Rm,ASR Rs
void rscsRri(interpreter::Cpu *cpu, uint32_t opcode) { RSCS(RRI(true)); } // RSCS Rd,Rn,Rm,ROR #i
void rscsRrr(interpreter::Cpu *cpu, uint32_t opcode) { RSCS(RRR(true)); } // RSCS Rd,Rn,Rm,ROR Rs

void tstLli(interpreter::Cpu *cpu, uint32_t opcode) { TST(LLI(false)); } // TST Rd,Rn,Rm,LSL #i
void tstLlr(interpreter::Cpu *cpu, uint32_t opcode) { TST(LLR(false)); } // TST Rd,Rn,Rm,LSL Rs
void tstLri(interpreter::Cpu *cpu, uint32_t opcode) { TST(LRI(false)); } // TST Rd,Rn,Rm,LSR #i
void tstLrr(interpreter::Cpu *cpu, uint32_t opcode) { TST(LRR(false)); } // TST Rd,Rn,Rm,LSR Rs
void tstAri(interpreter::Cpu *cpu, uint32_t opcode) { TST(ARI(false)); } // TST Rd,Rn,Rm,ASR #i
void tstArr(interpreter::Cpu *cpu, uint32_t opcode) { TST(ARR(false)); } // TST Rd,Rn,Rm,ASR Rs
void tstRri(interpreter::Cpu *cpu, uint32_t opcode) { TST(RRI(false)); } // TST Rd,Rn,Rm,ROR #i
void tstRrr(interpreter::Cpu *cpu, uint32_t opcode) { TST(RRR(false)); } // TST Rd,Rn,Rm,ROR Rs

void teqLli(interpreter::Cpu *cpu, uint32_t opcode) { TEQ(LLI(false)); } // TEQ Rd,Rn,Rm,LSL #i
void teqLlr(interpreter::Cpu *cpu, uint32_t opcode) { TEQ(LLR(false)); } // TEQ Rd,Rn,Rm,LSL Rs
void teqLri(interpreter::Cpu *cpu, uint32_t opcode) { TEQ(LRI(false)); } // TEQ Rd,Rn,Rm,LSR #i
void teqLrr(interpreter::Cpu *cpu, uint32_t opcode) { TEQ(LRR(false)); } // TEQ Rd,Rn,Rm,LSR Rs
void teqAri(interpreter::Cpu *cpu, uint32_t opcode) { TEQ(ARI(false)); } // TEQ Rd,Rn,Rm,ASR #i
void teqArr(interpreter::Cpu *cpu, uint32_t opcode) { TEQ(ARR(false)); } // TEQ Rd,Rn,Rm,ASR Rs
void teqRri(interpreter::Cpu *cpu, uint32_t opcode) { TEQ(RRI(false)); } // TEQ Rd,Rn,Rm,ROR #i
void teqRrr(interpreter::Cpu *cpu, uint32_t opcode) { TEQ(RRR(false)); } // TEQ Rd,Rn,Rm,ROR Rs

void cmpLli(interpreter::Cpu *cpu, uint32_t opcode) { CMP(LLI(false)); } // CMP Rd,Rn,Rm,LSL #i
void cmpLlr(interpreter::Cpu *cpu, uint32_t opcode) { CMP(LLR(false)); } // CMP Rd,Rn,Rm,LSL Rs
void cmpLri(interpreter::Cpu *cpu, uint32_t opcode) { CMP(LRI(false)); } // CMP Rd,Rn,Rm,LSR #i
void cmpLrr(interpreter::Cpu *cpu, uint32_t opcode) { CMP(LRR(false)); } // CMP Rd,Rn,Rm,LSR Rs
void cmpAri(interpreter::Cpu *cpu, uint32_t opcode) { CMP(ARI(false)); } // CMP Rd,Rn,Rm,ASR #i
void cmpArr(interpreter::Cpu *cpu, uint32_t opcode) { CMP(ARR(false)); } // CMP Rd,Rn,Rm,ASR Rs
void cmpRri(interpreter::Cpu *cpu, uint32_t opcode) { CMP(RRI(false)); } // CMP Rd,Rn,Rm,ROR #i
void cmpRrr(interpreter::Cpu *cpu, uint32_t opcode) { CMP(RRR(false)); } // CMP Rd,Rn,Rm,ROR Rs

void cmnLli(interpreter::Cpu *cpu, uint32_t opcode) { CMN(LLI(false)); } // CMN Rd,Rn,Rm,LSL #i
void cmnLlr(interpreter::Cpu *cpu, uint32_t opcode) { CMN(LLR(false)); } // CMN Rd,Rn,Rm,LSL Rs
void cmnLri(interpreter::Cpu *cpu, uint32_t opcode) { CMN(LRI(false)); } // CMN Rd,Rn,Rm,LSR #i
void cmnLrr(interpreter::Cpu *cpu, uint32_t opcode) { CMN(LRR(false)); } // CMN Rd,Rn,Rm,LSR Rs
void cmnAri(interpreter::Cpu *cpu, uint32_t opcode) { CMN(ARI(false)); } // CMN Rd,Rn,Rm,ASR #i
void cmnArr(interpreter::Cpu *cpu, uint32_t opcode) { CMN(ARR(false)); } // CMN Rd,Rn,Rm,ASR Rs
void cmnRri(interpreter::Cpu *cpu, uint32_t opcode) { CMN(RRI(false)); } // CMN Rd,Rn,Rm,ROR #i
void cmnRrr(interpreter::Cpu *cpu, uint32_t opcode) { CMN(RRR(false)); } // CMN Rd,Rn,Rm,ROR Rs

void orrLli(interpreter::Cpu *cpu, uint32_t opcode) { ORR(LLI(false)); } // ORR Rd,Rn,Rm,LSL #i
void orrLlr(interpreter::Cpu *cpu, uint32_t opcode) { ORR(LLR(false)); } // ORR Rd,Rn,Rm,LSL Rs
void orrLri(interpreter::Cpu *cpu, uint32_t opcode) { ORR(LRI(false)); } // ORR Rd,Rn,Rm,LSR #i
void orrLrr(interpreter::Cpu *cpu, uint32_t opcode) { ORR(LRR(false)); } // ORR Rd,Rn,Rm,LSR Rs
void orrAri(interpreter::Cpu *cpu, uint32_t opcode) { ORR(ARI(false)); } // ORR Rd,Rn,Rm,ASR #i
void orrArr(interpreter::Cpu *cpu, uint32_t opcode) { ORR(ARR(false)); } // ORR Rd,Rn,Rm,ASR Rs
void orrRri(interpreter::Cpu *cpu, uint32_t opcode) { ORR(RRI(false)); } // ORR Rd,Rn,Rm,ROR #i
void orrRrr(interpreter::Cpu *cpu, uint32_t opcode) { ORR(RRR(false)); } // ORR Rd,Rn,Rm,ROR Rs

void orrsLli(interpreter::Cpu *cpu, uint32_t opcode) { ORRS(LLI(true)); } // ORRS Rd,Rn,Rm,LSL #i
void orrsLlr(interpreter::Cpu *cpu, uint32_t opcode) { ORRS(LLR(true)); } // ORRS Rd,Rn,Rm,LSL Rs
void orrsLri(interpreter::Cpu *cpu, uint32_t opcode) { ORRS(LRI(true)); } // ORRS Rd,Rn,Rm,LSR #i
void orrsLrr(interpreter::Cpu *cpu, uint32_t opcode) { ORRS(LRR(true)); } // ORRS Rd,Rn,Rm,LSR Rs
void orrsAri(interpreter::Cpu *cpu, uint32_t opcode) { ORRS(ARI(true)); } // ORRS Rd,Rn,Rm,ASR #i
void orrsArr(interpreter::Cpu *cpu, uint32_t opcode) { ORRS(ARR(true)); } // ORRS Rd,Rn,Rm,ASR Rs
void orrsRri(interpreter::Cpu *cpu, uint32_t opcode) { ORRS(RRI(true)); } // ORRS Rd,Rn,Rm,ROR #i
void orrsRrr(interpreter::Cpu *cpu, uint32_t opcode) { ORRS(RRR(true)); } // ORRS Rd,Rn,Rm,ROR Rs

void movLli(interpreter::Cpu *cpu, uint32_t opcode) { MOV(LLI(false)); } // MOV Rd,Rm,LSL #i
void movLlr(interpreter::Cpu *cpu, uint32_t opcode) { MOV(LLR(false)); } // MOV Rd,Rm,LSL Rs
void movLri(interpreter::Cpu *cpu, uint32_t opcode) { MOV(LRI(false)); } // MOV Rd,Rm,LSR #i
void movLrr(interpreter::Cpu *cpu, uint32_t opcode) { MOV(LRR(false)); } // MOV Rd,Rm,LSR Rs
void movAri(interpreter::Cpu *cpu, uint32_t opcode) { MOV(ARI(false)); } // MOV Rd,Rm,ASR #i
void movArr(interpreter::Cpu *cpu, uint32_t opcode) { MOV(ARR(false)); } // MOV Rd,Rm,ASR Rs
void movRri(interpreter::Cpu *cpu, uint32_t opcode) { MOV(RRI(false)); } // MOV Rd,Rm,ROR #i
void movRrr(interpreter::Cpu *cpu, uint32_t opcode) { MOV(RRR(false)); } // MOV Rd,Rm,ROR Rs

void movsLli(interpreter::Cpu *cpu, uint32_t opcode) { MOVS(LLI(true)); } // MOVS Rd,Rm,LSL #i
void movsLlr(interpreter::Cpu *cpu, uint32_t opcode) { MOVS(LLR(true)); } // MOVS Rd,Rm,LSL Rs
void movsLri(interpreter::Cpu *cpu, uint32_t opcode) { MOVS(LRI(true)); } // MOVS Rd,Rm,LSR #i
void movsLrr(interpreter::Cpu *cpu, uint32_t opcode) { MOVS(LRR(true)); } // MOVS Rd,Rm,LSR Rs
void movsAri(interpreter::Cpu *cpu, uint32_t opcode) { MOVS(ARI(true)); } // MOVS Rd,Rm,ASR #i
void movsArr(interpreter::Cpu *cpu, uint32_t opcode) { MOVS(ARR(true)); } // MOVS Rd,Rm,ASR Rs
void movsRri(interpreter::Cpu *cpu, uint32_t opcode) { MOVS(RRI(true)); } // MOVS Rd,Rm,ROR #i
void movsRrr(interpreter::Cpu *cpu, uint32_t opcode) { MOVS(RRR(true)); } // MOVS Rd,Rm,ROR Rs

void bicLli(interpreter::Cpu *cpu, uint32_t opcode) { BIC(LLI(false)); } // BIC Rd,Rn,Rm,LSL #i
void bicLlr(interpreter::Cpu *cpu, uint32_t opcode) { BIC(LLR(false)); } // BIC Rd,Rn,Rm,LSL Rs
void bicLri(interpreter::Cpu *cpu, uint32_t opcode) { BIC(LRI(false)); } // BIC Rd,Rn,Rm,LSR #i
void bicLrr(interpreter::Cpu *cpu, uint32_t opcode) { BIC(LRR(false)); } // BIC Rd,Rn,Rm,LSR Rs
void bicAri(interpreter::Cpu *cpu, uint32_t opcode) { BIC(ARI(false)); } // BIC Rd,Rn,Rm,ASR #i
void bicArr(interpreter::Cpu *cpu, uint32_t opcode) { BIC(ARR(false)); } // BIC Rd,Rn,Rm,ASR Rs
void bicRri(interpreter::Cpu *cpu, uint32_t opcode) { BIC(RRI(false)); } // BIC Rd,Rn,Rm,ROR #i
void bicRrr(interpreter::Cpu *cpu, uint32_t opcode) { BIC(RRR(false)); } // BIC Rd,Rn,Rm,ROR Rs

void bicsLli(interpreter::Cpu *cpu, uint32_t opcode) { BICS(LLI(true)); } // BICS Rd,Rn,Rm,LSL #i
void bicsLlr(interpreter::Cpu *cpu, uint32_t opcode) { BICS(LLR(true)); } // BICS Rd,Rn,Rm,LSL Rs
void bicsLri(interpreter::Cpu *cpu, uint32_t opcode) { BICS(LRI(true)); } // BICS Rd,Rn,Rm,LSR #i
void bicsLrr(interpreter::Cpu *cpu, uint32_t opcode) { BICS(LRR(true)); } // BICS Rd,Rn,Rm,LSR Rs
void bicsAri(interpreter::Cpu *cpu, uint32_t opcode) { BICS(ARI(true)); } // BICS Rd,Rn,Rm,ASR #i
void bicsArr(interpreter::Cpu *cpu, uint32_t opcode) { BICS(ARR(true)); } // BICS Rd,Rn,Rm,ASR Rs
void bicsRri(interpreter::Cpu *cpu, uint32_t opcode) { BICS(RRI(true)); } // BICS Rd,Rn,Rm,ROR #i
void bicsRrr(interpreter::Cpu *cpu, uint32_t opcode) { BICS(RRR(true)); } // BICS Rd,Rn,Rm,ROR Rs

void mvnLli(interpreter::Cpu *cpu, uint32_t opcode) { MVN(LLI(false)); } // MVN Rd,Rm,LSL #i
void mvnLlr(interpreter::Cpu *cpu, uint32_t opcode) { MVN(LLR(false)); } // MVN Rd,Rm,LSL Rs
void mvnLri(interpreter::Cpu *cpu, uint32_t opcode) { MVN(LRI(false)); } // MVN Rd,Rm,LSR #i
void mvnLrr(interpreter::Cpu *cpu, uint32_t opcode) { MVN(LRR(false)); } // MVN Rd,Rm,LSR Rs
void mvnAri(interpreter::Cpu *cpu, uint32_t opcode) { MVN(ARI(false)); } // MVN Rd,Rm,ASR #i
void mvnArr(interpreter::Cpu *cpu, uint32_t opcode) { MVN(ARR(false)); } // MVN Rd,Rm,ASR Rs
void mvnRri(interpreter::Cpu *cpu, uint32_t opcode) { MVN(RRI(false)); } // MVN Rd,Rm,ROR #i
void mvnRrr(interpreter::Cpu *cpu, uint32_t opcode) { MVN(RRR(false)); } // MVN Rd,Rm,ROR Rs

void mvnsLli(interpreter::Cpu *cpu, uint32_t opcode) { MVNS(LLI(true)); } // MVNS Rd,Rm,LSL #i
void mvnsLlr(interpreter::Cpu *cpu, uint32_t opcode) { MVNS(LLR(true)); } // MVNS Rd,Rm,LSL Rs
void mvnsLri(interpreter::Cpu *cpu, uint32_t opcode) { MVNS(LRI(true)); } // MVNS Rd,Rm,LSR #i
void mvnsLrr(interpreter::Cpu *cpu, uint32_t opcode) { MVNS(LRR(true)); } // MVNS Rd,Rm,LSR Rs
void mvnsAri(interpreter::Cpu *cpu, uint32_t opcode) { MVNS(ARI(true)); } // MVNS Rd,Rm,ASR #i
void mvnsArr(interpreter::Cpu *cpu, uint32_t opcode) { MVNS(ARR(true)); } // MVNS Rd,Rm,ASR Rs
void mvnsRri(interpreter::Cpu *cpu, uint32_t opcode) { MVNS(RRI(true)); } // MVNS Rd,Rm,ROR #i
void mvnsRrr(interpreter::Cpu *cpu, uint32_t opcode) { MVNS(RRR(true)); } // MVNS Rd,Rm,ROR Rs

void andImm (interpreter::Cpu *cpu, uint32_t opcode) { AND(IMM(false)); } // AND  Rd,Rn,#i
void andsImm(interpreter::Cpu *cpu, uint32_t opcode) { ANDS(IMM(true)); } // ANDS Rd,Rn,#i
void eorImm (interpreter::Cpu *cpu, uint32_t opcode) { EOR(IMM(false)); } // EOR  Rd,Rn,#i
void eorsImm(interpreter::Cpu *cpu, uint32_t opcode) { EORS(IMM(true)); } // EORS Rd,Rn,#i
void subImm (interpreter::Cpu *cpu, uint32_t opcode) { SUB(IMM(false)); } // SUB  Rd,Rn,#i
void subsImm(interpreter::Cpu *cpu, uint32_t opcode) { SUBS(IMM(true)); } // SUBS Rd,Rn,#i
void rsbImm (interpreter::Cpu *cpu, uint32_t opcode) { RSB(IMM(false)); } // RSB  Rd,Rn,#i
void rsbsImm(interpreter::Cpu *cpu, uint32_t opcode) { RSBS(IMM(true)); } // RSBS Rd,Rn,#i
void addImm (interpreter::Cpu *cpu, uint32_t opcode) { ADD(IMM(false)); } // ADD  Rd,Rn,#i
void addsImm(interpreter::Cpu *cpu, uint32_t opcode) { ADDS(IMM(true)); } // ADDS Rd,Rn,#i
void adcImm (interpreter::Cpu *cpu, uint32_t opcode) { ADC(IMM(false)); } // ADC  Rd,Rn,#i
void adcsImm(interpreter::Cpu *cpu, uint32_t opcode) { ADCS(IMM(true)); } // ADCS Rd,Rn,#i
void sbcImm (interpreter::Cpu *cpu, uint32_t opcode) { SBC(IMM(false)); } // SBC  Rd,Rn,#i
void sbcsImm(interpreter::Cpu *cpu, uint32_t opcode) { SBCS(IMM(true)); } // SBCS Rd,Rn,#i
void rscImm (interpreter::Cpu *cpu, uint32_t opcode) { RSC(IMM(false)); } // RSC  Rd,Rn,#i
void rscsImm(interpreter::Cpu *cpu, uint32_t opcode) { RSCS(IMM(true)); } // RSCS Rd,Rn,#i
void tstImm (interpreter::Cpu *cpu, uint32_t opcode) { TST(IMM(false)); } // TST  Rd,Rn,#i
void teqImm (interpreter::Cpu *cpu, uint32_t opcode) { TEQ(IMM(false)); } // TEQ  Rd,Rn,#i
void cmpImm (interpreter::Cpu *cpu, uint32_t opcode) { CMP(IMM(false)); } // CMP  Rd,Rn,#i
void cmnImm (interpreter::Cpu *cpu, uint32_t opcode) { CMN(IMM(false)); } // CMN  Rd,Rn,#i
void orrImm (interpreter::Cpu *cpu, uint32_t opcode) { ORR(IMM(false)); } // ORR  Rd,Rn,#i
void orrsImm(interpreter::Cpu *cpu, uint32_t opcode) { ORRS(IMM(true)); } // ORRS Rd,Rn,#i
void movImm (interpreter::Cpu *cpu, uint32_t opcode) { MOV(IMM(false)); } // MOV  Rd,#i
void movsImm(interpreter::Cpu *cpu, uint32_t opcode) { MOVS(IMM(true)); } // MOVS Rd,#i
void bicImm (interpreter::Cpu *cpu, uint32_t opcode) { BIC(IMM(false)); } // BIC  Rd,Rn,#i
void bicsImm(interpreter::Cpu *cpu, uint32_t opcode) { BICS(IMM(true)); } // BICS Rd,Rn,#i
void mvnImm (interpreter::Cpu *cpu, uint32_t opcode) { MVN(IMM(false)); } // MVN  Rd,#i
void mvnsImm(interpreter::Cpu *cpu, uint32_t opcode) { MVNS(IMM(true)); } // MVNS Rd,#i

}
