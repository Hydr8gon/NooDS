/*
    Copyright 2019-2020 Hydr8gon

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
#include "interpreter_alu.h"
#include "interpreter_branch.h"
#include "interpreter_transfer.h"

Interpreter::Interpreter(Core *core, bool cpu): core(core), cpu(cpu)
{
    for (int i = 0; i < 16; i++)
        registers[i] = &registersUsr[i];

    // Prepare to boot the BIOS
    registersUsr[15] = ((cpu == 0) ? 0xFFFF0000 : 0x00000000) + 8;
    cpsr = 0x000000C0;
    setMode(0x13); // Supervisor
}

void Interpreter::directBoot()
{
    uint32_t entryAddr;

    // Prepare to directly boot an NDS ROM
    if (cpu == 0) // ARM9
    {
        entryAddr = core->memory.read<uint32_t>(0, 0x27FFE24);
        registersUsr[13] = 0x03002F7C;
        registersIrq[0]  = 0x03003F80;
        registersSvc[0]  = 0x03003FC0;
    }
    else // ARM7
    {
        entryAddr = core->memory.read<uint32_t>(0, 0x27FFE34);
        registersUsr[13] = 0x0380FD80;
        registersIrq[0]  = 0x0380FF80;
        registersSvc[0]  = 0x0380FFC0;
    }

    registersUsr[12] = entryAddr;
    registersUsr[14] = entryAddr;
    registersUsr[15] = entryAddr + 8;
    setMode(0x1F); // System
}

void Interpreter::enterGbaMode()
{
    // Point the program counter to the GBA BIOS
    // The GBA BIOS will take care of initializing everything else
    registersUsr[15] = 0x00000000 + 4;
    postFlg = 0;
}

void Interpreter::runCycle()
{
    // Trigger an interrupt if one was requested and enabled
    if (ime && (ie & irf) && !(cpsr & BIT(7)))
    {
        // Switch the CPU to interrupt mode
        uint32_t cpsrOld = cpsr;
        setMode(0x12);
        *spsr = cpsrOld;

        // Switch to ARM mode and block other interrupts
        cpsr &= ~BIT(5);
        cpsr |= BIT(7);

        // Save the return address and jump to the interrupt handler
        *registers[14] = *registers[15] - ((cpsrOld & BIT(5)) ? 0 : 4);
        *registers[15] = ((cpu == 0) ? core->cp15.getExceptionAddr() : 0) + 0x18 + 8;
    }

    // Execute an instruction
    if (cpsr & BIT(5)) // THUMB mode
    {
        // Execute 2 opcodes behind the program counter because of pipelining
        // In THUMB mode, this is 4 bytes behind
        uint16_t opcode = core->memory.read<uint16_t>(cpu, *registers[15] - 4);

        // THUMB lookup table, based on the map found at http://imrannazar.com/ARM-Opcode-Map
        // Uses bits 15-8 of an opcode to find the appropriate instruction
        switch ((opcode & 0xFF00) >> 8)
        {
            case 0x00: case 0x01: case 0x02: case 0x03:
            case 0x04: case 0x05: case 0x06: case 0x07:
                lslImmT(opcode); // LSL Rd,Rs,#i
                break;

            case 0x08: case 0x09: case 0x0A: case 0x0B:
            case 0x0C: case 0x0D: case 0x0E: case 0x0F:
                lsrImmT(opcode); // LSR Rd,Rs,#i
                break;

            case 0x10: case 0x11: case 0x12: case 0x13:
            case 0x14: case 0x15: case 0x16: case 0x17:
                asrImmT(opcode); // ASR Rd,Rs,#i
                break;

            case 0x18: case 0x19:
                addRegT(opcode); // ADD Rd,Rs,Rn
                break;

            case 0x1A: case 0x1B:
                subRegT(opcode); // SUB Rd,Rs,Rn
                break;

            case 0x1C: case 0x1D:
                addImm3T(opcode); // ADD Rd,Rs,#i
                break;

            case 0x1E: case 0x1F:
                subImm3T(opcode); // SUB Rd,Rs,#i
                break;

            case 0x20: case 0x21: case 0x22: case 0x23:
            case 0x24: case 0x25: case 0x26: case 0x27:
                movImm8T(opcode); // MOV Rd,#i
                break;

            case 0x28: case 0x29: case 0x2A: case 0x2B:
            case 0x2C: case 0x2D: case 0x2E: case 0x2F:
                cmpImm8T(opcode); // CMP Rd,#i
                break;

            case 0x30: case 0x31: case 0x32: case 0x33:
            case 0x34: case 0x35: case 0x36: case 0x37:
                addImm8T(opcode); // ADD Rd,#i
                break;

            case 0x38: case 0x39: case 0x3A: case 0x3B:
            case 0x3C: case 0x3D: case 0x3E: case 0x3F:
                subImm8T(opcode); // SUB Rd,#i
                break;

            case 0x40:
                switch ((opcode & 0x00C0) >> 6)
                {
                    case 0: andDpT(opcode); break; // AND Rd,Rs
                    case 1: eorDpT(opcode); break; // EOR Rd,Rs
                    case 2: lslDpT(opcode); break; // LSL Rd,Rs
                    case 3: lsrDpT(opcode); break; // LSR Rd,Rs
                }
                break;

            case 0x41:
                switch ((opcode & 0x00C0) >> 6)
                {
                    case 0: asrDpT(opcode); break; // ASR Rd,Rs
                    case 1: adcDpT(opcode); break; // ADC Rd,Rs
                    case 2: sbcDpT(opcode); break; // SBC Rd,Rs
                    case 3: rorDpT(opcode); break; // ROR Rd,Rs
                }
                break;

            case 0x42:
                switch ((opcode & 0x00C0) >> 6)
                {
                    case 0: tstDpT(opcode); break; // TST Rd,Rs
                    case 1: negDpT(opcode); break; // NEG Rd,Rs
                    case 2: cmpDpT(opcode); break; // CMP Rd,Rs
                    case 3: cmnDpT(opcode); break; // CMN Rd,Rs
                }
                break;

            case 0x43:
                switch ((opcode & 0x00C0) >> 6)
                {
                    case 0: orrDpT(opcode); break; // ORR Rd,Rs
                    case 1: mulDpT(opcode); break; // MUL Rd,Rs
                    case 2: bicDpT(opcode); break; // BIC Rd,Rs
                    case 3: mvnDpT(opcode); break; // MVN Rd,Rs
                }
                break;

            case 0x44:
                addHT(opcode); // ADD Rd,Rs
                break;

            case 0x45:
                cmpHT(opcode); // CMP Rd,Rs
                break;

            case 0x46:
                movHT(opcode); // MOV Rd,Rs
                break;

            case 0x47:
                if (opcode & BIT(7))
                    blxRegT(opcode); // BLX Rs
                else
                    bxRegT(opcode); // BX Rs
                break;

            case 0x48: case 0x49: case 0x4A: case 0x4B:
            case 0x4C: case 0x4D: case 0x4E: case 0x4F:
                ldrPcT(opcode); // LDR Rd,[PC,#i]
                break;

            case 0x50: case 0x51:
                strRegT(opcode); // STR Rd,[Rb,Ro]
                break;

            case 0x52: case 0x53:
                strhRegT(opcode); // STRH Rd,[Rb,Ro]
                break;

            case 0x54: case 0x55:
                strbRegT(opcode); // STRB Rd,[Rb,Ro]
                break;

            case 0x56: case 0x57:
                ldrsbRegT(opcode); // LDRSB Rd,[Rb,Ro]
                break;

            case 0x58: case 0x59:
                ldrRegT(opcode); // LDR Rd,[Rb,Ro]
                break;

            case 0x5A: case 0x5B:
                ldrhRegT(opcode); // LDRH Rd,[Rb,Ro]
                break;

            case 0x5C: case 0x5D:
                ldrbRegT(opcode); // LDRB Rd,[Rb,Ro]
                break;

            case 0x5E: case 0x5F:
                ldrshRegT(opcode); // LDRSH Rd,[Rb,Ro]
                break;

            case 0x60: case 0x61: case 0x62: case 0x63:
            case 0x64: case 0x65: case 0x66: case 0x67:
                strImm5T(opcode); // STR Rd,[Rb,#i]
                break;

            case 0x68: case 0x69: case 0x6A: case 0x6B:
            case 0x6C: case 0x6D: case 0x6E: case 0x6F:
                ldrImm5T(opcode); // LDR Rd,[Rb,#i]
                break;

            case 0x70: case 0x71: case 0x72: case 0x73:
            case 0x74: case 0x75: case 0x76: case 0x77:
                strbImm5T(opcode); // STRB Rd,[Rb,#i]
                break;

            case 0x78: case 0x79: case 0x7A: case 0x7B:
            case 0x7C: case 0x7D: case 0x7E: case 0x7F:
                ldrbImm5T(opcode); // LDRB Rd,[Rb,#i]
                break;

            case 0x80: case 0x81: case 0x82: case 0x83:
            case 0x84: case 0x85: case 0x86: case 0x87:
                strhImm5T(opcode); // STRH Rd,[Rb,#i]
                break;

            case 0x88: case 0x89: case 0x8A: case 0x8B:
            case 0x8C: case 0x8D: case 0x8E: case 0x8F:
                ldrhImm5T(opcode); // LDRH Rd,[Rb,#i]
                break;

            case 0x90: case 0x91: case 0x92: case 0x93:
            case 0x94: case 0x95: case 0x96: case 0x97:
                strSpT(opcode); // STR Rd,[SP,#i]
                break;

            case 0x98: case 0x99: case 0x9A: case 0x9B:
            case 0x9C: case 0x9D: case 0x9E: case 0x9F:
                ldrSpT(opcode); // LDR Rd,[SP,#i]
                break;

            case 0xA0: case 0xA1: case 0xA2: case 0xA3:
            case 0xA4: case 0xA5: case 0xA6: case 0xA7:
                addPcT(opcode); // ADD Rd,PC,#i
                break;

            case 0xA8: case 0xA9: case 0xAA: case 0xAB:
            case 0xAC: case 0xAD: case 0xAE: case 0xAF:
                addSpT(opcode); // ADD Rd,SP,#i
                break;

            case 0xB0:
                addSpImmT(opcode); // ADD SP,#i
                break;

            case 0xB4:
                pushT(opcode); // PUSH <Rlist>
                break;

            case 0xB5:
                pushLrT(opcode); // PUSH <Rlist>,LR
                break;

            case 0xBC:
                popT(opcode); // POP <Rlist>
                break;

            case 0xBD:
                popPcT(opcode); // POP <Rlist>,PC
                break;

            case 0xC0: case 0xC1: case 0xC2: case 0xC3:
            case 0xC4: case 0xC5: case 0xC6: case 0xC7:
                stmiaT(opcode); // STMIA Rb!,<Rlist>
                break;

            case 0xC8: case 0xC9: case 0xCA: case 0xCB:
            case 0xCC: case 0xCD: case 0xCE: case 0xCF:
                ldmiaT(opcode); // LDMIA Rb!,<Rlist>
                break;

            case 0xD0:
                beqT(opcode); // BEQ label
                break;

            case 0xD1:
                bneT(opcode); // BNE label
                break;

            case 0xD2:
                bcsT(opcode); // BCS label
                break;

            case 0xD3:
                bccT(opcode); // BCC label
                break;

            case 0xD4:
                bmiT(opcode); // BMI label
                break;

            case 0xD5:
                bplT(opcode); // BPL label
                break;

            case 0xD6:
                bvsT(opcode); // BVS label
                break;

            case 0xD7:
                bvcT(opcode); // BVC label
                break;

            case 0xD8:
                bhiT(opcode); // BHI label
                break;

            case 0xD9:
                blsT(opcode); // BLS label
                break;

            case 0xDA:
                bgeT(opcode); // BGE label
                break;

            case 0xDB:
                bltT(opcode); // BLT label
                break;

            case 0xDC:
                bgtT(opcode); // BGT label
                break;

            case 0xDD:
                bleT(opcode); // BLE label
                break;

            case 0xDF:
                swiT(); // SWI #i
                break;

            case 0xE0: case 0xE1: case 0xE2: case 0xE3:
            case 0xE4: case 0xE5: case 0xE6: case 0xE7:
                bT(opcode); // B label
                break;

            case 0xE8: case 0xE9: case 0xEA: case 0xEB:
            case 0xEC: case 0xED: case 0xEE: case 0xEF:
                blxOffT(opcode); // BLX label
                break;

            case 0xF0: case 0xF1: case 0xF2: case 0xF3:
            case 0xF4: case 0xF5: case 0xF6: case 0xF7:
                blSetupT(opcode); // BL/BLX label
                break;

            case 0xF8: case 0xF9: case 0xFA: case 0xFB:
            case 0xFC: case 0xFD: case 0xFE: case 0xFF:
                blOffT(opcode); // BL label
                break;

            default:
                printf("Unknown ARM%d THUMB opcode: 0x%X\n", ((cpu == 0) ? 9 : 7), opcode);
                break;
        }

        // Increment the program counter
        *registers[15] += 2;
    }
    else // ARM mode
    {
        // Execute 2 opcodes behind the program counter because of pipelining
        // In ARM mode, this is 8 bytes behind
        uint32_t opcode = core->memory.read<uint32_t>(cpu, *registers[15] - 8);

        if (condition(opcode))
        {
            // ARM lookup table, based on the map found at http://imrannazar.com/ARM-Opcode-Map
            // Uses bits 27-20 and 7-4 of an opcode to find the appropriate instruction
            switch (((opcode & 0x0FF00000) >> 16) | ((opcode & 0x000000F0) >> 4))
            {
                case 0x000: case 0x008:
                    _and(opcode, lli(opcode)); // AND Rd,Rn,Rm,LSL #i
                    break;

                case 0x001:
                    _and(opcode, llr(opcode)); // AND Rd,Rn,Rm,LSL Rs
                    break;

                case 0x002: case 0x00A:
                    _and(opcode, lri(opcode)); // AND Rd,Rn,Rm,LSR #i
                    break;

                case 0x003:
                    _and(opcode, lrr(opcode)); // AND Rd,Rn,Rm,LSR Rs
                    break;

                case 0x004: case 0x00C:
                    _and(opcode, ari(opcode)); // AND Rd,Rn,Rm,ASR #i
                    break;

                case 0x005:
                    _and(opcode, arr(opcode)); // AND Rd,Rn,Rm,ASR Rs
                    break;

                case 0x006: case 0x00E:
                    _and(opcode, rri(opcode)); // AND Rd,Rn,Rm,ROR #i
                    break;

                case 0x007:
                    _and(opcode, rrr(opcode)); // AND Rd,Rn,Rm,ROR Rs
                    break;

                case 0x009:
                    mul(opcode); // MUL Rd,Rm,Rs
                    break;

                case 0x00B: case 0x02B:
                    strhPt(opcode, -rp(opcode)); // STRH Rd,[Rn],-Rm
                    break;

                case 0x00D: case 0x02D:
                    ldrdPt(opcode, -rp(opcode)); // LDRD Rd,[Rn],-Rm
                    break;

                case 0x00F: case 0x02F:
                    strdPt(opcode, -rp(opcode)); // STRD Rd,[Rn],-Rm
                    break;

                case 0x010: case 0x018:
                    ands(opcode, lliS(opcode)); // ANDS Rd,Rn,Rm,LSL #i
                    break;

                case 0x011:
                    ands(opcode, llrS(opcode)); // ANDS Rd,Rn,Rm,LSL Rs
                    break;

                case 0x012: case 0x01A:
                    ands(opcode, lriS(opcode)); // ANDS Rd,Rn,Rm,LSR #i
                    break;

                case 0x013:
                    ands(opcode, lrrS(opcode)); // ANDS Rd,Rn,Rm,LSR Rs
                    break;

                case 0x014: case 0x01C:
                    ands(opcode, ariS(opcode)); // ANDS Rd,Rn,Rm,ASR #i
                    break;

                case 0x015:
                    ands(opcode, arrS(opcode)); // ANDS Rd,Rn,Rm,ASR Rs
                    break;

                case 0x016: case 0x01E:
                    ands(opcode, rriS(opcode)); // ANDS Rd,Rn,Rm,ROR #i
                    break;

                case 0x017:
                    ands(opcode, rrrS(opcode)); // ANDS Rd,Rn,Rm,ROR Rs
                    break;

                case 0x019:
                    muls(opcode); // MULS Rd,Rm,Rs
                    break;

                case 0x01B: case 0x03B:
                    ldrhPt(opcode, -rp(opcode)); // LDRH Rd,[Rn],-Rm
                    break;

                case 0x01D: case 0x03D:
                    ldrsbPt(opcode, -rp(opcode)); // LDRSB Rd,[Rn],-Rm
                    break;

                case 0x01F: case 0x03F:
                    ldrshPt(opcode, -rp(opcode)); // LDRSH Rd,[Rn],-Rm
                    break;

                case 0x020: case 0x028:
                    eor(opcode, lli(opcode)); // EOR Rd,Rn,Rm,LSL #i
                    break;

                case 0x021:
                    eor(opcode, llr(opcode)); // EOR Rd,Rn,Rm,LSL Rs
                    break;

                case 0x022: case 0x02A:
                    eor(opcode, lri(opcode)); // EOR Rd,Rn,Rm,LSR #i
                    break;

                case 0x023:
                    eor(opcode, lrr(opcode)); // EOR Rd,Rn,Rm,LSR Rs
                    break;

                case 0x024: case 0x02C:
                    eor(opcode, ari(opcode)); // EOR Rd,Rn,Rm,ASR #i
                    break;

                case 0x025:
                    eor(opcode, arr(opcode)); // EOR Rd,Rn,Rm,ASR Rs
                    break;

                case 0x026: case 0x02E:
                    eor(opcode, rri(opcode)); // EOR Rd,Rn,Rm,ROR #i
                    break;

                case 0x027:
                    eor(opcode, rrr(opcode)); // EOR Rd,Rn,Rm,ROR Rs
                    break;

                case 0x029:
                    mla(opcode); // MLA Rd,Rm,Rs,Rn
                    break;

                case 0x030: case 0x038:
                    eors(opcode, lliS(opcode)); // EORS Rd,Rn,Rm,LSL #i
                    break;

                case 0x031:
                    eors(opcode, llrS(opcode)); // EORS Rd,Rn,Rm,LSL Rs
                    break;

                case 0x032: case 0x03A:
                    eors(opcode, lriS(opcode)); // EORS Rd,Rn,Rm,LSR #i
                    break;

                case 0x033:
                    eors(opcode, lrrS(opcode)); // EORS Rd,Rn,Rm,LSR Rs
                    break;

                case 0x034: case 0x03C:
                    eors(opcode, ariS(opcode)); // EORS Rd,Rn,Rm,ASR #i
                    break;

                case 0x035:
                    eors(opcode, arrS(opcode)); // EORS Rd,Rn,Rm,ASR Rs
                    break;

                case 0x036: case 0x03E:
                    eors(opcode, rriS(opcode)); // EORS Rd,Rn,Rm,ROR #i
                    break;

                case 0x037:
                    eors(opcode, rrrS(opcode)); // EORS Rd,Rn,Rm,ROR Rs
                    break;

                case 0x039:
                    mlas(opcode); // MLAS Rd,Rm,Rs,Rn
                    break;

                case 0x040: case 0x048:
                    sub(opcode, lli(opcode)); // SUB Rd,Rn,Rm,LSL #i
                    break;

                case 0x041:
                    sub(opcode, llr(opcode)); // SUB Rd,Rn,Rm,LSL Rs
                    break;

                case 0x042: case 0x04A:
                    sub(opcode, lri(opcode)); // SUB Rd,Rn,Rm,LSR #i
                    break;

                case 0x043:
                    sub(opcode, lrr(opcode)); // SUB Rd,Rn,Rm,LSR Rs
                    break;

                case 0x044: case 0x04C:
                    sub(opcode, ari(opcode)); // SUB Rd,Rn,Rm,ASR #i
                    break;

                case 0x045:
                    sub(opcode, arr(opcode)); // SUB Rd,Rn,Rm,ASR Rs
                    break;

                case 0x046: case 0x04E:
                    sub(opcode, rri(opcode)); // SUB Rd,Rn,Rm,ROR #i
                    break;

                case 0x047:
                    sub(opcode, rrr(opcode)); // SUB Rd,Rn,Rm,ROR Rs
                    break;

                case 0x04B: case 0x06B:
                    strhPt(opcode, -ipH(opcode)); // STRH Rd,[Rn],-#i
                    break;

                case 0x04D: case 0x06D:
                    ldrdPt(opcode, -ipH(opcode)); // LDRD Rd,[Rn],-#i
                    break;

                case 0x04F: case 0x06F:
                    strdPt(opcode, -ipH(opcode)); // STRD Rd,[Rn],-#i
                    break;

                case 0x050: case 0x058:
                    subs(opcode, lli(opcode)); // SUBS Rd,Rn,Rm,LSL #i
                    break;

                case 0x051:
                    subs(opcode, llr(opcode)); // SUBS Rd,Rn,Rm,LSL Rs
                    break;

                case 0x052: case 0x05A:
                    subs(opcode, lri(opcode)); // SUBS Rd,Rn,Rm,LSR #i
                    break;

                case 0x053:
                    subs(opcode, lrr(opcode)); // SUBS Rd,Rn,Rm,LSR Rs
                    break;

                case 0x054: case 0x05C:
                    subs(opcode, ari(opcode)); // SUBS Rd,Rn,Rm,ASR #i
                    break;

                case 0x055:
                    subs(opcode, arr(opcode)); // SUBS Rd,Rn,Rm,ASR Rs
                    break;

                case 0x056: case 0x05E:
                    subs(opcode, rri(opcode)); // SUBS Rd,Rn,Rm,ROR #i
                    break;

                case 0x057:
                    subs(opcode, rrr(opcode)); // SUBS Rd,Rn,Rm,ROR Rs
                    break;

                case 0x05B: case 0x07B:
                    ldrhPt(opcode, -ipH(opcode)); // LDRH Rd,[Rn],-#i
                    break;

                case 0x05D: case 0x07D:
                    ldrsbPt(opcode, -ipH(opcode)); // LDRSB Rd,[Rn],-#i
                    break;

                case 0x05F: case 0x07F:
                    ldrshPt(opcode, -ipH(opcode)); // LDRSH Rd,[Rn],-#i
                    break;

                case 0x060: case 0x068:
                    rsb(opcode, lli(opcode)); // RSB Rd,Rn,Rm,LSL #i
                    break;

                case 0x061:
                    rsb(opcode, llr(opcode)); // RSB Rd,Rn,Rm,LSL Rs
                    break;

                case 0x062: case 0x06A:
                    rsb(opcode, lri(opcode)); // RSB Rd,Rn,Rm,LSR #i
                    break;

                case 0x063:
                    rsb(opcode, lrr(opcode)); // RSB Rd,Rn,Rm,LSR Rs
                    break;

                case 0x064: case 0x06C:
                    rsb(opcode, ari(opcode)); // RSB Rd,Rn,Rm,ASR #i
                    break;

                case 0x065:
                    rsb(opcode, arr(opcode)); // RSB Rd,Rn,Rm,ASR Rs
                    break;

                case 0x066: case 0x06E:
                    rsb(opcode, rri(opcode)); // RSB Rd,Rn,Rm,ROR #i
                    break;

                case 0x067:
                    rsb(opcode, rrr(opcode)); // RSB Rd,Rn,Rm,ROR Rs
                    break;

                case 0x070: case 0x078:
                    rsbs(opcode, lli(opcode)); // RSBS Rd,Rn,Rm,LSL #i
                    break;

                case 0x071:
                    rsbs(opcode, llr(opcode)); // RSBS Rd,Rn,Rm,LSL Rs
                    break;

                case 0x072: case 0x07A:
                    rsbs(opcode, lri(opcode)); // RSBS Rd,Rn,Rm,LSR #i
                    break;

                case 0x073:
                    rsbs(opcode, lrr(opcode)); // RSBS Rd,Rn,Rm,LSR Rs
                    break;

                case 0x074: case 0x07C:
                    rsbs(opcode, ari(opcode)); // RSBS Rd,Rn,Rm,ASR #i
                    break;

                case 0x075:
                    rsbs(opcode, arr(opcode)); // RSBS Rd,Rn,Rm,ASR Rs
                    break;

                case 0x076: case 0x07E:
                    rsbs(opcode, rri(opcode)); // RSBS Rd,Rn,Rm,ROR #i
                    break;

                case 0x077:
                    rsbs(opcode, rrr(opcode)); // RSBS Rd,Rn,Rm,ROR Rs
                    break;

                case 0x080: case 0x088:
                    add(opcode, lli(opcode)); // ADD Rd,Rn,Rm,LSL #i
                    break;

                case 0x081:
                    add(opcode, llr(opcode)); // ADD Rd,Rn,Rm,LSL Rs
                    break;

                case 0x082: case 0x08A:
                    add(opcode, lri(opcode)); // ADD Rd,Rn,Rm,LSR #i
                    break;

                case 0x083:
                    add(opcode, lrr(opcode)); // ADD Rd,Rn,Rm,LSR Rs
                    break;

                case 0x084: case 0x08C:
                    add(opcode, ari(opcode)); // ADD Rd,Rn,Rm,ASR #i
                    break;

                case 0x085:
                    add(opcode, arr(opcode)); // ADD Rd,Rn,Rm,ASR Rs
                    break;

                case 0x086: case 0x08E:
                    add(opcode, rri(opcode)); // ADD Rd,Rn,Rm,ROR #i
                    break;

                case 0x087:
                    add(opcode, rrr(opcode)); // ADD Rd,Rn,Rm,ROR Rs
                    break;

                case 0x089:
                    umull(opcode); // UMULL RdLo,RdHi,Rm,Rs
                    break;

                case 0x08B: case 0x0AB:
                    strhPt(opcode, rp(opcode)); // STRH Rd,[Rn],Rm
                    break;

                case 0x08D: case 0x0AD:
                    ldrdPt(opcode, rp(opcode)); // LDRD Rd,[Rn],Rm
                    break;

                case 0x08F: case 0x0AF:
                    strdPt(opcode, rp(opcode)); // STRD Rd,[Rn],Rm
                    break;

                case 0x090: case 0x098:
                    adds(opcode, lli(opcode)); // ADDS Rd,Rn,Rm,LSL #i
                    break;

                case 0x091:
                    adds(opcode, llr(opcode)); // ADDS Rd,Rn,Rm,LSL Rs
                    break;

                case 0x092: case 0x09A:
                    adds(opcode, lri(opcode)); // ADDS Rd,Rn,Rm,LSR #i
                    break;

                case 0x093:
                    adds(opcode, lrr(opcode)); // ADDS Rd,Rn,Rm,LSR Rs
                    break;

                case 0x094: case 0x09C:
                    adds(opcode, ari(opcode)); // ADDS Rd,Rn,Rm,ASR #i
                    break;

                case 0x095:
                    adds(opcode, arr(opcode)); // ADDS Rd,Rn,Rm,ASR Rs
                    break;

                case 0x096: case 0x09E:
                    adds(opcode, rri(opcode)); // ADDS Rd,Rn,Rm,ROR #i
                    break;

                case 0x097:
                    adds(opcode, rrr(opcode)); // ADDS Rd,Rn,Rm,ROR Rs
                    break;

                case 0x099:
                    umulls(opcode); // UMULLS RdLo,RdHi,Rm,Rs
                    break;

                case 0x09B: case 0x0BB:
                    ldrhPt(opcode, rp(opcode)); // LDRH Rd,[Rn],Rm
                    break;

                case 0x09D: case 0x0BD:
                    ldrsbPt(opcode, rp(opcode)); // LDRSB Rd,[Rn],Rm
                    break;

                case 0x09F: case 0x0BF:
                    ldrshPt(opcode, rp(opcode)); // LDRSH Rd,[Rn],Rm
                    break;

                case 0x0A0: case 0x0A8:
                    adc(opcode, lli(opcode)); // ADC Rd,Rn,Rm,LSL #i
                    break;

                case 0x0A1:
                    adc(opcode, llr(opcode)); // ADC Rd,Rn,Rm,LSL Rs
                    break;

                case 0x0A2: case 0x0AA:
                    adc(opcode, lri(opcode)); // ADC Rd,Rn,Rm,LSR #i
                    break;

                case 0x0A3:
                    adc(opcode, lrr(opcode)); // ADC Rd,Rn,Rm,LSR Rs
                    break;

                case 0x0A4: case 0x0AC:
                    adc(opcode, ari(opcode)); // ADC Rd,Rn,Rm,ASR #i
                    break;

                case 0x0A5:
                    adc(opcode, arr(opcode)); // ADC Rd,Rn,Rm,ASR Rs
                    break;

                case 0x0A6: case 0x0AE:
                    adc(opcode, rri(opcode)); // ADC Rd,Rn,Rm,ROR #i
                    break;

                case 0x0A7:
                    adc(opcode, rrr(opcode)); // ADC Rd,Rn,Rm,ROR Rs
                    break;

                case 0x0A9:
                    umlal(opcode); // UMLAL RdLo,RdHi,Rm,Rs
                    break;

                case 0x0B0: case 0x0B8:
                    adcs(opcode, lli(opcode)); // ADCS Rd,Rn,Rm,LSL #i
                    break;

                case 0x0B1:
                    adcs(opcode, llr(opcode)); // ADCS Rd,Rn,Rm,LSL Rs
                    break;

                case 0x0B2: case 0x0BA:
                    adcs(opcode, lri(opcode)); // ADCS Rd,Rn,Rm,LSR #i
                    break;

                case 0x0B3:
                    adcs(opcode, lrr(opcode)); // ADCS Rd,Rn,Rm,LSR Rs
                    break;

                case 0x0B4: case 0x0BC:
                    adcs(opcode, ari(opcode)); // ADCS Rd,Rn,Rm,ASR #i
                    break;

                case 0x0B5:
                    adcs(opcode, arr(opcode)); // ADCS Rd,Rn,Rm,ASR Rs
                    break;

                case 0x0B6: case 0x0BE:
                    adcs(opcode, rri(opcode)); // ADCS Rd,Rn,Rm,ROR #i
                    break;

                case 0x0B7:
                    adcs(opcode, rrr(opcode)); // ADCS Rd,Rn,Rm,ROR Rs
                    break;

                case 0x0B9:
                    umlals(opcode); // UMLALS RdLo,RdHi,Rm,Rs
                    break;

                case 0x0C0: case 0x0C8:
                    sbc(opcode, lli(opcode)); // SBC Rd,Rn,Rm,LSL #i
                    break;

                case 0x0C1:
                    sbc(opcode, llr(opcode)); // SBC Rd,Rn,Rm,LSL Rs
                    break;

                case 0x0C2: case 0x0CA:
                    sbc(opcode, lri(opcode)); // SBC Rd,Rn,Rm,LSR #i
                    break;

                case 0x0C3:
                    sbc(opcode, lrr(opcode)); // SBC Rd,Rn,Rm,LSR Rs
                    break;

                case 0x0C4: case 0x0CC:
                    sbc(opcode, ari(opcode)); // SBC Rd,Rn,Rm,ASR #i
                    break;

                case 0x0C5:
                    sbc(opcode, arr(opcode)); // SBC Rd,Rn,Rm,ASR Rs
                    break;

                case 0x0C6: case 0x0CE:
                    sbc(opcode, rri(opcode)); // SBC Rd,Rn,Rm,ROR #i
                    break;

                case 0x0C7:
                    sbc(opcode, rrr(opcode)); // SBC Rd,Rn,Rm,ROR Rs
                    break;

                case 0x0C9:
                    smull(opcode); // SMULL RdLo,RdHi,Rm,Rs
                    break;

                case 0x0CB: case 0x0EB:
                    strhPt(opcode, ipH(opcode)); // STRH Rd,[Rn],#i
                    break;

                case 0x0CD: case 0x0ED:
                    ldrdPt(opcode, ipH(opcode)); // LDRD Rd,[Rn],#i
                    break;

                case 0x0CF: case 0x0EF:
                    strdPt(opcode, ipH(opcode)); // STRD Rd,[Rn],#i
                    break;

                case 0x0D0: case 0x0D8:
                    sbcs(opcode, lli(opcode)); // SBCS Rd,Rn,Rm,LSL #i
                    break;

                case 0x0D1:
                    sbcs(opcode, llr(opcode)); // SBCS Rd,Rn,Rm,LSL Rs
                    break;

                case 0x0D2: case 0x0DA:
                    sbcs(opcode, lri(opcode)); // SBCS Rd,Rn,Rm,LSR #i
                    break;

                case 0x0D3:
                    sbcs(opcode, lrr(opcode)); // SBCS Rd,Rn,Rm,LSR Rs
                    break;

                case 0x0D4: case 0x0DC:
                    sbcs(opcode, ari(opcode)); // SBCS Rd,Rn,Rm,ASR #i
                    break;

                case 0x0D5:
                    sbcs(opcode, arr(opcode)); // SBCS Rd,Rn,Rm,ASR Rs
                    break;

                case 0x0D6: case 0x0DE:
                    sbcs(opcode, rri(opcode)); // SBCS Rd,Rn,Rm,ROR #i
                    break;

                case 0x0D7:
                    sbcs(opcode, rrr(opcode)); // SBCS Rd,Rn,Rm,ROR Rs
                    break;

                case 0x0D9:
                    smulls(opcode); // SMULLS RdLo,RdHi,Rm,Rs
                    break;

                case 0x0DB: case 0x0FB:
                    ldrhPt(opcode, ipH(opcode)); // LDRH Rd,[Rn],#i
                    break;

                case 0x0DD: case 0x0FD:
                    ldrsbPt(opcode, ipH(opcode)); // LDRSB Rd,[Rn],#i
                    break;

                case 0x0DF: case 0x0FF:
                    ldrshPt(opcode, ipH(opcode)); // LDRSH Rd,[Rn],#i
                    break;

                case 0x0E0: case 0x0E8:
                    rsc(opcode, lli(opcode)); // RSC Rd,Rn,Rm,LSL #i
                    break;

                case 0x0E1:
                    rsc(opcode, llr(opcode)); // RSC Rd,Rn,Rm,LSL Rs
                    break;

                case 0x0E2: case 0x0EA:
                    rsc(opcode, lri(opcode)); // RSC Rd,Rn,Rm,LSR #i
                    break;

                case 0x0E3:
                    rsc(opcode, lrr(opcode)); // RSC Rd,Rn,Rm,LSR Rs
                    break;

                case 0x0E4: case 0x0EC:
                    rsc(opcode, ari(opcode)); // RSC Rd,Rn,Rm,ASR #i
                    break;

                case 0x0E5:
                    rsc(opcode, arr(opcode)); // RSC Rd,Rn,Rm,ASR Rs
                    break;

                case 0x0E6: case 0x0EE:
                    rsc(opcode, rri(opcode)); // RSC Rd,Rn,Rm,ROR #i
                    break;

                case 0x0E7:
                    rsc(opcode, rrr(opcode)); // RSC Rd,Rn,Rm,ROR Rs
                    break;

                case 0x0E9:
                    smlal(opcode); // SMLAL RdLo,RdHi,Rm,Rs
                    break;

                case 0x0F0: case 0x0F8:
                    rscs(opcode, lli(opcode)); // RSCS Rd,Rn,Rm,LSL #i
                    break;

                case 0x0F1:
                    rscs(opcode, llr(opcode)); // RSCS Rd,Rn,Rm,LSL Rs
                    break;

                case 0x0F2: case 0x0FA:
                    rscs(opcode, lri(opcode)); // RSCS Rd,Rn,Rm,LSR #i
                    break;

                case 0x0F3:
                    rscs(opcode, lrr(opcode)); // RSCS Rd,Rn,Rm,LSR Rs
                    break;

                case 0x0F4: case 0x0FC:
                    rscs(opcode, ari(opcode)); // RSCS Rd,Rn,Rm,ASR #i
                    break;

                case 0x0F5:
                    rscs(opcode, arr(opcode)); // RSCS Rd,Rn,Rm,ASR Rs
                    break;

                case 0x0F6: case 0x0FE:
                    rscs(opcode, rri(opcode)); // RSCS Rd,Rn,Rm,ROR #i
                    break;

                case 0x0F7:
                    rscs(opcode, rrr(opcode)); // RSCS Rd,Rn,Rm,ROR Rs
                    break;

                case 0x0F9:
                    smlals(opcode); // SMLALS RdLo,RdHi,Rm,Rs
                    break;

                case 0x100:
                    mrsRc(opcode); // MRS Rd,CPSR
                    break;

                case 0x105:
                    qadd(opcode); // QADD Rd,Rm,Rn
                    break;

                case 0x108:
                    smlabb(opcode); // SMLABB Rd,Rm,Rs,Rn
                    break;

                case 0x109:
                    swp(opcode); // SWP Rd,Rm,[Rn]
                    break;

                case 0x10A:
                    smlatb(opcode); // SMLATB Rd,Rm,Rs,Rn
                    break;

                case 0x10B:
                    strhOf(opcode, -rp(opcode)); // STRH Rd,[Rn,-Rm]
                    break;

                case 0x10C:
                    smlabt(opcode); // SMLABT Rd,Rm,Rs,Rn
                    break;

                case 0x10D:
                    ldrdOf(opcode, -rp(opcode)); // LDRD Rd,[Rn,-Rm]
                    break;

                case 0x10E:
                    smlatt(opcode); // SMLATT Rd,Rm,Rs,Rn
                    break;

                case 0x10F:
                    strdOf(opcode, -rp(opcode)); // STRD Rd,[Rn,-Rm]
                    break;

                case 0x110: case 0x118:
                    tst(opcode, lliS(opcode)); // TST Rn,Rm,LSL #i
                    break;

                case 0x111:
                    tst(opcode, llrS(opcode)); // TST Rn,Rm,LSL Rs
                    break;

                case 0x112: case 0x11A:
                    tst(opcode, lriS(opcode)); // TST Rn,Rm,LSR #i
                    break;

                case 0x113:
                    tst(opcode, lrrS(opcode)); // TST Rn,Rm,LSR Rs
                    break;

                case 0x114: case 0x11C:
                    tst(opcode, ariS(opcode)); // TST Rn,Rm,ASR #i
                    break;

                case 0x115:
                    tst(opcode, arrS(opcode)); // TST Rn,Rm,ASR Rs
                    break;

                case 0x116: case 0x11E:
                    tst(opcode, rriS(opcode)); // TST Rn,Rm,ROR #i
                    break;

                case 0x117:
                    tst(opcode, rrrS(opcode)); // TST Rn,Rm,ROR Rs
                    break;

                case 0x11B:
                    ldrhOf(opcode, -rp(opcode)); // LDRH Rd,[Rn,-Rm]
                    break;

                case 0x11D:
                    ldrsbOf(opcode, -rp(opcode)); // LDRSB Rd,[Rn,-Rm]
                    break;

                case 0x11F:
                    ldrshOf(opcode, -rp(opcode)); // LDRSH Rd,[Rn,-Rm]
                    break;

                case 0x120:
                    msrRc(opcode); // MSR CPSR,Rm
                    break;

                case 0x121:
                    bx(opcode); // BX Rn
                    break;

                case 0x123:
                    blxReg(opcode); // BLX Rn
                    break;

                case 0x125:
                    qsub(opcode); // QSUB Rd,Rm,Rn
                    break;

                case 0x128:
                    smlawb(opcode); // SMLAWB Rd,Rm,Rs,Rn
                    break;

                case 0x12A:
                    smulwb(opcode); // SMULWB Rd,Rm,Rs
                    break;

                case 0x12B:
                    strhPr(opcode, -rp(opcode)); // STRH Rd,[Rn,-Rm]!
                    break;

                case 0x12C:
                    smlawt(opcode); // SMLAWT Rd,Rm,Rs,Rn
                    break;

                case 0x12D:
                    ldrdPr(opcode, -rp(opcode)); // LDRD Rd,[Rn,-Rm]!
                    break;

                case 0x12E:
                    smulwt(opcode); // SMULWT Rd,Rm,Rs
                    break;

                case 0x12F:
                    strdPr(opcode, -rp(opcode)); // STRD Rd,[Rn,-Rm]!
                    break;

                case 0x130: case 0x138:
                    teq(opcode, lliS(opcode)); // TEQ Rn,Rm,LSL #i
                    break;

                case 0x131:
                    teq(opcode, llrS(opcode)); // TEQ Rn,Rm,LSL Rs
                    break;

                case 0x132: case 0x13A:
                    teq(opcode, lriS(opcode)); // TEQ Rn,Rm,LSR #i
                    break;

                case 0x133:
                    teq(opcode, lrrS(opcode)); // TEQ Rn,Rm,LSR Rs
                    break;

                case 0x134: case 0x13C:
                    teq(opcode, ariS(opcode)); // TEQ Rn,Rm,ASR #i
                    break;

                case 0x135:
                    teq(opcode, arrS(opcode)); // TEQ Rn,Rm,ASR Rs
                    break;

                case 0x136: case 0x13E:
                    teq(opcode, rriS(opcode)); // TEQ Rn,Rm,ROR #i
                    break;

                case 0x137:
                    teq(opcode, rrrS(opcode)); // TEQ Rn,Rm,ROR Rs
                    break;

                case 0x13B:
                    ldrhPr(opcode, -rp(opcode)); // LDRH Rd,[Rn,-Rm]!
                    break;

                case 0x13D:
                    ldrsbPr(opcode, -rp(opcode)); // LDRSB Rd,[Rn,-Rm]!
                    break;

                case 0x13F:
                    ldrshPr(opcode, -rp(opcode)); // LDRSH Rd,[Rn,-Rm]!
                    break;

                case 0x140:
                    mrsRs(opcode); // MRS Rd,SPSR
                    break;

                case 0x145:
                    qdadd(opcode); // QDADD Rd,Rm,Rn
                    break;

                case 0x148:
                    smlalbb(opcode); // SMLALBB RdLo,RdHi,Rm,Rs
                    break;

                case 0x149:
                    swpb(opcode); // SWPB Rd,Rm,[Rn]
                    break;

                case 0x14A:
                    smlaltb(opcode); // SMLALTB RdLo,RdHi,Rm,Rs
                    break;

                case 0x14B:
                    strhOf(opcode, -ipH(opcode)); // STRH Rd,[Rn,-#i]
                    break;

                case 0x14C:
                    smlalbt(opcode); // SMLALBT RdLo,RdHi,Rm,Rs
                    break;

                case 0x14D:
                    ldrdOf(opcode, -ipH(opcode)); // LDRD Rd,[Rn,-#i]
                    break;

                case 0x14E:
                    smlaltt(opcode); // SMLALTT RdLo,RdHi,Rm,Rs
                    break;

                case 0x14F:
                    strdOf(opcode, -ipH(opcode)); // STRD Rd,[Rn,-#i]
                    break;

                case 0x150: case 0x158:
                    cmp(opcode, lli(opcode)); // CMP Rn,Rm,LSL #i
                    break;

                case 0x151:
                    cmp(opcode, llr(opcode)); // CMP Rn,Rm,LSL Rs
                    break;

                case 0x152: case 0x15A:
                    cmp(opcode, lri(opcode)); // CMP Rn,Rm,LSR #i
                    break;

                case 0x153:
                    cmp(opcode, lrr(opcode)); // CMP Rn,Rm,LSR Rs
                    break;

                case 0x154: case 0x15C:
                    cmp(opcode, ari(opcode)); // CMP Rn,Rm,ASR #i
                    break;

                case 0x155:
                    cmp(opcode, arr(opcode)); // CMP Rn,Rm,ASR Rs
                    break;

                case 0x156: case 0x15E:
                    cmp(opcode, rri(opcode)); // CMP Rn,Rm,ROR #i
                    break;

                case 0x157:
                    cmp(opcode, rrr(opcode)); // CMP Rn,Rm,ROR Rs
                    break;

                case 0x15B:
                    ldrhOf(opcode, -ipH(opcode)); // LDRH Rd,[Rn,-#i]
                    break;

                case 0x15D:
                    ldrsbOf(opcode, -ipH(opcode)); // LDRSB Rd,[Rn,-#i]
                    break;

                case 0x15F:
                    ldrshOf(opcode, -ipH(opcode)); // LDRSH Rd,[Rn,-#i]
                    break;

                case 0x160:
                    msrRs(opcode); // MSR SPSR,Rm
                    break;

                case 0x161:
                    clz(opcode); // CLZ Rd,Rm
                    break;

                case 0x165:
                    qdsub(opcode); // QDSUB Rd,Rm,Rn
                    break;

                case 0x168:
                    smulbb(opcode); // SMULBB Rd,Rm,Rs
                    break;

                case 0x16A:
                    smultb(opcode); // SMULTB Rd,Rm,Rs
                    break;

                case 0x16B:
                    strhPr(opcode, -ipH(opcode)); // STRH Rd,[Rn,-#i]!
                    break;

                case 0x16C:
                    smulbt(opcode); // SMULBT Rd,Rm,Rs
                    break;

                case 0x16D:
                    ldrdPr(opcode, -ipH(opcode)); // LDRD Rd,[Rn,-#i]!
                    break;

                case 0x16E:
                    smultt(opcode); // SMULTT Rd,Rm,Rs
                    break;

                case 0x16F:
                    strdPr(opcode, -ipH(opcode)); // STRD Rd,[Rn,-#i]!
                    break;

                case 0x170: case 0x178:
                    cmn(opcode, lli(opcode)); // CMN Rn,Rm,LSL #i
                    break;

                case 0x171:
                    cmn(opcode, llr(opcode)); // CMN Rn,Rm,LSL Rs
                    break;

                case 0x172: case 0x17A:
                    cmn(opcode, lri(opcode)); // CMN Rn,Rm,LSR #i
                    break;

                case 0x173:
                    cmn(opcode, lrr(opcode)); // CMN Rn,Rm,LSR Rs
                    break;

                case 0x174: case 0x17C:
                    cmn(opcode, ari(opcode)); // CMN Rn,Rm,ASR #i
                    break;

                case 0x175:
                    cmn(opcode, arr(opcode)); // CMN Rn,Rm,ASR Rs
                    break;

                case 0x176: case 0x17E:
                    cmn(opcode, rri(opcode)); // CMN Rn,Rm,ROR #i
                    break;

                case 0x177:
                    cmn(opcode, rrr(opcode)); // CMN Rn,Rm,ROR Rs
                    break;

                case 0x17B:
                    ldrhPr(opcode, -ipH(opcode)); // LDRH Rd,[Rn,-#i]!
                    break;

                case 0x17D:
                    ldrsbPr(opcode, -ipH(opcode)); // LDRSB Rd,[Rn,-#i]!
                    break;

                case 0x17F:
                    ldrshPr(opcode, -ipH(opcode)); // LDRSH Rd,[Rn,-#i]!
                    break;

                case 0x180: case 0x188:
                    orr(opcode, lli(opcode)); // ORR Rd,Rn,Rm,LSL #i
                    break;

                case 0x181:
                    orr(opcode, llr(opcode)); // ORR Rd,Rn,Rm,LSL Rs
                    break;

                case 0x182: case 0x18A:
                    orr(opcode, lri(opcode)); // ORR Rd,Rn,Rm,LSR #i
                    break;

                case 0x183:
                    orr(opcode, lrr(opcode)); // ORR Rd,Rn,Rm,LSR Rs
                    break;

                case 0x184: case 0x18C:
                    orr(opcode, ari(opcode)); // ORR Rd,Rn,Rm,ASR #i
                    break;

                case 0x185:
                    orr(opcode, arr(opcode)); // ORR Rd,Rn,Rm,ASR Rs
                    break;

                case 0x186: case 0x18E:
                    orr(opcode, rri(opcode)); // ORR Rd,Rn,Rm,ROR #i
                    break;

                case 0x187:
                    orr(opcode, rrr(opcode)); // ORR Rd,Rn,Rm,ROR Rs
                    break;

                case 0x18B:
                    strhOf(opcode, rp(opcode)); // STRH Rd,[Rn,Rm]
                    break;

                case 0x18D:
                    ldrdOf(opcode, rp(opcode)); // STRD Rd,[Rn,Rm]
                    break;

                case 0x18F:
                    strdOf(opcode, rp(opcode)); // STRD Rd,[Rn,Rm]
                    break;

                case 0x190: case 0x198:
                    orrs(opcode, lliS(opcode)); // ORRS Rd,Rn,Rm,LSL #i
                    break;

                case 0x191:
                    orrs(opcode, llrS(opcode)); // ORRS Rd,Rn,Rm,LSL Rs
                    break;

                case 0x192: case 0x19A:
                    orrs(opcode, lriS(opcode)); // ORRS Rd,Rn,Rm,LSR #i
                    break;

                case 0x193:
                    orrs(opcode, lrrS(opcode)); // ORRS Rd,Rn,Rm,LSR Rs
                    break;

                case 0x194: case 0x19C:
                    orrs(opcode, ariS(opcode)); // ORRS Rd,Rn,Rm,ASR #i
                    break;

                case 0x195:
                    orrs(opcode, arrS(opcode)); // ORRS Rd,Rn,Rm,ASR Rs
                    break;

                case 0x196: case 0x19E:
                    orrs(opcode, rriS(opcode)); // ORRS Rd,Rn,Rm,ROR #i
                    break;

                case 0x197:
                    orrs(opcode, rrrS(opcode)); // ORRS Rd,Rn,Rm,ROR Rs
                    break;

                case 0x19B:
                    ldrhOf(opcode, rp(opcode)); // LDRH Rd,[Rn,Rm]
                    break;

                case 0x19D:
                    ldrsbOf(opcode, rp(opcode)); // LDRSB Rd,[Rn,Rm]
                    break;

                case 0x19F:
                    ldrshOf(opcode, rp(opcode)); // LDRSH Rd,[Rn,Rm]
                    break;

                case 0x1A0: case 0x1A8:
                    mov(opcode, lli(opcode)); // MOV Rd,Rm,LSL #i
                    break;

                case 0x1A1:
                    mov(opcode, llr(opcode)); // MOV Rd,Rm,LSL Rs
                    break;

                case 0x1A2: case 0x1AA:
                    mov(opcode, lri(opcode)); // MOV Rd,Rm,LSR #i
                    break;

                case 0x1A3:
                    mov(opcode, lrr(opcode)); // MOV Rd,Rm,LSR Rs
                    break;

                case 0x1A4: case 0x1AC:
                    mov(opcode, ari(opcode)); // MOV Rd,Rm,ASR #i
                    break;

                case 0x1A5:
                    mov(opcode, arr(opcode)); // MOV Rd,Rm,ASR Rs
                    break;

                case 0x1A6: case 0x1AE:
                    mov(opcode, rri(opcode)); // MOV Rd,Rm,ROR #i
                    break;

                case 0x1A7:
                    mov(opcode, rrr(opcode)); // MOV Rd,Rm,ROR Rs
                    break;

                case 0x1AB:
                    strhPr(opcode, rp(opcode)); // STRH Rd,[Rn,Rm]!
                    break;

                case 0x1AD:
                    ldrdPr(opcode, rp(opcode)); // STRD Rd,[Rn,Rm]!
                    break;

                case 0x1AF:
                    strdPr(opcode, rp(opcode)); // STRD Rd,[Rn,Rm]!
                    break;

                case 0x1B0: case 0x1B8:
                    movs(opcode, lliS(opcode)); // MOVS Rd,Rm,LSL #i
                    break;

                case 0x1B1:
                    movs(opcode, llrS(opcode)); // MOVS Rd,Rm,LSL Rs
                    break;

                case 0x1B2: case 0x1BA:
                    movs(opcode, lriS(opcode)); // MOVS Rd,Rm,LSR #i
                    break;

                case 0x1B3:
                    movs(opcode, lrrS(opcode)); // MOVS Rd,Rm,LSR Rs
                    break;

                case 0x1B4: case 0x1BC:
                    movs(opcode, ariS(opcode)); // MOVS Rd,Rm,ASR #i
                    break;

                case 0x1B5:
                    movs(opcode, arrS(opcode)); // MOVS Rd,Rm,ASR Rs
                    break;

                case 0x1B6: case 0x1BE:
                    movs(opcode, rriS(opcode)); // MOVS Rd,Rm,ROR #i
                    break;

                case 0x1B7:
                    movs(opcode, rrrS(opcode)); // MOVS Rd,Rm,ROR Rs
                    break;

                case 0x1BB:
                    ldrhPr(opcode, rp(opcode)); // LDRH Rd,[Rn,Rm]!
                    break;

                case 0x1BD:
                    ldrsbPr(opcode, rp(opcode)); // LDRSB Rd,[Rn,Rm]!
                    break;

                case 0x1BF:
                    ldrshPr(opcode, rp(opcode)); // LDRSH Rd,[Rn,Rm]!
                    break;

                case 0x1C0: case 0x1C8:
                    bic(opcode, lli(opcode)); // BIC Rd,Rn,Rm,LSL #i
                    break;

                case 0x1C1:
                    bic(opcode, llr(opcode)); // BIC Rd,Rn,Rm,LSL Rs
                    break;

                case 0x1C2: case 0x1CA:
                    bic(opcode, lri(opcode)); // BIC Rd,Rn,Rm,LSR #i
                    break;

                case 0x1C3:
                    bic(opcode, lrr(opcode)); // BIC Rd,Rn,Rm,LSR Rs
                    break;

                case 0x1C4: case 0x1CC:
                    bic(opcode, ari(opcode)); // BIC Rd,Rn,Rm,ASR #i
                    break;

                case 0x1C5:
                    bic(opcode, arr(opcode)); // BIC Rd,Rn,Rm,ASR Rs
                    break;

                case 0x1C6: case 0x1CE:
                    bic(opcode, rri(opcode)); // BIC Rd,Rn,Rm,ROR #i
                    break;

                case 0x1C7:
                    bic(opcode, rrr(opcode)); // BIC Rd,Rn,Rm,ROR Rs
                    break;

                case 0x1CB:
                    strhOf(opcode, ipH(opcode)); // STRH Rd,[Rn,#i]
                    break;

                case 0x1CD:
                    ldrdOf(opcode, ipH(opcode)); // STRD Rd,[Rn,#i]
                    break;

                case 0x1CF:
                    strdOf(opcode, ipH(opcode)); // STRD Rd,[Rn,#i]
                    break;

                case 0x1D0: case 0x1D8:
                    bics(opcode, lliS(opcode)); // BICS Rd,Rn,Rm,LSL #i
                    break;

                case 0x1D1:
                    bics(opcode, llrS(opcode)); // BICS Rd,Rn,Rm,LSL Rs
                    break;

                case 0x1D2: case 0x1DA:
                    bics(opcode, lriS(opcode)); // BICS Rd,Rn,Rm,LSR #i
                    break;

                case 0x1D3:
                    bics(opcode, lrrS(opcode)); // BICS Rd,Rn,Rm,LSR Rs
                    break;

                case 0x1D4: case 0x1DC:
                    bics(opcode, ariS(opcode)); // BICS Rd,Rn,Rm,ASR #i
                    break;

                case 0x1D5:
                    bics(opcode, arrS(opcode)); // BICS Rd,Rn,Rm,ASR Rs
                    break;

                case 0x1D6: case 0x1DE:
                    bics(opcode, rriS(opcode)); // BICS Rd,Rn,Rm,ROR #i
                    break;

                case 0x1D7:
                    bics(opcode, rrrS(opcode)); // BICS Rd,Rn,Rm,ROR Rs
                    break;

                case 0x1DB:
                    ldrhOf(opcode, ipH(opcode)); // LDRH Rd,[Rn,#i]
                    break;

                case 0x1DD:
                    ldrsbOf(opcode, ipH(opcode)); // LDRSB Rd,[Rn,#i]
                    break;

                case 0x1DF:
                    ldrshOf(opcode, ipH(opcode)); // LDRSH Rd,[Rn,#i]
                    break;

                case 0x1E0: case 0x1E8:
                    mvn(opcode, lli(opcode)); // MVN Rd,Rm,LSL #i
                    break;

                case 0x1E1:
                    mvn(opcode, llr(opcode)); // MVN Rd,Rm,LSL Rs
                    break;

                case 0x1E2: case 0x1EA:
                    mvn(opcode, lri(opcode)); // MVN Rd,Rm,LSR #i
                    break;

                case 0x1E3:
                    mvn(opcode, lrr(opcode)); // MVN Rd,Rm,LSR Rs
                    break;

                case 0x1E4: case 0x1EC:
                    mvn(opcode, ari(opcode)); // MVN Rd,Rm,ASR #i
                    break;

                case 0x1E5:
                    mvn(opcode, arr(opcode)); // MVN Rd,Rm,ASR Rs
                    break;

                case 0x1E6: case 0x1EE:
                    mvn(opcode, rri(opcode)); // MVN Rd,Rm,ROR #i
                    break;

                case 0x1E7:
                    mvn(opcode, rrr(opcode)); // MVN Rd,Rm,ROR Rs
                    break;

                case 0x1EB:
                    strhPr(opcode, ipH(opcode)); // STRH Rd,[Rn,#i]!
                    break;

                case 0x1ED:
                    ldrdPr(opcode, ipH(opcode)); // STRD Rd,[Rn,Rm]!
                    break;

                case 0x1EF:
                    strdPr(opcode, ipH(opcode)); // STRD Rd,[Rn,Rm]!
                    break;

                case 0x1F0: case 0x1F8:
                    mvns(opcode, lliS(opcode)); // MVNS Rd,Rm,LSL #i
                    break;

                case 0x1F1:
                    mvns(opcode, llrS(opcode)); // MVNS Rd,Rm,LSL Rs
                    break;

                case 0x1F2: case 0x1FA:
                    mvns(opcode, lriS(opcode)); // MVNS Rd,Rm,LSR #i
                    break;

                case 0x1F3:
                    mvns(opcode, lrrS(opcode)); // MVNS Rd,Rm,LSR Rs
                    break;

                case 0x1F4: case 0x1FC:
                    mvns(opcode, ariS(opcode)); // MVNS Rd,Rm,ASR #i
                    break;

                case 0x1F5:
                    mvns(opcode, arrS(opcode)); // MVNS Rd,Rm,ASR Rs
                    break;

                case 0x1F6: case 0x1FE:
                    mvns(opcode, rriS(opcode)); // MVNS Rd,Rm,ROR #i
                    break;

                case 0x1F7:
                    mvns(opcode, rrrS(opcode)); // MVNS Rd,Rm,ROR Rs
                    break;

                case 0x1FB:
                    ldrhPr(opcode, ipH(opcode)); // LDRH Rd,[Rn,#i]!
                    break;

                case 0x1FD:
                    ldrsbPr(opcode, ipH(opcode)); // LDRSB Rd,[Rn,#i]!
                    break;

                case 0x1FF:
                    ldrshPr(opcode, ipH(opcode)); // LDRSH Rd,[Rn,#i]!
                    break;

                case 0x200: case 0x201: case 0x202: case 0x203:
                case 0x204: case 0x205: case 0x206: case 0x207:
                case 0x208: case 0x209: case 0x20A: case 0x20B:
                case 0x20C: case 0x20D: case 0x20E: case 0x20F:
                    _and(opcode, imm(opcode)); // AND Rd,Rn,#i
                    break;

                case 0x210: case 0x211: case 0x212: case 0x213:
                case 0x214: case 0x215: case 0x216: case 0x217:
                case 0x218: case 0x219: case 0x21A: case 0x21B:
                case 0x21C: case 0x21D: case 0x21E: case 0x21F:
                    ands(opcode, immS(opcode)); // ANDS Rd,Rn,#i
                    break;

                case 0x220: case 0x221: case 0x222: case 0x223:
                case 0x224: case 0x225: case 0x226: case 0x227:
                case 0x228: case 0x229: case 0x22A: case 0x22B:
                case 0x22C: case 0x22D: case 0x22E: case 0x22F:
                    eor(opcode, imm(opcode)); // EOR Rd,Rn,#i
                    break;

                case 0x230: case 0x231: case 0x232: case 0x233:
                case 0x234: case 0x235: case 0x236: case 0x237:
                case 0x238: case 0x239: case 0x23A: case 0x23B:
                case 0x23C: case 0x23D: case 0x23E: case 0x23F:
                    eors(opcode, immS(opcode)); // EORS Rd,Rn,#i
                    break;

                case 0x240: case 0x241: case 0x242: case 0x243:
                case 0x244: case 0x245: case 0x246: case 0x247:
                case 0x248: case 0x249: case 0x24A: case 0x24B:
                case 0x24C: case 0x24D: case 0x24E: case 0x24F:
                    sub(opcode, imm(opcode)); // SUB Rd,Rn,#i
                    break;

                case 0x250: case 0x251: case 0x252: case 0x253:
                case 0x254: case 0x255: case 0x256: case 0x257:
                case 0x258: case 0x259: case 0x25A: case 0x25B:
                case 0x25C: case 0x25D: case 0x25E: case 0x25F:
                    subs(opcode, immS(opcode)); // SUBS Rd,Rn,#i
                    break;

                case 0x260: case 0x261: case 0x262: case 0x263:
                case 0x264: case 0x265: case 0x266: case 0x267:
                case 0x268: case 0x269: case 0x26A: case 0x26B:
                case 0x26C: case 0x26D: case 0x26E: case 0x26F:
                    rsb(opcode, imm(opcode)); // RSB Rd,Rn,#i
                    break;

                case 0x270: case 0x271: case 0x272: case 0x273:
                case 0x274: case 0x275: case 0x276: case 0x277:
                case 0x278: case 0x279: case 0x27A: case 0x27B:
                case 0x27C: case 0x27D: case 0x27E: case 0x27F:
                    rsbs(opcode, immS(opcode)); // RSBS Rd,Rn,#i
                    break;

                case 0x280: case 0x281: case 0x282: case 0x283:
                case 0x284: case 0x285: case 0x286: case 0x287:
                case 0x288: case 0x289: case 0x28A: case 0x28B:
                case 0x28C: case 0x28D: case 0x28E: case 0x28F:
                    add(opcode, imm(opcode)); // ADD Rd,Rn,#i
                    break;

                case 0x290: case 0x291: case 0x292: case 0x293:
                case 0x294: case 0x295: case 0x296: case 0x297:
                case 0x298: case 0x299: case 0x29A: case 0x29B:
                case 0x29C: case 0x29D: case 0x29E: case 0x29F:
                    adds(opcode, immS(opcode)); // ADDS Rd,Rn,#i
                    break;

                case 0x2A0: case 0x2A1: case 0x2A2: case 0x2A3:
                case 0x2A4: case 0x2A5: case 0x2A6: case 0x2A7:
                case 0x2A8: case 0x2A9: case 0x2AA: case 0x2AB:
                case 0x2AC: case 0x2AD: case 0x2AE: case 0x2AF:
                    adc(opcode, imm(opcode)); // ADC Rd,Rn,#i
                    break;

                case 0x2B0: case 0x2B1: case 0x2B2: case 0x2B3:
                case 0x2B4: case 0x2B5: case 0x2B6: case 0x2B7:
                case 0x2B8: case 0x2B9: case 0x2BA: case 0x2BB:
                case 0x2BC: case 0x2BD: case 0x2BE: case 0x2BF:
                    adcs(opcode, immS(opcode)); // ADCS Rd,Rn,#i
                    break;

                case 0x2C0: case 0x2C1: case 0x2C2: case 0x2C3:
                case 0x2C4: case 0x2C5: case 0x2C6: case 0x2C7:
                case 0x2C8: case 0x2C9: case 0x2CA: case 0x2CB:
                case 0x2CC: case 0x2CD: case 0x2CE: case 0x2CF:
                    sbc(opcode, imm(opcode)); // SBC Rd,Rn,#i
                    break;

                case 0x2D0: case 0x2D1: case 0x2D2: case 0x2D3:
                case 0x2D4: case 0x2D5: case 0x2D6: case 0x2D7:
                case 0x2D8: case 0x2D9: case 0x2DA: case 0x2DB:
                case 0x2DC: case 0x2DD: case 0x2DE: case 0x2DF:
                    sbcs(opcode, immS(opcode)); // SBCS Rd,Rn,#i
                    break;

                case 0x2E0: case 0x2E1: case 0x2E2: case 0x2E3:
                case 0x2E4: case 0x2E5: case 0x2E6: case 0x2E7:
                case 0x2E8: case 0x2E9: case 0x2EA: case 0x2EB:
                case 0x2EC: case 0x2ED: case 0x2EE: case 0x2EF:
                    rsc(opcode, imm(opcode)); // RSC Rd,Rn,#i
                    break;

                case 0x2F0: case 0x2F1: case 0x2F2: case 0x2F3:
                case 0x2F4: case 0x2F5: case 0x2F6: case 0x2F7:
                case 0x2F8: case 0x2F9: case 0x2FA: case 0x2FB:
                case 0x2FC: case 0x2FD: case 0x2FE: case 0x2FF:
                    rscs(opcode, immS(opcode)); // RSCS Rd,Rn,#i
                    break;

                case 0x310: case 0x311: case 0x312: case 0x313:
                case 0x314: case 0x315: case 0x316: case 0x317:
                case 0x318: case 0x319: case 0x31A: case 0x31B:
                case 0x31C: case 0x31D: case 0x31E: case 0x31F:
                    tst(opcode, immS(opcode)); // TST Rn,#i
                    break;

                case 0x320: case 0x321: case 0x322: case 0x323:
                case 0x324: case 0x325: case 0x326: case 0x327:
                case 0x328: case 0x329: case 0x32A: case 0x32B:
                case 0x32C: case 0x32D: case 0x32E: case 0x32F:
                    msrIc(opcode); // MSR CPSR,#i
                    break;

                case 0x330: case 0x331: case 0x332: case 0x333:
                case 0x334: case 0x335: case 0x336: case 0x337:
                case 0x338: case 0x339: case 0x33A: case 0x33B:
                case 0x33C: case 0x33D: case 0x33E: case 0x33F:
                    teq(opcode, immS(opcode)); // TEQ Rn,#i
                    break;

                case 0x350: case 0x351: case 0x352: case 0x353:
                case 0x354: case 0x355: case 0x356: case 0x357:
                case 0x358: case 0x359: case 0x35A: case 0x35B:
                case 0x35C: case 0x35D: case 0x35E: case 0x35F:
                    cmp(opcode, immS(opcode)); // CMP Rn,#i
                    break;

                case 0x360: case 0x361: case 0x362: case 0x363:
                case 0x364: case 0x365: case 0x366: case 0x367:
                case 0x368: case 0x369: case 0x36A: case 0x36B:
                case 0x36C: case 0x36D: case 0x36E: case 0x36F:
                    msrIs(opcode); // MSR SPSR,#i
                    break;

                case 0x370: case 0x371: case 0x372: case 0x373:
                case 0x374: case 0x375: case 0x376: case 0x377:
                case 0x378: case 0x379: case 0x37A: case 0x37B:
                case 0x37C: case 0x37D: case 0x37E: case 0x37F:
                    cmn(opcode, immS(opcode)); // CMN Rn,#i
                    break;

                case 0x380: case 0x381: case 0x382: case 0x383:
                case 0x384: case 0x385: case 0x386: case 0x387:
                case 0x388: case 0x389: case 0x38A: case 0x38B:
                case 0x38C: case 0x38D: case 0x38E: case 0x38F:
                    orr(opcode, imm(opcode)); // ORR Rd,Rn,#i
                    break;

                case 0x390: case 0x391: case 0x392: case 0x393:
                case 0x394: case 0x395: case 0x396: case 0x397:
                case 0x398: case 0x399: case 0x39A: case 0x39B:
                case 0x39C: case 0x39D: case 0x39E: case 0x39F:
                    orrs(opcode, immS(opcode)); // ORRS Rd,Rn,#i
                    break;

                case 0x3A0: case 0x3A1: case 0x3A2: case 0x3A3:
                case 0x3A4: case 0x3A5: case 0x3A6: case 0x3A7:
                case 0x3A8: case 0x3A9: case 0x3AA: case 0x3AB:
                case 0x3AC: case 0x3AD: case 0x3AE: case 0x3AF:
                    mov(opcode, imm(opcode)); // MOV Rd,#i
                    break;

                case 0x3B0: case 0x3B1: case 0x3B2: case 0x3B3:
                case 0x3B4: case 0x3B5: case 0x3B6: case 0x3B7:
                case 0x3B8: case 0x3B9: case 0x3BA: case 0x3BB:
                case 0x3BC: case 0x3BD: case 0x3BE: case 0x3BF:
                    movs(opcode, immS(opcode)); // MOVS Rd,#i
                    break;

                case 0x3C0: case 0x3C1: case 0x3C2: case 0x3C3:
                case 0x3C4: case 0x3C5: case 0x3C6: case 0x3C7:
                case 0x3C8: case 0x3C9: case 0x3CA: case 0x3CB:
                case 0x3CC: case 0x3CD: case 0x3CE: case 0x3CF:
                    bic(opcode, imm(opcode)); // BIC Rd,Rn,#i
                    break;

                case 0x3D0: case 0x3D1: case 0x3D2: case 0x3D3:
                case 0x3D4: case 0x3D5: case 0x3D6: case 0x3D7:
                case 0x3D8: case 0x3D9: case 0x3DA: case 0x3DB:
                case 0x3DC: case 0x3DD: case 0x3DE: case 0x3DF:
                    bics(opcode, immS(opcode)); // BICS Rd,Rn,#i
                    break;

                case 0x3E0: case 0x3E1: case 0x3E2: case 0x3E3:
                case 0x3E4: case 0x3E5: case 0x3E6: case 0x3E7:
                case 0x3E8: case 0x3E9: case 0x3EA: case 0x3EB:
                case 0x3EC: case 0x3ED: case 0x3EE: case 0x3EF:
                    mvn(opcode, imm(opcode)); // MVN Rd,#i
                    break;

                case 0x3F0: case 0x3F1: case 0x3F2: case 0x3F3:
                case 0x3F4: case 0x3F5: case 0x3F6: case 0x3F7:
                case 0x3F8: case 0x3F9: case 0x3FA: case 0x3FB:
                case 0x3FC: case 0x3FD: case 0x3FE: case 0x3FF:
                    mvns(opcode, immS(opcode)); // MVNS Rd,#i
                    break;

                case 0x400: case 0x401: case 0x402: case 0x403:
                case 0x404: case 0x405: case 0x406: case 0x407:
                case 0x408: case 0x409: case 0x40A: case 0x40B:
                case 0x40C: case 0x40D: case 0x40E: case 0x40F:
                    strPt(opcode, -ip(opcode)); // STR Rd,[Rn],-#i
                    break;

                case 0x410: case 0x411: case 0x412: case 0x413:
                case 0x414: case 0x415: case 0x416: case 0x417:
                case 0x418: case 0x419: case 0x41A: case 0x41B:
                case 0x41C: case 0x41D: case 0x41E: case 0x41F:
                    ldrPt(opcode, -ip(opcode)); // LDR Rd,[Rn],-#i
                    break;

                case 0x440: case 0x441: case 0x442: case 0x443:
                case 0x444: case 0x445: case 0x446: case 0x447:
                case 0x448: case 0x449: case 0x44A: case 0x44B:
                case 0x44C: case 0x44D: case 0x44E: case 0x44F:
                    strbPt(opcode, -ip(opcode)); // STRB Rd,[Rn],-#i
                    break;

                case 0x450: case 0x451: case 0x452: case 0x453:
                case 0x454: case 0x455: case 0x456: case 0x457:
                case 0x458: case 0x459: case 0x45A: case 0x45B:
                case 0x45C: case 0x45D: case 0x45E: case 0x45F:
                    ldrbPt(opcode, -ip(opcode)); // LDRB Rd,[Rn],-#i
                    break;

                case 0x480: case 0x481: case 0x482: case 0x483:
                case 0x484: case 0x485: case 0x486: case 0x487:
                case 0x488: case 0x489: case 0x48A: case 0x48B:
                case 0x48C: case 0x48D: case 0x48E: case 0x48F:
                    strPt(opcode, ip(opcode)); // STR Rd,[Rn],#i
                    break;

                case 0x490: case 0x491: case 0x492: case 0x493:
                case 0x494: case 0x495: case 0x496: case 0x497:
                case 0x498: case 0x499: case 0x49A: case 0x49B:
                case 0x49C: case 0x49D: case 0x49E: case 0x49F:
                    ldrPt(opcode, ip(opcode)); // LDR Rd,[Rn],#i
                    break;

                case 0x4C0: case 0x4C1: case 0x4C2: case 0x4C3:
                case 0x4C4: case 0x4C5: case 0x4C6: case 0x4C7:
                case 0x4C8: case 0x4C9: case 0x4CA: case 0x4CB:
                case 0x4CC: case 0x4CD: case 0x4CE: case 0x4CF:
                    strbPt(opcode, ip(opcode)); // STRB Rd,[Rn],#i
                    break;

                case 0x4D0: case 0x4D1: case 0x4D2: case 0x4D3:
                case 0x4D4: case 0x4D5: case 0x4D6: case 0x4D7:
                case 0x4D8: case 0x4D9: case 0x4DA: case 0x4DB:
                case 0x4DC: case 0x4DD: case 0x4DE: case 0x4DF:
                    ldrbPt(opcode, ip(opcode)); // LDRB Rd,[Rn],#i
                    break;

                case 0x500: case 0x501: case 0x502: case 0x503:
                case 0x504: case 0x505: case 0x506: case 0x507:
                case 0x508: case 0x509: case 0x50A: case 0x50B:
                case 0x50C: case 0x50D: case 0x50E: case 0x50F:
                    strOf(opcode, -ip(opcode)); // STR Rd,[Rn,-#i]
                    break;

                case 0x510: case 0x511: case 0x512: case 0x513:
                case 0x514: case 0x515: case 0x516: case 0x517:
                case 0x518: case 0x519: case 0x51A: case 0x51B:
                case 0x51C: case 0x51D: case 0x51E: case 0x51F:
                    ldrOf(opcode, -ip(opcode)); // LDR Rd,[Rn,-#i]
                    break;

                case 0x520: case 0x521: case 0x522: case 0x523:
                case 0x524: case 0x525: case 0x526: case 0x527:
                case 0x528: case 0x529: case 0x52A: case 0x52B:
                case 0x52C: case 0x52D: case 0x52E: case 0x52F:
                    strPr(opcode, -ip(opcode)); // STR Rd,[Rn,-#i]
                    break;

                case 0x530: case 0x531: case 0x532: case 0x533:
                case 0x534: case 0x535: case 0x536: case 0x537:
                case 0x538: case 0x539: case 0x53A: case 0x53B:
                case 0x53C: case 0x53D: case 0x53E: case 0x53F:
                    ldrPr(opcode, -ip(opcode)); // LDR Rd,[Rn,-#i]
                    break;

                case 0x540: case 0x541: case 0x542: case 0x543:
                case 0x544: case 0x545: case 0x546: case 0x547:
                case 0x548: case 0x549: case 0x54A: case 0x54B:
                case 0x54C: case 0x54D: case 0x54E: case 0x54F:
                    strbOf(opcode, -ip(opcode)); // STRB Rd,[Rn,-#i]
                    break;

                case 0x550: case 0x551: case 0x552: case 0x553:
                case 0x554: case 0x555: case 0x556: case 0x557:
                case 0x558: case 0x559: case 0x55A: case 0x55B:
                case 0x55C: case 0x55D: case 0x55E: case 0x55F:
                    ldrbOf(opcode, -ip(opcode)); // LDRB Rd,[Rn,-#i]
                    break;

                case 0x560: case 0x561: case 0x562: case 0x563:
                case 0x564: case 0x565: case 0x566: case 0x567:
                case 0x568: case 0x569: case 0x56A: case 0x56B:
                case 0x56C: case 0x56D: case 0x56E: case 0x56F:
                    strbPr(opcode, -ip(opcode)); // STRB Rd,[Rn,-#i]!
                    break;

                case 0x570: case 0x571: case 0x572: case 0x573:
                case 0x574: case 0x575: case 0x576: case 0x577:
                case 0x578: case 0x579: case 0x57A: case 0x57B:
                case 0x57C: case 0x57D: case 0x57E: case 0x57F:
                    ldrbPr(opcode, -ip(opcode)); // LDRB Rd,[Rn,-#i]!
                    break;

                case 0x580: case 0x581: case 0x582: case 0x583:
                case 0x584: case 0x585: case 0x586: case 0x587:
                case 0x588: case 0x589: case 0x58A: case 0x58B:
                case 0x58C: case 0x58D: case 0x58E: case 0x58F:
                    strOf(opcode, ip(opcode)); // STR Rd,[Rn,#i]
                    break;

                case 0x590: case 0x591: case 0x592: case 0x593:
                case 0x594: case 0x595: case 0x596: case 0x597:
                case 0x598: case 0x599: case 0x59A: case 0x59B:
                case 0x59C: case 0x59D: case 0x59E: case 0x59F:
                    ldrOf(opcode, ip(opcode)); // LDR Rd,[Rn,#i]
                    break;

                case 0x5A0: case 0x5A1: case 0x5A2: case 0x5A3:
                case 0x5A4: case 0x5A5: case 0x5A6: case 0x5A7:
                case 0x5A8: case 0x5A9: case 0x5AA: case 0x5AB:
                case 0x5AC: case 0x5AD: case 0x5AE: case 0x5AF:
                    strPr(opcode, ip(opcode)); // STR Rd,[Rn,#i]
                    break;

                case 0x5B0: case 0x5B1: case 0x5B2: case 0x5B3:
                case 0x5B4: case 0x5B5: case 0x5B6: case 0x5B7:
                case 0x5B8: case 0x5B9: case 0x5BA: case 0x5BB:
                case 0x5BC: case 0x5BD: case 0x5BE: case 0x5BF:
                    ldrPr(opcode, ip(opcode)); // LDR Rd,[Rn,#i]
                    break;

                case 0x5C0: case 0x5C1: case 0x5C2: case 0x5C3:
                case 0x5C4: case 0x5C5: case 0x5C6: case 0x5C7:
                case 0x5C8: case 0x5C9: case 0x5CA: case 0x5CB:
                case 0x5CC: case 0x5CD: case 0x5CE: case 0x5CF:
                    strbOf(opcode, ip(opcode)); // STRB Rd,[Rn,#i]
                    break;

                case 0x5D0: case 0x5D1: case 0x5D2: case 0x5D3:
                case 0x5D4: case 0x5D5: case 0x5D6: case 0x5D7:
                case 0x5D8: case 0x5D9: case 0x5DA: case 0x5DB:
                case 0x5DC: case 0x5DD: case 0x5DE: case 0x5DF:
                    ldrbOf(opcode, ip(opcode)); // LDRB Rd,[Rn,#i]
                    break;

                case 0x5E0: case 0x5E1: case 0x5E2: case 0x5E3:
                case 0x5E4: case 0x5E5: case 0x5E6: case 0x5E7:
                case 0x5E8: case 0x5E9: case 0x5EA: case 0x5EB:
                case 0x5EC: case 0x5ED: case 0x5EE: case 0x5EF:
                    strbPr(opcode, ip(opcode)); // STRB Rd,[Rn,#i]!
                    break;

                case 0x5F0: case 0x5F1: case 0x5F2: case 0x5F3:
                case 0x5F4: case 0x5F5: case 0x5F6: case 0x5F7:
                case 0x5F8: case 0x5F9: case 0x5FA: case 0x5FB:
                case 0x5FC: case 0x5FD: case 0x5FE: case 0x5FF:
                    ldrbPr(opcode, ip(opcode)); // LDRB Rd,[Rn,#i]!
                    break;

                case 0x600: case 0x608:
                    strPt(opcode, -rpll(opcode)); // STR Rd,[Rn],-Rm,LSL #i
                    break;

                case 0x602: case 0x60A:
                    strPt(opcode, -rplr(opcode)); // STR Rd,[Rn],-Rm,LSR #i
                    break;

                case 0x604: case 0x60C:
                    strPt(opcode, -rpar(opcode)); // STR Rd,[Rn],-Rm,ASR #i
                    break;

                case 0x606: case 0x60E:
                    strPt(opcode, -rprr(opcode)); // STR Rd,[Rn],-Rm,ROR #i
                    break;

                case 0x610: case 0x618:
                    ldrPt(opcode, -rpll(opcode)); // LDR Rd,[Rn],-Rm,LSL #i
                    break;

                case 0x612: case 0x61A:
                    ldrPt(opcode, -rplr(opcode)); // LDR Rd,[Rn],-Rm,LSR #i
                    break;

                case 0x614: case 0x61C:
                    ldrPt(opcode, -rpar(opcode)); // LDR Rd,[Rn],-Rm,ASR #i
                    break;

                case 0x616: case 0x61E:
                    ldrPt(opcode, -rprr(opcode)); // LDR Rd,[Rn],-Rm,ROR #i
                    break;

                case 0x640: case 0x648:
                    strbPt(opcode, -rpll(opcode)); // STRB Rd,[Rn],-Rm,LSL #i
                    break;

                case 0x642: case 0x64A:
                    strbPt(opcode, -rplr(opcode)); // STRB Rd,[Rn],-Rm,LSR #i
                    break;

                case 0x644: case 0x64C:
                    strbPt(opcode, -rpar(opcode)); // STRB Rd,[Rn],-Rm,ASR #i
                    break;

                case 0x646: case 0x64E:
                    strbPt(opcode, -rprr(opcode)); // STRB Rd,[Rn],-Rm,ROR #i
                    break;

                case 0x650: case 0x658:
                    ldrbPt(opcode, -rpll(opcode)); // LDRB Rd,[Rn],-Rm,LSL #i
                    break;

                case 0x652: case 0x65A:
                    ldrbPt(opcode, -rplr(opcode)); // LDRB Rd,[Rn],-Rm,LSR #i
                    break;

                case 0x654: case 0x65C:
                    ldrbPt(opcode, -rpar(opcode)); // LDRB Rd,[Rn],-Rm,ASR #i
                    break;

                case 0x656: case 0x65E:
                    ldrbPt(opcode, -rprr(opcode)); // LDRB Rd,[Rn],-Rm,ROR #i
                    break;

                case 0x680: case 0x688:
                    strPt(opcode, rpll(opcode)); // STR Rd,[Rn],Rm,LSL #i
                    break;

                case 0x682: case 0x68A:
                    strPt(opcode, rplr(opcode)); // STR Rd,[Rn],Rm,LSR #i
                    break;

                case 0x684: case 0x68C:
                    strPt(opcode, rpar(opcode)); // STR Rd,[Rn],Rm,ASR #i
                    break;

                case 0x686: case 0x68E:
                    strPt(opcode, rprr(opcode)); // STR Rd,[Rn],Rm,ROR #i
                    break;

                case 0x690: case 0x698:
                    ldrPt(opcode, rpll(opcode)); // LDR Rd,[Rn],Rm,LSL #i
                    break;

                case 0x692: case 0x69A:
                    ldrPt(opcode, rplr(opcode)); // LDR Rd,[Rn],Rm,LSR #i
                    break;

                case 0x694: case 0x69C:
                    ldrPt(opcode, rpar(opcode)); // LDR Rd,[Rn],Rm,ASR #i
                    break;

                case 0x696: case 0x69E:
                    ldrPt(opcode, rprr(opcode)); // LDR Rd,[Rn],Rm,ROR #i
                    break;

                case 0x6C0: case 0x6C8:
                    strbPt(opcode, rpll(opcode)); // STRB Rd,[Rn],Rm,LSL #i
                    break;

                case 0x6C2: case 0x6CA:
                    strbPt(opcode, rplr(opcode)); // STRB Rd,[Rn],Rm,LSR #i
                    break;

                case 0x6C4: case 0x6CC:
                    strbPt(opcode, rpar(opcode)); // STRB Rd,[Rn],Rm,ASR #i
                    break;

                case 0x6C6: case 0x6CE:
                    strbPt(opcode, rprr(opcode)); // STRB Rd,[Rn],Rm,ROR #i
                    break;

                case 0x6D0: case 0x6D8:
                    ldrbPt(opcode, rpll(opcode)); // LDRB Rd,[Rn],Rm,LSL #i
                    break;

                case 0x6D2: case 0x6DA:
                    ldrbPt(opcode, rplr(opcode)); // LDRB Rd,[Rn],Rm,LSR #i
                    break;

                case 0x6D4: case 0x6DC:
                    ldrbPt(opcode, rpar(opcode)); // LDRB Rd,[Rn],Rm,ASR #i
                    break;

                case 0x6D6: case 0x6DE:
                    ldrbPt(opcode, rprr(opcode)); // LDRB Rd,[Rn],Rm,ROR #i
                    break;

                case 0x700: case 0x708:
                    strOf(opcode, -rpll(opcode)); // STR Rd,[Rn,-Rm,LSL #i]
                    break;

                case 0x702: case 0x70A:
                    strOf(opcode, -rplr(opcode)); // STR Rd,[Rn,-Rm,LSR #i]
                    break;

                case 0x704: case 0x70C:
                    strOf(opcode, -rpar(opcode)); // STR Rd,[Rn,-Rm,ASR #i]
                    break;

                case 0x706: case 0x70E:
                    strOf(opcode, -rprr(opcode)); // STR Rd,[Rn,-Rm,ROR #i]
                    break;

                case 0x710: case 0x718:
                    ldrOf(opcode, -rpll(opcode)); // LDR Rd,[Rn,-Rm,LSL #i]
                    break;

                case 0x712: case 0x71A:
                    ldrOf(opcode, -rplr(opcode)); // LDR Rd,[Rn,-Rm,LSR #i]
                    break;

                case 0x714: case 0x71C:
                    ldrOf(opcode, -rpar(opcode)); // LDR Rd,[Rn,-Rm,ASR #i]
                    break;

                case 0x716: case 0x71E:
                    ldrOf(opcode, -rprr(opcode)); // LDR Rd,[Rn,-Rm,ROR #i]
                    break;

                case 0x720: case 0x728:
                    strPr(opcode, -rpll(opcode)); // STR Rd,[Rn,-Rm,LSL #i]!
                    break;

                case 0x722: case 0x72A:
                    strPr(opcode, -rplr(opcode)); // STR Rd,[Rn,-Rm,LSR #i]!
                    break;

                case 0x724: case 0x72C:
                    strPr(opcode, -rpar(opcode)); // STR Rd,[Rn,-Rm,ASR #i]!
                    break;

                case 0x726: case 0x72E:
                    strPr(opcode, -rprr(opcode)); // STR Rd,[Rn,-Rm,ROR #i]!
                    break;

                case 0x730: case 0x738:
                    ldrPr(opcode, -rpll(opcode)); // LDR Rd,[Rn,-Rm,LSL #i]!
                    break;

                case 0x732: case 0x73A:
                    ldrPr(opcode, -rplr(opcode)); // LDR Rd,[Rn,-Rm,LSR #i]!
                    break;

                case 0x734: case 0x73C:
                    ldrPr(opcode, -rpar(opcode)); // LDR Rd,[Rn,-Rm,ASR #i]!
                    break;

                case 0x736: case 0x73E:
                    ldrPr(opcode, -rprr(opcode)); // LDR Rd,[Rn,-Rm,ROR #i]!
                    break;

                case 0x740: case 0x748:
                    strbOf(opcode, -rpll(opcode)); // STRB Rd,[Rn,-Rm,LSL #i]
                    break;

                case 0x742: case 0x74A:
                    strbOf(opcode, -rplr(opcode)); // STRB Rd,[Rn,-Rm,LSR #i]
                    break;

                case 0x744: case 0x74C:
                    strbOf(opcode, -rpar(opcode)); // STRB Rd,[Rn,-Rm,ASR #i]
                    break;

                case 0x746: case 0x74E:
                    strbOf(opcode, -rprr(opcode)); // STRB Rd,[Rn,-Rm,ROR #i]
                    break;

                case 0x750: case 0x758:
                    ldrbOf(opcode, -rpll(opcode)); // LDRB Rd,[Rn,-Rm,LSL #i]
                    break;

                case 0x752: case 0x75A:
                    ldrbOf(opcode, -rplr(opcode)); // LDRB Rd,[Rn,-Rm,LSR #i]
                    break;

                case 0x754: case 0x75C:
                    ldrbOf(opcode, -rpar(opcode)); // LDRB Rd,[Rn,-Rm,ASR #i]
                    break;

                case 0x756: case 0x75E:
                    ldrbOf(opcode, -rprr(opcode)); // LDRB Rd,[Rn,-Rm,ROR #i]
                    break;

                case 0x760: case 0x768:
                    strbPr(opcode, -rpll(opcode)); // STRB Rd,[Rn,-Rm,LSL #i]!
                    break;

                case 0x762: case 0x76A:
                    strbPr(opcode, -rplr(opcode)); // STRB Rd,[Rn,-Rm,LSR #i]!
                    break;

                case 0x764: case 0x76C:
                    strbPr(opcode, -rpar(opcode)); // STRB Rd,[Rn,-Rm,ASR #i]!
                    break;

                case 0x766: case 0x76E:
                    strbPr(opcode, -rprr(opcode)); // STRB Rd,[Rn,-Rm,ROR #i]!
                    break;

                case 0x770: case 0x778:
                    ldrbPr(opcode, -rpll(opcode)); // LDRB Rd,[Rn,-Rm,LSL #i]!
                    break;

                case 0x772: case 0x77A:
                    ldrbPr(opcode, -rplr(opcode)); // LDRB Rd,[Rn,-Rm,LSR #i]!
                    break;

                case 0x774: case 0x77C:
                    ldrbPr(opcode, -rpar(opcode)); // LDRB Rd,[Rn,-Rm,ASR #i]!
                    break;

                case 0x776: case 0x77E:
                    ldrbPr(opcode, -rprr(opcode)); // LDRB Rd,[Rn,-Rm,ROR #i]!
                    break;

                case 0x780: case 0x788:
                    strOf(opcode, rpll(opcode)); // STR Rd,[Rn,Rm,LSL #i]
                    break;

                case 0x782: case 0x78A:
                    strOf(opcode, rplr(opcode)); // STR Rd,[Rn,Rm,LSR #i]
                    break;

                case 0x784: case 0x78C:
                    strOf(opcode, rpar(opcode)); // STR Rd,[Rn,Rm,ASR #i]
                    break;

                case 0x786: case 0x78E:
                    strOf(opcode, rprr(opcode)); // STR Rd,[Rn,Rm,ROR #i]
                    break;

                case 0x790: case 0x798:
                    ldrOf(opcode, rpll(opcode)); // LDR Rd,[Rn,Rm,LSL #i]
                    break;

                case 0x792: case 0x79A:
                    ldrOf(opcode, rplr(opcode)); // LDR Rd,[Rn,Rm,LSR #i]
                    break;

                case 0x794: case 0x79C:
                    ldrOf(opcode, rpar(opcode)); // LDR Rd,[Rn,Rm,ASR #i]
                    break;

                case 0x796: case 0x79E:
                    ldrOf(opcode, rprr(opcode)); // LDR Rd,[Rn,Rm,ROR #i]
                    break;

                case 0x7A0: case 0x7A8:
                    strPr(opcode, rpll(opcode)); // STR Rd,[Rn,Rm,LSL #i]!
                    break;

                case 0x7A2: case 0x7AA:
                    strPr(opcode, rplr(opcode)); // STR Rd,[Rn,Rm,LSR #i]!
                    break;

                case 0x7A4: case 0x7AC:
                    strPr(opcode, rpar(opcode)); // STR Rd,[Rn,Rm,ASR #i]!
                    break;

                case 0x7A6: case 0x7AE:
                    strPr(opcode, rprr(opcode)); // STR Rd,[Rn,Rm,ROR #i]!
                    break;

                case 0x7B0: case 0x7B8:
                    ldrPr(opcode, rpll(opcode)); // LDR Rd,[Rn,Rm,LSL #i]!
                    break;

                case 0x7B2: case 0x7BA:
                    ldrPr(opcode, rplr(opcode)); // LDR Rd,[Rn,Rm,LSR #i]!
                    break;

                case 0x7B4: case 0x7BC:
                    ldrPr(opcode, rpar(opcode)); // LDR Rd,[Rn,Rm,ASR #i]!
                    break;

                case 0x7B6: case 0x7BE:
                    ldrPr(opcode, rprr(opcode)); // LDR Rd,[Rn,Rm,ROR #i]!
                    break;

                case 0x7C0: case 0x7C8:
                    strbOf(opcode, rpll(opcode)); // STRB Rd,[Rn,Rm,LSL #i]
                    break;

                case 0x7C2: case 0x7CA:
                    strbOf(opcode, rplr(opcode)); // STRB Rd,[Rn,Rm,LSR #i]
                    break;

                case 0x7C4: case 0x7CC:
                    strbOf(opcode, rpar(opcode)); // STRB Rd,[Rn,Rm,ASR #i]
                    break;

                case 0x7C6: case 0x7CE:
                    strbOf(opcode, rprr(opcode)); // STRB Rd,[Rn,Rm,ROR #i]
                    break;

                case 0x7D0: case 0x7D8:
                    ldrbOf(opcode, rpll(opcode)); // LDRB Rd,[Rn,Rm,LSL #i]
                    break;

                case 0x7D2: case 0x7DA:
                    ldrbOf(opcode, rplr(opcode)); // LDRB Rd,[Rn,Rm,LSR #i]
                    break;

                case 0x7D4: case 0x7DC:
                    ldrbOf(opcode, rpar(opcode)); // LDRB Rd,[Rn,Rm,ASR #i]
                    break;

                case 0x7D6: case 0x7DE:
                    ldrbOf(opcode, rprr(opcode)); // LDRB Rd,[Rn,Rm,ROR #i]
                    break;

                case 0x7E0: case 0x7E8:
                    strbPr(opcode, rpll(opcode)); // STRB Rd,[Rn,Rm,LSL #i]!
                    break;

                case 0x7E2: case 0x7EA:
                    strbPr(opcode, rplr(opcode)); // STRB Rd,[Rn,Rm,LSR #i]!
                    break;

                case 0x7E4: case 0x7EC:
                    strbPr(opcode, rpar(opcode)); // STRB Rd,[Rn,Rm,ASR #i]!
                    break;

                case 0x7E6: case 0x7EE:
                    strbPr(opcode, rprr(opcode)); // STRB Rd,[Rn,Rm,ROR #i]!
                    break;

                case 0x7F0: case 0x7F8:
                    ldrbPr(opcode, rpll(opcode)); // LDRB Rd,[Rn,Rm,LSL #i]!
                    break;

                case 0x7F2: case 0x7FA:
                    ldrbPr(opcode, rplr(opcode)); // LDRB Rd,[Rn,Rm,LSR #i]!
                    break;

                case 0x7F4: case 0x7FC:
                    ldrbPr(opcode, rpar(opcode)); // LDRB Rd,[Rn,Rm,ASR #i]!
                    break;

                case 0x7F6: case 0x7FE:
                    ldrbPr(opcode, rprr(opcode)); // LDRB Rd,[Rn,Rm,ROR #i]!
                    break;

                case 0x800: case 0x801: case 0x802: case 0x803:
                case 0x804: case 0x805: case 0x806: case 0x807:
                case 0x808: case 0x809: case 0x80A: case 0x80B:
                case 0x80C: case 0x80D: case 0x80E: case 0x80F:
                    stmda(opcode); // STMDA Rn, <Rlist>
                    break;

                case 0x810: case 0x811: case 0x812: case 0x813:
                case 0x814: case 0x815: case 0x816: case 0x817:
                case 0x818: case 0x819: case 0x81A: case 0x81B:
                case 0x81C: case 0x81D: case 0x81E: case 0x81F:
                    ldmda(opcode); // LDMDA Rn, <Rlist>
                    break;

                case 0x820: case 0x821: case 0x822: case 0x823:
                case 0x824: case 0x825: case 0x826: case 0x827:
                case 0x828: case 0x829: case 0x82A: case 0x82B:
                case 0x82C: case 0x82D: case 0x82E: case 0x82F:
                    stmdaW(opcode); // STMDA Rn!, <Rlist>
                    break;

                case 0x830: case 0x831: case 0x832: case 0x833:
                case 0x834: case 0x835: case 0x836: case 0x837:
                case 0x838: case 0x839: case 0x83A: case 0x83B:
                case 0x83C: case 0x83D: case 0x83E: case 0x83F:
                    ldmdaW(opcode); // LDMDA Rn!, <Rlist>
                    break;

                case 0x840: case 0x841: case 0x842: case 0x843:
                case 0x844: case 0x845: case 0x846: case 0x847:
                case 0x848: case 0x849: case 0x84A: case 0x84B:
                case 0x84C: case 0x84D: case 0x84E: case 0x84F:
                    stmdaU(opcode); // STMDA Rn, <Rlist>^
                    break;

                case 0x850: case 0x851: case 0x852: case 0x853:
                case 0x854: case 0x855: case 0x856: case 0x857:
                case 0x858: case 0x859: case 0x85A: case 0x85B:
                case 0x85C: case 0x85D: case 0x85E: case 0x85F:
                    ldmdaU(opcode); // LDMDA Rn, <Rlist>^
                    break;

                case 0x860: case 0x861: case 0x862: case 0x863:
                case 0x864: case 0x865: case 0x866: case 0x867:
                case 0x868: case 0x869: case 0x86A: case 0x86B:
                case 0x86C: case 0x86D: case 0x86E: case 0x86F:
                    stmdaUW(opcode); // STMDA Rn!, <Rlist>^
                    break;

                case 0x870: case 0x871: case 0x872: case 0x873:
                case 0x874: case 0x875: case 0x876: case 0x877:
                case 0x878: case 0x879: case 0x87A: case 0x87B:
                case 0x87C: case 0x87D: case 0x87E: case 0x87F:
                    ldmdaUW(opcode); // LDMDA Rn!, <Rlist>^
                    break;

                case 0x880: case 0x881: case 0x882: case 0x883:
                case 0x884: case 0x885: case 0x886: case 0x887:
                case 0x888: case 0x889: case 0x88A: case 0x88B:
                case 0x88C: case 0x88D: case 0x88E: case 0x88F:
                    stmia(opcode); // STMIA Rn, <Rlist>
                    break;

                case 0x890: case 0x891: case 0x892: case 0x893:
                case 0x894: case 0x895: case 0x896: case 0x897:
                case 0x898: case 0x899: case 0x89A: case 0x89B:
                case 0x89C: case 0x89D: case 0x89E: case 0x89F:
                    ldmia(opcode); // LDMIA Rn, <Rlist>
                    break;

                case 0x8A0: case 0x8A1: case 0x8A2: case 0x8A3:
                case 0x8A4: case 0x8A5: case 0x8A6: case 0x8A7:
                case 0x8A8: case 0x8A9: case 0x8AA: case 0x8AB:
                case 0x8AC: case 0x8AD: case 0x8AE: case 0x8AF:
                    stmiaW(opcode); // STMIA Rn!, <Rlist>
                    break;

                case 0x8B0: case 0x8B1: case 0x8B2: case 0x8B3:
                case 0x8B4: case 0x8B5: case 0x8B6: case 0x8B7:
                case 0x8B8: case 0x8B9: case 0x8BA: case 0x8BB:
                case 0x8BC: case 0x8BD: case 0x8BE: case 0x8BF:
                    ldmiaW(opcode); // LDMIA Rn!, <Rlist>
                    break;

                case 0x8C0: case 0x8C1: case 0x8C2: case 0x8C3:
                case 0x8C4: case 0x8C5: case 0x8C6: case 0x8C7:
                case 0x8C8: case 0x8C9: case 0x8CA: case 0x8CB:
                case 0x8CC: case 0x8CD: case 0x8CE: case 0x8CF:
                    stmiaU(opcode); // STMIA Rn, <Rlist>^
                    break;

                case 0x8D0: case 0x8D1: case 0x8D2: case 0x8D3:
                case 0x8D4: case 0x8D5: case 0x8D6: case 0x8D7:
                case 0x8D8: case 0x8D9: case 0x8DA: case 0x8DB:
                case 0x8DC: case 0x8DD: case 0x8DE: case 0x8DF:
                    ldmiaU(opcode); // LDMIA Rn, <Rlist>^
                    break;

                case 0x8E0: case 0x8E1: case 0x8E2: case 0x8E3:
                case 0x8E4: case 0x8E5: case 0x8E6: case 0x8E7:
                case 0x8E8: case 0x8E9: case 0x8EA: case 0x8EB:
                case 0x8EC: case 0x8ED: case 0x8EE: case 0x8EF:
                    stmiaUW(opcode); // STMIA Rn!, <Rlist>^
                    break;

                case 0x8F0: case 0x8F1: case 0x8F2: case 0x8F3:
                case 0x8F4: case 0x8F5: case 0x8F6: case 0x8F7:
                case 0x8F8: case 0x8F9: case 0x8FA: case 0x8FB:
                case 0x8FC: case 0x8FD: case 0x8FE: case 0x8FF:
                    ldmiaUW(opcode); // LDMIA Rn!, <Rlist>^
                    break;

                case 0x900: case 0x901: case 0x902: case 0x903:
                case 0x904: case 0x905: case 0x906: case 0x907:
                case 0x908: case 0x909: case 0x90A: case 0x90B:
                case 0x90C: case 0x90D: case 0x90E: case 0x90F:
                    stmdb(opcode); // STMDB Rn, <Rlist>
                    break;

                case 0x910: case 0x911: case 0x912: case 0x913:
                case 0x914: case 0x915: case 0x916: case 0x917:
                case 0x918: case 0x919: case 0x91A: case 0x91B:
                case 0x91C: case 0x91D: case 0x91E: case 0x91F:
                    ldmdb(opcode); // LDMDB Rn, <Rlist>
                    break;

                case 0x920: case 0x921: case 0x922: case 0x923:
                case 0x924: case 0x925: case 0x926: case 0x927:
                case 0x928: case 0x929: case 0x92A: case 0x92B:
                case 0x92C: case 0x92D: case 0x92E: case 0x92F:
                    stmdbW(opcode); // STMDB Rn!, <Rlist>
                    break;

                case 0x930: case 0x931: case 0x932: case 0x933:
                case 0x934: case 0x935: case 0x936: case 0x937:
                case 0x938: case 0x939: case 0x93A: case 0x93B:
                case 0x93C: case 0x93D: case 0x93E: case 0x93F:
                    ldmdbW(opcode); // LDMDB Rn!, <Rlist>
                    break;

                case 0x940: case 0x941: case 0x942: case 0x943:
                case 0x944: case 0x945: case 0x946: case 0x947:
                case 0x948: case 0x949: case 0x94A: case 0x94B:
                case 0x94C: case 0x94D: case 0x94E: case 0x94F:
                    stmdbU(opcode); // STMDB Rn, <Rlist>^
                    break;

                case 0x950: case 0x951: case 0x952: case 0x953:
                case 0x954: case 0x955: case 0x956: case 0x957:
                case 0x958: case 0x959: case 0x95A: case 0x95B:
                case 0x95C: case 0x95D: case 0x95E: case 0x95F:
                    ldmdbU(opcode); // LDMDB Rn, <Rlist>^
                    break;

                case 0x960: case 0x961: case 0x962: case 0x963:
                case 0x964: case 0x965: case 0x966: case 0x967:
                case 0x968: case 0x969: case 0x96A: case 0x96B:
                case 0x96C: case 0x96D: case 0x96E: case 0x96F:
                    stmdbUW(opcode); // STMDB Rn!, <Rlist>^
                    break;

                case 0x970: case 0x971: case 0x972: case 0x973:
                case 0x974: case 0x975: case 0x976: case 0x977:
                case 0x978: case 0x979: case 0x97A: case 0x97B:
                case 0x97C: case 0x97D: case 0x97E: case 0x97F:
                    ldmdbUW(opcode); // LDMDB Rn!, <Rlist>^
                    break;

                case 0x980: case 0x981: case 0x982: case 0x983:
                case 0x984: case 0x985: case 0x986: case 0x987:
                case 0x988: case 0x989: case 0x98A: case 0x98B:
                case 0x98C: case 0x98D: case 0x98E: case 0x98F:
                    stmib(opcode); // STMIB Rn, <Rlist>
                    break;

                case 0x990: case 0x991: case 0x992: case 0x993:
                case 0x994: case 0x995: case 0x996: case 0x997:
                case 0x998: case 0x999: case 0x99A: case 0x99B:
                case 0x99C: case 0x99D: case 0x99E: case 0x99F:
                    ldmib(opcode); // LDMIB Rn, <Rlist>
                    break;

                case 0x9A0: case 0x9A1: case 0x9A2: case 0x9A3:
                case 0x9A4: case 0x9A5: case 0x9A6: case 0x9A7:
                case 0x9A8: case 0x9A9: case 0x9AA: case 0x9AB:
                case 0x9AC: case 0x9AD: case 0x9AE: case 0x9AF:
                    stmibW(opcode); // STMIB Rn!, <Rlist>
                    break;

                case 0x9B0: case 0x9B1: case 0x9B2: case 0x9B3:
                case 0x9B4: case 0x9B5: case 0x9B6: case 0x9B7:
                case 0x9B8: case 0x9B9: case 0x9BA: case 0x9BB:
                case 0x9BC: case 0x9BD: case 0x9BE: case 0x9BF:
                    ldmibW(opcode); // LDMIB Rn!, <Rlist>
                    break;

                case 0x9C0: case 0x9C1: case 0x9C2: case 0x9C3:
                case 0x9C4: case 0x9C5: case 0x9C6: case 0x9C7:
                case 0x9C8: case 0x9C9: case 0x9CA: case 0x9CB:
                case 0x9CC: case 0x9CD: case 0x9CE: case 0x9CF:
                    stmibU(opcode); // STMIB Rn, <Rlist>^
                    break;

                case 0x9D0: case 0x9D1: case 0x9D2: case 0x9D3:
                case 0x9D4: case 0x9D5: case 0x9D6: case 0x9D7:
                case 0x9D8: case 0x9D9: case 0x9DA: case 0x9DB:
                case 0x9DC: case 0x9DD: case 0x9DE: case 0x9DF:
                    ldmibU(opcode); // LDMIB Rn, <Rlist>^
                    break;

                case 0x9E0: case 0x9E1: case 0x9E2: case 0x9E3:
                case 0x9E4: case 0x9E5: case 0x9E6: case 0x9E7:
                case 0x9E8: case 0x9E9: case 0x9EA: case 0x9EB:
                case 0x9EC: case 0x9ED: case 0x9EE: case 0x9EF:
                    stmibUW(opcode); // STMIB Rn!, <Rlist>^
                    break;

                case 0x9F0: case 0x9F1: case 0x9F2: case 0x9F3:
                case 0x9F4: case 0x9F5: case 0x9F6: case 0x9F7:
                case 0x9F8: case 0x9F9: case 0x9FA: case 0x9FB:
                case 0x9FC: case 0x9FD: case 0x9FE: case 0x9FF:
                    ldmibUW(opcode); // LDMIB Rn!, <Rlist>^
                    break;

                case 0xA00: case 0xA01: case 0xA02: case 0xA03:
                case 0xA04: case 0xA05: case 0xA06: case 0xA07:
                case 0xA08: case 0xA09: case 0xA0A: case 0xA0B:
                case 0xA0C: case 0xA0D: case 0xA0E: case 0xA0F:
                case 0xA10: case 0xA11: case 0xA12: case 0xA13:
                case 0xA14: case 0xA15: case 0xA16: case 0xA17:
                case 0xA18: case 0xA19: case 0xA1A: case 0xA1B:
                case 0xA1C: case 0xA1D: case 0xA1E: case 0xA1F:
                case 0xA20: case 0xA21: case 0xA22: case 0xA23:
                case 0xA24: case 0xA25: case 0xA26: case 0xA27:
                case 0xA28: case 0xA29: case 0xA2A: case 0xA2B:
                case 0xA2C: case 0xA2D: case 0xA2E: case 0xA2F:
                case 0xA30: case 0xA31: case 0xA32: case 0xA33:
                case 0xA34: case 0xA35: case 0xA36: case 0xA37:
                case 0xA38: case 0xA39: case 0xA3A: case 0xA3B:
                case 0xA3C: case 0xA3D: case 0xA3E: case 0xA3F:
                case 0xA40: case 0xA41: case 0xA42: case 0xA43:
                case 0xA44: case 0xA45: case 0xA46: case 0xA47:
                case 0xA48: case 0xA49: case 0xA4A: case 0xA4B:
                case 0xA4C: case 0xA4D: case 0xA4E: case 0xA4F:
                case 0xA50: case 0xA51: case 0xA52: case 0xA53:
                case 0xA54: case 0xA55: case 0xA56: case 0xA57:
                case 0xA58: case 0xA59: case 0xA5A: case 0xA5B:
                case 0xA5C: case 0xA5D: case 0xA5E: case 0xA5F:
                case 0xA60: case 0xA61: case 0xA62: case 0xA63:
                case 0xA64: case 0xA65: case 0xA66: case 0xA67:
                case 0xA68: case 0xA69: case 0xA6A: case 0xA6B:
                case 0xA6C: case 0xA6D: case 0xA6E: case 0xA6F:
                case 0xA70: case 0xA71: case 0xA72: case 0xA73:
                case 0xA74: case 0xA75: case 0xA76: case 0xA77:
                case 0xA78: case 0xA79: case 0xA7A: case 0xA7B:
                case 0xA7C: case 0xA7D: case 0xA7E: case 0xA7F:
                case 0xA80: case 0xA81: case 0xA82: case 0xA83:
                case 0xA84: case 0xA85: case 0xA86: case 0xA87:
                case 0xA88: case 0xA89: case 0xA8A: case 0xA8B:
                case 0xA8C: case 0xA8D: case 0xA8E: case 0xA8F:
                case 0xA90: case 0xA91: case 0xA92: case 0xA93:
                case 0xA94: case 0xA95: case 0xA96: case 0xA97:
                case 0xA98: case 0xA99: case 0xA9A: case 0xA9B:
                case 0xA9C: case 0xA9D: case 0xA9E: case 0xA9F:
                case 0xAA0: case 0xAA1: case 0xAA2: case 0xAA3:
                case 0xAA4: case 0xAA5: case 0xAA6: case 0xAA7:
                case 0xAA8: case 0xAA9: case 0xAAA: case 0xAAB:
                case 0xAAC: case 0xAAD: case 0xAAE: case 0xAAF:
                case 0xAB0: case 0xAB1: case 0xAB2: case 0xAB3:
                case 0xAB4: case 0xAB5: case 0xAB6: case 0xAB7:
                case 0xAB8: case 0xAB9: case 0xABA: case 0xABB:
                case 0xABC: case 0xABD: case 0xABE: case 0xABF:
                case 0xAC0: case 0xAC1: case 0xAC2: case 0xAC3:
                case 0xAC4: case 0xAC5: case 0xAC6: case 0xAC7:
                case 0xAC8: case 0xAC9: case 0xACA: case 0xACB:
                case 0xACC: case 0xACD: case 0xACE: case 0xACF:
                case 0xAD0: case 0xAD1: case 0xAD2: case 0xAD3:
                case 0xAD4: case 0xAD5: case 0xAD6: case 0xAD7:
                case 0xAD8: case 0xAD9: case 0xADA: case 0xADB:
                case 0xADC: case 0xADD: case 0xADE: case 0xADF:
                case 0xAE0: case 0xAE1: case 0xAE2: case 0xAE3:
                case 0xAE4: case 0xAE5: case 0xAE6: case 0xAE7:
                case 0xAE8: case 0xAE9: case 0xAEA: case 0xAEB:
                case 0xAEC: case 0xAED: case 0xAEE: case 0xAEF:
                case 0xAF0: case 0xAF1: case 0xAF2: case 0xAF3:
                case 0xAF4: case 0xAF5: case 0xAF6: case 0xAF7:
                case 0xAF8: case 0xAF9: case 0xAFA: case 0xAFB:
                case 0xAFC: case 0xAFD: case 0xAFE: case 0xAFF:
                    if ((opcode & 0xF0000000) != 0xF0000000)
                        b(opcode); // B label
                    else
                        blx(opcode); // BLX label
                    break;

                case 0xB00: case 0xB01: case 0xB02: case 0xB03:
                case 0xB04: case 0xB05: case 0xB06: case 0xB07:
                case 0xB08: case 0xB09: case 0xB0A: case 0xB0B:
                case 0xB0C: case 0xB0D: case 0xB0E: case 0xB0F:
                case 0xB10: case 0xB11: case 0xB12: case 0xB13:
                case 0xB14: case 0xB15: case 0xB16: case 0xB17:
                case 0xB18: case 0xB19: case 0xB1A: case 0xB1B:
                case 0xB1C: case 0xB1D: case 0xB1E: case 0xB1F:
                case 0xB20: case 0xB21: case 0xB22: case 0xB23:
                case 0xB24: case 0xB25: case 0xB26: case 0xB27:
                case 0xB28: case 0xB29: case 0xB2A: case 0xB2B:
                case 0xB2C: case 0xB2D: case 0xB2E: case 0xB2F:
                case 0xB30: case 0xB31: case 0xB32: case 0xB33:
                case 0xB34: case 0xB35: case 0xB36: case 0xB37:
                case 0xB38: case 0xB39: case 0xB3A: case 0xB3B:
                case 0xB3C: case 0xB3D: case 0xB3E: case 0xB3F:
                case 0xB40: case 0xB41: case 0xB42: case 0xB43:
                case 0xB44: case 0xB45: case 0xB46: case 0xB47:
                case 0xB48: case 0xB49: case 0xB4A: case 0xB4B:
                case 0xB4C: case 0xB4D: case 0xB4E: case 0xB4F:
                case 0xB50: case 0xB51: case 0xB52: case 0xB53:
                case 0xB54: case 0xB55: case 0xB56: case 0xB57:
                case 0xB58: case 0xB59: case 0xB5A: case 0xB5B:
                case 0xB5C: case 0xB5D: case 0xB5E: case 0xB5F:
                case 0xB60: case 0xB61: case 0xB62: case 0xB63:
                case 0xB64: case 0xB65: case 0xB66: case 0xB67:
                case 0xB68: case 0xB69: case 0xB6A: case 0xB6B:
                case 0xB6C: case 0xB6D: case 0xB6E: case 0xB6F:
                case 0xB70: case 0xB71: case 0xB72: case 0xB73:
                case 0xB74: case 0xB75: case 0xB76: case 0xB77:
                case 0xB78: case 0xB79: case 0xB7A: case 0xB7B:
                case 0xB7C: case 0xB7D: case 0xB7E: case 0xB7F:
                case 0xB80: case 0xB81: case 0xB82: case 0xB83:
                case 0xB84: case 0xB85: case 0xB86: case 0xB87:
                case 0xB88: case 0xB89: case 0xB8A: case 0xB8B:
                case 0xB8C: case 0xB8D: case 0xB8E: case 0xB8F:
                case 0xB90: case 0xB91: case 0xB92: case 0xB93:
                case 0xB94: case 0xB95: case 0xB96: case 0xB97:
                case 0xB98: case 0xB99: case 0xB9A: case 0xB9B:
                case 0xB9C: case 0xB9D: case 0xB9E: case 0xB9F:
                case 0xBA0: case 0xBA1: case 0xBA2: case 0xBA3:
                case 0xBA4: case 0xBA5: case 0xBA6: case 0xBA7:
                case 0xBA8: case 0xBA9: case 0xBAA: case 0xBAB:
                case 0xBAC: case 0xBAD: case 0xBAE: case 0xBAF:
                case 0xBB0: case 0xBB1: case 0xBB2: case 0xBB3:
                case 0xBB4: case 0xBB5: case 0xBB6: case 0xBB7:
                case 0xBB8: case 0xBB9: case 0xBBA: case 0xBBB:
                case 0xBBC: case 0xBBD: case 0xBBE: case 0xBBF:
                case 0xBC0: case 0xBC1: case 0xBC2: case 0xBC3:
                case 0xBC4: case 0xBC5: case 0xBC6: case 0xBC7:
                case 0xBC8: case 0xBC9: case 0xBCA: case 0xBCB:
                case 0xBCC: case 0xBCD: case 0xBCE: case 0xBCF:
                case 0xBD0: case 0xBD1: case 0xBD2: case 0xBD3:
                case 0xBD4: case 0xBD5: case 0xBD6: case 0xBD7:
                case 0xBD8: case 0xBD9: case 0xBDA: case 0xBDB:
                case 0xBDC: case 0xBDD: case 0xBDE: case 0xBDF:
                case 0xBE0: case 0xBE1: case 0xBE2: case 0xBE3:
                case 0xBE4: case 0xBE5: case 0xBE6: case 0xBE7:
                case 0xBE8: case 0xBE9: case 0xBEA: case 0xBEB:
                case 0xBEC: case 0xBED: case 0xBEE: case 0xBEF:
                case 0xBF0: case 0xBF1: case 0xBF2: case 0xBF3:
                case 0xBF4: case 0xBF5: case 0xBF6: case 0xBF7:
                case 0xBF8: case 0xBF9: case 0xBFA: case 0xBFB:
                case 0xBFC: case 0xBFD: case 0xBFE: case 0xBFF:
                    if ((opcode & 0xF0000000) != 0xF0000000)
                        bl(opcode); // BL label
                    else
                        blx(opcode); // BLX label
                    break;

                case 0xE01: case 0xE03: case 0xE05: case 0xE07:
                case 0xE09: case 0xE0B: case 0xE0D: case 0xE0F:
                case 0xE21: case 0xE23: case 0xE25: case 0xE27:
                case 0xE29: case 0xE2B: case 0xE2D: case 0xE2F:
                case 0xE41: case 0xE43: case 0xE45: case 0xE47:
                case 0xE49: case 0xE4B: case 0xE4D: case 0xE4F:
                case 0xE61: case 0xE63: case 0xE65: case 0xE67:
                case 0xE69: case 0xE6B: case 0xE6D: case 0xE6F:
                case 0xE81: case 0xE83: case 0xE85: case 0xE87:
                case 0xE89: case 0xE8B: case 0xE8D: case 0xE8F:
                case 0xEA1: case 0xEA3: case 0xEA5: case 0xEA7:
                case 0xEA9: case 0xEAB: case 0xEAD: case 0xEAF:
                case 0xEC1: case 0xEC3: case 0xEC5: case 0xEC7:
                case 0xEC9: case 0xECB: case 0xECD: case 0xECF:
                case 0xEE1: case 0xEE3: case 0xEE5: case 0xEE7:
                case 0xEE9: case 0xEEB: case 0xEED: case 0xEEF:
                    mcr(opcode); // MCR Pn,<cpopc>,Rd,Cn,Cm,<cp>
                    break;

                case 0xE11: case 0xE13: case 0xE15: case 0xE17:
                case 0xE19: case 0xE1B: case 0xE1D: case 0xE1F:
                case 0xE31: case 0xE33: case 0xE35: case 0xE37:
                case 0xE39: case 0xE3B: case 0xE3D: case 0xE3F:
                case 0xE51: case 0xE53: case 0xE55: case 0xE57:
                case 0xE59: case 0xE5B: case 0xE5D: case 0xE5F:
                case 0xE71: case 0xE73: case 0xE75: case 0xE77:
                case 0xE79: case 0xE7B: case 0xE7D: case 0xE7F:
                case 0xE91: case 0xE93: case 0xE95: case 0xE97:
                case 0xE99: case 0xE9B: case 0xE9D: case 0xE9F:
                case 0xEB1: case 0xEB3: case 0xEB5: case 0xEB7:
                case 0xEB9: case 0xEBB: case 0xEBD: case 0xEBF:
                case 0xED1: case 0xED3: case 0xED5: case 0xED7:
                case 0xED9: case 0xEDB: case 0xEDD: case 0xEDF:
                case 0xEF1: case 0xEF3: case 0xEF5: case 0xEF7:
                case 0xEF9: case 0xEFB: case 0xEFD: case 0xEFF:
                    mrc(opcode); // MRC Pn,<cpopc>,Rd,Cn,Cm,<cp>
                    break;

                case 0xF00: case 0xF01: case 0xF02: case 0xF03:
                case 0xF04: case 0xF05: case 0xF06: case 0xF07:
                case 0xF08: case 0xF09: case 0xF0A: case 0xF0B:
                case 0xF0C: case 0xF0D: case 0xF0E: case 0xF0F:
                case 0xF10: case 0xF11: case 0xF12: case 0xF13:
                case 0xF14: case 0xF15: case 0xF16: case 0xF17:
                case 0xF18: case 0xF19: case 0xF1A: case 0xF1B:
                case 0xF1C: case 0xF1D: case 0xF1E: case 0xF1F:
                case 0xF20: case 0xF21: case 0xF22: case 0xF23:
                case 0xF24: case 0xF25: case 0xF26: case 0xF27:
                case 0xF28: case 0xF29: case 0xF2A: case 0xF2B:
                case 0xF2C: case 0xF2D: case 0xF2E: case 0xF2F:
                case 0xF30: case 0xF31: case 0xF32: case 0xF33:
                case 0xF34: case 0xF35: case 0xF36: case 0xF37:
                case 0xF38: case 0xF39: case 0xF3A: case 0xF3B:
                case 0xF3C: case 0xF3D: case 0xF3E: case 0xF3F:
                case 0xF40: case 0xF41: case 0xF42: case 0xF43:
                case 0xF44: case 0xF45: case 0xF46: case 0xF47:
                case 0xF48: case 0xF49: case 0xF4A: case 0xF4B:
                case 0xF4C: case 0xF4D: case 0xF4E: case 0xF4F:
                case 0xF50: case 0xF51: case 0xF52: case 0xF53:
                case 0xF54: case 0xF55: case 0xF56: case 0xF57:
                case 0xF58: case 0xF59: case 0xF5A: case 0xF5B:
                case 0xF5C: case 0xF5D: case 0xF5E: case 0xF5F:
                case 0xF60: case 0xF61: case 0xF62: case 0xF63:
                case 0xF64: case 0xF65: case 0xF66: case 0xF67:
                case 0xF68: case 0xF69: case 0xF6A: case 0xF6B:
                case 0xF6C: case 0xF6D: case 0xF6E: case 0xF6F:
                case 0xF70: case 0xF71: case 0xF72: case 0xF73:
                case 0xF74: case 0xF75: case 0xF76: case 0xF77:
                case 0xF78: case 0xF79: case 0xF7A: case 0xF7B:
                case 0xF7C: case 0xF7D: case 0xF7E: case 0xF7F:
                case 0xF80: case 0xF81: case 0xF82: case 0xF83:
                case 0xF84: case 0xF85: case 0xF86: case 0xF87:
                case 0xF88: case 0xF89: case 0xF8A: case 0xF8B:
                case 0xF8C: case 0xF8D: case 0xF8E: case 0xF8F:
                case 0xF90: case 0xF91: case 0xF92: case 0xF93:
                case 0xF94: case 0xF95: case 0xF96: case 0xF97:
                case 0xF98: case 0xF99: case 0xF9A: case 0xF9B:
                case 0xF9C: case 0xF9D: case 0xF9E: case 0xF9F:
                case 0xFA0: case 0xFA1: case 0xFA2: case 0xFA3:
                case 0xFA4: case 0xFA5: case 0xFA6: case 0xFA7:
                case 0xFA8: case 0xFA9: case 0xFAA: case 0xFAB:
                case 0xFAC: case 0xFAD: case 0xFAE: case 0xFAF:
                case 0xFB0: case 0xFB1: case 0xFB2: case 0xFB3:
                case 0xFB4: case 0xFB5: case 0xFB6: case 0xFB7:
                case 0xFB8: case 0xFB9: case 0xFBA: case 0xFBB:
                case 0xFBC: case 0xFBD: case 0xFBE: case 0xFBF:
                case 0xFC0: case 0xFC1: case 0xFC2: case 0xFC3:
                case 0xFC4: case 0xFC5: case 0xFC6: case 0xFC7:
                case 0xFC8: case 0xFC9: case 0xFCA: case 0xFCB:
                case 0xFCC: case 0xFCD: case 0xFCE: case 0xFCF:
                case 0xFD0: case 0xFD1: case 0xFD2: case 0xFD3:
                case 0xFD4: case 0xFD5: case 0xFD6: case 0xFD7:
                case 0xFD8: case 0xFD9: case 0xFDA: case 0xFDB:
                case 0xFDC: case 0xFDD: case 0xFDE: case 0xFDF:
                case 0xFE0: case 0xFE1: case 0xFE2: case 0xFE3:
                case 0xFE4: case 0xFE5: case 0xFE6: case 0xFE7:
                case 0xFE8: case 0xFE9: case 0xFEA: case 0xFEB:
                case 0xFEC: case 0xFED: case 0xFEE: case 0xFEF:
                case 0xFF0: case 0xFF1: case 0xFF2: case 0xFF3:
                case 0xFF4: case 0xFF5: case 0xFF6: case 0xFF7:
                case 0xFF8: case 0xFF9: case 0xFFA: case 0xFFB:
                case 0xFFC: case 0xFFD: case 0xFFE: case 0xFFF:
                    swi(); // SWI #i
                    break;

                default:
                    printf("Unknown ARM%d ARM opcode: 0x%X\n", ((cpu == 0) ? 9 : 7), opcode);
                    break;
            }
        }

        // Increment the program counter
        *registers[15] += 4;
    }
}

void Interpreter::halt()
{
    // Halt the CPU
    halted = true;

    // The ARM9 needs the IME to be set when halting, or else it locks up
    // It makes more sense to check this when unhalting, but doing so causes mysterious slowdown
    if (cpu == 0 && !ime)
        ie = 0;
}

void Interpreter::sendInterrupt(int bit)
{
    // Set the interrupt's request bit
    irf |= BIT(bit);

    // Unhalt the CPU if the requested interrupt is enabled
    if (ie & irf)
        halted = false;
}

bool Interpreter::condition(uint32_t opcode)
{
    // Evaluate the condition of an ARM opcode
    switch ((opcode & 0xF0000000) >> 28)
    {
        case 0x0: return  (cpsr & BIT(30));                                               // EQ
        case 0x1: return !(cpsr & BIT(30));                                               // NE
        case 0x2: return  (cpsr & BIT(29));                                               // CS
        case 0x3: return !(cpsr & BIT(29));                                               // CC
        case 0x4: return  (cpsr & BIT(31));                                               // MI
        case 0x5: return !(cpsr & BIT(31));                                               // PL
        case 0x6: return  (cpsr & BIT(28));                                               // VS
        case 0x7: return !(cpsr & BIT(28));                                               // VC
        case 0x8: return  (cpsr & BIT(29)) && !(cpsr & BIT(30));                          // HI
        case 0x9: return !(cpsr & BIT(29)) ||  (cpsr & BIT(30));                          // LS
        case 0xA: return  (cpsr & BIT(31)) ==  (cpsr & BIT(28)) << 3;                     // GE
        case 0xB: return  (cpsr & BIT(31)) !=  (cpsr & BIT(28)) << 3;                     // LT
        case 0xC: return !(cpsr & BIT(30)) &&  (cpsr & BIT(31)) == (cpsr & BIT(28)) << 3; // GT
        case 0xD: return  (cpsr & BIT(30)) ||  (cpsr & BIT(31)) != (cpsr & BIT(28)) << 3; // LE
        case 0xE: return true;                                                            // AL

        default: // Reserved
        {
            // The ARM9-exclusive BLX instruction uses condition code 0xF, so let it run
            if ((opcode & 0x0E000000) == 0x0A000000)
                return true;

            printf("Unknown ARM%d ARM opcode: 0x%X\n", ((cpu == 0) ? 9 : 7), opcode);
            return false;
        }
    }
}

void Interpreter::setMode(uint8_t mode)
{
    // Point the registers to the correct ones for the new mode
    switch (mode & 0x1F)
    {
        case 0x10: // User
        case 0x1F: // System
            registers[8]  = &registersUsr[8];
            registers[9]  = &registersUsr[9];
            registers[10] = &registersUsr[10];
            registers[11] = &registersUsr[11];
            registers[12] = &registersUsr[12];
            registers[13] = &registersUsr[13];
            registers[14] = &registersUsr[14];
            spsr = nullptr;
            break;

        case 0x11: // FIQ
            registers[8]  = &registersFiq[0];
            registers[9]  = &registersFiq[1];
            registers[10] = &registersFiq[2];
            registers[11] = &registersFiq[3];
            registers[12] = &registersFiq[4];
            registers[13] = &registersFiq[5];
            registers[14] = &registersFiq[6];
            spsr = &spsrFiq;
            break;

        case 0x12: // IRQ
            registers[8]  = &registersUsr[8];
            registers[9]  = &registersUsr[9];
            registers[10] = &registersUsr[10];
            registers[11] = &registersUsr[11];
            registers[12] = &registersUsr[12];
            registers[13] = &registersIrq[0];
            registers[14] = &registersIrq[1];
            spsr = &spsrIrq;
            break;

        case 0x13: // Supervisor
            registers[8]  = &registersUsr[8];
            registers[9]  = &registersUsr[9];
            registers[10] = &registersUsr[10];
            registers[11] = &registersUsr[11];
            registers[12] = &registersUsr[12];
            registers[13] = &registersSvc[0];
            registers[14] = &registersSvc[1];
            spsr = &spsrSvc;
            break;

        case 0x17: // Abort
            registers[8]  = &registersUsr[8];
            registers[9]  = &registersUsr[9];
            registers[10] = &registersUsr[10];
            registers[11] = &registersUsr[11];
            registers[12] = &registersUsr[12];
            registers[13] = &registersAbt[0];
            registers[14] = &registersAbt[1];
            spsr = &spsrAbt;
            break;

        case 0x1B: // Undefined
            registers[8]  = &registersUsr[8];
            registers[9]  = &registersUsr[9];
            registers[10] = &registersUsr[10];
            registers[11] = &registersUsr[11];
            registers[12] = &registersUsr[12];
            registers[13] = &registersUnd[0];
            registers[14] = &registersUnd[1];
            spsr = &spsrUnd;
            break;

        default:
            printf("Unknown ARM%d CPU mode: 0x%X\n", ((cpu == 0) ? 9 : 7), mode & 0x1F);
            return;
    }

    // Set the new mode
    cpsr = (cpsr & ~0x1F) | (mode & 0x1F);
}

void Interpreter::writeIme(uint8_t value)
{
    // Write to the IME register
    ime = value & 0x01;
}

void Interpreter::writeIe(uint32_t mask, uint32_t value)
{
    // Write to the IE register
    mask &= ((cpu == 0) ? 0x003F3F7F : 0x01FF3FFF);
    ie = (ie & ~mask) | (value & mask);
}

void Interpreter::writeIrf(uint32_t mask, uint32_t value)
{
    // Write to the IF register
    // Setting a bit actually clears it to acknowledge an interrupt
    irf &= ~(value & mask);
}

void Interpreter::writePostFlg(uint8_t value)
{
    // Write to the POSTFLG register
    // The first bit can be set, but never cleared
    // For some reason, the second bit is writable on the ARM9
    postFlg |= value & 0x01;
    if (cpu == 0) postFlg = (postFlg & ~0x02) | (value & 0x02);
}
