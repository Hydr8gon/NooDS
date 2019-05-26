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

#ifndef INTERPRETER_ALU
#define INTERPRETER_ALU

#include "interpreter.h"

namespace interpreter_alu
{

void andLli(interpreter::Cpu *cpu, uint32_t opcode);
void andLlr(interpreter::Cpu *cpu, uint32_t opcode);
void andLri(interpreter::Cpu *cpu, uint32_t opcode);
void andLrr(interpreter::Cpu *cpu, uint32_t opcode);
void andAri(interpreter::Cpu *cpu, uint32_t opcode);
void andArr(interpreter::Cpu *cpu, uint32_t opcode);
void andRri(interpreter::Cpu *cpu, uint32_t opcode);
void andRrr(interpreter::Cpu *cpu, uint32_t opcode);

void mul(interpreter::Cpu *cpu, uint32_t opcode);

void andsLli(interpreter::Cpu *cpu, uint32_t opcode);
void andsLlr(interpreter::Cpu *cpu, uint32_t opcode);
void andsLri(interpreter::Cpu *cpu, uint32_t opcode);
void andsLrr(interpreter::Cpu *cpu, uint32_t opcode);
void andsAri(interpreter::Cpu *cpu, uint32_t opcode);
void andsArr(interpreter::Cpu *cpu, uint32_t opcode);
void andsRri(interpreter::Cpu *cpu, uint32_t opcode);
void andsRrr(interpreter::Cpu *cpu, uint32_t opcode);

void muls(interpreter::Cpu *cpu, uint32_t opcode);

void eorLli(interpreter::Cpu *cpu, uint32_t opcode);
void eorLlr(interpreter::Cpu *cpu, uint32_t opcode);
void eorLri(interpreter::Cpu *cpu, uint32_t opcode);
void eorLrr(interpreter::Cpu *cpu, uint32_t opcode);
void eorAri(interpreter::Cpu *cpu, uint32_t opcode);
void eorArr(interpreter::Cpu *cpu, uint32_t opcode);
void eorRri(interpreter::Cpu *cpu, uint32_t opcode);
void eorRrr(interpreter::Cpu *cpu, uint32_t opcode);

void mla(interpreter::Cpu *cpu, uint32_t opcode);

void eorsLli(interpreter::Cpu *cpu, uint32_t opcode);
void eorsLlr(interpreter::Cpu *cpu, uint32_t opcode);
void eorsLri(interpreter::Cpu *cpu, uint32_t opcode);
void eorsLrr(interpreter::Cpu *cpu, uint32_t opcode);
void eorsAri(interpreter::Cpu *cpu, uint32_t opcode);
void eorsArr(interpreter::Cpu *cpu, uint32_t opcode);
void eorsRri(interpreter::Cpu *cpu, uint32_t opcode);
void eorsRrr(interpreter::Cpu *cpu, uint32_t opcode);

void mlas(interpreter::Cpu *cpu, uint32_t opcode);

void subLli(interpreter::Cpu *cpu, uint32_t opcode);
void subLlr(interpreter::Cpu *cpu, uint32_t opcode);
void subLri(interpreter::Cpu *cpu, uint32_t opcode);
void subLrr(interpreter::Cpu *cpu, uint32_t opcode);
void subAri(interpreter::Cpu *cpu, uint32_t opcode);
void subArr(interpreter::Cpu *cpu, uint32_t opcode);
void subRri(interpreter::Cpu *cpu, uint32_t opcode);
void subRrr(interpreter::Cpu *cpu, uint32_t opcode);

void subsLli(interpreter::Cpu *cpu, uint32_t opcode);
void subsLlr(interpreter::Cpu *cpu, uint32_t opcode);
void subsLri(interpreter::Cpu *cpu, uint32_t opcode);
void subsLrr(interpreter::Cpu *cpu, uint32_t opcode);
void subsAri(interpreter::Cpu *cpu, uint32_t opcode);
void subsArr(interpreter::Cpu *cpu, uint32_t opcode);
void subsRri(interpreter::Cpu *cpu, uint32_t opcode);
void subsRrr(interpreter::Cpu *cpu, uint32_t opcode);

void rsbLli(interpreter::Cpu *cpu, uint32_t opcode);
void rsbLlr(interpreter::Cpu *cpu, uint32_t opcode);
void rsbLri(interpreter::Cpu *cpu, uint32_t opcode);
void rsbLrr(interpreter::Cpu *cpu, uint32_t opcode);
void rsbAri(interpreter::Cpu *cpu, uint32_t opcode);
void rsbArr(interpreter::Cpu *cpu, uint32_t opcode);
void rsbRri(interpreter::Cpu *cpu, uint32_t opcode);
void rsbRrr(interpreter::Cpu *cpu, uint32_t opcode);

void rsbsLli(interpreter::Cpu *cpu, uint32_t opcode);
void rsbsLlr(interpreter::Cpu *cpu, uint32_t opcode);
void rsbsLri(interpreter::Cpu *cpu, uint32_t opcode);
void rsbsLrr(interpreter::Cpu *cpu, uint32_t opcode);
void rsbsAri(interpreter::Cpu *cpu, uint32_t opcode);
void rsbsArr(interpreter::Cpu *cpu, uint32_t opcode);
void rsbsRri(interpreter::Cpu *cpu, uint32_t opcode);
void rsbsRrr(interpreter::Cpu *cpu, uint32_t opcode);

void addLli(interpreter::Cpu *cpu, uint32_t opcode);
void addLlr(interpreter::Cpu *cpu, uint32_t opcode);
void addLri(interpreter::Cpu *cpu, uint32_t opcode);
void addLrr(interpreter::Cpu *cpu, uint32_t opcode);
void addAri(interpreter::Cpu *cpu, uint32_t opcode);
void addArr(interpreter::Cpu *cpu, uint32_t opcode);
void addRri(interpreter::Cpu *cpu, uint32_t opcode);
void addRrr(interpreter::Cpu *cpu, uint32_t opcode);

void umull(interpreter::Cpu *cpu, uint32_t opcode);

void addsLli(interpreter::Cpu *cpu, uint32_t opcode);
void addsLlr(interpreter::Cpu *cpu, uint32_t opcode);
void addsLri(interpreter::Cpu *cpu, uint32_t opcode);
void addsLrr(interpreter::Cpu *cpu, uint32_t opcode);
void addsAri(interpreter::Cpu *cpu, uint32_t opcode);
void addsArr(interpreter::Cpu *cpu, uint32_t opcode);
void addsRri(interpreter::Cpu *cpu, uint32_t opcode);
void addsRrr(interpreter::Cpu *cpu, uint32_t opcode);

void umulls(interpreter::Cpu *cpu, uint32_t opcode);

void adcLli(interpreter::Cpu *cpu, uint32_t opcode);
void adcLlr(interpreter::Cpu *cpu, uint32_t opcode);
void adcLri(interpreter::Cpu *cpu, uint32_t opcode);
void adcLrr(interpreter::Cpu *cpu, uint32_t opcode);
void adcAri(interpreter::Cpu *cpu, uint32_t opcode);
void adcArr(interpreter::Cpu *cpu, uint32_t opcode);
void adcRri(interpreter::Cpu *cpu, uint32_t opcode);
void adcRrr(interpreter::Cpu *cpu, uint32_t opcode);

void umlal(interpreter::Cpu *cpu, uint32_t opcode);

void adcsLli(interpreter::Cpu *cpu, uint32_t opcode);
void adcsLlr(interpreter::Cpu *cpu, uint32_t opcode);
void adcsLri(interpreter::Cpu *cpu, uint32_t opcode);
void adcsLrr(interpreter::Cpu *cpu, uint32_t opcode);
void adcsAri(interpreter::Cpu *cpu, uint32_t opcode);
void adcsArr(interpreter::Cpu *cpu, uint32_t opcode);
void adcsRri(interpreter::Cpu *cpu, uint32_t opcode);
void adcsRrr(interpreter::Cpu *cpu, uint32_t opcode);

void umlals(interpreter::Cpu *cpu, uint32_t opcode);

void sbcLli(interpreter::Cpu *cpu, uint32_t opcode);
void sbcLlr(interpreter::Cpu *cpu, uint32_t opcode);
void sbcLri(interpreter::Cpu *cpu, uint32_t opcode);
void sbcLrr(interpreter::Cpu *cpu, uint32_t opcode);
void sbcAri(interpreter::Cpu *cpu, uint32_t opcode);
void sbcArr(interpreter::Cpu *cpu, uint32_t opcode);
void sbcRri(interpreter::Cpu *cpu, uint32_t opcode);
void sbcRrr(interpreter::Cpu *cpu, uint32_t opcode);

void smull(interpreter::Cpu *cpu, uint32_t opcode);

void sbcsLli(interpreter::Cpu *cpu, uint32_t opcode);
void sbcsLlr(interpreter::Cpu *cpu, uint32_t opcode);
void sbcsLri(interpreter::Cpu *cpu, uint32_t opcode);
void sbcsLrr(interpreter::Cpu *cpu, uint32_t opcode);
void sbcsAri(interpreter::Cpu *cpu, uint32_t opcode);
void sbcsArr(interpreter::Cpu *cpu, uint32_t opcode);
void sbcsRri(interpreter::Cpu *cpu, uint32_t opcode);
void sbcsRrr(interpreter::Cpu *cpu, uint32_t opcode);

void smulls(interpreter::Cpu *cpu, uint32_t opcode);

void rscLli(interpreter::Cpu *cpu, uint32_t opcode);
void rscLlr(interpreter::Cpu *cpu, uint32_t opcode);
void rscLri(interpreter::Cpu *cpu, uint32_t opcode);
void rscLrr(interpreter::Cpu *cpu, uint32_t opcode);
void rscAri(interpreter::Cpu *cpu, uint32_t opcode);
void rscArr(interpreter::Cpu *cpu, uint32_t opcode);
void rscRri(interpreter::Cpu *cpu, uint32_t opcode);
void rscRrr(interpreter::Cpu *cpu, uint32_t opcode);

void smlal(interpreter::Cpu *cpu, uint32_t opcode);

void rscsLli(interpreter::Cpu *cpu, uint32_t opcode);
void rscsLlr(interpreter::Cpu *cpu, uint32_t opcode);
void rscsLri(interpreter::Cpu *cpu, uint32_t opcode);
void rscsLrr(interpreter::Cpu *cpu, uint32_t opcode);
void rscsAri(interpreter::Cpu *cpu, uint32_t opcode);
void rscsArr(interpreter::Cpu *cpu, uint32_t opcode);
void rscsRri(interpreter::Cpu *cpu, uint32_t opcode);
void rscsRrr(interpreter::Cpu *cpu, uint32_t opcode);

void smlals(interpreter::Cpu *cpu, uint32_t opcode);

void tstLli(interpreter::Cpu *cpu, uint32_t opcode);
void tstLlr(interpreter::Cpu *cpu, uint32_t opcode);
void tstLri(interpreter::Cpu *cpu, uint32_t opcode);
void tstLrr(interpreter::Cpu *cpu, uint32_t opcode);
void tstAri(interpreter::Cpu *cpu, uint32_t opcode);
void tstArr(interpreter::Cpu *cpu, uint32_t opcode);
void tstRri(interpreter::Cpu *cpu, uint32_t opcode);
void tstRrr(interpreter::Cpu *cpu, uint32_t opcode);

void teqLli(interpreter::Cpu *cpu, uint32_t opcode);
void teqLlr(interpreter::Cpu *cpu, uint32_t opcode);
void teqLri(interpreter::Cpu *cpu, uint32_t opcode);
void teqLrr(interpreter::Cpu *cpu, uint32_t opcode);
void teqAri(interpreter::Cpu *cpu, uint32_t opcode);
void teqArr(interpreter::Cpu *cpu, uint32_t opcode);
void teqRri(interpreter::Cpu *cpu, uint32_t opcode);
void teqRrr(interpreter::Cpu *cpu, uint32_t opcode);

void cmpLli(interpreter::Cpu *cpu, uint32_t opcode);
void cmpLlr(interpreter::Cpu *cpu, uint32_t opcode);
void cmpLri(interpreter::Cpu *cpu, uint32_t opcode);
void cmpLrr(interpreter::Cpu *cpu, uint32_t opcode);
void cmpAri(interpreter::Cpu *cpu, uint32_t opcode);
void cmpArr(interpreter::Cpu *cpu, uint32_t opcode);
void cmpRri(interpreter::Cpu *cpu, uint32_t opcode);
void cmpRrr(interpreter::Cpu *cpu, uint32_t opcode);

void cmnLli(interpreter::Cpu *cpu, uint32_t opcode);
void cmnLlr(interpreter::Cpu *cpu, uint32_t opcode);
void cmnLri(interpreter::Cpu *cpu, uint32_t opcode);
void cmnLrr(interpreter::Cpu *cpu, uint32_t opcode);
void cmnAri(interpreter::Cpu *cpu, uint32_t opcode);
void cmnArr(interpreter::Cpu *cpu, uint32_t opcode);
void cmnRri(interpreter::Cpu *cpu, uint32_t opcode);
void cmnRrr(interpreter::Cpu *cpu, uint32_t opcode);

void orrLli(interpreter::Cpu *cpu, uint32_t opcode);
void orrLlr(interpreter::Cpu *cpu, uint32_t opcode);
void orrLri(interpreter::Cpu *cpu, uint32_t opcode);
void orrLrr(interpreter::Cpu *cpu, uint32_t opcode);
void orrAri(interpreter::Cpu *cpu, uint32_t opcode);
void orrArr(interpreter::Cpu *cpu, uint32_t opcode);
void orrRri(interpreter::Cpu *cpu, uint32_t opcode);
void orrRrr(interpreter::Cpu *cpu, uint32_t opcode);

void orrsLli(interpreter::Cpu *cpu, uint32_t opcode);
void orrsLlr(interpreter::Cpu *cpu, uint32_t opcode);
void orrsLri(interpreter::Cpu *cpu, uint32_t opcode);
void orrsLrr(interpreter::Cpu *cpu, uint32_t opcode);
void orrsAri(interpreter::Cpu *cpu, uint32_t opcode);
void orrsArr(interpreter::Cpu *cpu, uint32_t opcode);
void orrsRri(interpreter::Cpu *cpu, uint32_t opcode);
void orrsRrr(interpreter::Cpu *cpu, uint32_t opcode);

void movLli(interpreter::Cpu *cpu, uint32_t opcode);
void movLlr(interpreter::Cpu *cpu, uint32_t opcode);
void movLri(interpreter::Cpu *cpu, uint32_t opcode);
void movLrr(interpreter::Cpu *cpu, uint32_t opcode);
void movAri(interpreter::Cpu *cpu, uint32_t opcode);
void movArr(interpreter::Cpu *cpu, uint32_t opcode);
void movRri(interpreter::Cpu *cpu, uint32_t opcode);
void movRrr(interpreter::Cpu *cpu, uint32_t opcode);

void movsLli(interpreter::Cpu *cpu, uint32_t opcode);
void movsLlr(interpreter::Cpu *cpu, uint32_t opcode);
void movsLri(interpreter::Cpu *cpu, uint32_t opcode);
void movsLrr(interpreter::Cpu *cpu, uint32_t opcode);
void movsAri(interpreter::Cpu *cpu, uint32_t opcode);
void movsArr(interpreter::Cpu *cpu, uint32_t opcode);
void movsRri(interpreter::Cpu *cpu, uint32_t opcode);
void movsRrr(interpreter::Cpu *cpu, uint32_t opcode);

void bicLli(interpreter::Cpu *cpu, uint32_t opcode);
void bicLlr(interpreter::Cpu *cpu, uint32_t opcode);
void bicLri(interpreter::Cpu *cpu, uint32_t opcode);
void bicLrr(interpreter::Cpu *cpu, uint32_t opcode);
void bicAri(interpreter::Cpu *cpu, uint32_t opcode);
void bicArr(interpreter::Cpu *cpu, uint32_t opcode);
void bicRri(interpreter::Cpu *cpu, uint32_t opcode);
void bicRrr(interpreter::Cpu *cpu, uint32_t opcode);

void bicsLli(interpreter::Cpu *cpu, uint32_t opcode);
void bicsLlr(interpreter::Cpu *cpu, uint32_t opcode);
void bicsLri(interpreter::Cpu *cpu, uint32_t opcode);
void bicsLrr(interpreter::Cpu *cpu, uint32_t opcode);
void bicsAri(interpreter::Cpu *cpu, uint32_t opcode);
void bicsArr(interpreter::Cpu *cpu, uint32_t opcode);
void bicsRri(interpreter::Cpu *cpu, uint32_t opcode);
void bicsRrr(interpreter::Cpu *cpu, uint32_t opcode);

void mvnLli(interpreter::Cpu *cpu, uint32_t opcode);
void mvnLlr(interpreter::Cpu *cpu, uint32_t opcode);
void mvnLri(interpreter::Cpu *cpu, uint32_t opcode);
void mvnLrr(interpreter::Cpu *cpu, uint32_t opcode);
void mvnAri(interpreter::Cpu *cpu, uint32_t opcode);
void mvnArr(interpreter::Cpu *cpu, uint32_t opcode);
void mvnRri(interpreter::Cpu *cpu, uint32_t opcode);
void mvnRrr(interpreter::Cpu *cpu, uint32_t opcode);

void mvnsLli(interpreter::Cpu *cpu, uint32_t opcode);
void mvnsLlr(interpreter::Cpu *cpu, uint32_t opcode);
void mvnsLri(interpreter::Cpu *cpu, uint32_t opcode);
void mvnsLrr(interpreter::Cpu *cpu, uint32_t opcode);
void mvnsAri(interpreter::Cpu *cpu, uint32_t opcode);
void mvnsArr(interpreter::Cpu *cpu, uint32_t opcode);
void mvnsRri(interpreter::Cpu *cpu, uint32_t opcode);
void mvnsRrr(interpreter::Cpu *cpu, uint32_t opcode);

void andImm (interpreter::Cpu *cpu, uint32_t opcode);
void andsImm(interpreter::Cpu *cpu, uint32_t opcode);
void eorImm (interpreter::Cpu *cpu, uint32_t opcode);
void eorsImm(interpreter::Cpu *cpu, uint32_t opcode);
void subImm (interpreter::Cpu *cpu, uint32_t opcode);
void subsImm(interpreter::Cpu *cpu, uint32_t opcode);
void rsbImm (interpreter::Cpu *cpu, uint32_t opcode);
void rsbsImm(interpreter::Cpu *cpu, uint32_t opcode);
void addImm (interpreter::Cpu *cpu, uint32_t opcode);
void addsImm(interpreter::Cpu *cpu, uint32_t opcode);
void adcImm (interpreter::Cpu *cpu, uint32_t opcode);
void adcsImm(interpreter::Cpu *cpu, uint32_t opcode);
void sbcImm (interpreter::Cpu *cpu, uint32_t opcode);
void sbcsImm(interpreter::Cpu *cpu, uint32_t opcode);
void rscImm (interpreter::Cpu *cpu, uint32_t opcode);
void rscsImm(interpreter::Cpu *cpu, uint32_t opcode);
void tstImm (interpreter::Cpu *cpu, uint32_t opcode);
void teqImm (interpreter::Cpu *cpu, uint32_t opcode);
void cmpImm (interpreter::Cpu *cpu, uint32_t opcode);
void cmnImm (interpreter::Cpu *cpu, uint32_t opcode);
void orrImm (interpreter::Cpu *cpu, uint32_t opcode);
void orrsImm(interpreter::Cpu *cpu, uint32_t opcode);
void movImm (interpreter::Cpu *cpu, uint32_t opcode);
void movsImm(interpreter::Cpu *cpu, uint32_t opcode);
void bicImm (interpreter::Cpu *cpu, uint32_t opcode);
void bicsImm(interpreter::Cpu *cpu, uint32_t opcode);
void mvnImm (interpreter::Cpu *cpu, uint32_t opcode);
void mvnsImm(interpreter::Cpu *cpu, uint32_t opcode);

}

#endif // INTERPRETER_ALU
