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

#define SET_FLAG(bit, cond) if (cond) cpu->cpsr |= BIT(bit); else cpu->cpsr &= ~BIT(bit);

#define RN (*cpu->registers[(opcode & 0x000F0000) >> 16])
#define RD (*cpu->registers[(opcode & 0x0000F000) >> 12])
#define RS (*cpu->registers[(opcode & 0x00000F00) >>  8])
#define RM (*cpu->registers[(opcode & 0x0000000F)])

#define LLI lsl(*cpu->registers[(opcode & 0x0000000F)], (opcode & 0x00000F80) >> 7)
#define LLR lsl(*cpu->registers[(opcode & 0x0000000F)], *cpu->registers[(opcode & 0x00000F00) >> 8])
#define LRI lsr(*cpu->registers[(opcode & 0x0000000F)], (opcode & 0x00000F80) >> 7)
#define LRR lsr(*cpu->registers[(opcode & 0x0000000F)], *cpu->registers[(opcode & 0x00000F00) >> 8])
#define ARI asr(*cpu->registers[(opcode & 0x0000000F)], (opcode & 0x00000F80) >> 7)
#define ARR asr(*cpu->registers[(opcode & 0x0000000F)], *cpu->registers[(opcode & 0x00000F00) >> 8])
#define RRI ror(*cpu->registers[(opcode & 0x0000000F)], (opcode & 0x00000F80) >> 7)
#define RRR ror(*cpu->registers[(opcode & 0x0000000F)], *cpu->registers[(opcode & 0x00000F00) >> 8])
#define IMM ror((opcode & 0x000000FF),                  (opcode & 0x00000F00) >> 7)

#define LLIS lsls(cpu, *cpu->registers[(opcode & 0x0000000F)], (opcode & 0x00000F80) >> 7)
#define LLRS lsls(cpu, *cpu->registers[(opcode & 0x0000000F)], *cpu->registers[(opcode & 0x00000F00) >> 8])
#define LRIS lsrs(cpu, *cpu->registers[(opcode & 0x0000000F)], (opcode & 0x00000F80) >> 7)
#define LRRS lsrs(cpu, *cpu->registers[(opcode & 0x0000000F)], *cpu->registers[(opcode & 0x00000F00) >> 8])
#define ARIS asrs(cpu, *cpu->registers[(opcode & 0x0000000F)], (opcode & 0x00000F80) >> 7)
#define ARRS asrs(cpu, *cpu->registers[(opcode & 0x0000000F)], *cpu->registers[(opcode & 0x00000F00) >> 8])
#define RRIS rors(cpu, *cpu->registers[(opcode & 0x0000000F)], (opcode & 0x00000F80) >> 7)
#define RRRS rors(cpu, *cpu->registers[(opcode & 0x0000000F)], *cpu->registers[(opcode & 0x00000F00) >> 8])
#define IMMS rors(cpu, (opcode & 0x000000FF),                  (opcode & 0x00000F00) >> 7)

#define LSL                 \
    return value << amount;

#define LSR                 \
    return value >> amount;

#define ASR                                       \
    for (int i = 0; i < amount; i++)              \
        value = (value & BIT(31)) | (value >> 1); \
    return value;

#define ROR                                       \
    for (int i = 0; i < amount; i++)              \
        value = (value << 31) | (value >> 1);     \
    return value;

#define AND(op2)   \
    RD = RN & op2;

#define EOR(op2)   \
    RD = RN ^ op2;

#define SUB(op2)        \
    uint32_t pre = RN;  \
    uint32_t sub = op2; \
    RD = pre - sub;

#define RSB(op2)        \
    uint32_t pre = op2; \
    uint32_t sub = RN;  \
    RD = pre - sub;

#define ADD(op2)        \
    uint32_t pre = RN;  \
    uint32_t add = op2; \
    RD = pre + add;

#define ADC(op2)                                    \
    uint32_t pre = RN;                              \
    uint32_t add = op2;                             \
    RD = pre + add + ((cpu->cpsr & BIT(29)) >> 29);

#define SBC(op2)                                        \
    uint32_t pre = RN;                                  \
    uint32_t sub = op2;                                 \
    RD = pre - sub - 1 + ((cpu->cpsr & BIT(29)) >> 29);

#define RSC(op2)                                        \
    uint32_t pre = op2;                                 \
    uint32_t sub = RN;                                  \
    RD = pre - sub - 1 + ((cpu->cpsr & BIT(29)) >> 29);

#define TST(op2)             \
    uint32_t res = RN & op2; \
    COMMON_FLAGS(res)

#define TEQ(op2)             \
    uint32_t res = RN ^ op2; \
    COMMON_FLAGS(res)

#define CMP(op2)              \
    uint32_t pre = RN;        \
    uint32_t sub = op2;       \
    uint32_t res = pre - sub; \
    SUB_FLAGS(res)

#define CMN(op2)              \
    uint32_t pre = RN;        \
    uint32_t add = op2;       \
    uint32_t res = pre + add; \
    ADD_FLAGS(res)

#define ORR(op2)   \
    RD = RN | op2;

#define MOV(op2) \
    RD = op2;

#define BIC(op2)    \
    RD = RN & ~op2;

#define MVN(op2) \
    RD = ~op2;

#define MUL       \
    RN = RM * RS;

#define MLA            \
    RN = RM * RS + RD;

#define UMULL                         \
    uint64_t res = (uint64_t)RM * RS; \
    RN = res >> 32;                   \
    RD = res;

#define UMLAL                         \
    uint64_t res = (uint64_t)RM * RS; \
    res += ((uint64_t)RN << 32) | RD; \
    RN = res >> 32;                   \
    RD = res;

#define SMULL                  \
    int64_t res = (int32_t)RM; \
    res *= (int32_t)RS;        \
    RN = res >> 32;            \
    RD = res;

#define SMLAL                        \
    int64_t res = (int32_t)RM;       \
    res *= (int32_t)RS;              \
    res += ((int64_t)RN << 32) | RD; \
    RN = res >> 32;                  \
    RD = res;

#define LSL_FLAGS                                                \
    if (amount > 0)                                              \
       SET_FLAG(29, amount <= 32 && (value & BIT(32 - amount)));

#define LSR_FLAGS                                               \
    if (amount > 0)                                             \
       SET_FLAG(29, amount <= 32 && (value & BIT(amount - 1)));

#define ASR_FLAGS                                              \
    if (amount > 0)                                            \
       SET_FLAG(29, amount > 32 || (value & BIT(amount - 1)));

#define ROR_FLAGS                                       \
    if (amount > 0)                                     \
        SET_FLAG(29, (value & BIT((amount - 1) % 32)));

#define COMMON_FLAGS(dst)                                   \
    if(&dst == cpu->registers[15] && cpu->spsr)             \
    {                                                       \
        cpu->cpsr = *cpu->spsr & ~0x0000001F;               \
        interpreter::setMode(cpu, *cpu->spsr & 0x0000001F); \
        return;                                             \
    }                                                       \
    SET_FLAG(31, dst & BIT(31))                             \
    SET_FLAG(30, dst == 0)

#define SUB_FLAGS(dst)                                                                     \
    COMMON_FLAGS(dst)                                                                      \
    SET_FLAG(29, pre >= dst)                                                               \
    SET_FLAG(28, (sub & BIT(31)) != (pre & BIT(31)) && (dst & BIT(31)) == (sub & BIT(31)))

#define ADD_FLAGS(dst)                                                                     \
    COMMON_FLAGS(dst)                                                                      \
    SET_FLAG(29, pre > dst)                                                                \
    SET_FLAG(28, (add & BIT(31)) == (pre & BIT(31)) && (dst & BIT(31)) != (add & BIT(31)))

#define ADC_FLAGS(dst)                                                                     \
    COMMON_FLAGS(dst)                                                                      \
    SET_FLAG(29, pre > dst || (add == 0xFFFFFFFF && (cpu->cpsr & BIT(29))))                \
    SET_FLAG(28, (add & BIT(31)) == (pre & BIT(31)) && (dst & BIT(31)) != (add & BIT(31)))

#define SBC_FLAGS(dst)                                                                     \
    COMMON_FLAGS(dst)                                                                      \
    SET_FLAG(29, pre >= dst && (sub != 0xFFFFFFFF || (cpu->cpsr & BIT(29))))               \
    SET_FLAG(28, (sub & BIT(31)) != (pre & BIT(31)) && (dst & BIT(31)) == (sub & BIT(31)))

#define MUL_FLAGS(dst)         \
    COMMON_FLAGS(dst)          \
    if (cpu->type == 7)        \
        cpu->cpsr &= ~BIT(29);

namespace interpreter_alu
{

uint32_t lsl(uint32_t value, uint8_t amount) { LSL }
uint32_t lsr(uint32_t value, uint8_t amount) { LSR }
uint32_t asr(uint32_t value, uint8_t amount) { ASR }
uint32_t ror(uint32_t value, uint8_t amount) { ROR }

uint32_t lsls(interpreter::Cpu *cpu, uint32_t value, uint8_t amount) { LSL_FLAGS LSL }
uint32_t lsrs(interpreter::Cpu *cpu, uint32_t value, uint8_t amount) { LSR_FLAGS LSR }
uint32_t asrs(interpreter::Cpu *cpu, uint32_t value, uint8_t amount) { ASR_FLAGS ASR }
uint32_t rors(interpreter::Cpu *cpu, uint32_t value, uint8_t amount) { ASR_FLAGS ROR }

void andLli(interpreter::Cpu *cpu, uint32_t opcode) { AND(LLI) } // AND Rd,Rn,Rm,LSL #i
void andLlr(interpreter::Cpu *cpu, uint32_t opcode) { AND(LLR) } // AND Rd,Rn,Rm,LSL Rs
void andLri(interpreter::Cpu *cpu, uint32_t opcode) { AND(LRI) } // AND Rd,Rn,Rm,LSR #i
void andLrr(interpreter::Cpu *cpu, uint32_t opcode) { AND(LRR) } // AND Rd,Rn,Rm,LSR Rs
void andAri(interpreter::Cpu *cpu, uint32_t opcode) { AND(ARI) } // AND Rd,Rn,Rm,ASR #i
void andArr(interpreter::Cpu *cpu, uint32_t opcode) { AND(ARR) } // AND Rd,Rn,Rm,ASR Rs
void andRri(interpreter::Cpu *cpu, uint32_t opcode) { AND(RRI) } // AND Rd,Rn,Rm,ROR #i
void andRrr(interpreter::Cpu *cpu, uint32_t opcode) { AND(RRR) } // AND Rd,Rn,Rm,ROR Rs

void mul(interpreter::Cpu *cpu, uint32_t opcode) { MUL } // MUL Rd,Rm,Rs

void andsLli(interpreter::Cpu *cpu, uint32_t opcode) { AND(LLIS) COMMON_FLAGS(RD) } // ANDS Rd,Rn,Rm,LSL #i
void andsLlr(interpreter::Cpu *cpu, uint32_t opcode) { AND(LLRS) COMMON_FLAGS(RD) } // ANDS Rd,Rn,Rm,LSL Rs
void andsLri(interpreter::Cpu *cpu, uint32_t opcode) { AND(LRIS) COMMON_FLAGS(RD) } // ANDS Rd,Rn,Rm,LSR #i
void andsLrr(interpreter::Cpu *cpu, uint32_t opcode) { AND(LRRS) COMMON_FLAGS(RD) } // ANDS Rd,Rn,Rm,LSR Rs
void andsAri(interpreter::Cpu *cpu, uint32_t opcode) { AND(ARIS) COMMON_FLAGS(RD) } // ANDS Rd,Rn,Rm,ASR #i
void andsArr(interpreter::Cpu *cpu, uint32_t opcode) { AND(ARRS) COMMON_FLAGS(RD) } // ANDS Rd,Rn,Rm,ASR Rs
void andsRri(interpreter::Cpu *cpu, uint32_t opcode) { AND(RRIS) COMMON_FLAGS(RD) } // ANDS Rd,Rn,Rm,ROR #i
void andsRrr(interpreter::Cpu *cpu, uint32_t opcode) { AND(RRRS) COMMON_FLAGS(RD) } // ANDS Rd,Rn,Rm,ROR Rs

void muls(interpreter::Cpu *cpu, uint32_t opcode) { MUL MUL_FLAGS(RN) } // MULS Rd,Rm,Rs

void eorLli(interpreter::Cpu *cpu, uint32_t opcode) { EOR(LLI) } // EOR Rd,Rn,Rm,LSL #i
void eorLlr(interpreter::Cpu *cpu, uint32_t opcode) { EOR(LLR) } // EOR Rd,Rn,Rm,LSL Rs
void eorLri(interpreter::Cpu *cpu, uint32_t opcode) { EOR(LRI) } // EOR Rd,Rn,Rm,LSR #i
void eorLrr(interpreter::Cpu *cpu, uint32_t opcode) { EOR(LRR) } // EOR Rd,Rn,Rm,LSR Rs
void eorAri(interpreter::Cpu *cpu, uint32_t opcode) { EOR(ARI) } // EOR Rd,Rn,Rm,ASR #i
void eorArr(interpreter::Cpu *cpu, uint32_t opcode) { EOR(ARR) } // EOR Rd,Rn,Rm,ASR Rs
void eorRri(interpreter::Cpu *cpu, uint32_t opcode) { EOR(RRI) } // EOR Rd,Rn,Rm,ROR #i
void eorRrr(interpreter::Cpu *cpu, uint32_t opcode) { EOR(RRR) } // EOR Rd,Rn,Rm,ROR Rs

void mla(interpreter::Cpu *cpu, uint32_t opcode) { MLA } // MLA Rd,Rm,Rs,Rn

void eorsLli(interpreter::Cpu *cpu, uint32_t opcode) { EOR(LLIS) COMMON_FLAGS(RD) } // EORS Rd,Rn,Rm,LSL #i
void eorsLlr(interpreter::Cpu *cpu, uint32_t opcode) { EOR(LLRS) COMMON_FLAGS(RD) } // EORS Rd,Rn,Rm,LSL Rs
void eorsLri(interpreter::Cpu *cpu, uint32_t opcode) { EOR(LRIS) COMMON_FLAGS(RD) } // EORS Rd,Rn,Rm,LSR #i
void eorsLrr(interpreter::Cpu *cpu, uint32_t opcode) { EOR(LRRS) COMMON_FLAGS(RD) } // EORS Rd,Rn,Rm,LSR Rs
void eorsAri(interpreter::Cpu *cpu, uint32_t opcode) { EOR(ARIS) COMMON_FLAGS(RD) } // EORS Rd,Rn,Rm,ASR #i
void eorsArr(interpreter::Cpu *cpu, uint32_t opcode) { EOR(ARRS) COMMON_FLAGS(RD) } // EORS Rd,Rn,Rm,ASR Rs
void eorsRri(interpreter::Cpu *cpu, uint32_t opcode) { EOR(RRIS) COMMON_FLAGS(RD) } // EORS Rd,Rn,Rm,ROR #i
void eorsRrr(interpreter::Cpu *cpu, uint32_t opcode) { EOR(RRRS) COMMON_FLAGS(RD) } // EORS Rd,Rn,Rm,ROR Rs

void mlas(interpreter::Cpu *cpu, uint32_t opcode) { MLA MUL_FLAGS(RN) } // MLAS Rd,Rm,Rs,Rn

void subLli(interpreter::Cpu *cpu, uint32_t opcode) { SUB(LLI) } // SUB Rd,Rn,Rm,LSL #i
void subLlr(interpreter::Cpu *cpu, uint32_t opcode) { SUB(LLR) } // SUB Rd,Rn,Rm,LSL Rs
void subLri(interpreter::Cpu *cpu, uint32_t opcode) { SUB(LRI) } // SUB Rd,Rn,Rm,LSR #i
void subLrr(interpreter::Cpu *cpu, uint32_t opcode) { SUB(LRR) } // SUB Rd,Rn,Rm,LSR Rs
void subAri(interpreter::Cpu *cpu, uint32_t opcode) { SUB(ARI) } // SUB Rd,Rn,Rm,ASR #i
void subArr(interpreter::Cpu *cpu, uint32_t opcode) { SUB(ARR) } // SUB Rd,Rn,Rm,ASR Rs
void subRri(interpreter::Cpu *cpu, uint32_t opcode) { SUB(RRI) } // SUB Rd,Rn,Rm,ROR #i
void subRrr(interpreter::Cpu *cpu, uint32_t opcode) { SUB(RRR) } // SUB Rd,Rn,Rm,ROR Rs

void subsLli(interpreter::Cpu *cpu, uint32_t opcode) { SUB(LLI) SUB_FLAGS(RD) } // SUBS Rd,Rn,Rm,LSL #i
void subsLlr(interpreter::Cpu *cpu, uint32_t opcode) { SUB(LLR) SUB_FLAGS(RD) } // SUBS Rd,Rn,Rm,LSL Rs
void subsLri(interpreter::Cpu *cpu, uint32_t opcode) { SUB(LRI) SUB_FLAGS(RD) } // SUBS Rd,Rn,Rm,LSR #i
void subsLrr(interpreter::Cpu *cpu, uint32_t opcode) { SUB(LRR) SUB_FLAGS(RD) } // SUBS Rd,Rn,Rm,LSR Rs
void subsAri(interpreter::Cpu *cpu, uint32_t opcode) { SUB(ARI) SUB_FLAGS(RD) } // SUBS Rd,Rn,Rm,ASR #i
void subsArr(interpreter::Cpu *cpu, uint32_t opcode) { SUB(ARR) SUB_FLAGS(RD) } // SUBS Rd,Rn,Rm,ASR Rs
void subsRri(interpreter::Cpu *cpu, uint32_t opcode) { SUB(RRI) SUB_FLAGS(RD) } // SUBS Rd,Rn,Rm,ROR #i
void subsRrr(interpreter::Cpu *cpu, uint32_t opcode) { SUB(RRR) SUB_FLAGS(RD) } // SUBS Rd,Rn,Rm,ROR Rs

void rsbLli(interpreter::Cpu *cpu, uint32_t opcode) { RSB(LLI) } // RSB Rd,Rn,Rm,LSL #i
void rsbLlr(interpreter::Cpu *cpu, uint32_t opcode) { RSB(LLR) } // RSB Rd,Rn,Rm,LSL Rs
void rsbLri(interpreter::Cpu *cpu, uint32_t opcode) { RSB(LRI) } // RSB Rd,Rn,Rm,LSR #i
void rsbLrr(interpreter::Cpu *cpu, uint32_t opcode) { RSB(LRR) } // RSB Rd,Rn,Rm,LSR Rs
void rsbAri(interpreter::Cpu *cpu, uint32_t opcode) { RSB(ARI) } // RSB Rd,Rn,Rm,ASR #i
void rsbArr(interpreter::Cpu *cpu, uint32_t opcode) { RSB(ARR) } // RSB Rd,Rn,Rm,ASR Rs
void rsbRri(interpreter::Cpu *cpu, uint32_t opcode) { RSB(RRI) } // RSB Rd,Rn,Rm,ROR #i
void rsbRrr(interpreter::Cpu *cpu, uint32_t opcode) { RSB(RRR) } // RSB Rd,Rn,Rm,ROR Rs

void rsbsLli(interpreter::Cpu *cpu, uint32_t opcode) { RSB(LLI) SUB_FLAGS(RD) } // RSBS Rd,Rn,Rm,LSL #i
void rsbsLlr(interpreter::Cpu *cpu, uint32_t opcode) { RSB(LLR) SUB_FLAGS(RD) } // RSBS Rd,Rn,Rm,LSL Rs
void rsbsLri(interpreter::Cpu *cpu, uint32_t opcode) { RSB(LRI) SUB_FLAGS(RD) } // RSBS Rd,Rn,Rm,LSR #i
void rsbsLrr(interpreter::Cpu *cpu, uint32_t opcode) { RSB(LRR) SUB_FLAGS(RD) } // RSBS Rd,Rn,Rm,LSR Rs
void rsbsAri(interpreter::Cpu *cpu, uint32_t opcode) { RSB(ARI) SUB_FLAGS(RD) } // RSBS Rd,Rn,Rm,ASR #i
void rsbsArr(interpreter::Cpu *cpu, uint32_t opcode) { RSB(ARR) SUB_FLAGS(RD) } // RSBS Rd,Rn,Rm,ASR Rs
void rsbsRri(interpreter::Cpu *cpu, uint32_t opcode) { RSB(RRI) SUB_FLAGS(RD) } // RSBS Rd,Rn,Rm,ROR #i
void rsbsRrr(interpreter::Cpu *cpu, uint32_t opcode) { RSB(RRR) SUB_FLAGS(RD) } // RSBS Rd,Rn,Rm,ROR Rs

void addLli(interpreter::Cpu *cpu, uint32_t opcode) { ADD(LLI) } // ADD Rd,Rn,Rm,LSL #i
void addLlr(interpreter::Cpu *cpu, uint32_t opcode) { ADD(LLR) } // ADD Rd,Rn,Rm,LSL Rs
void addLri(interpreter::Cpu *cpu, uint32_t opcode) { ADD(LRI) } // ADD Rd,Rn,Rm,LSR #i
void addLrr(interpreter::Cpu *cpu, uint32_t opcode) { ADD(LRR) } // ADD Rd,Rn,Rm,LSR Rs
void addAri(interpreter::Cpu *cpu, uint32_t opcode) { ADD(ARI) } // ADD Rd,Rn,Rm,ASR #i
void addArr(interpreter::Cpu *cpu, uint32_t opcode) { ADD(ARR) } // ADD Rd,Rn,Rm,ASR Rs
void addRri(interpreter::Cpu *cpu, uint32_t opcode) { ADD(RRI) } // ADD Rd,Rn,Rm,ROR #i
void addRrr(interpreter::Cpu *cpu, uint32_t opcode) { ADD(RRR) } // ADD Rd,Rn,Rm,ROR Rs

void umull(interpreter::Cpu *cpu, uint32_t opcode) { UMULL } // UMULL RdLo,RdHi,Rm,Rs

void addsLli(interpreter::Cpu *cpu, uint32_t opcode) { ADD(LLI) ADD_FLAGS(RD) } // ADDS Rd,Rn,Rm,LSL #i
void addsLlr(interpreter::Cpu *cpu, uint32_t opcode) { ADD(LLR) ADD_FLAGS(RD) } // ADDS Rd,Rn,Rm,LSL Rs
void addsLri(interpreter::Cpu *cpu, uint32_t opcode) { ADD(LRI) ADD_FLAGS(RD) } // ADDS Rd,Rn,Rm,LSR #i
void addsLrr(interpreter::Cpu *cpu, uint32_t opcode) { ADD(LRR) ADD_FLAGS(RD) } // ADDS Rd,Rn,Rm,LSR Rs
void addsAri(interpreter::Cpu *cpu, uint32_t opcode) { ADD(ARI) ADD_FLAGS(RD) } // ADDS Rd,Rn,Rm,ASR #i
void addsArr(interpreter::Cpu *cpu, uint32_t opcode) { ADD(ARR) ADD_FLAGS(RD) } // ADDS Rd,Rn,Rm,ASR Rs
void addsRri(interpreter::Cpu *cpu, uint32_t opcode) { ADD(RRI) ADD_FLAGS(RD) } // ADDS Rd,Rn,Rm,ROR #i
void addsRrr(interpreter::Cpu *cpu, uint32_t opcode) { ADD(RRR) ADD_FLAGS(RD) } // ADDS Rd,Rn,Rm,ROR Rs

void umulls(interpreter::Cpu *cpu, uint32_t opcode) { UMULL MUL_FLAGS(RN) } // UMULLS RdLo,RdHi,Rm,Rs

void adcLli(interpreter::Cpu *cpu, uint32_t opcode) { ADC(LLI) } // ADC Rd,Rn,Rm,LSL #i
void adcLlr(interpreter::Cpu *cpu, uint32_t opcode) { ADC(LLR) } // ADC Rd,Rn,Rm,LSL Rs
void adcLri(interpreter::Cpu *cpu, uint32_t opcode) { ADC(LRI) } // ADC Rd,Rn,Rm,LSR #i
void adcLrr(interpreter::Cpu *cpu, uint32_t opcode) { ADC(LRR) } // ADC Rd,Rn,Rm,LSR Rs
void adcAri(interpreter::Cpu *cpu, uint32_t opcode) { ADC(ARI) } // ADC Rd,Rn,Rm,ASR #i
void adcArr(interpreter::Cpu *cpu, uint32_t opcode) { ADC(ARR) } // ADC Rd,Rn,Rm,ASR Rs
void adcRri(interpreter::Cpu *cpu, uint32_t opcode) { ADC(RRI) } // ADC Rd,Rn,Rm,ROR #i
void adcRrr(interpreter::Cpu *cpu, uint32_t opcode) { ADC(RRR) } // ADC Rd,Rn,Rm,ROR Rs

void umlal(interpreter::Cpu *cpu, uint32_t opcode) { UMLAL } // UMLAL RdLo,RdHi,Rm,Rs

void adcsLli(interpreter::Cpu *cpu, uint32_t opcode) { ADC(LLI) ADC_FLAGS(RD) } // ADCS Rd,Rn,Rm,LSL #i
void adcsLlr(interpreter::Cpu *cpu, uint32_t opcode) { ADC(LLR) ADC_FLAGS(RD) } // ADCS Rd,Rn,Rm,LSL Rs
void adcsLri(interpreter::Cpu *cpu, uint32_t opcode) { ADC(LRI) ADC_FLAGS(RD) } // ADCS Rd,Rn,Rm,LSR #i
void adcsLrr(interpreter::Cpu *cpu, uint32_t opcode) { ADC(LRR) ADC_FLAGS(RD) } // ADCS Rd,Rn,Rm,LSR Rs
void adcsAri(interpreter::Cpu *cpu, uint32_t opcode) { ADC(ARI) ADC_FLAGS(RD) } // ADCS Rd,Rn,Rm,ASR #i
void adcsArr(interpreter::Cpu *cpu, uint32_t opcode) { ADC(ARR) ADC_FLAGS(RD) } // ADCS Rd,Rn,Rm,ASR Rs
void adcsRri(interpreter::Cpu *cpu, uint32_t opcode) { ADC(RRI) ADC_FLAGS(RD) } // ADCS Rd,Rn,Rm,ROR #i
void adcsRrr(interpreter::Cpu *cpu, uint32_t opcode) { ADC(RRR) ADC_FLAGS(RD) } // ADCS Rd,Rn,Rm,ROR Rs

void umlals(interpreter::Cpu *cpu, uint32_t opcode) { UMLAL MUL_FLAGS(RN) } // UMLALS RdLo,RdHi,Rm,Rs

void sbcLli(interpreter::Cpu *cpu, uint32_t opcode) { SBC(LLI) } // SBC Rd,Rn,Rm,LSL #i
void sbcLlr(interpreter::Cpu *cpu, uint32_t opcode) { SBC(LLR) } // SBC Rd,Rn,Rm,LSL Rs
void sbcLri(interpreter::Cpu *cpu, uint32_t opcode) { SBC(LRI) } // SBC Rd,Rn,Rm,LSR #i
void sbcLrr(interpreter::Cpu *cpu, uint32_t opcode) { SBC(LRR) } // SBC Rd,Rn,Rm,LSR Rs
void sbcAri(interpreter::Cpu *cpu, uint32_t opcode) { SBC(ARI) } // SBC Rd,Rn,Rm,ASR #i
void sbcArr(interpreter::Cpu *cpu, uint32_t opcode) { SBC(ARR) } // SBC Rd,Rn,Rm,ASR Rs
void sbcRri(interpreter::Cpu *cpu, uint32_t opcode) { SBC(RRI) } // SBC Rd,Rn,Rm,ROR #i
void sbcRrr(interpreter::Cpu *cpu, uint32_t opcode) { SBC(RRR) } // SBC Rd,Rn,Rm,ROR Rs

void smull(interpreter::Cpu *cpu, uint32_t opcode) { SMULL } // SMULL RdLo,RdHi,Rm,Rs

void sbcsLli(interpreter::Cpu *cpu, uint32_t opcode) { SBC(LLI) SBC_FLAGS(RD) } // SBCS Rd,Rn,Rm,LSL #i
void sbcsLlr(interpreter::Cpu *cpu, uint32_t opcode) { SBC(LLR) SBC_FLAGS(RD) } // SBCS Rd,Rn,Rm,LSL Rs
void sbcsLri(interpreter::Cpu *cpu, uint32_t opcode) { SBC(LRI) SBC_FLAGS(RD) } // SBCS Rd,Rn,Rm,LSR #i
void sbcsLrr(interpreter::Cpu *cpu, uint32_t opcode) { SBC(LRR) SBC_FLAGS(RD) } // SBCS Rd,Rn,Rm,LSR Rs
void sbcsAri(interpreter::Cpu *cpu, uint32_t opcode) { SBC(ARI) SBC_FLAGS(RD) } // SBCS Rd,Rn,Rm,ASR #i
void sbcsArr(interpreter::Cpu *cpu, uint32_t opcode) { SBC(ARR) SBC_FLAGS(RD) } // SBCS Rd,Rn,Rm,ASR Rs
void sbcsRri(interpreter::Cpu *cpu, uint32_t opcode) { SBC(RRI) SBC_FLAGS(RD) } // SBCS Rd,Rn,Rm,ROR #i
void sbcsRrr(interpreter::Cpu *cpu, uint32_t opcode) { SBC(RRR) SBC_FLAGS(RD) } // SBCS Rd,Rn,Rm,ROR Rs

void smulls(interpreter::Cpu *cpu, uint32_t opcode) { SMULL MUL_FLAGS(RN) } // SMULLS RdLo,RdHi,Rm,Rs

void rscLli(interpreter::Cpu *cpu, uint32_t opcode) { RSC(LLI) } // RSC Rd,Rn,Rm,LSL #i
void rscLlr(interpreter::Cpu *cpu, uint32_t opcode) { RSC(LLR) } // RSC Rd,Rn,Rm,LSL Rs
void rscLri(interpreter::Cpu *cpu, uint32_t opcode) { RSC(LRI) } // RSC Rd,Rn,Rm,LSR #i
void rscLrr(interpreter::Cpu *cpu, uint32_t opcode) { RSC(LRR) } // RSC Rd,Rn,Rm,LSR Rs
void rscAri(interpreter::Cpu *cpu, uint32_t opcode) { RSC(ARI) } // RSC Rd,Rn,Rm,ASR #i
void rscArr(interpreter::Cpu *cpu, uint32_t opcode) { RSC(ARR) } // RSC Rd,Rn,Rm,ASR Rs
void rscRri(interpreter::Cpu *cpu, uint32_t opcode) { RSC(RRI) } // RSC Rd,Rn,Rm,ROR #i
void rscRrr(interpreter::Cpu *cpu, uint32_t opcode) { RSC(RRR) } // RSC Rd,Rn,Rm,ROR Rs

void smlal(interpreter::Cpu *cpu, uint32_t opcode) { SMLAL } // SMLAL RdLo,RdHi,Rm,Rs

void rscsLli(interpreter::Cpu *cpu, uint32_t opcode) { RSC(LLI) SBC_FLAGS(RD) } // RSCS Rd,Rn,Rm,LSL #i
void rscsLlr(interpreter::Cpu *cpu, uint32_t opcode) { RSC(LLR) SBC_FLAGS(RD) } // RSCS Rd,Rn,Rm,LSL Rs
void rscsLri(interpreter::Cpu *cpu, uint32_t opcode) { RSC(LRI) SBC_FLAGS(RD) } // RSCS Rd,Rn,Rm,LSR #i
void rscsLrr(interpreter::Cpu *cpu, uint32_t opcode) { RSC(LRR) SBC_FLAGS(RD) } // RSCS Rd,Rn,Rm,LSR Rs
void rscsAri(interpreter::Cpu *cpu, uint32_t opcode) { RSC(ARI) SBC_FLAGS(RD) } // RSCS Rd,Rn,Rm,ASR #i
void rscsArr(interpreter::Cpu *cpu, uint32_t opcode) { RSC(ARR) SBC_FLAGS(RD) } // RSCS Rd,Rn,Rm,ASR Rs
void rscsRri(interpreter::Cpu *cpu, uint32_t opcode) { RSC(RRI) SBC_FLAGS(RD) } // RSCS Rd,Rn,Rm,ROR #i
void rscsRrr(interpreter::Cpu *cpu, uint32_t opcode) { RSC(RRR) SBC_FLAGS(RD) } // RSCS Rd,Rn,Rm,ROR Rs

void smlals(interpreter::Cpu *cpu, uint32_t opcode) { SMLAL MUL_FLAGS(RN) } // SMLALS RdLo,RdHi,Rm,Rs

void tstLli(interpreter::Cpu *cpu, uint32_t opcode) { TST(LLIS) } // TST Rd,Rn,Rm,LSL #i
void tstLlr(interpreter::Cpu *cpu, uint32_t opcode) { TST(LLRS) } // TST Rd,Rn,Rm,LSL Rs
void tstLri(interpreter::Cpu *cpu, uint32_t opcode) { TST(LRIS) } // TST Rd,Rn,Rm,LSR #i
void tstLrr(interpreter::Cpu *cpu, uint32_t opcode) { TST(LRRS) } // TST Rd,Rn,Rm,LSR Rs
void tstAri(interpreter::Cpu *cpu, uint32_t opcode) { TST(ARIS) } // TST Rd,Rn,Rm,ASR #i
void tstArr(interpreter::Cpu *cpu, uint32_t opcode) { TST(ARRS) } // TST Rd,Rn,Rm,ASR Rs
void tstRri(interpreter::Cpu *cpu, uint32_t opcode) { TST(RRIS) } // TST Rd,Rn,Rm,ROR #i
void tstRrr(interpreter::Cpu *cpu, uint32_t opcode) { TST(RRRS) } // TST Rd,Rn,Rm,ROR Rs

void teqLli(interpreter::Cpu *cpu, uint32_t opcode) { TEQ(LLIS) } // TEQ Rd,Rn,Rm,LSL #i
void teqLlr(interpreter::Cpu *cpu, uint32_t opcode) { TEQ(LLRS) } // TEQ Rd,Rn,Rm,LSL Rs
void teqLri(interpreter::Cpu *cpu, uint32_t opcode) { TEQ(LRIS) } // TEQ Rd,Rn,Rm,LSR #i
void teqLrr(interpreter::Cpu *cpu, uint32_t opcode) { TEQ(LRRS) } // TEQ Rd,Rn,Rm,LSR Rs
void teqAri(interpreter::Cpu *cpu, uint32_t opcode) { TEQ(ARIS) } // TEQ Rd,Rn,Rm,ASR #i
void teqArr(interpreter::Cpu *cpu, uint32_t opcode) { TEQ(ARRS) } // TEQ Rd,Rn,Rm,ASR Rs
void teqRri(interpreter::Cpu *cpu, uint32_t opcode) { TEQ(RRIS) } // TEQ Rd,Rn,Rm,ROR #i
void teqRrr(interpreter::Cpu *cpu, uint32_t opcode) { TEQ(RRRS) } // TEQ Rd,Rn,Rm,ROR Rs

void cmpLli(interpreter::Cpu *cpu, uint32_t opcode) { CMP(LLI) } // CMP Rd,Rn,Rm,LSL #i
void cmpLlr(interpreter::Cpu *cpu, uint32_t opcode) { CMP(LLR) } // CMP Rd,Rn,Rm,LSL Rs
void cmpLri(interpreter::Cpu *cpu, uint32_t opcode) { CMP(LRI) } // CMP Rd,Rn,Rm,LSR #i
void cmpLrr(interpreter::Cpu *cpu, uint32_t opcode) { CMP(LRR) } // CMP Rd,Rn,Rm,LSR Rs
void cmpAri(interpreter::Cpu *cpu, uint32_t opcode) { CMP(ARI) } // CMP Rd,Rn,Rm,ASR #i
void cmpArr(interpreter::Cpu *cpu, uint32_t opcode) { CMP(ARR) } // CMP Rd,Rn,Rm,ASR Rs
void cmpRri(interpreter::Cpu *cpu, uint32_t opcode) { CMP(RRI) } // CMP Rd,Rn,Rm,ROR #i
void cmpRrr(interpreter::Cpu *cpu, uint32_t opcode) { CMP(RRR) } // CMP Rd,Rn,Rm,ROR Rs

void cmnLli(interpreter::Cpu *cpu, uint32_t opcode) { CMN(LLI) } // CMN Rd,Rn,Rm,LSL #i
void cmnLlr(interpreter::Cpu *cpu, uint32_t opcode) { CMN(LLR) } // CMN Rd,Rn,Rm,LSL Rs
void cmnLri(interpreter::Cpu *cpu, uint32_t opcode) { CMN(LRI) } // CMN Rd,Rn,Rm,LSR #i
void cmnLrr(interpreter::Cpu *cpu, uint32_t opcode) { CMN(LRR) } // CMN Rd,Rn,Rm,LSR Rs
void cmnAri(interpreter::Cpu *cpu, uint32_t opcode) { CMN(ARI) } // CMN Rd,Rn,Rm,ASR #i
void cmnArr(interpreter::Cpu *cpu, uint32_t opcode) { CMN(ARR) } // CMN Rd,Rn,Rm,ASR Rs
void cmnRri(interpreter::Cpu *cpu, uint32_t opcode) { CMN(RRI) } // CMN Rd,Rn,Rm,ROR #i
void cmnRrr(interpreter::Cpu *cpu, uint32_t opcode) { CMN(RRR) } // CMN Rd,Rn,Rm,ROR Rs

void orrLli(interpreter::Cpu *cpu, uint32_t opcode) { ORR(LLI) } // ORR Rd,Rn,Rm,LSL #i
void orrLlr(interpreter::Cpu *cpu, uint32_t opcode) { ORR(LLR) } // ORR Rd,Rn,Rm,LSL Rs
void orrLri(interpreter::Cpu *cpu, uint32_t opcode) { ORR(LRI) } // ORR Rd,Rn,Rm,LSR #i
void orrLrr(interpreter::Cpu *cpu, uint32_t opcode) { ORR(LRR) } // ORR Rd,Rn,Rm,LSR Rs
void orrAri(interpreter::Cpu *cpu, uint32_t opcode) { ORR(ARI) } // ORR Rd,Rn,Rm,ASR #i
void orrArr(interpreter::Cpu *cpu, uint32_t opcode) { ORR(ARR) } // ORR Rd,Rn,Rm,ASR Rs
void orrRri(interpreter::Cpu *cpu, uint32_t opcode) { ORR(RRI) } // ORR Rd,Rn,Rm,ROR #i
void orrRrr(interpreter::Cpu *cpu, uint32_t opcode) { ORR(RRR) } // ORR Rd,Rn,Rm,ROR Rs

void orrsLli(interpreter::Cpu *cpu, uint32_t opcode) { ORR(LLIS) COMMON_FLAGS(RD) } // ORRS Rd,Rn,Rm,LSL #i
void orrsLlr(interpreter::Cpu *cpu, uint32_t opcode) { ORR(LLRS) COMMON_FLAGS(RD) } // ORRS Rd,Rn,Rm,LSL Rs
void orrsLri(interpreter::Cpu *cpu, uint32_t opcode) { ORR(LRIS) COMMON_FLAGS(RD) } // ORRS Rd,Rn,Rm,LSR #i
void orrsLrr(interpreter::Cpu *cpu, uint32_t opcode) { ORR(LRRS) COMMON_FLAGS(RD) } // ORRS Rd,Rn,Rm,LSR Rs
void orrsAri(interpreter::Cpu *cpu, uint32_t opcode) { ORR(ARIS) COMMON_FLAGS(RD) } // ORRS Rd,Rn,Rm,ASR #i
void orrsArr(interpreter::Cpu *cpu, uint32_t opcode) { ORR(ARRS) COMMON_FLAGS(RD) } // ORRS Rd,Rn,Rm,ASR Rs
void orrsRri(interpreter::Cpu *cpu, uint32_t opcode) { ORR(RRIS) COMMON_FLAGS(RD) } // ORRS Rd,Rn,Rm,ROR #i
void orrsRrr(interpreter::Cpu *cpu, uint32_t opcode) { ORR(RRRS) COMMON_FLAGS(RD) } // ORRS Rd,Rn,Rm,ROR Rs

void movLli(interpreter::Cpu *cpu, uint32_t opcode) { MOV(LLI) } // MOV Rd,Rm,LSL #i
void movLlr(interpreter::Cpu *cpu, uint32_t opcode) { MOV(LLR) } // MOV Rd,Rm,LSL Rs
void movLri(interpreter::Cpu *cpu, uint32_t opcode) { MOV(LRI) } // MOV Rd,Rm,LSR #i
void movLrr(interpreter::Cpu *cpu, uint32_t opcode) { MOV(LRR) } // MOV Rd,Rm,LSR Rs
void movAri(interpreter::Cpu *cpu, uint32_t opcode) { MOV(ARI) } // MOV Rd,Rm,ASR #i
void movArr(interpreter::Cpu *cpu, uint32_t opcode) { MOV(ARR) } // MOV Rd,Rm,ASR Rs
void movRri(interpreter::Cpu *cpu, uint32_t opcode) { MOV(RRI) } // MOV Rd,Rm,ROR #i
void movRrr(interpreter::Cpu *cpu, uint32_t opcode) { MOV(RRR) } // MOV Rd,Rm,ROR Rs

void movsLli(interpreter::Cpu *cpu, uint32_t opcode) { MOV(LLIS) COMMON_FLAGS(RD) } // MOVS Rd,Rm,LSL #i
void movsLlr(interpreter::Cpu *cpu, uint32_t opcode) { MOV(LLRS) COMMON_FLAGS(RD) } // MOVS Rd,Rm,LSL Rs
void movsLri(interpreter::Cpu *cpu, uint32_t opcode) { MOV(LRIS) COMMON_FLAGS(RD) } // MOVS Rd,Rm,LSR #i
void movsLrr(interpreter::Cpu *cpu, uint32_t opcode) { MOV(LRRS) COMMON_FLAGS(RD) } // MOVS Rd,Rm,LSR Rs
void movsAri(interpreter::Cpu *cpu, uint32_t opcode) { MOV(ARIS) COMMON_FLAGS(RD) } // MOVS Rd,Rm,ASR #i
void movsArr(interpreter::Cpu *cpu, uint32_t opcode) { MOV(ARRS) COMMON_FLAGS(RD) } // MOVS Rd,Rm,ASR Rs
void movsRri(interpreter::Cpu *cpu, uint32_t opcode) { MOV(RRIS) COMMON_FLAGS(RD) } // MOVS Rd,Rm,ROR #i
void movsRrr(interpreter::Cpu *cpu, uint32_t opcode) { MOV(RRRS) COMMON_FLAGS(RD) } // MOVS Rd,Rm,ROR Rs

void bicLli(interpreter::Cpu *cpu, uint32_t opcode) { BIC(LLI) } // BIC Rd,Rn,Rm,LSL #i
void bicLlr(interpreter::Cpu *cpu, uint32_t opcode) { BIC(LLR) } // BIC Rd,Rn,Rm,LSL Rs
void bicLri(interpreter::Cpu *cpu, uint32_t opcode) { BIC(LRI) } // BIC Rd,Rn,Rm,LSR #i
void bicLrr(interpreter::Cpu *cpu, uint32_t opcode) { BIC(LRR) } // BIC Rd,Rn,Rm,LSR Rs
void bicAri(interpreter::Cpu *cpu, uint32_t opcode) { BIC(ARI) } // BIC Rd,Rn,Rm,ASR #i
void bicArr(interpreter::Cpu *cpu, uint32_t opcode) { BIC(ARR) } // BIC Rd,Rn,Rm,ASR Rs
void bicRri(interpreter::Cpu *cpu, uint32_t opcode) { BIC(RRI) } // BIC Rd,Rn,Rm,ROR #i
void bicRrr(interpreter::Cpu *cpu, uint32_t opcode) { BIC(RRR) } // BIC Rd,Rn,Rm,ROR Rs

void bicsLli(interpreter::Cpu *cpu, uint32_t opcode) { BIC(LLIS) COMMON_FLAGS(RD) } // BICS Rd,Rn,Rm,LSL #i
void bicsLlr(interpreter::Cpu *cpu, uint32_t opcode) { BIC(LLRS) COMMON_FLAGS(RD) } // BICS Rd,Rn,Rm,LSL Rs
void bicsLri(interpreter::Cpu *cpu, uint32_t opcode) { BIC(LRIS) COMMON_FLAGS(RD) } // BICS Rd,Rn,Rm,LSR #i
void bicsLrr(interpreter::Cpu *cpu, uint32_t opcode) { BIC(LRRS) COMMON_FLAGS(RD) } // BICS Rd,Rn,Rm,LSR Rs
void bicsAri(interpreter::Cpu *cpu, uint32_t opcode) { BIC(ARIS) COMMON_FLAGS(RD) } // BICS Rd,Rn,Rm,ASR #i
void bicsArr(interpreter::Cpu *cpu, uint32_t opcode) { BIC(ARRS) COMMON_FLAGS(RD) } // BICS Rd,Rn,Rm,ASR Rs
void bicsRri(interpreter::Cpu *cpu, uint32_t opcode) { BIC(RRIS) COMMON_FLAGS(RD) } // BICS Rd,Rn,Rm,ROR #i
void bicsRrr(interpreter::Cpu *cpu, uint32_t opcode) { BIC(RRRS) COMMON_FLAGS(RD) } // BICS Rd,Rn,Rm,ROR Rs

void mvnLli(interpreter::Cpu *cpu, uint32_t opcode) { MVN(LLI) } // MVN Rd,Rm,LSL #i
void mvnLlr(interpreter::Cpu *cpu, uint32_t opcode) { MVN(LLR) } // MVN Rd,Rm,LSL Rs
void mvnLri(interpreter::Cpu *cpu, uint32_t opcode) { MVN(LRI) } // MVN Rd,Rm,LSR #i
void mvnLrr(interpreter::Cpu *cpu, uint32_t opcode) { MVN(LRR) } // MVN Rd,Rm,LSR Rs
void mvnAri(interpreter::Cpu *cpu, uint32_t opcode) { MVN(ARI) } // MVN Rd,Rm,ASR #i
void mvnArr(interpreter::Cpu *cpu, uint32_t opcode) { MVN(ARR) } // MVN Rd,Rm,ASR Rs
void mvnRri(interpreter::Cpu *cpu, uint32_t opcode) { MVN(RRI) } // MVN Rd,Rm,ROR #i
void mvnRrr(interpreter::Cpu *cpu, uint32_t opcode) { MVN(RRR) } // MVN Rd,Rm,ROR Rs

void mvnsLli(interpreter::Cpu *cpu, uint32_t opcode) { MVN(LLIS) COMMON_FLAGS(RD) } // MVNS Rd,Rm,LSL #i
void mvnsLlr(interpreter::Cpu *cpu, uint32_t opcode) { MVN(LLRS) COMMON_FLAGS(RD) } // MVNS Rd,Rm,LSL Rs
void mvnsLri(interpreter::Cpu *cpu, uint32_t opcode) { MVN(LRIS) COMMON_FLAGS(RD) } // MVNS Rd,Rm,LSR #i
void mvnsLrr(interpreter::Cpu *cpu, uint32_t opcode) { MVN(LRRS) COMMON_FLAGS(RD) } // MVNS Rd,Rm,LSR Rs
void mvnsAri(interpreter::Cpu *cpu, uint32_t opcode) { MVN(ARIS) COMMON_FLAGS(RD) } // MVNS Rd,Rm,ASR #i
void mvnsArr(interpreter::Cpu *cpu, uint32_t opcode) { MVN(ARRS) COMMON_FLAGS(RD) } // MVNS Rd,Rm,ASR Rs
void mvnsRri(interpreter::Cpu *cpu, uint32_t opcode) { MVN(RRIS) COMMON_FLAGS(RD) } // MVNS Rd,Rm,ROR #i
void mvnsRrr(interpreter::Cpu *cpu, uint32_t opcode) { MVN(RRRS) COMMON_FLAGS(RD) } // MVNS Rd,Rm,ROR Rs

void andImm (interpreter::Cpu *cpu, uint32_t opcode) { AND(IMM)                   } // AND  Rd,Rn,#i
void andsImm(interpreter::Cpu *cpu, uint32_t opcode) { AND(IMMS) COMMON_FLAGS(RD) } // ANDS Rd,Rn,#i
void eorImm (interpreter::Cpu *cpu, uint32_t opcode) { EOR(IMM)                   } // EOR  Rd,Rn,#i
void eorsImm(interpreter::Cpu *cpu, uint32_t opcode) { EOR(IMMS) COMMON_FLAGS(RD) } // EORS Rd,Rn,#i
void subImm (interpreter::Cpu *cpu, uint32_t opcode) { SUB(IMM)                   } // SUB  Rd,Rn,#i
void subsImm(interpreter::Cpu *cpu, uint32_t opcode) { SUB(IMM) SUB_FLAGS(RD)     } // SUBS Rd,Rn,#i
void rsbImm (interpreter::Cpu *cpu, uint32_t opcode) { RSB(IMM)                   } // RSB  Rd,Rn,#i
void rsbsImm(interpreter::Cpu *cpu, uint32_t opcode) { RSB(IMM) SUB_FLAGS(RD)     } // RSBS Rd,Rn,#i
void addImm (interpreter::Cpu *cpu, uint32_t opcode) { ADD(IMM)                   } // ADD  Rd,Rn,#i
void addsImm(interpreter::Cpu *cpu, uint32_t opcode) { ADD(IMM) ADD_FLAGS(RD)     } // ADDS Rd,Rn,#i
void adcImm (interpreter::Cpu *cpu, uint32_t opcode) { ADC(IMM)                   } // ADC  Rd,Rn,#i
void adcsImm(interpreter::Cpu *cpu, uint32_t opcode) { ADC(IMM) ADC_FLAGS(RD)     } // ADCS Rd,Rn,#i
void sbcImm (interpreter::Cpu *cpu, uint32_t opcode) { SBC(IMM)                   } // SBC  Rd,Rn,#i
void sbcsImm(interpreter::Cpu *cpu, uint32_t opcode) { SBC(IMM) SBC_FLAGS(RD)     } // SBCS Rd,Rn,#i
void rscImm (interpreter::Cpu *cpu, uint32_t opcode) { RSC(IMM)                   } // RSC  Rd,Rn,#i
void rscsImm(interpreter::Cpu *cpu, uint32_t opcode) { RSC(IMM) SBC_FLAGS(RD)     } // RSCS Rd,Rn,#i
void tstImm (interpreter::Cpu *cpu, uint32_t opcode) { TST(IMMS)                  } // TST  Rd,Rn,#i
void teqImm (interpreter::Cpu *cpu, uint32_t opcode) { TEQ(IMMS)                  } // TEQ  Rd,Rn,#i
void cmpImm (interpreter::Cpu *cpu, uint32_t opcode) { CMP(IMM)                   } // CMP  Rd,Rn,#i
void cmnImm (interpreter::Cpu *cpu, uint32_t opcode) { CMN(IMM)                   } // CMN  Rd,Rn,#i
void orrImm (interpreter::Cpu *cpu, uint32_t opcode) { ORR(IMM)                   } // ORR  Rd,Rn,#i
void orrsImm(interpreter::Cpu *cpu, uint32_t opcode) { ORR(IMMS) COMMON_FLAGS(RD) } // ORRS Rd,Rn,#i
void movImm (interpreter::Cpu *cpu, uint32_t opcode) { MOV(IMM)                   } // MOV  Rd,#i
void movsImm(interpreter::Cpu *cpu, uint32_t opcode) { MOV(IMMS) COMMON_FLAGS(RD) } // MOVS Rd,#i
void bicImm (interpreter::Cpu *cpu, uint32_t opcode) { BIC(IMM)                   } // BIC  Rd,Rn,#i
void bicsImm(interpreter::Cpu *cpu, uint32_t opcode) { BIC(IMMS) COMMON_FLAGS(RD) } // BICS Rd,Rn,#i
void mvnImm (interpreter::Cpu *cpu, uint32_t opcode) { MVN(IMM)                   } // MVN  Rd,#i
void mvnsImm(interpreter::Cpu *cpu, uint32_t opcode) { MVN(IMMS) COMMON_FLAGS(RD) } // MVNS Rd,#i

}
