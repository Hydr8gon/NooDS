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

#ifndef INTERPRETER_OTHER_H
#define INTERPRETER_OTHER_H

#include "interpreter.h"

namespace interpreter_other
{

void mrsRc(interpreter::Cpu *cpu, uint32_t opcode);
void msrRc(interpreter::Cpu *cpu, uint32_t opcode);

void bx (interpreter::Cpu *cpu, uint32_t opcode);
void blx(interpreter::Cpu *cpu, uint32_t opcode);

void mrsRs(interpreter::Cpu *cpu, uint32_t opcode);
void msrRs(interpreter::Cpu *cpu, uint32_t opcode);

void b (interpreter::Cpu *cpu, uint32_t opcode);
void bl(interpreter::Cpu *cpu, uint32_t opcode);

void mcr(interpreter::Cpu *cpu, uint32_t opcode);
void mrc(interpreter::Cpu *cpu, uint32_t opcode);

void swi(interpreter::Cpu *cpu, uint32_t opcode);

}

#endif // INTERPRETER_OTHER_H
