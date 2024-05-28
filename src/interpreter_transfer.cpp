/*
    Copyright 2019-2024 Hydr8gon

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

// Define functions for each ARM offset variation (half type)
#define HALF_FUNCS(func)                                                                    \
    int Interpreter::func##Ofrm(uint32_t opcode) { return func##Of(opcode, -rp(opcode));  } \
    int Interpreter::func##Ofim(uint32_t opcode) { return func##Of(opcode, -ipH(opcode)); } \
    int Interpreter::func##Ofrp(uint32_t opcode) { return func##Of(opcode,  rp(opcode));  } \
    int Interpreter::func##Ofip(uint32_t opcode) { return func##Of(opcode,  ipH(opcode)); } \
    int Interpreter::func##Prrm(uint32_t opcode) { return func##Pr(opcode, -rp(opcode));  } \
    int Interpreter::func##Prim(uint32_t opcode) { return func##Pr(opcode, -ipH(opcode)); } \
    int Interpreter::func##Prrp(uint32_t opcode) { return func##Pr(opcode,  rp(opcode));  } \
    int Interpreter::func##Prip(uint32_t opcode) { return func##Pr(opcode,  ipH(opcode)); } \
    int Interpreter::func##Ptrm(uint32_t opcode) { return func##Pt(opcode, -rp(opcode));  } \
    int Interpreter::func##Ptim(uint32_t opcode) { return func##Pt(opcode, -ipH(opcode)); } \
    int Interpreter::func##Ptrp(uint32_t opcode) { return func##Pt(opcode,  rp(opcode));  } \
    int Interpreter::func##Ptip(uint32_t opcode) { return func##Pt(opcode,  ipH(opcode)); }

// Define functions for each ARM offset variation (full type)
#define FULL_FUNCS(func) \
    int Interpreter::func##Ofim(uint32_t opcode)   { return func##Of(opcode, -ip(opcode));   } \
    int Interpreter::func##Ofip(uint32_t opcode)   { return func##Of(opcode,  ip(opcode));   } \
    int Interpreter::func##Ofrmll(uint32_t opcode) { return func##Of(opcode, -rpll(opcode)); } \
    int Interpreter::func##Ofrmlr(uint32_t opcode) { return func##Of(opcode, -rplr(opcode)); } \
    int Interpreter::func##Ofrmar(uint32_t opcode) { return func##Of(opcode, -rpar(opcode)); } \
    int Interpreter::func##Ofrmrr(uint32_t opcode) { return func##Of(opcode, -rprr(opcode)); } \
    int Interpreter::func##Ofrpll(uint32_t opcode) { return func##Of(opcode,  rpll(opcode)); } \
    int Interpreter::func##Ofrplr(uint32_t opcode) { return func##Of(opcode,  rplr(opcode)); } \
    int Interpreter::func##Ofrpar(uint32_t opcode) { return func##Of(opcode,  rpar(opcode)); } \
    int Interpreter::func##Ofrprr(uint32_t opcode) { return func##Of(opcode,  rprr(opcode)); } \
    int Interpreter::func##Prim(uint32_t opcode)   { return func##Pr(opcode, -ip(opcode));   } \
    int Interpreter::func##Prip(uint32_t opcode)   { return func##Pr(opcode,  ip(opcode));   } \
    int Interpreter::func##Prrmll(uint32_t opcode) { return func##Pr(opcode, -rpll(opcode)); } \
    int Interpreter::func##Prrmlr(uint32_t opcode) { return func##Pr(opcode, -rplr(opcode)); } \
    int Interpreter::func##Prrmar(uint32_t opcode) { return func##Pr(opcode, -rpar(opcode)); } \
    int Interpreter::func##Prrmrr(uint32_t opcode) { return func##Pr(opcode, -rprr(opcode)); } \
    int Interpreter::func##Prrpll(uint32_t opcode) { return func##Pr(opcode,  rpll(opcode)); } \
    int Interpreter::func##Prrplr(uint32_t opcode) { return func##Pr(opcode,  rplr(opcode)); } \
    int Interpreter::func##Prrpar(uint32_t opcode) { return func##Pr(opcode,  rpar(opcode)); } \
    int Interpreter::func##Prrprr(uint32_t opcode) { return func##Pr(opcode,  rprr(opcode)); } \
    int Interpreter::func##Ptim(uint32_t opcode)   { return func##Pt(opcode, -ip(opcode));   } \
    int Interpreter::func##Ptip(uint32_t opcode)   { return func##Pt(opcode,  ip(opcode));   } \
    int Interpreter::func##Ptrmll(uint32_t opcode) { return func##Pt(opcode, -rpll(opcode)); } \
    int Interpreter::func##Ptrmlr(uint32_t opcode) { return func##Pt(opcode, -rplr(opcode)); } \
    int Interpreter::func##Ptrmar(uint32_t opcode) { return func##Pt(opcode, -rpar(opcode)); } \
    int Interpreter::func##Ptrmrr(uint32_t opcode) { return func##Pt(opcode, -rprr(opcode)); } \
    int Interpreter::func##Ptrpll(uint32_t opcode) { return func##Pt(opcode,  rpll(opcode)); } \
    int Interpreter::func##Ptrplr(uint32_t opcode) { return func##Pt(opcode,  rplr(opcode)); } \
    int Interpreter::func##Ptrpar(uint32_t opcode) { return func##Pt(opcode,  rpar(opcode)); } \
    int Interpreter::func##Ptrprr(uint32_t opcode) { return func##Pt(opcode,  rprr(opcode)); }

// Create functions for instructions that have offset variations (half type)
HALF_FUNCS(ldrsb)
HALF_FUNCS(ldrsh)
HALF_FUNCS(ldrh)
HALF_FUNCS(strh)
HALF_FUNCS(ldrd)
HALF_FUNCS(strd)

// Create functions for instructions that have offset variations (full type)
FULL_FUNCS(ldrb)
FULL_FUNCS(strb)
FULL_FUNCS(ldr)
FULL_FUNCS(str)

FORCE_INLINE uint32_t Interpreter::ip(uint32_t opcode) // #i (B/_)
{
    // Immediate offset for byte and word transfers
    return opcode & 0xFFF;
}

FORCE_INLINE uint32_t Interpreter::ipH(uint32_t opcode) // #i (SB/SH/H/D)
{
    // Immediate offset for signed, half-word, and double word transfers
    return ((opcode >> 4) & 0xF0) | (opcode & 0xF);
}

FORCE_INLINE uint32_t Interpreter::rp(uint32_t opcode) // Rm
{
    // Register offset for signed and half-word transfers
    return *registers[opcode & 0xF];
}

FORCE_INLINE uint32_t Interpreter::rpll(uint32_t opcode) // Rm,LSL #i
{
    // Logical shift left by immediate
    uint32_t value = *registers[opcode & 0xF];
    uint8_t shift = (opcode >> 7) & 0x1F;
    return value << shift;
}

FORCE_INLINE uint32_t Interpreter::rplr(uint32_t opcode) // Rm,LSR #i
{
    // Logical shift right by immediate
    // A shift of 0 translates to a shift of 32
    uint32_t value = *registers[opcode & 0xF];
    uint8_t shift = (opcode >> 7) & 0x1F;
    return shift ? (value >> shift) : 0;
}

FORCE_INLINE uint32_t Interpreter::rpar(uint32_t opcode) // Rm,ASR #i
{
    // Arithmetic shift right by immediate
    // A shift of 0 translates to a shift of 32
    int32_t value = *registers[opcode & 0xF];
    uint8_t shift = (opcode >> 7) & 0x1F;
    return value >> (shift ? shift : 31);
}

FORCE_INLINE uint32_t Interpreter::rprr(uint32_t opcode) // Rm,ROR #i
{
    // Rotate right by immediate
    // A shift of 0 translates to a1 rotate with carry of 1
    uint32_t value = *registers[opcode & 0xF];
    uint8_t shift = (opcode >> 7) & 0x1F;
    return shift ? ((value << (32 - shift)) | (value >> shift)) : (((cpsr & BIT(29)) << 2) | (value >> 1));
}

FORCE_INLINE int Interpreter::ldrsbOf(uint32_t opcode, uint32_t op2) // LDRSB Rd,[Rn,op2]
{
    // Signed byte load, pre-adjust without writeback
    uint32_t *op0 = registers[(opcode >> 12) & 0xF];
    uint32_t op1 = *registers[(opcode >> 16) & 0xF];
    *op0 = (int8_t)core->memory.read<uint8_t>(arm7, op1 + op2);

    // Handle pipelining
    if (op0 != registers[15]) return (arm7 << 1) + 1;
    flushPipeline();
    return 5;
}

FORCE_INLINE int Interpreter::ldrshOf(uint32_t opcode, uint32_t op2) // LDRSH Rd,[Rn,op2]
{
    // Signed half-word load, pre-adjust without writeback
    uint32_t *op0 = registers[(opcode >> 12) & 0xF];
    uint32_t op1 = *registers[(opcode >> 16) & 0xF];
    *op0 = (int16_t)core->memory.read<uint16_t>(arm7, op1 += op2);

    // Shift misaligned reads on ARM7
    if (op1 & arm7)
        *op0 = (int16_t)*op0 >> 8;

    // Handle pipelining
    if (op0 != registers[15]) return (arm7 << 1) + 1;
    flushPipeline();
    return 5;
}

FORCE_INLINE int Interpreter::ldrbOf(uint32_t opcode, uint32_t op2) // LDRB Rd,[Rn,op2]
{
    // Byte load, pre-adjust without writeback
    uint32_t *op0 = registers[(opcode >> 12) & 0xF];
    uint32_t op1 = *registers[(opcode >> 16) & 0xF];
    *op0 = core->memory.read<uint8_t>(arm7, op1 + op2);

    // Handle pipelining and THUMB switching
    if (op0 != registers[15]) return (arm7 << 1) + 1;
    cpsr |= (*op0 & !arm7) << 5;
    flushPipeline();
    return 5;
}

FORCE_INLINE int Interpreter::strbOf(uint32_t opcode, uint32_t op2) // STRB Rd,[Rn,op2]
{
    // Byte store, pre-adjust without writeback
    // When used as Rd, the program counter is read with +4
    uint32_t op0 = *registers[(opcode >> 12) & 0xF] + (((opcode & 0xF000) == 0xF000) << 2);
    uint32_t op1 = *registers[(opcode >> 16) & 0xF];
    core->memory.write<uint8_t>(arm7, op1 + op2, op0);
    return arm7 + 1;
}

FORCE_INLINE int Interpreter::ldrhOf(uint32_t opcode, uint32_t op2) // LDRH Rd,[Rn,op2]
{
    // Half-word load, pre-adjust without writeback
    uint32_t *op0 = registers[(opcode >> 12) & 0xF];
    uint32_t op1 = *registers[(opcode >> 16) & 0xF];
    *op0 = core->memory.read<uint16_t>(arm7, op1 += op2);

    // Rotate misaligned reads on ARM7
    if (op1 & arm7)
        *op0 = (*op0 << 24) | (*op0 >> 8);

    // Handle pipelining
    if (op0 != registers[15]) return (arm7 << 1) + 1;
    flushPipeline();
    return 5;
}

FORCE_INLINE int Interpreter::strhOf(uint32_t opcode, uint32_t op2) // STRH Rd,[Rn,op2]
{
    // Half-word store, pre-adjust without writeback
    // When used as Rd, the program counter is read with +4
    uint32_t op0 = *registers[(opcode >> 12) & 0xF] + (((opcode & 0xF000) == 0xF000) << 2);
    uint32_t op1 = *registers[(opcode >> 16) & 0xF];
    core->memory.write<uint16_t>(arm7, op1 + op2, op0);
    return arm7 + 1;
}

FORCE_INLINE int Interpreter::ldrOf(uint32_t opcode, uint32_t op2) // LDR Rd,[Rn,op2]
{
    // Word load, pre-adjust without writeback
    uint32_t *op0 = registers[(opcode >> 12) & 0xF];
    uint32_t op1 = *registers[(opcode >> 16) & 0xF];
    *op0 = core->memory.read<uint32_t>(arm7, op1 += op2);

    // Rotate misaligned reads
    if (op1 & 0x3)
    {
        uint8_t shift = (op1 & 0x3) << 3;
        *op0 = (*op0 << (32 - shift)) | (*op0 >> shift);
    }

    // Handle pipelining and THUMB switching
    if (op0 != registers[15]) return (arm7 << 1) + 1;
    cpsr |= (*op0 & !arm7) << 5;
    flushPipeline();
    return 5;
}

FORCE_INLINE int Interpreter::strOf(uint32_t opcode, uint32_t op2) // STR Rd,[Rn,op2]
{
    // Word store, pre-adjust without writeback
    // When used as Rd, the program counter is read with +4
    uint32_t op0 = *registers[(opcode >> 12) & 0xF] + (((opcode & 0xF000) == 0xF000) << 2);
    uint32_t op1 = *registers[(opcode >> 16) & 0xF];
    core->memory.write<uint32_t>(arm7, op1 + op2, op0);
    return arm7 + 1;
}

FORCE_INLINE int Interpreter::ldrdOf(uint32_t opcode, uint32_t op2) // LDRD Rd,[Rn,op2]
{
    // Double word load, pre-adjust without writeback
    uint8_t op0 = (opcode >> 12) & 0xF;
    if (arm7 || op0 == 15) return 1; // ARM9 exclusive
    uint32_t op1 = *registers[(opcode >> 16) & 0xF];
    *registers[op0] = core->memory.read<uint32_t>(arm7, op1 += op2);
    *registers[op0 + 1] = core->memory.read<uint32_t>(arm7, op1 + 4);
    return 2;
}

FORCE_INLINE int Interpreter::strdOf(uint32_t opcode, uint32_t op2) // STRD Rd,[Rn,op2]
{
    // Double word store, pre-adjust without writeback
    uint8_t op0 = (opcode >> 12) & 0xF;
    if (arm7 || op0 == 15) return 1; // ARM9 exclusive
    uint32_t op1 = *registers[(opcode >> 16) & 0xF];
    core->memory.write<uint32_t>(arm7, op1 += op2, *registers[op0]);
    core->memory.write<uint32_t>(arm7, op1 + 4, *registers[op0 + 1]);
    return 2;
}

FORCE_INLINE int Interpreter::ldrsbPr(uint32_t opcode, uint32_t op2) // LDRSB Rd,[Rn,op2]!
{
    // Signed byte load, pre-adjust with writeback
    uint32_t *op0 = registers[(opcode >> 12) & 0xF];
    uint32_t *op1 = registers[(opcode >> 16) & 0xF];
    *op0 = (int8_t)core->memory.read<uint8_t>(arm7, *op1 += op2);

    // Handle pipelining
    if (op0 != registers[15]) return (arm7 << 1) + 1;
    flushPipeline();
    return 5;
}

FORCE_INLINE int Interpreter::ldrshPr(uint32_t opcode, uint32_t op2) // LDRSH Rd,[Rn,op2]!
{
    // Signed half-word load, pre-adjust with writeback
    uint32_t *op0 = registers[(opcode >> 12) & 0xF];
    uint32_t *op1 = registers[(opcode >> 16) & 0xF];
    uint32_t address = *op1 += op2;
    *op0 = (int16_t)core->memory.read<uint16_t>(arm7, address);

    // Shift misaligned reads on ARM7
    if (address & arm7)
        *op0 = (int16_t)*op0 >> 8;

    // Handle pipelining
    if (op0 != registers[15]) return (arm7 << 1) + 1;
    flushPipeline();
    return 5;
}

FORCE_INLINE int Interpreter::ldrbPr(uint32_t opcode, uint32_t op2) // LDRB Rd,[Rn,op2]!
{
    // Byte load, pre-adjust with writeback
    uint32_t *op0 = registers[(opcode >> 12) & 0xF];
    uint32_t *op1 = registers[(opcode >> 16) & 0xF];
    *op0 = core->memory.read<uint8_t>(arm7, *op1 += op2);

    // Handle pipelining and THUMB switching
    if (op0 != registers[15]) return (arm7 << 1) + 1;
    cpsr |= (*op0 & !arm7) << 5;
    flushPipeline();
    return 5;
}

FORCE_INLINE int Interpreter::strbPr(uint32_t opcode, uint32_t op2) // STRB Rd,[Rn,op2]!
{
    // Byte store, pre-adjust with writeback
    // When used as Rd, the program counter is read with +4
    uint32_t op0 = *registers[(opcode >> 12) & 0xF] + (((opcode & 0xF000) == 0xF000) << 2);
    uint32_t *op1 = registers[(opcode >> 16) & 0xF];
    core->memory.write<uint8_t>(arm7, *op1 += op2, op0);
    return arm7 + 1;
}

FORCE_INLINE int Interpreter::ldrhPr(uint32_t opcode, uint32_t op2) // LDRH Rd,[Rn,op2]!
{
    // Half-word load, pre-adjust with writeback
    uint32_t *op0 = registers[(opcode >> 12) & 0xF];
    uint32_t *op1 = registers[(opcode >> 16) & 0xF];
    uint32_t address = *op1 += op2;
    *op0 = core->memory.read<uint16_t>(arm7, address);

    // Rotate misaligned reads on ARM7
    if (address & arm7)
        *op0 = (*op0 << 24) | (*op0 >> 8);

    // Handle pipelining
    if (op0 != registers[15]) return (arm7 << 1) + 1;
    flushPipeline();
    return 5;
}

FORCE_INLINE int Interpreter::strhPr(uint32_t opcode, uint32_t op2) // STRH Rd,[Rn,op2]!
{
    // Half-word store, pre-adjust with writeback
    // When used as Rd, the program counter is read with +4
    uint32_t op0 = *registers[(opcode >> 12) & 0xF] + (((opcode & 0xF000) == 0xF000) << 2);
    uint32_t *op1 = registers[(opcode >> 16) & 0xF];
    core->memory.write<uint16_t>(arm7, *op1 += op2, op0);
    return arm7 + 1;
}

FORCE_INLINE int Interpreter::ldrPr(uint32_t opcode, uint32_t op2) // LDR Rd,[Rn,op2]!
{
    // Word load, pre-adjust with writeback
    uint32_t *op0 = registers[(opcode >> 12) & 0xF];
    uint32_t *op1 = registers[(opcode >> 16) & 0xF];
    uint32_t address = *op1 += op2;
    *op0 = core->memory.read<uint32_t>(arm7, address);

    // Rotate misaligned reads
    if (address & 0x3)
    {
        uint8_t shift = (address & 0x3) << 3;
        *op0 = (*op0 << (32 - shift)) | (*op0 >> shift);
    }

    // Handle pipelining and THUMB switching
    if (op0 != registers[15]) return (arm7 << 1) + 1;
    cpsr |= (*op0 & !arm7) << 5;
    flushPipeline();
    return 5;
}

FORCE_INLINE int Interpreter::strPr(uint32_t opcode, uint32_t op2) // STR Rd,[Rn,op2]!
{
    // Word store, pre-adjust with writeback
    // When used as Rd, the program counter is read with +4
    uint32_t op0 = *registers[(opcode >> 12) & 0xF] + (((opcode & 0xF000) == 0xF000) << 2);
    uint32_t *op1 = registers[(opcode >> 16) & 0xF];
    core->memory.write<uint32_t>(arm7, *op1 += op2, op0);
    return arm7 + 1;
}

FORCE_INLINE int Interpreter::ldrdPr(uint32_t opcode, uint32_t op2) // LDRD Rd,[Rn,op2]!
{
    // Double word load, pre-adjust with writeback
    uint8_t op0 = (opcode >> 12) & 0xF;
    if (arm7 || op0 == 15) return 1; // ARM9 exclusive
    uint32_t *op1 = registers[(opcode >> 16) & 0xF];
    *registers[op0] = core->memory.read<uint32_t>(arm7, *op1 += op2);
    *registers[op0 + 1] = core->memory.read<uint32_t>(arm7, *op1 + 4);
    return 2;
}

FORCE_INLINE int Interpreter::strdPr(uint32_t opcode, uint32_t op2) // STRD Rd,[Rn,op2]!
{
    // Double word store, pre-adjust with writeback
    uint8_t op0 = (opcode >> 12) & 0xF;
    if (arm7 || op0 == 15) return 1; // ARM9 exclusive
    uint32_t *op1 = registers[(opcode >> 16) & 0xF];
    core->memory.write<uint32_t>(arm7, *op1 += op2, *registers[op0]);
    core->memory.write<uint32_t>(arm7, *op1 + 4, *registers[op0 + 1]);
    return 2;
}

FORCE_INLINE int Interpreter::ldrsbPt(uint32_t opcode, uint32_t op2) // LDRSB Rd,[Rn],op2
{
    // Signed byte load, post-adjust
    uint32_t *op0 = registers[(opcode >> 12) & 0xF];
    uint32_t *op1 = registers[(opcode >> 16) & 0xF];
    uint32_t address = (*op1 += op2) - op2;
    *op0 = (int8_t)core->memory.read<uint8_t>(arm7, address);

    // Handle pipelining
    if (op0 != registers[15]) return (arm7 << 1) + 1;
    flushPipeline();
    return 5;
}

FORCE_INLINE int Interpreter::ldrshPt(uint32_t opcode, uint32_t op2) // LDRSH Rd,[Rn],op2
{
    // Signed half-word load, post-adjust
    uint32_t *op0 = registers[(opcode >> 12) & 0xF];
    uint32_t *op1 = registers[(opcode >> 16) & 0xF];
    uint32_t address = (*op1 += op2) - op2;
    *op0 = (int16_t)core->memory.read<uint16_t>(arm7, address);

    // Shift misaligned reads on ARM7
    if (address & arm7)
        *op0 = (int16_t)*op0 >> 8;

    // Handle pipelining
    if (op0 != registers[15]) return (arm7 << 1) + 1;
    flushPipeline();
    return 5;
}

FORCE_INLINE int Interpreter::ldrbPt(uint32_t opcode, uint32_t op2) // LDRB Rd,[Rn],op2
{
    // Byte load, post-adjust
    uint32_t *op0 = registers[(opcode >> 12) & 0xF];
    uint32_t *op1 = registers[(opcode >> 16) & 0xF];
    uint32_t address = (*op1 += op2) - op2;
    *op0 = core->memory.read<uint8_t>(arm7, address);

    // Handle pipelining and THUMB switching
    if (op0 != registers[15]) return (arm7 << 1) + 1;
    cpsr |= (*op0 & !arm7) << 5;
    flushPipeline();
    return 5;
}

FORCE_INLINE int Interpreter::strbPt(uint32_t opcode, uint32_t op2) // STRB Rd,[Rn],op2
{
    // Byte store, post-adjust
    // When used as Rd, the program counter is read with +4
    uint32_t op0 = *registers[(opcode >> 12) & 0xF] + (((opcode & 0xF000) == 0xF000) << 2);
    uint32_t *op1 = registers[(opcode >> 16) & 0xF];
    core->memory.write<uint8_t>(arm7, *op1, op0);
    *op1 += op2;
    return arm7 + 1;
}

FORCE_INLINE int Interpreter::ldrhPt(uint32_t opcode, uint32_t op2) // LDRH Rd,[Rn],op2
{
    // Half-word load, post-adjust
    uint32_t *op0 = registers[(opcode >> 12) & 0xF];
    uint32_t *op1 = registers[(opcode >> 16) & 0xF];
    uint32_t address = (*op1 += op2) - op2;
    *op0 = core->memory.read<uint16_t>(arm7, address);

    // Rotate misaligned reads on ARM7
    if (address & arm7)
        *op0 = (*op0 << 24) | (*op0 >> 8);

    // Handle pipelining
    if (op0 != registers[15]) return (arm7 << 1) + 1;
    flushPipeline();
    return 5;
}

FORCE_INLINE int Interpreter::strhPt(uint32_t opcode, uint32_t op2) // STRH Rd,[Rn],op2
{
    // Half-word store, post-adjust
    // When used as Rd, the program counter is read with +4
    uint32_t op0 = *registers[(opcode >> 12) & 0xF] + (((opcode & 0xF000) == 0xF000) << 2);
    uint32_t *op1 = registers[(opcode >> 16) & 0xF];
    core->memory.write<uint16_t>(arm7, *op1, op0);
    *op1 += op2;
    return arm7 + 1;
}

FORCE_INLINE int Interpreter::ldrPt(uint32_t opcode, uint32_t op2) // LDR Rd,[Rn],op2
{
    // Word load, post-adjust
    uint32_t *op0 = registers[(opcode >> 12) & 0xF];
    uint32_t *op1 = registers[(opcode >> 16) & 0xF];
    uint32_t address = (*op1 += op2) - op2;
    *op0 = core->memory.read<uint32_t>(arm7, address);

    // Rotate misaligned reads
    if (address & 0x3)
    {
        uint8_t shift = (address & 0x3) << 3;
        *op0 = (*op0 << (32 - shift)) | (*op0 >> shift);
    }

    // Handle pipelining and THUMB switching
    if (op0 != registers[15]) return (arm7 << 1) + 1;
    cpsr |= (*op0 & !arm7) << 5;
    flushPipeline();
    return 5;
}

FORCE_INLINE int Interpreter::strPt(uint32_t opcode, uint32_t op2) // STR Rd,[Rn],op2
{
    // Word store, post-adjust
    // When used as Rd, the program counter is read with +4
    uint32_t op0 = *registers[(opcode >> 12) & 0xF] + (((opcode & 0xF000) == 0xF000) << 2);
    uint32_t *op1 = registers[(opcode >> 16) & 0xF];
    core->memory.write<uint32_t>(arm7, *op1, op0);
    *op1 += op2;
    return arm7 + 1;
}

FORCE_INLINE int Interpreter::ldrdPt(uint32_t opcode, uint32_t op2) // LDRD Rd,[Rn],op2
{
    // Double word load, post-adjust
    uint8_t op0 = (opcode >> 12) & 0xF;
    if (arm7 || op0 == 15) return 1; // ARM9 exclusive
    uint32_t *op1 = registers[(opcode >> 16) & 0xF];
    uint32_t address = (*op1 += op2) - op2;
    *registers[op0] = core->memory.read<uint32_t>(arm7, address);
    *registers[op0 + 1] = core->memory.read<uint32_t>(arm7, address + 4);
    return 2;
}

FORCE_INLINE int Interpreter::strdPt(uint32_t opcode, uint32_t op2) // STRD Rd,[Rn],op2
{
    // Double word store, post-adjust
    uint8_t op0 = (opcode >> 12) & 0xF;
    if (arm7 || op0 == 15) return 1;
    uint32_t *op1 = registers[(opcode >> 16) & 0xF];
    core->memory.write<uint32_t>(arm7, *op1, *registers[op0]);
    core->memory.write<uint32_t>(arm7, *op1 + 4, *registers[op0 + 1]);
    *op1 += op2;
    return 2;
}

int Interpreter::swpb(uint32_t opcode) // SWPB Rd,Rm,[Rn]
{
    // Swap byte
    uint32_t *op0 = registers[(opcode >> 12) & 0xF];
    uint32_t op1 = *registers[opcode & 0xF];
    uint32_t op2 = *registers[(opcode >> 16) & 0xF];
    *op0 = core->memory.read<uint8_t>(arm7, op2);
    core->memory.write<uint8_t>(arm7, op2, op1);
    return (arm7 << 1) + 2;
}

int Interpreter::swp(uint32_t opcode) // SWP Rd,Rm,[Rn]
{
    // Swap word
    uint32_t *op0 = registers[(opcode >> 12) & 0xF];
    uint32_t op1 = *registers[opcode & 0xF];
    uint32_t op2 = *registers[(opcode >> 16) & 0xF];
    *op0 = core->memory.read<uint32_t>(arm7, op2);
    core->memory.write<uint32_t>(arm7, op2, op1);

    // Rotate misaligned reads
    if (op2 & 0x3)
    {
        uint8_t shift = (op2 & 0x3) << 3;
        *op0 = (*op0 << (32 - shift)) | (*op0 >> shift);
    }
    return (arm7 << 1) + 2;
}

int Interpreter::ldmda(uint32_t opcode) // LDMDA Rn, <Rlist>
{
    // Block load, post-decrement without writeback
    uint8_t m = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[(opcode >> 16) & 0xF] - (m << 2);
    for (int i = 0; i < 16; i++)
    {
        if (~opcode & BIT(i)) continue;
        *registers[i] = core->memory.read<uint32_t>(arm7, op0 += 4);
    }

    // Handle pipelining and THUMB switching
    if (~opcode & BIT(15)) return m + (arm7 ? 2 : (m < 2));
    cpsr |= (*registers[15] & !arm7) << 5;
    flushPipeline();
    return m + 4;
}

int Interpreter::stmda(uint32_t opcode) // STMDA Rn, <Rlist>
{
    // Block store, post-decrement without writeback
    uint8_t m = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[(opcode >> 16) & 0xF] - (m << 2);
    for (int i = 0; i < 16; i++)
    {
        if (~opcode & BIT(i)) continue;
        core->memory.write<uint32_t>(arm7, op0 += 4, *registers[i]);
    }
    return m + (arm7 || m < 2);
}

int Interpreter::ldmia(uint32_t opcode) // LDMIA Rn, <Rlist>
{
    // Block load, post-increment without writeback
    uint8_t m = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[(opcode >> 16) & 0xF];
    for (int i = 0; i < 16; i++)
    {
        if (~opcode & BIT(i)) continue;
        *registers[i] = core->memory.read<uint32_t>(arm7, op0);
        op0 += 4;
    }

    // Handle pipelining and THUMB switching
    if (~opcode & BIT(15)) return m + (arm7 ? 2 : (m < 2));
    cpsr |= (*registers[15] & !arm7) << 5;
    flushPipeline();
    return m + 4;
}

int Interpreter::stmia(uint32_t opcode) // STMIA Rn, <Rlist>
{
    // Block store, post-increment without writeback
    uint8_t m = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[(opcode >> 16) & 0xF];
    for (int i = 0; i < 16; i++)
    {
        if (~opcode & BIT(i)) continue;
        core->memory.write<uint32_t>(arm7, op0, *registers[i]);
        op0 += 4;
    }
    return m + (arm7 || m < 2);
}

int Interpreter::ldmdb(uint32_t opcode) // LDMDB Rn, <Rlist>
{
    // Block load, pre-decrement without writeback
    uint8_t m = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[(opcode >> 16) & 0xF] - (m << 2);
    for (int i = 0; i < 16; i++)
    {
        if (~opcode & BIT(i)) continue;
        *registers[i] = core->memory.read<uint32_t>(arm7, op0);
        op0 += 4;
    }

    // Handle pipelining and THUMB switching
    if (~opcode & BIT(15)) return m + (arm7 ? 2 : (m < 2));
    cpsr |= (*registers[15] & !arm7) << 5;
    flushPipeline();
    return m + 4;
}

int Interpreter::stmdb(uint32_t opcode) // STMDB Rn, <Rlist>
{
    // Block store, pre-decrement without writeback
    uint8_t m = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[(opcode >> 16) & 0xF] - (m << 2);
    for (int i = 0; i < 16; i++)
    {
        if (~opcode & BIT(i)) continue;
        core->memory.write<uint32_t>(arm7, op0, *registers[i]);
        op0 += 4;
    }
    return m + (arm7 || m < 2);
}

int Interpreter::ldmib(uint32_t opcode) // LDMIB Rn, <Rlist>
{
    // Block load, pre-increment without writeback
    uint8_t m = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[(opcode >> 16) & 0xF];
    for (int i = 0; i < 16; i++)
    {
        if (~opcode & BIT(i)) continue;
        *registers[i] = core->memory.read<uint32_t>(arm7, op0 += 4);
    }

    // Handle pipelining and THUMB switching
    if (~opcode & BIT(15)) return m + (arm7 ? 2 : (m < 2));
    cpsr |= (*registers[15] & !arm7) << 5;
    flushPipeline();
    return m + 4;
}

int Interpreter::stmib(uint32_t opcode) // STMIB Rn, <Rlist>
{
    // Block store, pre-increment without writeback
    uint8_t m = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[(opcode >> 16) & 0xF];
    for (int i = 0; i < 16; i++)
    {
        if (~opcode & BIT(i)) continue;
        core->memory.write<uint32_t>(arm7, op0 += 4, *registers[i]);
    }
    return m + (arm7 || m < 2);
}

int Interpreter::ldmdaW(uint32_t opcode) // LDMDA Rn!, <Rlist>
{
    // Block load, post-decrement with writeback
    uint8_t m = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint8_t op0 = (opcode >> 16) & 0xF;
    uint32_t address = (*registers[op0] -= (m << 2));
    for (int i = 0; i < 16; i++)
    {
        if (~opcode & BIT(i)) continue;
        *registers[i] = core->memory.read<uint32_t>(arm7, address += 4);
    }

    // Load the writeback value if it's not last or is the only listed register on ARM9
    if (!arm7 && ((opcode & 0xFFFF & ~(BIT(op0 + 1) - 1)) || (opcode & 0xFFFF) == BIT(op0)))
        *registers[op0] = address - (m << 2);

    // Handle pipelining and THUMB switching
    if (~opcode & BIT(15)) return m + (arm7 ? 2 : (m < 2));
    cpsr |= (*registers[15] & !arm7) << 5;
    flushPipeline();
    return m + 4;
}

int Interpreter::stmdaW(uint32_t opcode) // STMDA Rn!, <Rlist>
{
    // Store the writeback value if it's not the first listed register on ARM7
    uint8_t m = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint8_t op0 = (opcode >> 16) & 0xF;
    uint32_t address = *registers[op0] - (m << 2);
    if (arm7 && (opcode & (BIT(op0 + 1) - 1)) > BIT(op0))
        *registers[op0] = address;

    // Block store, post-decrement with writeback
    for (int i = 0; i < 16; i++)
    {
        if (~opcode & BIT(i)) continue;
        core->memory.write<uint32_t>(arm7, address += 4, *registers[i]);
    }
    *registers[op0] = address - (m << 2);
    return m + (arm7 || m < 2);
}

int Interpreter::ldmiaW(uint32_t opcode) // LDMIA Rn!, <Rlist>
{
    // Block load, post-increment with writeback
    uint8_t m = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint8_t op0 = (opcode >> 16) & 0xF;
    uint32_t address = (*registers[op0] += (m << 2)) - (m << 2);
    for (int i = 0; i < 16; i++)
    {
        if (~opcode & BIT(i)) continue;
        *registers[i] = core->memory.read<uint32_t>(arm7, address);
        address += 4;
    }

    // Load the writeback value if it's not last or is the only listed register on ARM9
    if (!arm7 && ((opcode & 0xFFFF & ~(BIT(op0 + 1) - 1)) || (opcode & 0xFFFF) == BIT(op0)))
        *registers[op0] = address;

    // Handle pipelining and THUMB switching
    if (~opcode & BIT(15)) return m + (arm7 ? 2 : (m < 2));
    cpsr |= (*registers[15] & !arm7) << 5;
    flushPipeline();
    return m + 4;
}

int Interpreter::stmiaW(uint32_t opcode) // STMIA Rn!, <Rlist>
{
    // Store the writeback value if it's not the first listed register on ARM7
    uint8_t m = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint8_t op0 = (opcode >> 16) & 0xF;
    uint32_t address = *registers[op0];
    if (arm7 && (opcode & (BIT(op0 + 1) - 1)) > BIT(op0))
        *registers[op0] = address + (m << 2);

    // Block store, post-increment with writeback
    for (int i = 0; i < 16; i++)
    {
        if (~opcode & BIT(i)) continue;
        core->memory.write<uint32_t>(arm7, address, *registers[i]);
        address += 4;
    }
    *registers[op0] = address;
    return m + (arm7 || m < 2);
}

int Interpreter::ldmdbW(uint32_t opcode) // LDMDB Rn!, <Rlist>
{
    // Block load, pre-decrement with writeback
    uint8_t m = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint8_t op0 = (opcode >> 16) & 0xF;
    uint32_t address = (*registers[op0] -= (m << 2));
    for (int i = 0; i < 16; i++)
    {
        if (~opcode & BIT(i)) continue;
        *registers[i] = core->memory.read<uint32_t>(arm7, address);
        address += 4;
    }

    // Load the writeback value if it's not last or is the only listed register on ARM9
    if (!arm7 && ((opcode & 0xFFFF & ~(BIT(op0 + 1) - 1)) || (opcode & 0xFFFF) == BIT(op0)))
        *registers[op0] = address - (m << 2);

    // Handle pipelining and THUMB switching
    if (~opcode & BIT(15)) return m + (arm7 ? 2 : (m < 2));
    cpsr |= (*registers[15] & !arm7) << 5;
    flushPipeline();
    return m + 4;
}

int Interpreter::stmdbW(uint32_t opcode) // STMDB Rn!, <Rlist>
{
    // Store the writeback value if it's not the first listed register on ARM7
    uint8_t m = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint8_t op0 = (opcode >> 16) & 0xF;
    uint32_t address = *registers[op0] - (m << 2);
    if (arm7 && (opcode & (BIT(op0 + 1) - 1)) > BIT(op0))
        *registers[op0] = address;

    // Block store, pre-decrement with writeback
    for (int i = 0; i < 16; i++)
    {
        if (~opcode & BIT(i)) continue;
        core->memory.write<uint32_t>(arm7, address, *registers[i]);
        address += 4;
    }
    *registers[op0] = address - (m << 2);
    return m + (arm7 || m < 2);
}

int Interpreter::ldmibW(uint32_t opcode) // LDMIB Rn!, <Rlist>
{
    // Block load, pre-increment with writeback
    uint8_t m = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint8_t op0 = (opcode >> 16) & 0xF;
    uint32_t address = (*registers[op0] += (m << 2)) - (m << 2);
    for (int i = 0; i < 16; i++)
    {
        if (~opcode & BIT(i)) continue;
        *registers[i] = core->memory.read<uint32_t>(arm7, address += 4);
    }

    // Load the writeback value if it's not last or is the only listed register on ARM9
    if (!arm7 && ((opcode & 0xFFFF & ~(BIT(op0 + 1) - 1)) || (opcode & 0xFFFF) == BIT(op0)))
        *registers[op0] = address;

    // Handle pipelining and THUMB switching
    if (~opcode & BIT(15)) return m + (arm7 ? 2 : (m < 2));
    cpsr |= (*registers[15] & !arm7) << 5;
    flushPipeline();
    return m + 4;
}

int Interpreter::stmibW(uint32_t opcode) // STMIB Rn!, <Rlist>
{
    // Store the writeback value if it's not the first listed register on ARM7
    uint8_t m = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint8_t op0 = (opcode >> 16) & 0xF;
    uint32_t address = *registers[op0];
    if (arm7 && (opcode & (BIT(op0 + 1) - 1)) > BIT(op0))
        *registers[op0] = address + (m << 2);

    // Block store, pre-increment with writeback
    for (int i = 0; i < 16; i++)
    {
        if (~opcode & BIT(i)) continue;
        core->memory.write<uint32_t>(arm7, address += 4, *registers[i]);
    }
    *registers[op0] = address;
    return m + (arm7 || m < 2);
}

int Interpreter::ldmdaU(uint32_t opcode) // LDMDA Rn, <Rlist>^
{
    // User block load, post-decrement without writeback; normal registers if branching
    uint8_t m = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[(opcode >> 16) & 0xF] - (m << 2);
    uint32_t **regs = &registers[(~opcode & BIT(15)) >> 11];
    for (int i = 0; i < 16; i++)
    {
        if (~opcode & BIT(i)) continue;
        *regs[i] = core->memory.read<uint32_t>(arm7, op0 += 4);
    }

    // Handle pipelining and mode/THUMB switching
    if (~opcode & BIT(15)) return m + (arm7 ? 2 : (m < 2));
    if (spsr) setCpsr(*spsr);
    cpsr |= (*registers[15] & !arm7) << 5;
    flushPipeline();
    return m + 4;
}

int Interpreter::stmdaU(uint32_t opcode) // STMDA Rn, <Rlist>^
{
    // User block store, post-decrement without writeback
    uint8_t m = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[(opcode >> 16) & 0xF] - (m << 2);
    for (int i = 0; i < 16; i++)
    {
        if (~opcode & BIT(i)) continue;
        core->memory.write<uint32_t>(arm7, op0 += 4, registersUsr[i]);
    }
    return m + (arm7 || m < 2);
}

int Interpreter::ldmiaU(uint32_t opcode) // LDMIA Rn, <Rlist>^
{
    // User block load, post-increment without writeback; normal registers if branching
    uint8_t m = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[(opcode >> 16) & 0xF];
    uint32_t **regs = &registers[(~opcode & BIT(15)) >> 11];
    for (int i = 0; i < 16; i++)
    {
        if (~opcode & BIT(i)) continue;
        *regs[i] = core->memory.read<uint32_t>(arm7, op0);
        op0 += 4;
    }

    // Handle pipelining and mode/THUMB switching
    if (~opcode & BIT(15)) return m + (arm7 ? 2 : (m < 2));
    if (spsr) setCpsr(*spsr);
    cpsr |= (*registers[15] & !arm7) << 5;
    flushPipeline();
    return m + 4;
}

int Interpreter::stmiaU(uint32_t opcode) // STMIA Rn, <Rlist>^
{
    // User block store, post-increment without writeback
    uint8_t m = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[(opcode >> 16) & 0xF];
    for (int i = 0; i < 16; i++)
    {
        if (~opcode & BIT(i)) continue;
        core->memory.write<uint32_t>(arm7, op0, registersUsr[i]);
        op0 += 4;
    }
    return m + (arm7 || m < 2);
}

int Interpreter::ldmdbU(uint32_t opcode) // LDMDB Rn, <Rlist>^
{
    // User block load, pre-decrement without writeback; normal registers if branching
    uint8_t m = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[(opcode >> 16) & 0xF] - (m << 2);
    uint32_t **regs = &registers[(~opcode & BIT(15)) >> 11];
    for (int i = 0; i < 16; i++)
    {
        if (~opcode & BIT(i)) continue;
        *regs[i] = core->memory.read<uint32_t>(arm7, op0);
        op0 += 4;
    }

    // Handle pipelining and mode/THUMB switching
    if (~opcode & BIT(15)) return m + (arm7 ? 2 : (m < 2));
    if (spsr) setCpsr(*spsr);
    cpsr |= (*registers[15] & !arm7) << 5;
    flushPipeline();
    return m + 4;
}

int Interpreter::stmdbU(uint32_t opcode) // STMDB Rn, <Rlist>^
{
    // User block store, pre-decrement without writeback
    uint8_t m = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[(opcode >> 16) & 0xF] - (m << 2);
    for (int i = 0; i < 16; i++)
    {
        if (~opcode & BIT(i)) continue;
        core->memory.write<uint32_t>(arm7, op0, registersUsr[i]);
        op0 += 4;
    }
    return m + (arm7 || m < 2);
}

int Interpreter::ldmibU(uint32_t opcode) // LDMIB Rn, <Rlist>^
{
    // User block load, pre-increment without writeback; normal registers if branching
    uint8_t m = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[(opcode >> 16) & 0xF];
    uint32_t **regs = &registers[(~opcode & BIT(15)) >> 11];
    for (int i = 0; i < 16; i++)
    {
        if (~opcode & BIT(i)) continue;
        *regs[i] = core->memory.read<uint32_t>(arm7, op0 += 4);
    }

    // Handle pipelining and mode/THUMB switching
    if (~opcode & BIT(15)) return m + (arm7 ? 2 : (m < 2));
    if (spsr) setCpsr(*spsr);
    cpsr |= (*registers[15] & !arm7) << 5;
    flushPipeline();
    return m + 4;
}

int Interpreter::stmibU(uint32_t opcode) // STMIB Rn, <Rlist>^
{
    // User block store, pre-increment without writeback
    uint8_t m = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint32_t op0 = *registers[(opcode >> 16) & 0xF];
    for (int i = 0; i < 16; i++)
    {
        if (~opcode & BIT(i)) continue;
        core->memory.write<uint32_t>(arm7, op0 += 4, registersUsr[i]);
    }
    return m + (arm7 || m < 2);
}

int Interpreter::ldmdaUW(uint32_t opcode) // LDMDA Rn!, <Rlist>^
{
    // User block load, post-decrement with writeback; normal registers if branching
    uint8_t m = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint8_t op0 = (opcode >> 16) & 0xF;
    uint32_t address = (*registers[op0] -= (m << 2));
    uint32_t **regs = &registers[(~opcode & BIT(15)) >> 11];
    for (int i = 0; i < 16; i++)
    {
        if (~opcode & BIT(i)) continue;
        *regs[i] = core->memory.read<uint32_t>(arm7, address += 4);
    }

    // Load the writeback value if it's not last or is the only listed register on ARM9
    if (!arm7 && ((opcode & 0xFFFF & ~(BIT(op0 + 1) - 1)) || (opcode & 0xFFFF) == BIT(op0)))
        *registers[op0] = address - (m << 2);

    // Handle pipelining and mode/THUMB switching
    if (~opcode & BIT(15)) return m + (arm7 ? 2 : (m < 2));
    if (spsr) setCpsr(*spsr);
    cpsr |= (*registers[15] & !arm7) << 5;
    flushPipeline();
    return m + 4;
}

int Interpreter::stmdaUW(uint32_t opcode) // STMDA Rn!, <Rlist>^
{
    // Store the writeback value if it's not the first listed register on ARM7
    uint8_t m = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint8_t op0 = (opcode >> 16) & 0xF;
    uint32_t address = *registers[op0] - (m << 2);
    if (arm7 && (opcode & (BIT(op0 + 1) - 1)) > BIT(op0))
        *registers[op0] = address;

    // User block store, post-decrement with writeback
    for (int i = 0; i < 16; i++)
    {
        if (~opcode & BIT(i)) continue;
        core->memory.write<uint32_t>(arm7, address += 4, registersUsr[i]);
    }
    *registers[op0] = address - (m << 2);
    return m + (arm7 || m < 2);
}

int Interpreter::ldmiaUW(uint32_t opcode) // LDMIA Rn!, <Rlist>^
{
    // User block load, post-increment with writeback; normal registers if branching
    uint8_t m = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint8_t op0 = (opcode >> 16) & 0xF;
    uint32_t address = (*registers[op0] += (m << 2)) - (m << 2);
    uint32_t **regs = &registers[(~opcode & BIT(15)) >> 11];
    for (int i = 0; i < 16; i++)
    {
        if (~opcode & BIT(i)) continue;
        *regs[i] = core->memory.read<uint32_t>(arm7, address);
        address += 4;
    }

    // Load the writeback value if it's not last or is the only listed register on ARM9
    if (!arm7 && ((opcode & 0xFFFF & ~(BIT(op0 + 1) - 1)) || (opcode & 0xFFFF) == BIT(op0)))
        *registers[op0] = address;

    // Handle pipelining and mode/THUMB switching
    if (~opcode & BIT(15)) return m + (arm7 ? 2 : (m < 2));
    if (spsr) setCpsr(*spsr);
    cpsr |= (*registers[15] & !arm7) << 5;
    flushPipeline();
    return m + 4;
}

int Interpreter::stmiaUW(uint32_t opcode) // STMIA Rn!, <Rlist>^
{
    // Store the writeback value if it's not the first listed register on ARM7
    uint8_t m = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint8_t op0 = (opcode >> 16) & 0xF;
    uint32_t address = *registers[op0];
    if (arm7 && (opcode & (BIT(op0 + 1) - 1)) > BIT(op0))
        *registers[op0] = address + (m << 2);

    // User block store, post-increment with writeback
    for (int i = 0; i < 16; i++)
    {
        if (~opcode & BIT(i)) continue;
        core->memory.write<uint32_t>(arm7, address, registersUsr[i]);
        address += 4;
    }
    *registers[op0] = address;
    return m + (arm7 || m < 2);
}

int Interpreter::ldmdbUW(uint32_t opcode) // LDMDB Rn!, <Rlist>^
{
    // User block load, pre-decrement with writeback; normal registers if branching
    uint8_t m = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint8_t op0 = (opcode >> 16) & 0xF;
    uint32_t address = (*registers[op0] -= (m << 2));
    uint32_t **regs = &registers[(~opcode & BIT(15)) >> 11];
    for (int i = 0; i < 16; i++)
    {
        if (~opcode & BIT(i)) continue;
        *regs[i] = core->memory.read<uint32_t>(arm7, address);
        address += 4;
    }

    // Load the writeback value if it's not last or is the only listed register on ARM9
    if (!arm7 && ((opcode & 0xFFFF & ~(BIT(op0 + 1) - 1)) || (opcode & 0xFFFF) == BIT(op0)))
        *registers[op0] = address - (m << 2);

    // Handle pipelining and mode/THUMB switching
    if (~opcode & BIT(15)) return m + (arm7 ? 2 : (m < 2));
    if (spsr) setCpsr(*spsr);
    cpsr |= (*registers[15] & !arm7) << 5;
    flushPipeline();
    return m + 4;
}

int Interpreter::stmdbUW(uint32_t opcode) // STMDB Rn!, <Rlist>^
{
    // Store the writeback value if it's not the first listed register on ARM7
    uint8_t m = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint8_t op0 = (opcode >> 16) & 0xF;
    uint32_t address = *registers[op0] - (m << 2);
    if (arm7 && (opcode & (BIT(op0 + 1) - 1)) > BIT(op0))
        *registers[op0] = address;

    // User block store, pre-decrement with writeback
    for (int i = 0; i < 16; i++)
    {
        if (~opcode & BIT(i)) continue;
        core->memory.write<uint32_t>(arm7, address, registersUsr[i]);
        address += 4;
    }
    *registers[op0] = address - (m << 2);
    return m + (arm7 || m < 2);
}

int Interpreter::ldmibUW(uint32_t opcode) // LDMIB Rn!, <Rlist>^
{
    // User block load, pre-increment with writeback; normal registers if branching
    uint8_t m = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint8_t op0 = (opcode >> 16) & 0xF;
    uint32_t address = (*registers[op0] += (m << 2)) - (m << 2);
    uint32_t **regs = &registers[(~opcode & BIT(15)) >> 11];
    for (int i = 0; i < 16; i++)
    {
        if (~opcode & BIT(i)) continue;
        *regs[i] = core->memory.read<uint32_t>(arm7, address += 4);
    }

    // Load the writeback value if it's not last or is the only listed register on ARM9
    if (!arm7 && ((opcode & 0xFFFF & ~(BIT(op0 + 1) - 1)) || (opcode & 0xFFFF) == BIT(op0)))
        *registers[op0] = address;

    // Handle pipelining and mode/THUMB switching
    if (~opcode & BIT(15)) return m + (arm7 ? 2 : (m < 2));
    if (spsr) setCpsr(*spsr);
    cpsr |= (*registers[15] & !arm7) << 5;
    flushPipeline();
    return m + 4;
}

int Interpreter::stmibUW(uint32_t opcode) // STMIB Rn!, <Rlist>^
{
    // Store the writeback value if it's not the first listed register on ARM7
    uint8_t m = bitCount[opcode & 0xFF] + bitCount[(opcode >> 8) & 0xFF];
    uint8_t op0 = (opcode >> 16) & 0xF;
    uint32_t address = *registers[op0];
    if (arm7 && (opcode & (BIT(op0 + 1) - 1)) > BIT(op0))
        *registers[op0] = address + (m << 2);

    // User block store, pre-increment with writeback
    for (int i = 0; i < 16; i++)
    {
        if (~opcode & BIT(i)) continue;
        core->memory.write<uint32_t>(arm7, address += 4, registersUsr[i]);
    }
    *registers[op0] = address;
    return m + (arm7 || m < 2);
}

int Interpreter::msrRc(uint32_t opcode) // MSR CPSR,Rm
{
    // Write the first 8 bits of the status flags, only changing the CPU mode when not in user mode
    uint32_t op1 = *registers[opcode & 0xF];
    if (opcode & BIT(16))
    {
        uint8_t mask = ((cpsr & 0x1F) == 0x10) ? 0xE0 : 0xFF;
        setCpsr((cpsr & ~mask) | (op1 & mask));
    }

    // Write the remaining 8-bit blocks of the status flags
    for (int i = 1; i < 4; i++)
    {
        if (opcode & BIT(16 + i))
            cpsr = (cpsr & ~(0xFF << (i << 3))) | (op1 & (0xFF << (i << 3)));
    }
    return 1;
}

int Interpreter::msrRs(uint32_t opcode) // MSR SPSR,Rm
{
    // Write the saved status flags in 8-bit blocks
    if (!spsr) return 1;
    uint32_t op1 = *registers[opcode & 0xF];
    for (int i = 0; i < 4; i++)
    {
        if (opcode & BIT(16 + i))
            *spsr = (*spsr & ~(0xFF << (i << 3))) | (op1 & (0xFF << (i << 3)));
    }
    return 1;
}

int Interpreter::msrIc(uint32_t opcode) // MSR CPSR,#i
{
    // Rotate the immediate value
    uint32_t value = opcode & 0xFF;
    uint8_t shift = (opcode >> 7) & 0x1E;
    uint32_t op1 = (value << (32 - shift)) | (value >> shift);

    // Write the first 8 bits of the status flags, only changing the CPU mode when not in user mode
    if (opcode & BIT(16))
    {
        uint8_t mask = ((cpsr & 0x1F) == 0x10) ? 0xE0 : 0xFF;
        setCpsr((cpsr & ~mask) | (op1 & mask));
    }

    // Write the remaining 8-bit blocks of the status flags
    for (int i = 1; i < 4; i++)
    {
        if (opcode & BIT(16 + i))
            cpsr = (cpsr & ~(0xFF << (i << 3))) | (op1 & (0xFF << (i << 3)));
    }
    return 1;
}

int Interpreter::msrIs(uint32_t opcode) // MSR SPSR,#i
{
    // Rotate the immediate value
    if (!spsr) return 1;
    uint32_t value = opcode & 0xFF;
    uint8_t shift = (opcode >> 7) & 0x1E;
    uint32_t op1 = (value << (32 - shift)) | (value >> shift);

    // Write the saved status flags in 8-bit blocks
    for (int i = 0; i < 4; i++)
    {
        if (opcode & BIT(16 + i))
            *spsr = (*spsr & ~(0xFF << (i << 3))) | (op1 & (0xFF << (i << 3)));
    }
    return 1;
}

int Interpreter::mrsRc(uint32_t opcode) // MRS Rd,CPSR
{
    // Copy the status flags to a register
    uint32_t *op0 = registers[(opcode >> 12) & 0xF];
    *op0 = cpsr;
    return 2 - arm7;
}

int Interpreter::mrsRs(uint32_t opcode) // MRS Rd,SPSR
{
    // Copy the saved status flags to a register
    uint32_t *op0 = registers[(opcode >> 12) & 0xF];
    if (spsr) *op0 = *spsr;
    return 2 - arm7;
}

int Interpreter::mrc(uint32_t opcode) // MRC Pn,<cpopc>,Rd,Cn,Cm,<cp>
{
    // Read from a CP15 register
    if (arm7) return 1; // ARM9 exclusive
    uint32_t *op2 = registers[(opcode >> 12) & 0xF];
    uint8_t op3 = (opcode >> 16) & 0xF;
    uint8_t op4 = opcode & 0xF;
    uint8_t op5 = (opcode >> 5) & 0x7;
    *op2 = core->cp15.read(op3, op4, op5);
    return 1;
}

int Interpreter::mcr(uint32_t opcode) // MCR Pn,<cpopc>,Rd,Cn,Cm,<cp>
{
    // Write to a CP15 register
    if (arm7) return 1; // ARM9 exclusive
    uint32_t op2 = *registers[(opcode >> 12) & 0xF];
    uint8_t op3 = (opcode >> 16) & 0xF;
    uint8_t op4 = opcode & 0xF;
    uint8_t op5 = (opcode >> 5) & 0x7;
    core->cp15.write(op3, op4, op5, op2);
    return 1;
}

int Interpreter::ldrsbRegT(uint16_t opcode) // LDRSB Rd,[Rb,Ro]
{
    // Signed byte load, pre-adjust without writeback (THUMB)
    uint32_t *op0 = registers[opcode & 0x7];
    uint32_t op1 = *registers[(opcode >> 3) & 0x7];
    uint32_t op2 = *registers[(opcode >> 6) & 0x7];
    *op0 = (int8_t)core->memory.read<uint8_t>(arm7, op1 + op2);
    return (arm7 << 1) + 1;
}

int Interpreter::ldrshRegT(uint16_t opcode) // LDRSH Rd,[Rb,Ro]
{
    // Signed half-word load, pre-adjust without writeback (THUMB)
    uint32_t *op0 = registers[opcode & 0x7];
    uint32_t op1 = *registers[(opcode >> 3) & 0x7];
    uint32_t op2 = *registers[(opcode >> 6) & 0x7];
    *op0 = (int16_t)core->memory.read<uint16_t>(arm7, op1 += op2);

    // Shift misaligned reads on ARM7
    if (op1 & arm7)
        *op0 = (int16_t)*op0 >> 8;
    return (arm7 << 1) + 1;
}

int Interpreter::ldrbRegT(uint16_t opcode) // LDRB Rd,[Rb,Ro]
{
    // Byte load, pre-adjust without writeback (THUMB)
    uint32_t *op0 = registers[opcode & 0x7];
    uint32_t op1 = *registers[(opcode >> 3) & 0x7];
    uint32_t op2 = *registers[(opcode >> 6) & 0x7];
    *op0 = core->memory.read<uint8_t>(arm7, op1 + op2);
    return (arm7 << 1) + 1;
}

int Interpreter::strbRegT(uint16_t opcode) // STRB Rd,[Rb,Ro]
{
    // Byte write, pre-adjust without writeback (THUMB)
    uint32_t op0 = *registers[opcode & 0x7];
    uint32_t op1 = *registers[(opcode >> 3) & 0x7];
    uint32_t op2 = *registers[(opcode >> 6) & 0x7];
    core->memory.write<uint8_t>(arm7, op1 + op2, op0);
    return arm7 + 1;
}

int Interpreter::ldrhRegT(uint16_t opcode) // LDRH Rd,[Rb,Ro]
{
    // Half-word load, pre-adjust without writeback (THUMB)
    uint32_t *op0 = registers[opcode & 0x7];
    uint32_t op1 = *registers[(opcode >> 3) & 0x7];
    uint32_t op2 = *registers[(opcode >> 6) & 0x7];
    *op0 = core->memory.read<uint16_t>(arm7, op1 += op2);

    // Rotate misaligned reads on ARM7
    if (op1 & arm7)
        *op0 = (*op0 << 24) | (*op0 >> 8);
    return (arm7 << 1) + 1;
}

int Interpreter::strhRegT(uint16_t opcode) // STRH Rd,[Rb,Ro]
{
    // Half-word write, pre-adjust without writeback (THUMB)
    uint32_t op0 = *registers[opcode & 0x7];
    uint32_t op1 = *registers[(opcode >> 3) & 0x7];
    uint32_t op2 = *registers[(opcode >> 6) & 0x7];
    core->memory.write<uint16_t>(arm7, op1 + op2, op0);
    return arm7 + 1;
}

int Interpreter::ldrRegT(uint16_t opcode) // LDR Rd,[Rb,Ro]
{
    // Word load, pre-adjust without writeback (THUMB)
    uint32_t *op0 = registers[opcode & 0x7];
    uint32_t op1 = *registers[(opcode >> 3) & 0x7];
    uint32_t op2 = *registers[(opcode >> 6) & 0x7];
    *op0 = core->memory.read<uint32_t>(arm7, op1 += op2);

    // Rotate misaligned reads
    if (op1 & 0x3)
    {
        uint8_t shift = (op1 & 0x3) << 3;
        *op0 = (*op0 << (32 - shift)) | (*op0 >> shift);
    }
    return (arm7 << 1) + 1;
}

int Interpreter::strRegT(uint16_t opcode) // STR Rd,[Rb,Ro]
{
    // Word write, pre-adjust without writeback (THUMB)
    uint32_t op0 = *registers[opcode & 0x7];
    uint32_t op1 = *registers[(opcode >> 3) & 0x7];
    uint32_t op2 = *registers[(opcode >> 6) & 0x7];
    core->memory.write<uint32_t>(arm7, op1 + op2, op0);
    return arm7 + 1;
}

int Interpreter::ldrbImm5T(uint16_t opcode) // LDRB Rd,[Rb,#i]
{
    // Byte load, pre-adjust without writeback (THUMB)
    uint32_t *op0 = registers[opcode & 0x7];
    uint32_t op1 = *registers[(opcode >> 3) & 0x7];
    uint32_t op2 = (opcode & 0x07C0) >> 6;
    *op0 = core->memory.read<uint8_t>(arm7, op1 + op2);
    return (arm7 << 1) + 1;
}

int Interpreter::strbImm5T(uint16_t opcode) // STRB Rd,[Rb,#i]
{
    // Byte store, pre-adjust without writeback (THUMB)
    uint32_t op0 = *registers[opcode & 0x7];
    uint32_t op1 = *registers[(opcode >> 3) & 0x7];
    uint32_t op2 = (opcode & 0x07C0) >> 6;
    core->memory.write<uint8_t>(arm7, op1 + op2, op0);
    return arm7 + 1;
}

int Interpreter::ldrhImm5T(uint16_t opcode) // LDRH Rd,[Rb,#i]
{
    // Half-word load, pre-adjust without writeback (THUMB)
    uint32_t *op0 = registers[opcode & 0x7];
    uint32_t op1 = *registers[(opcode >> 3) & 0x7];
    uint32_t op2 = (opcode >> 5) & 0x3E;
    *op0 = core->memory.read<uint16_t>(arm7, op1 += op2);

    // Rotate misaligned reads on ARM7
    if (op1 & arm7)
        *op0 = (*op0 << 24) | (*op0 >> 8);
    return (arm7 << 1) + 1;
}

int Interpreter::strhImm5T(uint16_t opcode) // STRH Rd,[Rb,#i]
{
    // Half-word store, pre-adjust without writeback (THUMB)
    uint32_t op0 = *registers[opcode & 0x7];
    uint32_t op1 = *registers[(opcode >> 3) & 0x7];
    uint32_t op2 = (opcode >> 5) & 0x3E;
    core->memory.write<uint16_t>(arm7, op1 + op2, op0);
    return arm7 + 1;
}

int Interpreter::ldrImm5T(uint16_t opcode) // LDR Rd,[Rb,#i]
{
    // Word load, pre-adjust without writeback (THUMB)
    uint32_t *op0 = registers[opcode & 0x7];
    uint32_t op1 = *registers[(opcode >> 3) & 0x7];
    uint32_t op2 = (opcode >> 4) & 0x7C;
    *op0 = core->memory.read<uint32_t>(arm7, op1 += op2);

    // Rotate misaligned reads
    if (op1 & 0x3)
    {
        uint8_t shift = (op1 & 0x3) << 3;
        *op0 = (*op0 << (32 - shift)) | (*op0 >> shift);
    }
    return (arm7 << 1) + 1;
}

int Interpreter::strImm5T(uint16_t opcode) // STR Rd,[Rb,#i]
{
    // Word store, pre-adjust without writeback (THUMB)
    uint32_t op0 = *registers[opcode & 0x7];
    uint32_t op1 = *registers[(opcode >> 3) & 0x7];
    uint32_t op2 = (opcode >> 4) & 0x7C;
    core->memory.write<uint32_t>(arm7, op1 + op2, op0);
    return arm7 + 1;
}

int Interpreter::ldrPcT(uint16_t opcode) // LDR Rd,[PC,#i]
{
    // PC-relative word load, pre-adjust without writeback (THUMB)
    uint32_t *op0 = registers[(opcode >> 8) & 0x7];
    uint32_t op1 = *registers[15] & ~0x3;
    uint32_t op2 = (opcode & 0xFF) << 2;
    *op0 = core->memory.read<uint32_t>(arm7, op1 += op2);

    // Rotate misaligned reads
    if (op1 & 0x3)
    {
        uint8_t shift = (op1 & 0x3) << 3;
        *op0 = (*op0 << (32 - shift)) | (*op0 >> shift);
    }
    return (arm7 << 1) + 1;
}

int Interpreter::ldrSpT(uint16_t opcode) // LDR Rd,[SP,#i]
{
    // SP-relative word load, pre-adjust without writeback (THUMB)
    uint32_t *op0 = registers[(opcode >> 8) & 0x7];
    uint32_t op1 = *registers[13];
    uint32_t op2 = (opcode & 0xFF) << 2;
    *op0 = core->memory.read<uint32_t>(arm7, op1 += op2);

    // Rotate misaligned reads
    if (op1 & 0x3)
    {
        uint8_t shift = (op1 & 0x3) << 3;
        *op0 = (*op0 << (32 - shift)) | (*op0 >> shift);
    }
    return (arm7 << 1) + 1;
}

int Interpreter::strSpT(uint16_t opcode) // STR Rd,[SP,#i]
{
    // SP-relative word store, pre-adjust without writeback (THUMB)
    uint32_t op0 = *registers[(opcode >> 8) & 0x7];
    uint32_t op1 = *registers[13];
    uint32_t op2 = (opcode & 0xFF) << 2;
    core->memory.write<uint32_t>(arm7, op1 + op2, op0);
    return arm7 + 1;
}

int Interpreter::ldmiaT(uint16_t opcode) // LDMIA Rb!,<Rlist>
{
    // Block load, post-increment with writeback (THUMB)
    uint8_t m = bitCount[opcode & 0xFF];
    uint32_t *op0 = registers[(opcode >> 8) & 0x7];
    uint32_t address = (*op0 += (m << 2)) - (m << 2);
    for (int i = 0; i < 8; i++)
    {
        if (~opcode & BIT(i)) continue;
        *registers[i] = core->memory.read<uint32_t>(arm7, address);
        address += 4;
    }
    return m + (arm7 ? 2 : (m < 2));
}

int Interpreter::stmiaT(uint16_t opcode) // STMIA Rb!,<Rlist>
{
    // Store the writeback value if it's not the first listed register on ARM7
    uint8_t m = bitCount[opcode & 0xFF];
    uint8_t op0 = (opcode >> 8) & 0x7;
    uint32_t address = *registers[op0];
    if (arm7 && (opcode & (BIT(op0 + 1) - 1)) > BIT(op0))
        *registers[op0] = address + (m << 2);

    // Block store, post-increment with writeback (THUMB)
    for (int i = 0; i < 8; i++)
    {
        if (~opcode & BIT(i)) continue;
        core->memory.write<uint32_t>(arm7, address, *registers[i]);
        address += 4;
    }
    *registers[op0] = address;
    return m + (arm7 || m < 2);
}

int Interpreter::popT(uint16_t opcode) // POP <Rlist>
{
    // SP-relative block load, post-increment with writeback (THUMB)
    uint8_t m = bitCount[opcode & 0xFF];
    for (int i = 0; i < 8; i++)
    {
        if (~opcode & BIT(i)) continue;
        *registers[i] = core->memory.read<uint32_t>(arm7, *registers[13]);
        *registers[13] += 4;
    }
    return m + (arm7 ? 2 : (m < 2));
}

int Interpreter::pushT(uint16_t opcode) // PUSH <Rlist>
{
    // SP-relative block store, pre-decrement with writeback (THUMB)
    uint8_t m = bitCount[opcode & 0xFF];
    uint32_t address = (*registers[13] -= (m << 2));
    for (int i = 0; i < 8; i++)
    {
        if (~opcode & BIT(i)) continue;
        core->memory.write<uint32_t>(arm7, address, *registers[i]);
        address += 4;
    }
    return m + (arm7 || m < 2);
}

int Interpreter::popPcT(uint16_t opcode) // POP <Rlist>,PC
{
    // SP-relative block load, post-increment with writeback (THUMB)
    uint8_t m = bitCount[opcode & 0xFF] + 1;
    for (int i = 0; i < 8; i++)
    {
        if (~opcode & BIT(i)) continue;
        *registers[i] = core->memory.read<uint32_t>(arm7, *registers[13]);
        *registers[13] += 4;
    }

    // Load the program counter and handle pipelining
    *registers[15] = core->memory.read<uint32_t>(arm7, *registers[13]);
    *registers[13] += 4;
    cpsr &= ~((~(*registers[15]) & !arm7) << 5);
    flushPipeline();
    return m + 4;
}

int Interpreter::pushLrT(uint16_t opcode) // PUSH <Rlist>,LR
{
    // SP-relative block store, pre-decrement with writeback (THUMB)
    uint8_t m = bitCount[opcode & 0xFF] + 1;
    uint32_t address = (*registers[13] -= (m << 2));
    for (int i = 0; i < 8; i++)
    {
        if (~opcode & BIT(i)) continue;
        core->memory.write<uint32_t>(arm7, address, *registers[i]);
        address += 4;
    }

    // Store the link register
    core->memory.write<uint32_t>(arm7, address, *registers[14]);
    return m + (arm7 || m < 2);
}
