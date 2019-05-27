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

#include "interpreter_other.h"
#include "cp15.h"

#define BIT(i) (1 << i)

#define RD (*cpu->registers[(opcode & 0x0000F000) >> 12])
#define RM (*cpu->registers[(opcode & 0x0000000F)])

#define CN ((opcode & 0x000F0000) >> 16)
#define CP ((opcode & 0x000000E0) >> 5)
#define CM ((opcode & 0x0000000F))

#define B_OFFSET (((opcode & 0x00FFFFFF) << 2) | ((opcode & BIT(23)) ? 0xFC000000 : 0))
#define BX_DEST  (*cpu->registers[opcode & 0x0000000F])

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
    *cpu->registers[15] = BX_DEST;
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
        *cpu->registers[15] = BX_DEST;
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
            *cpu->spsr = (cpu->cpsr & ~0xFF000000) | (RM & 0xFF000000);
        if (opcode & BIT(16))
            *cpu->spsr = (cpu->cpsr & ~0x000000FF) | (RM & 0x000000FF);
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
    *cpu->registers[14] = *cpu->registers[15] - 4;
    *cpu->spsr = cpsr;
    cpu->cpsr |= BIT(7);
    *cpu->registers[15] = ((cpu->type == 9) ? cp15::exceptions : 0x00000000) + 8;
}

}
