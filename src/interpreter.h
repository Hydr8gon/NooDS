/*
    Copyright 2019-2021 Hydr8gon

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

#ifndef INTERPRETER_H
#define INTERPRETER_H

#include <cstdint>

#include "defines.h"

class Core;

class Interpreter
{
    public:
        Interpreter(Core *core, bool cpu);

        void directBoot();
        void enterGbaMode();

        void runCycle();

        void halt();
        void sendInterrupt(int bit);

        bool shouldRun() { return !halted;  }

        uint8_t  readIme()     { return ime;     }
        uint32_t readIe()      { return ie;      }
        uint32_t readIrf()     { return irf;     }
        uint8_t  readPostFlg() { return postFlg; }

        void writeIme(uint8_t value);
        void writeIe(uint32_t mask, uint32_t value);
        void writeIrf(uint32_t mask, uint32_t value);
        void writePostFlg(uint8_t value);

    private:
        Core *core;
        bool cpu;

        uint32_t *registers[16]   = {};
        uint32_t registersUsr[16] = {};
        uint32_t registersFiq[7]  = {};
        uint32_t registersSvc[2]  = {};
        uint32_t registersAbt[2]  = {};
        uint32_t registersIrq[2]  = {};
        uint32_t registersUnd[2]  = {};

        uint32_t cpsr = 0, *spsr = nullptr;
        uint32_t spsrFiq = 0, spsrSvc = 0, spsrAbt = 0, spsrIrq = 0, spsrUnd = 0;

        bool halted = false;

        uint8_t ime = 0;
        uint32_t ie = 0, irf = 0;
        uint8_t postFlg = 0;

        bool condition(uint32_t opcode);
        void setMode(uint8_t mode);

        uint32_t lli(uint32_t opcode);
        uint32_t llr(uint32_t opcode);
        uint32_t lri(uint32_t opcode);
        uint32_t lrr(uint32_t opcode);
        uint32_t ari(uint32_t opcode);
        uint32_t arr(uint32_t opcode);
        uint32_t rri(uint32_t opcode);
        uint32_t rrr(uint32_t opcode);
        uint32_t imm(uint32_t opcode);
        uint32_t lliS(uint32_t opcode);
        uint32_t llrS(uint32_t opcode);
        uint32_t lriS(uint32_t opcode);
        uint32_t lrrS(uint32_t opcode);
        uint32_t ariS(uint32_t opcode);
        uint32_t arrS(uint32_t opcode);
        uint32_t rriS(uint32_t opcode);
        uint32_t rrrS(uint32_t opcode);
        uint32_t immS(uint32_t opcode);

        void _and(uint32_t opcode, uint32_t op2);
        void eor(uint32_t opcode, uint32_t op2);
        void sub(uint32_t opcode, uint32_t op2);
        void rsb(uint32_t opcode, uint32_t op2);
        void add(uint32_t opcode, uint32_t op2);
        void adc(uint32_t opcode, uint32_t op2);
        void sbc(uint32_t opcode, uint32_t op2);
        void rsc(uint32_t opcode, uint32_t op2);
        void tst(uint32_t opcode, uint32_t op2);
        void teq(uint32_t opcode, uint32_t op2);
        void cmp(uint32_t opcode, uint32_t op2);
        void cmn(uint32_t opcode, uint32_t op2);
        void orr(uint32_t opcode, uint32_t op2);
        void mov(uint32_t opcode, uint32_t op2);
        void bic(uint32_t opcode, uint32_t op2);
        void mvn(uint32_t opcode, uint32_t op2);
        void ands(uint32_t opcode, uint32_t op2);
        void eors(uint32_t opcode, uint32_t op2);
        void subs(uint32_t opcode, uint32_t op2);
        void rsbs(uint32_t opcode, uint32_t op2);
        void adds(uint32_t opcode, uint32_t op2);
        void adcs(uint32_t opcode, uint32_t op2);
        void sbcs(uint32_t opcode, uint32_t op2);
        void rscs(uint32_t opcode, uint32_t op2);
        void orrs(uint32_t opcode, uint32_t op2);
        void movs(uint32_t opcode, uint32_t op2);
        void bics(uint32_t opcode, uint32_t op2);
        void mvns(uint32_t opcode, uint32_t op2);

        void mul(uint32_t opcode);
        void mla(uint32_t opcode);
        void umull(uint32_t opcode);
        void umlal(uint32_t opcode);
        void smull(uint32_t opcode);
        void smlal(uint32_t opcode);
        void muls(uint32_t opcode);
        void mlas(uint32_t opcode);
        void umulls(uint32_t opcode);
        void umlals(uint32_t opcode);
        void smulls(uint32_t opcode);
        void smlals(uint32_t opcode);
        void smulbb(uint32_t opcode);
        void smulbt(uint32_t opcode);
        void smultb(uint32_t opcode);
        void smultt(uint32_t opcode);
        void smulwb(uint32_t opcode);
        void smulwt(uint32_t opcode);
        void smlabb(uint32_t opcode);
        void smlabt(uint32_t opcode);
        void smlatb(uint32_t opcode);
        void smlatt(uint32_t opcode);
        void smlawb(uint32_t opcode);
        void smlawt(uint32_t opcode);
        void smlalbb(uint32_t opcode);
        void smlalbt(uint32_t opcode);
        void smlaltb(uint32_t opcode);
        void smlaltt(uint32_t opcode);
        void qadd(uint32_t opcode);
        void qsub(uint32_t opcode);
        void qdadd(uint32_t opcode);
        void qdsub(uint32_t opcode);
        void clz(uint32_t opcode);

        void addRegT(uint16_t opcode);
        void subRegT(uint16_t opcode);
        void addHT(uint16_t opcode);
        void cmpHT(uint16_t opcode);
        void movHT(uint16_t opcode);
        void addPcT(uint16_t opcode);
        void addSpT(uint16_t opcode);
        void addSpImmT(uint16_t opcode);
        void lslImmT(uint16_t opcode);
        void lsrImmT(uint16_t opcode);
        void asrImmT(uint16_t opcode);
        void addImm3T(uint16_t opcode);
        void subImm3T(uint16_t opcode);
        void addImm8T(uint16_t opcode);
        void subImm8T(uint16_t opcode);
        void cmpImm8T(uint16_t opcode);
        void movImm8T(uint16_t opcode);
        void lslDpT(uint16_t opcode);
        void lsrDpT(uint16_t opcode);
        void asrDpT(uint16_t opcode);
        void rorDpT(uint16_t opcode);
        void andDpT(uint16_t opcode);
        void eorDpT(uint16_t opcode);
        void adcDpT(uint16_t opcode);
        void sbcDpT(uint16_t opcode);
        void tstDpT(uint16_t opcode);
        void cmpDpT(uint16_t opcode);
        void cmnDpT(uint16_t opcode);
        void orrDpT(uint16_t opcode);
        void bicDpT(uint16_t opcode);
        void mvnDpT(uint16_t opcode);
        void negDpT(uint16_t opcode);
        void mulDpT(uint16_t opcode);

        uint32_t ip(uint32_t opcode);
        uint32_t ipH(uint32_t opcode);
        uint32_t rp(uint32_t opcode);
        uint32_t rpll(uint32_t opcode);
        uint32_t rplr(uint32_t opcode);
        uint32_t rpar(uint32_t opcode);
        uint32_t rprr(uint32_t opcode);

        void ldrsbOf(uint32_t opcode, uint32_t op2);
        void ldrshOf(uint32_t opcode, uint32_t op2);
        void ldrbOf(uint32_t opcode, uint32_t op2);
        void strbOf(uint32_t opcode, uint32_t op2);
        void ldrhOf(uint32_t opcode, uint32_t op2);
        void strhOf(uint32_t opcode, uint32_t op2);
        void ldrOf(uint32_t opcode, uint32_t op2);
        void strOf(uint32_t opcode, uint32_t op2);
        void ldrdOf(uint32_t opcode, uint32_t op2);
        void strdOf(uint32_t opcode, uint32_t op2);
        void ldrsbPr(uint32_t opcode, uint32_t op2);
        void ldrshPr(uint32_t opcode, uint32_t op2);
        void ldrbPr(uint32_t opcode, uint32_t op2);
        void strbPr(uint32_t opcode, uint32_t op2);
        void ldrhPr(uint32_t opcode, uint32_t op2);
        void strhPr(uint32_t opcode, uint32_t op2);
        void ldrPr(uint32_t opcode, uint32_t op2);
        void strPr(uint32_t opcode, uint32_t op2);
        void ldrdPr(uint32_t opcode, uint32_t op2);
        void strdPr(uint32_t opcode, uint32_t op2);
        void ldrsbPt(uint32_t opcode, uint32_t op2);
        void ldrshPt(uint32_t opcode, uint32_t op2);
        void ldrbPt(uint32_t opcode, uint32_t op2);
        void strbPt(uint32_t opcode, uint32_t op2);
        void ldrhPt(uint32_t opcode, uint32_t op2);
        void strhPt(uint32_t opcode, uint32_t op2);
        void ldrPt(uint32_t opcode, uint32_t op2);
        void strPt(uint32_t opcode, uint32_t op2);
        void ldrdPt(uint32_t opcode, uint32_t op2);
        void strdPt(uint32_t opcode, uint32_t op2);

        void swpb(uint32_t opcode);
        void swp(uint32_t opcode);
        void ldmda(uint32_t opcode);
        void stmda(uint32_t opcode);
        void ldmia(uint32_t opcode);
        void stmia(uint32_t opcode);
        void ldmdb(uint32_t opcode);
        void stmdb(uint32_t opcode);
        void ldmib(uint32_t opcode);
        void stmib(uint32_t opcode);
        void ldmdaW(uint32_t opcode);
        void stmdaW(uint32_t opcode);
        void ldmiaW(uint32_t opcode);
        void stmiaW(uint32_t opcode);
        void ldmdbW(uint32_t opcode);
        void stmdbW(uint32_t opcode);
        void ldmibW(uint32_t opcode);
        void stmibW(uint32_t opcode);
        void ldmdaU(uint32_t opcode);
        void stmdaU(uint32_t opcode);
        void ldmiaU(uint32_t opcode);
        void stmiaU(uint32_t opcode);
        void ldmdbU(uint32_t opcode);
        void stmdbU(uint32_t opcode);
        void ldmibU(uint32_t opcode);
        void stmibU(uint32_t opcode);
        void ldmdaUW(uint32_t opcode);
        void stmdaUW(uint32_t opcode);
        void ldmiaUW(uint32_t opcode);
        void stmiaUW(uint32_t opcode);
        void ldmdbUW(uint32_t opcode);
        void stmdbUW(uint32_t opcode);
        void ldmibUW(uint32_t opcode);
        void stmibUW(uint32_t opcode);
        void msrRc(uint32_t opcode);
        void msrRs(uint32_t opcode);
        void msrIc(uint32_t opcode);
        void msrIs(uint32_t opcode);
        void mrsRc(uint32_t opcode);
        void mrsRs(uint32_t opcode);
        void mrc(uint32_t opcode);
        void mcr(uint32_t opcode);

        void ldrsbRegT(uint16_t opcode);
        void ldrshRegT(uint16_t opcode);
        void ldrbRegT(uint16_t opcode);
        void strbRegT(uint16_t opcode);
        void ldrhRegT(uint16_t opcode);
        void strhRegT(uint16_t opcode);
        void ldrRegT(uint16_t opcode);
        void strRegT(uint16_t opcode);
        void ldrbImm5T(uint16_t opcode);
        void strbImm5T(uint16_t opcode);
        void ldrhImm5T(uint16_t opcode);
        void strhImm5T(uint16_t opcode);
        void ldrImm5T(uint16_t opcode);
        void strImm5T(uint16_t opcode);
        void ldrPcT(uint16_t opcode);
        void ldrSpT(uint16_t opcode);
        void strSpT(uint16_t opcode);
        void ldmiaT(uint16_t opcode);
        void stmiaT(uint16_t opcode);
        void popT(uint16_t opcode);
        void pushT(uint16_t opcode);
        void popPcT(uint16_t opcode);
        void pushLrT(uint16_t opcode);

        void bx(uint32_t opcode);
        void blxReg(uint32_t opcode);
        void b(uint32_t opcode);
        void bl(uint32_t opcode);
        void blx(uint32_t opcode);
        void swi();

        void bxRegT(uint16_t opcode);
        void blxRegT(uint16_t opcode);
        void bT(uint16_t opcode);
        void beqT(uint16_t opcode);
        void bneT(uint16_t opcode);
        void bcsT(uint16_t opcode);
        void bccT(uint16_t opcode);
        void bmiT(uint16_t opcode);
        void bplT(uint16_t opcode);
        void bvsT(uint16_t opcode);
        void bvcT(uint16_t opcode);
        void bhiT(uint16_t opcode);
        void blsT(uint16_t opcode);
        void bgeT(uint16_t opcode);
        void bltT(uint16_t opcode);
        void bgtT(uint16_t opcode);
        void bleT(uint16_t opcode);
        void blSetupT(uint16_t opcode);
        void blOffT(uint16_t opcode);
        void blxOffT(uint16_t opcode);
        void swiT();
};

#endif // INTERPRETER_H
