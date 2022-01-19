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

#ifndef INTERPRETER_H
#define INTERPRETER_H

#include <cstdint>
#include <functional>

#include "defines.h"

class Core;

class Interpreter
{
    public:
        Interpreter(Core *core, bool cpu);

        void init();
        void directBoot();

        int runOpcode();

        void halt(int bit)   { halted |=  BIT(bit); }
        void unhalt(int bit) { halted &= ~BIT(bit); }
        void sendInterrupt(int bit);

        bool     shouldRun() { return !halted;        }
        uint32_t getPC()     { return *registers[15]; }

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

        uint32_t pipeline[2] = {};

        uint32_t *registers[16]   = {};
        uint32_t registersUsr[16] = {};
        uint32_t registersFiq[7]  = {};
        uint32_t registersSvc[2]  = {};
        uint32_t registersAbt[2]  = {};
        uint32_t registersIrq[2]  = {};
        uint32_t registersUnd[2]  = {};

        uint32_t cpsr = 0, *spsr = nullptr;
        uint32_t spsrFiq = 0, spsrSvc = 0, spsrAbt = 0, spsrIrq = 0, spsrUnd = 0;

        uint8_t halted = 0;

        uint8_t ime = 0;
        uint32_t ie = 0, irf = 0;
        uint8_t postFlg = 0;

        static const uint8_t condition[0x100];
        static const uint8_t bitCount[0x100];

        std::function<void()> interruptTask;

        void interrupt();
        void flushPipeline();
        void setCpsr(uint32_t value, bool save = false);
        int handleReserved(uint32_t opcode);

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

        int _and(uint32_t opcode, uint32_t op2);
        int eor(uint32_t opcode, uint32_t op2);
        int sub(uint32_t opcode, uint32_t op2);
        int rsb(uint32_t opcode, uint32_t op2);
        int add(uint32_t opcode, uint32_t op2);
        int adc(uint32_t opcode, uint32_t op2);
        int sbc(uint32_t opcode, uint32_t op2);
        int rsc(uint32_t opcode, uint32_t op2);
        int tst(uint32_t opcode, uint32_t op2);
        int teq(uint32_t opcode, uint32_t op2);
        int cmp(uint32_t opcode, uint32_t op2);
        int cmn(uint32_t opcode, uint32_t op2);
        int orr(uint32_t opcode, uint32_t op2);
        int mov(uint32_t opcode, uint32_t op2);
        int bic(uint32_t opcode, uint32_t op2);
        int mvn(uint32_t opcode, uint32_t op2);
        int ands(uint32_t opcode, uint32_t op2);
        int eors(uint32_t opcode, uint32_t op2);
        int subs(uint32_t opcode, uint32_t op2);
        int rsbs(uint32_t opcode, uint32_t op2);
        int adds(uint32_t opcode, uint32_t op2);
        int adcs(uint32_t opcode, uint32_t op2);
        int sbcs(uint32_t opcode, uint32_t op2);
        int rscs(uint32_t opcode, uint32_t op2);
        int orrs(uint32_t opcode, uint32_t op2);
        int movs(uint32_t opcode, uint32_t op2);
        int bics(uint32_t opcode, uint32_t op2);
        int mvns(uint32_t opcode, uint32_t op2);

        int mul(uint32_t opcode);
        int mla(uint32_t opcode);
        int umull(uint32_t opcode);
        int umlal(uint32_t opcode);
        int smull(uint32_t opcode);
        int smlal(uint32_t opcode);
        int muls(uint32_t opcode);
        int mlas(uint32_t opcode);
        int umulls(uint32_t opcode);
        int umlals(uint32_t opcode);
        int smulls(uint32_t opcode);
        int smlals(uint32_t opcode);
        int smulbb(uint32_t opcode);
        int smulbt(uint32_t opcode);
        int smultb(uint32_t opcode);
        int smultt(uint32_t opcode);
        int smulwb(uint32_t opcode);
        int smulwt(uint32_t opcode);
        int smlabb(uint32_t opcode);
        int smlabt(uint32_t opcode);
        int smlatb(uint32_t opcode);
        int smlatt(uint32_t opcode);
        int smlawb(uint32_t opcode);
        int smlawt(uint32_t opcode);
        int smlalbb(uint32_t opcode);
        int smlalbt(uint32_t opcode);
        int smlaltb(uint32_t opcode);
        int smlaltt(uint32_t opcode);
        int qadd(uint32_t opcode);
        int qsub(uint32_t opcode);
        int qdadd(uint32_t opcode);
        int qdsub(uint32_t opcode);
        int clz(uint32_t opcode);

        int addRegT(uint16_t opcode);
        int subRegT(uint16_t opcode);
        int addHT(uint16_t opcode);
        int cmpHT(uint16_t opcode);
        int movHT(uint16_t opcode);
        int addPcT(uint16_t opcode);
        int addSpT(uint16_t opcode);
        int addSpImmT(uint16_t opcode);
        int lslImmT(uint16_t opcode);
        int lsrImmT(uint16_t opcode);
        int asrImmT(uint16_t opcode);
        int addImm3T(uint16_t opcode);
        int subImm3T(uint16_t opcode);
        int addImm8T(uint16_t opcode);
        int subImm8T(uint16_t opcode);
        int cmpImm8T(uint16_t opcode);
        int movImm8T(uint16_t opcode);
        int lslDpT(uint16_t opcode);
        int lsrDpT(uint16_t opcode);
        int asrDpT(uint16_t opcode);
        int rorDpT(uint16_t opcode);
        int andDpT(uint16_t opcode);
        int eorDpT(uint16_t opcode);
        int adcDpT(uint16_t opcode);
        int sbcDpT(uint16_t opcode);
        int tstDpT(uint16_t opcode);
        int cmpDpT(uint16_t opcode);
        int cmnDpT(uint16_t opcode);
        int orrDpT(uint16_t opcode);
        int bicDpT(uint16_t opcode);
        int mvnDpT(uint16_t opcode);
        int negDpT(uint16_t opcode);
        int mulDpT(uint16_t opcode);

        uint32_t ip(uint32_t opcode);
        uint32_t ipH(uint32_t opcode);
        uint32_t rp(uint32_t opcode);
        uint32_t rpll(uint32_t opcode);
        uint32_t rplr(uint32_t opcode);
        uint32_t rpar(uint32_t opcode);
        uint32_t rprr(uint32_t opcode);

        int ldrsbOf(uint32_t opcode, uint32_t op2);
        int ldrshOf(uint32_t opcode, uint32_t op2);
        int ldrbOf(uint32_t opcode, uint32_t op2);
        int strbOf(uint32_t opcode, uint32_t op2);
        int ldrhOf(uint32_t opcode, uint32_t op2);
        int strhOf(uint32_t opcode, uint32_t op2);
        int ldrOf(uint32_t opcode, uint32_t op2);
        int strOf(uint32_t opcode, uint32_t op2);
        int ldrdOf(uint32_t opcode, uint32_t op2);
        int strdOf(uint32_t opcode, uint32_t op2);
        int ldrsbPr(uint32_t opcode, uint32_t op2);
        int ldrshPr(uint32_t opcode, uint32_t op2);
        int ldrbPr(uint32_t opcode, uint32_t op2);
        int strbPr(uint32_t opcode, uint32_t op2);
        int ldrhPr(uint32_t opcode, uint32_t op2);
        int strhPr(uint32_t opcode, uint32_t op2);
        int ldrPr(uint32_t opcode, uint32_t op2);
        int strPr(uint32_t opcode, uint32_t op2);
        int ldrdPr(uint32_t opcode, uint32_t op2);
        int strdPr(uint32_t opcode, uint32_t op2);
        int ldrsbPt(uint32_t opcode, uint32_t op2);
        int ldrshPt(uint32_t opcode, uint32_t op2);
        int ldrbPt(uint32_t opcode, uint32_t op2);
        int strbPt(uint32_t opcode, uint32_t op2);
        int ldrhPt(uint32_t opcode, uint32_t op2);
        int strhPt(uint32_t opcode, uint32_t op2);
        int ldrPt(uint32_t opcode, uint32_t op2);
        int strPt(uint32_t opcode, uint32_t op2);
        int ldrdPt(uint32_t opcode, uint32_t op2);
        int strdPt(uint32_t opcode, uint32_t op2);

        int swpb(uint32_t opcode);
        int swp(uint32_t opcode);
        int ldmda(uint32_t opcode);
        int stmda(uint32_t opcode);
        int ldmia(uint32_t opcode);
        int stmia(uint32_t opcode);
        int ldmdb(uint32_t opcode);
        int stmdb(uint32_t opcode);
        int ldmib(uint32_t opcode);
        int stmib(uint32_t opcode);
        int ldmdaW(uint32_t opcode);
        int stmdaW(uint32_t opcode);
        int ldmiaW(uint32_t opcode);
        int stmiaW(uint32_t opcode);
        int ldmdbW(uint32_t opcode);
        int stmdbW(uint32_t opcode);
        int ldmibW(uint32_t opcode);
        int stmibW(uint32_t opcode);
        int ldmdaU(uint32_t opcode);
        int stmdaU(uint32_t opcode);
        int ldmiaU(uint32_t opcode);
        int stmiaU(uint32_t opcode);
        int ldmdbU(uint32_t opcode);
        int stmdbU(uint32_t opcode);
        int ldmibU(uint32_t opcode);
        int stmibU(uint32_t opcode);
        int ldmdaUW(uint32_t opcode);
        int stmdaUW(uint32_t opcode);
        int ldmiaUW(uint32_t opcode);
        int stmiaUW(uint32_t opcode);
        int ldmdbUW(uint32_t opcode);
        int stmdbUW(uint32_t opcode);
        int ldmibUW(uint32_t opcode);
        int stmibUW(uint32_t opcode);
        int msrRc(uint32_t opcode);
        int msrRs(uint32_t opcode);
        int msrIc(uint32_t opcode);
        int msrIs(uint32_t opcode);
        int mrsRc(uint32_t opcode);
        int mrsRs(uint32_t opcode);
        int mrc(uint32_t opcode);
        int mcr(uint32_t opcode);

        int ldrsbRegT(uint16_t opcode);
        int ldrshRegT(uint16_t opcode);
        int ldrbRegT(uint16_t opcode);
        int strbRegT(uint16_t opcode);
        int ldrhRegT(uint16_t opcode);
        int strhRegT(uint16_t opcode);
        int ldrRegT(uint16_t opcode);
        int strRegT(uint16_t opcode);
        int ldrbImm5T(uint16_t opcode);
        int strbImm5T(uint16_t opcode);
        int ldrhImm5T(uint16_t opcode);
        int strhImm5T(uint16_t opcode);
        int ldrImm5T(uint16_t opcode);
        int strImm5T(uint16_t opcode);
        int ldrPcT(uint16_t opcode);
        int ldrSpT(uint16_t opcode);
        int strSpT(uint16_t opcode);
        int ldmiaT(uint16_t opcode);
        int stmiaT(uint16_t opcode);
        int popT(uint16_t opcode);
        int pushT(uint16_t opcode);
        int popPcT(uint16_t opcode);
        int pushLrT(uint16_t opcode);

        int bx(uint32_t opcode);
        int blxReg(uint32_t opcode);
        int b(uint32_t opcode);
        int bl(uint32_t opcode);
        int blx(uint32_t opcode);
        int swi();

        int bxRegT(uint16_t opcode);
        int blxRegT(uint16_t opcode);
        int bT(uint16_t opcode);
        int beqT(uint16_t opcode);
        int bneT(uint16_t opcode);
        int bcsT(uint16_t opcode);
        int bccT(uint16_t opcode);
        int bmiT(uint16_t opcode);
        int bplT(uint16_t opcode);
        int bvsT(uint16_t opcode);
        int bvcT(uint16_t opcode);
        int bhiT(uint16_t opcode);
        int blsT(uint16_t opcode);
        int bgeT(uint16_t opcode);
        int bltT(uint16_t opcode);
        int bgtT(uint16_t opcode);
        int bleT(uint16_t opcode);
        int blSetupT(uint16_t opcode);
        int blOffT(uint16_t opcode);
        int blxOffT(uint16_t opcode);
        int swiT();
};

#endif // INTERPRETER_H
