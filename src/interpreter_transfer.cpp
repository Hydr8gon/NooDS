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

#include "interpreter_transfer.h"
#include "core.h"
#include "memory.h"

#define RN (*cpu->registers[(opcode & 0x000F0000) >> 16])
#define RD (*cpu->registers[(opcode & 0x0000F000) >> 12])
#define RM (*cpu->registers[(opcode & 0x0000000F)])

#define SING_IMM (opcode & 0x00000FFF)
#define SPEC_IMM (((opcode & 0x00000F00) >> 4) | (opcode & 0xF))
#define SHIFT    ((opcode & 0x00000F80) >> 7)

#define THUMB_SWITCH                    \
    if (*cpu->registers[15] & BIT(0))   \
    {                                   \
        if (cpu->type == 9)             \
            cpu->cpsr |= BIT(5);        \
        *cpu->registers[15] &= ~BIT(0); \
    }

namespace interpreter_transfer
{

void strhPtrm(interpreter::Cpu *cpu, uint32_t opcode) // STRH Rd,[Rn],-Rm
{
    memory::write<uint16_t>(cpu, RN, RD);
    RN -= RM;
}

void ldrhPtrm(interpreter::Cpu *cpu, uint32_t opcode) // LDRH Rd,[Rn],-Rm
{
    RD = memory::read<uint16_t>(cpu, RN);
    RN -= RM;
}

void strhPtim(interpreter::Cpu *cpu, uint32_t opcode) // STRH Rd,[Rn],-#i
{
    memory::write<uint16_t>(cpu, RN, RD);
    RN -= SPEC_IMM;
}

void ldrhPtim(interpreter::Cpu *cpu, uint32_t opcode) // LDRH Rd,[Rn],-#i
{
    RD = memory::read<uint16_t>(cpu, RN);
    RN -= SPEC_IMM;
}

void strhPtrp(interpreter::Cpu *cpu, uint32_t opcode) // STRH Rd,[Rn],Rm
{
    memory::write<uint16_t>(cpu, RN, RD);
    RN += RM;
}

void ldrhPtrp(interpreter::Cpu *cpu, uint32_t opcode) // LDRH Rd,[Rn],Rm
{
    RD = memory::read<uint16_t>(cpu, RN);
    RN += RM;
}

void strhPtip(interpreter::Cpu *cpu, uint32_t opcode) // STRH Rd,[Rn],#i
{
    memory::write<uint16_t>(cpu, RN, RD);
    RN += SPEC_IMM;
}

void ldrhPtip(interpreter::Cpu *cpu, uint32_t opcode) // LDRH Rd,[Rn],#i
{
    RD = memory::read<uint16_t>(cpu, RN);
    RN += SPEC_IMM;
}

void swp(interpreter::Cpu *cpu, uint32_t opcode) // SWP Rd,Rm,[Rn]
{
    RD = memory::read<uint32_t>(cpu, RN);
    memory::write<uint32_t>(cpu, RN, RM);
}

void strhOfrm(interpreter::Cpu *cpu, uint32_t opcode) // STRH Rd,[Rn,-Rm]
{
    memory::write<uint16_t>(cpu, RN - RM, RD);
}

void ldrhOfrm(interpreter::Cpu *cpu, uint32_t opcode) // LDRH Rd,[Rn,-Rm]
{
    RD = memory::read<uint16_t>(cpu, RN - RM);
}

void strhPrrm(interpreter::Cpu *cpu, uint32_t opcode) // STRH Rd,[Rn,-Rm]!
{
    RN -= RM;
    memory::write<uint16_t>(cpu, RN, RD);
}

void ldrhPrrm(interpreter::Cpu *cpu, uint32_t opcode) // LDRH Rd,[Rn,-Rm]!
{
    RN -= RM;
    RD = memory::read<uint16_t>(cpu, RN);
}

void swpb(interpreter::Cpu *cpu, uint32_t opcode) // SWPB Rd,Rm,[Rn]
{
    RD = memory::read<uint8_t>(cpu, RN);
    memory::write<uint8_t>(cpu, RN, RM);
}

void strhOfim(interpreter::Cpu *cpu, uint32_t opcode) // STRH Rd,[Rn,-#i]
{
    memory::write<uint16_t>(cpu, RN - SPEC_IMM, RD);
}

void ldrhOfim(interpreter::Cpu *cpu, uint32_t opcode) // LDRH Rd,[Rn,-#i]
{
    RD = memory::read<uint16_t>(cpu, RN - SPEC_IMM);
}

void strhPrim(interpreter::Cpu *cpu, uint32_t opcode) // STRH Rd,[Rn,-#i]!
{
    RN -= SPEC_IMM;
    memory::write<uint16_t>(cpu, RN, RD);
}

void ldrhPrim(interpreter::Cpu *cpu, uint32_t opcode) // LDRH Rd,[Rn,-#i]!
{
    RN -= SPEC_IMM;
    RD = memory::read<uint16_t>(cpu, RN);
}

void strhOfrp(interpreter::Cpu *cpu, uint32_t opcode) // STRH Rd,[Rn,Rm]
{
    memory::write<uint16_t>(cpu, RN + RM, RD);
}

void ldrhOfrp(interpreter::Cpu *cpu, uint32_t opcode) // LDRH Rd,[Rn,Rm]
{
    RD = memory::read<uint16_t>(cpu, RN + RM);
}

void strhPrrp(interpreter::Cpu *cpu, uint32_t opcode) // STRH Rd,[Rn,Rm]!
{
    RN += RM;
    memory::write<uint16_t>(cpu, RN, RD);
}

void ldrhPrrp(interpreter::Cpu *cpu, uint32_t opcode) // LDRH Rd,[Rn,Rm]!
{
    RN += RM;
    RD = memory::read<uint16_t>(cpu, RN);
}

void strhOfip(interpreter::Cpu *cpu, uint32_t opcode) // STRH Rd,[Rn,#i]
{
    memory::write<uint16_t>(cpu, RN + SPEC_IMM, RD);
}

void ldrhOfip(interpreter::Cpu *cpu, uint32_t opcode) // LDRH Rd,[Rn,#i]
{
    RD = memory::read<uint16_t>(cpu, RN + SPEC_IMM);
}

void strhPrip(interpreter::Cpu *cpu, uint32_t opcode) // STRH Rd,[Rn,#i]!
{
    RN += SPEC_IMM;
    memory::write<uint16_t>(cpu, RN, RD);
}

void ldrhPrip(interpreter::Cpu *cpu, uint32_t opcode) // LDRH Rd,[Rn,#i]!
{
    RN += SPEC_IMM;
    RD = memory::read<uint16_t>(cpu, RN);
}

void strPtim(interpreter::Cpu *cpu, uint32_t opcode) // STR Rd,[Rn],-#i
{
    memory::write<uint32_t>(cpu, RN, RD);
    RN -= SING_IMM;
}

void ldrPtim(interpreter::Cpu *cpu, uint32_t opcode) // LDR Rd,[Rn],-#i
{
    RD = memory::read<uint32_t>(cpu, RN);
    RN -= SING_IMM;
    THUMB_SWITCH;
}

void strbPtim(interpreter::Cpu *cpu, uint32_t opcode) // STRB Rd,[Rn],-#i
{
    memory::write<uint8_t>(cpu, RN, RD);
    RN -= SING_IMM;
}

void ldrbPtim(interpreter::Cpu *cpu, uint32_t opcode) // LDRB Rd,[Rn],-#i
{
    RD = memory::read<uint8_t>(cpu, RN);
    RN -= SING_IMM;
}

void strPtip(interpreter::Cpu *cpu, uint32_t opcode) // STR Rd,[Rn],#i
{
    memory::write<uint32_t>(cpu, RN, RD);
    RN += SING_IMM;
}

void ldrPtip(interpreter::Cpu *cpu, uint32_t opcode) // LDR Rd,[Rn],#i
{
    RD = memory::read<uint32_t>(cpu, RN);
    RN += SING_IMM;
    THUMB_SWITCH;
}

void strbPtip(interpreter::Cpu *cpu, uint32_t opcode) // STRB Rd,[Rn],#i
{
    memory::write<uint8_t>(cpu, RN, RD);
    RN += SING_IMM;
}

void ldrbPtip(interpreter::Cpu *cpu, uint32_t opcode) // LDRB Rd,[Rn],#i
{
    RD = memory::read<uint8_t>(cpu, RN);
    RN += SING_IMM;
}

void strOfim(interpreter::Cpu *cpu, uint32_t opcode) // STR Rd,[Rn,-#i]
{
    memory::write<uint32_t>(cpu, RN - SING_IMM, RD);
}

void ldrOfim(interpreter::Cpu *cpu, uint32_t opcode) // LDR Rd,[Rn,-#i]
{
    RD = memory::read<uint32_t>(cpu, RN - SING_IMM);
    THUMB_SWITCH;
}

void strbOfim(interpreter::Cpu *cpu, uint32_t opcode) // STRB Rd,[Rn,-#i]
{
    memory::write<uint8_t>(cpu, RN - SING_IMM, RD);
}

void ldrbOfim(interpreter::Cpu *cpu, uint32_t opcode) // LDRB Rd,[Rn,-#i]
{
    RD = memory::read<uint8_t>(cpu, RN - SING_IMM);
}

void strPrim(interpreter::Cpu *cpu, uint32_t opcode) // STR Rd,[Rn,-#i]!
{
    RN -= SING_IMM;
    memory::write<uint32_t>(cpu, RN, RD);
}

void ldrPrim(interpreter::Cpu *cpu, uint32_t opcode) // LDR Rd,[Rn,-#i]!
{
    RN -= SING_IMM;
    RD = memory::read<uint32_t>(cpu, RN);
    THUMB_SWITCH;
}

void strbPrim(interpreter::Cpu *cpu, uint32_t opcode) // STRB Rd,[Rn,-#i]!
{
    RN -= SING_IMM;
    memory::write<uint8_t>(cpu, RN, RD);
}

void ldrbPrim(interpreter::Cpu *cpu, uint32_t opcode) // LDRB Rd,[Rn,-#i]!
{
    RN -= SING_IMM;
    RD = memory::read<uint8_t>(cpu, RN);
}

void strOfip(interpreter::Cpu *cpu, uint32_t opcode) // STR Rd,[Rn,#i]
{
    memory::write<uint32_t>(cpu, RN + SING_IMM, RD);
}

void ldrOfip(interpreter::Cpu *cpu, uint32_t opcode) // LDR Rd,[Rn,#i]
{
    RD = memory::read<uint32_t>(cpu, RN + SING_IMM);
    THUMB_SWITCH;
}

void strbOfip(interpreter::Cpu *cpu, uint32_t opcode) // STRB Rd,[Rn,#i]
{
    memory::write<uint8_t>(cpu, RN + SING_IMM, RD);
}

void ldrbOfip(interpreter::Cpu *cpu, uint32_t opcode) // LDRB Rd,[Rn,#i]
{
    RD = memory::read<uint8_t>(cpu, RN + SING_IMM);
}

void strPrip(interpreter::Cpu *cpu, uint32_t opcode) // STR Rd,[Rn,#i]!
{
    RN += SING_IMM;
    memory::write<uint32_t>(cpu, RN, RD);
}

void ldrPrip(interpreter::Cpu *cpu, uint32_t opcode) // LDR Rd,[Rn,#i]!
{
    RN += SING_IMM;
    RD = memory::read<uint32_t>(cpu, RN);
    THUMB_SWITCH;
}

void strbPrip(interpreter::Cpu *cpu, uint32_t opcode) // STRB Rd,[Rn,#i]!
{
    RN += SING_IMM;
    memory::write<uint8_t>(cpu, RN, RD);
}

void ldrbPrip(interpreter::Cpu *cpu, uint32_t opcode) // LDRB Rd,[Rn,#i]!
{
    RN += SING_IMM;
    RD = memory::read<uint8_t>(cpu, RN);
}

void strPtrmll(interpreter::Cpu *cpu, uint32_t opcode) // STR Rd,[Rn],-Rm,LSL #i
{
    memory::write<uint32_t>(cpu, RN, RD);
    RN -= interpreter::lsl(cpu, RM, SHIFT, false);
}

void strPtrmlr(interpreter::Cpu *cpu, uint32_t opcode) // STR Rd,[Rn],-Rm,LSR #i
{
    memory::write<uint32_t>(cpu, RN, RD);
    RN -= interpreter::lsr(cpu, RM, SHIFT, false);
}

void strPtrmar(interpreter::Cpu *cpu, uint32_t opcode) // STR Rd,[Rn],-Rm,ASR #i
{
    memory::write<uint32_t>(cpu, RN, RD);
    RN -= interpreter::asr(cpu, RM, SHIFT, false);
}

void strPtrmrr(interpreter::Cpu *cpu, uint32_t opcode) // STR Rd,[Rn],-Rm,ROR #i
{
    memory::write<uint32_t>(cpu, RN, RD);
    RN -= interpreter::ror(cpu, RM, SHIFT, false);
}

void ldrPtrmll(interpreter::Cpu *cpu, uint32_t opcode) // LDR Rd,[Rn],-Rm,LSL #i
{
    RD = memory::read<uint32_t>(cpu, RN);
    RN -= interpreter::lsl(cpu, RM, SHIFT, false);
    THUMB_SWITCH;
}

void ldrPtrmlr(interpreter::Cpu *cpu, uint32_t opcode) // LDR Rd,[Rn],-Rm,LSR #i
{
    RD = memory::read<uint32_t>(cpu, RN);
    RN -= interpreter::lsr(cpu, RM, SHIFT, false);
    THUMB_SWITCH;
}

void ldrPtrmar(interpreter::Cpu *cpu, uint32_t opcode) // LDR Rd,[Rn],-Rm,ASR #i
{
    RD = memory::read<uint32_t>(cpu, RN);
    RN -= interpreter::asr(cpu, RM, SHIFT, false);
    THUMB_SWITCH;
}

void ldrPtrmrr(interpreter::Cpu *cpu, uint32_t opcode) // LDR Rd,[Rn],-Rm,ROR #i
{
    RD = memory::read<uint32_t>(cpu, RN);
    RN -= interpreter::ror(cpu, RM, SHIFT, false);
    THUMB_SWITCH;
}

void strbPtrmll(interpreter::Cpu *cpu, uint32_t opcode) // STRB Rd,[Rn],-Rm,LSL #i
{
    memory::write<uint8_t>(cpu, RN, RD);
    RN -= interpreter::lsl(cpu, RM, SHIFT, false);
}

void strbPtrmlr(interpreter::Cpu *cpu, uint32_t opcode) // STRB Rd,[Rn],-Rm,LSR #i
{
    memory::write<uint8_t>(cpu, RN, RD);
    RN -= interpreter::lsr(cpu, RM, SHIFT, false);
}

void strbPtrmar(interpreter::Cpu *cpu, uint32_t opcode) // STRB Rd,[Rn],-Rm,ASR #i
{
    memory::write<uint8_t>(cpu, RN, RD);
    RN -= interpreter::asr(cpu, RM, SHIFT, false);
}

void strbPtrmrr(interpreter::Cpu *cpu, uint32_t opcode) // STRB Rd,[Rn],-Rm,ROR #i
{
    memory::write<uint8_t>(cpu, RN, RD);
    RN -= interpreter::ror(cpu, RM, SHIFT, false);
}

void ldrbPtrmll(interpreter::Cpu *cpu, uint32_t opcode) // LDRB Rd,[Rn],-Rm,LSL #i
{
    RD = memory::read<uint8_t>(cpu, RN);
    RN -= interpreter::lsl(cpu, RM, SHIFT, false);
}

void ldrbPtrmlr(interpreter::Cpu *cpu, uint32_t opcode) // LDRB Rd,[Rn],-Rm,LSR #i
{
    RD = memory::read<uint8_t>(cpu, RN);
    RN -= interpreter::lsr(cpu, RM, SHIFT, false);
}

void ldrbPtrmar(interpreter::Cpu *cpu, uint32_t opcode) // LDRB Rd,[Rn],-Rm,ASR #i
{
    RD = memory::read<uint8_t>(cpu, RN);
    RN -= interpreter::asr(cpu, RM, SHIFT, false);
}

void ldrbPtrmrr(interpreter::Cpu *cpu, uint32_t opcode) // LDRB Rd,[Rn],-Rm,ROR #i
{
    RD = memory::read<uint8_t>(cpu, RN);
    RN -= interpreter::ror(cpu, RM, SHIFT, false);
}

void strPtrpll(interpreter::Cpu *cpu, uint32_t opcode) // STR Rd,[Rn],Rm,LSL #i
{
    memory::write<uint32_t>(cpu, RN, RD);
    RN += interpreter::lsl(cpu, RM, SHIFT, false);
}

void strPtrplr(interpreter::Cpu *cpu, uint32_t opcode) // STR Rd,[Rn],Rm,LSR #i
{
    memory::write<uint32_t>(cpu, RN, RD);
    RN += interpreter::lsr(cpu, RM, SHIFT, false);
}

void strPtrpar(interpreter::Cpu *cpu, uint32_t opcode) // STR Rd,[Rn],Rm,ASR #i
{
    memory::write<uint32_t>(cpu, RN, RD);
    RN += interpreter::asr(cpu, RM, SHIFT, false);
}

void strPtrprr(interpreter::Cpu *cpu, uint32_t opcode) // STR Rd,[Rn],Rm,ROR #i
{
    memory::write<uint32_t>(cpu, RN, RD);
    RN += interpreter::ror(cpu, RM, SHIFT, false);
}

void ldrPtrpll(interpreter::Cpu *cpu, uint32_t opcode) // LDR Rd,[Rn],Rm,LSL #i
{
    RD = memory::read<uint32_t>(cpu, RN);
    RN += interpreter::lsl(cpu, RM, SHIFT, false);
    THUMB_SWITCH;
}

void ldrPtrplr(interpreter::Cpu *cpu, uint32_t opcode) // LDR Rd,[Rn],Rm,LSR #i
{
    RD = memory::read<uint32_t>(cpu, RN);
    RN += interpreter::lsr(cpu, RM, SHIFT, false);
    THUMB_SWITCH;
}

void ldrPtrpar(interpreter::Cpu *cpu, uint32_t opcode) // LDR Rd,[Rn],Rm,ASR #i
{
    RD = memory::read<uint32_t>(cpu, RN);
    RN += interpreter::asr(cpu, RM, SHIFT, false);
    THUMB_SWITCH;
}

void ldrPtrprr(interpreter::Cpu *cpu, uint32_t opcode) // LDR Rd,[Rn],Rm,ROR #i
{
    RD = memory::read<uint32_t>(cpu, RN);
    RN += interpreter::ror(cpu, RM, SHIFT, false);
    THUMB_SWITCH;
}

void strbPtrpll(interpreter::Cpu *cpu, uint32_t opcode) // STRB Rd,[Rn],Rm,LSL #i
{
    memory::write<uint8_t>(cpu, RN, RD);
    RN += interpreter::lsl(cpu, RM, SHIFT, false);
}

void strbPtrplr(interpreter::Cpu *cpu, uint32_t opcode) // STRB Rd,[Rn],Rm,LSR #i
{
    memory::write<uint8_t>(cpu, RN, RD);
    RN += interpreter::lsr(cpu, RM, SHIFT, false);
}

void strbPtrpar(interpreter::Cpu *cpu, uint32_t opcode) // STRB Rd,[Rn],Rm,ASR #i
{
    memory::write<uint8_t>(cpu, RN, RD);
    RN += interpreter::asr(cpu, RM, SHIFT, false);
}

void strbPtrprr(interpreter::Cpu *cpu, uint32_t opcode) // STRB Rd,[Rn],Rm,ROR #i
{
    memory::write<uint8_t>(cpu, RN, RD);
    RN += interpreter::ror(cpu, RM, SHIFT, false);
}

void ldrbPtrpll(interpreter::Cpu *cpu, uint32_t opcode) // LDRB Rd,[Rn],Rm,LSL #i
{
    RD = memory::read<uint8_t>(cpu, RN);
    RN += interpreter::lsl(cpu, RM, SHIFT, false);
}

void ldrbPtrplr(interpreter::Cpu *cpu, uint32_t opcode) // LDRB Rd,[Rn],Rm,LSR #i
{
    RD = memory::read<uint8_t>(cpu, RN);
    RN += interpreter::lsr(cpu, RM, SHIFT, false);
}

void ldrbPtrpar(interpreter::Cpu *cpu, uint32_t opcode) // LDRB Rd,[Rn],Rm,ASR #i
{
    RD = memory::read<uint8_t>(cpu, RN);
    RN += interpreter::asr(cpu, RM, SHIFT, false);
}

void ldrbPtrprr(interpreter::Cpu *cpu, uint32_t opcode) // LDRB Rd,[Rn],Rm,ROR #i
{
    RD = memory::read<uint8_t>(cpu, RN);
    RN += interpreter::ror(cpu, RM, SHIFT, false);
}

void strOfrmll(interpreter::Cpu *cpu, uint32_t opcode) // STR Rd,[Rn,-Rm,LSL #i]
{
    memory::write<uint32_t>(cpu, RN - interpreter::lsl(cpu, RM, SHIFT, false), RD);
}

void strOfrmlr(interpreter::Cpu *cpu, uint32_t opcode) // STR Rd,[Rn,-Rm,LSR #i]
{
    memory::write<uint32_t>(cpu, RN - interpreter::lsr(cpu, RM, SHIFT, false), RD);
}

void strOfrmar(interpreter::Cpu *cpu, uint32_t opcode) // STR Rd,[Rn,-Rm,ASR #i]
{
    memory::write<uint32_t>(cpu, RN - interpreter::asr(cpu, RM, SHIFT, false), RD);
}

void strOfrmrr(interpreter::Cpu *cpu, uint32_t opcode) // STR Rd,[Rn,-Rm,ROR #i]
{
    memory::write<uint32_t>(cpu, RN - interpreter::ror(cpu, RM, SHIFT, false), RD);
}

void ldrOfrmll(interpreter::Cpu *cpu, uint32_t opcode) // LDR Rd,[Rn,-Rm,LSL #i]
{
    RD = memory::read<uint32_t>(cpu, RN - interpreter::lsl(cpu, RM, SHIFT, false));
    THUMB_SWITCH;
}

void ldrOfrmlr(interpreter::Cpu *cpu, uint32_t opcode) // LDR Rd,[Rn,-Rm,LSR #i]
{
    RD = memory::read<uint32_t>(cpu, RN - interpreter::lsr(cpu, RM, SHIFT, false));
    THUMB_SWITCH;
}

void ldrOfrmar(interpreter::Cpu *cpu, uint32_t opcode) // LDR Rd,[Rn,-Rm,ASR #i]
{
    RD = memory::read<uint32_t>(cpu, RN - interpreter::asr(cpu, RM, SHIFT, false));
    THUMB_SWITCH;
}

void ldrOfrmrr(interpreter::Cpu *cpu, uint32_t opcode) // LDR Rd,[Rn,-Rm,ROR #i]
{
    RD = memory::read<uint32_t>(cpu, RN - interpreter::ror(cpu, RM, SHIFT, false));
    THUMB_SWITCH;
}

void strPrrmll(interpreter::Cpu *cpu, uint32_t opcode) // STR Rd,[Rn,-Rm,LSL #i]!
{
    RN -= interpreter::lsl(cpu, RM, SHIFT, false);
    memory::write<uint32_t>(cpu, RN, RD);
}

void strPrrmlr(interpreter::Cpu *cpu, uint32_t opcode) // STR Rd,[Rn,-Rm,LSR #i]!
{
    RN -= interpreter::lsr(cpu, RM, SHIFT, false);
    memory::write<uint32_t>(cpu, RN, RD);
}

void strPrrmar(interpreter::Cpu *cpu, uint32_t opcode) // STR Rd,[Rn,-Rm,ASR #i]!
{
    RN -= interpreter::asr(cpu, RM, SHIFT, false);
    memory::write<uint32_t>(cpu, RN, RD);
}

void strPrrmrr(interpreter::Cpu *cpu, uint32_t opcode) // STR Rd,[Rn,-Rm,ROR #i]!
{
    RN -= interpreter::ror(cpu, RM, SHIFT, false);
    memory::write<uint32_t>(cpu, RN, RD);
}

void ldrPrrmll(interpreter::Cpu *cpu, uint32_t opcode) // LDR Rd,[Rn,-Rm,LSL #i]!
{
    RN -= interpreter::lsl(cpu, RM, SHIFT, false);
    RD = memory::read<uint32_t>(cpu, RN);
    THUMB_SWITCH;
}

void ldrPrrmlr(interpreter::Cpu *cpu, uint32_t opcode) // LDR Rd,[Rn,-Rm,LSR #i]!
{
    RN -= interpreter::lsr(cpu, RM, SHIFT, false);
    RD = memory::read<uint32_t>(cpu, RN);
    THUMB_SWITCH;
}

void ldrPrrmar(interpreter::Cpu *cpu, uint32_t opcode) // LDR Rd,[Rn,-Rm,ASR #i]!
{
    RN -= interpreter::asr(cpu, RM, SHIFT, false);
    RD = memory::read<uint32_t>(cpu, RN);
    THUMB_SWITCH;
}

void ldrPrrmrr(interpreter::Cpu *cpu, uint32_t opcode) // LDR Rd,[Rn,-Rm,ROR #i]!
{
    RN -= interpreter::ror(cpu, RM, SHIFT, false);
    RD = memory::read<uint32_t>(cpu, RN);
    THUMB_SWITCH;
}

void strbOfrmll(interpreter::Cpu *cpu, uint32_t opcode) // STRB Rd,[Rn,-Rm,LSL #i]
{
    memory::write<uint8_t>(cpu, RN - interpreter::lsl(cpu, RM, SHIFT, false), RD);
}

void strbOfrmlr(interpreter::Cpu *cpu, uint32_t opcode) // STRB Rd,[Rn,-Rm,LSR #i]
{
    memory::write<uint8_t>(cpu, RN - interpreter::lsr(cpu, RM, SHIFT, false), RD);
}

void strbOfrmar(interpreter::Cpu *cpu, uint32_t opcode) // STRB Rd,[Rn,-Rm,ASR #i]
{
    memory::write<uint8_t>(cpu, RN - interpreter::asr(cpu, RM, SHIFT, false), RD);
}

void strbOfrmrr(interpreter::Cpu *cpu, uint32_t opcode) // STRB Rd,[Rn,-Rm,ROR #i]
{
    memory::write<uint8_t>(cpu, RN - interpreter::ror(cpu, RM, SHIFT, false), RD);
}

void ldrbOfrmll(interpreter::Cpu *cpu, uint32_t opcode) // LDRB Rd,[Rn,-Rm,LSL #i]
{
    RD = memory::read<uint8_t>(cpu, RN - interpreter::lsl(cpu, RM, SHIFT, false));
}

void ldrbOfrmlr(interpreter::Cpu *cpu, uint32_t opcode) // LDRB Rd,[Rn,-Rm,LSR #i]
{
    RD = memory::read<uint8_t>(cpu, RN - interpreter::lsr(cpu, RM, SHIFT, false));
}

void ldrbOfrmar(interpreter::Cpu *cpu, uint32_t opcode) // LDRB Rd,[Rn,-Rm,ASR #i]
{
    RD = memory::read<uint8_t>(cpu, RN - interpreter::asr(cpu, RM, SHIFT, false));
}

void ldrbOfrmrr(interpreter::Cpu *cpu, uint32_t opcode) // LDRB Rd,[Rn,-Rm,ROR #i]
{
    RD = memory::read<uint8_t>(cpu, RN - interpreter::ror(cpu, RM, SHIFT, false));
}

void strbPrrmll(interpreter::Cpu *cpu, uint32_t opcode) // STRB Rd,[Rn,-Rm,LSL #i]!
{
    RN -= interpreter::lsl(cpu, RM, SHIFT, false);
    memory::write<uint8_t>(cpu, RN, RD);
}

void strbPrrmlr(interpreter::Cpu *cpu, uint32_t opcode) // STRB Rd,[Rn,-Rm,LSR #i]!
{
    RN -= interpreter::lsr(cpu, RM, SHIFT, false);
    memory::write<uint8_t>(cpu, RN, RD);
}

void strbPrrmar(interpreter::Cpu *cpu, uint32_t opcode) // STRB Rd,[Rn,-Rm,ASR #i]!
{
    RN -= interpreter::asr(cpu, RM, SHIFT, false);
    memory::write<uint8_t>(cpu, RN, RD);
}

void strbPrrmrr(interpreter::Cpu *cpu, uint32_t opcode) // STRB Rd,[Rn,-Rm,ROR #i]!
{
    RN -= interpreter::ror(cpu, RM, SHIFT, false);
    memory::write<uint8_t>(cpu, RN, RD);
}

void ldrbPrrmll(interpreter::Cpu *cpu, uint32_t opcode) // LDRB Rd,[Rn,-Rm,LSL #i]!
{
    RN -= interpreter::lsl(cpu, RM, SHIFT, false);
    RD = memory::read<uint8_t>(cpu, RN);
}

void ldrbPrrmlr(interpreter::Cpu *cpu, uint32_t opcode) // LDRB Rd,[Rn,-Rm,LSR #i]!
{
    RN -= interpreter::lsr(cpu, RM, SHIFT, false);
    RD = memory::read<uint8_t>(cpu, RN);
}

void ldrbPrrmar(interpreter::Cpu *cpu, uint32_t opcode) // LDRB Rd,[Rn,-Rm,ASR #i]!
{
    RN -= interpreter::asr(cpu, RM, SHIFT, false);
    RD = memory::read<uint8_t>(cpu, RN);
}

void ldrbPrrmrr(interpreter::Cpu *cpu, uint32_t opcode) // LDRB Rd,[Rn,-Rm,ROR #i]!
{
    RN -= interpreter::ror(cpu, RM, SHIFT, false);
    RD = memory::read<uint8_t>(cpu, RN);
}

void strOfrpll(interpreter::Cpu *cpu, uint32_t opcode) // STR Rd,[Rn,Rm,LSL #i]
{
    memory::write<uint32_t>(cpu, RN + interpreter::lsl(cpu, RM, SHIFT, false), RD);
}

void strOfrplr(interpreter::Cpu *cpu, uint32_t opcode) // STR Rd,[Rn,Rm,LSR #i]
{
    memory::write<uint32_t>(cpu, RN + interpreter::lsr(cpu, RM, SHIFT, false), RD);
}

void strOfrpar(interpreter::Cpu *cpu, uint32_t opcode) // STR Rd,[Rn,Rm,ASR #i]
{
    memory::write<uint32_t>(cpu, RN + interpreter::asr(cpu, RM, SHIFT, false), RD);
}

void strOfrprr(interpreter::Cpu *cpu, uint32_t opcode) // STR Rd,[Rn,Rm,ROR #i]
{
    memory::write<uint32_t>(cpu, RN + interpreter::ror(cpu, RM, SHIFT, false), RD);
}

void ldrOfrpll(interpreter::Cpu *cpu, uint32_t opcode) // LDR Rd,[Rn,Rm,LSL #i]
{
    RD = memory::read<uint32_t>(cpu, RN + interpreter::lsl(cpu, RM, SHIFT, false));
    THUMB_SWITCH;
}

void ldrOfrplr(interpreter::Cpu *cpu, uint32_t opcode) // LDR Rd,[Rn,Rm,LSR #i]
{
    RD = memory::read<uint32_t>(cpu, RN + interpreter::lsr(cpu, RM, SHIFT, false));
    THUMB_SWITCH;
}

void ldrOfrpar(interpreter::Cpu *cpu, uint32_t opcode) // LDR Rd,[Rn,Rm,ASR #i]
{
    RD = memory::read<uint32_t>(cpu, RN + interpreter::asr(cpu, RM, SHIFT, false));
    THUMB_SWITCH;
}

void ldrOfrprr(interpreter::Cpu *cpu, uint32_t opcode) // LDR Rd,[Rn,Rm,ROR #i]
{
    RD = memory::read<uint32_t>(cpu, RN + interpreter::ror(cpu, RM, SHIFT, false));
    THUMB_SWITCH;
}

void strPrrpll(interpreter::Cpu *cpu, uint32_t opcode) // STR Rd,[Rn,Rm,LSL #i]!
{
    RN += interpreter::lsl(cpu, RM, SHIFT, false);
    memory::write<uint32_t>(cpu, RN, RD);
}

void strPrrplr(interpreter::Cpu *cpu, uint32_t opcode) // STR Rd,[Rn,Rm,LSR #i]!
{
    RN += interpreter::lsr(cpu, RM, SHIFT, false);
    memory::write<uint32_t>(cpu, RN, RD);
}

void strPrrpar(interpreter::Cpu *cpu, uint32_t opcode) // STR Rd,[Rn,Rm,ASR #i]!
{
    RN += interpreter::asr(cpu, RM, SHIFT, false);
    memory::write<uint32_t>(cpu, RN, RD);
}

void strPrrprr(interpreter::Cpu *cpu, uint32_t opcode) // STR Rd,[Rn,Rm,ROR #i]!
{
    RN += interpreter::ror(cpu, RM, SHIFT, false);
    memory::write<uint32_t>(cpu, RN, RD);
}

void ldrPrrpll(interpreter::Cpu *cpu, uint32_t opcode) // LDR Rd,[Rn,Rm,LSL #i]!
{
    RN += interpreter::lsl(cpu, RM, SHIFT, false);
    RD = memory::read<uint32_t>(cpu, RN);
    THUMB_SWITCH;
}

void ldrPrrplr(interpreter::Cpu *cpu, uint32_t opcode) // LDR Rd,[Rn,Rm,LSR #i]!
{
    RN += interpreter::lsr(cpu, RM, SHIFT, false);
    RD = memory::read<uint32_t>(cpu, RN);
    THUMB_SWITCH;
}

void ldrPrrpar(interpreter::Cpu *cpu, uint32_t opcode) // LDR Rd,[Rn,Rm,ASR #i]!
{
    RN += interpreter::asr(cpu, RM, SHIFT, false);
    RD = memory::read<uint32_t>(cpu, RN);
    THUMB_SWITCH;
}

void ldrPrrprr(interpreter::Cpu *cpu, uint32_t opcode) // LDR Rd,[Rn,Rm,ROR #i]!
{
    RN += interpreter::ror(cpu, RM, SHIFT, false);
    RD = memory::read<uint32_t>(cpu, RN);
    THUMB_SWITCH;
}

void strbOfrpll(interpreter::Cpu *cpu, uint32_t opcode) // STRB Rd,[Rn,Rm,LSL #i]
{
    memory::write<uint8_t>(cpu, RN + interpreter::lsl(cpu, RM, SHIFT, false), RD);
}

void strbOfrplr(interpreter::Cpu *cpu, uint32_t opcode) // STRB Rd,[Rn,Rm,LSR #i]
{
    memory::write<uint8_t>(cpu, RN + interpreter::lsr(cpu, RM, SHIFT, false), RD);
}

void strbOfrpar(interpreter::Cpu *cpu, uint32_t opcode) // STRB Rd,[Rn,Rm,ASR #i]
{
    memory::write<uint8_t>(cpu, RN + interpreter::asr(cpu, RM, SHIFT, false), RD);
}

void strbOfrprr(interpreter::Cpu *cpu, uint32_t opcode) // STRB Rd,[Rn,Rm,ROR #i]
{
    memory::write<uint8_t>(cpu, RN + interpreter::ror(cpu, RM, SHIFT, false), RD);
}

void ldrbOfrpll(interpreter::Cpu *cpu, uint32_t opcode) // LDRB Rd,[Rn,Rm,LSL #i]
{
    RD = memory::read<uint8_t>(cpu, RN + interpreter::lsl(cpu, RM, SHIFT, false));
}

void ldrbOfrplr(interpreter::Cpu *cpu, uint32_t opcode) // LDRB Rd,[Rn,Rm,LSR #i]
{
    RD = memory::read<uint8_t>(cpu, RN + interpreter::lsr(cpu, RM, SHIFT, false));
}

void ldrbOfrpar(interpreter::Cpu *cpu, uint32_t opcode) // LDRB Rd,[Rn,Rm,ASR #i]
{
    RD = memory::read<uint8_t>(cpu, RN + interpreter::asr(cpu, RM, SHIFT, false));
}

void ldrbOfrprr(interpreter::Cpu *cpu, uint32_t opcode) // LDRB Rd,[Rn,Rm,ROR #i]
{
    RD = memory::read<uint8_t>(cpu, RN + interpreter::ror(cpu, RM, SHIFT, false));
}

void strbPrrpll(interpreter::Cpu *cpu, uint32_t opcode) // STRB Rd,[Rn,Rm,LSL #i]!
{
    RN += interpreter::lsl(cpu, RM, SHIFT, false);
    memory::write<uint8_t>(cpu, RN, RD);
}

void strbPrrplr(interpreter::Cpu *cpu, uint32_t opcode) // STRB Rd,[Rn,Rm,LSR #i]!
{
    RN += interpreter::lsr(cpu, RM, SHIFT, false);
    memory::write<uint8_t>(cpu, RN, RD);
}

void strbPrrpar(interpreter::Cpu *cpu, uint32_t opcode) // STRB Rd,[Rn,Rm,ASR #i]!
{
    RN += interpreter::asr(cpu, RM, SHIFT, false);
    memory::write<uint8_t>(cpu, RN, RD);
}

void strbPrrprr(interpreter::Cpu *cpu, uint32_t opcode) // STRB Rd,[Rn,Rm,ROR #i]!
{
    RN += interpreter::ror(cpu, RM, SHIFT, false);
    memory::write<uint8_t>(cpu, RN, RD);
}

void ldrbPrrpll(interpreter::Cpu *cpu, uint32_t opcode) // LDRB Rd,[Rn,Rm,LSL #i]!
{
    RN += interpreter::lsl(cpu, RM, SHIFT, false);
    RD = memory::read<uint8_t>(cpu, RN);
}

void ldrbPrrplr(interpreter::Cpu *cpu, uint32_t opcode) // LDRB Rd,[Rn,Rm,LSR #i]!
{
    RN += interpreter::lsr(cpu, RM, SHIFT, false);
    RD = memory::read<uint8_t>(cpu, RN);
}

void ldrbPrrpar(interpreter::Cpu *cpu, uint32_t opcode) // LDRB Rd,[Rn,Rm,ASR #i]!
{
    RN += interpreter::asr(cpu, RM, SHIFT, false);
    RD = memory::read<uint8_t>(cpu, RN);
}

void ldrbPrrprr(interpreter::Cpu *cpu, uint32_t opcode) // LDRB Rd,[Rn,Rm,ROR #i]!
{
    RN += interpreter::ror(cpu, RM, SHIFT, false);
    RD = memory::read<uint8_t>(cpu, RN);
}

void stmda(interpreter::Cpu *cpu, uint32_t opcode) // STMDA Rn,<Rlist>
{
    uint32_t address = RN;
    for (int i = 15; i >= 0; i--)
    {
        if (opcode & BIT(i))
        {
            memory::write<uint32_t>(cpu, address, *cpu->registers[i]);
            address -= 4;
        }
    }
}

void ldmda(interpreter::Cpu *cpu, uint32_t opcode) // LDMDA Rn,<Rlist>
{
    uint32_t address = RN;
    for (int i = 15; i >= 0; i--)
    {
        if (opcode & BIT(i))
        {
            *cpu->registers[i] = memory::read<uint32_t>(cpu, address);
            address -= 4;
        }
    }
    THUMB_SWITCH;
}

void stmdaW(interpreter::Cpu *cpu, uint32_t opcode) // STMDA Rn!,<Rlist>
{
    for (int i = 15; i >= 0; i--)
    {
        if (opcode & BIT(i))
        {
            memory::write<uint32_t>(cpu, RN, *cpu->registers[i]);
            RN -= 4;
        }
    }
}

void ldmdaW(interpreter::Cpu *cpu, uint32_t opcode) // LDMDA Rn!,<Rlist>
{
    for (int i = 15; i >= 0; i--)
    {
        if (opcode & BIT(i))
        {
            *cpu->registers[i] = memory::read<uint32_t>(cpu, RN);
            RN -= 4;
        }
    }
    THUMB_SWITCH;
}

void stmia(interpreter::Cpu *cpu, uint32_t opcode) // STMIA Rn,<Rlist>
{
    uint32_t address = RN;
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            memory::write<uint32_t>(cpu, address, *cpu->registers[i]);
            address += 4;
        }
    }
}

void ldmia(interpreter::Cpu *cpu, uint32_t opcode) // LDMIA Rn,<Rlist>
{
    uint32_t address = RN;
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            *cpu->registers[i] = memory::read<uint32_t>(cpu, address);
            address += 4;
        }
    }
    THUMB_SWITCH;
}

void stmiaW(interpreter::Cpu *cpu, uint32_t opcode) // STMIA Rn!,<Rlist>
{
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            memory::write<uint32_t>(cpu, RN, *cpu->registers[i]);
            RN += 4;
        }
    }
}

void ldmiaW(interpreter::Cpu *cpu, uint32_t opcode) // LDMIA Rn!,<Rlist>
{
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            *cpu->registers[i] = memory::read<uint32_t>(cpu, RN);
            RN += 4;
        }
    }
    THUMB_SWITCH;
}

void stmdb(interpreter::Cpu *cpu, uint32_t opcode) // STMDB Rn,<Rlist>
{
    uint32_t address = RN;
    for (int i = 15; i >= 0; i--)
    {
        if (opcode & BIT(i))
        {
            address -= 4;
            memory::write<uint32_t>(cpu, address, *cpu->registers[i]);
        }
    }
}

void ldmdb(interpreter::Cpu *cpu, uint32_t opcode) // LDMDB Rn,<Rlist>
{
    uint32_t address = RN;
    for (int i = 15; i >= 0; i--)
    {
        if (opcode & BIT(i))
        {
            address -= 4;
            *cpu->registers[i] = memory::read<uint32_t>(cpu, address);
        }
    }
    THUMB_SWITCH;
}

void stmdbW(interpreter::Cpu *cpu, uint32_t opcode) // STMDB Rn!,<Rlist>
{
    for (int i = 15; i >= 0; i--)
    {
        if (opcode & BIT(i))
        {
            RN -= 4;
            memory::write<uint32_t>(cpu, RN, *cpu->registers[i]);
        }
    }
}

void ldmdbW(interpreter::Cpu *cpu, uint32_t opcode) // LDMDB Rn!,<Rlist>
{
    for (int i = 15; i >= 0; i--)
    {
        if (opcode & BIT(i))
        {
            RN -= 4;
            *cpu->registers[i] = memory::read<uint32_t>(cpu, RN);
        }
    }
    THUMB_SWITCH;
}

void stmib(interpreter::Cpu *cpu, uint32_t opcode) // STMIB Rn,<Rlist>
{
    uint32_t address = RN;
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            address += 4;
            memory::write<uint32_t>(cpu, address, *cpu->registers[i]);
        }
    }
}

void ldmib(interpreter::Cpu *cpu, uint32_t opcode) // LDMIB Rn,<Rlist>
{
    uint32_t address = RN;
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            address += 4;
            *cpu->registers[i] = memory::read<uint32_t>(cpu, address);
        }
    }
    THUMB_SWITCH;
}

void stmibW(interpreter::Cpu *cpu, uint32_t opcode) // STMIB Rn!,<Rlist>
{
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            RN += 4;
            memory::write<uint32_t>(cpu, RN, *cpu->registers[i]);
        }
    }
}

void ldmibW(interpreter::Cpu *cpu, uint32_t opcode) // LDMIB Rn!,<Rlist>
{
    for (int i = 0; i < 16; i++)
    {
        if (opcode & BIT(i))
        {
            RN += 4;
            *cpu->registers[i] = memory::read<uint32_t>(cpu, RN);
        }
    }
    THUMB_SWITCH;
}

}
