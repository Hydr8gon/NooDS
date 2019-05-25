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

#ifndef INTERPRETER_THUMB_H
#define INTERPRETER_THUMB_H

#include "interpreter.h"

namespace interpreter_thumb
{

void lslImm5(interpreter::Cpu *cpu, uint32_t opcode);
void lsrImm5(interpreter::Cpu *cpu, uint32_t opcode);
void asrImm5(interpreter::Cpu *cpu, uint32_t opcode);

void addReg (interpreter::Cpu *cpu, uint32_t opcode);
void subReg (interpreter::Cpu *cpu, uint32_t opcode);
void addImm3(interpreter::Cpu *cpu, uint32_t opcode);
void subImm3(interpreter::Cpu *cpu, uint32_t opcode);

void movImm8(interpreter::Cpu *cpu, uint32_t opcode);
void cmpImm8(interpreter::Cpu *cpu, uint32_t opcode);
void addImm8(interpreter::Cpu *cpu, uint32_t opcode);
void subImm8(interpreter::Cpu *cpu, uint32_t opcode);

void dpG1(interpreter::Cpu *cpu, uint32_t opcode);
void dpG2(interpreter::Cpu *cpu, uint32_t opcode);
void dpG3(interpreter::Cpu *cpu, uint32_t opcode);
void dpG4(interpreter::Cpu *cpu, uint32_t opcode);

void addh (interpreter::Cpu *cpu, uint32_t opcode);
void cmph (interpreter::Cpu *cpu, uint32_t opcode);
void movh (interpreter::Cpu *cpu, uint32_t opcode);
void bxReg(interpreter::Cpu *cpu, uint32_t opcode);

void ldrPc(interpreter::Cpu *cpu, uint32_t opcode);

void strReg  (interpreter::Cpu *cpu, uint32_t opcode);
void strhReg (interpreter::Cpu *cpu, uint32_t opcode);
void strbReg (interpreter::Cpu *cpu, uint32_t opcode);
void ldrsbReg(interpreter::Cpu *cpu, uint32_t opcode);

void ldrReg(interpreter::Cpu *cpu, uint32_t opcode);
void ldrhReg(interpreter::Cpu *cpu, uint32_t opcode);
void ldrbReg(interpreter::Cpu *cpu, uint32_t opcode);
void ldrshReg(interpreter::Cpu *cpu, uint32_t opcode);

void strImm5 (interpreter::Cpu *cpu, uint32_t opcode);
void ldrImm5 (interpreter::Cpu *cpu, uint32_t opcode);
void strbImm5(interpreter::Cpu *cpu, uint32_t opcode);
void ldrbImm5(interpreter::Cpu *cpu, uint32_t opcode);
void strhImm5(interpreter::Cpu *cpu, uint32_t opcode);
void ldrhImm5(interpreter::Cpu *cpu, uint32_t opcode);

void strSp(interpreter::Cpu *cpu, uint32_t opcode);
void ldrSp(interpreter::Cpu *cpu, uint32_t opcode);

void addPc   (interpreter::Cpu *cpu, uint32_t opcode);
void addSp   (interpreter::Cpu *cpu, uint32_t opcode);
void addSpImm(interpreter::Cpu *cpu, uint32_t opcode);

void push  (interpreter::Cpu *cpu, uint32_t opcode);
void pushLr(interpreter::Cpu *cpu, uint32_t opcode);
void pop   (interpreter::Cpu *cpu, uint32_t opcode);
void popPc (interpreter::Cpu *cpu, uint32_t opcode);

void stmia(interpreter::Cpu *cpu, uint32_t opcode);
void ldmia(interpreter::Cpu *cpu, uint32_t opcode);

void beq(interpreter::Cpu *cpu, uint32_t opcode);
void bne(interpreter::Cpu *cpu, uint32_t opcode);
void bcs(interpreter::Cpu *cpu, uint32_t opcode);
void bcc(interpreter::Cpu *cpu, uint32_t opcode);
void bmi(interpreter::Cpu *cpu, uint32_t opcode);
void bpl(interpreter::Cpu *cpu, uint32_t opcode);
void bvs(interpreter::Cpu *cpu, uint32_t opcode);
void bvc(interpreter::Cpu *cpu, uint32_t opcode);
void bhi(interpreter::Cpu *cpu, uint32_t opcode);
void bls(interpreter::Cpu *cpu, uint32_t opcode);
void bge(interpreter::Cpu *cpu, uint32_t opcode);
void blt(interpreter::Cpu *cpu, uint32_t opcode);
void bgt(interpreter::Cpu *cpu, uint32_t opcode);
void ble(interpreter::Cpu *cpu, uint32_t opcode);

void b      (interpreter::Cpu *cpu, uint32_t opcode);
void blxOff (interpreter::Cpu *cpu, uint32_t opcode);
void blSetup(interpreter::Cpu *cpu, uint32_t opcode);
void blOff  (interpreter::Cpu *cpu, uint32_t opcode);

}

#endif // INTERPRETER_THUMB_H
