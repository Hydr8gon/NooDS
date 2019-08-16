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

// Opcode condition
#define CONDITION ((opcode & 0xF0000000) >> 28)

// Register values for ARM opcodes
#define RD (*cpu->registers[(opcode & 0x0000F000) >> 12])
#define RM (*cpu->registers[(opcode & 0x0000000F)])

// CP15 register selectors
#define CN ((opcode & 0x000F0000) >> 16)
#define CP ((opcode & 0x000000E0) >> 5)
#define CM (opcode & 0x0000000F)

// Immediate value for MSR opcodes
#define MSR_IMM   (opcode & 0x000000FF)
#define MSR_SHIFT ((opcode & 0x00000F00) >> 7)
#define MSR_ROR   ((MSR_IMM << (32 - MSR_SHIFT)) | (MSR_IMM >> (MSR_SHIFT)))

// Branch offset for ARM opcodes
// Negative values have their MSB extended to 32 bits
#define B_OFFSET (((opcode & 0x00FFFFFF) << 2) | ((opcode & BIT(23)) ? 0xFC000000 : 0))

// Branch offsets for THUMB opcodes
// Negative values have their MSB extended to 32 bits
#define BCOND_OFFSET_THUMB (((opcode & 0x00FF) << 1) | ((opcode & BIT(7))  ? 0xFFFFFE00 : 0))
#define B_OFFSET_THUMB     (((opcode & 0x07FF) << 1) | ((opcode & BIT(10)) ? 0xFFFFF000 : 0))
#define BL_OFFSET_THUMB    ((opcode & 0x07FF) << 1)
#define BX_ADDRESS_THUMB   (*cpu->registers[(opcode & 0x0078) >> 3])

namespace interpreter_other
{

void mrsRc(interpreter::Cpu *cpu, uint32_t opcode) // MRS Rd,CPSR
{
    // Copy the status flags to a register
    RD = cpu->cpsr;
}

void msrRc(interpreter::Cpu *cpu, uint32_t opcode) // MSR CPSR,Rm
{
    // Write the first 8 bits of the status flags, but only allow changing the CPU mode when not in user mode
    if (opcode & BIT(16))
    {
        cpu->cpsr = (cpu->cpsr & ~0x000000E0) | (RM & 0x000000E0);
        if ((cpu->cpsr & 0x0000001F) != 0x10)
            interpreter::setMode(cpu, RM & 0x0000001F);
    }

    // Write the remaining 8-bit blocks of the status flags
    // Bits 16-19 of the opcode determine which blocks are written
    for (int i = 0; i < 3; i++)
    {
        if (opcode & BIT(17 + i))
            cpu->cpsr = (cpu->cpsr & ~(0x0000FF00 << (i * 8))) | (RM & (0x0000FF00 << (i * 8)));
    }
}

void bx(interpreter::Cpu *cpu, uint32_t opcode) // BX Rn
{
    // Branch and switch to THUMB mode if bit 0 is set
    *cpu->registers[15] = RM;
    if (*cpu->registers[15] & BIT(0))
        cpu->cpsr |= BIT(5);
    *cpu->registers[15] |= BIT(0);
}

void blx(interpreter::Cpu *cpu, uint32_t opcode) // BLX Rn
{
    // Branch to address with link and switch to THUMB mode if bit 0 is set (ARM9 exclusive)
    if (cpu->type == 9)
    {
        *cpu->registers[14] = *cpu->registers[15] - 4;
        *cpu->registers[15] = RM;
        if (*cpu->registers[15] & BIT(0))
            cpu->cpsr |= BIT(5);
        *cpu->registers[15] |= BIT(0);
    }
}

void mrsRs(interpreter::Cpu *cpu, uint32_t opcode) // MRS Rd,SPSR
{
    // Copy the saved status flags to a register
    if (cpu->spsr)
        RD = *cpu->spsr;
}

void msrRs(interpreter::Cpu *cpu, uint32_t opcode) // MSR SPSR,Rm
{
    // Write the saved status flags in 8-bit blocks
    // Bits 16-19 of the opcode determine which blocks are written
    if (cpu->spsr)
    {
        for (int i = 0; i < 4; i++)
        {
            if (opcode & BIT(16 + i))
                *cpu->spsr = (*cpu->spsr & ~(0x000000FF << (i * 8))) | (RM & (0x000000FF << (i * 8)));
        }
    }
}

void clz(interpreter::Cpu *cpu, uint32_t opcode)
{
    // Count the leading zeros of a register (ARM9 exclusive)
    if (cpu->type == 9)
    {
        uint32_t value = RM;
        uint8_t count = 0;
        while (value != 0)
        {
            value >>= 1;
            count++;
        }
        RD = 32 - count;
    }
}

void msrIc(interpreter::Cpu *cpu, uint32_t opcode) // MSR CPSR,#i
{
    uint32_t value = MSR_ROR;

    // Write the first 8 bits of the status flags, but only allow changing the CPU mode when not in user mode
    if (opcode & BIT(16))
    {
        cpu->cpsr = (cpu->cpsr & ~0x000000E0) | (value & 0x000000E0);
        if ((cpu->cpsr & 0x0000001F) != 0x10)
            interpreter::setMode(cpu, value & 0x0000001F);
    }

    // Write the remaining 8-bit blocks of the status flags
    // Bits 16-19 of the opcode determine which blocks are written
    for (int i = 0; i < 3; i++)
    {
        if (opcode & BIT(17 + i))
            cpu->cpsr = (cpu->cpsr & ~(0x0000FF00 << (i * 8))) | (value & (0x0000FF00 << (i * 8)));
    }
}

void msrIs(interpreter::Cpu *cpu, uint32_t opcode) // MSR SPSR,#i
{
    // Write the saved status flags in 8-bit blocks
    // Bits 16-19 of the opcode determine which blocks are written
    if (cpu->spsr)
    {
        uint32_t value = MSR_ROR;

        for (int i = 0; i < 4; i++)
        {
            if (opcode & BIT(16 + i))
                *cpu->spsr = (*cpu->spsr & ~(0x000000FF << (i * 8))) | (value & (0x000000FF << (i * 8)));
        }
    }
}

void b(interpreter::Cpu *cpu, uint32_t opcode)
{
    if (CONDITION == 0xF) // BLX label
    {
        // Branch to offset with link and switch to THUMB mode (ARM9 exclusive)
        if (cpu->type == 9)
        {
            *cpu->registers[14] = *cpu->registers[15] - 4;
            *cpu->registers[15] += B_OFFSET | BIT(0);
            cpu->cpsr |= BIT(5);
        }
    }
    else // B label
    {
        // Branch to offset
        *cpu->registers[15] += B_OFFSET | BIT(0);
    }
}

void bl(interpreter::Cpu *cpu, uint32_t opcode)
{
    if (CONDITION == 0xF) // BLX label
    {
        // Branch to offset plus halfword with link and switch to THUMB mode (ARM9 exclusive)
        if (cpu->type == 9)
        {
            *cpu->registers[14] = *cpu->registers[15] - 4;
            *cpu->registers[15] += (B_OFFSET + 2) | BIT(0);
            cpu->cpsr |= BIT(5);
        }
    }
    else // BL label
    {
        // Branch to offset with link
        *cpu->registers[14] = *cpu->registers[15] - 4;
        *cpu->registers[15] += B_OFFSET | BIT(0);
    }
}

void mcr(interpreter::Cpu *cpu, uint32_t opcode) // MCR Pn,<cpopc>,Rd,Cn,Cm,<cp>
{
    // Write to a CP15 register
    // Only the ARM9 has a CP15
    if (cpu->type == 9)
        cp15::writeRegister(CN, CM, CP, RD);
}

void mrc(interpreter::Cpu *cpu, uint32_t opcode) // MRC Pn,<cpopc>,Rd,Cn,Cm,<cp>
{
    // Read from a CP15 register
    // Only the ARM9 has a CP15
    if (cpu->type == 9)
        RD = cp15::readRegister(CN, CM, CP);
}

void swi(interpreter::Cpu *cpu, uint32_t opcode) // SWI #i
{
    // Software interrupt
    uint32_t cpsr = cpu->cpsr;
    setMode(cpu, 0x13);
    *cpu->spsr = cpsr;
    cpu->cpsr |= BIT(7);
    *cpu->registers[14] = *cpu->registers[15] - 4;
    *cpu->registers[15] = ((cpu->type == 9) ? cp15::exceptionBase : 0) + 0x08;
}

}

namespace interpreter_other_thumb
{

void bxReg(interpreter::Cpu *cpu, uint32_t opcode) // BX/BLX Rs
{
    if (opcode & BIT(7)) // BLX Rs
    {
        // Branch to address with link and switch to ARM mode if bit 0 is cleared (ARM9 exclusive)
        if (cpu->type == 9)
        {
            *cpu->registers[14] = *cpu->registers[15] - 1;
            *cpu->registers[15] = BX_ADDRESS_THUMB;
            if (!(*cpu->registers[15] & BIT(0)))
                cpu->cpsr &= ~BIT(5);
            *cpu->registers[15] |= BIT(0);
        }
    }
    else // BX Rs
    {
        // Branch to address and switch to ARM mode if bit 0 is cleared
        *cpu->registers[15] = BX_ADDRESS_THUMB;
        if (!(*cpu->registers[15] & BIT(0)))
            cpu->cpsr &= ~BIT(5);
        *cpu->registers[15] |= BIT(0);
    }
}

void beq(interpreter::Cpu *cpu, uint32_t opcode) // BEQ label
{
    // Branch to offset if equal
    if (cpu->cpsr & BIT(30))
        *cpu->registers[15] += BCOND_OFFSET_THUMB | BIT(0);
}

void bne(interpreter::Cpu *cpu, uint32_t opcode) // BNE label
{
    // Branch to offset if not equal
    if (!(cpu->cpsr & BIT(30)))
        *cpu->registers[15] += BCOND_OFFSET_THUMB | BIT(0);
}

void bcs(interpreter::Cpu *cpu, uint32_t opcode) // BCS label
{
    // Branch to offset if carry set
    if (cpu->cpsr & BIT(29))
        *cpu->registers[15] += BCOND_OFFSET_THUMB | BIT(0);
}

void bcc(interpreter::Cpu *cpu, uint32_t opcode) // BCC label
{
    // Branch to offset if carry clear
    if (!(cpu->cpsr & BIT(29)))
        *cpu->registers[15] += BCOND_OFFSET_THUMB | BIT(0);
}

void bmi(interpreter::Cpu *cpu, uint32_t opcode) // BMI label
{
    // Branch to offset if negative
    if (cpu->cpsr & BIT(31))
        *cpu->registers[15] += BCOND_OFFSET_THUMB | BIT(0);
}

void bpl(interpreter::Cpu *cpu, uint32_t opcode) // BPL label
{
    // Branch to offset if positive
    if (!(cpu->cpsr & BIT(31)))
        *cpu->registers[15] += BCOND_OFFSET_THUMB | BIT(0);
}

void bvs(interpreter::Cpu *cpu, uint32_t opcode) // BVS label
{
    // Branch to offset if overflow set
    if (cpu->cpsr & BIT(28))
        *cpu->registers[15] += BCOND_OFFSET_THUMB | BIT(0);
}

void bvc(interpreter::Cpu *cpu, uint32_t opcode) // BVC label
{
    // Branch to offset if overflow clear
    if (!(cpu->cpsr & BIT(28)))
        *cpu->registers[15] += BCOND_OFFSET_THUMB | BIT(0);
}

void bhi(interpreter::Cpu *cpu, uint32_t opcode) // BHI label
{
    // Branch to offset if higher
    if ((cpu->cpsr & BIT(29)) && !(cpu->cpsr & BIT(30)))
        *cpu->registers[15] += BCOND_OFFSET_THUMB | BIT(0);
}

void bls(interpreter::Cpu *cpu, uint32_t opcode) // BLS label
{
    // Branch to offset if lower or same
    if (!(cpu->cpsr & BIT(29)) || (cpu->cpsr & BIT(30)))
        *cpu->registers[15] += BCOND_OFFSET_THUMB | BIT(0);
}

void bge(interpreter::Cpu *cpu, uint32_t opcode) // BGE label
{
    // Branch to offset if signed greater or equal
    if ((cpu->cpsr & BIT(31)) == (cpu->cpsr & BIT(28)) << 3)
        *cpu->registers[15] += BCOND_OFFSET_THUMB | BIT(0);
}

void blt(interpreter::Cpu *cpu, uint32_t opcode) // BLT label
{
    // Branch to offset if signed less than
    if ((cpu->cpsr & BIT(31)) != (cpu->cpsr & BIT(28)) << 3)
        *cpu->registers[15] += BCOND_OFFSET_THUMB | BIT(0);
}

void bgt(interpreter::Cpu *cpu, uint32_t opcode) // BGT label
{
    // Branch to offset if signed greater than
    if (!(cpu->cpsr & BIT(30)) && (cpu->cpsr & BIT(31)) == (cpu->cpsr & BIT(28)) << 3)
        *cpu->registers[15] += BCOND_OFFSET_THUMB | BIT(0);
}

void ble(interpreter::Cpu *cpu, uint32_t opcode) // BLE label
{
    // Branch to offset if signed less or equal
    if ((cpu->cpsr & BIT(30)) || (cpu->cpsr & BIT(31)) != (cpu->cpsr & BIT(28)) << 3)
        *cpu->registers[15] += BCOND_OFFSET_THUMB | BIT(0);
}

void swi(interpreter::Cpu *cpu, uint32_t opcode) // SWI #i
{
    // Software interrupt
    uint32_t cpsr = cpu->cpsr;
    setMode(cpu, 0x13);
    *cpu->spsr = cpsr;
    cpu->cpsr &= ~BIT(5);
    cpu->cpsr |= BIT(7);
    *cpu->registers[14] = *cpu->registers[15] - 2;
    *cpu->registers[15] = ((cpu->type == 9) ? cp15::exceptionBase : 0) + 0x08;
}

void b(interpreter::Cpu *cpu, uint32_t opcode) // B label
{
    // Branch to offset
    *cpu->registers[15] += B_OFFSET_THUMB | BIT(0);
}

void blxOff(interpreter::Cpu *cpu, uint32_t opcode) // BLX label
{
    // Long branch to offset with link and switch to ARM mode (ARM9 exclusive)
    if (cpu->type == 9)
    {
        uint32_t ret = *cpu->registers[15] - 1;
        *cpu->registers[15] = ((*cpu->registers[14] & ~BIT(1)) + BL_OFFSET_THUMB) | BIT(0);
        *cpu->registers[14] = ret;
        cpu->cpsr &= ~BIT(5);
    }
}

void blSetup(interpreter::Cpu *cpu, uint32_t opcode) // BL/BLX label
{
    // Set the upper 11 bits of the target address for a long BL/BLX
    *cpu->registers[14] = *cpu->registers[15] + (B_OFFSET_THUMB << 11);
}

void blOff(interpreter::Cpu *cpu, uint32_t opcode) // BL label
{
    // Long branch to offset with link
    uint32_t ret = *cpu->registers[15] - 1;
    *cpu->registers[15] = (*cpu->registers[14] + BL_OFFSET_THUMB) | BIT(0);
    *cpu->registers[14] = ret;
}

}
