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

#include "interpreter_other.h"
#include "core.h"
#include "cp15.h"

#define RD (*cpu->registers[(opcode & 0x0000F000) >> 12])
#define RM (*cpu->registers[(opcode & 0x0000000F)])

#define CN ((opcode & 0x000F0000) >> 16)
#define CP ((opcode & 0x000000E0) >> 5)
#define CM (opcode & 0x0000000F)

#define B_OFFSET (((opcode & 0x00FFFFFF) << 2) | ((opcode & BIT(23)) ? 0xFC000000 : 0))

#define BCOND_OFFSET_THUMB (((opcode & 0x00FF) << 1) | ((opcode & BIT(7))  ? 0xFFFFFE00 : 0))
#define B_OFFSET_THUMB     (((opcode & 0x07FF) << 1) | ((opcode & BIT(10)) ? 0xFFFFF000 : 0))
#define BL_OFFSET_THUMB    ((opcode & 0x07FF) << 1)
#define BX_OFFSET_THUMB    (*cpu->registers[(opcode & 0x0078) >> 3])

namespace interpreter_other
{

void mrsRc(interpreter::Cpu *cpu, uint32_t opcode) // MRS Rd,CPSR
{
    RD = cpu->cpsr;
}

void msrRc(interpreter::Cpu *cpu, uint32_t opcode) // MSR CPSR,Rm
{
    if (opcode & BIT(19))
        cpu->cpsr = (cpu->cpsr & ~0xFF000000) | (RM & 0xFF000000);
    if ((opcode & BIT(16)) && (cpu->cpsr & 0x0000001F) != 0x10)
    {
        cpu->cpsr = (cpu->cpsr & ~0x000000E0) | (RM & 0x000000E0);
        interpreter::setMode(cpu, RM & 0x0000001F);
    }
}

void bx(interpreter::Cpu *cpu, uint32_t opcode) // BX Rn
{
    *cpu->registers[15] = RM;
    if (*cpu->registers[15] & BIT(0))
    {
        cpu->cpsr |= BIT(5);
        *cpu->registers[15] &= ~BIT(0);
    }
}

void blx(interpreter::Cpu *cpu, uint32_t opcode) // BLX Rn
{
    if (cpu->type == 9)
    {
        *cpu->registers[14] = *cpu->registers[15] - 4;
        *cpu->registers[15] = RM;
        if (*cpu->registers[15] & BIT(0))
        {
            cpu->cpsr |= BIT(5);
            *cpu->registers[15] &= ~BIT(0);
        }
    }
}

void mrsRs(interpreter::Cpu *cpu, uint32_t opcode) // MRS Rd,SPSR
{
    if (cpu->spsr)
        RD = *cpu->spsr;
}

void msrRs(interpreter::Cpu *cpu, uint32_t opcode) // MSR SPSR,Rm
{
    if (cpu->spsr)
    {
        if (opcode & BIT(19))
            *cpu->spsr = (*cpu->spsr & ~0xFF000000) | (RM & 0xFF000000);
        if (opcode & BIT(16))
            *cpu->spsr = (*cpu->spsr & ~0x000000FF) | (RM & 0x000000FF);
    }
}

void b(interpreter::Cpu *cpu, uint32_t opcode) // B label
{
    *cpu->registers[15] += B_OFFSET;
}

void bl(interpreter::Cpu *cpu, uint32_t opcode) // BL label
{
    *cpu->registers[14] = *cpu->registers[15] - 4;
    *cpu->registers[15] += B_OFFSET;
}

void mcr(interpreter::Cpu *cpu, uint32_t opcode) // MCR Pn,<cpopc>,Rd,Cn,Cm,<cp>
{
    if (cpu->type == 9)
        cp15::writeRegister(CN, CM, CP, RD);
}

void mrc(interpreter::Cpu *cpu, uint32_t opcode) // MRC Pn,<cpopc>,Rd,Cn,Cm,<cp>
{
    if (cpu->type == 9)
        RD = cp15::readRegister(CN, CM, CP);
}

void swi(interpreter::Cpu *cpu, uint32_t opcode) // SWI #i
{
    uint32_t cpsr = cpu->cpsr;
    setMode(cpu, 0x13);
    *cpu->spsr = cpsr;
    cpu->cpsr |= BIT(7);
    *cpu->registers[14] = *cpu->registers[15] - 4;
    *cpu->registers[15] = ((cpu->type == 9) ? cp15::exceptions : 0x00000000) + 0x08;
}

}

namespace interpreter_other_thumb
{

void bxReg(interpreter::Cpu *cpu, uint32_t opcode) // BX/BLX Rs
{
    if (!(opcode & BIT(7)) || cpu->type == 9)
    {
        if (opcode & BIT(7))
            *cpu->registers[14] = *cpu->registers[15] - 1;
        *cpu->registers[15] = BX_OFFSET_THUMB & ~BIT(0);
        if (!(BX_OFFSET_THUMB & BIT(0)))
            cpu->cpsr &= ~BIT(5);
    }
}

void beq(interpreter::Cpu *cpu, uint32_t opcode) // BEQ label
{
    if (cpu->cpsr & BIT(30))
        *cpu->registers[15] += BCOND_OFFSET_THUMB;
}

void bne(interpreter::Cpu *cpu, uint32_t opcode) // BNE label
{
    if (!(cpu->cpsr & BIT(30)))
        *cpu->registers[15] += BCOND_OFFSET_THUMB;
}

void bcs(interpreter::Cpu *cpu, uint32_t opcode) // BCS label
{
    if (cpu->cpsr & BIT(29))
        *cpu->registers[15] += BCOND_OFFSET_THUMB;
}

void bcc(interpreter::Cpu *cpu, uint32_t opcode) // BCC label
{
    if (!(cpu->cpsr & BIT(29)))
        *cpu->registers[15] += BCOND_OFFSET_THUMB;
}

void bmi(interpreter::Cpu *cpu, uint32_t opcode) // BMI label
{
    if (cpu->cpsr & BIT(31))
        *cpu->registers[15] += BCOND_OFFSET_THUMB;
}

void bpl(interpreter::Cpu *cpu, uint32_t opcode) // BPL label
{
    if (!(cpu->cpsr & BIT(31)))
        *cpu->registers[15] += BCOND_OFFSET_THUMB;
}

void bvs(interpreter::Cpu *cpu, uint32_t opcode) // BVS label
{
    if (cpu->cpsr & BIT(28))
        *cpu->registers[15] += BCOND_OFFSET_THUMB;
}

void bvc(interpreter::Cpu *cpu, uint32_t opcode) // BVC label
{
    if (!(cpu->cpsr & BIT(28)))
        *cpu->registers[15] += BCOND_OFFSET_THUMB;
}

void bhi(interpreter::Cpu *cpu, uint32_t opcode) // BHI label
{
    if ((cpu->cpsr & BIT(29)) && !(cpu->cpsr & BIT(30)))
        *cpu->registers[15] += BCOND_OFFSET_THUMB;
}

void bls(interpreter::Cpu *cpu, uint32_t opcode) // BLS label
{
    if (!(cpu->cpsr & BIT(29)) || (cpu->cpsr & BIT(30)))
        *cpu->registers[15] += BCOND_OFFSET_THUMB;
}

void bge(interpreter::Cpu *cpu, uint32_t opcode) // BGE label
{
    if ((cpu->cpsr & BIT(31)) == (cpu->cpsr & BIT(28)) << 3)
        *cpu->registers[15] += BCOND_OFFSET_THUMB;
}

void blt(interpreter::Cpu *cpu, uint32_t opcode) // BLT label
{
    if ((cpu->cpsr & BIT(31)) != (cpu->cpsr & BIT(28)) << 3)
        *cpu->registers[15] += BCOND_OFFSET_THUMB;
}

void bgt(interpreter::Cpu *cpu, uint32_t opcode) // BGT label
{
    if (!(cpu->cpsr & BIT(30)) && (cpu->cpsr & BIT(31)) == (cpu->cpsr & BIT(28)) << 3)
        *cpu->registers[15] += BCOND_OFFSET_THUMB;
}

void ble(interpreter::Cpu *cpu, uint32_t opcode) // BLE label
{
    if ((cpu->cpsr & BIT(30)) || (cpu->cpsr & BIT(31)) != (cpu->cpsr & BIT(28)) << 3)
        *cpu->registers[15] += BCOND_OFFSET_THUMB;
}

void swi(interpreter::Cpu *cpu, uint32_t opcode) // SWI #i
{
    uint32_t cpsr = cpu->cpsr;
    setMode(cpu, 0x13);
    *cpu->spsr = cpsr;
    cpu->cpsr &= ~BIT(5);
    cpu->cpsr |= BIT(7);
    *cpu->registers[14] = *cpu->registers[15] - 2;
    *cpu->registers[15] = ((cpu->type == 9) ? cp15::exceptions : 0x00000000) + 0x08;
}

void b(interpreter::Cpu *cpu, uint32_t opcode) // B label
{
    *cpu->registers[15] += B_OFFSET_THUMB;
}

void blxOff(interpreter::Cpu *cpu, uint32_t opcode) // BLX label
{
    if (cpu->type == 9)
    {
        uint32_t ret = *cpu->registers[15] - 1;
        *cpu->registers[15] = *cpu->registers[14] + BL_OFFSET_THUMB;
        *cpu->registers[14] = ret;
        cpu->cpsr &= ~BIT(5);
    }
}

void blSetup(interpreter::Cpu *cpu, uint32_t opcode) // BL/BLX label
{
    *cpu->registers[14] = *cpu->registers[15] + (B_OFFSET_THUMB << 11);
}

void blOff(interpreter::Cpu *cpu, uint32_t opcode) // BL label
{
    uint32_t ret = *cpu->registers[15] - 1;
    *cpu->registers[15] = *cpu->registers[14] + BL_OFFSET_THUMB;
    *cpu->registers[14] = ret;
}

}
