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

#ifndef INTERPRETER_H
#define INTERPRETER_H

#include <cstdint>

#include "defines.h"

class Core;
class Bios;

class Interpreter
{
    public:
        Interpreter(Core *core, bool arm7);
        void saveState(FILE *file);
        void loadState(FILE *file);

        void init();
        void directBoot();
        void resetCycles();

        static void runNdsFrame(Core &core);
        static void runGbaFrame(Core &core);

        void halt(int bit)   { halted |=  BIT(bit); }
        void unhalt(int bit) { halted &= ~BIT(bit); }
        void sendInterrupt(int bit);
        void interrupt();

        bool     isThumb() { return cpsr & BIT(5);  }
        uint32_t getPC()   { return *registers[15]; }

        void setBios(Bios *bios) { this->bios = bios; }
        int handleHleIrq();

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
        bool arm7;

        Bios *bios = nullptr;
        uint32_t pipeline[2] = {};

        uint32_t *registers[32]   = {};
        uint32_t registersUsr[16] = {};
        uint32_t registersFiq[7]  = {};
        uint32_t registersSvc[2]  = {};
        uint32_t registersAbt[2]  = {};
        uint32_t registersIrq[2]  = {};
        uint32_t registersUnd[2]  = {};

        uint32_t cpsr = 0, *spsr = nullptr;
        uint32_t spsrFiq = 0, spsrSvc = 0, spsrAbt = 0, spsrIrq = 0, spsrUnd = 0;

        uint8_t halted = 0;
        uint32_t cycles = 0;

        uint8_t ime = 0;
        uint32_t ie = 0, irf = 0;
        uint8_t postFlg = 0;

        static int (Interpreter::*armInstrs[0x1000])(uint32_t);
        static int (Interpreter::*thumbInstrs[0x400])(uint16_t);

        static const uint8_t condition[0x100];
        static const uint8_t bitCount[0x100];

        int runOpcode();
        int exception(uint8_t vector);
        void flushPipeline();
        void swapRegisters(uint32_t value);
        void setCpsr(uint32_t value, bool save = false);
        int handleReserved(uint32_t opcode);
        int finishHleIrq();

        int unkArm(uint32_t opcode);
        int unkThumb(uint16_t opcode);

        int32_t clampQ(int64_t value);

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

        int _andLli(uint32_t opcode);
        int _andLlr(uint32_t opcode);
        int _andLri(uint32_t opcode);
        int _andLrr(uint32_t opcode);
        int _andAri(uint32_t opcode);
        int _andArr(uint32_t opcode);
        int _andRri(uint32_t opcode);
        int _andRrr(uint32_t opcode);
        int _andImm(uint32_t opcode);
        int andsLli(uint32_t opcode);
        int andsLlr(uint32_t opcode);
        int andsLri(uint32_t opcode);
        int andsLrr(uint32_t opcode);
        int andsAri(uint32_t opcode);
        int andsArr(uint32_t opcode);
        int andsRri(uint32_t opcode);
        int andsRrr(uint32_t opcode);
        int andsImm(uint32_t opcode);

        int eorLli(uint32_t opcode);
        int eorLlr(uint32_t opcode);
        int eorLri(uint32_t opcode);
        int eorLrr(uint32_t opcode);
        int eorAri(uint32_t opcode);
        int eorArr(uint32_t opcode);
        int eorRri(uint32_t opcode);
        int eorRrr(uint32_t opcode);
        int eorImm(uint32_t opcode);
        int eorsLli(uint32_t opcode);
        int eorsLlr(uint32_t opcode);
        int eorsLri(uint32_t opcode);
        int eorsLrr(uint32_t opcode);
        int eorsAri(uint32_t opcode);
        int eorsArr(uint32_t opcode);
        int eorsRri(uint32_t opcode);
        int eorsRrr(uint32_t opcode);
        int eorsImm(uint32_t opcode);

        int subLli(uint32_t opcode);
        int subLlr(uint32_t opcode);
        int subLri(uint32_t opcode);
        int subLrr(uint32_t opcode);
        int subAri(uint32_t opcode);
        int subArr(uint32_t opcode);
        int subRri(uint32_t opcode);
        int subRrr(uint32_t opcode);
        int subImm(uint32_t opcode);
        int subsLli(uint32_t opcode);
        int subsLlr(uint32_t opcode);
        int subsLri(uint32_t opcode);
        int subsLrr(uint32_t opcode);
        int subsAri(uint32_t opcode);
        int subsArr(uint32_t opcode);
        int subsRri(uint32_t opcode);
        int subsRrr(uint32_t opcode);
        int subsImm(uint32_t opcode);

        int rsbLli(uint32_t opcode);
        int rsbLlr(uint32_t opcode);
        int rsbLri(uint32_t opcode);
        int rsbLrr(uint32_t opcode);
        int rsbAri(uint32_t opcode);
        int rsbArr(uint32_t opcode);
        int rsbRri(uint32_t opcode);
        int rsbRrr(uint32_t opcode);
        int rsbImm(uint32_t opcode);
        int rsbsLli(uint32_t opcode);
        int rsbsLlr(uint32_t opcode);
        int rsbsLri(uint32_t opcode);
        int rsbsLrr(uint32_t opcode);
        int rsbsAri(uint32_t opcode);
        int rsbsArr(uint32_t opcode);
        int rsbsRri(uint32_t opcode);
        int rsbsRrr(uint32_t opcode);
        int rsbsImm(uint32_t opcode);

        int addLli(uint32_t opcode);
        int addLlr(uint32_t opcode);
        int addLri(uint32_t opcode);
        int addLrr(uint32_t opcode);
        int addAri(uint32_t opcode);
        int addArr(uint32_t opcode);
        int addRri(uint32_t opcode);
        int addRrr(uint32_t opcode);
        int addImm(uint32_t opcode);
        int addsLli(uint32_t opcode);
        int addsLlr(uint32_t opcode);
        int addsLri(uint32_t opcode);
        int addsLrr(uint32_t opcode);
        int addsAri(uint32_t opcode);
        int addsArr(uint32_t opcode);
        int addsRri(uint32_t opcode);
        int addsRrr(uint32_t opcode);
        int addsImm(uint32_t opcode);

        int adcLli(uint32_t opcode);
        int adcLlr(uint32_t opcode);
        int adcLri(uint32_t opcode);
        int adcLrr(uint32_t opcode);
        int adcAri(uint32_t opcode);
        int adcArr(uint32_t opcode);
        int adcRri(uint32_t opcode);
        int adcRrr(uint32_t opcode);
        int adcImm(uint32_t opcode);
        int adcsLli(uint32_t opcode);
        int adcsLlr(uint32_t opcode);
        int adcsLri(uint32_t opcode);
        int adcsLrr(uint32_t opcode);
        int adcsAri(uint32_t opcode);
        int adcsArr(uint32_t opcode);
        int adcsRri(uint32_t opcode);
        int adcsRrr(uint32_t opcode);
        int adcsImm(uint32_t opcode);

        int sbcLli(uint32_t opcode);
        int sbcLlr(uint32_t opcode);
        int sbcLri(uint32_t opcode);
        int sbcLrr(uint32_t opcode);
        int sbcAri(uint32_t opcode);
        int sbcArr(uint32_t opcode);
        int sbcRri(uint32_t opcode);
        int sbcRrr(uint32_t opcode);
        int sbcImm(uint32_t opcode);
        int sbcsLli(uint32_t opcode);
        int sbcsLlr(uint32_t opcode);
        int sbcsLri(uint32_t opcode);
        int sbcsLrr(uint32_t opcode);
        int sbcsAri(uint32_t opcode);
        int sbcsArr(uint32_t opcode);
        int sbcsRri(uint32_t opcode);
        int sbcsRrr(uint32_t opcode);
        int sbcsImm(uint32_t opcode);

        int rscLli(uint32_t opcode);
        int rscLlr(uint32_t opcode);
        int rscLri(uint32_t opcode);
        int rscLrr(uint32_t opcode);
        int rscAri(uint32_t opcode);
        int rscArr(uint32_t opcode);
        int rscRri(uint32_t opcode);
        int rscRrr(uint32_t opcode);
        int rscImm(uint32_t opcode);
        int rscsLli(uint32_t opcode);
        int rscsLlr(uint32_t opcode);
        int rscsLri(uint32_t opcode);
        int rscsLrr(uint32_t opcode);
        int rscsAri(uint32_t opcode);
        int rscsArr(uint32_t opcode);
        int rscsRri(uint32_t opcode);
        int rscsRrr(uint32_t opcode);
        int rscsImm(uint32_t opcode);

        int tstLli(uint32_t opcode);
        int tstLlr(uint32_t opcode);
        int tstLri(uint32_t opcode);
        int tstLrr(uint32_t opcode);
        int tstAri(uint32_t opcode);
        int tstArr(uint32_t opcode);
        int tstRri(uint32_t opcode);
        int tstRrr(uint32_t opcode);
        int tstImm(uint32_t opcode);
        int teqLli(uint32_t opcode);
        int teqLlr(uint32_t opcode);
        int teqLri(uint32_t opcode);
        int teqLrr(uint32_t opcode);
        int teqAri(uint32_t opcode);
        int teqArr(uint32_t opcode);
        int teqRri(uint32_t opcode);
        int teqRrr(uint32_t opcode);
        int teqImm(uint32_t opcode);

        int cmpLli(uint32_t opcode);
        int cmpLlr(uint32_t opcode);
        int cmpLri(uint32_t opcode);
        int cmpLrr(uint32_t opcode);
        int cmpAri(uint32_t opcode);
        int cmpArr(uint32_t opcode);
        int cmpRri(uint32_t opcode);
        int cmpRrr(uint32_t opcode);
        int cmpImm(uint32_t opcode);
        int cmnLli(uint32_t opcode);
        int cmnLlr(uint32_t opcode);
        int cmnLri(uint32_t opcode);
        int cmnLrr(uint32_t opcode);
        int cmnAri(uint32_t opcode);
        int cmnArr(uint32_t opcode);
        int cmnRri(uint32_t opcode);
        int cmnRrr(uint32_t opcode);
        int cmnImm(uint32_t opcode);

        int orrLli(uint32_t opcode);
        int orrLlr(uint32_t opcode);
        int orrLri(uint32_t opcode);
        int orrLrr(uint32_t opcode);
        int orrAri(uint32_t opcode);
        int orrArr(uint32_t opcode);
        int orrRri(uint32_t opcode);
        int orrRrr(uint32_t opcode);
        int orrImm(uint32_t opcode);
        int orrsLli(uint32_t opcode);
        int orrsLlr(uint32_t opcode);
        int orrsLri(uint32_t opcode);
        int orrsLrr(uint32_t opcode);
        int orrsAri(uint32_t opcode);
        int orrsArr(uint32_t opcode);
        int orrsRri(uint32_t opcode);
        int orrsRrr(uint32_t opcode);
        int orrsImm(uint32_t opcode);

        int movLli(uint32_t opcode);
        int movLlr(uint32_t opcode);
        int movLri(uint32_t opcode);
        int movLrr(uint32_t opcode);
        int movAri(uint32_t opcode);
        int movArr(uint32_t opcode);
        int movRri(uint32_t opcode);
        int movRrr(uint32_t opcode);
        int movImm(uint32_t opcode);
        int movsLli(uint32_t opcode);
        int movsLlr(uint32_t opcode);
        int movsLri(uint32_t opcode);
        int movsLrr(uint32_t opcode);
        int movsAri(uint32_t opcode);
        int movsArr(uint32_t opcode);
        int movsRri(uint32_t opcode);
        int movsRrr(uint32_t opcode);
        int movsImm(uint32_t opcode);

        int bicLli(uint32_t opcode);
        int bicLlr(uint32_t opcode);
        int bicLri(uint32_t opcode);
        int bicLrr(uint32_t opcode);
        int bicAri(uint32_t opcode);
        int bicArr(uint32_t opcode);
        int bicRri(uint32_t opcode);
        int bicRrr(uint32_t opcode);
        int bicImm(uint32_t opcode);
        int bicsLli(uint32_t opcode);
        int bicsLlr(uint32_t opcode);
        int bicsLri(uint32_t opcode);
        int bicsLrr(uint32_t opcode);
        int bicsAri(uint32_t opcode);
        int bicsArr(uint32_t opcode);
        int bicsRri(uint32_t opcode);
        int bicsRrr(uint32_t opcode);
        int bicsImm(uint32_t opcode);

        int mvnLli(uint32_t opcode);
        int mvnLlr(uint32_t opcode);
        int mvnLri(uint32_t opcode);
        int mvnLrr(uint32_t opcode);
        int mvnAri(uint32_t opcode);
        int mvnArr(uint32_t opcode);
        int mvnRri(uint32_t opcode);
        int mvnRrr(uint32_t opcode);
        int mvnImm(uint32_t opcode);
        int mvnsLli(uint32_t opcode);
        int mvnsLlr(uint32_t opcode);
        int mvnsLri(uint32_t opcode);
        int mvnsLrr(uint32_t opcode);
        int mvnsAri(uint32_t opcode);
        int mvnsArr(uint32_t opcode);
        int mvnsRri(uint32_t opcode);
        int mvnsRrr(uint32_t opcode);
        int mvnsImm(uint32_t opcode);

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

        int ldrsbOfrm(uint32_t opcode);
        int ldrsbOfim(uint32_t opcode);
        int ldrsbOfrp(uint32_t opcode);
        int ldrsbOfip(uint32_t opcode);
        int ldrsbPrrm(uint32_t opcode);
        int ldrsbPrim(uint32_t opcode);
        int ldrsbPrrp(uint32_t opcode);
        int ldrsbPrip(uint32_t opcode);
        int ldrsbPtrm(uint32_t opcode);
        int ldrsbPtim(uint32_t opcode);
        int ldrsbPtrp(uint32_t opcode);
        int ldrsbPtip(uint32_t opcode);
        int ldrshOfrm(uint32_t opcode);
        int ldrshOfim(uint32_t opcode);
        int ldrshOfrp(uint32_t opcode);
        int ldrshOfip(uint32_t opcode);
        int ldrshPrrm(uint32_t opcode);
        int ldrshPrim(uint32_t opcode);
        int ldrshPrrp(uint32_t opcode);
        int ldrshPrip(uint32_t opcode);
        int ldrshPtrm(uint32_t opcode);
        int ldrshPtim(uint32_t opcode);
        int ldrshPtrp(uint32_t opcode);
        int ldrshPtip(uint32_t opcode);

        int ldrbOfim(uint32_t opcode);
        int ldrbOfip(uint32_t opcode);
        int ldrbOfrmll(uint32_t opcode);
        int ldrbOfrmlr(uint32_t opcode);
        int ldrbOfrmar(uint32_t opcode);
        int ldrbOfrmrr(uint32_t opcode);
        int ldrbOfrpll(uint32_t opcode);
        int ldrbOfrplr(uint32_t opcode);
        int ldrbOfrpar(uint32_t opcode);
        int ldrbOfrprr(uint32_t opcode);
        int ldrbPrim(uint32_t opcode);
        int ldrbPrip(uint32_t opcode);
        int ldrbPrrmll(uint32_t opcode);
        int ldrbPrrmlr(uint32_t opcode);
        int ldrbPrrmar(uint32_t opcode);
        int ldrbPrrmrr(uint32_t opcode);
        int ldrbPrrpll(uint32_t opcode);
        int ldrbPrrplr(uint32_t opcode);
        int ldrbPrrpar(uint32_t opcode);
        int ldrbPrrprr(uint32_t opcode);
        int ldrbPtim(uint32_t opcode);
        int ldrbPtip(uint32_t opcode);
        int ldrbPtrmll(uint32_t opcode);
        int ldrbPtrmlr(uint32_t opcode);
        int ldrbPtrmar(uint32_t opcode);
        int ldrbPtrmrr(uint32_t opcode);
        int ldrbPtrpll(uint32_t opcode);
        int ldrbPtrplr(uint32_t opcode);
        int ldrbPtrpar(uint32_t opcode);
        int ldrbPtrprr(uint32_t opcode);

        int strbOfim(uint32_t opcode);
        int strbOfip(uint32_t opcode);
        int strbOfrmll(uint32_t opcode);
        int strbOfrmlr(uint32_t opcode);
        int strbOfrmar(uint32_t opcode);
        int strbOfrmrr(uint32_t opcode);
        int strbOfrpll(uint32_t opcode);
        int strbOfrplr(uint32_t opcode);
        int strbOfrpar(uint32_t opcode);
        int strbOfrprr(uint32_t opcode);
        int strbPrim(uint32_t opcode);
        int strbPrip(uint32_t opcode);
        int strbPrrmll(uint32_t opcode);
        int strbPrrmlr(uint32_t opcode);
        int strbPrrmar(uint32_t opcode);
        int strbPrrmrr(uint32_t opcode);
        int strbPrrpll(uint32_t opcode);
        int strbPrrplr(uint32_t opcode);
        int strbPrrpar(uint32_t opcode);
        int strbPrrprr(uint32_t opcode);
        int strbPtim(uint32_t opcode);
        int strbPtip(uint32_t opcode);
        int strbPtrmll(uint32_t opcode);
        int strbPtrmlr(uint32_t opcode);
        int strbPtrmar(uint32_t opcode);
        int strbPtrmrr(uint32_t opcode);
        int strbPtrpll(uint32_t opcode);
        int strbPtrplr(uint32_t opcode);
        int strbPtrpar(uint32_t opcode);
        int strbPtrprr(uint32_t opcode);

        int ldrhOfrm(uint32_t opcode);
        int ldrhOfim(uint32_t opcode);
        int ldrhOfrp(uint32_t opcode);
        int ldrhOfip(uint32_t opcode);
        int ldrhPrrm(uint32_t opcode);
        int ldrhPrim(uint32_t opcode);
        int ldrhPrrp(uint32_t opcode);
        int ldrhPrip(uint32_t opcode);
        int ldrhPtrm(uint32_t opcode);
        int ldrhPtim(uint32_t opcode);
        int ldrhPtrp(uint32_t opcode);
        int ldrhPtip(uint32_t opcode);
        int strhOfrm(uint32_t opcode);
        int strhOfim(uint32_t opcode);
        int strhOfrp(uint32_t opcode);
        int strhOfip(uint32_t opcode);
        int strhPrrm(uint32_t opcode);
        int strhPrim(uint32_t opcode);
        int strhPrrp(uint32_t opcode);
        int strhPrip(uint32_t opcode);
        int strhPtrm(uint32_t opcode);
        int strhPtim(uint32_t opcode);
        int strhPtrp(uint32_t opcode);
        int strhPtip(uint32_t opcode);

        int ldrOfim(uint32_t opcode);
        int ldrOfip(uint32_t opcode);
        int ldrOfrmll(uint32_t opcode);
        int ldrOfrmlr(uint32_t opcode);
        int ldrOfrmar(uint32_t opcode);
        int ldrOfrmrr(uint32_t opcode);
        int ldrOfrpll(uint32_t opcode);
        int ldrOfrplr(uint32_t opcode);
        int ldrOfrpar(uint32_t opcode);
        int ldrOfrprr(uint32_t opcode);
        int ldrPrim(uint32_t opcode);
        int ldrPrip(uint32_t opcode);
        int ldrPrrmll(uint32_t opcode);
        int ldrPrrmlr(uint32_t opcode);
        int ldrPrrmar(uint32_t opcode);
        int ldrPrrmrr(uint32_t opcode);
        int ldrPrrpll(uint32_t opcode);
        int ldrPrrplr(uint32_t opcode);
        int ldrPrrpar(uint32_t opcode);
        int ldrPrrprr(uint32_t opcode);
        int ldrPtim(uint32_t opcode);
        int ldrPtip(uint32_t opcode);
        int ldrPtrmll(uint32_t opcode);
        int ldrPtrmlr(uint32_t opcode);
        int ldrPtrmar(uint32_t opcode);
        int ldrPtrmrr(uint32_t opcode);
        int ldrPtrpll(uint32_t opcode);
        int ldrPtrplr(uint32_t opcode);
        int ldrPtrpar(uint32_t opcode);
        int ldrPtrprr(uint32_t opcode);

        int strOfim(uint32_t opcode);
        int strOfip(uint32_t opcode);
        int strOfrmll(uint32_t opcode);
        int strOfrmlr(uint32_t opcode);
        int strOfrmar(uint32_t opcode);
        int strOfrmrr(uint32_t opcode);
        int strOfrpll(uint32_t opcode);
        int strOfrplr(uint32_t opcode);
        int strOfrpar(uint32_t opcode);
        int strOfrprr(uint32_t opcode);
        int strPrim(uint32_t opcode);
        int strPrip(uint32_t opcode);
        int strPrrmll(uint32_t opcode);
        int strPrrmlr(uint32_t opcode);
        int strPrrmar(uint32_t opcode);
        int strPrrmrr(uint32_t opcode);
        int strPrrpll(uint32_t opcode);
        int strPrrplr(uint32_t opcode);
        int strPrrpar(uint32_t opcode);
        int strPrrprr(uint32_t opcode);
        int strPtim(uint32_t opcode);
        int strPtip(uint32_t opcode);
        int strPtrmll(uint32_t opcode);
        int strPtrmlr(uint32_t opcode);
        int strPtrmar(uint32_t opcode);
        int strPtrmrr(uint32_t opcode);
        int strPtrpll(uint32_t opcode);
        int strPtrplr(uint32_t opcode);
        int strPtrpar(uint32_t opcode);
        int strPtrprr(uint32_t opcode);

        int ldrdOfrm(uint32_t opcode);
        int ldrdOfim(uint32_t opcode);
        int ldrdOfrp(uint32_t opcode);
        int ldrdOfip(uint32_t opcode);
        int ldrdPrrm(uint32_t opcode);
        int ldrdPrim(uint32_t opcode);
        int ldrdPrrp(uint32_t opcode);
        int ldrdPrip(uint32_t opcode);
        int ldrdPtrm(uint32_t opcode);
        int ldrdPtim(uint32_t opcode);
        int ldrdPtrp(uint32_t opcode);
        int ldrdPtip(uint32_t opcode);
        int strdOfrm(uint32_t opcode);
        int strdOfim(uint32_t opcode);
        int strdOfrp(uint32_t opcode);
        int strdOfip(uint32_t opcode);
        int strdPrrm(uint32_t opcode);
        int strdPrim(uint32_t opcode);
        int strdPrrp(uint32_t opcode);
        int strdPrip(uint32_t opcode);
        int strdPtrm(uint32_t opcode);
        int strdPtim(uint32_t opcode);
        int strdPtrp(uint32_t opcode);
        int strdPtip(uint32_t opcode);

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
        int swi(uint32_t opcode);

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
        int swiT(uint16_t opcode);
};

#endif // INTERPRETER_H
