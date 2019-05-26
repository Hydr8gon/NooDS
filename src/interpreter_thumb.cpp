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

#include "interpreter_thumb.h"
#include "memory.h"

#define BIT(i) (1 << i)
#define SET_FLAG(bit, cond) if (cond) cpu->cpsr |= BIT(bit); else cpu->cpsr &= ~BIT(bit);

#define RN (*cpu->registers[(opcode & 0x000001C0) >> 6])
#define RS (*cpu->registers[(opcode & 0x00000038) >> 3])
#define RD (*cpu->registers[(opcode & 0x00000007)])

#define IMM3 ((opcode & 0x000001C0) >> 6)
#define IMM5 ((opcode & 0x000007C0) >> 6)
#define IMM8 (opcode & 0x000000FF)
#define RD8  (*cpu->registers[(opcode & 0x00000700) >> 8])
#define RS16 (*cpu->registers[(opcode & 0x00000078) >> 3])
#define RD16 (*cpu->registers[((opcode & 0x00000080) >> 4) | (opcode & 0x7)])

#define BCOND_OFFSET (((opcode & 0x000000FF) << 1) | ((opcode & BIT(7))  ? 0xFFFFFE00 : 0))
#define B_OFFSET     (((opcode & 0x000007FF) << 1) | ((opcode & BIT(10)) ? 0xFFFFF000 : 0))
#define BL_OFFSET    ((opcode & 0x000007FF) << 1)

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

#define MUL_FLAGS              \
    if (cpu->type == 7)        \
        cpu->cpsr &= ~BIT(29); \
    COMMON_FLAGS(RD);

namespace interpreter_thumb
{

void lslImm5(interpreter::Cpu *cpu, uint32_t opcode) // LSL Rd,Rs,#i
{
    RD = interpreter::lsl(cpu, RS, IMM5, true);
    COMMON_FLAGS(RD);
}

void lsrImm5(interpreter::Cpu *cpu, uint32_t opcode) // LSR Rd,Rs,#i
{
    RD = interpreter::lsr(cpu, RS, IMM5, true);
    COMMON_FLAGS(RD);
}

void asrImm5(interpreter::Cpu *cpu, uint32_t opcode) // ASR Rd,Rs,#i
{
    RD = interpreter::asr(cpu, RS, IMM5, true);
    COMMON_FLAGS(RD);
}

void addReg(interpreter::Cpu *cpu, uint32_t opcode) // ADD Rd,Rs,Rn
{
    uint32_t pre = RS;
    uint32_t add = RN;
    RD = pre + add;
    ADD_FLAGS(RD);
}

void subReg(interpreter::Cpu *cpu, uint32_t opcode) // SUB Rd,Rs,Rn
{
    uint32_t pre = RD;
    uint32_t sub = RN;
    RD = pre - sub;
    SUB_FLAGS(RD);
}

void addImm3(interpreter::Cpu *cpu, uint32_t opcode) // ADD Rd,Rs,#i
{
    uint32_t pre = RS;
    uint32_t add = IMM3;
    RD = pre + add;
    ADD_FLAGS(RD);
}

void subImm3(interpreter::Cpu *cpu, uint32_t opcode) // SUB Rd,Rs,#i
{
    uint32_t pre = RD;
    uint32_t sub = IMM3;
    RD = pre - sub;
    SUB_FLAGS(RD);
}

void movImm8(interpreter::Cpu *cpu, uint32_t opcode) // MOV Rd,#i
{
    RD8 = IMM8;
    COMMON_FLAGS(RD8);
}

void cmpImm8(interpreter::Cpu *cpu, uint32_t opcode) // CMP Rd,#i
{
    uint32_t pre = RD8;
    uint32_t sub = IMM8;
    uint32_t res = pre - sub;
    SUB_FLAGS(res);
}

void addImm8(interpreter::Cpu *cpu, uint32_t opcode) // ADD Rd,#i
{
    uint32_t pre = RD8;
    uint32_t add = IMM8;
    RD8 += add;
    ADD_FLAGS(RD8);
}

void subImm8(interpreter::Cpu *cpu, uint32_t opcode) // SUB Rd,#i
{
    uint32_t pre = RD8;
    uint32_t sub = IMM8;
    RD8 -= sub;
    SUB_FLAGS(RD8);
}

void dpG1(interpreter::Cpu *cpu, uint32_t opcode) // AND/EOR/LSL/LSR Rd,Rs
{
    switch ((opcode & 0xC0) >> 6)
    {
        case 0x0: // AND
            RD &= RS;
            COMMON_FLAGS(RD);
            return;

        case 0x1: // EOR
            RD ^= RS;
            COMMON_FLAGS(RD);
            return;

        case 0x2: // LSL
            RD = interpreter::lsl(cpu, RD, RS & 0xFF, true);
            COMMON_FLAGS(RD);
            return;

        case 0x3: // LSR
            RD = interpreter::lsr(cpu, RD, RS & 0xFF, true);
            COMMON_FLAGS(RD);
            return;
    }
}

void dpG2(interpreter::Cpu *cpu, uint32_t opcode) // ASR/ADC/SBC/ROR Rd,Rs
{
    switch ((opcode & 0xC0) >> 6)
    {
        case 0x0: // ASR
            RD = interpreter::asr(cpu, RD, RS & 0xFF, true);
            COMMON_FLAGS(RD);
            return;

        case 0x1: // ADC
            {
                uint32_t pre = RD;
                uint32_t add = RS;
                RD += add + ((cpu->cpsr & BIT(29)) >> 29);
                ADC_FLAGS(RD);
            }
            return;

        case 0x2: // SBC
            {
                uint32_t pre = RD;
                uint32_t sub = RS;
                RD -= sub + 1 - ((cpu->cpsr & BIT(29)) >> 29);
                SBC_FLAGS(RD);
            }
            return;

        case 0x3: // ROR
            RD = interpreter::ror(cpu, RD, RS & 0xFF, true);
            COMMON_FLAGS(RD);
            return;
    }
}

void dpG3(interpreter::Cpu *cpu, uint32_t opcode) // TST/NEG/CMP/CMN Rd,Rs
{
    switch ((opcode & 0xC0) >> 6)
    {
        case 0x0: // TST
            {
                uint32_t res = RD & RS;
                COMMON_FLAGS(res);
            }
            return;

        case 0x1: // NEG
            RD = 0 - RS;
            COMMON_FLAGS(RD);
            return;

        case 0x2: // CMP
            {
                uint32_t pre = RD;
                uint32_t sub = RS;
                uint32_t res = pre - sub;
                SUB_FLAGS(res);
            }
            return;

        case 0x3: // CMN
            {
                uint32_t pre = RD;
                uint32_t add = RS;
                uint32_t res = pre + add;
                ADD_FLAGS(res);
            }
            return;
    }
}

void dpG4(interpreter::Cpu *cpu, uint32_t opcode) // ORR/MUL/BIC/MVN Rd,Rs
{
    switch ((opcode & 0xC0) >> 6)
    {
        case 0x0: // ORR
            RD |= RS;
            COMMON_FLAGS(RD);
            return;

        case 0x1: // MUL
            RD *= RS;
            MUL_FLAGS;
            return;

        case 0x2: // BIC
            RD &= ~RS;
            COMMON_FLAGS(RD);
            return;

        case 0x3: // MVN
            RD = ~RS;
            COMMON_FLAGS(RD);
            return;
    }
}

void addh(interpreter::Cpu *cpu, uint32_t opcode) // ADDH Rd,Rs
{
    RD16 += RS16;
}

void cmph(interpreter::Cpu *cpu, uint32_t opcode) // CMPH Rd,Rs
{
    uint32_t pre = RD16;
    uint32_t sub = RS16;
    uint32_t res = pre - sub;
    SUB_FLAGS(res);
}

void movh(interpreter::Cpu *cpu, uint32_t opcode) // MOVH Rd,Rs
{
    RD16 = RS16;
}

void bxReg(interpreter::Cpu *cpu, uint32_t opcode) // BX/BLX Rs
{
    if (!(opcode & BIT(7)) || cpu->type == 9)
    {
        if (opcode & BIT(7))
            *cpu->registers[14] = *cpu->registers[15] - 1;
        *cpu->registers[15] = RS16 & ~BIT(0);
        if (!(RS16 & BIT(0)))
            cpu->cpsr &= ~BIT(5);
    }
}

void ldrPc(interpreter::Cpu *cpu, uint32_t opcode) // LDR Rd,[PC,#i]
{
    RD8 = memory::read<uint32_t>(cpu, (*cpu->registers[15] & ~BIT(1)) + (IMM8 << 2));
}

void strReg(interpreter::Cpu *cpu, uint32_t opcode) // STR Rd,[Rb,Ro]
{
    memory::write<uint32_t>(cpu, RS + RN, RD);
}

void strhReg(interpreter::Cpu *cpu, uint32_t opcode) // STRH Rd,[Rb,Ro]
{
    memory::write<uint16_t>(cpu, RS + RN, RD);
}

void strbReg(interpreter::Cpu *cpu, uint32_t opcode) // STRB Rd,[Rb,Ro]
{
    memory::write<uint8_t>(cpu, RS + RN, RD);
}

void ldrsbReg(interpreter::Cpu *cpu, uint32_t opcode) // LDRSB Rd,[Rb,Ro]
{
    RD = (int8_t)memory::read<uint8_t>(cpu, RS + RN);
}

void ldrReg(interpreter::Cpu *cpu, uint32_t opcode) // LDR Rd,[Rb,Ro]
{
    RD = memory::read<uint32_t>(cpu, RS + RN);
}

void ldrhReg(interpreter::Cpu *cpu, uint32_t opcode) // LDRH Rd,[Rb,Ro]
{
    RD = memory::read<uint16_t>(cpu, RS + RN);
}

void ldrbReg(interpreter::Cpu *cpu, uint32_t opcode) // LDRB Rd,[Rb,Ro]
{
    RD = memory::read<uint8_t>(cpu, RS + RN);
}

void ldrshReg(interpreter::Cpu *cpu, uint32_t opcode) // LDRSH Rd,[Rb,Ro]
{
    RD = (int16_t)memory::read<uint16_t>(cpu, RS + RN);
}

void strImm5(interpreter::Cpu *cpu, uint32_t opcode) // STR Rd,[Rb,#i]
{
    memory::write<uint32_t>(cpu, RS + (IMM5 << 2), RD);
}

void ldrImm5(interpreter::Cpu *cpu, uint32_t opcode) // LDR Rd,[Rb,#i]
{
    RD = memory::read<uint32_t>(cpu, RS + (IMM5 << 2));
}

void strbImm5(interpreter::Cpu *cpu, uint32_t opcode) // STRB Rd,[Rb,#i]
{
    memory::write<uint8_t>(cpu, RS + IMM5, RD);
}

void ldrbImm5(interpreter::Cpu *cpu, uint32_t opcode) // LDRB Rd,[Rb,#i]
{
    RD = memory::read<uint8_t>(cpu, RS + IMM5);
}

void strhImm5(interpreter::Cpu *cpu, uint32_t opcode) // STRH Rd,[Rb,#i]
{
    memory::write<uint16_t>(cpu, RS + (IMM5 << 2), RD);
}

void ldrhImm5(interpreter::Cpu *cpu, uint32_t opcode) // LDRH Rd,[Rb,#i]
{
    RD = memory::read<uint16_t>(cpu, RS + (IMM5 << 1));
}

void strSp(interpreter::Cpu *cpu, uint32_t opcode) // STR Rd,[SP,#i]
{
    memory::write<uint32_t>(cpu, *cpu->registers[13] + (IMM8 << 2), RD8);
}

void ldrSp(interpreter::Cpu *cpu, uint32_t opcode) // LDR Rd,[SP,#i]
{
    RD8 = memory::read<uint32_t>(cpu, *cpu->registers[13] + (IMM8 << 2));
}

void addPc(interpreter::Cpu *cpu, uint32_t opcode) // ADD Rd,PC,#i
{
    RD8 = (*cpu->registers[15] & ~BIT(1)) + (IMM8 << 2);
}

void addSp(interpreter::Cpu *cpu, uint32_t opcode) // ADD Rd,SP,#i
{
    RD8 = *cpu->registers[14] + (IMM8 << 2);
}

void addSpImm(interpreter::Cpu *cpu, uint32_t opcode) // ADD SP,#i
{
    *cpu->registers[14] += (int8_t)(IMM8 << 2);
}

void push(interpreter::Cpu *cpu, uint32_t opcode) // PUSH <Rlist>
{
    for (int i = 7; i >= 0; i--)
    {
        if (opcode & BIT(i))
        {
            *cpu->registers[13] -= 4;
            memory::write<uint32_t>(cpu, *cpu->registers[13], *cpu->registers[i]);
        }
    }
}

void pushLr(interpreter::Cpu *cpu, uint32_t opcode) // PUSH <Rlist>,LR
{
    *cpu->registers[13] -= 4;
    memory::write<uint32_t>(cpu, *cpu->registers[13], *cpu->registers[14]);
    for (int i = 7; i >= 0; i--)
    {
        if (opcode & BIT(i))
        {
            *cpu->registers[13] -= 4;
            memory::write<uint32_t>(cpu, *cpu->registers[13], *cpu->registers[i]);
        }
    }
}

void pop(interpreter::Cpu *cpu, uint32_t opcode) // POP <Rlist>
{
    for (int i = 0; i < 8; i++)
    {
        if (opcode & BIT(i))
        {
            *cpu->registers[i] = memory::read<uint32_t>(cpu, *cpu->registers[13]);
            *cpu->registers[13] += 4;
        }
    }
}

void popPc(interpreter::Cpu *cpu, uint32_t opcode) // POP <Rlist>,PC
{
    for (int i = 0; i < 8; i++)
    {
        if (opcode & BIT(i))
        {
            *cpu->registers[i] = memory::read<uint32_t>(cpu, *cpu->registers[13]);
            *cpu->registers[13] += 4;
        }
    }
    *cpu->registers[15] = memory::read<uint32_t>(cpu, *cpu->registers[13]);
    *cpu->registers[13] += 4;
    if (cpu->type == 9 && !(*cpu->registers[15] & BIT(0)))
        cpu->cpsr &= ~BIT(5);
    else
        *cpu->registers[15] &= ~BIT(0);
}

void stmia(interpreter::Cpu *cpu, uint32_t opcode) // STMIA Rb!,<Rlist>
{
    for (int i = 0; i < 8; i++)
    {
        if (opcode & BIT(i))
        {
            memory::write<uint32_t>(cpu, RD8, *cpu->registers[i]);
            RD8 += 4;
        }
    }
}

void ldmia(interpreter::Cpu *cpu, uint32_t opcode) // LDMIA Rb!,<Rlist>
{
    for (int i = 0; i < 8; i++)
    {
        if (opcode & BIT(i))
        {
            *cpu->registers[i] = memory::read<uint32_t>(cpu, RD8);
            RD8 += 4;
        }
    }
}

void beq(interpreter::Cpu *cpu, uint32_t opcode) // BEQ label
{
    if (cpu->cpsr & BIT(30))
        *cpu->registers[15] += BCOND_OFFSET;
}

void bne(interpreter::Cpu *cpu, uint32_t opcode) // BNE label
{
    if (!(cpu->cpsr & BIT(30)))
        *cpu->registers[15] += BCOND_OFFSET;
}

void bcs(interpreter::Cpu *cpu, uint32_t opcode) // BCS label
{
    if (cpu->cpsr & BIT(29))
        *cpu->registers[15] += BCOND_OFFSET;
}

void bcc(interpreter::Cpu *cpu, uint32_t opcode) // BCC label
{
    if (!(cpu->cpsr & BIT(29)))
        *cpu->registers[15] += BCOND_OFFSET;
}

void bmi(interpreter::Cpu *cpu, uint32_t opcode) // BMI label
{
    if (cpu->cpsr & BIT(31))
        *cpu->registers[15] += BCOND_OFFSET;
}

void bpl(interpreter::Cpu *cpu, uint32_t opcode) // BPL label
{
    if (!(cpu->cpsr & BIT(31)))
        *cpu->registers[15] += BCOND_OFFSET;
}

void bvs(interpreter::Cpu *cpu, uint32_t opcode) // BVS label
{
    if (cpu->cpsr & BIT(28))
        *cpu->registers[15] += BCOND_OFFSET;
}

void bvc(interpreter::Cpu *cpu, uint32_t opcode) // BVC label
{
    if (!(cpu->cpsr & BIT(28)))
        *cpu->registers[15] += BCOND_OFFSET;
}

void bhi(interpreter::Cpu *cpu, uint32_t opcode) // BHI label
{
    if ((cpu->cpsr & BIT(29)) && !(cpu->cpsr & BIT(30)))
        *cpu->registers[15] += BCOND_OFFSET;
}

void bls(interpreter::Cpu *cpu, uint32_t opcode) // BLS label
{
    if (!(cpu->cpsr & BIT(29)) || (cpu->cpsr & BIT(30)))
        *cpu->registers[15] += BCOND_OFFSET;
}

void bge(interpreter::Cpu *cpu, uint32_t opcode) // BGE label
{
    if ((cpu->cpsr & BIT(31)) == (cpu->cpsr & BIT(28)) << 3)
        *cpu->registers[15] += BCOND_OFFSET;
}

void blt(interpreter::Cpu *cpu, uint32_t opcode) // BLT label
{
    if ((cpu->cpsr & BIT(31)) != (cpu->cpsr & BIT(28)) << 3)
        *cpu->registers[15] += BCOND_OFFSET;
}

void bgt(interpreter::Cpu *cpu, uint32_t opcode) // BGT label
{
    if (!(cpu->cpsr & BIT(30)) && (cpu->cpsr & BIT(31)) == (cpu->cpsr & BIT(28)) << 3)
        *cpu->registers[15] += BCOND_OFFSET;
}

void ble(interpreter::Cpu *cpu, uint32_t opcode) // BLE label
{
    if ((cpu->cpsr & BIT(30)) || (cpu->cpsr & BIT(31)) != (cpu->cpsr & BIT(28)) << 3)
        *cpu->registers[15] += BCOND_OFFSET;
}

void swi(interpreter::Cpu *cpu, uint32_t opcode) // SWI #i
{
    uint32_t cpsr = cpu->cpsr;
    setMode(cpu, 0x13);
    *cpu->registers[14] = *cpu->registers[15] - 2;
    *cpu->spsr = cpsr;
    cpu->cpsr &= ~BIT(5);
    cpu->cpsr |= BIT(7);
    *cpu->registers[15] = ((cpu->type == 9) ? 0xFFFF0008 : 0x00000008);
}

void b(interpreter::Cpu *cpu, uint32_t opcode) // B label
{
    *cpu->registers[15] += B_OFFSET;
}

void blxOff(interpreter::Cpu *cpu, uint32_t opcode) // BLX label
{
    if (cpu->type == 9)
    {
        uint32_t ret = (*cpu->registers[15] - 2) | 1;
        *cpu->registers[15] = *cpu->registers[14] + BL_OFFSET;
        *cpu->registers[14] = ret;
        cpu->cpsr &= ~BIT(5);
    }
}

void blSetup(interpreter::Cpu *cpu, uint32_t opcode) // BL/BLX label
{
    *cpu->registers[14] = *cpu->registers[15] + (B_OFFSET << 11);
}

void blOff(interpreter::Cpu *cpu, uint32_t opcode) // BL label
{
    uint32_t ret = (*cpu->registers[15] - 2) | 1;
    *cpu->registers[15] = *cpu->registers[14] + BL_OFFSET;
    *cpu->registers[14] = ret;
}

}
