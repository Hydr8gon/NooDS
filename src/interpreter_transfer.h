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

#ifndef INTERPRETER_TRANSFER_H
#define INTERPRETER_TRANSFER_H

#include "interpreter.h"

namespace interpreter_transfer
{

void strhPtrm (interpreter::Cpu *cpu, uint32_t opcode);
void ldrhPtrm (interpreter::Cpu *cpu, uint32_t opcode);
void ldrsbPtrm(interpreter::Cpu *cpu, uint32_t opcode);
void ldrshPtrm(interpreter::Cpu *cpu, uint32_t opcode);

void strhPtim (interpreter::Cpu *cpu, uint32_t opcode);
void ldrhPtim (interpreter::Cpu *cpu, uint32_t opcode);
void ldrsbPtim(interpreter::Cpu *cpu, uint32_t opcode);
void ldrshPtim(interpreter::Cpu *cpu, uint32_t opcode);

void strhPtrp (interpreter::Cpu *cpu, uint32_t opcode);
void ldrhPtrp (interpreter::Cpu *cpu, uint32_t opcode);
void ldrsbPtrp(interpreter::Cpu *cpu, uint32_t opcode);
void ldrshPtrp(interpreter::Cpu *cpu, uint32_t opcode);

void strhPtip (interpreter::Cpu *cpu, uint32_t opcode);
void ldrhPtip (interpreter::Cpu *cpu, uint32_t opcode);
void ldrsbPtip(interpreter::Cpu *cpu, uint32_t opcode);
void ldrshPtip(interpreter::Cpu *cpu, uint32_t opcode);

void swp(interpreter::Cpu *cpu, uint32_t opcode);

void strhOfrm (interpreter::Cpu *cpu, uint32_t opcode);
void ldrhOfrm (interpreter::Cpu *cpu, uint32_t opcode);
void ldrsbOfrm(interpreter::Cpu *cpu, uint32_t opcode);
void ldrshOfrm(interpreter::Cpu *cpu, uint32_t opcode);

void strhPrrm (interpreter::Cpu *cpu, uint32_t opcode);
void ldrhPrrm (interpreter::Cpu *cpu, uint32_t opcode);
void ldrsbPrrm(interpreter::Cpu *cpu, uint32_t opcode);
void ldrshPrrm(interpreter::Cpu *cpu, uint32_t opcode);

void swpb(interpreter::Cpu *cpu, uint32_t opcode);

void strhOfim (interpreter::Cpu *cpu, uint32_t opcode);
void ldrhOfim (interpreter::Cpu *cpu, uint32_t opcode);
void ldrsbOfim(interpreter::Cpu *cpu, uint32_t opcode);
void ldrshOfim(interpreter::Cpu *cpu, uint32_t opcode);

void strhPrim (interpreter::Cpu *cpu, uint32_t opcode);
void ldrhPrim (interpreter::Cpu *cpu, uint32_t opcode);
void ldrsbPrim(interpreter::Cpu *cpu, uint32_t opcode);
void ldrshPrim(interpreter::Cpu *cpu, uint32_t opcode);

void strhOfrp (interpreter::Cpu *cpu, uint32_t opcode);
void ldrhOfrp (interpreter::Cpu *cpu, uint32_t opcode);
void ldrsbOfrp(interpreter::Cpu *cpu, uint32_t opcode);
void ldrshOfrp(interpreter::Cpu *cpu, uint32_t opcode);

void strhPrrp (interpreter::Cpu *cpu, uint32_t opcode);
void ldrhPrrp (interpreter::Cpu *cpu, uint32_t opcode);
void ldrsbPrrp(interpreter::Cpu *cpu, uint32_t opcode);
void ldrshPrrp(interpreter::Cpu *cpu, uint32_t opcode);

void strhOfip (interpreter::Cpu *cpu, uint32_t opcode);
void ldrhOfip (interpreter::Cpu *cpu, uint32_t opcode);
void ldrsbOfip(interpreter::Cpu *cpu, uint32_t opcode);
void ldrshOfip(interpreter::Cpu *cpu, uint32_t opcode);

void strhPrip (interpreter::Cpu *cpu, uint32_t opcode);
void ldrhPrip (interpreter::Cpu *cpu, uint32_t opcode);
void ldrsbPrip(interpreter::Cpu *cpu, uint32_t opcode);
void ldrshPrip(interpreter::Cpu *cpu, uint32_t opcode);

void strPtim (interpreter::Cpu *cpu, uint32_t opcode);
void ldrPtim (interpreter::Cpu *cpu, uint32_t opcode);
void strbPtim(interpreter::Cpu *cpu, uint32_t opcode);
void ldrbPtim(interpreter::Cpu *cpu, uint32_t opcode);

void strPtip (interpreter::Cpu *cpu, uint32_t opcode);
void ldrPtip (interpreter::Cpu *cpu, uint32_t opcode);
void strbPtip(interpreter::Cpu *cpu, uint32_t opcode);
void ldrbPtip(interpreter::Cpu *cpu, uint32_t opcode);

void strOfim (interpreter::Cpu *cpu, uint32_t opcode);
void ldrOfim (interpreter::Cpu *cpu, uint32_t opcode);
void strbOfim(interpreter::Cpu *cpu, uint32_t opcode);
void ldrbOfim(interpreter::Cpu *cpu, uint32_t opcode);

void strPrim (interpreter::Cpu *cpu, uint32_t opcode);
void ldrPrim (interpreter::Cpu *cpu, uint32_t opcode);
void strbPrim(interpreter::Cpu *cpu, uint32_t opcode);
void ldrbPrim(interpreter::Cpu *cpu, uint32_t opcode);

void strOfip (interpreter::Cpu *cpu, uint32_t opcode);
void ldrOfip (interpreter::Cpu *cpu, uint32_t opcode);
void strbOfip(interpreter::Cpu *cpu, uint32_t opcode);
void ldrbOfip(interpreter::Cpu *cpu, uint32_t opcode);

void strPrip (interpreter::Cpu *cpu, uint32_t opcode);
void ldrPrip (interpreter::Cpu *cpu, uint32_t opcode);
void strbPrip(interpreter::Cpu *cpu, uint32_t opcode);
void ldrbPrip(interpreter::Cpu *cpu, uint32_t opcode);

void strPtrmll(interpreter::Cpu *cpu, uint32_t opcode);
void strPtrmlr(interpreter::Cpu *cpu, uint32_t opcode);
void strPtrmar(interpreter::Cpu *cpu, uint32_t opcode);
void strPtrmrr(interpreter::Cpu *cpu, uint32_t opcode);
void ldrPtrmll(interpreter::Cpu *cpu, uint32_t opcode);
void ldrPtrmlr(interpreter::Cpu *cpu, uint32_t opcode);
void ldrPtrmar(interpreter::Cpu *cpu, uint32_t opcode);
void ldrPtrmrr(interpreter::Cpu *cpu, uint32_t opcode);

void strbPtrmll(interpreter::Cpu *cpu, uint32_t opcode);
void strbPtrmlr(interpreter::Cpu *cpu, uint32_t opcode);
void strbPtrmar(interpreter::Cpu *cpu, uint32_t opcode);
void strbPtrmrr(interpreter::Cpu *cpu, uint32_t opcode);
void ldrbPtrmll(interpreter::Cpu *cpu, uint32_t opcode);
void ldrbPtrmlr(interpreter::Cpu *cpu, uint32_t opcode);
void ldrbPtrmar(interpreter::Cpu *cpu, uint32_t opcode);
void ldrbPtrmrr(interpreter::Cpu *cpu, uint32_t opcode);

void strPtrpll(interpreter::Cpu *cpu, uint32_t opcode);
void strPtrplr(interpreter::Cpu *cpu, uint32_t opcode);
void strPtrpar(interpreter::Cpu *cpu, uint32_t opcode);
void strPtrprr(interpreter::Cpu *cpu, uint32_t opcode);
void ldrPtrpll(interpreter::Cpu *cpu, uint32_t opcode);
void ldrPtrplr(interpreter::Cpu *cpu, uint32_t opcode);
void ldrPtrpar(interpreter::Cpu *cpu, uint32_t opcode);
void ldrPtrprr(interpreter::Cpu *cpu, uint32_t opcode);

void strbPtrpll(interpreter::Cpu *cpu, uint32_t opcode);
void strbPtrplr(interpreter::Cpu *cpu, uint32_t opcode);
void strbPtrpar(interpreter::Cpu *cpu, uint32_t opcode);
void strbPtrprr(interpreter::Cpu *cpu, uint32_t opcode);
void ldrbPtrpll(interpreter::Cpu *cpu, uint32_t opcode);
void ldrbPtrplr(interpreter::Cpu *cpu, uint32_t opcode);
void ldrbPtrpar(interpreter::Cpu *cpu, uint32_t opcode);
void ldrbPtrprr(interpreter::Cpu *cpu, uint32_t opcode);

void strOfrmll(interpreter::Cpu *cpu, uint32_t opcode);
void strOfrmlr(interpreter::Cpu *cpu, uint32_t opcode);
void strOfrmar(interpreter::Cpu *cpu, uint32_t opcode);
void strOfrmrr(interpreter::Cpu *cpu, uint32_t opcode);
void ldrOfrmll(interpreter::Cpu *cpu, uint32_t opcode);
void ldrOfrmlr(interpreter::Cpu *cpu, uint32_t opcode);
void ldrOfrmar(interpreter::Cpu *cpu, uint32_t opcode);
void ldrOfrmrr(interpreter::Cpu *cpu, uint32_t opcode);

void strPrrmll(interpreter::Cpu *cpu, uint32_t opcode);
void strPrrmlr(interpreter::Cpu *cpu, uint32_t opcode);
void strPrrmar(interpreter::Cpu *cpu, uint32_t opcode);
void strPrrmrr(interpreter::Cpu *cpu, uint32_t opcode);
void ldrPrrmll(interpreter::Cpu *cpu, uint32_t opcode);
void ldrPrrmlr(interpreter::Cpu *cpu, uint32_t opcode);
void ldrPrrmar(interpreter::Cpu *cpu, uint32_t opcode);
void ldrPrrmrr(interpreter::Cpu *cpu, uint32_t opcode);

void strbOfrmll(interpreter::Cpu *cpu, uint32_t opcode);
void strbOfrmlr(interpreter::Cpu *cpu, uint32_t opcode);
void strbOfrmar(interpreter::Cpu *cpu, uint32_t opcode);
void strbOfrmrr(interpreter::Cpu *cpu, uint32_t opcode);
void ldrbOfrmll(interpreter::Cpu *cpu, uint32_t opcode);
void ldrbOfrmlr(interpreter::Cpu *cpu, uint32_t opcode);
void ldrbOfrmar(interpreter::Cpu *cpu, uint32_t opcode);
void ldrbOfrmrr(interpreter::Cpu *cpu, uint32_t opcode);

void strbPrrmll(interpreter::Cpu *cpu, uint32_t opcode);
void strbPrrmlr(interpreter::Cpu *cpu, uint32_t opcode);
void strbPrrmar(interpreter::Cpu *cpu, uint32_t opcode);
void strbPrrmrr(interpreter::Cpu *cpu, uint32_t opcode);
void ldrbPrrmll(interpreter::Cpu *cpu, uint32_t opcode);
void ldrbPrrmlr(interpreter::Cpu *cpu, uint32_t opcode);
void ldrbPrrmar(interpreter::Cpu *cpu, uint32_t opcode);
void ldrbPrrmrr(interpreter::Cpu *cpu, uint32_t opcode);

void strOfrpll(interpreter::Cpu *cpu, uint32_t opcode);
void strOfrplr(interpreter::Cpu *cpu, uint32_t opcode);
void strOfrpar(interpreter::Cpu *cpu, uint32_t opcode);
void strOfrprr(interpreter::Cpu *cpu, uint32_t opcode);
void ldrOfrpll(interpreter::Cpu *cpu, uint32_t opcode);
void ldrOfrplr(interpreter::Cpu *cpu, uint32_t opcode);
void ldrOfrpar(interpreter::Cpu *cpu, uint32_t opcode);
void ldrOfrprr(interpreter::Cpu *cpu, uint32_t opcode);

void strPrrpll(interpreter::Cpu *cpu, uint32_t opcode);
void strPrrplr(interpreter::Cpu *cpu, uint32_t opcode);
void strPrrpar(interpreter::Cpu *cpu, uint32_t opcode);
void strPrrprr(interpreter::Cpu *cpu, uint32_t opcode);
void ldrPrrpll(interpreter::Cpu *cpu, uint32_t opcode);
void ldrPrrplr(interpreter::Cpu *cpu, uint32_t opcode);
void ldrPrrpar(interpreter::Cpu *cpu, uint32_t opcode);
void ldrPrrprr(interpreter::Cpu *cpu, uint32_t opcode);

void strbOfrpll(interpreter::Cpu *cpu, uint32_t opcode);
void strbOfrplr(interpreter::Cpu *cpu, uint32_t opcode);
void strbOfrpar(interpreter::Cpu *cpu, uint32_t opcode);
void strbOfrprr(interpreter::Cpu *cpu, uint32_t opcode);
void ldrbOfrpll(interpreter::Cpu *cpu, uint32_t opcode);
void ldrbOfrplr(interpreter::Cpu *cpu, uint32_t opcode);
void ldrbOfrpar(interpreter::Cpu *cpu, uint32_t opcode);
void ldrbOfrprr(interpreter::Cpu *cpu, uint32_t opcode);

void strbPrrpll(interpreter::Cpu *cpu, uint32_t opcode);
void strbPrrplr(interpreter::Cpu *cpu, uint32_t opcode);
void strbPrrpar(interpreter::Cpu *cpu, uint32_t opcode);
void strbPrrprr(interpreter::Cpu *cpu, uint32_t opcode);
void ldrbPrrpll(interpreter::Cpu *cpu, uint32_t opcode);
void ldrbPrrplr(interpreter::Cpu *cpu, uint32_t opcode);
void ldrbPrrpar(interpreter::Cpu *cpu, uint32_t opcode);
void ldrbPrrprr(interpreter::Cpu *cpu, uint32_t opcode);

void stmda (interpreter::Cpu *cpu, uint32_t opcode);
void ldmda (interpreter::Cpu *cpu, uint32_t opcode);
void stmdaW(interpreter::Cpu *cpu, uint32_t opcode);
void ldmdaW(interpreter::Cpu *cpu, uint32_t opcode);

void stmia (interpreter::Cpu *cpu, uint32_t opcode);
void ldmia (interpreter::Cpu *cpu, uint32_t opcode);
void stmiaW(interpreter::Cpu *cpu, uint32_t opcode);
void ldmiaW(interpreter::Cpu *cpu, uint32_t opcode);

void stmdb (interpreter::Cpu *cpu, uint32_t opcode);
void ldmdb (interpreter::Cpu *cpu, uint32_t opcode);
void stmdbW(interpreter::Cpu *cpu, uint32_t opcode);
void ldmdbW(interpreter::Cpu *cpu, uint32_t opcode);

void stmib (interpreter::Cpu *cpu, uint32_t opcode);
void ldmib (interpreter::Cpu *cpu, uint32_t opcode);
void stmibW(interpreter::Cpu *cpu, uint32_t opcode);
void ldmibW(interpreter::Cpu *cpu, uint32_t opcode);

}

namespace interpreter_transfer_thumb
{

void ldrPc(interpreter::Cpu *cpu, uint32_t opcode);

void strReg  (interpreter::Cpu *cpu, uint32_t opcode);
void strhReg (interpreter::Cpu *cpu, uint32_t opcode);
void strbReg (interpreter::Cpu *cpu, uint32_t opcode);
void ldrsbReg(interpreter::Cpu *cpu, uint32_t opcode);

void ldrReg  (interpreter::Cpu *cpu, uint32_t opcode);
void ldrhReg (interpreter::Cpu *cpu, uint32_t opcode);
void ldrbReg (interpreter::Cpu *cpu, uint32_t opcode);
void ldrshReg(interpreter::Cpu *cpu, uint32_t opcode);

void strImm5 (interpreter::Cpu *cpu, uint32_t opcode);
void ldrImm5 (interpreter::Cpu *cpu, uint32_t opcode);
void strbImm5(interpreter::Cpu *cpu, uint32_t opcode);
void ldrbImm5(interpreter::Cpu *cpu, uint32_t opcode);
void strhImm5(interpreter::Cpu *cpu, uint32_t opcode);
void ldrhImm5(interpreter::Cpu *cpu, uint32_t opcode);

void strSp(interpreter::Cpu *cpu, uint32_t opcode);
void ldrSp(interpreter::Cpu *cpu, uint32_t opcode);

void push  (interpreter::Cpu *cpu, uint32_t opcode);
void pushLr(interpreter::Cpu *cpu, uint32_t opcode);

void pop   (interpreter::Cpu *cpu, uint32_t opcode);
void popPc (interpreter::Cpu *cpu, uint32_t opcode);

void stmia(interpreter::Cpu *cpu, uint32_t opcode);
void ldmia(interpreter::Cpu *cpu, uint32_t opcode);

}

#endif // INTERPRETER_TRANSFER_H
