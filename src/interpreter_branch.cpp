/*
    Copyright 2019-2022 Hydr8gon

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

#include "interpreter.h"
#include "core.h"

int Interpreter::bx(uint32_t opcode) // BX Rn
{
    // Decode the operand
    uint32_t op0 = *registers[opcode & 0x0000000F];

    // Branch to address and switch to THUMB if bit 0 is set
    if (op0 & BIT(0)) cpsr |= BIT(5);
    *registers[15] = op0;
    flushPipeline();

    return 3;
}

int Interpreter::blxReg(uint32_t opcode) // BLX Rn
{
    if (cpu == 1) return 1; // ARM9 exclusive

    // Decode the operand
    uint32_t op0 = *registers[opcode & 0x0000000F];

    // Branch to address with link and switch to THUMB if bit 0 is set
    if (op0 & BIT(0)) cpsr |= BIT(5);
    *registers[14] = *registers[15] - 4;
    *registers[15] = op0;
    flushPipeline();

    return 3;
}

int Interpreter::b(uint32_t opcode) // B label
{
    // Decode the operand
    uint32_t op0 = ((opcode & BIT(23)) ? 0xFC000000 : 0) | ((opcode & 0x00FFFFFF) << 2);

    // Branch to offset
    *registers[15] += op0;
    flushPipeline();

    return 3;
}

int Interpreter::bl(uint32_t opcode) // BL label
{
    // Decode the operand
    uint32_t op0 = ((opcode & BIT(23)) ? 0xFC000000 : 0) | ((opcode & 0x00FFFFFF) << 2);

    // Branch to offset with link
    *registers[14] = *registers[15] - 4;
    *registers[15] += op0;
    flushPipeline();

    return 3;
}

int Interpreter::blx(uint32_t opcode) // BLX label
{
    if (cpu == 1) return 1; // ARM9 exclusive

    // Decode the operand
    uint32_t op0 = ((opcode & BIT(23)) ? 0xFC000000 : 0) | ((opcode & 0x00FFFFFF) << 2) | ((opcode & BIT(24)) >> 23);

    // Branch to offset with link and switch to THUMB
    cpsr |= BIT(5);
    *registers[14] = *registers[15] - 4;
    *registers[15] += op0;
    flushPipeline();

    return 3;
}

int Interpreter::swi(uint32_t opcode) // SWI #i
{
    // Software interrupt
    setCpsr((cpsr & ~0x1F) | 0x93, true); // Supervisor, interrupts off
    *registers[14] = *registers[15] - 4;
    *registers[15] = ((cpu == 0) ? core->cp15.getExceptionAddr() : 0x00000000) + 0x08;
    flushPipeline();

    return 3;
}

int Interpreter::bxRegT(uint16_t opcode) // BX Rs
{
    // Decode the operand
    uint32_t op0 = *registers[(opcode & 0x0078) >> 3];

    // Branch to address and switch to ARM mode if bit 0 is cleared
    if (!(op0 & BIT(0))) cpsr &= ~BIT(5);
    *registers[15] = op0;
    flushPipeline();

    return 3;
}

int Interpreter::blxRegT(uint16_t opcode) // BLX Rs
{
    if (cpu == 1) return 1; // ARM9 exclusive

    // Decode the operand
    uint32_t op0 = *registers[(opcode & 0x0078) >> 3];

    // Branch to address with link and switch to ARM mode if bit 0 is cleared
    if (!(op0 & BIT(0))) cpsr &= ~BIT(5);
    *registers[14] = *registers[15] - 1;
    *registers[15] = op0;
    flushPipeline();

    return 3;
}

int Interpreter::beqT(uint16_t opcode) // BEQ label
{
    // Decode the operand
    uint32_t op0 = ((opcode & BIT(7)) ? 0xFFFFFE00 : 0) | ((opcode & 0x00FF) << 1);

    // Branch to offset if equal
    if (cpsr & BIT(30))
    {
        *registers[15] += op0;
        flushPipeline();
        return 3;
    }

    return 1;
}

int Interpreter::bneT(uint16_t opcode) // BNE label
{
    // Decode the operand
    uint32_t op0 = ((opcode & BIT(7)) ? 0xFFFFFE00 : 0) | ((opcode & 0x00FF) << 1);

    // Branch to offset if not equal
    if (!(cpsr & BIT(30)))
    {
        *registers[15] += op0;
        flushPipeline();
        return 3;
    }

    return 1;
}

int Interpreter::bcsT(uint16_t opcode) // BCS label
{
    // Decode the operand
    uint32_t op0 = ((opcode & BIT(7)) ? 0xFFFFFE00 : 0) | ((opcode & 0x00FF) << 1);

    // Branch to offset if carry set
    if (cpsr & BIT(29))
    {
        *registers[15] += op0;
        flushPipeline();
        return 3;
    }

    return 1;
}

int Interpreter::bccT(uint16_t opcode) // BCC label
{
    // Decode the operand
    uint32_t op0 = ((opcode & BIT(7)) ? 0xFFFFFE00 : 0) | ((opcode & 0x00FF) << 1);

    // Branch to offset if carry clear
    if (!(cpsr & BIT(29)))
    {
        *registers[15] += op0;
        flushPipeline();
        return 3;
    }

    return 1;
}

int Interpreter::bmiT(uint16_t opcode) // BMI label
{
    // Decode the operand
    uint32_t op0 = ((opcode & BIT(7)) ? 0xFFFFFE00 : 0) | ((opcode & 0x00FF) << 1);

    // Branch to offset if negative
    if (cpsr & BIT(31))
    {
        *registers[15] += op0;
        flushPipeline();
        return 3;
    }

    return 1;
}

int Interpreter::bplT(uint16_t opcode) // BPL label
{
    // Decode the operand
    uint32_t op0 = ((opcode & BIT(7)) ? 0xFFFFFE00 : 0) | ((opcode & 0x00FF) << 1);

    // Branch to offset if positive
    if (!(cpsr & BIT(31)))
    {
        *registers[15] += op0;
        flushPipeline();
        return 3;
    }

    return 1;
}

int Interpreter::bvsT(uint16_t opcode) // BVS label
{
    // Decode the operand
    uint32_t op0 = ((opcode & BIT(7)) ? 0xFFFFFE00 : 0) | ((opcode & 0x00FF) << 1);

    // Branch to offset if overflow set
    if (cpsr & BIT(28))
    {
        *registers[15] += op0;
        flushPipeline();
        return 3;
    }

    return 1;
}

int Interpreter::bvcT(uint16_t opcode) // BVC label
{
    // Decode the operand
    uint32_t op0 = ((opcode & BIT(7)) ? 0xFFFFFE00 : 0) | ((opcode & 0x00FF) << 1);

    // Branch to offset if overflow clear
    if (!(cpsr & BIT(28)))
    {
        *registers[15] += op0;
        flushPipeline();
        return 3;
    }

    return 1;
}

int Interpreter::bhiT(uint16_t opcode) // BHI label
{
    // Decode the operand
    uint32_t op0 = ((opcode & BIT(7)) ? 0xFFFFFE00 : 0) | ((opcode & 0x00FF) << 1);

    // Branch to offset if higher
    if ((cpsr & BIT(29)) && !(cpsr & BIT(30)))
    {
        *registers[15] += op0;
        flushPipeline();
        return 3;
    }

    return 1;
}

int Interpreter::blsT(uint16_t opcode) // BLS label
{
    // Decode the operand
    uint32_t op0 = ((opcode & BIT(7)) ? 0xFFFFFE00 : 0) | ((opcode & 0x00FF) << 1);

    // Branch to offset if lower or same
    if (!(cpsr & BIT(29)) || (cpsr & BIT(30)))
    {
        *registers[15] += op0;
        flushPipeline();
        return 3;
    }

    return 1;
}

int Interpreter::bgeT(uint16_t opcode) // BGE label
{
    // Decode the operand
    uint32_t op0 = ((opcode & BIT(7)) ? 0xFFFFFE00 : 0) | ((opcode & 0x00FF) << 1);

    // Branch to offset if signed greater or equal
    if ((cpsr & BIT(31)) == (cpsr & BIT(28)) << 3)
    {
        *registers[15] += op0;
        flushPipeline();
        return 3;
    }

    return 1;
}

int Interpreter::bltT(uint16_t opcode) // BLT label
{
    // Decode the operand
    uint32_t op0 = ((opcode & BIT(7)) ? 0xFFFFFE00 : 0) | ((opcode & 0x00FF) << 1);

    // Branch to offset if signed less than
    if ((cpsr & BIT(31)) != (cpsr & BIT(28)) << 3)
    {
        *registers[15] += op0;
        flushPipeline();
        return 3;
    }

    return 1;
}

int Interpreter::bgtT(uint16_t opcode) // BGT label
{
    // Decode the operand
    uint32_t op0 = ((opcode & BIT(7)) ? 0xFFFFFE00 : 0) | ((opcode & 0x00FF) << 1);

    // Branch to offset if signed greater than
    if (!(cpsr & BIT(30)) && (cpsr & BIT(31)) == (cpsr & BIT(28)) << 3)
    {
        *registers[15] += op0;
        flushPipeline();
        return 3;
    }

    return 1;
}

int Interpreter::bleT(uint16_t opcode) // BLE label
{
    // Decode the operand
    uint32_t op0 = ((opcode & BIT(7)) ? 0xFFFFFE00 : 0) | ((opcode & 0x00FF) << 1);

    // Branch to offset if signed less or equal
    if ((cpsr & BIT(30)) || (cpsr & BIT(31)) != (cpsr & BIT(28)) << 3)
    {
        *registers[15] += op0;
        flushPipeline();
        return 3;
    }

    return 1;
}

int Interpreter::bT(uint16_t opcode) // B label
{
    // Decode the operand
    uint32_t op0 = ((opcode & BIT(10)) ? 0xFFFFF000 : 0) | ((opcode & 0x07FF) << 1);

    // Branch to offset
    *registers[15] += op0;
    flushPipeline();

    return 3;
}

int Interpreter::blSetupT(uint16_t opcode) // BL/BLX label
{
    // Decode the operand
    uint32_t op0 = ((opcode & BIT(10)) ? 0xFFFFF000 : 0) | ((opcode & 0x07FF) << 1);

    // Set the upper 11 bits of the target address for a long BL/BLX
    *registers[14] = *registers[15] + (op0 << 11);

    return 1;
}

int Interpreter::blOffT(uint16_t opcode) // BL label
{
    // Decode the operand
    uint32_t op0 = (opcode & 0x07FF) << 1;

    // Long branch to offset with link
    uint32_t ret = *registers[15] - 1;
    *registers[15] = *registers[14] + op0;
    *registers[14] = ret;
    flushPipeline();

    return 3;
}

int Interpreter::blxOffT(uint16_t opcode) // BLX label
{
    if (cpu == 1) return 1; // ARM9 exclusive

    // Decode the operand
    uint32_t op0 = (opcode & 0x07FF) << 1;

    // Long branch to offset with link and switch to ARM mode
    cpsr &= ~BIT(5);
    uint32_t ret = *registers[15] - 1;
    *registers[15] = *registers[14] + op0;
    *registers[14] = ret;
    flushPipeline();

    return 3;
}

int Interpreter::swiT(uint16_t opcode) // SWI #i
{
    // Software interrupt
    setCpsr((cpsr & ~0x3F) | 0x93, true); // ARM, supervisor, interrupts off
    *registers[14] = *registers[15] - 2;
    *registers[15] = ((cpu == 0) ? core->cp15.getExceptionAddr() : 0x00000000) + 0x08;
    flushPipeline();

    return 3;
}
